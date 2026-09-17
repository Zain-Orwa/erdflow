#include "infrastructure/project_store.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <stdexcept>
#include <vector>

namespace erdflow::infrastructure {
using namespace domain;
namespace {
// Version 2 added connector shapes; version 3 added associative relationships
// and participants that may target one; version 4 added specializations, and
// version 5 recorded which way each one was read; version 6 lets a placed
// triangle wait for its supertype.
// Earlier versions remain readable; the format specification states the
// compatibility rule for each.
// Version 11 adds pictures and notes, the visual aids placed on the canvas;
// version 12 lets an element's surface be see-through by a percentage;
// version 13 adds weak entities and identifying relationships; version 14 adds
// the paper the diagram is drawn on; version 15 lets a note be a plain one, a
// single character drawn bare on the diagram.
constexpr int current_format_version = 15;
QString text(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
Uuid bytes_of(const QUuid& value) {
    Uuid result;
    const auto bytes = value.toRfc4122();
    std::memcpy(result.bytes.data(), bytes.constData(), result.bytes.size());
    return result;
}
[[noreturn]] void invalid(const QString& detail) { throw std::runtime_error(detail.toStdString()); }
QJsonObject object(const QJsonValue& value, std::initializer_list<const char*> keys) {
    if (!value.isObject()) invalid("Expected a project object.");
    const auto result = value.toObject();
    if (result.size() != static_cast<qsizetype>(keys.size())) invalid("Missing or unsupported project fields. This file cannot be safely edited.");
    for (const auto* key : keys) if (!result.contains(QLatin1String(key))) invalid("Missing a required project field.");
    return result;
}
std::string string(const QJsonValue& value, std::size_t max = max_name_bytes) {
    if (!value.isString()) invalid("Expected a text field.");
    const auto unicode = value.toString();
    if (!unicode.isValidUtf16()) invalid("A text field contains an unpaired Unicode surrogate.");
    const auto result = unicode.toUtf8();
    if (static_cast<std::size_t>(result.size()) > max) invalid("A text field exceeds the project limit.");
    return result.toStdString();
}
QJsonArray array(const QJsonValue& value) {
    if (!value.isArray()) invalid("Expected an array.");
    auto result = value.toArray();
    if (result.size() > static_cast<qsizetype>(max_elements)) invalid("The project exceeds its element limit.");
    return result;
}
Uuid parse_id(const QJsonValue& value) {
    const auto input = value.isString() ? value.toString() : QString{};
    const QUuid id(input);
    if (input.size() != 36 || id.toString(QUuid::WithoutBraces) != input)
        invalid("Project identifiers must use canonical lowercase UUID text.");
    auto result = bytes_of(id);
    if (!result.valid()) invalid("Project identifiers must be UUIDv7 values.");
    return result;
}
QJsonObject reference(const ElementRef& ref) {
    const char* type = std::holds_alternative<EntityId>(ref) ? "entity"
        : std::holds_alternative<AttributeId>(ref) ? "attribute"
        : std::holds_alternative<SpecializationId>(ref) ? "specialization"
        : std::holds_alternative<PictureId>(ref) ? "picture"
        : std::holds_alternative<NoteId>(ref) ? "note" : "relationship";
    return {{"type", QLatin1String(type)}, {"id", uuid_text(uuid(ref))}};
}
QJsonObject connector_reference(const ConnectorRef& ref) {
    const bool attribute = std::holds_alternative<AttributeId>(ref);
    const auto id = attribute ? std::get<AttributeId>(ref).value : std::get<ParticipantId>(ref).value;
    return {{"type", QLatin1String(attribute ? "attribute" : "participant")}, {"id", uuid_text(id)}};
}
ConnectorRef parse_connector_reference(const QJsonValue& value) {
    auto o = object(value, {"type", "id"});
    auto id = parse_id(o["id"]);
    const auto type = string(o["type"]);
    if (type == "attribute") return AttributeId{id};
    if (type == "participant") return ParticipantId{id};
    invalid("Unsupported connector link type.");
}
ElementRef parse_ref(const QJsonValue& value) {
    auto o = object(value, {"type", "id"});
    auto id = parse_id(o["id"]);
    const auto type = string(o["type"]);
    if (type == "entity") return EntityId{id};
    if (type == "attribute") return AttributeId{id};
    if (type == "relationship") return RelationshipId{id};
    if (type == "specialization") return SpecializationId{id};
    if (type == "picture") return PictureId{id};
    if (type == "note") return NoteId{id};
    invalid("Unsupported element type.");
}
// A picture's bytes travel as base64 text. Anything that is not base64, or
// that decodes to more than a picture may hold, is refused rather than
// trimmed; the domain then checks that the bytes could be an image at all.
std::vector<std::uint8_t> image_bytes(const QJsonValue& value) {
    if (!value.isString()) invalid("A picture's image must be base64 text.");
    const auto encoded = value.toString().toLatin1();
    if (encoded.isEmpty()) return {};
    if (static_cast<std::size_t>(encoded.size()) > (max_image_bytes + 2) / 3 * 4)
        invalid("A picture's image exceeds the 2 MiB limit.");
    const auto decoded = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) invalid("A picture's image must be base64 text.");
    const auto& bytes = *decoded;
    return {bytes.begin(), bytes.end()};
}
QString image_text(const std::vector<std::uint8_t>& image) {
    return QString::fromLatin1(QByteArray::fromRawData(reinterpret_cast<const char*>(image.data()),
                                                       static_cast<qsizetype>(image.size())).toBase64());
}
QString background_name(BackgroundStyle style) {
    switch (style) {
    case BackgroundStyle::Theme: return "theme";
    case BackgroundStyle::Squares: return "squares";
    case BackgroundStyle::Lines: return "lines";
    case BackgroundStyle::Dots: return "dots";
    case BackgroundStyle::Image: return "image";
    }
    invalid("Invalid background style.");
}
BackgroundStyle parse_background_style(const QJsonValue& value) {
    const auto style = string(value);
    if (style == "theme") return BackgroundStyle::Theme;
    if (style == "squares") return BackgroundStyle::Squares;
    if (style == "lines") return BackgroundStyle::Lines;
    if (style == "dots") return BackgroundStyle::Dots;
    if (style == "image") return BackgroundStyle::Image;
    invalid("Unsupported background style.");
}
QString kind_name(AttributeKind kind) {
    switch (kind) {
    case AttributeKind::Normal: return "normal";
    case AttributeKind::Key: return "key";
    case AttributeKind::Composite: return "composite";
    case AttributeKind::Multivalued: return "multivalued";
    case AttributeKind::Derived: return "derived";
    }
    invalid("Invalid attribute kind.");
}
AttributeKind parse_kind(const QJsonValue& value) {
    const auto kind = string(value);
    if (kind == "normal") return AttributeKind::Normal;
    if (kind == "key") return AttributeKind::Key;
    if (kind == "composite") return AttributeKind::Composite;
    if (kind == "multivalued") return AttributeKind::Multivalued;
    if (kind == "derived") return AttributeKind::Derived;
    invalid("Unsupported attribute kind.");
}
double number(const QJsonValue& value) {
    if (!value.isDouble() || !std::isfinite(value.toDouble())) invalid("Invalid layout number.");
    return value.toDouble();
}
void require_valid(const Project& project) {
    for (const auto& issue : validate(project)) if (issue.blocks_save) invalid(text(issue.message));
}
int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

// Canonical UTF-8 form of one object key, scanned within the token itself.
// Invoking the JSON parser per key made rejecting a hostile 8 MiB file cost
// about seventeen times a parse-only pass, so keys are unescaped directly.
// The authoritative document parse below still validates JSON syntax, values,
// and text encoding; this pass only has to agree on key identity.
QByteArray key_text(const QByteArray& input, qsizetype open_quote, qsizetype close_quote) {
    const auto* begin = input.constData() + open_quote + 1;
    const auto length = close_quote - open_quote - 1;
    if (!QByteArray::fromRawData(begin, length).contains('\\')) return {begin, length};
    QString decoded;
    decoded.reserve(length);
    auto code_unit = [&](qsizetype at) {
        int value = 0;
        for (qsizetype digit = 0; digit < 4; ++digit) {
            const auto parsed = hex_digit(begin[at + digit]);
            if (parsed < 0) invalid("Invalid project JSON field name.");
            value = value * 16 + parsed;
        }
        return static_cast<char16_t>(value);
    };
    for (qsizetype at = 0; at < length;) {
        if (begin[at] != '\\') {
            const auto run = at;
            while (at < length && begin[at] != '\\') ++at;
            decoded += QString::fromUtf8(begin + run, at - run);
            continue;
        }
        if (++at >= length) invalid("Invalid project JSON field name.");
        switch (const auto escape = begin[at++]; escape) {
        case '"': decoded += QChar(u'"'); break;
        case '\\': decoded += QChar(u'\\'); break;
        case '/': decoded += QChar(u'/'); break;
        case 'b': decoded += QChar(u'\b'); break;
        case 'f': decoded += QChar(u'\f'); break;
        case 'n': decoded += QChar(u'\n'); break;
        case 'r': decoded += QChar(u'\r'); break;
        case 't': decoded += QChar(u'\t'); break;
        case 'u': {
            if (length - at < 4) invalid("Invalid project JSON field name.");
            const auto leading = code_unit(at);
            at += 4;
            if (QChar::isLowSurrogate(leading))
                invalid("A project field name contains an unpaired Unicode surrogate.");
            decoded += QChar(leading);
            if (!QChar::isHighSurrogate(leading)) break;
            if (length - at < 6 || begin[at] != '\\' || begin[at + 1] != 'u')
                invalid("A project field name contains an unpaired Unicode surrogate.");
            const auto trailing = code_unit(at + 2);
            if (!QChar::isLowSurrogate(trailing))
                invalid("A project field name contains an unpaired Unicode surrogate.");
            at += 6;
            decoded += QChar(trailing);
            break;
        }
        default: invalid("Invalid project JSON field name.");
        }
    }
    return decoded.toUtf8();
}

void check_structure(const QByteArray& input) {
    // Objects are capped at sixteen keys, so a linear scan over a reused buffer
    // settles duplicates without building an ordered container per object. A
    // hostile file can contain a million objects; the scopes are therefore
    // pooled by depth and only their contents are cleared.
    struct Scope {
        char opener = 0;
        std::vector<QByteArray> keys;
    };
    std::vector<Scope> scopes(32);
    std::size_t depth = 0;
    auto whitespace = [](char ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'; };
    for (qsizetype i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        if (ch == '{' || ch == '[') {
            if (depth == 32) invalid("Project nesting exceeds the supported limit.");
            scopes[depth].opener = ch;
            scopes[depth].keys.clear();
            ++depth;
        } else if (ch == '}' || ch == ']') {
            if (depth == 0 || scopes[depth - 1].opener != (ch == '}' ? '{' : '['))
                invalid("Invalid project JSON structure.");
            --depth;
        } else if (ch == '"') {
            const qsizetype start = i++;
            while (i < input.size() && input[i] != '"') {
                if (input[i] == '\\') ++i;
                ++i;
            }
            if (i >= input.size()) invalid("Unterminated project JSON string.");
            auto next = i + 1;
            while (next < input.size() && whitespace(input[next])) ++next;
            if (next == input.size() || input[next] != ':') continue;
            if (depth == 0 || scopes[depth - 1].opener != '{') invalid("Invalid project JSON field.");
            auto& keys = scopes[depth - 1].keys;
            // QJson normalizes duplicate object keys, so they are detected here
            // before the authoritative parse discards the collision.
            auto key = key_text(input, start, i);
            if (std::find(keys.begin(), keys.end(), key) != keys.end())
                invalid("Duplicate project JSON field.");
            keys.push_back(std::move(key));
            // Current objects have at most six fields. This conservative cap
            // bounds preflight bookkeeping for hostile objects before parsing.
            if (keys.size() > 16) invalid("Unsupported project fields.");
        }
    }
}
} // namespace

Uuid QtIdGenerator::next() { return bytes_of(QUuid::createUuidV7()); }
QString uuid_text(Uuid value) {
    return QUuid::fromRfc4122(QByteArrayView(reinterpret_cast<const char*>(value.bytes.data()), 16)).toString(QUuid::WithoutBraces);
}

QByteArray ErdxProjectStore::encode(const Project& project) {
    require_valid(project);
    // Reject clearly oversized models before constructing duplicate Qt/JSON
    // representations. Encoded JSON can only be larger than its UTF-8 text.
    std::size_t text_bytes = project.name.size();
    const auto budget = static_cast<std::size_t>(max_file_bytes);
    auto count = [&](const std::string& value) {
        if (value.size() > budget - text_bytes) invalid("The project exceeds the 8 MiB file limit.");
        text_bytes += value.size();
    };
    for (const auto& [id, entity] : project.entities) { (void)id; count(entity.name); count(entity.description); }
    for (const auto& [id, attribute] : project.attributes) { (void)id; count(attribute.name); count(attribute.description); }
    for (const auto& [id, relationship] : project.relationships) {
        (void)id;
        count(relationship.name); count(relationship.description);
        for (const auto& participant : relationship.participants) count(participant.role);
    }
    for (const auto& [id, specialization] : project.specializations) { (void)id; count(specialization.name); count(specialization.description); }
    for (const auto& [id, note] : project.notes) { (void)id; count(note.name); count(note.description); }
    // An image is written as base64, which is a third again as long as the bytes.
    for (const auto& [id, picture] : project.pictures) {
        (void)id;
        count(picture.name); count(picture.description);
        const auto encoded = (picture.image.size() + 2) / 3 * 4;
        if (encoded > budget - text_bytes) invalid("The project exceeds the 8 MiB file limit.");
        text_bytes += encoded;
    }
    QJsonArray entities, attributes, relationships, layout;
    for (const auto& [id, entity] : project.entities)
        entities.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(entity.name)},
                                    {"description", text(entity.description)}, {"weak", entity.weak}});
    for (const auto& [id, attribute] : project.attributes)
        attributes.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(attribute.name)},
            {"description", text(attribute.description)}, {"kind", kind_name(attribute.kind)},
            {"owner", attribute.owner ? QJsonValue(reference(*attribute.owner)) : QJsonValue(QJsonValue::Null)}});
    for (const auto& [id, relationship] : project.relationships) {
        QJsonArray participants;
        for (const auto& p : relationship.participants)
            participants.append(QJsonObject{{"id", uuid_text(p.id.value)}, {"target", reference(target_ref(p.target))},
                {"maximum", p.maximum == Cardinality::One ? "one" : "many"},
                {"participation", p.participation == Participation::Total ? "total" : "partial"}, {"role", text(p.role)},
                {"show_constraints", p.show_constraints}});
        relationships.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(relationship.name)},
            {"description", text(relationship.description)}, {"associative", relationship.associative},
            {"identifying", relationship.identifying}, {"participants", participants}});
    }
    for (const auto& [ref, rect] : project.layout)
        layout.append(QJsonObject{{"element", reference(ref)}, {"x", rect.x}, {"y", rect.y}, {"width", rect.width}, {"height", rect.height}});
    QJsonArray specializations;
    for (const auto& [id, specialization] : project.specializations) {
        QJsonArray subtypes;
        for (const auto& subtype : specialization.subtypes) subtypes.append(uuid_text(subtype.value));
        specializations.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(specialization.name)},
            {"description", text(specialization.description)},
            {"direction", specialization.direction == Inheritance::Generalization ? "generalization" : "specialization"},
            {"supertype", specialization.supertype ? QJsonValue(uuid_text(specialization.supertype->value))
                                                   : QJsonValue(QJsonValue::Null)},
            {"subtypes", subtypes},
            {"constraint", specialization.constraint == Disjointness::Overlapping ? "overlapping" : "disjoint"},
            {"completeness", specialization.completeness == Completeness::Total ? "total" : "partial"}});
    }
    QJsonArray colours;
    for (const auto& [ref, colour] : project.colours)
        colours.append(QJsonObject{{"element", reference(ref)},
                                   {"red", colour.red}, {"green", colour.green}, {"blue", colour.blue}});
    QJsonArray transparency;
    for (const auto& [ref, percent] : project.transparency)
        transparency.append(QJsonObject{{"element", reference(ref)}, {"percent", percent}});
    QJsonArray pictures, notes;
    for (const auto& [id, picture] : project.pictures)
        pictures.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(picture.name)},
            {"description", text(picture.description)}, {"image", image_text(picture.image)}});
    for (const auto& [id, note] : project.notes)
        notes.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(note.name)},
            {"description", text(note.description)}, {"plain", note.plain}});
    QJsonArray connectors;
    for (const auto& [ref, connector] : project.connectors) {
        // Both anchors are always written, null when the join is not pinned, so
        // that every connector in a file carries the same field set. The reader
        // refuses an object whose keys it does not expect, so an optional field
        // has to be a present null rather than an absent key.
        QJsonArray route;
        for (const auto& point : connector.waypoints)
            route.append(QJsonObject{{"x", point.x}, {"y", point.y}});
        connectors.append(QJsonObject{{"link", connector_reference(ref)}, {"offset", connector.offset},
            {"owner_anchor", connector.owner_anchor ? QJsonValue(*connector.owner_anchor) : QJsonValue()},
            {"child_anchor", connector.child_anchor ? QJsonValue(*connector.child_anchor) : QJsonValue()},
            {"waypoints", route}});
    }
    auto bytes = QJsonDocument(QJsonObject{{"format", "erdflow"}, {"format_version", current_format_version},
        {"project", QJsonObject{{"id", uuid_text(project.id.value)}, {"name", text(project.name)},
        {"entities", entities}, {"attributes", attributes}, {"relationships", relationships},
        {"layout", layout}, {"connectors", connectors}, {"colours", colours},
        {"specializations", specializations}, {"pictures", pictures}, {"notes", notes},
        {"transparency", transparency},
        {"background", QJsonObject{{"style", background_name(project.background.style)},
                                   {"strength", project.background.strength},
                                   {"image", image_text(project.background.image)}}}}}}).toJson(QJsonDocument::Indented);
    if (bytes.size() > max_file_bytes) invalid("The project exceeds the 8 MiB file limit.");
    return bytes;
}

application::LoadResult ErdxProjectStore::decode(const QByteArray& input) {
    try {
        if (input.size() > max_file_bytes) invalid("The project exceeds the 8 MiB file limit.");
        check_structure(input);
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(input, &error);
        if (error.error != QJsonParseError::NoError) invalid("Invalid project JSON: " + error.errorString());
        const auto root = object(document.isObject() ? QJsonValue(document.object()) : QJsonValue{}, {"format", "format_version", "project"});
        if (string(root["format"]) != "erdflow") invalid("This is not an ERDFlow project.");
        // Version 1 had no connector shapes. It still opens, and its links start
        // routed automatically; saving then writes the current version.
        const auto version = root["format_version"];
        const auto number_version = version.isDouble() ? version.toDouble() : 0;
        if (number_version < 1 || number_version > current_format_version || number_version != std::floor(number_version))
            invalid("Unsupported project version. Use a compatible ERDFlow release.");
        const bool shaped_connectors = number_version >= 2;
        const bool associative_entities = number_version >= 3;
        const bool inheritance = number_version >= 4;
        const bool inheritance_direction = number_version >= 5;
        const bool detachable_supertype = number_version >= 6;
        // Version 7 lets a connector pin where it meets each shape. Since the
        // reader refuses fields it does not expect, a release that predates
        // pinning declines the whole file rather than opening it and dropping
        // the pins on the next save, which is the better of the two.
        const bool pinned_connectors = number_version >= 7;
        // Version 8 lets a connector carry a route of its own. Earlier files
        // have at most the single bend, which is exactly the shape they had.
        const bool routed_connectors = number_version >= 8;
        // Version 9 lets an element carry a colour of its own. Earlier files
        // have none, and every element follows its theme, as they always did.
        const bool chosen_colours = number_version >= 9;
        // Version 10 lets one side of a relationship be drawn bare. Earlier
        // files draw both, which is what every one of them meant.
        const bool hidable_constraints = number_version >= 10;
        // Version 11 adds the pictures and notes placed on the canvas. Earlier
        // files have none, and could not have had.
        const bool figures = number_version >= 11;
        // Version 12 lets a surface be see-through by a percentage. Earlier
        // files' surfaces are solid, which is all they could be.
        const bool translucent = number_version >= 12;
        // Version 13 adds weak entities and identifying relationships. Earlier
        // files' entities are all regular and their relationships never
        // identifying, which is all they could say.
        const bool weak_entities = number_version >= 13;
        // Version 14 gives the diagram its paper. Earlier files are drawn on
        // the plain colour their theme gives the canvas, which is all they had.
        const bool papered = number_version >= 14;
        // Version 15 lets a note be a plain one. A note written by an earlier
        // version is a card, which is what those files meant.
        const bool plain_notes = number_version >= 15;
        const auto data = papered
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "transparency", "background"})
            : translucent
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "transparency"})
            : figures
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes"})
            : chosen_colours
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours"})
            : inheritance
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations"})
            : shaped_connectors
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors"})
            : object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout"});
        Project project;
        project.id = ProjectId{parse_id(data["id"])};
        project.name = string(data["name"]);
        for (const auto& value : array(data["entities"])) {
            auto o = weak_entities ? object(value, {"id", "name", "description", "weak"})
                                   : object(value, {"id", "name", "description"});
            Entity entity{EntityId{parse_id(o["id"])}, string(o["name"]), string(o["description"], max_description_bytes)};
            if (weak_entities) {
                if (!o["weak"].isBool()) invalid("An entity's weak flag must be true or false.");
                entity.weak = o["weak"].toBool();
            }
            if (!project.entities.emplace(entity.id, entity).second) invalid("Duplicate entity identifier.");
        }
        for (const auto& value : array(data["attributes"])) {
            auto o = object(value, {"id", "name", "description", "kind", "owner"});
            Attribute attribute{AttributeId{parse_id(o["id"])}, string(o["name"]), string(o["description"], max_description_bytes), parse_kind(o["kind"]), {}};
            if (!o["owner"].isNull()) attribute.owner = parse_ref(o["owner"]);
            if (!project.attributes.emplace(attribute.id, attribute).second) invalid("Duplicate attribute identifier.");
        }
        std::size_t participant_count = 0;
        for (const auto& value : array(data["relationships"])) {
            auto o = weak_entities
                ? object(value, {"id", "name", "description", "associative", "identifying", "participants"})
                : associative_entities
                ? object(value, {"id", "name", "description", "associative", "participants"})
                : object(value, {"id", "name", "description", "participants"});
            Relationship relationship{RelationshipId{parse_id(o["id"])}, string(o["name"]), string(o["description"], max_description_bytes), false, false, {}};
            if (associative_entities) {
                if (!o["associative"].isBool()) invalid("A relationship's associative flag must be true or false.");
                relationship.associative = o["associative"].toBool();
            }
            if (weak_entities) {
                if (!o["identifying"].isBool()) invalid("A relationship's identifying flag must be true or false.");
                relationship.identifying = o["identifying"].toBool();
            }
            for (const auto& part : array(o["participants"])) {
                if (++participant_count > max_elements) invalid("The project exceeds its participant limit.");
                auto p = associative_entities
                    ? (hidable_constraints
                        ? object(part, {"id", "target", "maximum", "participation", "role", "show_constraints"})
                        : object(part, {"id", "target", "maximum", "participation", "role"}))
                    : object(part, {"id", "entity", "maximum", "participation", "role"});
                const auto maximum = string(p["maximum"]);
                const auto participation = string(p["participation"]);
                if (maximum != "one" && maximum != "many") invalid("Invalid participant cardinality.");
                if (participation != "partial" && participation != "total") invalid("Invalid participation.");
                // Before version 3 a participant could only be an entity, and
                // was stored as a bare identifier rather than a typed reference.
                ParticipantTarget target = EntityId{};
                if (associative_entities) {
                    const auto element = parse_ref(p["target"]);
                    if (const auto* entity = std::get_if<EntityId>(&element)) target = *entity;
                    else if (const auto* onward = std::get_if<RelationshipId>(&element)) target = *onward;
                    else invalid("A participant must target an entity or an associative relationship.");
                } else {
                    target = EntityId{parse_id(p["entity"])};
                }
                // A file written before a side could be drawn bare draws both,
                // which is what every one of those diagrams meant.
                const auto shown = hidable_constraints ? p["show_constraints"] : QJsonValue(true);
                if (!shown.isBool()) invalid("A participant's show_constraints must be true or false.");
                relationship.participants.push_back({ParticipantId{parse_id(p["id"])}, target,
                    maximum == "one" ? Cardinality::One : Cardinality::Many,
                    participation == "total" ? Participation::Total : Participation::Partial, string(p["role"]),
                    shown.toBool()});
            }
            if (!project.relationships.emplace(relationship.id, relationship).second) invalid("Duplicate relationship identifier.");
        }
        for (const auto& value : array(data["layout"])) {
            const auto o = object(value, {"element", "x", "y", "width", "height"});
            if (!project.layout.emplace(parse_ref(o["element"]), Rect{number(o["x"]), number(o["y"]), number(o["width"]), number(o["height"])}).second)
                invalid("Duplicate element layout.");
        }
        if (inheritance) {
            for (const auto& value : array(data["specializations"])) {
                const auto o = inheritance_direction
                    ? object(value, {"id", "name", "description", "direction", "supertype", "subtypes", "constraint", "completeness"})
                    : object(value, {"id", "name", "description", "supertype", "subtypes", "constraint", "completeness"});
                Specialization specialization{SpecializationId{parse_id(o["id"])}, string(o["name"]),
                    string(o["description"], max_description_bytes), Inheritance::Specialization,
                    {}, {}, Disjointness::Disjoint, Completeness::Partial};
                if (!o["supertype"].isNull()) specialization.supertype = EntityId{parse_id(o["supertype"])};
                else if (!detachable_supertype) invalid("A specialization before version 6 must name its supertype.");
                if (inheritance_direction) {
                    const auto direction = string(o["direction"]);
                    if (direction != "generalization" && direction != "specialization") invalid("Invalid ISA direction.");
                    specialization.direction = direction == "generalization"
                        ? Inheritance::Generalization : Inheritance::Specialization;
                }
                for (const auto& subtype : array(o["subtypes"])) specialization.subtypes.push_back(EntityId{parse_id(subtype)});
                const auto constraint = string(o["constraint"]);
                const auto completeness = string(o["completeness"]);
                if (constraint != "disjoint" && constraint != "overlapping") invalid("Invalid specialization constraint.");
                if (completeness != "partial" && completeness != "total") invalid("Invalid specialization completeness.");
                specialization.constraint = constraint == "overlapping" ? Disjointness::Overlapping : Disjointness::Disjoint;
                specialization.completeness = completeness == "total" ? Completeness::Total : Completeness::Partial;
                if (!project.specializations.emplace(specialization.id, specialization).second)
                    invalid("Duplicate specialization identifier.");
            }
        }
        if (shaped_connectors) {
            for (const auto& value : array(data["connectors"])) {
                const auto o = routed_connectors
                    ? object(value, {"link", "offset", "owner_anchor", "child_anchor", "waypoints"})
                    : pinned_connectors
                    ? object(value, {"link", "offset", "owner_anchor", "child_anchor"})
                    : object(value, {"link", "offset"});
                domain::Connector shaped;
                shaped.offset = number(o["offset"]);
                // Files written before connectors could be pinned carry no
                // anchors, which is exactly the automatic join they were drawn with.
                if (pinned_connectors && !o["owner_anchor"].isNull()) shaped.owner_anchor = number(o["owner_anchor"]);
                if (pinned_connectors && !o["child_anchor"].isNull()) shaped.child_anchor = number(o["child_anchor"]);
                if (routed_connectors)
                    for (const auto& point : array(o["waypoints"])) {
                        const auto p = object(point, {"x", "y"});
                        shaped.waypoints.push_back(domain::Point{number(p["x"]), number(p["y"])});
                    }
                if (!project.connectors.emplace(parse_connector_reference(o["link"]), shaped).second)
                    invalid("Duplicate connector shape.");
            }
        }
        if (chosen_colours) {
            for (const auto& value : array(data["colours"])) {
                const auto o = object(value, {"element", "red", "green", "blue"});
                domain::Colour colour;
                // Each channel is one byte, so a value outside it is a broken
                // document rather than something to clamp into range quietly.
                for (const auto& [name, channel] : {std::pair{"red", &colour.red}, std::pair{"green", &colour.green},
                                                    std::pair{"blue", &colour.blue}}) {
                    const auto level = number(o[name]);
                    if (level < 0 || level > 255 || level != std::floor(level))
                        invalid("A colour channel must be a whole number between 0 and 255.");
                    *channel = static_cast<std::uint8_t>(level);
                }
                if (!project.colours.emplace(parse_ref(o["element"]), colour).second)
                    invalid("Duplicate element colour.");
            }
        }
        if (translucent) {
            for (const auto& value : array(data["transparency"])) {
                const auto o = object(value, {"element", "percent"});
                const auto percent = number(o["percent"]);
                // A percentage, whole, and never more than all of it.
                if (percent < 0 || percent > max_transparency || percent != std::floor(percent))
                    invalid("Transparency is a whole percentage from 0 to 100.");
                if (!project.transparency.emplace(parse_ref(o["element"]), static_cast<std::uint8_t>(percent)).second)
                    invalid("Duplicate element transparency.");
            }
        }
        if (figures) {
            for (const auto& value : array(data["pictures"])) {
                const auto o = object(value, {"id", "name", "description", "image"});
                Picture picture{PictureId{parse_id(o["id"])}, string(o["name"]),
                                string(o["description"], max_description_bytes), image_bytes(o["image"])};
                if (!project.pictures.emplace(picture.id, std::move(picture)).second) invalid("Duplicate picture identifier.");
            }
            for (const auto& value : array(data["notes"])) {
                const auto o = plain_notes ? object(value, {"id", "name", "description", "plain"})
                                           : object(value, {"id", "name", "description"});
                Note note{NoteId{parse_id(o["id"])}, string(o["name"]), string(o["description"], max_description_bytes)};
                if (plain_notes) {
                    if (!o["plain"].isBool()) invalid("A note's plain flag must be true or false.");
                    note.plain = o["plain"].toBool();
                }
                if (!project.notes.emplace(note.id, std::move(note)).second) invalid("Duplicate note identifier.");
            }
        }
        if (papered) {
            const auto o = object(data["background"], {"style", "strength", "image"});
            project.background.style = parse_background_style(o["style"]);
            const auto strength = number(o["strength"]);
            if (strength < 0 || strength > max_strength || strength != std::floor(strength))
                invalid("A background's strength is a whole percentage from 0 to 100.");
            project.background.strength = static_cast<std::uint8_t>(strength);
            project.background.image = image_bytes(o["image"]);
        }
        require_valid(project);
        return {std::move(project), {}};
    } catch (const std::exception& error) { return {{}, error.what()}; }
}

application::LoadResult ErdxProjectStore::load(const std::string& location) {
    QFile file(text(location));
    if (!file.open(QIODevice::ReadOnly)) return {{}, file.errorString().toStdString()};
    if (file.size() > max_file_bytes) return {{}, "The project exceeds the 8 MiB file limit."};
    const auto data = file.read(max_file_bytes + 1);
    if (file.error() != QFileDevice::NoError) return {{}, file.errorString().toStdString()};
    return decode(data);
}

application::SaveResult ErdxProjectStore::save(const std::string& location, const Project& project) {
    try {
        const auto data = encode(project);
        QSaveFile file(text(location));
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)) return {false, file.errorString().toStdString()};
        if (file.write(data) != data.size()) { file.cancelWriting(); return {false, file.errorString().toStdString()}; }
        if (!file.commit()) return {false, file.errorString().toStdString()};
        return {true, {}};
    } catch (const std::exception& error) { return {false, error.what()}; }
}

} // namespace erdflow::infrastructure
