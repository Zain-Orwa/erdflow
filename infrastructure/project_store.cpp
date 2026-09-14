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
    const char* type = std::holds_alternative<EntityId>(ref) ? "entity" : std::holds_alternative<AttributeId>(ref) ? "attribute" : "relationship";
    return {{"type", QLatin1String(type)}, {"id", uuid_text(uuid(ref))}};
}
ElementRef parse_ref(const QJsonValue& value) {
    auto o = object(value, {"type", "id"});
    auto id = parse_id(o["id"]);
    const auto type = string(o["type"]);
    if (type == "entity") return EntityId{id};
    if (type == "attribute") return AttributeId{id};
    if (type == "relationship") return RelationshipId{id};
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
void check_structure(const QByteArray& input) {
    struct Scope {
        char opener;
        std::set<QString> keys;
    };
    std::vector<Scope> scopes;
    scopes.reserve(32);
    auto whitespace = [](char ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'; };
    for (qsizetype i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        if (ch == '{' || ch == '[') {
            if (scopes.size() == 32) invalid("Project nesting exceeds the supported limit.");
            scopes.push_back({ch, {}});
        } else if (ch == '}' || ch == ']') {
            if (scopes.empty() || scopes.back().opener != (ch == '}' ? '{' : '['))
                invalid("Invalid project JSON structure.");
            scopes.pop_back();
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
            if (scopes.empty() || scopes.back().opener != '{') invalid("Invalid project JSON field.");
            // QJson normalizes duplicate object keys. Inspect keys first, using
            // Qt's parser only to unescape each small key token; the full parse
            // below remains authoritative for JSON syntax and value parsing.
            QJsonParseError error;
            const auto token = QByteArray("[") + input.mid(start, i - start + 1) + ']';
            const auto decoded = QJsonDocument::fromJson(token, &error);
            if (error.error != QJsonParseError::NoError || !decoded.isArray()) invalid("Invalid project JSON field name.");
            const auto key = decoded.array().first().toString();
            if (!key.isValidUtf16()) invalid("A project field name contains an unpaired Unicode surrogate.");
            if (!scopes.back().keys.insert(key).second) invalid("Duplicate project JSON field.");
            // Current objects have at most six fields. This conservative cap
            // bounds preflight bookkeeping for hostile objects before parsing.
            if (scopes.back().keys.size() > 16) invalid("Unsupported project fields.");
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
            participants.append(QJsonObject{{"id", uuid_text(p.id.value)}, {"entity", uuid_text(p.entity.value)},
                {"maximum", p.maximum == Cardinality::One ? "one" : "many"},
                {"participation", p.participation == Participation::Total ? "total" : "partial"}, {"role", text(p.role)}});
        relationships.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(relationship.name)},
            {"description", text(relationship.description)}, {"participants", participants}});
    }
    for (const auto& [ref, rect] : project.layout)
        layout.append(QJsonObject{{"element", reference(ref)}, {"x", rect.x}, {"y", rect.y}, {"width", rect.width}, {"height", rect.height}});
    auto bytes = QJsonDocument(QJsonObject{{"format", "erdflow"}, {"format_version", 1},
        {"project", QJsonObject{{"id", uuid_text(project.id.value)}, {"name", text(project.name)},
        {"entities", entities}, {"attributes", attributes}, {"relationships", relationships}, {"layout", layout}}}}).toJson(QJsonDocument::Indented);
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
        if (!root["format_version"].isDouble() || root["format_version"].toDouble() != 1) invalid("Unsupported project version. Use a compatible ERDFlow release.");
        const auto data = object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout"});
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
            auto o = object(value, {"id", "name", "description", "participants"});
            Relationship relationship{RelationshipId{parse_id(o["id"])}, string(o["name"]), string(o["description"], max_description_bytes), {}};
            for (const auto& part : array(o["participants"])) {
                if (++participant_count > max_elements) invalid("The project exceeds its participant limit.");
                auto p = object(part, {"id", "entity", "maximum", "participation", "role"});
                const auto maximum = string(p["maximum"]);
                const auto participation = string(p["participation"]);
                if (maximum != "one" && maximum != "many") invalid("Invalid participant cardinality.");
                if (participation != "partial" && participation != "total") invalid("Invalid participation.");
                relationship.participants.push_back({ParticipantId{parse_id(p["id"])}, EntityId{parse_id(p["entity"])},
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
