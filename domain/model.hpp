#pragma once

#include <array>
#include <compare>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace erdflow::domain {

struct Uuid {
    std::array<std::uint8_t, 16> bytes{};
    auto operator<=>(const Uuid&) const = default;
    [[nodiscard]] bool valid() const;
};

template<class Tag> struct Id {
    Uuid value;
    auto operator<=>(const Id&) const = default;
};
using ProjectId = Id<struct ProjectTag>;
using EntityId = Id<struct EntityTag>;
using AttributeId = Id<struct AttributeTag>;
using RelationshipId = Id<struct RelationshipTag>;
using SpecializationId = Id<struct SpecializationTag>;
using ParticipantId = Id<struct ParticipantTag>;
// A specialization is a placed element of its own: it carries the ISA triangle
// on the canvas and the constraints that decide how it converts to relations.
using ElementRef = std::variant<EntityId, AttributeId, RelationshipId, SpecializationId>;
// An attribute belongs to an entity, a relationship, or a composite attribute;
// never to a specialization, which owns no data of its own.
using AttributeOwner = ElementRef;
// A connector is drawn from the record that creates it, so it is identified by
// that record rather than by its own identity: an attribute's ownership link,
// or one relationship participant. Connectors are therefore never orphaned.
using ConnectorRef = std::variant<AttributeId, ParticipantId>;

struct Rect {
    double x = 0;
    double y = 0;
    double width = 160;
    double height = 80;
    auto operator<=>(const Rect&) const = default;
};

enum class AttributeKind { Normal, Key, Composite, Multivalued, Derived };
enum class Cardinality { One, Many };
enum class Participation { Partial, Total };
// Whether an instance of the supertype may belong to more than one subtype,
// and whether it must belong to at least one. Together these choose the
// relational mapping strategy when the model is converted.
enum class Disjointness { Disjoint, Overlapping };
enum class Completeness { Partial, Total };
// Which way the hierarchy was read. It is not only provenance: the ISA triangle
// points at the supertype when generalising and at the subtypes when
// specialising, so the direction is part of the notation and is stored.
enum class Inheritance { Generalization, Specialization };

struct Entity {
    EntityId id;
    std::string name;
    std::string description;
    auto operator<=>(const Entity&) const = default;
};
struct Attribute {
    AttributeId id;
    std::string name;
    std::string description;
    AttributeKind kind = AttributeKind::Normal;
    std::optional<AttributeOwner> owner;
    auto operator<=>(const Attribute&) const = default;
};
// A participant attaches to an entity, or to an associative relationship that
// is acting as one. Only an associative relationship may be a target.
using ParticipantTarget = std::variant<EntityId, RelationshipId>;

struct Participant {
    ParticipantId id;
    ParticipantTarget target;
    Cardinality maximum = Cardinality::Many;
    Participation participation = Participation::Partial;
    std::string role;
    auto operator<=>(const Participant&) const = default;
};
struct Relationship {
    RelationshipId id;
    std::string name;
    std::string description;
    // An associative relationship carries its own identity and may participate
    // in further relationships. It is drawn as a diamond inside a rectangle.
    bool associative = false;
    std::vector<Participant> participants;
    auto operator<=>(const Relationship&) const = default;
};
struct Specialization {
    SpecializationId id;
    std::string name;
    std::string description;
    Inheritance direction = Inheritance::Specialization;
    // Absent while the triangle has been placed but not yet connected. Like a
    // relationship with too few participants, that is work in progress.
    std::optional<EntityId> supertype;
    std::vector<EntityId> subtypes;
    Disjointness constraint = Disjointness::Disjoint;
    Completeness completeness = Completeness::Partial;
    auto operator<=>(const Specialization&) const = default;
};
struct Project {
    ProjectId id;
    std::string name = "Untitled";
    std::map<EntityId, Entity> entities;
    std::map<AttributeId, Attribute> attributes;
    std::map<RelationshipId, Relationship> relationships;
    std::map<SpecializationId, Specialization> specializations;
    std::map<ElementRef, Rect> layout;
    // Signed perpendicular bend, in canvas units, for connectors the user has
    // shaped. An absent entry means the connector is routed automatically.
    std::map<ConnectorRef, double> connectors;
    auto operator<=>(const Project&) const = default;
};

enum class Severity { Error, Warning, Info };
struct Issue {
    Severity severity;
    std::string code;
    std::string message;
    std::optional<ElementRef> element;
    bool blocks_save = false;
};

[[nodiscard]] Uuid uuid(const ElementRef& ref);
[[nodiscard]] bool exists(const Project& project, const ElementRef& ref);
// A connector exists while the record that draws it does: an owned attribute,
// or a participant still listed by its relationship.
[[nodiscard]] bool connector_exists(const Project& project, const ConnectorRef& ref);
// The element a participant attaches to, as a general element reference.
[[nodiscard]] ElementRef target_ref(const ParticipantTarget& target);
[[nodiscard]] std::string name(const Project& project, const ElementRef& ref);
[[nodiscard]] std::string description(const Project& project, const ElementRef& ref);
[[nodiscard]] std::vector<Issue> validate(const Project& project);

inline constexpr std::size_t max_elements = 10000;
inline constexpr std::size_t max_name_bytes = 512;
inline constexpr std::size_t max_description_bytes = 16384;
inline constexpr double max_coordinate = 100000;

} // namespace erdflow::domain
