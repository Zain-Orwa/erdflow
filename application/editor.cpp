#include "application/editor.hpp"

#include <algorithm>
#include <functional>
#include <set>
#include <stdexcept>
#include <type_traits>
#include <utility>

namespace erdflow::application {
using namespace domain;
namespace {
constexpr std::size_t history_budget = 32U * 1024U * 1024U;

EditResult failure(std::string message) { return {false, std::move(message), {}, {}}; }

template<class Key, class Value> struct Changes {
    std::set<Key> keys;
    std::map<Key, Value> alternate;

    void put(Key key, Value value) {
        keys.insert(key);
        alternate.insert_or_assign(key, std::move(value));
    }
    void remove(Key key) {
        keys.insert(key);
        alternate.erase(key);
    }
    void toggle(std::map<Key, Value>& live) {
        // Node handles transfer existing allocations, including strings/vectors.
        // Undo, redo and rollback therefore need no allocation to restore state.
        for (const auto& key : keys) {
            auto current = live.extract(key);
            auto replacement = alternate.extract(key);
            if (!replacement.empty()) live.insert(std::move(replacement));
            if (!current.empty()) alternate.insert(std::move(current));
        }
    }
};

std::size_t payload(const Entity& value) { return value.name.capacity() + value.description.capacity(); }
std::size_t payload(const Attribute& value) { return value.name.capacity() + value.description.capacity(); }
std::size_t payload(const Relationship& value) {
    std::size_t result = value.name.capacity() + value.description.capacity()
        + value.participants.capacity() * sizeof(Participant);
    for (const auto& participant : value.participants) result += participant.role.capacity();
    return result;
}
std::size_t payload(const Specialization& value) {
    return value.name.capacity() + value.description.capacity() + value.subtypes.capacity() * sizeof(EntityId);
}
std::size_t payload(const Rect&) { return 0; }
std::size_t payload(const Colour&) { return 0; }
std::size_t payload(double) { return 0; }
// A connector holds its bend and joins inline; only the route is on the heap.
std::size_t payload(const Connector& value) { return value.waypoints.capacity() * sizeof(Point); }

template<class Key, class Value>
std::size_t cost(const Changes<Key, Value>& changes, const std::map<Key, Value>& live) {
    std::size_t result = changes.keys.size() * (sizeof(Key) + 4 * sizeof(void*));
    for (const auto& key : changes.keys) {
        // Conservatively account for both directions, tree-node overhead and
        // owned heap payloads. Only the inactive value is retained by history.
        for (const auto* values : {&live, &changes.alternate}) {
            const auto found = values->find(key);
            if (found != values->end()) result += sizeof(std::pair<const Key, Value>) + 4 * sizeof(void*) + payload(found->second);
        }
    }
    return result;
}

struct Delta {
    std::string label;
    std::optional<std::string> project_name;
    Changes<EntityId, Entity> entities;
    Changes<AttributeId, Attribute> attributes;
    Changes<RelationshipId, Relationship> relationships;
    Changes<SpecializationId, Specialization> specializations;
    Changes<ElementRef, Rect> layout;
    Changes<ConnectorRef, Connector> connectors;
    Changes<ElementRef, Colour> colours;
    std::size_t bytes = 0;
    std::uint64_t before_state = 0;
    std::uint64_t after_state = 0;

    [[nodiscard]] bool empty() const {
        return !project_name && entities.keys.empty() && attributes.keys.empty()
            && relationships.keys.empty() && specializations.keys.empty()
            && layout.keys.empty() && connectors.keys.empty() && colours.keys.empty();
    }
    void toggle(Project& project) {
        if (project_name) project.name.swap(*project_name);
        entities.toggle(project.entities);
        attributes.toggle(project.attributes);
        relationships.toggle(project.relationships);
        specializations.toggle(project.specializations);
        layout.toggle(project.layout);
        connectors.toggle(project.connectors);
        colours.toggle(project.colours);
    }
    [[nodiscard]] std::size_t estimate(const Project& project) const {
        return sizeof(Delta) + sizeof(std::unique_ptr<Delta>) + label.capacity()
            + (project_name ? project_name->capacity() + project.name.capacity() : 0)
            + cost(entities, project.entities) + cost(attributes, project.attributes)
            + cost(relationships, project.relationships) + cost(specializations, project.specializations)
            + cost(layout, project.layout) + cost(connectors, project.connectors)
            + cost(colours, project.colours);
    }
};

std::set<ElementRef> owned_closure(const Project& project, const std::vector<ElementRef>& selection) {
    std::set<ElementRef> result;
    std::vector<ElementRef> pending;
    for (const auto& ref : selection) {
        if (!exists(project, ref)) throw std::invalid_argument("An element no longer exists.");
        if (result.insert(ref).second) pending.push_back(ref);
    }
    std::map<ElementRef, std::vector<AttributeId>> children;
    for (const auto& [id, attribute] : project.attributes)
        if (attribute.owner) children[*attribute.owner].push_back(id);
    for (std::size_t i = 0; i < pending.size(); ++i) {
        const auto found = children.find(pending[i]);
        if (found == children.end()) continue;
        for (const auto& id : found->second)
            if (result.insert(ElementRef{id}).second) pending.emplace_back(id);
    }
    return result;
}
} // namespace

struct Editor::Impl {
    explicit Impl(IdGenerator& generator) : ids(generator) {}
    IdGenerator& ids;
    Project project;
    std::vector<std::unique_ptr<Delta>> history;
    std::size_t cursor = 0;
    std::size_t bytes = 0;
    std::uint64_t revision = 0;
    std::uint64_t state = 0;
    std::uint64_t saved_state = 0;
    // Retain issued identities for the session: a faulty injectable generator
    // cannot reuse a deleted identity even after its history has been evicted.
    std::set<Uuid> issued;

    Uuid next_id() {
        for (unsigned attempt = 0; attempt < 16; ++attempt) {
            const auto id = ids.next();
            if (id.valid() && issued.insert(id).second) return id;
        }
        throw std::runtime_error("The identifier generator did not produce a fresh UUIDv7.");
    }

    template<class Work> EditResult edit(std::string label, Work work) {
        try {
            auto delta = std::make_unique<Delta>();
            delta->label = std::move(label);
            EditResult result = work(*delta);
            if (!result || delta->empty()) return result;
            delta->bytes = delta->estimate(project);
            if (delta->bytes > history_budget)
                return failure("This change exceeds the 32 MiB undo budget. Apply it in smaller groups.");
            // Prepare history storage before touching the model. Rejected edits
            // keep the redo branch and clean marker intact.
            if (history.capacity() < cursor + 1) history.reserve(std::max(cursor + 1, history.capacity() * 2));
            delta->toggle(project);
            try {
                const auto issues = validate(project);
                const auto invalid = std::find_if(issues.begin(), issues.end(), [](const auto& issue) { return issue.blocks_save; });
                if (invalid != issues.end()) {
                    auto rejected = failure(invalid->message);
                    delta->toggle(project);
                    return rejected;
                }
            } catch (...) {
                delta->toggle(project);
                throw;
            }
            delta->before_state = state;
            delta->after_state = revision + 1;
            while (history.size() > cursor) {
                bytes -= history.back()->bytes;
                history.pop_back();
            }
            bytes += delta->bytes;
            state = delta->after_state;
            history.push_back(std::move(delta));
            ++cursor;
            ++revision;
            std::size_t discard = 0;
            while (bytes > history_budget && discard < cursor) {
                bytes -= history[discard]->bytes;
                ++discard;
            }
            if (discard != 0) {
                history.erase(history.begin(), history.begin() + static_cast<std::ptrdiff_t>(discard));
                cursor -= discard;
            }
            return result;
        } catch (const std::exception& error) {
            return failure(error.what());
        }
    }
};

Editor::Editor(IdGenerator& ids) : impl_(std::make_unique<Impl>(ids)) { new_project(); }
Editor::~Editor() = default;
const Project& Editor::project() const { return impl_->project; }
bool Editor::dirty() const { return impl_->state != impl_->saved_state; }
std::uint64_t Editor::revision() const { return impl_->revision; }
bool Editor::can_undo() const { return impl_->cursor > 0; }
bool Editor::can_redo() const { return impl_->cursor < impl_->history.size(); }
std::string Editor::undo_label() const { return can_undo() ? impl_->history[impl_->cursor - 1]->label : std::string{}; }
std::string Editor::redo_label() const { return can_redo() ? impl_->history[impl_->cursor]->label : std::string{}; }
std::size_t Editor::history_bytes() const { return impl_->bytes; }

void Editor::new_project() {
    Project fresh;
    fresh.id = ProjectId{impl_->next_id()};
    const auto result = replace_project(std::move(fresh));
    if (!result) throw std::runtime_error(result.error);
}

EditResult Editor::replace_project(Project project) {
    try {
        const auto issues = validate(project);
        const auto invalid = std::find_if(issues.begin(), issues.end(), [](const auto& issue) { return issue.blocks_save; });
        if (invalid != issues.end()) return failure(invalid->message);
        std::set<Uuid> identities{project.id.value};
        for (const auto& [id, entity] : project.entities) { (void)entity; identities.insert(id.value); }
        for (const auto& [id, attribute] : project.attributes) { (void)attribute; identities.insert(id.value); }
        for (const auto& [id, relationship] : project.relationships) {
            identities.insert(id.value);
            for (const auto& participant : relationship.participants) identities.insert(participant.id.value);
        }
        impl_->project = std::move(project);
        impl_->issued.swap(identities);
        impl_->history.clear();
        impl_->cursor = 0;
        impl_->bytes = 0;
        impl_->state = ++impl_->revision;
        impl_->saved_state = impl_->state;
        return {};
    } catch (const std::exception& error) { return failure(error.what()); }
}

void Editor::mark_saved(std::uint64_t revision) {
    if (revision == impl_->revision) impl_->saved_state = impl_->state;
}

EditResult Editor::rename_project(std::string name) {
    return impl_->edit("Rename project", [&](Delta& delta) {
        if (name != project().name) delta.project_name = std::move(name);
        return EditResult{};
    });
}
EditResult Editor::create_entity(std::string name, Rect rect) {
    return impl_->edit("Create entity", [&](Delta& delta) {
        const EntityId id{impl_->next_id()};
        delta.entities.put(id, Entity{id, std::move(name), {}});
        delta.layout.put(ElementRef{id}, rect);
        return EditResult{true, {}, ElementRef{id}, {}};
    });
}
EditResult Editor::create_attribute(std::string name, Rect rect, std::optional<AttributeOwner> owner) {
    return impl_->edit("Create attribute", [&](Delta& delta) {
        const AttributeId id{impl_->next_id()};
        delta.attributes.put(id, Attribute{id, std::move(name), {}, AttributeKind::Normal, owner});
        delta.layout.put(ElementRef{id}, rect);
        return EditResult{true, {}, ElementRef{id}, {}};
    });
}
EditResult Editor::create_relationship(std::string name, Rect rect) {
    return impl_->edit("Create relationship", [&](Delta& delta) {
        const RelationshipId id{impl_->next_id()};
        delta.relationships.put(id, Relationship{id, std::move(name), {}, false, {}});
        delta.layout.put(ElementRef{id}, rect);
        return EditResult{true, {}, ElementRef{id}, {}};
    });
}

EditResult Editor::create_specialization(std::string name, Rect rect, Inheritance direction) {
    return impl_->edit(direction == Inheritance::Generalization ? "Create generalization" : "Create specialization",
                       [&](Delta& delta) {
        const SpecializationId id{impl_->next_id()};
        delta.specializations.put(id, Specialization{id, std::move(name), {}, direction, {}, {},
                                                     Disjointness::Disjoint, Completeness::Partial});
        delta.layout.put(ElementRef{id}, rect);
        return EditResult{true, {}, ElementRef{id}, {}};
    });
}
EditResult Editor::set_inheritance_direction(SpecializationId specialization, Inheritance direction) {
    return impl_->edit("Change ISA direction", [&](Delta& delta) {
        const auto found = project().specializations.find(specialization);
        if (found == project().specializations.end()) return failure("The specialization no longer exists.");
        if (found->second.direction == direction) return EditResult{};
        auto value = found->second;
        value.direction = direction;
        delta.specializations.put(specialization, std::move(value));
        return EditResult{};
    });
}
EditResult Editor::set_supertype(SpecializationId specialization, std::optional<EntityId> supertype) {
    return impl_->edit("Set supertype", [&](Delta& delta) {
        const auto found = project().specializations.find(specialization);
        if (found == project().specializations.end()) return failure("The specialization no longer exists.");
        if (supertype && !project().entities.contains(*supertype)) return failure("That entity no longer exists.");
        if (found->second.supertype == supertype) return EditResult{};
        auto value = found->second;
        value.supertype = supertype;
        delta.specializations.put(specialization, std::move(value));
        return EditResult{};
    });
}
EditResult Editor::attach_subtype(SpecializationId specialization, EntityId subtype) {
    return impl_->edit("Attach subtype", [&](Delta& delta) {
        const auto found = project().specializations.find(specialization);
        if (found == project().specializations.end() || !project().entities.contains(subtype))
            return failure("The specialization or entity no longer exists.");
        auto value = found->second;
        if (std::find(value.subtypes.begin(), value.subtypes.end(), subtype) != value.subtypes.end())
            return failure("That entity is already a subtype here.");
        value.subtypes.push_back(subtype);
        delta.specializations.put(specialization, std::move(value));
        return EditResult{};
    });
}
EditResult Editor::detach_subtype(SpecializationId specialization, EntityId subtype) {
    return impl_->edit("Detach subtype", [&](Delta& delta) {
        const auto found = project().specializations.find(specialization);
        if (found == project().specializations.end()) return failure("The specialization no longer exists.");
        auto value = found->second;
        if (std::erase(value.subtypes, subtype) == 0) return failure("That entity is not a subtype here.");
        delta.specializations.put(specialization, std::move(value));
        return EditResult{};
    });
}
EditResult Editor::set_specialization_rules(SpecializationId specialization,
                                            Disjointness constraint, Completeness completeness) {
    return impl_->edit("Change specialization rules", [&](Delta& delta) {
        const auto found = project().specializations.find(specialization);
        if (found == project().specializations.end()) return failure("The specialization no longer exists.");
        if (found->second.constraint == constraint && found->second.completeness == completeness) return EditResult{};
        auto value = found->second;
        value.constraint = constraint;
        value.completeness = completeness;
        delta.specializations.put(specialization, std::move(value));
        return EditResult{};
    });
}

namespace {
template<class Edit> EditResult edit_element(const Project& project, Delta& delta, ElementRef ref, Edit edit) {
    if (!exists(project, ref)) return failure("The element no longer exists.");
    std::visit([&](const auto& id) {
        using T = std::decay_t<decltype(id)>;
        if constexpr (std::is_same_v<T, EntityId>) {
            auto value = project.entities.at(id);
            edit(value);
            if (value != project.entities.at(id)) delta.entities.put(id, std::move(value));
        } else if constexpr (std::is_same_v<T, AttributeId>) {
            auto value = project.attributes.at(id);
            edit(value);
            if (value != project.attributes.at(id)) delta.attributes.put(id, std::move(value));
        } else if constexpr (std::is_same_v<T, SpecializationId>) {
            auto value = project.specializations.at(id);
            edit(value);
            if (value != project.specializations.at(id)) delta.specializations.put(id, std::move(value));
        } else {
            auto value = project.relationships.at(id);
            edit(value);
            if (value != project.relationships.at(id)) delta.relationships.put(id, std::move(value));
        }
    }, ref);
    return {};
}
} // namespace

EditResult Editor::rename(ElementRef ref, std::string name) {
    return impl_->edit("Rename element", [&](Delta& delta) {
        return edit_element(project(), delta, ref, [&](auto& value) { value.name = std::move(name); });
    });
}
EditResult Editor::describe(ElementRef ref, std::string description) {
    return impl_->edit("Edit description", [&](Delta& delta) {
        return edit_element(project(), delta, ref, [&](auto& value) { value.description = std::move(description); });
    });
}
EditResult Editor::set_attribute_kind(AttributeId id, AttributeKind kind) {
    return impl_->edit("Change attribute kind", [&](Delta& delta) {
        const auto found = project().attributes.find(id);
        if (found == project().attributes.end()) return failure("The attribute no longer exists.");
        if (found->second.kind != kind) { auto value = found->second; value.kind = kind; delta.attributes.put(id, std::move(value)); }
        return EditResult{};
    });
}
EditResult Editor::set_attribute_owner(AttributeId id, std::optional<AttributeOwner> owner) {
    return impl_->edit("Change attribute owner", [&](Delta& delta) {
        const auto found = project().attributes.find(id);
        if (found == project().attributes.end()) return failure("The attribute no longer exists.");
        if (found->second.owner != owner) {
            auto value = found->second;
            value.owner = owner;
            delta.attributes.put(id, std::move(value));
            // Detaching removes the link, and with it any shape given to it.
            if (!owner && project().connectors.contains(ConnectorRef{id})) delta.connectors.remove(id);
        }
        return EditResult{};
    });
}
EditResult Editor::connect(RelationshipId relationship, ParticipantTarget target) {
    return impl_->edit("Connect participant", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end() || !exists(project(), target_ref(target)))
            return failure("The relationship or its participant no longer exists.");
        const ParticipantId id{impl_->next_id()};
        auto value = found->second;
        value.participants.push_back({id, target, Cardinality::Many, Participation::Partial, {}});
        delta.relationships.put(relationship, std::move(value));
        return EditResult{true, {}, {}, id};
    });
}
EditResult Editor::set_associative(RelationshipId relationship, bool associative, std::optional<Rect> body) {
    return impl_->edit(associative ? "Make associative entity" : "Make plain relationship", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end()) return failure("The relationship no longer exists.");
        if (found->second.associative == associative) return EditResult{};
        auto value = found->second;
        value.associative = associative;
        delta.relationships.put(relationship, std::move(value));
        if (body) {
            const ElementRef ref{relationship};
            const auto layout = project().layout.find(ref);
            if (layout != project().layout.end() && layout->second != *body) delta.layout.put(ref, *body);
        }
        return EditResult{};
    });
}
EditResult Editor::update_participant(RelationshipId relationship, ParticipantId participant,
                                       Cardinality maximum, Participation participation, std::string role) {
    return impl_->edit("Edit participant", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end()) return failure("The relationship no longer exists.");
        auto value = found->second;
        const auto item = std::find_if(value.participants.begin(), value.participants.end(), [&](const auto& entry) { return entry.id == participant; });
        if (item == value.participants.end()) return failure("The participant does not belong to this relationship.");
        item->maximum = maximum;
        item->participation = participation;
        item->role = std::move(role);
        if (value != found->second) delta.relationships.put(relationship, std::move(value));
        return EditResult{};
    });
}
EditResult Editor::show_participant_constraints(RelationshipId relationship, ParticipantId participant,
                                                bool shown) {
    return impl_->edit(shown ? "Show constraints" : "Hide constraints", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end()) return failure("The relationship no longer exists.");
        auto value = found->second;
        const auto item = std::find_if(value.participants.begin(), value.participants.end(),
                                       [&](const auto& entry) { return entry.id == participant; });
        if (item == value.participants.end()) return failure("The participant does not belong to this relationship.");
        item->show_constraints = shown;
        if (value != found->second) delta.relationships.put(relationship, std::move(value));
        return EditResult{};
    });
}
EditResult Editor::set_ratio(RelationshipId relationship, Cardinality first, Cardinality second) {
    return impl_->edit("Change relationship ratio", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end()) return failure("The relationship no longer exists.");
        if (found->second.participants.size() != 2)
            return failure("A ratio describes exactly two participants. Edit each side separately.");
        auto value = found->second;
        value.participants[0].maximum = first;
        value.participants[1].maximum = second;
        if (value == found->second) return EditResult{};
        delta.relationships.put(relationship, std::move(value));
        return EditResult{};
    });
}
EditResult Editor::reverse_participants(RelationshipId relationship) {
    return impl_->edit("Reverse relationship", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end()) return failure("The relationship no longer exists.");
        if (found->second.participants.size() != 2)
            return failure("Reversing describes exactly two participants. Edit each side separately.");
        auto value = found->second;
        std::swap(value.participants[0].maximum, value.participants[1].maximum);
        std::swap(value.participants[0].participation, value.participants[1].participation);
        if (value == found->second) return EditResult{};
        delta.relationships.put(relationship, std::move(value));
        return EditResult{};
    });
}
EditResult Editor::disconnect(RelationshipId relationship, ParticipantId participant) {
    return impl_->edit("Disconnect participant", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end()) return failure("The relationship no longer exists.");
        auto value = found->second;
        const auto removed = std::erase_if(value.participants, [&](const auto& entry) { return entry.id == participant; });
        if (removed == 0) return failure("The participant does not belong to this relationship.");
        delta.relationships.put(relationship, std::move(value));
        if (project().connectors.contains(ConnectorRef{participant})) delta.connectors.remove(participant);
        return EditResult{};
    });
}
EditResult Editor::move(const std::map<ElementRef, Rect>& positions) {
    return impl_->edit("Move elements", [&](Delta& delta) {
        for (const auto& [ref, rect] : positions) {
            if (!exists(project(), ref)) return failure("An element no longer exists.");
            const auto found = project().layout.find(ref);
            if (found == project().layout.end() || found->second != rect) delta.layout.put(ref, rect);
        }
        return EditResult{};
    });
}
// Writes a reshaped connector back, dropping the record entirely once nothing
// on it differs from automatic routing. Both of the shaping commands end this
// way, so neither can leave an entry behind that says nothing.
namespace {
EditResult store_connector(const Project& project, Delta& delta, const ConnectorRef& ref,
                           const Connector& shaped) {
    const auto found = project.connectors.find(ref);
    const auto had = found != project.connectors.end();
    if (shaped.automatic()) {
        if (had) delta.connectors.remove(ref);
        return EditResult{};
    }
    if (had && found->second == shaped) return EditResult{};
    delta.connectors.put(ref, shaped);
    return EditResult{};
}
} // namespace

EditResult Editor::bend_connector(ConnectorRef ref, std::optional<double> offset) {
    return impl_->edit("Shape connector", [&](Delta& delta) {
        if (!connector_exists(project(), ref)) return failure("That connector no longer exists.");
        const auto found = project().connectors.find(ref);
        auto shaped = found == project().connectors.end() ? Connector{} : found->second;
        // A pinned join is a separate choice from the bend, so straightening
        // the line must not quietly discard it.
        shaped.offset = offset.value_or(0);
        return store_connector(project(), delta, ref, shaped);
    });
}

EditResult Editor::recolour(const std::vector<ElementRef>& elements, std::optional<Colour> colour) {
    // One edit for the whole selection, so recolouring several elements is a
    // single step to undo rather than one per element.
    return impl_->edit(colour ? "Set colour" : "Clear colour", [&](Delta& delta) {
        if (elements.empty()) return EditResult{};
        for (const auto& ref : elements)
            if (!exists(project(), ref)) return failure("One of those elements no longer exists.");
        for (const auto& ref : elements) {
            const auto found = project().colours.find(ref);
            const auto current = found == project().colours.end() ? std::optional<Colour>{} : std::optional{found->second};
            if (current == colour) continue;
            if (colour) delta.colours.put(ref, *colour);
            else delta.colours.remove(ref);
        }
        return EditResult{};
    });
}

EditResult Editor::route_connector(ConnectorRef ref, std::vector<domain::Point> waypoints) {
    return impl_->edit(waypoints.empty() ? "Straighten connector" : "Route connector", [&](Delta& delta) {
        if (!connector_exists(project(), ref)) return failure("That connector no longer exists.");
        const auto found = project().connectors.find(ref);
        auto shaped = found == project().connectors.end() ? Connector{} : found->second;
        // A route replaces the single bend rather than combining with it, so a
        // line never has two different opinions about its own shape.
        if (!waypoints.empty()) shaped.offset = 0;
        shaped.waypoints = std::move(waypoints);
        return store_connector(project(), delta, ref, shaped);
    });
}

EditResult Editor::pin_connector(ConnectorRef ref, std::optional<double> owner_anchor,
                                 std::optional<double> child_anchor) {
    const auto pinning = owner_anchor.has_value() || child_anchor.has_value();
    return impl_->edit(pinning ? "Lock connector" : "Unlock connector", [&](Delta& delta) {
        if (!connector_exists(project(), ref)) return failure("That connector no longer exists.");
        const auto found = project().connectors.find(ref);
        auto shaped = found == project().connectors.end() ? Connector{} : found->second;
        shaped.owner_anchor = owner_anchor;
        shaped.child_anchor = child_anchor;
        return store_connector(project(), delta, ref, shaped);
    });
}
EditResult Editor::erase(const std::vector<ElementRef>& elements,
                          const std::vector<std::pair<RelationshipId, ParticipantId>>& participants,
                          const std::vector<AttributeId>& detached_attributes,
                          const std::vector<InheritanceLink>& detached_inheritance) {
    return impl_->edit("Delete elements", [&](Delta& delta) {
        const auto removed = owned_closure(project(), elements);
        // Review 2026-09-15, finding 1: gather the inheritance links to cut,
        // per triangle, so they are applied in one pass with the cascade below
        // rather than each overwriting the other's version of the triangle.
        struct Cut { bool supertype = false; std::set<EntityId> subtypes; };
        std::map<SpecializationId, Cut> cuts;
        for (const auto& [specialization, subtype] : detached_inheritance) {
            if (!project().specializations.contains(specialization))
                return failure("A selected inheritance link no longer exists.");
            if (subtype) cuts[specialization].subtypes.insert(*subtype);
            else cuts[specialization].supertype = true;
        }
        // A connector shape outlives nothing: dropping the attribute link or
        // participant that draws it must drop the stored bend in the same edit,
        // so undo restores both together and validation never sees a dangling one.
        auto drop_connector = [&](const ConnectorRef& ref) {
            if (project().connectors.contains(ref)) delta.connectors.remove(ref);
        };
        std::map<RelationshipId, std::set<ParticipantId>> disconnected;
        for (const auto& [relationship, participant] : participants) {
            const auto found = project().relationships.find(relationship);
            if (found == project().relationships.end()
                || std::none_of(found->second.participants.begin(), found->second.participants.end(), [&](const auto& item) { return item.id == participant; }))
                return failure("A selected participant no longer belongs to this relationship.");
            disconnected[relationship].insert(participant);
        }
        for (const auto& id : detached_attributes) {
            const auto found = project().attributes.find(id);
            if (found == project().attributes.end()) return failure("A selected attribute no longer exists.");
            if (removed.contains(ElementRef{id}) || !found->second.owner) continue;
            auto value = found->second;
            value.owner.reset();
            delta.attributes.put(id, std::move(value));
            drop_connector(id);
        }
        for (const auto& ref : removed) {
            std::visit([&](const auto& id) {
                using T = std::decay_t<decltype(id)>;
                if constexpr (std::is_same_v<T, EntityId>) delta.entities.remove(id);
                else if constexpr (std::is_same_v<T, AttributeId>) { delta.attributes.remove(id); drop_connector(id); }
                else if constexpr (std::is_same_v<T, SpecializationId>) delta.specializations.remove(id);
                else {
                    delta.relationships.remove(id);
                    for (const auto& participant : project().relationships.at(id).participants)
                        drop_connector(participant.id);
                }
            }, ref);
            if (project().layout.contains(ref)) delta.layout.remove(ref);
            // A chosen colour outlives nothing either: it goes in the same edit,
            // so undo restores the element and its colour together and
            // validation never sees one pointing at something that is gone.
            if (project().colours.contains(ref)) delta.colours.remove(ref);
        }
        // A triangle without its supertype means nothing, so it goes with it;
        // a deleted subtype is simply detached from the ones that survive.
        for (const auto& [id, specialization] : project().specializations) {
            if (removed.contains(ElementRef{id})) continue;
            if (specialization.supertype && removed.contains(ElementRef{*specialization.supertype})) {
                delta.specializations.remove(id);
                if (project().layout.contains(ElementRef{id})) delta.layout.remove(ElementRef{id});
                // Review 2026-09-15, finding 4: the cascade dropped the triangle
                // and its layout but left its colour, and validation then
                // refused the whole deletion for a colour with no element.
                if (project().colours.contains(ElementRef{id})) delta.colours.remove(ElementRef{id});
                continue;
            }
            const auto cut = cuts.find(id);
            const auto gone = [&](const EntityId& subtype) {
                return removed.contains(ElementRef{subtype})
                    || (cut != cuts.end() && cut->second.subtypes.contains(subtype));
            };
            const bool cut_supertype = cut != cuts.end() && cut->second.supertype;
            if (!cut_supertype && std::none_of(specialization.subtypes.begin(), specialization.subtypes.end(), gone)) continue;
            auto value = specialization;
            std::erase_if(value.subtypes, gone);
            if (cut_supertype) value.supertype.reset();
            delta.specializations.put(id, std::move(value));
        }
        for (const auto& [id, relationship] : project().relationships) {
            if (removed.contains(ElementRef{id})) continue;
            const auto should_remove = [&](const auto& participant) {
                return removed.contains(target_ref(participant.target))
                    || (disconnected.contains(id) && disconnected.at(id).contains(participant.id));
            };
            const bool affected = std::any_of(relationship.participants.begin(), relationship.participants.end(), should_remove);
            if (!affected) continue;
            auto value = relationship;
            for (const auto& participant : value.participants)
                if (should_remove(participant)) drop_connector(participant.id);
            std::erase_if(value.participants, should_remove);
            delta.relationships.put(id, std::move(value));
        }
        return EditResult{};
    });
}
EditResult Editor::duplicate(const std::vector<ElementRef>& elements, double dx, double dy) {
    return impl_->edit("Duplicate elements", [&](Delta& delta) {
        const auto copied = owned_closure(project(), elements);
        std::map<ElementRef, ElementRef> mapping;
        for (const auto& ref : copied) {
            mapping.emplace(ref, std::visit([&](const auto& id) -> ElementRef {
                using T = std::decay_t<decltype(id)>;
                return T{impl_->next_id()};
            }, ref));
        }
        for (const auto& [original, replacement] : mapping) {
            std::visit([&](const auto& id) {
                using T = std::decay_t<decltype(id)>;
                const auto new_id = std::get<T>(replacement);
                if constexpr (std::is_same_v<T, EntityId>) {
                    auto value = project().entities.at(id);
                    value.id = new_id;
                    delta.entities.put(new_id, std::move(value));
                } else if constexpr (std::is_same_v<T, AttributeId>) {
                    auto value = project().attributes.at(id);
                    value.id = new_id;
                    if (value.owner && mapping.contains(*value.owner)) value.owner = mapping.at(*value.owner);
                    delta.attributes.put(new_id, std::move(value));
                    // A copy keeps the shape of the link it was copied from.
                    const auto shape = project().connectors.find(ConnectorRef{id});
                    if (shape != project().connectors.end() && value.owner)
                        delta.connectors.put(ConnectorRef{new_id}, shape->second);
                } else if constexpr (std::is_same_v<T, SpecializationId>) {
                    auto value = project().specializations.at(id);
                    value.id = new_id;
                    // Point the copy at copied supertype and subtypes where the
                    // selection included them, and at the originals otherwise.
                    if (value.supertype)
                        if (const auto found = mapping.find(ElementRef{*value.supertype}); found != mapping.end())
                            value.supertype = std::get<EntityId>(found->second);
                    for (auto& subtype : value.subtypes)
                        if (const auto found = mapping.find(ElementRef{subtype}); found != mapping.end())
                            subtype = std::get<EntityId>(found->second);
                    delta.specializations.put(new_id, std::move(value));
                } else {
                    auto value = project().relationships.at(id);
                    value.id = new_id;
                    for (auto& participant : value.participants) {
                        const auto original_participant = participant.id;
                        participant.id = ParticipantId{impl_->next_id()};
                        if (mapping.contains(target_ref(participant.target))) {
                            const auto replacement_target = mapping.at(target_ref(participant.target));
                            if (const auto* copied_entity = std::get_if<EntityId>(&replacement_target)) participant.target = *copied_entity;
                            else if (const auto* copied_rel = std::get_if<RelationshipId>(&replacement_target)) participant.target = *copied_rel;
                        }
                        const auto shape = project().connectors.find(ConnectorRef{original_participant});
                        if (shape != project().connectors.end())
                            delta.connectors.put(ConnectorRef{participant.id}, shape->second);
                    }
                    delta.relationships.put(new_id, std::move(value));
                }
            }, original);
            const auto found = project().layout.find(original);
            auto rect = found == project().layout.end() ? Rect{} : found->second;
            rect.x += dx;
            rect.y += dy;
            delta.layout.put(replacement, rect);
            // A copy looks like what it was copied from, so it carries the
            // colour the original was given along with its shape.
            const auto colour = project().colours.find(original);
            if (colour != project().colours.end()) delta.colours.put(replacement, colour->second);
        }
        EditResult result;
        if (!elements.empty()) result.created = mapping.at(elements.front());
        return result;
    });
}
EditResult Editor::undo() {
    if (!can_undo()) return failure("There is nothing to undo.");
    auto& delta = impl_->history[impl_->cursor - 1];
    delta->toggle(impl_->project);
    impl_->state = delta->before_state;
    --impl_->cursor;
    ++impl_->revision;
    return {};
}
EditResult Editor::redo() {
    if (!can_redo()) return failure("There is nothing to redo.");
    auto& delta = impl_->history[impl_->cursor];
    delta->toggle(impl_->project);
    impl_->state = delta->after_state;
    ++impl_->cursor;
    ++impl_->revision;
    return {};
}

} // namespace erdflow::application
