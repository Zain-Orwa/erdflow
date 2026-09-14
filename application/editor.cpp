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
std::size_t payload(const Rect&) { return 0; }

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
    Changes<ElementRef, Rect> layout;
    std::size_t bytes = 0;
    std::uint64_t before_state = 0;
    std::uint64_t after_state = 0;

    [[nodiscard]] bool empty() const {
        return !project_name && entities.keys.empty() && attributes.keys.empty()
            && relationships.keys.empty() && layout.keys.empty();
    }
    void toggle(Project& project) {
        if (project_name) project.name.swap(*project_name);
        entities.toggle(project.entities);
        attributes.toggle(project.attributes);
        relationships.toggle(project.relationships);
        layout.toggle(project.layout);
    }
    [[nodiscard]] std::size_t estimate(const Project& project) const {
        return sizeof(Delta) + sizeof(std::unique_ptr<Delta>) + label.capacity()
            + (project_name ? project_name->capacity() + project.name.capacity() : 0)
            + cost(entities, project.entities) + cost(attributes, project.attributes)
            + cost(relationships, project.relationships) + cost(layout, project.layout);
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
        delta.relationships.put(id, Relationship{id, std::move(name), {}, {}});
        delta.layout.put(ElementRef{id}, rect);
        return EditResult{true, {}, ElementRef{id}, {}};
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
        if (found->second.owner != owner) { auto value = found->second; value.owner = owner; delta.attributes.put(id, std::move(value)); }
        return EditResult{};
    });
}
EditResult Editor::connect(RelationshipId relationship, EntityId entity) {
    return impl_->edit("Connect participant", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end() || !project().entities.contains(entity))
            return failure("The relationship or entity no longer exists.");
        const ParticipantId id{impl_->next_id()};
        auto value = found->second;
        value.participants.push_back({id, entity, Cardinality::Many, Participation::Partial, {}});
        delta.relationships.put(relationship, std::move(value));
        return EditResult{true, {}, {}, id};
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
EditResult Editor::disconnect(RelationshipId relationship, ParticipantId participant) {
    return impl_->edit("Disconnect participant", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end()) return failure("The relationship no longer exists.");
        auto value = found->second;
        const auto removed = std::erase_if(value.participants, [&](const auto& entry) { return entry.id == participant; });
        if (removed == 0) return failure("The participant does not belong to this relationship.");
        delta.relationships.put(relationship, std::move(value));
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
EditResult Editor::erase(const std::vector<ElementRef>& elements,
                          const std::vector<std::pair<RelationshipId, ParticipantId>>& participants,
                          const std::vector<AttributeId>& detached_attributes) {
    return impl_->edit("Delete elements", [&](Delta& delta) {
        const auto removed = owned_closure(project(), elements);
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
        }
        for (const auto& ref : removed) {
            std::visit([&](const auto& id) {
                using T = std::decay_t<decltype(id)>;
                if constexpr (std::is_same_v<T, EntityId>) delta.entities.remove(id);
                else if constexpr (std::is_same_v<T, AttributeId>) delta.attributes.remove(id);
                else delta.relationships.remove(id);
            }, ref);
            if (project().layout.contains(ref)) delta.layout.remove(ref);
        }
        for (const auto& [id, relationship] : project().relationships) {
            if (removed.contains(ElementRef{id})) continue;
            const auto should_remove = [&](const auto& participant) {
                return removed.contains(ElementRef{participant.entity})
                    || (disconnected.contains(id) && disconnected.at(id).contains(participant.id));
            };
            const bool affected = std::any_of(relationship.participants.begin(), relationship.participants.end(), should_remove);
            if (!affected) continue;
            auto value = relationship;
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
                } else {
                    auto value = project().relationships.at(id);
                    value.id = new_id;
                    for (auto& participant : value.participants) {
                        participant.id = ParticipantId{impl_->next_id()};
                        if (mapping.contains(ElementRef{participant.entity})) participant.entity = std::get<EntityId>(mapping.at(ElementRef{participant.entity}));
                    }
                    delta.relationships.put(new_id, std::move(value));
                }
            }, original);
            const auto found = project().layout.find(original);
            auto rect = found == project().layout.end() ? Rect{} : found->second;
            rect.x += dx;
            rect.y += dy;
            delta.layout.put(replacement, rect);
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
