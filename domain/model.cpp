#include "domain/model.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <set>
#include <type_traits>

namespace erdflow::domain {

bool Uuid::valid() const {
    return (bytes[6] & 0xf0U) == 0x70U && (bytes[8] & 0xc0U) == 0x80U;
}

Uuid uuid(const ElementRef& ref) {
    return std::visit([](const auto& id) { return id.value; }, ref);
}

namespace {
template<class Visitor> auto visit_element(const Project& project, const ElementRef& ref,
                                           Visitor visitor) {
    return std::visit([&](const auto& id) {
        using T = std::decay_t<decltype(id)>;
        if constexpr (std::is_same_v<T, EntityId>) {
            const auto found = project.entities.find(id);
            return visitor(found == project.entities.end() ? nullptr : &found->second);
        } else if constexpr (std::is_same_v<T, AttributeId>) {
            const auto found = project.attributes.find(id);
            return visitor(found == project.attributes.end() ? nullptr : &found->second);
        } else if constexpr (std::is_same_v<T, RelationshipId>) {
            const auto found = project.relationships.find(id);
            return visitor(found == project.relationships.end() ? nullptr : &found->second);
        } else if constexpr (std::is_same_v<T, PictureId>) {
            const auto found = project.pictures.find(id);
            return visitor(found == project.pictures.end() ? nullptr : &found->second);
        } else if constexpr (std::is_same_v<T, NoteId>) {
            const auto found = project.notes.find(id);
            return visitor(found == project.notes.end() ? nullptr : &found->second);
        } else {
            const auto found = project.specializations.find(id);
            return visitor(found == project.specializations.end() ? nullptr : &found->second);
        }
    }, ref);
}

// The first bytes of a PNG or of a JPEG file. This is all the domain asks of
// a picture: that its bytes could be an image at all.
bool looks_like_image(const std::vector<std::uint8_t>& bytes) {
    static constexpr std::array<std::uint8_t, 8> png{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    static constexpr std::array<std::uint8_t, 3> jpeg{0xff, 0xd8, 0xff};
    return (bytes.size() >= png.size() && std::equal(png.begin(), png.end(), bytes.begin()))
        || (bytes.size() >= jpeg.size() && std::equal(jpeg.begin(), jpeg.end(), bytes.begin()));
}

// Reject malformed UTF-8, NUL, and control characters that cannot safely be
// displayed or round-tripped. Descriptions may contain tabs and line breaks.
bool valid_text(const std::string& text, bool multiline) {
    for (std::size_t i = 0; i < text.size();) {
        const auto first = static_cast<unsigned char>(text[i++]);
        if (first < 0x80U) {
            if (first < 0x20U && !(multiline && (first == '\n' || first == '\r' || first == '\t')))
                return false;
            if (first == 0x7fU) return false;
            continue;
        }
        std::uint32_t code = 0;
        std::uint32_t minimum = 0;
        std::size_t count = 0;
        if (first >= 0xc2U && first <= 0xdfU) { code = first & 0x1fU; minimum = 0x80U; count = 1; }
        else if (first >= 0xe0U && first <= 0xefU) { code = first & 0x0fU; minimum = 0x800U; count = 2; }
        else if (first >= 0xf0U && first <= 0xf4U) { code = first & 0x07U; minimum = 0x10000U; count = 3; }
        else return false;
        if (count > text.size() - i) return false;
        while (count-- != 0) {
            const auto next = static_cast<unsigned char>(text[i++]);
            if ((next & 0xc0U) != 0x80U) return false;
            code = (code << 6U) | (next & 0x3fU);
        }
        if (code < minimum || code > 0x10ffffU || (code >= 0xd800U && code <= 0xdfffU)
            || (code >= 0x80U && code <= 0x9fU)) return false;
    }
    return true;
}

bool empty_name(const std::string& value) {
    return value.empty() || std::all_of(value.begin(), value.end(), [](char c) { return c == ' '; });
}
} // namespace

bool exists(const Project& project, const ElementRef& ref) {
    return visit_element(project, ref, [](const auto* element) { return element != nullptr; });
}
bool is_figure(const ElementRef& ref) {
    return std::holds_alternative<PictureId>(ref) || std::holds_alternative<NoteId>(ref);
}
RelationshipKind relationship_kind(const Relationship& relationship) {
    if (relationship.associative) return RelationshipKind::Associative;
    return relationship.identifying ? RelationshipKind::Identifying : RelationshipKind::Regular;
}
ElementRef target_ref(const ParticipantTarget& target) {
    if (const auto* entity = std::get_if<EntityId>(&target)) return *entity;
    return std::get<RelationshipId>(target);
}

bool connector_exists(const Project& project, const ConnectorRef& ref) {
    if (const auto* attribute = std::get_if<AttributeId>(&ref)) {
        const auto found = project.attributes.find(*attribute);
        return found != project.attributes.end() && found->second.owner.has_value();
    }
    const auto& participant = std::get<ParticipantId>(ref);
    return std::any_of(project.relationships.begin(), project.relationships.end(), [&](const auto& entry) {
        return std::any_of(entry.second.participants.begin(), entry.second.participants.end(),
                           [&](const auto& item) { return item.id == participant; });
    });
}

std::string name(const Project& project, const ElementRef& ref) {
    return visit_element(project, ref, [](const auto* element) { return element ? element->name : std::string{}; });
}
std::string description(const Project& project, const ElementRef& ref) {
    return visit_element(project, ref, [](const auto* element) { return element ? element->description : std::string{}; });
}

std::vector<Issue> validate(const Project& project) {
    std::vector<Issue> issues;
    auto error = [&](std::string code, std::string message, std::optional<ElementRef> ref = {}) {
        issues.push_back({Severity::Error, std::move(code), std::move(message), ref, true});
    };
    auto warning = [&](std::string code, std::string message, std::optional<ElementRef> ref = {}) {
        issues.push_back({Severity::Warning, std::move(code), std::move(message), ref, false});
    };
    const auto elements = project.entities.size() + project.attributes.size()
        + project.relationships.size() + project.specializations.size()
        + project.pictures.size() + project.notes.size();
    std::size_t participants = 0;
    for (const auto& [id, relationship] : project.relationships) {
        (void)id;
        participants += relationship.participants.size();
        if (participants > max_elements) break;
    }
    if (elements > max_elements || participants > max_elements) {
        error("project.limit", "The project exceeds the limit of 10,000 elements or 10,000 participants.");
        return issues;
    }
    std::set<Uuid> identifiers;
    auto identity = [&](Uuid id, std::optional<ElementRef> ref = {}) {
        if (!id.valid()) error("identity.invalid", "A persistent identifier is not a valid UUIDv7.", ref);
        if (!identifiers.insert(id).second) error("identity.duplicate", "Persistent identifiers must be globally unique within the project.", ref);
    };
    // A figure need not be named: a picture speaks for itself, and a note is
    // often only its text. Everything in the model is asked for a name.
    auto text_fields = [&](const std::string& title, const std::string& details, std::optional<ElementRef> ref = {},
                           bool named = true) {
        if (title.size() > max_name_bytes || !valid_text(title, false))
            error("text.name.invalid", "Names must be valid UTF-8, contain no control characters, and fit in 512 bytes.", ref);
        else if (named && empty_name(title)) warning("name.missing", "Give this element a name when you are ready.", ref);
        if (details.size() > max_description_bytes || !valid_text(details, true))
            error("text.description.invalid", "Descriptions must be valid UTF-8 and fit in 16,384 bytes.", ref);
    };
    identity(project.id.value);
    text_fields(project.name, {});
    for (const auto& [id, entity] : project.entities) {
        const ElementRef ref = id;
        if (!project.layout.contains(ref)) error("layout.element.missing", "The entity has no canvas layout.", ref);
        identity(id.value, ref);
        if (entity.id != id) error("identity.key_mismatch", "The entity key and identifier differ.", ref);
        text_fields(entity.name, entity.description, ref);
    }
    for (const auto& [id, attribute] : project.attributes) {
        const ElementRef ref = id;
        if (!project.layout.contains(ref)) error("layout.element.missing", "The attribute has no canvas layout.", ref);
        identity(id.value, ref);
        if (attribute.id != id) error("identity.key_mismatch", "The attribute key and identifier differ.", ref);
        text_fields(attribute.name, attribute.description, ref);
        switch (attribute.kind) {
        case AttributeKind::Normal: case AttributeKind::Key: case AttributeKind::Composite:
        case AttributeKind::Multivalued: case AttributeKind::Derived: break;
        default: error("attribute.kind.invalid", "The attribute kind is invalid.", ref);
        }
        if (!attribute.owner) warning("attribute.owner.missing", "Attach this attribute to an entity, relationship, or composite attribute.", ref);
        else if (!exists(project, *attribute.owner)) error("attribute.owner.missing_reference", "The attribute owner does not exist.", ref);
        else if (const auto* parent = std::get_if<AttributeId>(&*attribute.owner)) {
            if (project.attributes.at(*parent).kind != AttributeKind::Composite)
                error("attribute.owner.not_composite", "Only a composite attribute can own other attributes.", ref);
        }
        if (attribute.owner && std::holds_alternative<SpecializationId>(*attribute.owner))
            error("attribute.owner.specialization", "A specialization holds no attributes of its own.", ref);
        if (attribute.owner && is_figure(*attribute.owner))
            error("attribute.owner.figure", "A picture or a note holds no attributes.", ref);
        if (attribute.kind == AttributeKind::Key && attribute.owner && std::holds_alternative<RelationshipId>(*attribute.owner))
            error("attribute.key.relationship", "A relationship-owned attribute cannot be an entity identifier.", ref);
    }
    // Iterative traversal with coloring is linear and cannot overflow the stack
    // even for a hostile file containing thousands of nested attributes.
    std::map<AttributeId, unsigned char> color;
    for (const auto& [id, attribute] : project.attributes) {
        (void)attribute;
        if (color[id] == 2) continue;
        std::vector<AttributeId> path;
        auto current = id;
        while (true) {
            const auto found = project.attributes.find(current);
            if (found == project.attributes.end() || color[current] == 2) break;
            if (color[current] == 1) {
                error("attribute.owner.cycle", "Composite attribute ownership must not contain a cycle.", ElementRef{current});
                break;
            }
            color[current] = 1;
            path.push_back(current);
            const auto& owner = found->second.owner;
            if (!owner || !std::holds_alternative<AttributeId>(*owner)) break;
            current = std::get<AttributeId>(*owner);
        }
        for (const auto& visited : path) color[visited] = 2;
    }
    for (const auto& [id, relationship] : project.relationships) {
        const ElementRef ref = id;
        if (!project.layout.contains(ref)) error("layout.element.missing", "The relationship has no canvas layout.", ref);
        identity(id.value, ref);
        if (relationship.id != id) error("identity.key_mismatch", "The relationship key and identifier differ.", ref);
        text_fields(relationship.name, relationship.description, ref);
        if (relationship.participants.size() < 2)
            warning("relationship.participants.incomplete", "Connect at least two participant roles to complete this relationship.", ref);
        if (relationship.associative && relationship.identifying)
            error("relationship.kind.conflict", "A relationship is identifying or associative, not both.", ref);
        std::map<ElementRef, std::vector<std::string>> roles;
        for (const auto& participant : relationship.participants) {
            identity(participant.id.value, ref);
            if (const auto* entity = std::get_if<EntityId>(&participant.target)) {
                if (!project.entities.contains(*entity))
                    error("participant.entity.missing", "A relationship participant refers to a missing entity.", ref);
            } else {
                // Only an associative relationship carries the identity needed
                // to take part in another relationship.
                const auto target = std::get<RelationshipId>(participant.target);
                const auto found = project.relationships.find(target);
                if (found == project.relationships.end())
                    error("participant.relationship.missing", "A relationship participant refers to a missing relationship.", ref);
                else if (target == id)
                    error("participant.self", "A relationship cannot take part in itself.", ref);
                else if (!found->second.associative)
                    error("participant.not_associative", "Only an associative relationship can take part in another relationship.", ref);
            }
            if (participant.maximum != Cardinality::One && participant.maximum != Cardinality::Many)
                error("participant.cardinality.invalid", "Participant maximum cardinality must be one or many.", ref);
            if (participant.participation != Participation::Partial && participant.participation != Participation::Total)
                error("participant.participation.invalid", "Participant participation must be partial or total.", ref);
            if (participant.role.size() > max_name_bytes || !valid_text(participant.role, false))
                error("participant.role.invalid", "Participant roles must be valid UTF-8 and fit in 512 bytes.", ref);
            roles[target_ref(participant.target)].push_back(participant.role);
        }
        for (const auto& [entity, names] : roles) {
            (void)entity;
            if (names.size() < 2) continue;
            std::set<std::string> unique;
            if (std::any_of(names.begin(), names.end(), [&](const auto& role) { return empty_name(role) || !unique.insert(role).second; }))
                warning("relationship.recursive.roles", "Give repeated participants distinct role names to explain this recursive relationship.", ref);
        }
    }
    // A weak entity is identified through an identifying relationship, so each
    // side is asked about the other, as advice rather than as a fault: both
    // are work in progress until they are connected.
    std::set<EntityId> identified;
    for (const auto& [id, relationship] : project.relationships) {
        if (!relationship.identifying) continue;
        bool weak_side = false;
        for (const auto& participant : relationship.participants)
            if (const auto* entity = std::get_if<EntityId>(&participant.target))
                if (const auto found = project.entities.find(*entity); found != project.entities.end() && found->second.weak) {
                    identified.insert(*entity);
                    weak_side = true;
                }
        if (!weak_side)
            warning("relationship.identifying.no_weak", "Connect the weak entity this relationship identifies.", ElementRef{id});
    }
    for (const auto& [id, entity] : project.entities)
        if (entity.weak && !identified.contains(id))
            warning("entity.weak.unidentified", "Connect this weak entity to an identifying relationship.", ElementRef{id});
    // Associative relationships can take part in one another, so the same
    // iterative colouring used for composite attributes guards against a cycle
    // that no traversal could terminate on.
    // Review 2026-09-15, finding 3: this followed only the first relationship a
    // relationship took part in, so a loop through its second participant was
    // never seen. It is now a full depth-first walk over every participant,
    // with a node marked finished only once all of its onward edges are done:
    // 1 means on the current path, 2 means fully explored.
    std::map<RelationshipId, unsigned char> relationship_color;
    for (const auto& [start, relationship] : project.relationships) {
        (void)relationship;
        if (relationship_color[start] != 0) continue;
        std::vector<RelationshipId> stack{start};
        while (!stack.empty()) {
            const auto current = stack.back();
            if (relationship_color[current] == 0) {
                relationship_color[current] = 1;
                const auto found = project.relationships.find(current);
                if (found != project.relationships.end())
                    for (const auto& participant : found->second.participants) {
                        const auto* onward = std::get_if<RelationshipId>(&participant.target);
                        if (!onward || !project.relationships.contains(*onward)) continue;
                        if (relationship_color[*onward] == 1) {
                            error("participant.cycle", "Associative relationships must not take part in one another in a cycle.", ElementRef{*onward});
                            stack.clear();
                            break;
                        }
                        if (relationship_color[*onward] == 0) stack.push_back(*onward);
                    }
                continue;
            }
            if (relationship_color[current] == 1) relationship_color[current] = 2;
            stack.pop_back();
        }
    }
    for (const auto& [id, specialization] : project.specializations) {
        const ElementRef ref = id;
        if (!project.layout.contains(ref)) error("layout.element.missing", "The specialization has no canvas layout.", ref);
        identity(id.value, ref);
        if (specialization.id != id) error("identity.key_mismatch", "The specialization key and identifier differ.", ref);
        text_fields(specialization.name, specialization.description, ref);
        if (!specialization.supertype)
            warning("specialization.supertype.incomplete", "Connect the entity this triangle generalises.", ref);
        else if (!project.entities.contains(*specialization.supertype))
            error("specialization.supertype.missing", "The specialization refers to a missing supertype.", ref);
        if (specialization.subtypes.empty())
            warning("specialization.subtypes.incomplete", "Connect at least one subtype to complete this specialization.", ref);
        std::set<EntityId> seen;
        for (const auto& subtype : specialization.subtypes) {
            if (!project.entities.contains(subtype))
                error("specialization.subtype.missing", "The specialization refers to a missing subtype.", ref);
            if (specialization.supertype && subtype == *specialization.supertype)
                error("specialization.self", "An entity cannot be a subtype of itself.", ref);
            if (!seen.insert(subtype).second)
                error("specialization.subtype.duplicate", "An entity can appear only once among a specialization's subtypes.", ref);
        }
        if (specialization.direction != Inheritance::Generalization && specialization.direction != Inheritance::Specialization)
            error("specialization.direction.invalid", "An ISA direction must be generalization or specialization.", ref);
        if (specialization.constraint != Disjointness::Disjoint && specialization.constraint != Disjointness::Overlapping)
            error("specialization.constraint.invalid", "A specialization constraint must be disjoint or overlapping.", ref);
        if (specialization.completeness != Completeness::Partial && specialization.completeness != Completeness::Total)
            error("specialization.completeness.invalid", "Specialization completeness must be partial or total.", ref);
    }
    // An entity that is its own ancestor could not be converted to relations at
    // all, so inheritance is checked for cycles the same iterative way.
    std::map<EntityId, std::vector<EntityId>> supertypes_of;
    for (const auto& [id, specialization] : project.specializations) {
        (void)id;
        if (!specialization.supertype) continue;
        for (const auto& subtype : specialization.subtypes) supertypes_of[subtype].push_back(*specialization.supertype);
    }
    // Review 2026-09-15, finding 2: a finished branch stayed marked as active
    // until the whole walk ended, so a diamond — two parents sharing an
    // ancestor — looked like a cycle when the ancestor was met the second
    // time. An entity is now marked finished the moment it is popped, so only
    // an entity still on the current path can close a loop.
    std::map<EntityId, unsigned char> inheritance_color;
    for (const auto& [start, entity] : project.entities) {
        (void)entity;
        if (inheritance_color[start] != 0) continue;
        std::vector<EntityId> stack{start};
        while (!stack.empty()) {
            const auto current = stack.back();
            if (inheritance_color[current] == 0) {
                inheritance_color[current] = 1;
                const auto found = supertypes_of.find(current);
                if (found != supertypes_of.end())
                    for (const auto& parent : found->second) {
                        if (inheritance_color[parent] == 1) {
                            error("specialization.cycle", "Inheritance must not form a cycle.", ElementRef{parent});
                            stack.clear();
                            break;
                        }
                        if (inheritance_color[parent] == 0) stack.push_back(parent);
                    }
                continue;
            }
            if (inheritance_color[current] == 1) inheritance_color[current] = 2;
            stack.pop_back();
        }
    }
    for (const auto& [id, picture] : project.pictures) {
        const ElementRef ref = id;
        if (!project.layout.contains(ref)) error("layout.element.missing", "The picture has no canvas layout.", ref);
        identity(id.value, ref);
        if (picture.id != id) error("identity.key_mismatch", "The picture key and identifier differ.", ref);
        text_fields(picture.name, picture.description, ref, false);
        if (picture.image.empty() || !looks_like_image(picture.image))
            error("picture.image.invalid", "A picture must hold a PNG or JPEG image.", ref);
        else if (picture.image.size() > max_image_bytes)
            error("picture.image.limit", "A picture's image must fit in 2 MiB.", ref);
    }
    for (const auto& [id, note] : project.notes) {
        const ElementRef ref = id;
        if (!project.layout.contains(ref)) error("layout.element.missing", "The note has no canvas layout.", ref);
        identity(id.value, ref);
        if (note.id != id) error("identity.key_mismatch", "The note key and identifier differ.", ref);
        text_fields(note.name, note.description, ref, false);
    }
    if (project.connectors.size() > max_elements) error("connector.limit", "The connector shapes exceed the element limit.");
    for (const auto& [ref, connector] : project.connectors) {
        // Report against the owning element so the canvas can highlight it; a
        // connector has no element reference of its own.
        const std::optional<ElementRef> owner = std::holds_alternative<AttributeId>(ref)
            ? std::optional<ElementRef>{std::get<AttributeId>(ref)} : std::nullopt;
        if (!connector_exists(project, ref))
            error("connector.reference.missing", "A connector shape refers to a link that no longer exists.", owner);
        if (!std::isfinite(connector.offset) || std::abs(connector.offset) > max_coordinate)
            error("connector.bounds.invalid", "A connector bend must be finite and within the supported canvas.", owner);
        // A pinned join is a direction, so anything that is not a finite angle
        // would leave the line with nowhere to meet its shape.
        for (const auto& anchor : {connector.owner_anchor, connector.child_anchor})
            if (anchor && !std::isfinite(*anchor))
                error("connector.anchor.invalid", "A pinned connector join must be a finite direction.", owner);
        // A route is bounded the same way the layout is: each point has to be
        // somewhere on the canvas, and a line cannot carry more corners than the
        // document is allowed elements.
        if (connector.waypoints.size() > max_elements)
            error("connector.route.limit", "A connector route exceeds the element limit.", owner);
        for (const auto& point : connector.waypoints)
            if (!std::isfinite(point.x) || !std::isfinite(point.y)
                || std::abs(point.x) > max_coordinate || std::abs(point.y) > max_coordinate)
                error("connector.route.invalid", "A connector route point must be within the supported canvas.", owner);
    }
    if (project.colours.size() > max_elements) error("colour.limit", "The chosen colours exceed the element limit.");
    for (const auto& [ref, colour] : project.colours) {
        (void)colour; // Every channel is already a byte, so only the reference can be wrong.
        if (!exists(project, ref)) error("colour.reference.missing", "A colour refers to a missing element.", ref);
    }
    switch (project.background.style) {
    case BackgroundStyle::Theme: case BackgroundStyle::Squares: case BackgroundStyle::Lines:
    case BackgroundStyle::Dots: case BackgroundStyle::Image: break;
    default: error("background.style.invalid", "The background style is invalid.");
    }
    if (project.background.strength > max_strength)
        error("background.strength.invalid", "A background's strength is a percentage from 0 to 100.");
    if (project.background.style == BackgroundStyle::Image) {
        if (project.background.image.empty() || !looks_like_image(project.background.image))
            error("background.image.invalid", "A background picture must be a PNG or JPEG image.");
        else if (project.background.image.size() > max_image_bytes)
            error("background.image.limit", "A background picture must fit in 2 MiB.");
    } else if (!project.background.image.empty()) {
        error("background.image.unused", "Only a picture background carries a picture.");
    }
    if (project.transparency.size() > max_elements) error("transparency.limit", "The transparency entries exceed the element limit.");
    for (const auto& [ref, percent] : project.transparency) {
        if (!exists(project, ref)) error("transparency.reference.missing", "A transparency refers to a missing element.", ref);
        if (percent > max_transparency) error("transparency.invalid", "Transparency is a percentage from 0 to 100.", ref);
    }
    if (project.layout.size() > max_elements) error("layout.limit", "The layout exceeds the element limit.");
    for (const auto& [ref, rect] : project.layout) {
        if (!exists(project, ref)) error("layout.reference.missing", "A layout entry refers to a missing element.", ref);
        if (!std::isfinite(rect.x) || !std::isfinite(rect.y) || !std::isfinite(rect.width) || !std::isfinite(rect.height)
            || std::abs(rect.x) > max_coordinate || std::abs(rect.y) > max_coordinate
            || rect.width <= 0 || rect.height <= 0 || rect.width > max_coordinate || rect.height > max_coordinate
            || std::abs(rect.x + rect.width) > max_coordinate || std::abs(rect.y + rect.height) > max_coordinate)
            error("layout.bounds.invalid", "Element bounds must be finite, positive, and within the supported canvas.", ref);
    }
    return issues;
}

} // namespace erdflow::domain
