// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
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
// A sixteen-byte value worked out from another, the same way on every machine
// and in every run.
//
// This is how a generated relation is given an identity of its own without
// drawing one from the generator. ADR-001 forbids identity that moves and
// ADR-008 forbids reusing a conceptual identity one level down; a derived
// value satisfies both, and has the property the generator cannot offer --
// open the same project twice and the relations are the same relations, so
// everything kept against them is still theirs.
//
// FNV-1a, run four times from different starting points to fill the sixteen
// bytes. It is not a cryptographic hash and is not trying to be: it has to be
// stable, spread well enough that two origins do not collide, and nothing
// here is a secret. The version and variant bits are stamped afterwards so
// the result is a well-formed identity like any other.
Uuid derived(const Uuid& from, std::uint8_t kind) {
    Uuid made{};
    for (std::uint64_t pass = 0; pass < 4; ++pass) {
        std::uint64_t hash = 0xcbf29ce484222325ULL + pass * 0x9e3779b97f4a7c15ULL;
        const auto mix = [&hash](std::uint8_t byte) {
            hash ^= byte;
            hash *= 0x100000001b3ULL;
        };
        mix(kind);
        for (const auto byte : from.bytes) mix(byte);
        for (std::size_t i = 0; i < 4; ++i)
            made.bytes[pass * 4 + i] = static_cast<std::uint8_t>(hash >> (i * 8));
    }
    made.bytes[6] = static_cast<std::uint8_t>((made.bytes[6] & 0x0fU) | 0x70U);
    made.bytes[8] = static_cast<std::uint8_t>((made.bytes[8] & 0x3fU) | 0x80U);
    return made;
}
// What the origin was, so that two origins which happen to share a value but
// not a kind cannot derive the same identity.
constexpr std::uint8_t relation_kind = 0x52;      // 'R'
constexpr std::uint8_t foreign_key_kind = 0x4b;   // 'K'
} // namespace

RelationId relation_from(const ElementRef& origin) {
    return RelationId{derived(uuid(origin),
                              static_cast<std::uint8_t>(relation_kind + origin.index()))};
}

ForeignKeyId foreign_key_from(const LinkSource& origin) {
    const auto value = std::visit([](const auto& id) { return id.value; }, origin);
    return ForeignKeyId{derived(value,
                                static_cast<std::uint8_t>(foreign_key_kind + origin.index()))};
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

bool countable(LogicalType type) {
    switch (type) {
    case LogicalType::TinyInt: case LogicalType::SmallInt:
    case LogicalType::Int: case LogicalType::BigInt:
    case LogicalType::Decimal: case LogicalType::Numeric:
        return true;
    default:
        return false;
    }
}

TypeSize size_of(LogicalType type) {
    switch (type) {
    case LogicalType::Char: case LogicalType::Varchar:
    case LogicalType::NChar: case LogicalType::NVarchar:
    case LogicalType::Binary: case LogicalType::Varbinary:
    case LogicalType::Float:
    case LogicalType::Time: case LogicalType::DateTime2: case LogicalType::DateTimeOffset:
        return TypeSize::Length;
    case LogicalType::Decimal: case LogicalType::Numeric:
        return TypeSize::Precision;
    default:
        return TypeSize::None;
    }
}

bool deprecated_type(LogicalType type) {
    return type == LogicalType::Text || type == LogicalType::NText || type == LogicalType::Image;
}

// Every value the enum actually has. A file that names something outside it is
// refused rather than read as whatever happens to share its number.
bool known_type(LogicalType type) {
    switch (type) {
    case LogicalType::Unset:
    case LogicalType::Int: case LogicalType::BigInt: case LogicalType::SmallInt:
    case LogicalType::TinyInt: case LogicalType::Bit: case LogicalType::Decimal:
    case LogicalType::Numeric: case LogicalType::Money: case LogicalType::SmallMoney:
    case LogicalType::Float: case LogicalType::Real:
    case LogicalType::Char: case LogicalType::Varchar: case LogicalType::VarcharMax:
    case LogicalType::Text:
    case LogicalType::NChar: case LogicalType::NVarchar: case LogicalType::NVarcharMax:
    case LogicalType::NText:
    case LogicalType::Binary: case LogicalType::Varbinary: case LogicalType::VarbinaryMax:
    case LogicalType::Image:
    case LogicalType::Date: case LogicalType::Time: case LogicalType::DateTime:
    case LogicalType::DateTime2: case LogicalType::DateTimeOffset: case LogicalType::SmallDateTime:
    case LogicalType::UniqueIdentifier: case LogicalType::Xml: case LogicalType::RowVersion:
    case LogicalType::HierarchyId: case LogicalType::SqlVariant: case LogicalType::Cursor:
    case LogicalType::Table: case LogicalType::Geometry: case LogicalType::Geography:
        return true;
    }
    return false;
}

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

// What a schema line is drawn from, asked the same way a connector is. The
// three alternatives are every foreign key there is: a relationship's key on a
// participant, a multivalued attribute's table pointing home, and a subtype
// pointing at its parent. The first two are connectors under another name, so
// they are asked through the same question rather than a second copy of it.
bool link_exists(const Project& project, const LinkSource& link) {
    if (const auto* entity = std::get_if<EntityId>(&link)) return project.entities.contains(*entity);
    if (const auto* attribute = std::get_if<AttributeId>(&link))
        return connector_exists(project, ConnectorRef{*attribute});
    return connector_exists(project, ConnectorRef{std::get<ParticipantId>(link)});
}

std::size_t character_count(const std::string& text) {
    // The continuation bytes of a UTF-8 sequence are the ones that are not
    // counted; everything else begins a character. The text has already been
    // checked as UTF-8 by the time a range is measured against it.
    std::size_t characters = 0;
    for (const auto byte : text)
        if ((static_cast<unsigned char>(byte) & 0xc0U) != 0x80U) ++characters;
    return characters;
}

bool target_exists(const Project& project, const CommentTarget& target) {
    if (const auto* element = std::get_if<ElementRef>(&target)) return exists(project, *element);
    if (const auto* connector = std::get_if<ConnectorRef>(&target)) return connector_exists(project, *connector);
    const auto& anchor = std::get<TextAnchor>(target);
    if (!exists(project, anchor.owner)) return false;
    // A range has to lie inside the text it is pinned into. Text is edited
    // after a comment is pinned to it, so this is asked every time rather than
    // trusted once.
    const auto written = anchor.field == TextField::Name ? name(project, anchor.owner)
                                                         : description(project, anchor.owner);
    const auto characters = character_count(written);
    return anchor.begin <= characters && anchor.length <= characters - anchor.begin;
}

namespace {
// The comments pinned to one thing, in the order the project holds them, so a
// shape draws its remarks the same way twice.
template<class Match>
std::vector<CommentId> comments_matching(const Project& project, Match&& match) {
    std::vector<CommentId> found;
    for (const auto& [id, comment] : project.comments)
        if (std::any_of(comment.targets.begin(), comment.targets.end(), match)) found.push_back(id);
    return found;
}
} // namespace

std::vector<CommentId> comments_on(const Project& project, const ElementRef& ref) {
    return comments_matching(project, [&](const CommentTarget& target) {
        if (const auto* element = std::get_if<ElementRef>(&target)) return *element == ref;
        // A remark pinned into an element's own writing is a remark on it.
        if (const auto* anchor = std::get_if<TextAnchor>(&target)) return anchor->owner == ref;
        return false;
    });
}

std::vector<CommentId> comments_on_connector(const Project& project, const ConnectorRef& ref) {
    return comments_matching(project, [&](const CommentTarget& target) {
        const auto* connector = std::get_if<ConnectorRef>(&target);
        return connector != nullptr && *connector == ref;
    });
}

// Held between a quarter and sixteen times ordinary, so a box pulled to
// either extreme still draws a name that is some size rather than none.
double lettering_factor(const LetteringBase& base, double width, double height) {
    if (base.width <= 0 || base.height <= 0) return 1.0;
    return std::clamp(std::min(width / base.width, height / base.height), 0.25, 16.0);
}

std::string name(const Project& project, const ElementRef& ref) {
    return visit_element(project, ref, [](const auto* element) { return element ? element->name : std::string{}; });
}
std::string description(const Project& project, const ElementRef& ref) {
    return visit_element(project, ref, [](const auto* element) { return element ? element->description : std::string{}; });
}

namespace {
// Every identity a relation could have in this project, and every identity a
// foreign key could have.
//
// A derived identity cannot be turned back into the element it came from, so
// validation asks the question the other way round: it works out what the
// project could produce and checks that the schema's own state refers to one
// of those. Anything else is a remnant of something deleted.
//
// Deliberately looser than asking the conversion itself. Whether a particular
// element produces a table today depends on conversion decisions somebody can
// change; whether the element is still there does not, and that is what makes
// a remnant a remnant.
std::set<RelationId> possible_relations(const Project& project) {
    std::set<RelationId> possible;
    for (const auto& [id, entity] : project.entities) { (void)entity; possible.insert(relation_from(ElementRef{id})); }
    for (const auto& [id, attribute] : project.attributes) { (void)attribute; possible.insert(relation_from(ElementRef{id})); }
    for (const auto& [id, relationship] : project.relationships) { (void)relationship; possible.insert(relation_from(ElementRef{id})); }
    return possible;
}

std::set<ForeignKeyId> possible_foreign_keys(const Project& project) {
    std::set<ForeignKeyId> possible;
    for (const auto& [id, attribute] : project.attributes) { (void)attribute; possible.insert(foreign_key_from(LinkSource{id})); }
    for (const auto& [id, entity] : project.entities) { (void)entity; possible.insert(foreign_key_from(LinkSource{id})); }
    for (const auto& [id, relationship] : project.relationships) {
        (void)id;
        for (const auto& side : relationship.participants) possible.insert(foreign_key_from(LinkSource{side.id}));
    }
    return possible;
}
} // namespace

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
    // A comment written for the database is prose like a description, and is
    // held to the same bounds. It is checked separately because only the three
    // things that become tables and columns carry one.
    auto schema_comment = [&](const std::string& words, std::optional<ElementRef> ref) {
        if (words.size() > max_comment_bytes || !valid_text(words, true))
            error("text.comment.invalid", "Comments must be valid UTF-8 and fit in 16,384 bytes.", ref);
    };
    identity(project.id.value);
    text_fields(project.name, project.description);
    for (const auto& [id, entity] : project.entities) {
        const ElementRef ref = id;
        if (!project.layout.contains(ref)) error("layout.element.missing", "The entity has no canvas layout.", ref);
        identity(id.value, ref);
        if (entity.id != id) error("identity.key_mismatch", "The entity key and identifier differ.", ref);
        text_fields(entity.name, entity.description, ref);
        schema_comment(entity.comment, ref);
    }
    for (const auto& [id, attribute] : project.attributes) {
        const ElementRef ref = id;
        if (!project.layout.contains(ref)) error("layout.element.missing", "The attribute has no canvas layout.", ref);
        identity(id.value, ref);
        if (attribute.id != id) error("identity.key_mismatch", "The attribute key and identifier differ.", ref);
        text_fields(attribute.name, attribute.description, ref);
        schema_comment(attribute.comment, ref);
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
        schema_comment(relationship.comment, ref);
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
    if (project.lettering.size() > max_elements) error("lettering.limit", "The lettering entries exceed the element limit.");
    for (const auto& [ref, base] : project.lettering) {
        if (!exists(project, ref)) error("lettering.reference.missing", "A lettering size refers to a missing element.", ref);
        else if (!std::holds_alternative<EntityId>(ref) && !std::holds_alternative<RelationshipId>(ref)
                 && !std::holds_alternative<AttributeId>(ref))
            error("lettering.kind", "Only an entity, a relationship or an attribute has a lettering size.", ref);
        if (!std::isfinite(base.width) || !std::isfinite(base.height) || base.width <= 0 || base.height <= 0
            || base.width > max_coordinate || base.height > max_coordinate)
            error("lettering.invalid", "A lettering size must be a positive width and height.", ref);
    }
    // What turning an attribute into a column will need. A file that arrives
    // holding nonsense is refused whether or not anyone is currently looking
    // at these fields, because they are part of the model either way.
    for (const auto& [id, attribute] : project.attributes) {
        const ElementRef ref = id;
        if (size_of(attribute.logical_type) == TypeSize::None && attribute.logical_type != LogicalType::Unset
            && !known_type(attribute.logical_type))
            error("attribute.type.invalid", "The attribute's logical type is invalid.", ref);
        if (attribute.length > max_logical_length)
            error("attribute.length.limit", "A logical length must be within 1,000,000.", ref);
        // A size belongs only to the types that take one. Left on a Bit it
        // would be carried into the schema and mean nothing there.
        const auto takes = size_of(attribute.logical_type);
        if (attribute.length != 0 && takes == TypeSize::None)
            error("attribute.length.unused", "Only a sized type carries a length.", ref);
        // A scale is the digits after the point, so only a precision has one,
        // and it can never exceed the precision it is part of.
        if (attribute.scale != 0 && takes != TypeSize::Precision)
            error("attribute.scale.unused", "Only Decimal and Numeric carry a scale.", ref);
        if (attribute.scale > attribute.length)
            error("attribute.scale.invalid", "A scale cannot exceed its precision.", ref);
    }
    // Every answer must still be about something that is there. An element can
    // be deleted after a question about it was answered, and a decision left
    // pointing at nothing would be carried silently into a conversion.
    {
        const auto& decided = project.decisions;
        switch (decided.naming) {
        case TableNaming::Plural: case TableNaming::AsDrawn: break;
        default: error("decision.naming.invalid", "The table naming convention is invalid.");
        }
        for (const auto& [id, strategy] : decided.isa) {
            const ElementRef ref = id;
            if (!project.specializations.contains(id))
                error("decision.isa.missing", "A mapping strategy refers to a missing hierarchy.", ref);
            switch (strategy) {
            case IsaStrategy::PerSubclass: case IsaStrategy::SingleTable: case IsaStrategy::PerConcrete: break;
            default: error("decision.isa.invalid", "The mapping strategy is invalid.", ref);
            }
        }
        for (const auto& [id, mode] : decided.composite) {
            const ElementRef ref = id;
            const auto found = project.attributes.find(id);
            if (found == project.attributes.end())
                error("decision.composite.missing", "A composite decision refers to a missing attribute.", ref);
            else if (found->second.kind != AttributeKind::Composite)
                error("decision.composite.unused", "Only a composite attribute is asked what it becomes.", ref);
            switch (mode) {
            case CompositeMode::Parts: case CompositeMode::Whole: case CompositeMode::Both: break;
            default: error("decision.composite.invalid", "The composite decision is invalid.", ref);
            }
        }
        for (const auto& [id, side] : decided.one_to_one_key) {
            const ElementRef ref = id;
            const auto found = project.relationships.find(id);
            if (found == project.relationships.end()) {
                error("decision.key_side.missing", "A key-side decision refers to a missing relationship.", ref);
                continue;
            }
            const auto& sides = found->second.participants;
            if (std::none_of(sides.begin(), sides.end(),
                             [&](const Participant& one) { return one.id == side; }))
                error("decision.key_side.invalid", "The chosen side does not belong to this relationship.", ref);
        }
        for (const auto& [id, keyed] : decided.bridge_key) {
            const ElementRef ref = id;
            // A bridge answered and then made one-to-many keeps its answer
            // unused rather than invalid: it applies again the moment the
            // relationship is many-to-many again.
            if (!project.relationships.contains(id))
                error("decision.bridge_key.missing", "A bridge-key decision refers to a missing relationship.", ref);
            switch (keyed) {
            case BridgeKey::Pair: case BridgeKey::Own: break;
            default: error("decision.bridge_key.invalid", "The bridge-key decision is invalid.", ref);
            }
        }
        for (const auto& [id, chosen] : decided.junction_name) {
            const ElementRef ref = id;
            if (!project.relationships.contains(id))
                error("decision.junction.missing", "A bridge name refers to a missing relationship.", ref);
            if (chosen.size() > max_name_bytes || !valid_text(chosen, false) || empty_name(chosen))
                error("decision.junction.invalid", "A bridge table name must be valid text.", ref);
        }
        for (const auto& [id, chosen] : decided.identifier) {
            const ElementRef ref = id;
            if (!project.entities.contains(id))
                error("decision.identifier.missing", "An identifier refers to a missing entity.", ref);
            const auto found = project.attributes.find(chosen);
            if (found == project.attributes.end() || found->second.owner != AttributeOwner{ElementRef{id}})
                error("decision.identifier.invalid", "The chosen identifier is not this entity's attribute.", ref);
        }
        for (const auto& [relation, chosen] : decided.table_name) {
            if (!possible_relations(project).contains(relation))
                error("decision.table_name.missing", "A table name refers to a missing element.");
            if (chosen.size() > max_name_bytes || !valid_text(chosen, false) || empty_name(chosen))
                error("decision.table_name.invalid", "A table name must be valid text.");
        }
    }
    const auto relations = possible_relations(project);
    const auto foreign_keys = possible_foreign_keys(project);
    // What the schema has been told that the diagram does not say. ADR-010
    // allows the two levels to differ, and a difference is a fact to be
    // recorded rather than a mistake -- but a difference has to be about
    // something. A column added to a table whose element is gone, and a hidden
    // attribute that no longer exists, are not differences but remnants.
    // Nothing asked this until now, so a remnant was written to the file and
    // read back without complaint.
    {
        std::size_t added = 0;
        for (const auto& [relation, columns] : project.schema.added) {
            const std::optional<ElementRef> ref;
            if (!relations.contains(relation))
                error("schema.table.missing", "A schema column was added to a missing element.");
            added += columns.size();
            for (const auto& column : columns) {
                identity(column.id.value, ref);
                if (column.name.size() > max_name_bytes || !valid_text(column.name, false) || empty_name(column.name))
                    error("schema.column.name.invalid", "A schema column needs a valid name.", ref);
                schema_comment(column.comment, ref);
                // A column added here becomes a column like any other, so it is
                // held to the same type rules an attribute is held to.
                const auto takes = size_of(column.logical_type);
                if (takes == TypeSize::None && column.logical_type != LogicalType::Unset
                    && !known_type(column.logical_type))
                    error("schema.column.type.invalid", "The schema column's logical type is invalid.", ref);
                if (column.length > max_logical_length)
                    error("schema.column.length.limit", "A logical length must be within 1,000,000.", ref);
                if (column.length != 0 && takes == TypeSize::None)
                    error("schema.column.length.unused", "Only a sized type carries a length.", ref);
                if (column.scale != 0 && takes != TypeSize::Precision)
                    error("schema.column.scale.unused", "Only Decimal and Numeric carry a scale.", ref);
                if (column.scale > column.length)
                    error("schema.column.scale.invalid", "A scale cannot exceed its precision.", ref);
            }
        }
        if (added > max_elements)
            error("schema.column.limit", "The added schema columns exceed the element limit.");
        if (project.schema.hidden.size() > max_elements)
            error("schema.hidden.limit", "The hidden columns exceed the element limit.");
        for (const auto& id : project.schema.hidden) {
            const ElementRef ref = id;
            if (!project.attributes.contains(id))
                error("schema.hidden.missing", "A hidden column refers to a missing attribute.", ref);
        }
        // A name given to an invented key is text like a table's typed name,
        // and belongs to an element that has to still be there.
        for (const auto& [relation, chosen] : project.schema.key_names) {
            if (!relations.contains(relation))
                error("schema.key_name.missing", "A key name refers to a missing element.");
            if (chosen.size() > max_name_bytes || !valid_text(chosen, false) || empty_name(chosen))
                error("schema.key_name.invalid", "A key name must be valid text.");
        }
        // A key told to count itself up has to belong to a relation that is
        // still there, for the same reason its name does.
        for (const auto& relation : project.schema.counting_keys)
            if (!relations.contains(relation))
                error("schema.counting_key.missing", "A counting key refers to a missing element.");
    }
    // An attribute with no logical type yet has not answered a question that
    // conversion will ask. That is a matter of readiness rather than validity:
    // validate() says whether the model is sound, and a model is allowed to be
    // half answered while it is being worked on. Phase 14's readiness() is
    // where the question is asked, under the issue classes of ADR-009.
    if (project.comments.size() > max_elements) error("comment.limit", "The comments exceed the element limit.");
    for (const auto& [id, comment] : project.comments) {
        // A comment has no element reference of its own, so anything wrong with
        // it is reported against the first thing it is pinned to, which is what
        // the canvas would highlight.
        std::optional<ElementRef> where;
        for (const auto& target : comment.targets) {
            if (const auto* element = std::get_if<ElementRef>(&target)) { where = *element; break; }
            if (const auto* anchor = std::get_if<TextAnchor>(&target)) { where = anchor->owner; break; }
            const auto& connector = std::get<ConnectorRef>(target);
            if (const auto* attribute = std::get_if<AttributeId>(&connector)) { where = *attribute; break; }
        }
        identity(id.value, where);
        if (comment.id != id) error("identity.key_mismatch", "The comment key and identifier differ.", where);
        if (comment.text.size() > max_comment_bytes || !valid_text(comment.text, true))
            error("comment.text.invalid", "A comment must be valid text within 16 KiB.", where);
        else if (empty_name(comment.text))
            error("comment.text.empty", "A comment must say something.", where);
        // A comment pinned to nothing could never be found again, so it is not
        // a comment that may be saved. Deleting the last thing a comment is
        // pinned to deletes the comment with it, in the same edit.
        if (comment.targets.empty()) error("comment.target.missing", "A comment must be pinned to something.", where);
        if (comment.targets.size() > max_comment_targets)
            error("comment.target.limit", "A comment exceeds the element limit for what it is pinned to.", where);
        std::set<CommentTarget> seen;
        for (const auto& target : comment.targets) {
            if (!target_exists(project, target))
                error("comment.target.dangling", "A comment is pinned to something that is no longer there.", where);
            if (!seen.insert(target).second)
                error("comment.target.duplicate", "A comment is pinned to the same thing twice.", where);
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
    // The schema's arrangement is presentation exactly as the layout above is,
    // so it is held to the same bounds and asked the same question: a place, a
    // size and a shaped line must each be about something that is still there.
    {
        const auto& arranged = project.schema_layout;
        const auto on_canvas = [](const Point& point) {
            return std::isfinite(point.x) && std::isfinite(point.y)
                && std::abs(point.x) <= max_coordinate && std::abs(point.y) <= max_coordinate;
        };
        if (arranged.tables.size() > max_elements || arranged.widths.size() > max_elements
            || arranged.heights.size() > max_elements || arranged.lines.size() > max_elements)
            error("schema.layout.limit", "The schema arrangement exceeds the element limit.");
        for (const auto& [relation, point] : arranged.tables) {
            if (!relations.contains(relation))
                error("schema.layout.reference.missing", "A schema place refers to a missing element.");
            if (!on_canvas(point))
                error("schema.layout.bounds.invalid", "A schema table must be placed within the supported canvas.");
        }
        // A width and a height are pulled separately and stored separately, so
        // they are asked separately rather than assumed to arrive together.
        for (const auto* sizes : {&arranged.widths, &arranged.heights})
            for (const auto& [relation, size] : *sizes) {
                if (!relations.contains(relation))
                    error("schema.layout.reference.missing", "A schema size refers to a missing element.");
                if (!std::isfinite(size) || size <= 0 || size > max_coordinate)
                    error("schema.layout.size.invalid", "A schema table's size must be finite, positive, and within the supported canvas.");
            }
        for (const auto& [key, line] : arranged.lines) {
            // A foreign key has an identity of its own now, and that identity
            // cannot be turned back into the element behind it, so a line is
            // reported without one. What is asked is the same question: does
            // the key this line was drawn for still exist?
            const std::optional<ElementRef> owner;
            if (!foreign_keys.contains(key))
                error("schema.line.reference.missing", "A schema line refers to a link that no longer exists.", owner);
            if (line.route.size() > max_elements)
                error("schema.line.route.limit", "A schema line route exceeds the element limit.", owner);
            for (const auto& point : line.route)
                if (!on_canvas(point))
                    error("schema.line.route.invalid", "A schema line route point must be within the supported canvas.", owner);
            // An end is a fraction of its table's box where it sits on the
            // table and a point on the schema where it does not. Both are
            // within the canvas, which is what is asked here; which of the two
            // it is belongs to the schema that reads it.
            for (const auto& end : {line.from, line.to})
                if (end && !on_canvas(end->at))
                    error("schema.line.end.invalid", "A schema line's end must be within the supported canvas.", owner);
        }
    }
    return issues;
}

} // namespace erdflow::domain
