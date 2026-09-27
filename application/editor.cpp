// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
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
// A picture's bytes are the one large thing history can hold, so a deleted
// picture counts its image against the undo budget like any other payload.
std::size_t payload(const Picture& value) {
    return value.name.capacity() + value.description.capacity() + value.image.capacity();
}
std::size_t payload(const Note& value) { return value.name.capacity() + value.description.capacity(); }
std::size_t payload(const Rect&) { return 0; }
std::size_t payload(const Colour&) { return 0; }
std::size_t payload(const LetteringBase&) { return 0; }
std::size_t payload(std::uint8_t) { return 0; }
std::size_t payload(double) { return 0; }
// A connector holds its bend and joins inline; only the route is on the heap.
std::size_t payload(const Connector& value) { return value.waypoints.capacity() * sizeof(Point); }
// A comment holds its text and the list of what it is pinned to on the heap.
std::size_t payload(const Comment& value) {
    return value.text.capacity() + value.targets.capacity() * sizeof(CommentTarget);
}

// The three fields history swaps whole rather than key by key. They are the
// most expensive kind of delta there is -- a single answer stores a copy of
// every answer alongside it -- and until now they were the only fields that
// cost the undo budget nothing at all, so a history of them could grow past
// the 32 MiB it is allowed without the budget noticing.
std::size_t entries(std::size_t count, std::size_t each) { return count * (each + 4 * sizeof(void*)); }
std::size_t payload(const ConversionDecisions& value) {
    std::size_t result =
        entries(value.isa.size(), sizeof(std::pair<const SpecializationId, IsaStrategy>))
        + entries(value.composite.size(), sizeof(std::pair<const AttributeId, CompositeMode>))
        + entries(value.one_to_one_key.size(), sizeof(std::pair<const RelationshipId, ParticipantId>))
        + entries(value.junction_name.size(), sizeof(std::pair<const RelationshipId, std::string>))
        + entries(value.bridge_key.size(), sizeof(std::pair<const RelationshipId, BridgeKey>))
        + entries(value.identifier.size(), sizeof(std::pair<const EntityId, AttributeId>))
        + entries(value.table_name.size(), sizeof(std::pair<const ElementRef, std::string>));
    // A typed name is the only thing here that is on the heap.
    for (const auto& [id, chosen] : value.junction_name) { (void)id; result += chosen.capacity(); }
    for (const auto& [ref, chosen] : value.table_name) { (void)ref; result += chosen.capacity(); }
    return result;
}
std::size_t payload(const SchemaColumn& value) { return value.name.capacity() + value.comment.capacity(); }
std::size_t payload(const SchemaOverrides& value) {
    std::size_t result = entries(value.added.size(), sizeof(std::pair<const ElementRef, std::vector<SchemaColumn>>))
        + entries(value.hidden.size(), sizeof(AttributeId))
        + entries(value.key_names.size(), sizeof(std::pair<const ElementRef, std::string>));
    for (const auto& [table, chosen] : value.key_names) { (void)table; result += chosen.capacity(); }
    for (const auto& [table, columns] : value.added) {
        (void)table;
        result += columns.capacity() * sizeof(SchemaColumn);
        for (const auto& column : columns) result += payload(column);
    }
    return result;
}
std::size_t payload(const SchemaLayout& value) {
    std::size_t result = entries(value.tables.size(), sizeof(std::pair<const ElementRef, Point>))
        + entries(value.widths.size() + value.heights.size(), sizeof(std::pair<const ElementRef, double>))
        + entries(value.lines.size(), sizeof(std::pair<const LinkSource, SchemaLine>));
    // A route is the one thing here that grows without bound: a line carries as
    // many corners as a hand cares to put in it.
    for (const auto& [link, line] : value.lines) { (void)link; result += line.route.capacity() * sizeof(Point); }
    return result;
}

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
    std::optional<std::string> project_description;
    std::optional<Background> background;
    std::optional<ConversionDecisions> decisions;
    std::optional<SchemaOverrides> schema;
    std::optional<SchemaLayout> schema_layout;
    Changes<EntityId, Entity> entities;
    Changes<AttributeId, Attribute> attributes;
    Changes<RelationshipId, Relationship> relationships;
    Changes<SpecializationId, Specialization> specializations;
    Changes<PictureId, Picture> pictures;
    Changes<NoteId, Note> notes;
    Changes<CommentId, Comment> comments;
    Changes<ElementRef, Rect> layout;
    Changes<ConnectorRef, Connector> connectors;
    Changes<ElementRef, Colour> colours;
    Changes<ElementRef, std::uint8_t> transparency;
    Changes<ElementRef, LetteringBase> lettering;
    std::size_t bytes = 0;
    std::uint64_t before_state = 0;
    std::uint64_t after_state = 0;

    [[nodiscard]] bool empty() const {
        return !project_name && !project_description && !background && !decisions && !schema && !schema_layout
            && entities.keys.empty() && attributes.keys.empty()
            && relationships.keys.empty() && specializations.keys.empty()
            && pictures.keys.empty() && notes.keys.empty() && comments.keys.empty()
            && layout.keys.empty() && connectors.keys.empty() && colours.keys.empty()
            && transparency.keys.empty() && lettering.keys.empty();
    }
    void toggle(Project& project) {
        if (project_name) project.name.swap(*project_name);
        if (project_description) project.description.swap(*project_description);
        if (background) std::swap(project.background, *background);
        if (decisions) std::swap(project.decisions, *decisions);
        if (schema) std::swap(project.schema, *schema);
        if (schema_layout) std::swap(project.schema_layout, *schema_layout);
        entities.toggle(project.entities);
        attributes.toggle(project.attributes);
        relationships.toggle(project.relationships);
        specializations.toggle(project.specializations);
        pictures.toggle(project.pictures);
        notes.toggle(project.notes);
        comments.toggle(project.comments);
        layout.toggle(project.layout);
        connectors.toggle(project.connectors);
        colours.toggle(project.colours);
        transparency.toggle(project.transparency);
        lettering.toggle(project.lettering);
    }
    [[nodiscard]] std::size_t estimate(const Project& project) const {
        return sizeof(Delta) + sizeof(std::unique_ptr<Delta>) + label.capacity()
            + (project_name ? project_name->capacity() + project.name.capacity() : 0)
            + (project_description ? project_description->capacity() + project.description.capacity() : 0)
            + (background ? background->image.capacity() + project.background.image.capacity() : 0)
            // Both directions, because toggle() exchanges the delta's copy with
            // the project's and history retains whichever is not in use.
            + (decisions ? payload(*decisions) + payload(project.decisions) : 0)
            + (schema ? payload(*schema) + payload(project.schema) : 0)
            + (schema_layout ? payload(*schema_layout) + payload(project.schema_layout) : 0)
            + cost(entities, project.entities) + cost(attributes, project.attributes)
            + cost(relationships, project.relationships) + cost(specializations, project.specializations)
            + cost(pictures, project.pictures) + cost(notes, project.notes)
            + cost(comments, project.comments)
            + cost(layout, project.layout) + cost(connectors, project.connectors)
            + cost(colours, project.colours) + cost(transparency, project.transparency)
            + cost(lettering, project.lettering);
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
        // A project made before a key oval and the primary key were one fact
        // may hold one without the other. It is brought into agreement on the
        // way in: an attribute drawn as a key is the primary key, and one
        // marked as the primary key is drawn as a key (Zain, 2026-09-24).
        // Composite, multivalued and derived attributes are left as they are:
        // none of them can be drawn as a key.
        for (auto& [id, attribute] : project.attributes) {
            (void)id;
            if (attribute.kind == AttributeKind::Key) {
                attribute.identifier = true;
                attribute.required = true;
            } else if (attribute.identifier && attribute.kind == AttributeKind::Normal) {
                attribute.kind = AttributeKind::Key;
            }
        }
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
        for (const auto& [id, specialization] : project.specializations) { (void)specialization; identities.insert(id.value); }
        for (const auto& [id, picture] : project.pictures) { (void)picture; identities.insert(id.value); }
        for (const auto& [id, note] : project.notes) { (void)note; identities.insert(id.value); }
        for (const auto& [id, comment] : project.comments) { (void)comment; identities.insert(id.value); }
        // A column the schema added carries an identity like everything else,
        // and the guard above can only refuse to reissue what it is told was
        // issued. Left out here, a column's identity was forgotten the moment
        // its project was opened, and the generator was free to hand it out
        // again to something else.
        for (const auto& [table, columns] : project.schema.added) {
            (void)table;
            for (const auto& column : columns) identities.insert(column.id.value);
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
EditResult Editor::describe_project(std::string description) {
    return impl_->edit("Describe project", [&](Delta& delta) {
        if (description != project().description) delta.project_description = std::move(description);
        return EditResult{};
    });
}
EditResult Editor::create_entity(std::string name, Rect rect) {
    return impl_->edit("Create entity", [&](Delta& delta) {
        const EntityId id{impl_->next_id()};
        delta.entities.put(id, Entity{.id = id, .name = std::move(name)});
        delta.layout.put(ElementRef{id}, rect);
        return EditResult{true, {}, ElementRef{id}, {}};
    });
}
EditResult Editor::create_attribute(std::string name, Rect rect, std::optional<AttributeOwner> owner) {
    return impl_->edit("Create attribute", [&](Delta& delta) {
        const AttributeId id{impl_->next_id()};
        delta.attributes.put(id, Attribute{.id = id, .name = std::move(name), .kind = AttributeKind::Normal, .owner = owner});
        delta.layout.put(ElementRef{id}, rect);
        return EditResult{true, {}, ElementRef{id}, {}};
    });
}
EditResult Editor::create_relationship(std::string name, Rect rect) {
    return impl_->edit("Create relationship", [&](Delta& delta) {
        const RelationshipId id{impl_->next_id()};
        delta.relationships.put(id, Relationship{.id = id, .name = std::move(name)});
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
EditResult Editor::create_picture(std::string name, Rect rect, std::vector<std::uint8_t> image) {
    return impl_->edit("Insert picture", [&](Delta& delta) {
        const PictureId id{impl_->next_id()};
        delta.pictures.put(id, Picture{id, std::move(name), {}, std::move(image)});
        delta.layout.put(ElementRef{id}, rect);
        return EditResult{true, {}, ElementRef{id}, {}};
    });
}
EditResult Editor::create_note(std::string name, Rect rect, std::string text) {
    return impl_->edit("Insert note", [&](Delta& delta) {
        const NoteId id{impl_->next_id()};
        delta.notes.put(id, Note{id, std::move(name), std::move(text), false});
        delta.layout.put(ElementRef{id}, rect);
        return EditResult{true, {}, ElementRef{id}, {}};
    });
}
EditResult Editor::create_symbol(std::string character, Rect rect) {
    return impl_->edit("Insert symbol", [&](Delta& delta) {
        const NoteId id{impl_->next_id()};
        delta.notes.put(id, Note{id, std::move(character), {}, true});
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
// Editing text that a comment is pinned into moves the ground under the remark.
// Refusing the edit would be worse than useless -- a name could not be
// shortened once anybody had commented on it -- and dropping the remark loses
// what a reviewer said. So each range is held inside the text the field now
// has: one that ran past the new end is shortened to reach it, and one that
// began past the end becomes a caret at the end. The remark stays on the field
// it was left on, which is what its reader wants, even when the exact words it
// pointed at have gone.
void hold_anchors(const Project& project, Delta& delta, const ElementRef& ref, TextField field,
                  std::size_t characters) {
    for (const auto& [id, comment] : project.comments) {
        bool moved = false;
        auto value = comment;
        for (auto& target : value.targets) {
            auto* anchor = std::get_if<TextAnchor>(&target);
            if (!anchor || anchor->owner != ref || anchor->field != field) continue;
            const auto limit = static_cast<std::uint32_t>(std::min<std::size_t>(characters, 0xffffffffU));
            const auto begin = std::min(anchor->begin, limit);
            const auto length = std::min(anchor->length, limit - begin);
            if (begin == anchor->begin && length == anchor->length) continue;
            anchor->begin = begin;
            anchor->length = length;
            moved = true;
        }
        if (moved) delta.comments.put(id, std::move(value));
    }
}

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
        } else if constexpr (std::is_same_v<T, PictureId>) {
            auto value = project.pictures.at(id);
            edit(value);
            if (value != project.pictures.at(id)) delta.pictures.put(id, std::move(value));
        } else if constexpr (std::is_same_v<T, NoteId>) {
            auto value = project.notes.at(id);
            edit(value);
            if (value != project.notes.at(id)) delta.notes.put(id, std::move(value));
        } else {
            auto value = project.relationships.at(id);
            edit(value);
            if (value != project.relationships.at(id)) delta.relationships.put(id, std::move(value));
        }
    }, ref);
    return {};
}
} // namespace

EditResult Editor::merge_project(const Project& other, double dx, double dy) {
    return impl_->edit("Import project", [&](Delta& delta) {
        if (&other == &project()) return failure("A project cannot be imported into itself.");
        // Every element of the incoming project gets an identity of its own
        // here, so nothing it carries can collide with anything already drawn.
        std::map<ElementRef, ElementRef> mapping;
        const auto claim = [&](const ElementRef& ref) {
            mapping.emplace(ref, std::visit([&](const auto& id) -> ElementRef {
                using T = std::decay_t<decltype(id)>;
                (void)id;
                return T{impl_->next_id()};
            }, ref));
        };
        for (const auto& [id, value] : other.entities) { (void)value; claim(ElementRef{id}); }
        for (const auto& [id, value] : other.attributes) { (void)value; claim(ElementRef{id}); }
        for (const auto& [id, value] : other.relationships) { (void)value; claim(ElementRef{id}); }
        for (const auto& [id, value] : other.specializations) { (void)value; claim(ElementRef{id}); }
        for (const auto& [id, value] : other.pictures) { (void)value; claim(ElementRef{id}); }
        for (const auto& [id, value] : other.notes) { (void)value; claim(ElementRef{id}); }

        // The links are renamed too, so a shape drawn on an incoming line
        // follows it in rather than being left pointing at the old identity.
        std::map<ConnectorRef, ConnectorRef> links;
        for (const auto& [id, attribute] : other.attributes) {
            (void)attribute;
            links.emplace(ConnectorRef{id}, ConnectorRef{std::get<AttributeId>(mapping.at(ElementRef{id}))});
        }

        for (const auto& [id, entity] : other.entities) {
            auto value = entity;
            value.id = std::get<EntityId>(mapping.at(ElementRef{id}));
            delta.entities.put(value.id, std::move(value));
        }
        for (const auto& [id, attribute] : other.attributes) {
            auto value = attribute;
            value.id = std::get<AttributeId>(mapping.at(ElementRef{id}));
            if (value.owner) value.owner = mapping.at(*value.owner);
            delta.attributes.put(value.id, std::move(value));
        }
        for (const auto& [id, relationship] : other.relationships) {
            auto value = relationship;
            value.id = std::get<RelationshipId>(mapping.at(ElementRef{id}));
            for (auto& participant : value.participants) {
                const auto was = participant.id;
                participant.id = ParticipantId{impl_->next_id()};
                links.emplace(ConnectorRef{was}, ConnectorRef{participant.id});
                const auto target = mapping.at(target_ref(participant.target));
                if (const auto* entity = std::get_if<EntityId>(&target)) participant.target = *entity;
                else participant.target = std::get<RelationshipId>(target);
            }
            delta.relationships.put(value.id, std::move(value));
        }
        for (const auto& [id, hierarchy] : other.specializations) {
            auto value = hierarchy;
            value.id = std::get<SpecializationId>(mapping.at(ElementRef{id}));
            if (value.supertype) value.supertype = std::get<EntityId>(mapping.at(ElementRef{*value.supertype}));
            for (auto& subtype : value.subtypes) subtype = std::get<EntityId>(mapping.at(ElementRef{subtype}));
            delta.specializations.put(value.id, std::move(value));
        }
        for (const auto& [id, picture] : other.pictures) {
            auto value = picture;
            value.id = std::get<PictureId>(mapping.at(ElementRef{id}));
            delta.pictures.put(value.id, std::move(value));
        }
        for (const auto& [id, note] : other.notes) {
            auto value = note;
            value.id = std::get<NoteId>(mapping.at(ElementRef{id}));
            delta.notes.put(value.id, std::move(value));
        }
        // What was said about the work comes with the work.
        for (const auto& [id, comment] : other.comments) {
            (void)id;
            auto value = comment;
            value.id = CommentId{impl_->next_id()};
            for (auto& target : value.targets) {
                if (auto* element = std::get_if<ElementRef>(&target)) *element = mapping.at(*element);
                else if (auto* link = std::get_if<ConnectorRef>(&target)) *link = links.at(*link);
                else std::get<TextAnchor>(target).owner = mapping.at(std::get<TextAnchor>(target).owner);
            }
            delta.comments.put(value.id, std::move(value));
        }
        // Moved clear of what is already drawn, so an import never lands on
        // top of the work it is joining.
        for (const auto& [ref, rect] : other.layout) {
            auto moved = rect;
            moved.x += dx;
            moved.y += dy;
            delta.layout.put(mapping.at(ref), moved);
        }
        for (const auto& [ref, colour] : other.colours) delta.colours.put(mapping.at(ref), colour);
        for (const auto& [ref, percent] : other.transparency) delta.transparency.put(mapping.at(ref), percent);
        for (const auto& [ref, base] : other.lettering) delta.lettering.put(mapping.at(ref), base);
        for (const auto& [ref, connector] : other.connectors) {
            const auto found = links.find(ref);
            if (found == links.end()) continue;
            auto shaped = connector;
            for (auto& point : shaped.waypoints) { point.x += dx; point.y += dy; }
            delta.connectors.put(found->second, std::move(shaped));
        }
        if (delta.empty()) return failure("That project has nothing in it to import.");
        return EditResult{};
    });
}

EditResult Editor::rename(ElementRef ref, std::string name) {
    return impl_->edit("Rename element", [&](Delta& delta) {
        const auto characters = character_count(name);
        auto result = edit_element(project(), delta, ref, [&](auto& value) { value.name = std::move(name); });
        if (result) hold_anchors(project(), delta, ref, TextField::Name, characters);
        return result;
    });
}
EditResult Editor::describe(ElementRef ref, std::string description) {
    return impl_->edit("Edit description", [&](Delta& delta) {
        const auto characters = character_count(description);
        auto result = edit_element(project(), delta, ref,
                                   [&](auto& value) { value.description = std::move(description); });
        if (result) hold_anchors(project(), delta, ref, TextField::Description, characters);
        return result;
    });
}

EditResult Editor::create_comment(std::string text, std::vector<CommentTarget> targets) {
    return impl_->edit("Add comment", [&](Delta& delta) {
        if (targets.empty()) return failure("A comment must be pinned to something.");
        for (const auto& target : targets)
            if (!target_exists(project(), target))
                return failure("A comment cannot be pinned to something that is not there.");
        Comment comment;
        comment.id = CommentId{impl_->next_id()};
        comment.text = std::move(text);
        comment.targets = std::move(targets);
        delta.comments.put(comment.id, std::move(comment));
        return EditResult{};
    });
}

EditResult Editor::set_comment_text(CommentId id, std::string text) {
    return impl_->edit("Edit comment", [&](Delta& delta) {
        const auto found = project().comments.find(id);
        if (found == project().comments.end()) return failure("The comment no longer exists.");
        if (found->second.text == text) return EditResult{};
        auto value = found->second;
        value.text = std::move(text);
        delta.comments.put(id, std::move(value));
        return EditResult{};
    });
}

EditResult Editor::set_comment_targets(CommentId id, std::vector<CommentTarget> targets) {
    return impl_->edit("Change what a comment is pinned to", [&](Delta& delta) {
        const auto found = project().comments.find(id);
        if (found == project().comments.end()) return failure("The comment no longer exists.");
        if (targets.empty()) return failure("A comment must stay pinned to something. Delete it instead.");
        for (const auto& target : targets)
            if (!target_exists(project(), target))
                return failure("A comment cannot be pinned to something that is not there.");
        if (found->second.targets == targets) return EditResult{};
        auto value = found->second;
        value.targets = std::move(targets);
        delta.comments.put(id, std::move(value));
        return EditResult{};
    });
}

EditResult Editor::set_comment_hidden(CommentId id, bool hidden) {
    return impl_->edit(hidden ? "Hide comment" : "Show comment", [&](Delta& delta) {
        const auto found = project().comments.find(id);
        if (found == project().comments.end()) return failure("The comment no longer exists.");
        if (found->second.hidden == hidden) return EditResult{};
        auto value = found->second;
        value.hidden = hidden;
        delta.comments.put(id, std::move(value));
        return EditResult{};
    });
}

EditResult Editor::erase_comment(CommentId id) {
    return impl_->edit("Delete comment", [&](Delta& delta) {
        if (!project().comments.contains(id)) return failure("The comment no longer exists.");
        delta.comments.remove(id);
        return EditResult{};
    });
}
EditResult Editor::set_schema_comment(ElementRef ref, std::string comment) {
    return impl_->edit("Edit comment", [&](Delta& delta) {
        // Only what becomes a table or a column carries one. A triangle, a
        // picture and a note become nothing, so a comment on one would be
        // written for a reader that never arrives.
        if (!std::holds_alternative<EntityId>(ref) && !std::holds_alternative<AttributeId>(ref)
            && !std::holds_alternative<RelationshipId>(ref))
            return failure("Only entities, attributes and relationships carry a schema comment.");
        return edit_element(project(), delta, ref, [&](auto& value) {
            if constexpr (requires { value.comment; }) value.comment = std::move(comment);
        });
    });
}

namespace {
// Every decision command is the same shape: take a copy, change one answer,
// and keep it only if it actually changed anything.
template<class Change>
ConversionDecisions decided_with(const ConversionDecisions& from, Change&& change) {
    auto copy = from;
    change(copy);
    return copy;
}
} // namespace

EditResult Editor::set_table_naming(TableNaming naming) {
    return impl_->edit("Set table naming", [&](Delta& delta) {
        auto value = decided_with(project().decisions, [&](ConversionDecisions& d) { d.naming = naming; });
        if (value == project().decisions) return EditResult{};
        delta.decisions = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_isa_strategy(SpecializationId id, std::optional<IsaStrategy> strategy) {
    return impl_->edit("Set mapping strategy", [&](Delta& delta) {
        if (!project().specializations.contains(id)) return failure("The hierarchy no longer exists.");
        auto value = decided_with(project().decisions, [&](ConversionDecisions& d) {
            if (strategy) d.isa[id] = *strategy; else d.isa.erase(id);
        });
        if (value == project().decisions) return EditResult{};
        delta.decisions = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_composite_mode(AttributeId id, std::optional<CompositeMode> mode) {
    return impl_->edit("Set composite mapping", [&](Delta& delta) {
        const auto found = project().attributes.find(id);
        if (found == project().attributes.end()) return failure("The attribute no longer exists.");
        if (found->second.kind != AttributeKind::Composite)
            return failure("Only a composite attribute is asked what it becomes.");
        auto value = decided_with(project().decisions, [&](ConversionDecisions& d) {
            if (mode) d.composite[id] = *mode; else d.composite.erase(id);
        });
        if (value == project().decisions) return EditResult{};
        delta.decisions = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_bridge_key(RelationshipId id, std::optional<BridgeKey> keyed) {
    return impl_->edit("Choose what keys the bridge", [&](Delta& delta) {
        if (!project().relationships.contains(id)) return failure("The relationship no longer exists.");
        auto value = decided_with(project().decisions, [&](ConversionDecisions& d) {
            if (keyed) d.bridge_key[id] = *keyed; else d.bridge_key.erase(id);
        });
        if (value == project().decisions) return EditResult{};
        delta.decisions = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_one_to_one_key(RelationshipId id, std::optional<ParticipantId> side) {
    return impl_->edit("Choose which side keeps the key", [&](Delta& delta) {
        const auto found = project().relationships.find(id);
        if (found == project().relationships.end()) return failure("The relationship no longer exists.");
        if (side) {
            const auto& sides = found->second.participants;
            if (std::none_of(sides.begin(), sides.end(),
                             [&](const Participant& one) { return one.id == *side; }))
                return failure("That side does not belong to this relationship.");
        }
        auto value = decided_with(project().decisions, [&](ConversionDecisions& d) {
            if (side) d.one_to_one_key[id] = *side; else d.one_to_one_key.erase(id);
        });
        if (value == project().decisions) return EditResult{};
        delta.decisions = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_junction_name(RelationshipId id, std::string chosen) {
    return impl_->edit("Name the bridge table", [&](Delta& delta) {
        if (!project().relationships.contains(id)) return failure("The relationship no longer exists.");
        auto value = decided_with(project().decisions, [&](ConversionDecisions& d) {
            if (chosen.empty()) d.junction_name.erase(id); else d.junction_name[id] = chosen;
        });
        if (value == project().decisions) return EditResult{};
        delta.decisions = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_entity_identifier(EntityId id, std::optional<AttributeId> chosen) {
    return impl_->edit("Choose the identifier", [&](Delta& delta) {
        if (!project().entities.contains(id)) return failure("The entity no longer exists.");
        if (chosen) {
            const auto found = project().attributes.find(*chosen);
            if (found == project().attributes.end() || found->second.owner != AttributeOwner{ElementRef{id}})
                return failure("That attribute does not belong to this entity.");
        }
        auto value = decided_with(project().decisions, [&](ConversionDecisions& d) {
            if (chosen) d.identifier[id] = *chosen; else d.identifier.erase(id);
        });
        if (value == project().decisions) return EditResult{};
        delta.decisions = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_table_name(ElementRef ref, std::string chosen) {
    return impl_->edit("Rename the table", [&](Delta& delta) {
        if (!exists(project(), ref)) return failure("The element no longer exists.");
        auto value = decided_with(project().decisions, [&](ConversionDecisions& d) {
                const auto relation = relation_from(ref);
            if (chosen.empty()) d.table_name.erase(relation); else d.table_name[relation] = chosen;
        });
        if (value == project().decisions) return EditResult{};
        delta.decisions = std::move(value);
        return EditResult{};
    });
}

namespace {
// The overrides with one change made to them, in the same shape as the helper
// the conversion decisions use, so the two read alike.
template<class Change>
SchemaOverrides schema_with(const SchemaOverrides& from, Change&& change) {
    auto value = from;
    change(value);
    return value;
}

// Where a schema-only column lives, since it is held under the table it was
// added to rather than in a list of its own.
const SchemaColumn* find_schema_column(const SchemaOverrides& schema, SchemaColumnId id) {
    for (const auto& [table, columns] : schema.added) {
        (void)table;
        for (const auto& column : columns)
            if (column.id == id) return &column;
    }
    return nullptr;
}
} // namespace

EditResult Editor::add_schema_column(ElementRef table, std::string name) {
    return impl_->edit("Add a column", [&](Delta& delta) {
        if (!exists(project(), table)) return failure("The element no longer exists.");
        if (name.empty()) return failure("A column needs a name.");
        SchemaColumn column;
        column.id = SchemaColumnId{impl_->next_id()};
        column.name = std::move(name);
        auto value = schema_with(project().schema, [&](SchemaOverrides& s) {
            s.added[relation_from(table)].push_back(std::move(column));
        });
        delta.schema = std::move(value);
        return EditResult{};
    });
}

// Renaming a table renames the element it came from.
//
// The schema and the diagram are two views of one model, so a name typed in
// one of them is the name in both. This is not the kind of difference ADR-010
// is about: recording a column the schema has and the diagram does not is a
// fact about structure, while two names for one thing is only a disagreement.
// Any name previously typed over the derived one is given up in the same edit,
// since it would otherwise go on masking the name just chosen.
EditResult Editor::rename_table(ElementRef ref, std::string name) {
    return impl_->edit("Rename table", [&](Delta& delta) {
        const auto characters = character_count(name);
        auto result = edit_element(project(), delta, ref, [&](auto& value) { value.name = std::move(name); });
        if (!result) return result;
        hold_anchors(project(), delta, ref, TextField::Name, characters);
        if (project().decisions.table_name.contains(relation_from(ref))) {
            auto decided = project().decisions;
            decided.table_name.erase(relation_from(ref));
            delta.decisions = std::move(decided);
        }
        return result;
    });
}

// What a key the conversion invented is called. Generating the key is right --
// a table with nothing to identify it still needs one -- but the name is a
// guess from the table's own name, and a guess is something the user may take
// back. Empty hands the name to the rule again, as an empty table name does.
EditResult Editor::rename_schema_key(ElementRef table, std::string chosen) {
    return impl_->edit("Rename the key", [&](Delta& delta) {
        if (!exists(project(), table)) return failure("The element no longer exists.");
        auto value = schema_with(project().schema, [&](SchemaOverrides& s) {
                const auto relation = relation_from(table);
            if (chosen.empty()) s.key_names.erase(relation); else s.key_names[relation] = chosen;
        });
        if (value == project().schema) return EditResult{};
        delta.schema = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::rename_schema_column(SchemaColumnId id, std::string name) {
    return impl_->edit("Rename the column", [&](Delta& delta) {
        if (!find_schema_column(project().schema, id)) return failure("The column no longer exists.");
        if (name.empty()) return failure("A column needs a name.");
        auto value = schema_with(project().schema, [&](SchemaOverrides& s) {
            for (auto& [table, columns] : s.added) {
                (void)table;
                for (auto& column : columns)
                    if (column.id == id) column.name = name;
            }
        });
        if (value == project().schema) return EditResult{};
        delta.schema = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_schema_column_type(SchemaColumnId id, LogicalType type) {
    return impl_->edit("Set logical type", [&](Delta& delta) {
        if (!find_schema_column(project().schema, id)) return failure("The column no longer exists.");
        if (!known_type(type)) return failure("That is not a logical type.");
        auto value = schema_with(project().schema, [&](SchemaOverrides& s) {
            for (auto& [table, columns] : s.added) {
                (void)table;
                for (auto& column : columns) {
                    if (column.id != id) continue;
                    column.logical_type = type;
                    if (size_of(type) == TypeSize::None) { column.length = 0; column.scale = 0; }
                    if (size_of(type) != TypeSize::Precision) column.scale = 0;
                }
            }
        });
        if (value == project().schema) return EditResult{};
        delta.schema = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::erase_schema_column(SchemaColumnId id) {
    return impl_->edit("Remove the column", [&](Delta& delta) {
        if (!find_schema_column(project().schema, id)) return failure("The column no longer exists.");
        auto value = schema_with(project().schema, [&](SchemaOverrides& s) {
            for (auto& [table, columns] : s.added) {
                (void)table;
                std::erase_if(columns, [&](const SchemaColumn& column) { return column.id == id; });
            }
            // A table that has none of its own left holds no entry at all, so
            // an empty list never counts as a difference from the diagram.
            std::erase_if(s.added, [](const auto& entry) { return entry.second.empty(); });
        });
        delta.schema = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::hide_in_schema(AttributeId id, bool hidden) {
    return impl_->edit(hidden ? "Remove the column" : "Show the column", [&](Delta& delta) {
        if (!project().attributes.contains(id)) return failure("The attribute no longer exists.");
        auto value = schema_with(project().schema, [&](SchemaOverrides& s) {
            if (hidden) s.hidden.insert(id); else s.hidden.erase(id);
        });
        if (value == project().schema) return EditResult{};
        delta.schema = std::move(value);
        return EditResult{};
    });
}

namespace {
template<class Change>
SchemaLayout arranged_with(const SchemaLayout& from, Change&& change) {
    auto value = from;
    change(value);
    return value;
}
} // namespace

EditResult Editor::move_schema_tables(const std::map<ElementRef, Point>& places,
                                      const std::vector<LinkSource>& give_way) {
    return impl_->edit("Move on the schema", [&](Delta& delta) {
        auto value = arranged_with(project().schema_layout, [&](SchemaLayout& layout) {
            for (const auto& [table, at] : places) layout.tables[relation_from(table)] = at;
            for (const auto& link : give_way) layout.lines.erase(foreign_key_from(link));
        });
        if (value == project().schema_layout) return EditResult{};
        delta.schema_layout = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::resize_schema_tables(const std::map<ElementRef, SchemaTableBox>& boxes) {
    return impl_->edit("Resize on the schema", [&](Delta& delta) {
        auto value = arranged_with(project().schema_layout, [&](SchemaLayout& layout) {
            for (const auto& [table, box] : boxes) {
                // Back at the size it would have had anyway is the same as
                // never having been pulled, so it holds no entry rather than
                // one that matches.
                const auto relation = relation_from(table);
                if (box.width <= 0) layout.widths.erase(relation);
                else layout.widths[relation] = std::clamp(box.width, min_table_width, max_table_width);
                if (box.height <= 0) layout.heights.erase(relation);
                else layout.heights[relation] = std::clamp(box.height, min_table_height, max_table_height);
                // A left or top edge takes the table with it, so where it now
                // stands is written in this same edit. A right or bottom edge
                // carries no place and leaves the table's own entry alone,
                // which is what keeps a table that has only been pulled wider
                // following the automatic arrangement.
                if (box.at) layout.tables[relation] = *box.at;
            }
        });
        if (value == project().schema_layout) return EditResult{};
        delta.schema_layout = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::shape_schema_line(LinkSource link, SchemaLine shape) {
    return impl_->edit("Shape a line", [&](Delta& delta) {
        auto value = arranged_with(project().schema_layout, [&](SchemaLayout& layout) {
            // A line with nothing said about it holds no entry at all, so
            // giving one back to the router is the same as never shaping it.
            const auto key = foreign_key_from(link);
            if (shape.empty()) layout.lines.erase(key);
            else layout.lines[key] = std::move(shape);
        });
        if (value == project().schema_layout) return EditResult{};
        delta.schema_layout = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::release_schema_lines() {
    return impl_->edit("Release the lines", [&](Delta& delta) {
        if (project().schema_layout.lines.empty()) return EditResult{};
        auto value = project().schema_layout;
        value.lines.clear();
        delta.schema_layout = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::tidy_schema() {
    return impl_->edit("Tidy the schema", [&](Delta& delta) {
        if (project().schema_layout.empty()) return EditResult{};
        delta.schema_layout = SchemaLayout{};
        return EditResult{};
    });
}

EditResult Editor::set_logical_types(const std::vector<AttributeId>& ids, LogicalType type) {
    return impl_->edit("Set logical type", [&](Delta& delta) {
        if (!known_type(type)) return failure("That is not a logical type.");
        for (const auto& id : ids) {
            const auto found = project().attributes.find(id);
            if (found == project().attributes.end()) continue;   // gone since it was listed
            auto value = found->second;
            value.logical_type = type;
            // A length belongs to the type that takes one, and the old type's
            // length means nothing to the new one.
            if (size_of(type) == TypeSize::None) { value.length = 0; value.scale = 0; }
            if (size_of(type) != TypeSize::Precision) value.scale = 0;
            if (value != found->second) delta.attributes.put(id, std::move(value));
        }
        return EditResult{};
    });
}

EditResult Editor::set_logical_type(AttributeId id, LogicalType type, std::uint32_t length) {
    return impl_->edit("Set logical type", [&](Delta& delta) {
        const auto found = project().attributes.find(id);
        if (found == project().attributes.end()) return failure("The attribute no longer exists.");
        auto value = found->second;
        value.logical_type = type;
        // Only a sized type is measured, so anything else is given no number
        // rather than keeping one it cannot use. A scale belongs to a
        // precision alone, so it goes the same way.
        const auto takes = size_of(type);
        value.length = takes == TypeSize::None ? 0 : length;
        if (takes != TypeSize::Precision) value.scale = 0;
        if (value == found->second) return EditResult{};
        delta.attributes.put(id, std::move(value));
        return EditResult{};
    });
}

EditResult Editor::set_type_size(AttributeId id, std::uint32_t length, std::uint32_t scale) {
    return impl_->edit("Set the size", [&](Delta& delta) {
        const auto found = project().attributes.find(id);
        if (found == project().attributes.end()) return failure("The attribute no longer exists.");
        const auto takes = size_of(found->second.logical_type);
        if (takes == TypeSize::None) return failure("That type is not measured.");
        if (length > max_logical_length) return failure("That is longer than a column can be.");
        if (takes == TypeSize::Precision && scale > length)
            return failure("A scale cannot be longer than the precision it sits in.");
        auto value = found->second;
        value.length = length;
        value.scale = takes == TypeSize::Precision ? scale : 0;
        if (value == found->second) return EditResult{};
        delta.attributes.put(id, std::move(value));
        return EditResult{};
    });
}

EditResult Editor::set_schema_column_size(SchemaColumnId id, std::uint32_t length, std::uint32_t scale) {
    return impl_->edit("Set the size", [&](Delta& delta) {
        const auto* column = find_schema_column(project().schema, id);
        if (!column) return failure("The column no longer exists.");
        const auto takes = size_of(column->logical_type);
        if (takes == TypeSize::None) return failure("That type is not measured.");
        if (length > max_logical_length) return failure("That is longer than a column can be.");
        if (takes == TypeSize::Precision && scale > length)
            return failure("A scale cannot be longer than the precision it sits in.");
        auto value = schema_with(project().schema, [&](SchemaOverrides& s) {
            for (auto& [table, columns] : s.added) {
                (void)table;
                for (auto& one : columns) {
                    if (one.id != id) continue;
                    one.length = length;
                    one.scale = takes == TypeSize::Precision ? scale : 0;
                }
            }
        });
        if (value == project().schema) return EditResult{};
        delta.schema = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_attribute_rules(AttributeId id, bool identifier, bool required, bool unique) {
    return impl_->edit("Change attribute rules", [&](Delta& delta) {
        const auto found = project().attributes.find(id);
        if (found == project().attributes.end()) return failure("The attribute no longer exists.");
        auto value = found->second;
        // Being the identifier and being drawn as a key are one fact (Zain,
        // 2026-09-24), so ticking one ticks the other, as the schema's own
        // primary-key toggle already did. A composite keeps its shape: it can
        // identify a row without being drawn as a single key oval, and its
        // parts are what carry the key into the table.
        if (identifier != found->second.identifier) {
            if (identifier && found->second.kind == AttributeKind::Normal) value.kind = AttributeKind::Key;
            else if (!identifier && found->second.kind == AttributeKind::Key) value.kind = AttributeKind::Normal;
        }
        value.identifier = identifier;
        // A key is what identifies a row, so it always has to be there.
        value.required = required || identifier;
        value.unique = unique;
        if (value == found->second) return EditResult{};
        delta.attributes.put(id, std::move(value));
        return EditResult{};
    });
}

EditResult Editor::set_primary_key(AttributeId id, bool key) {
    return impl_->edit(key ? "Make it the primary key" : "Take off the primary key", [&](Delta& delta) {
        const auto found = project().attributes.find(id);
        if (found == project().attributes.end()) return failure("The attribute no longer exists.");
        if (key && found->second.kind != AttributeKind::Normal && found->second.kind != AttributeKind::Key)
            return failure("A composite, multivalued or derived attribute cannot be the primary key.");
        auto value = found->second;
        value.identifier = key;
        // A key is what identifies a row, so it always has to be there. Taking
        // the key off says nothing about whether the column may now be empty,
        // so what it required is left as it was for somebody to decide.
        if (key) value.required = true;
        value.kind = key ? AttributeKind::Key
                         : (found->second.kind == AttributeKind::Key ? AttributeKind::Normal
                                                                     : found->second.kind);
        if (value == found->second) return EditResult{};
        delta.attributes.put(id, std::move(value));
        return EditResult{};
    });
}

EditResult Editor::set_participation(ParticipantId participant, Participation participation) {
    return impl_->edit(participation == Participation::Total ? "Make the side total"
                                                             : "Make the side partial",
                       [&](Delta& delta) {
        for (const auto& [id, relationship] : project().relationships) {
            const auto item = std::find_if(relationship.participants.begin(),
                                           relationship.participants.end(),
                                           [&](const auto& entry) { return entry.id == participant; });
            if (item == relationship.participants.end()) continue;
            auto value = relationship;
            auto& side = value.participants[static_cast<std::size_t>(
                std::distance(relationship.participants.begin(), item))];
            side.participation = participation;
            // Answered now. Somebody said which way it goes, and a side that
            // was told is a different fact from one that was never asked --
            // which is what the conversion reads. The cardinality was not
            // part of what was said and keeps whatever it had.
            side.participation_confirmed = true;
            if (value == relationship) return EditResult{};
            delta.relationships.put(id, std::move(value));
            return EditResult{};
        }
        return failure("That side of the relationship no longer exists.");
    });
}

EditResult Editor::set_schema_column_rules(SchemaColumnId id, bool identifier, bool required, bool unique) {
    return impl_->edit("Change column rules", [&](Delta& delta) {
        if (!find_schema_column(project().schema, id)) return failure("The column no longer exists.");
        auto value = schema_with(project().schema, [&](SchemaOverrides& s) {
            for (auto& [table, columns] : s.added) {
                (void)table;
                for (auto& column : columns) {
                    if (column.id != id) continue;
                    column.identifier = identifier;
                    column.required = required || identifier;
                    column.unique = unique;
                }
            }
        });
        if (value == project().schema) return EditResult{};
        delta.schema = std::move(value);
        return EditResult{};
    });
}

namespace {
// Why this column cannot count itself up, or nothing where it can. The same
// question for an attribute and for a schema-only column, so it is asked once.
std::string not_countable(LogicalType type, std::uint32_t scale) {
    if (type == LogicalType::Unset) return "Give the column a type before saying it counts itself up.";
    if (!countable(type)) return "Only a whole-number column can count itself up.";
    if ((type == LogicalType::Decimal || type == LogicalType::Numeric) && scale != 0)
        return "A column that counts itself up can keep no digits after the point.";
    return {};
}
} // namespace

EditResult Editor::set_auto_increment(AttributeId id, bool counting) {
    return impl_->edit(counting ? "Count the column up" : "Stop counting the column up",
                       [&](Delta& delta) {
        const auto found = project().attributes.find(id);
        if (found == project().attributes.end()) return failure("The attribute no longer exists.");
        if (counting) {
            const auto why = not_countable(found->second.logical_type, found->second.scale);
            if (!why.empty()) return failure(why);
        }
        auto value = found->second;
        value.auto_increment = counting;
        // A column the database fills in is never empty, so it is required in
        // the same edit. Stopping says nothing about whether it may be empty
        // now, so what it required is left for somebody to decide.
        if (counting) value.required = true;
        if (value == found->second) return EditResult{};
        delta.attributes.put(id, std::move(value));
        return EditResult{};
    });
}

EditResult Editor::set_schema_column_auto_increment(SchemaColumnId id, bool counting) {
    return impl_->edit(counting ? "Count the column up" : "Stop counting the column up",
                       [&](Delta& delta) {
        const auto* current = find_schema_column(project().schema, id);
        if (!current) return failure("The column no longer exists.");
        if (counting) {
            const auto why = not_countable(current->logical_type, current->scale);
            if (!why.empty()) return failure(why);
        }
        auto value = schema_with(project().schema, [&](SchemaOverrides& s) {
            for (auto& [table, columns] : s.added) {
                (void)table;
                for (auto& column : columns) {
                    if (column.id != id) continue;
                    column.auto_increment = counting;
                    if (counting) column.required = true;
                }
            }
        });
        if (value == project().schema) return EditResult{};
        delta.schema = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_key_auto_increment(ElementRef table, bool counting) {
    return impl_->edit(counting ? "Count the key up" : "Stop counting the key up", [&](Delta& delta) {
        if (!exists(project(), table)) return failure("The element no longer exists.");
        auto value = schema_with(project().schema, [&](SchemaOverrides& s) {
                const auto relation = relation_from(table);
            if (counting) s.counting_keys.insert(relation); else s.counting_keys.erase(relation);
        });
        if (value == project().schema) return EditResult{};
        delta.schema = std::move(value);
        return EditResult{};
    });
}

EditResult Editor::set_cardinality(ParticipantId participant, Cardinality maximum) {
    return impl_->edit(maximum == Cardinality::One ? "Make the side one" : "Make the side many",
                       [&](Delta& delta) {
        for (const auto& [id, relationship] : project().relationships) {
            const auto item = std::find_if(relationship.participants.begin(),
                                           relationship.participants.end(),
                                           [&](const auto& entry) { return entry.id == participant; });
            if (item == relationship.participants.end()) continue;
            auto value = relationship;
            auto& side = value.participants[static_cast<std::size_t>(
                std::distance(relationship.participants.begin(), item))];
            side.maximum = maximum;
            // Answered now. The participation was not part of what was said
            // and keeps whatever it had, answered or not.
            side.cardinality_confirmed = true;
            if (value == relationship) return EditResult{};
            delta.relationships.put(id, std::move(value));
            return EditResult{};
        }
        return failure("That side of the relationship no longer exists.");
    });
}

EditResult Editor::set_attribute_kind(AttributeId id, AttributeKind kind) {
    return impl_->edit("Change attribute kind", [&](Delta& delta) {
        const auto found = project().attributes.find(id);
        if (found == project().attributes.end()) return failure("The attribute no longer exists.");
        if (found->second.kind != kind) {
            auto value = found->second;
            value.kind = kind;
            // Drawn as a key is the primary key (Zain, 2026-09-24): one fact,
            // whichever side it is changed from. Taking the key off says
            // nothing about whether the column may now be empty, so what it
            // required is left for somebody to decide, as the schema's toggle
            // leaves it.
            if (kind == AttributeKind::Key) {
                value.identifier = true;
                value.required = true;
            } else if (found->second.kind == AttributeKind::Key) {
                value.identifier = false;
            }
            delta.attributes.put(id, std::move(value));
        }
        return EditResult{};
    });
}
EditResult Editor::set_attribute_owner(AttributeId id, std::optional<AttributeOwner> owner, Connector shape) {
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
        // A link made by clicking two points is pinned to them here, in the
        // same edit, so one undo takes back the link and its joins together.
        if (owner && !shape.automatic()) delta.connectors.put(ConnectorRef{id}, std::move(shape));
        return EditResult{};
    });
}
EditResult Editor::connect(RelationshipId relationship, ParticipantTarget target, Connector shape) {
    return impl_->edit("Connect participant", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end() || !exists(project(), target_ref(target)))
            return failure("The relationship or its participant no longer exists.");
        const ParticipantId id{impl_->next_id()};
        auto value = found->second;
        value.participants.push_back({id, target, Cardinality::Many, Participation::Partial, {}});
        delta.relationships.put(relationship, std::move(value));
        if (!shape.automatic()) delta.connectors.put(ConnectorRef{id}, std::move(shape));
        return EditResult{true, {}, {}, id};
    });
}
EditResult Editor::relate(EntityId first, EntityId second, Rect body, std::string name,
                          Connector first_shape, Connector second_shape) {
    return impl_->edit("Relate entities", [&](Delta& delta) {
        if (!project().entities.contains(first) || !project().entities.contains(second))
            return failure("One of those entities no longer exists.");
        const RelationshipId id{impl_->next_id()};
        const ParticipantId first_side{impl_->next_id()};
        const ParticipantId second_side{impl_->next_id()};
        Relationship relationship{.id = id, .name = std::move(name)};
        relationship.participants.push_back({first_side, first, Cardinality::Many, Participation::Partial, {}});
        relationship.participants.push_back({second_side, second, Cardinality::Many, Participation::Partial, {}});
        delta.relationships.put(id, std::move(relationship));
        delta.layout.put(ElementRef{id}, body);
        // Each side may be pinned where its entity was clicked, in this same edit.
        if (!first_shape.automatic()) delta.connectors.put(ConnectorRef{first_side}, std::move(first_shape));
        if (!second_shape.automatic()) delta.connectors.put(ConnectorRef{second_side}, std::move(second_shape));
        return EditResult{true, {}, ElementRef{id}, {}};
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
EditResult Editor::set_entity_weak(EntityId id, bool weak) {
    return impl_->edit("Change entity kind", [&](Delta& delta) {
        const auto found = project().entities.find(id);
        if (found == project().entities.end()) return failure("The entity no longer exists.");
        if (found->second.weak == weak) return EditResult{};
        auto value = found->second;
        value.weak = weak;
        delta.entities.put(id, std::move(value));
        return EditResult{};
    });
}
EditResult Editor::set_relationship_kind(RelationshipId relationship, RelationshipKind kind, std::optional<Rect> body) {
    return impl_->edit("Change relationship kind", [&](Delta& delta) {
        const auto found = project().relationships.find(relationship);
        if (found == project().relationships.end()) return failure("The relationship no longer exists.");
        auto value = found->second;
        value.associative = kind == RelationshipKind::Associative;
        value.identifying = kind == RelationshipKind::Identifying;
        if (value != found->second) delta.relationships.put(relationship, std::move(value));
        if (body) {
            const auto layout = project().layout.find(ElementRef{relationship});
            if (layout == project().layout.end() || layout->second != *body) delta.layout.put(ElementRef{relationship}, *body);
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
        // Answered now, whatever the values are. Choosing Many deliberately is
        // a different fact from a side that was never asked, and this is the
        // moment the difference is made: conversion reads these to tell a
        // decided M:M from two untouched defaults.
        item->cardinality_confirmed = true;
        item->participation_confirmed = true;
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
namespace {
// The first time an entity, relationship or attribute is made a different
// size by hand, the size it had is kept as the size its name is drawn for,
// so from then on the name grows and shrinks with the box (Zain,
// 2026-09-26). Kept in the same edit as the resize, so one undo takes both.
void keep_lettering_base(const Project& project, Delta& delta, const ElementRef& ref, const Rect& sized) {
    if (!std::holds_alternative<EntityId>(ref) && !std::holds_alternative<RelationshipId>(ref)
        && !std::holds_alternative<AttributeId>(ref))
        return;
    if (project.lettering.contains(ref)) return;
    const auto found = project.layout.find(ref);
    if (found == project.layout.end()) return;
    if (found->second.width == sized.width && found->second.height == sized.height) return;
    delta.lettering.put(ref, LetteringBase{found->second.width, found->second.height});
}
} // namespace

EditResult Editor::move(const std::map<ElementRef, Rect>& positions) {
    return impl_->edit("Move elements", [&](Delta& delta) {
        for (const auto& [ref, rect] : positions) {
            if (!exists(project(), ref)) return failure("An element no longer exists.");
            keep_lettering_base(project(), delta, ref, rect);
            const auto found = project().layout.find(ref);
            if (found == project().layout.end() || found->second != rect) delta.layout.put(ref, rect);
        }
        return EditResult{};
    });
}
EditResult Editor::resize_symbols(const std::map<ElementRef, Rect>& boxes) {
    return impl_->edit("Resize symbol", [&](Delta& delta) {
        for (const auto& [ref, box] : boxes) {
            const auto* note_id = std::get_if<NoteId>(&ref);
            if (!note_id) return failure("Only a symbol can be resized.");
            const auto note = project().notes.find(*note_id);
            if (note == project().notes.end()) return failure("The symbol no longer exists.");
            if (!note->second.plain) return failure("Only a symbol can be resized.");
            // Clamped rather than refused: a size comes from a grip being
            // dragged, and a drag that runs past the end is asking for the
            // end, not for nothing to happen.
            auto sized = box;
            sized.width = std::clamp(sized.width, min_symbol_size, max_symbol_size);
            sized.height = std::clamp(sized.height, min_symbol_size, max_symbol_size);
            sized.x = std::clamp(sized.x, -max_coordinate, max_coordinate - sized.width);
            sized.y = std::clamp(sized.y, -max_coordinate, max_coordinate - sized.height);
            const auto found = project().layout.find(ref);
            if (found == project().layout.end() || found->second != sized) delta.layout.put(ref, sized);
        }
        return EditResult{};
    });
}
EditResult Editor::resize_entities(const std::map<ElementRef, Rect>& boxes) {
    return impl_->edit("Resize entity", [&](Delta& delta) {
        for (const auto& [ref, box] : boxes) {
            if (!std::holds_alternative<EntityId>(ref)) return failure("Only an entity can be resized here.");
            if (!exists(project(), ref)) return failure("The entity no longer exists.");
            // Clamped rather than refused: a size comes from an edge being
            // dragged, and a drag that runs past the end is asking for the
            // end, not for nothing to happen.
            auto sized = box;
            sized.width = std::clamp(sized.width, min_entity_width, max_entity_width);
            sized.height = std::clamp(sized.height, min_entity_height, max_entity_height);
            sized.x = std::clamp(sized.x, -max_coordinate, max_coordinate - sized.width);
            sized.y = std::clamp(sized.y, -max_coordinate, max_coordinate - sized.height);
            keep_lettering_base(project(), delta, ref, sized);
            const auto found = project().layout.find(ref);
            if (found == project().layout.end() || found->second != sized) delta.layout.put(ref, sized);
        }
        return EditResult{};
    });
}
EditResult Editor::resize_relationships(const std::map<ElementRef, Rect>& boxes) {
    return impl_->edit("Resize relationship", [&](Delta& delta) {
        for (const auto& [ref, box] : boxes) {
            if (!std::holds_alternative<RelationshipId>(ref)) return failure("Only a relationship can be resized here.");
            if (!exists(project(), ref)) return failure("The relationship no longer exists.");
            // Clamped rather than refused: a size comes from an edge being
            // dragged, and a drag that runs past the end is asking for the
            // end, not for nothing to happen.
            auto sized = box;
            sized.width = std::clamp(sized.width, min_entity_width, max_entity_width);
            sized.height = std::clamp(sized.height, min_entity_height, max_entity_height);
            sized.x = std::clamp(sized.x, -max_coordinate, max_coordinate - sized.width);
            sized.y = std::clamp(sized.y, -max_coordinate, max_coordinate - sized.height);
            keep_lettering_base(project(), delta, ref, sized);
            const auto found = project().layout.find(ref);
            if (found == project().layout.end() || found->second != sized) delta.layout.put(ref, sized);
        }
        return EditResult{};
    });
}
EditResult Editor::resize_attributes(const std::map<ElementRef, Rect>& boxes) {
    return impl_->edit("Resize attribute", [&](Delta& delta) {
        for (const auto& [ref, box] : boxes) {
            if (!std::holds_alternative<AttributeId>(ref)) return failure("Only an attribute can be resized here.");
            if (!exists(project(), ref)) return failure("The attribute no longer exists.");
            // Clamped rather than refused, as for an entity: a drag that runs
            // past the end is asking for the end, not for nothing to happen.
            auto sized = box;
            sized.width = std::clamp(sized.width, min_entity_width, max_entity_width);
            sized.height = std::clamp(sized.height, min_entity_height, max_entity_height);
            sized.x = std::clamp(sized.x, -max_coordinate, max_coordinate - sized.width);
            sized.y = std::clamp(sized.y, -max_coordinate, max_coordinate - sized.height);
            keep_lettering_base(project(), delta, ref, sized);
            const auto found = project().layout.find(ref);
            if (found == project().layout.end() || found->second != sized) delta.layout.put(ref, sized);
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

EditResult Editor::set_background(Background background) {
    return impl_->edit("Change background", [&](Delta& delta) {
        if (background.strength > max_strength)
            return failure("A background's strength is a percentage from 0 to 100.");
        // Only a picture background carries a picture, so switching away from
        // one lets its bytes go rather than keeping them out of sight. A
        // ruling is drawn as the ruling it is: fading it is what a picture
        // needs, so that it sits behind the diagram rather than in front.
        if (background.style != BackgroundStyle::Image) {
            background.image.clear();
            background.strength = max_strength;
        }
        if (background != project().background) delta.background = std::move(background);
        return EditResult{};
    });
}

EditResult Editor::set_transparency(const std::vector<ElementRef>& elements, std::uint8_t percent) {
    return impl_->edit("Set transparency", [&](Delta& delta) {
        if (percent > max_transparency) return failure("Transparency is a percentage from 0 to 100.");
        for (const auto& ref : elements)
            if (!exists(project(), ref)) return failure("One of those elements no longer exists.");
        for (const auto& ref : elements) {
            const auto found = project().transparency.find(ref);
            const std::uint8_t current = found == project().transparency.end() ? 0 : found->second;
            if (current == percent) continue;
            if (percent == 0) delta.transparency.remove(ref);
            else delta.transparency.put(ref, percent);
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

EditResult Editor::shape_connector(ConnectorRef ref, Connector shape) {
    return impl_->edit("Shape connector", [&](Delta& delta) {
        if (!connector_exists(project(), ref)) return failure("That connector no longer exists.");
        return store_connector(project(), delta, ref, shape);
    });
}

EditResult Editor::shape_connectors(const std::map<ConnectorRef, Connector>& shapes, std::string label) {
    return impl_->edit(std::move(label), [&](Delta& delta) {
        for (const auto& [ref, shape] : shapes) {
            (void)shape;
            if (!connector_exists(project(), ref)) return failure("One of those connectors no longer exists.");
        }
        for (const auto& [ref, shape] : shapes) {
            const auto stored = store_connector(project(), delta, ref, shape);
            if (!stored) return stored;
        }
        return EditResult{};
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
        // The links being cut are remembered as well as unshaped, because a
        // comment may be pinned to a line that was never given a shape of its
        // own, and that comment has to go with it just the same.
        std::set<ConnectorRef> cut_connectors;
        auto drop_connector = [&](const ConnectorRef& ref) {
            cut_connectors.insert(ref);
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
                else if constexpr (std::is_same_v<T, PictureId>) delta.pictures.remove(id);
                else if constexpr (std::is_same_v<T, NoteId>) delta.notes.remove(id);
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
            if (project().transparency.contains(ref)) delta.transparency.remove(ref);
            if (project().lettering.contains(ref)) delta.lettering.remove(ref);
        }
        // A triangle without its supertype means nothing, so it goes with it;
        // a deleted subtype is simply detached from the ones that survive.
        // What goes this way is gathered as well, because a remark may be
        // pinned to a triangle nobody asked to delete, and it has to go with it.
        std::set<ElementRef> gone = removed;
        for (const auto& [id, specialization] : project().specializations) {
            if (removed.contains(ElementRef{id})) continue;
            if (specialization.supertype && removed.contains(ElementRef{*specialization.supertype})) {
                gone.insert(ElementRef{id});
                delta.specializations.remove(id);
                if (project().layout.contains(ElementRef{id})) delta.layout.remove(ElementRef{id});
                // Review 2026-09-15, finding 4: the cascade dropped the triangle
                // and its layout but left its colour, and validation then
                // refused the whole deletion for a colour with no element.
                if (project().colours.contains(ElementRef{id})) delta.colours.remove(ElementRef{id});
                if (project().transparency.contains(ElementRef{id})) delta.transparency.remove(ElementRef{id});
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
        // A comment outlives nothing it is pinned to. Deleting an element or
        // cutting a line unpins every comment pinned to it, and a comment left
        // pinned to nothing goes with them, in this same edit -- so one undo
        // brings back the element, the line and the remark about them together.
        auto still_there = [&](const CommentTarget& target) {
            if (const auto* element = std::get_if<ElementRef>(&target)) return !gone.contains(*element);
            if (const auto* anchor = std::get_if<TextAnchor>(&target)) return !gone.contains(anchor->owner);
            return !cut_connectors.contains(std::get<ConnectorRef>(target));
        };
        for (const auto& [id, comment] : project().comments) {
            if (std::all_of(comment.targets.begin(), comment.targets.end(), still_there)) continue;
            auto value = comment;
            std::erase_if(value.targets, [&](const CommentTarget& target) { return !still_there(target); });
            if (value.targets.empty()) delta.comments.remove(id);
            else delta.comments.put(id, std::move(value));
        }
        // An answer about an element outlives it no more than a remark does.
        // A decision left pointing at something deleted is not merely stale:
        // validate() refuses it, and refusing it here refuses the whole
        // deletion, so the element could not be deleted at all while an answer
        // about it survived. The schema is the same story told the other way --
        // nothing validates it yet, so a column added to a table whose element
        // is gone simply stayed, and was written to the file. Both go in this
        // same edit, so one undo brings the element, its answers and its
        // columns back together.
        const auto cut = [&](const ConnectorRef& ref) { return cut_connectors.contains(ref); };
        // What the schema loses when these elements go.
        //
        // A relation and a foreign key have identities of their own now, and a
        // derived identity cannot be turned back into the element it came
        // from, so what is going is worked out forwards: every element being
        // removed is asked what relational identity it would have produced,
        // and those are what the schema's own state is cleared of.
        std::set<domain::RelationId> gone_relations;
        for (const auto& ref : gone) gone_relations.insert(domain::relation_from(ref));
        std::set<domain::ForeignKeyId> gone_keys;
        for (const auto& ref : gone)
            if (const auto* entity = std::get_if<EntityId>(&ref))
                gone_keys.insert(domain::foreign_key_from(domain::LinkSource{*entity}));
        for (const auto& ref : cut_connectors)
            std::visit([&](const auto& id) {
                gone_keys.insert(domain::foreign_key_from(domain::LinkSource{id}));
            }, ref);
        auto decided = project().decisions;
        std::erase_if(decided.isa, [&](const auto& entry) { return gone.contains(ElementRef{entry.first}); });
        std::erase_if(decided.composite, [&](const auto& entry) { return gone.contains(ElementRef{entry.first}); });
        // A key side can be lost without its relationship: the participant it
        // names may be the one that was disconnected.
        std::erase_if(decided.one_to_one_key, [&](const auto& entry) {
            return gone.contains(ElementRef{entry.first}) || cut(ConnectorRef{entry.second});
        });
        std::erase_if(decided.junction_name, [&](const auto& entry) { return gone.contains(ElementRef{entry.first}); });
        std::erase_if(decided.bridge_key, [&](const auto& entry) { return gone.contains(ElementRef{entry.first}); });
        // An identifier names an attribute of an entity, so it falls with
        // either of them, and also when the attribute is merely detached --
        // an attribute that no longer belongs to the entity cannot identify it.
        std::erase_if(decided.identifier, [&](const auto& entry) {
            return gone.contains(ElementRef{entry.first}) || gone.contains(ElementRef{entry.second})
                || cut(ConnectorRef{entry.second});
        });
        std::erase_if(decided.table_name, [&](const auto& entry) { return gone_relations.contains(entry.first); });
        if (decided != project().decisions) delta.decisions = std::move(decided);

        auto overrides = project().schema;
        std::erase_if(overrides.added, [&](const auto& entry) { return gone_relations.contains(entry.first); });
        std::erase_if(overrides.hidden, [&](const AttributeId& id) { return gone.contains(ElementRef{id}); });
        std::erase_if(overrides.key_names, [&](const auto& entry) { return gone_relations.contains(entry.first); });
        std::erase_if(overrides.counting_keys, [&](const auto& id) { return gone_relations.contains(id); });
        if (overrides != project().schema) delta.schema = std::move(overrides);

        // The schema's own arrangement is presentation, exactly as the diagram's
        // layout is, and it is dropped here for the same reason the diagram's is.
        auto arranged = project().schema_layout;
        std::erase_if(arranged.tables, [&](const auto& entry) { return gone_relations.contains(entry.first); });
        std::erase_if(arranged.widths, [&](const auto& entry) { return gone_relations.contains(entry.first); });
        std::erase_if(arranged.heights, [&](const auto& entry) { return gone_relations.contains(entry.first); });
        std::erase_if(arranged.lines, [&](const auto& entry) { return gone_keys.contains(entry.first); });
        if (arranged != project().schema_layout) delta.schema_layout = std::move(arranged);
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
                } else if constexpr (std::is_same_v<T, PictureId>) {
                    auto value = project().pictures.at(id);
                    value.id = new_id;
                    delta.pictures.put(new_id, std::move(value));
                } else if constexpr (std::is_same_v<T, NoteId>) {
                    auto value = project().notes.at(id);
                    value.id = new_id;
                    delta.notes.put(new_id, std::move(value));
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
            const auto faded = project().transparency.find(original);
            if (faded != project().transparency.end()) delta.transparency.put(replacement, faded->second);
            // And its name is drawn the size the original's is.
            const auto lettering = project().lettering.find(original);
            if (lettering != project().lettering.end()) delta.lettering.put(replacement, lettering->second);
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
