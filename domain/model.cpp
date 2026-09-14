#include "domain/model.hpp"

#include <algorithm>
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
        } else {
            const auto found = project.relationships.find(id);
            return visitor(found == project.relationships.end() ? nullptr : &found->second);
        }
    }, ref);
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
    const auto elements = project.entities.size() + project.attributes.size() + project.relationships.size();
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
    auto text_fields = [&](const std::string& title, const std::string& details, std::optional<ElementRef> ref = {}) {
        if (title.size() > max_name_bytes || !valid_text(title, false))
            error("text.name.invalid", "Names must be valid UTF-8, contain no control characters, and fit in 512 bytes.", ref);
        else if (empty_name(title)) warning("name.missing", "Give this element a name when you are ready.", ref);
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
        std::map<EntityId, std::vector<std::string>> roles;
        for (const auto& participant : relationship.participants) {
            identity(participant.id.value, ref);
            if (!project.entities.contains(participant.entity)) error("participant.entity.missing", "A relationship participant refers to a missing entity.", ref);
            if (participant.maximum != Cardinality::One && participant.maximum != Cardinality::Many)
                error("participant.cardinality.invalid", "Participant maximum cardinality must be one or many.", ref);
            if (participant.participation != Participation::Partial && participant.participation != Participation::Total)
                error("participant.participation.invalid", "Participant participation must be partial or total.", ref);
            if (participant.role.size() > max_name_bytes || !valid_text(participant.role, false))
                error("participant.role.invalid", "Participant roles must be valid UTF-8 and fit in 512 bytes.", ref);
            roles[participant.entity].push_back(participant.role);
        }
        for (const auto& [entity, names] : roles) {
            (void)entity;
            if (names.size() < 2) continue;
            std::set<std::string> unique;
            if (std::any_of(names.begin(), names.end(), [&](const auto& role) { return empty_name(role) || !unique.insert(role).second; }))
                warning("relationship.recursive.roles", "Give repeated participants distinct role names to explain this recursive relationship.", ref);
        }
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
