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
using ParticipantId = Id<struct ParticipantTag>;
using ElementRef = std::variant<EntityId, AttributeId, RelationshipId>;
using AttributeOwner = ElementRef;

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
struct Participant {
    ParticipantId id;
    EntityId entity;
    Cardinality maximum = Cardinality::Many;
    Participation participation = Participation::Partial;
    std::string role;
    auto operator<=>(const Participant&) const = default;
};
struct Relationship {
    RelationshipId id;
    std::string name;
    std::string description;
    std::vector<Participant> participants;
    auto operator<=>(const Relationship&) const = default;
};
struct Project {
    ProjectId id;
    std::string name = "Untitled";
    std::map<EntityId, Entity> entities;
    std::map<AttributeId, Attribute> attributes;
    std::map<RelationshipId, Relationship> relationships;
    std::map<ElementRef, Rect> layout;
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
[[nodiscard]] std::string name(const Project& project, const ElementRef& ref);
[[nodiscard]] std::string description(const Project& project, const ElementRef& ref);
[[nodiscard]] std::vector<Issue> validate(const Project& project);

inline constexpr std::size_t max_elements = 10000;
inline constexpr std::size_t max_name_bytes = 512;
inline constexpr std::size_t max_description_bytes = 16384;
inline constexpr double max_coordinate = 100000;

} // namespace erdflow::domain
