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
// and participants that may target one; version 4 adds specializations.
// Earlier versions remain readable; the format specification states the
// compatibility rule for each.
constexpr int current_format_version = 4;
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
        : std::holds_alternative<SpecializationId>(ref) ? "specialization" : "relationship";
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
    invalid("Unsupported element type.");
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
    QJsonArray entities, attributes, relationships, layout;
    for (const auto& [id, entity] : project.entities)
        entities.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(entity.name)}, {"description", text(entity.description)}});
    for (const auto& [id, attribute] : project.attributes)
        attributes.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(attribute.name)},
            {"description", text(attribute.description)}, {"kind", kind_name(attribute.kind)},
            {"owner", attribute.owner ? QJsonValue(reference(*attribute.owner)) : QJsonValue(QJsonValue::Null)}});
    for (const auto& [id, relationship] : project.relationships) {
        QJsonArray participants;
        for (const auto& p : relationship.participants)
            participants.append(QJsonObject{{"id", uuid_text(p.id.value)}, {"target", reference(target_ref(p.target))},
                {"maximum", p.maximum == Cardinality::One ? "one" : "many"},
                {"participation", p.participation == Participation::Total ? "total" : "partial"}, {"role", text(p.role)}});
        relationships.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(relationship.name)},
            {"description", text(relationship.description)}, {"associative", relationship.associative},
            {"participants", participants}});
    }
    for (const auto& [ref, rect] : project.layout)
        layout.append(QJsonObject{{"element", reference(ref)}, {"x", rect.x}, {"y", rect.y}, {"width", rect.width}, {"height", rect.height}});
    QJsonArray specializations;
    for (const auto& [id, specialization] : project.specializations) {
        QJsonArray subtypes;
        for (const auto& subtype : specialization.subtypes) subtypes.append(uuid_text(subtype.value));
        specializations.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(specialization.name)},
            {"description", text(specialization.description)}, {"supertype", uuid_text(specialization.supertype.value)},
            {"subtypes", subtypes},
            {"constraint", specialization.constraint == Disjointness::Overlapping ? "overlapping" : "disjoint"},
            {"completeness", specialization.completeness == Completeness::Total ? "total" : "partial"}});
    }
    QJsonArray connectors;
    for (const auto& [ref, offset] : project.connectors)
        connectors.append(QJsonObject{{"link", connector_reference(ref)}, {"offset", offset}});
    auto bytes = QJsonDocument(QJsonObject{{"format", "erdflow"}, {"format_version", current_format_version},
        {"project", QJsonObject{{"id", uuid_text(project.id.value)}, {"name", text(project.name)},
        {"entities", entities}, {"attributes", attributes}, {"relationships", relationships},
        {"layout", layout}, {"connectors", connectors},
        {"specializations", specializations}}}}).toJson(QJsonDocument::Indented);
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
        if (number_version < 1 || number_version > 4 || number_version != std::floor(number_version))
            invalid("Unsupported project version. Use a compatible ERDFlow release.");
        const bool shaped_connectors = number_version >= 2;
        const bool associative_entities = number_version >= 3;
        const bool inheritance = number_version >= 4;
        const auto data = inheritance
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations"})
            : shaped_connectors
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors"})
            : object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout"});
        Project project;
        project.id = ProjectId{parse_id(data["id"])};
        project.name = string(data["name"]);
        for (const auto& value : array(data["entities"])) {
            auto o = object(value, {"id", "name", "description"});
            Entity entity{EntityId{parse_id(o["id"])}, string(o["name"]), string(o["description"], max_description_bytes)};
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
            auto o = associative_entities
                ? object(value, {"id", "name", "description", "associative", "participants"})
                : object(value, {"id", "name", "description", "participants"});
            Relationship relationship{RelationshipId{parse_id(o["id"])}, string(o["name"]), string(o["description"], max_description_bytes), false, {}};
            if (associative_entities) {
                if (!o["associative"].isBool()) invalid("A relationship's associative flag must be true or false.");
                relationship.associative = o["associative"].toBool();
            }
            for (const auto& part : array(o["participants"])) {
                if (++participant_count > max_elements) invalid("The project exceeds its participant limit.");
                auto p = associative_entities
                    ? object(part, {"id", "target", "maximum", "participation", "role"})
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
                relationship.participants.push_back({ParticipantId{parse_id(p["id"])}, target,
                    maximum == "one" ? Cardinality::One : Cardinality::Many,
                    participation == "total" ? Participation::Total : Participation::Partial, string(p["role"])});
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
                const auto o = object(value, {"id", "name", "description", "supertype", "subtypes", "constraint", "completeness"});
                Specialization specialization{SpecializationId{parse_id(o["id"])}, string(o["name"]),
                    string(o["description"], max_description_bytes), EntityId{parse_id(o["supertype"])}, {},
                    Disjointness::Disjoint, Completeness::Partial};
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
                const auto o = object(value, {"link", "offset"});
                if (!project.connectors.emplace(parse_connector_reference(o["link"]), number(o["offset"])).second)
                    invalid("Duplicate connector shape.");
            }
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
