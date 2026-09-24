// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "domain/schema_preview.hpp"
#include "infrastructure/project_store.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <functional>
#include <map>
#include <iostream>
#include <set>
#include <stdexcept>

using namespace erdflow::domain;
using namespace erdflow::application;
using namespace erdflow::infrastructure;

namespace {
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string(__func__) + ":" + std::to_string(__LINE__) + ": " #condition); } while (false)

struct Fixture {
    QtIdGenerator ids;
    Editor editor{ids};
    EntityId employee;
    RelationshipId supervises;
    AttributeId address;
    SpecializationId specialisation;
    Fixture() {
        const auto ent = editor.create_entity("Employee 学生", {-120.25, 20.5, 160, 80});
        CHECK(ent);
        employee = std::get<EntityId>(*ent.created);
        CHECK(editor.describe(employee, "First line\nSecond\tline: \"quoted\" and \\backslash"));
        const auto rel = editor.create_relationship("Supervises", {80, 220, 150, 100});
        CHECK(rel);
        supervises = std::get<RelationshipId>(*rel.created);
        const auto p1 = editor.connect(supervises, employee);
        const auto p2 = editor.connect(supervises, employee);
        CHECK(p1 && p2);
        CHECK(editor.update_participant(supervises, *p1.participant, Cardinality::One, Participation::Partial, "Supervisor"));
        CHECK(editor.update_participant(supervises, *p2.participant, Cardinality::Many, Participation::Total, "Report"));
        const auto attr = editor.create_attribute("Address", {}, ElementRef{employee});
        CHECK(attr);
        address = std::get<AttributeId>(*attr.created);
        CHECK(editor.set_attribute_kind(address, AttributeKind::Composite));
        CHECK(editor.create_attribute("Street", {}, ElementRef{address}));
        const auto key = editor.create_attribute("Employee number", {}, ElementRef{employee});
        CHECK(key);
        CHECK(editor.set_attribute_kind(std::get<AttributeId>(*key.created), AttributeKind::Key));
        const auto date = editor.create_attribute("Since", {}, ElementRef{supervises});
        CHECK(date);
        CHECK(editor.set_attribute_kind(std::get<AttributeId>(*date.created), AttributeKind::Derived));
        const auto phone = editor.create_attribute("Phone", {}, ElementRef{employee});
        CHECK(phone);
        CHECK(editor.set_attribute_kind(std::get<AttributeId>(*phone.created), AttributeKind::Multivalued));
        // A specialization in the fixture gives the cross-version checks teeth;
        // without one, stripping its fields from a document changes nothing.
        const auto manager = editor.create_entity("Manager", {320, 420, 160, 80});
        CHECK(manager);
        const auto isa = editor.create_specialization("IS A", {120, 360, 96, 74}, Inheritance::Generalization);
        CHECK(isa);
        specialisation = std::get<SpecializationId>(*isa.created);
        CHECK(editor.set_supertype(specialisation, employee));
        CHECK(editor.attach_subtype(specialisation, std::get<EntityId>(*manager.created)));
        CHECK(editor.set_specialization_rules(specialisation, Disjointness::Overlapping, Completeness::Total));
        CHECK(editor.rename_project("University design"));
    }
    QJsonObject document() const { return QJsonDocument::fromJson(ErdxProjectStore::encode(editor.project())).object(); }
};

QByteArray bytes(const QJsonObject& object) { return QJsonDocument(object).toJson(QJsonDocument::Compact); }
void reject(const QByteArray& input) {
    const auto result = ErdxProjectStore::decode(input);
    CHECK(!result);
    CHECK(!result.error.empty());
}
// Several unrelated rules can refuse the same document, so cases that pin one
// specific rule assert the reported reason instead of only the refusal.
void reject_because(const QByteArray& input, const std::string& reason) {
    const auto result = ErdxProjectStore::decode(input);
    CHECK(!result);
    CHECK(result.error.find(reason) != std::string::npos);
}
// A document claiming to predate version 19 must look like one throughout: it
// knew nothing of which relationship sides had been answered.
void forget_answered_sides(QJsonObject& project) {
    QJsonArray relationships;
    for (const auto& value : project["relationships"].toArray()) {
        auto entry = value.toObject();
        QJsonArray sides;
        for (const auto& side : entry["participants"].toArray()) {
            auto one = side.toObject();
            one.remove("cardinality_confirmed");
            one.remove("participation_confirmed");
            sides.append(one);
        }
        entry["participants"] = sides;
        relationships.append(entry);
    }
    project["relationships"] = relationships;
}

// A document claiming to predate version 22 must look like one throughout: it
// recorded nothing about how the schema had been arranged, because it could
// not yet be arranged by hand.
void forget_schema_layout(QJsonObject& project) {
    project.remove("schema_layout");
}
// Version 27 is the first format that stores the prose entered with the other
// project-creation details. A fixture taken back to any earlier version must
// therefore lose the field as well as claim the earlier version.
void forget_project_description(QJsonObject& project) {
    project.remove("description");
}
// Version 28 is the first to record which bridges were chosen to be keyed by
// their participants' foreign keys, so an earlier one carries no such list.
void forget_bridge_keys(QJsonObject& project) {
    if (!project.contains("decisions")) return;
    auto decided = project["decisions"].toObject();
    decided.remove("bridge_key");
    project["decisions"] = decided;
}
// A document claiming to predate version 26 keyed the schema's own state by
// the conceptual element each object came from, rather than by a relational
// identity of its own.
//
// Turning a current document back into one means undoing a derivation, which
// cannot be done from the identity alone -- so it is done from the project:
// every element is asked what identity it would derive, and that answer is
// looked up backwards. This is exactly the mapping the reader performs
// forwards when it migrates such a file.
void forget_relational(QJsonObject& project) {
    std::map<QString, QJsonObject> whose;
    const auto note = [&](const char* type, const QJsonArray& of) {
        for (const auto& value : of) {
            const auto id = value.toObject()["id"].toString();
            auto bytes = Uuid{};
            auto text = id;
            text.remove('-');
            for (std::size_t i = 0; i < bytes.bytes.size(); ++i)
                bytes.bytes[i] = static_cast<std::uint8_t>(
                    text.mid(static_cast<int>(i) * 2, 2).toUInt(nullptr, 16));
            ElementRef ref = EntityId{bytes};
            if (std::string(type) == "attribute") ref = AttributeId{bytes};
            else if (std::string(type) == "relationship") ref = RelationshipId{bytes};
            const auto derived = relation_from(ref);
            QString made;
            for (const auto byte : derived.value.bytes)
                made += QString("%1").arg(byte, 2, 16, QChar('0'));
            made.insert(20, '-'); made.insert(16, '-'); made.insert(12, '-'); made.insert(8, '-');
            whose[made] = QJsonObject{{"type", QLatin1String(type)}, {"id", id}};
        }
    };
    note("entity", project["entities"].toArray());
    note("attribute", project["attributes"].toArray());
    note("relationship", project["relationships"].toArray());
    // Every list that used to name an element instead of a relation.
    const auto back = [&](const QJsonArray& of) {
        QJsonArray older;
        for (const auto& value : of) {
            auto entry = value.toObject();
            const auto found = whose.find(entry["relation"].toString());
            if (found == whose.end()) continue;
            entry.remove("relation");
            entry["element"] = found->second;
            older.append(entry);
        }
        return older;
    };
    // Each list only where the document still has it: a document already taken
    // back past the version that introduced one must not be handed it again.
    if (project.contains("decisions")) {
        auto decided = project["decisions"].toObject();
        if (decided.contains("table_name")) decided["table_name"] = back(decided["table_name"].toArray());
        project["decisions"] = decided;
    }
    if (project.contains("schema")) {
        auto schema = project["schema"].toObject();
        if (schema.contains("added")) schema["added"] = back(schema["added"].toArray());
        if (schema.contains("keys")) schema["keys"] = back(schema["keys"].toArray());
        // Only where the document still has them. A document already taken
        // back past version 25 has no such list, and giving it an empty one
        // would be handing it a field of a version it is claiming to predate.
        if (schema.contains("counting_keys")) {
            QJsonArray counting;
            for (const auto& value : schema["counting_keys"].toArray()) {
                const auto found = whose.find(value.toString());
                if (found != whose.end()) counting.append(found->second);
            }
            schema["counting_keys"] = counting;
        }
        project["schema"] = schema;
    }
    if (!project.contains("schema_layout")) return;
    auto layout = project["schema_layout"].toObject();
    layout["tables"] = back(layout["tables"].toArray());
    // A line before 26 named the link it was drawn for; that cannot be worked
    // back out of the key, so such a document simply has none.
    layout["lines"] = QJsonArray{};
    project["schema_layout"] = layout;
}

// A document claiming to predate version 25 knew nothing of a column counting
// itself up, so it says so nowhere -- neither on an attribute nor on a column
// the schema added on its own.
void forget_auto_increment(QJsonObject& project) {
    QJsonArray kept;
    for (const auto& value : project.value("attributes").toArray()) {
        auto entry = value.toObject();
        entry.remove("auto_increment");
        kept.append(entry);
    }
    if (!kept.isEmpty()) project["attributes"] = kept;
    if (!project.contains("schema")) return;
    auto schema = project["schema"].toObject();
    QJsonArray tables;
    for (const auto& value : schema.value("added").toArray()) {
        auto table = value.toObject();
        QJsonArray columns;
        for (const auto& one : table.value("columns").toArray()) {
            auto column = one.toObject();
            column.remove("auto_increment");
            columns.append(column);
        }
        table["columns"] = columns;
        tables.append(table);
    }
    schema["added"] = tables;
    // Nor did it know a key the conversion invented could count itself up.
    schema.remove("counting_keys");
    project["schema"] = schema;
}
// And one claiming to predate version 21 recorded nothing about the schema
// differing from the diagram, because it could not yet be edited away from it.
void forget_schema_edits(QJsonObject& project) {
    project.remove("schema");
}
// A document claiming to predate version 20 must look like one throughout: it
// recorded no conversion decisions, gave no column a scale, and named its
// types from the small portable set rather than from the SQL catalogue.
void forget_decisions(QJsonObject& project) {
    project.remove("decisions");
    static const std::map<QString, QString> portable{
        {"varchar", "text"}, {"int", "integer"}, {"decimal", "decimal"}, {"bit", "boolean"},
        {"date", "date"}, {"datetime", "datetime"}, {"varbinary", "binary"},
        {"uniqueidentifier", "uuid"}, {"unset", "unset"}};
    QJsonArray kept;
    for (const auto& value : project.value("attributes").toArray()) {
        auto entry = value.toObject();
        entry.remove("scale");
        // value() rather than operator[]: the non-const subscript inserts the
        // key it is asked for, which would put back the very field a document
        // of this age must not have.
        if (entry.contains("type")) {
            const auto found = portable.find(entry.value("type").toString());
            if (found != portable.end()) entry["type"] = found->second;
        }
        kept.append(entry);
    }
    project["attributes"] = kept;
}

void change_project(QJsonObject& root, const std::function<void(QJsonObject&)>& change) {
    auto project = root["project"].toObject();
    change(project);
    root["project"] = project;
}
void change_first(QJsonObject& root, const char* array_name, const std::function<void(QJsonObject&)>& change) {
    change_project(root, [&](QJsonObject& project) {
        auto list = project[array_name].toArray();
        auto item = list.first().toObject();
        change(item);
        list[0] = item;
        project[array_name] = list;
    });
}
QByteArray read_file(const QString& path) {
    QFile file(path);
    CHECK(file.open(QIODevice::ReadOnly));
    return file.readAll();
}
void write_file(const QString& path, const QByteArray& content) {
    QFile file(path);
    CHECK(file.open(QIODevice::WriteOnly));
    CHECK(file.write(content) == content.size());
}

void uuid_generation_and_roundtrip() {
    QtIdGenerator generator;
    std::set<Uuid> seen;
    for (std::size_t i = 0; i < 1000; ++i) {
        const auto id = generator.next();
        CHECK(id.valid());
        CHECK(seen.insert(id).second);
        const auto text = uuid_text(id);
        CHECK(text.size() == 36);
        CHECK(text == text.toLower());
        CHECK(text[14] == QLatin1Char('7'));
        CHECK(!text.contains(QLatin1Char('{')));
    }
    Fixture fixture;
    const auto before = fixture.editor.project();
    const auto encoded = ErdxProjectStore::encode(before);
    const auto result = ErdxProjectStore::decode(encoded);
    CHECK(result);
    CHECK(result.error.empty());
    CHECK(*result.project == before);
    CHECK(ErdxProjectStore::encode(*result.project) == encoded);
    CHECK(fixture.editor.duplicate({ElementRef{fixture.employee}, ElementRef{fixture.supervises}}));
    const auto copied = fixture.editor.project();
    const auto restored = ErdxProjectStore::decode(ErdxProjectStore::encode(copied));
    CHECK(restored);
    CHECK(*restored.project == copied);
    CHECK(copied.entities.size() == 3);
    CHECK(copied.relationships.size() == 2);
    std::set<ParticipantId> participants;
    for (const auto& [id, relationship] : copied.relationships) {
        (void)id;
        for (const auto& participant : relationship.participants) CHECK(participants.insert(participant.id).second);
    }
    CHECK(participants.size() == 4);
}

void incomplete_models_save_and_open() {
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    QtIdGenerator ids;
    Editor editor(ids);
    CHECK(editor.create_entity("", {}));
    CHECK(editor.create_attribute("", {}));
    CHECK(editor.create_relationship("", {}));
    const auto path = temporary.filePath(QString::fromUtf8("draft 学生.erdx"));
    ErdxProjectStore store;
    CHECK(editor.dirty());
    CHECK(save_project(editor, store, path.toStdString()));
    CHECK(!editor.dirty());
    const auto draft = editor.project();
    editor.new_project();
    CHECK(open_project(editor, store, path.toStdString()));
    CHECK(editor.project() == draft);
    CHECK(!editor.dirty());
    CHECK(!editor.can_undo());
    CHECK(!editor.can_redo());
}

void project_description_persists_across_the_version_boundary() {
    Fixture fixture;
    const std::string description = "A shared model of university staffing.\nReviewed each semester.";
    CHECK(fixture.editor.describe_project(description));

    const auto encoded = ErdxProjectStore::encode(fixture.editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    CHECK(root["format_version"].toInt() == 28);
    CHECK(root["project"].toObject()["description"].toString().toStdString() == description);
    const auto reopened = ErdxProjectStore::decode(encoded);
    CHECK(reopened);
    CHECK(reopened.project->description == description);
    CHECK(*reopened.project == fixture.editor.project());

    // The field is required in the version that introduced it, and an older
    // version may not smuggle it in where an old writer could not understand
    // or preserve it.
    auto missing = root;
    change_project(missing, [](QJsonObject& project) { project.remove("description"); });
    reject(bytes(missing));
    auto older = root;
    auto older_project = older["project"].toObject();
    forget_bridge_keys(older_project);
    forget_project_description(older_project);
    older["project"] = older_project;
    older["format_version"] = 26;
    const auto from_older = ErdxProjectStore::decode(bytes(older));
    CHECK(from_older);
    CHECK(from_older.project->description.empty());
    CHECK(QJsonDocument::fromJson(ErdxProjectStore::encode(*from_older.project))
              .object()["format_version"].toInt() == 28);
    change_project(older, [&](QJsonObject& project) { project["description"] = QString::fromStdString(description); });
    reject(bytes(older));

    auto wrong_type = root;
    change_project(wrong_type, [](QJsonObject& project) { project["description"] = true; });
    reject(bytes(wrong_type));
    auto too_long = root;
    change_project(too_long, [](QJsonObject& project) {
        project["description"] = QString(static_cast<qsizetype>(max_description_bytes + 1), QLatin1Char('x'));
    });
    reject(bytes(too_long));
}

void malformed_json_and_text() {
    for (const auto& input : {QByteArray{}, QByteArray{"{"}, QByteArray{"[]"}, QByteArray{"null"}, QByteArray{"{\"format\":true}"}}) reject(input);
    Fixture fixture;
    for (const auto& name : {QJsonValue(true), QJsonValue(42), QJsonValue(QJsonArray{}), QJsonValue(QJsonValue::Null)}) {
        auto root = fixture.document();
        change_project(root, [&](QJsonObject& project) { project["name"] = name; });
        reject(bytes(root));
    }
    auto root = fixture.document();
    change_project(root, [](QJsonObject& project) { project["name"] = QString(513, QLatin1Char('x')); });
    reject(bytes(root));
    root = fixture.document();
    change_first(root, "entities", [](QJsonObject& entity) { entity["description"] = QString(16385, QLatin1Char('x')); });
    reject(bytes(root));
    root = fixture.document();
    change_project(root, [](QJsonObject& project) { project["name"] = QString(QChar(0)); });
    reject(bytes(root));
    root = fixture.document();
    change_project(root, [](QJsonObject& project) { project["name"] = QString::fromUtf8("a\nb"); });
    reject(bytes(root));
    auto raw = bytes(fixture.document());
    raw.replace("University design", "\xc0\x80");
    reject(raw);
    for (const auto& surrogate : {QByteArray{"\\ud800"}, QByteArray{"\\udc00"}}) {
        raw = bytes(fixture.document());
        raw.replace("University design", surrogate);
        reject(raw);
    }
    raw = bytes(fixture.document());
    raw.replace("University design", "\\ud83d\\udcda");
    const auto valid_pair = ErdxProjectStore::decode(raw);
    CHECK(valid_pair);
    CHECK(valid_pair.project->name == "📚");
}

void strict_version_and_field_contract() {
    Fixture fixture;
    for (const auto& version : {QJsonValue(0), QJsonValue(29), QJsonValue(1.5), QJsonValue("1"), QJsonValue(true)}) {
        auto root = fixture.document();
        root["format_version"] = version;
        reject(bytes(root));
    }
    auto root = fixture.document();
    root["format"] = "other";
    reject(bytes(root));
    for (const auto* key : {"format", "format_version", "project"}) {
        root = fixture.document();
        root.remove(QLatin1String(key));
        reject(bytes(root));
    }
    root = fixture.document();
    root["future_extension"] = "must not discard";
    reject(bytes(root));
    root = fixture.document();
    change_project(root, [](QJsonObject& project) { project["pages"] = QJsonArray{}; });
    reject(bytes(root));
    for (const auto* array : {"entities", "attributes", "relationships", "layout"}) {
        root = fixture.document();
        change_first(root, array, [](QJsonObject& item) { item["future_extension"] = 1; });
        reject(bytes(root));
        root = fixture.document();
        change_first(root, array, [](QJsonObject& item) { item.remove(item.constBegin().key()); });
        reject(bytes(root));
    }
    root = fixture.document();
    change_first(root, "attributes", [](QJsonObject& item) {
        auto owner = item["owner"].toObject(); owner["future_extension"] = 1; item["owner"] = owner;
    });
    reject(bytes(root));
    root = fixture.document();
    change_first(root, "relationships", [](QJsonObject& item) {
        auto list = item["participants"].toArray(); auto p = list[0].toObject(); p["future_extension"] = 1; list[0] = p; item["participants"] = list;
    });
    reject(bytes(root));
    // Derive the version text and assert it was found: hard-coding it let these
    // cases silently pass against an unmodified document after a version bump.
    const auto version_text = "\"format_version\":"
        + QByteArray::number(fixture.document()["format_version"].toInt());
    auto duplicate = bytes(fixture.document());
    CHECK(duplicate.contains(version_text));
    duplicate.replace(version_text, "\"format_version\":99," + version_text);
    reject(duplicate);
    duplicate = bytes(fixture.document());
    CHECK(duplicate.contains("\"name\":\"University design\""));
    duplicate.replace("\"name\":\"University design\"", "\"name\":\"discarded\",\"name\":\"University design\"");
    reject(duplicate);
    // Escaped property names are the same key after JSON unescaping.
    duplicate = bytes(fixture.document());
    duplicate.replace(version_text, version_text + ",\"format_\\u0076ersion\":1");
    reject(duplicate);
}

// Field names are unescaped by the loader itself rather than by a parser
// invocation per key. These cases pin that decoder's agreement with JSON.
void escaped_field_names() {
    Fixture fixture;
    // Every simple escape decodes to the documented character, so an escaped
    // spelling of a supported name stays a duplicate of its plain spelling.
    for (const auto& escaped : {QByteArray("\\u0066ormat"), QByteArray("\\u0066\\u006frmat")}) {
        auto duplicate = bytes(fixture.document());
        duplicate.replace("\"format\":", "\"" + escaped + "\":\"erdflow\",\"format\":");
        reject(duplicate);
    }
    // A surrogate pair in a field name is well-formed text, so it must be
    // refused for being an unsupported field rather than for being malformed.
    auto raw = bytes(fixture.document());
    raw.replace("\"format\":", "\"\\ud83d\\udcda\":1,\"format\":");
    reject_because(raw, "unsupported project fields");
    // Unpaired surrogates are a decoding fault, reported as such.
    for (const auto& broken : {QByteArray("\\ud800"), QByteArray("\\udc00"), QByteArray("\\ud83d\\u0061")}) {
        raw = bytes(fixture.document());
        raw.replace("\"format\":", "\"" + broken + "\":1,\"format\":");
        reject_because(raw, "unpaired Unicode surrogate");
    }
    // Truncated, non-hexadecimal and unknown escapes are decoding faults. A
    // dangling escape cannot reach the decoder: it would consume the closing
    // quote, so the scanner reports an unterminated string instead.
    for (const auto& broken : {QByteArray("\\u00"), QByteArray("\\uzzzz"), QByteArray("\\q")}) {
        raw = bytes(fixture.document());
        raw.replace("\"format\":", "\"" + broken + "\":1,\"format\":");
        reject_because(raw, "Invalid project JSON field name");
    }
    // A valid document must still load once escaped names are handled here.
    CHECK(ErdxProjectStore::decode(bytes(fixture.document())));
}

// Version 2 adds connector shapes. Version 1 files must still open, and the
// shapes a user gives connectors must survive a full save/open cycle.
void connector_shapes_persist_and_older_versions_still_open() {
    Fixture fixture;
    const auto side = fixture.editor.project().relationships.at(fixture.supervises).participants.front().id;
    CHECK(fixture.editor.bend_connector(ConnectorRef{side}, 37.5));
    CHECK(fixture.editor.bend_connector(ConnectorRef{fixture.address}, -12.25));

    const auto encoded = ErdxProjectStore::encode(fixture.editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    CHECK(root["format_version"].toInt() == 28);
    CHECK(root["project"].toObject()["connectors"].toArray().size() == 2);

    // A pinned join survives the same round trip.
    CHECK(fixture.editor.pin_connector(ConnectorRef{fixture.address}, 1.25, -0.5));
    const auto pinned = ErdxProjectStore::encode(fixture.editor.project());
    const auto reloaded = ErdxProjectStore::decode(pinned);
    CHECK(reloaded);
    CHECK(reloaded.project->connectors == fixture.editor.project().connectors);
    const auto& kept = reloaded.project->connectors.at(ConnectorRef{fixture.address});
    CHECK(kept.owner_anchor && *kept.owner_anchor == 1.25);
    CHECK(kept.child_anchor && *kept.child_anchor == -0.5);
    CHECK(kept.offset == -12.25);
    // Every connector carries the same field set whether or not it is pinned,
    // since the reader refuses an object with keys it does not expect.
    for (const auto& value : QJsonDocument::fromJson(pinned).object()["project"].toObject()["connectors"].toArray()) {
        const auto connector = value.toObject();
        CHECK(connector.size() == 5);
        CHECK(connector.contains("owner_anchor") && connector.contains("child_anchor"));
        CHECK(connector.contains("waypoints"));
    }
    CHECK(fixture.editor.pin_connector(ConnectorRef{fixture.address}, std::nullopt, std::nullopt));

    // A side drawn bare survives the round trip, and the constraints it still
    // holds come back untouched.
    const auto bare_side = fixture.editor.project().relationships.at(fixture.supervises).participants.front().id;
    CHECK(fixture.editor.show_participant_constraints(fixture.supervises, bare_side, false));
    const auto bare = ErdxProjectStore::decode(ErdxProjectStore::encode(fixture.editor.project()));
    CHECK(bare);
    const auto& reopened_side = bare.project->relationships.at(fixture.supervises).participants.front();
    CHECK(!reopened_side.show_constraints);
    CHECK(reopened_side.maximum == fixture.editor.project().relationships.at(fixture.supervises).participants.front().maximum);
    CHECK(fixture.editor.show_participant_constraints(fixture.supervises, bare_side, true));

    // An element's chosen colour survives the round trip.
    const Colour coral{0xFF, 0xA8, 0xA8};
    CHECK(fixture.editor.recolour({ElementRef{fixture.employee}}, coral));
    const auto coloured = ErdxProjectStore::decode(ErdxProjectStore::encode(fixture.editor.project()));
    CHECK(coloured);
    CHECK(coloured.project->colours.at(ElementRef{fixture.employee}) == coral);
    CHECK(*coloured.project == fixture.editor.project());
    CHECK(fixture.editor.recolour({ElementRef{fixture.employee}}, std::nullopt));

    // A route survives the same round trip, in order.
    const std::vector<Point> route{Point{12.5, -30.0}, Point{44.0, 61.5}};
    CHECK(fixture.editor.route_connector(ConnectorRef{side}, route));
    const auto routed = ErdxProjectStore::decode(ErdxProjectStore::encode(fixture.editor.project()));
    CHECK(routed);
    const auto& shape = routed.project->connectors.at(ConnectorRef{side});
    CHECK(shape.waypoints.size() == 2);
    CHECK(shape.waypoints == route);
    // A route supersedes the single bend rather than sitting alongside it.
    CHECK(shape.offset == 0);
    CHECK(fixture.editor.route_connector(ConnectorRef{side}, std::vector<Point>{}));
    CHECK(fixture.editor.bend_connector(ConnectorRef{side}, 37.5));

    const auto reopened = ErdxProjectStore::decode(encoded);
    CHECK(reopened);
    CHECK(reopened.project->specializations == fixture.editor.project().specializations);
    CHECK(reopened.project->specializations.at(fixture.specialisation).direction == Inheritance::Generalization);
    CHECK(reopened.project->connectors == fixture.editor.project().connectors);
    CHECK(*reopened.project == fixture.editor.project());

    // Rewrite the current document as an older version: before version 3 a
    // relationship had no associative flag and a participant stored a bare
    // entity identifier, and before version 2 there were no connector shapes.
    const auto downgrade = [&](int version) {
        auto document = fixture.document();
        auto project = document["project"].toObject();
        QJsonArray relationships;
        for (const auto& value : project["relationships"].toArray()) {
            auto relationship = value.toObject();
            if (version >= 3) { relationships.append(relationship); continue; }
            relationship.remove("associative");
            QJsonArray participants;
            for (const auto& item : relationship["participants"].toArray()) {
                auto participant = item.toObject();
                participant["entity"] = participant["target"].toObject()["id"];
                participant.remove("target");
                participants.append(participant);
            }
            relationship["participants"] = participants;
            relationships.append(relationship);
        }
        project["relationships"] = relationships;
        if (version < 6) {
            // Before version 6 a specialization always named its supertype, so
            // any triangle still waiting for one cannot be represented.
            QJsonArray specializations;
            for (const auto& value : project["specializations"].toArray())
                if (!value.toObject()["supertype"].isNull()) specializations.append(value);
            project["specializations"] = specializations;
        }
        if (version < 5) {
            QJsonArray specializations;
            for (const auto& value : project["specializations"].toArray()) {
                auto specialization = value.toObject();
                specialization.remove("direction");
                specializations.append(specialization);
            }
            project["specializations"] = specializations;
        }
        if (version < 4) {
            // Dropping the specializations must drop their layout entries too,
            // or the document describes a layout for an element it no longer has.
            project.remove("specializations");
            QJsonArray layout;
            for (const auto& value : project["layout"].toArray()) {
                const auto entry = value.toObject();
                if (entry["element"].toObject()["type"].toString() != "specialization") layout.append(entry);
            }
            project["layout"] = layout;
        }
        // Before version 17 a model was never asked what it would become.
        if (version < 17) {
            project.remove("mode");
            for (const char* group : {"entities", "attributes", "relationships"}) {
                QJsonArray kept;
                for (const auto& value : project[group].toArray()) {
                    auto entry = value.toObject();
                    for (const char* field : {"comment", "type", "length", "identifier", "required", "unique"})
                        entry.remove(field);
                    kept.append(entry);
                }
                project[group] = kept;
            }
        }
        // Before version 16 nobody could leave a remark on the diagram.
        if (version < 16) project.remove("comments");
        // Before version 14 a diagram had no paper of its own.
        if (version < 14) project.remove("background");
        // Before version 13 every entity was regular and no relationship identifying.
        if (version < 13) {
            QJsonArray entities;
            for (const auto& value : project["entities"].toArray()) {
                auto entity = value.toObject();
                entity.remove("weak");
                entities.append(entity);
            }
            project["entities"] = entities;
            QJsonArray relationships;
            for (const auto& value : project["relationships"].toArray()) {
                auto relationship = value.toObject();
                relationship.remove("identifying");
                relationships.append(relationship);
            }
            project["relationships"] = relationships;
        }
        // Before version 12 every surface was solid.
        if (version < 12) project.remove("transparency");
        // Before version 11 there were no pictures or notes on the canvas.
        if (version < 11) {
            project.remove("pictures");
            project.remove("notes");
        }
        // Before version 9 an element could not carry a colour of its own.
        if (version < 9) project.remove("colours");
        // Before version 10 both sides of a relationship were always drawn.
        if (version < 10) {
            QJsonArray without;
            for (const auto& value : project["relationships"].toArray()) {
                auto relationship = value.toObject();
                QJsonArray participants;
                for (const auto& item : relationship["participants"].toArray()) {
                    auto participant = item.toObject();
                    participant.remove("show_constraints");
                    participants.append(participant);
                }
                relationship["participants"] = participants;
                without.append(relationship);
            }
            project["relationships"] = without;
        }
        if (version < 8 || version < 7) {
            // Before version 8 a connector had no route of its own, and before
            // version 7 no pinned joins, so a file of that vintage carries
            // none of those keys at all.
            QJsonArray connectors;
            for (const auto& value : project["connectors"].toArray()) {
                auto connector = value.toObject();
                if (version < 8) connector.remove("waypoints");
                if (version < 7) {
                    connector.remove("owner_anchor");
                    connector.remove("child_anchor");
                }
                connectors.append(connector);
            }
            project["connectors"] = connectors;
        }
        if (version < 2) project.remove("connectors");
        forget_project_description(project);
        forget_answered_sides(project);
        forget_schema_layout(project);
        forget_auto_increment(project);
        forget_schema_edits(project);
        forget_decisions(project);
        document["project"] = project;
        document["format_version"] = version;
        return document;
    };

    for (const int version : {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13}) {
        const auto opened = ErdxProjectStore::decode(bytes(downgrade(version)));
        if (!opened) throw std::runtime_error("version " + std::to_string(version) + ": " + opened.error);
        CHECK(opened.project->entities == fixture.editor.project().entities);
        CHECK(opened.project->connectors.size() == (version == 1 ? 0u : 2u));
        for (const auto& [id, relationship] : opened.project->relationships) {
            (void)id;
            CHECK(!relationship.associative);
            for (const auto& participant : relationship.participants)
                CHECK(std::holds_alternative<EntityId>(participant.target));
        }
        // Saving an older document upgrades it to the current version.
        CHECK(opened.project->specializations.size() == (version < 4 ? 0u : 1u));
        for (const auto& [id, specialization] : opened.project->specializations) {
            (void)id;
            CHECK(specialization.supertype.has_value());
        }
        for (const auto& [id, specialization] : opened.project->specializations) {
            (void)id;
            // Version 5 onwards records the direction; before that it is lost
            // and reads as specialization, which is how those files were drawn.
            CHECK(specialization.direction == (version >= 5 ? Inheritance::Generalization : Inheritance::Specialization));
        }
        CHECK(QJsonDocument::fromJson(ErdxProjectStore::encode(*opened.project)).object()["format_version"].toInt() == 28);
    }

    // A document whose shape contradicts its declared version is refused rather
    // than read leniently, in both directions.
    auto smuggled = fixture.document();
    smuggled["format_version"] = 4;
    reject(bytes(smuggled));
    auto stale = downgrade(4);
    stale["format_version"] = 5;
    reject(bytes(stale));
    // A triangle still waiting for its supertype cannot be written as version 5.
    auto waiting = fixture.document();
    change_first(waiting, "specializations", [](QJsonObject& item) { item["supertype"] = QJsonValue::Null; });
    CHECK(ErdxProjectStore::decode(bytes(waiting)));
    waiting["format_version"] = 5;
    reject(bytes(waiting));
    auto missing = fixture.document();
    auto project = missing["project"].toObject();
    project.remove("connectors");
    missing["project"] = project;
    reject(bytes(missing));
}

// Pictures and notes are written with the model and come back byte for byte.
// A document from before version 11 must not carry them, one that declares
// version 11 must, and a picture's image has to be base64 that decodes to
// something that could be an image.
void pictures_and_notes_persist() {
    Fixture fixture;
    const std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
    const auto picture = fixture.editor.create_picture("Campus map", {400, -200, 240, 160}, png);
    CHECK(picture && picture.created);
    const auto note = fixture.editor.create_note("Assumptions", {-400, 300, 200, 120},
                                                 "Each student enrols each term.\nGrades per enrolment.");
    CHECK(note && note.created);
    CHECK(fixture.editor.recolour({*note.created}, Colour{255, 224, 138}));

    const auto encoded = ErdxProjectStore::encode(fixture.editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    CHECK(root["format_version"].toInt() == 28);
    const auto project = root["project"].toObject();
    CHECK(project["pictures"].toArray().size() == 1);
    CHECK(project["notes"].toArray().size() == 1);
    const auto written = project["pictures"].toArray()[0].toObject();
    CHECK(written.size() == 4 && written["image"].isString());
    CHECK(QByteArray::fromBase64(written["image"].toString().toLatin1())
          == QByteArray(reinterpret_cast<const char*>(png.data()), static_cast<qsizetype>(png.size())));

    const auto reopened = ErdxProjectStore::decode(encoded);
    CHECK(reopened);
    CHECK(*reopened.project == fixture.editor.project());
    CHECK(reopened.project->pictures.at(std::get<PictureId>(*picture.created)).image == png);
    CHECK(reopened.project->notes.at(std::get<NoteId>(*note.created)).description.find("Grades") != std::string::npos);
    CHECK(reopened.project->colours.contains(*note.created));
    CHECK(!reopened.project->notes.at(std::get<NoteId>(*note.created)).plain);

    // A symbol is a note that is one character drawn bare, so it travels the
    // same way and says so in the file.
    const auto symbol = fixture.editor.create_symbol("⋈", {120, 120, 56, 56});
    CHECK(symbol && symbol.created);
    const auto with_symbol = ErdxProjectStore::encode(fixture.editor.project());
    const auto written_notes = QJsonDocument::fromJson(with_symbol).object()["project"].toObject()["notes"].toArray();
    CHECK(written_notes.size() == 2);
    for (const auto& value : written_notes) CHECK(value.toObject().size() == 4 && value.toObject()["plain"].isBool());
    const auto reread = ErdxProjectStore::decode(with_symbol);
    CHECK(reread);
    CHECK(*reread.project == fixture.editor.project());
    CHECK(reread.project->notes.at(std::get<NoteId>(*symbol.created)).plain);
    CHECK(reread.project->notes.at(std::get<NoteId>(*symbol.created)).name == "⋈");

    // A note written before version 15 is a card, which is what those files
    // meant; the flag must not be invented for them.
    auto older = QJsonDocument::fromJson(with_symbol).object();
    older["format_version"] = 14;
    auto older_project = older["project"].toObject();
    // Version 14 knew nothing of comments either, so the field goes with the
    // flag: a document claiming to be older must look older throughout.
    older_project.remove("comments");
    older_project.remove("mode");
    for (const char* group : {"entities", "attributes", "relationships"}) {
        QJsonArray kept;
        for (const auto& value : older_project[group].toArray()) {
            auto entry = value.toObject();
            for (const char* field : {"comment", "type", "length", "identifier", "required", "unique"})
                entry.remove(field);
            kept.append(entry);
        }
        older_project[group] = kept;
    }
    auto stripped = QJsonArray();
    for (const auto& value : older_project["notes"].toArray()) {
        auto entry = value.toObject();
        entry.remove("plain");
        stripped.append(entry);
    }
    older_project["notes"] = stripped;
    forget_bridge_keys(older_project);
    forget_project_description(older_project);
    forget_answered_sides(older_project);
    forget_schema_layout(older_project);
    forget_auto_increment(older_project);
    forget_schema_edits(older_project);
    forget_decisions(older_project);
    older["project"] = older_project;
    const auto from_older = ErdxProjectStore::decode(QJsonDocument(older).toJson(QJsonDocument::Compact));
    CHECK(from_older);
    for (const auto& [id, kept] : from_older.project->notes) { (void)id; CHECK(!kept.plain); }

    // Through the adapter and back, the same.
    QTemporaryDir directory;
    CHECK(directory.isValid());
    ErdxProjectStore store;
    const auto path = directory.filePath("figures.erdx").toStdString();
    CHECK(store.save(path, fixture.editor.project()).ok);
    const auto loaded = store.load(path);
    CHECK(loaded && *loaded.project == fixture.editor.project());

    // The version and the fields have to agree, in both directions.
    auto stale = root;
    stale["format_version"] = 10;
    reject(bytes(stale));
    auto missing = root;
    change_project(missing, [](QJsonObject& item) { item.remove("pictures"); });
    reject(bytes(missing));
    // The image is base64 text that decodes to an image, of at most 2 MiB.
    auto garbled = root;
    change_first(garbled, "pictures", [](QJsonObject& item) { item["image"] = "not base64!!"; });
    reject_because(bytes(garbled), "base64");
    auto not_image = root;
    change_first(not_image, "pictures", [](QJsonObject& item) {
        item["image"] = QString::fromLatin1(QByteArray("hello world").toBase64());
    });
    reject_because(bytes(not_image), "PNG or JPEG");
    auto oversized = root;
    change_first(oversized, "pictures", [&](QJsonObject& item) {
        QByteArray huge(static_cast<qsizetype>(max_image_bytes) + 1, '\0');
        std::copy(png.begin(), png.end(), huge.begin());
        item["image"] = QString::fromLatin1(huge.toBase64());
    });
    reject_because(bytes(oversized), "2 MiB");
    // A picture is placed like everything else, and nothing belongs to one.
    auto unplaced = root;
    change_project(unplaced, [](QJsonObject& item) {
        QJsonArray layout;
        for (const auto& value : item["layout"].toArray())
            if (value.toObject()["element"].toObject()["type"].toString() != "picture") layout.append(value);
        item["layout"] = layout;
    });
    reject(bytes(unplaced));
    auto owned_by_note = root;
    change_first(owned_by_note, "attributes", [&](QJsonObject& item) {
        item["owner"] = QJsonObject{{"type", "note"}, {"id", uuid_text(uuid(*note.created))}};
    });
    reject_because(bytes(owned_by_note), "holds no attributes");
}

// How see-through each surface is travels with the model from version 12, as
// a percentage per element. Earlier files' surfaces are solid and must not
// carry one.
void transparency_persists() {
    Fixture fixture;
    CHECK(fixture.editor.set_transparency({ElementRef{fixture.employee}}, 100));
    CHECK(fixture.editor.set_transparency({ElementRef{fixture.supervises}}, 40));
    const auto encoded = ErdxProjectStore::encode(fixture.editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    const auto entries = root["project"].toObject()["transparency"].toArray();
    CHECK(entries.size() == 2);
    for (const auto& value : entries) CHECK(value.toObject().size() == 2 && value.toObject().contains("percent"));
    const auto reopened = ErdxProjectStore::decode(encoded);
    CHECK(reopened);
    CHECK(reopened.project->transparency.at(ElementRef{fixture.employee}) == 100);
    CHECK(reopened.project->transparency.at(ElementRef{fixture.supervises}) == 40);
    CHECK(*reopened.project == fixture.editor.project());
    // A whole percentage, and never more than all of it.
    auto too_much = root;
    change_first(too_much, "transparency", [](QJsonObject& item) { item["percent"] = 101; });
    reject_because(bytes(too_much), "0 to 100");
    auto fractional = root;
    change_first(fractional, "transparency", [](QJsonObject& item) { item["percent"] = 12.5; });
    reject_because(bytes(fractional), "whole");
    // The version and the field have to agree, in both directions.
    auto stale = root;
    stale["format_version"] = 11;
    reject(bytes(stale));
    auto solid = root;
    change_project(solid, [](QJsonObject& item) { item.remove("transparency"); });
    reject(bytes(solid));
    // A dangling entry is refused like a dangling colour.
    auto dangling = root;
    change_first(dangling, "transparency", [](QJsonObject& item) {
        item["element"] = QJsonObject{{"type", "entity"}, {"id", "019947b9-7111-7000-8000-0000000000ff"}};
    });
    reject(bytes(dangling));
}

// A weak entity and an identifying relationship travel with the model from
// version 13; earlier files carry neither flag and read as regular.
void weak_entities_and_identifying_relationships_persist() {
    Fixture fixture;
    const auto dependant = fixture.editor.create_entity("Dependant", {400, 20, 160, 80});
    CHECK(dependant && dependant.created);
    CHECK(fixture.editor.set_entity_weak(std::get<EntityId>(*dependant.created), true));
    CHECK(fixture.editor.set_relationship_kind(fixture.supervises, RelationshipKind::Identifying));
    const auto encoded = ErdxProjectStore::encode(fixture.editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    for (const auto& value : root["project"].toObject()["entities"].toArray()) CHECK(value.toObject().contains("weak"));
    for (const auto& value : root["project"].toObject()["relationships"].toArray()) CHECK(value.toObject().contains("identifying"));
    const auto reopened = ErdxProjectStore::decode(encoded);
    CHECK(reopened);
    CHECK(reopened.project->entities.at(std::get<EntityId>(*dependant.created)).weak);
    CHECK(reopened.project->relationships.at(fixture.supervises).identifying);
    CHECK(*reopened.project == fixture.editor.project());
    // The version and the fields have to agree, in both directions.
    auto stale = root;
    stale["format_version"] = 12;
    reject(bytes(stale));
    auto missing = root;
    change_first(missing, "entities", [](QJsonObject& item) { item.remove("weak"); });
    reject(bytes(missing));
    // A flag is true or false, and a relationship is not both kinds at once.
    auto wrong = root;
    change_first(wrong, "entities", [](QJsonObject& item) { item["weak"] = "yes"; });
    reject(bytes(wrong));
    auto both = root;
    change_first(both, "relationships", [](QJsonObject& item) { item["associative"] = true; item["identifying"] = true; });
    reject_because(bytes(both), "not both");
}

// Whether a relationship side was answered travels with it from version 19. A
// side reads Many and Partial whether somebody chose that or never looked, so
// without this a conversion cannot tell a decided M:M from two untouched
// defaults, and would build junction tables out of questions nobody was asked.
void answered_sides_persist() {
    Fixture fixture;
    auto& editor = fixture.editor;

    // The fixture answers both of its sides, so this needs a relationship of
    // its own that has only been drawn.
    const auto made = editor.create_relationship("Mentors", {320, 220, 150, 100});
    CHECK(made);
    const auto mentors = std::get<RelationshipId>(*made.created);
    const auto left = editor.connect(mentors, fixture.employee);
    const auto right = editor.connect(mentors, fixture.employee);
    CHECK(left && right);

    // A side that is merely connected reads Many and Partial, and has not been
    // answered. Those two facts are exactly what has to stay distinguishable.
    for (const auto& side : editor.project().relationships.at(mentors).participants) {
        CHECK(side.maximum == Cardinality::Many);
        CHECK(side.participation == Participation::Partial);
        CHECK(!side.cardinality_confirmed);
        CHECK(!side.participation_confirmed);
    }

    // Answering one says so, and leaves the other side alone. The values do not
    // move: choosing Many is a different fact from never having been asked, and
    // that is why the flag sits beside the value rather than being derived
    // from it.
    CHECK(editor.update_participant(mentors, *left.participant, Cardinality::Many, Participation::Partial, ""));
    const auto& answered = editor.project().relationships.at(mentors);
    CHECK(answered.participants.front().cardinality_confirmed);
    CHECK(answered.participants.front().participation_confirmed);
    CHECK(answered.participants.front().maximum == Cardinality::Many);
    CHECK(answered.participants.front().participation == Participation::Partial);
    CHECK(!answered.participants.back().cardinality_confirmed);
    CHECK(!answered.participants.back().participation_confirmed);

    const auto encoded = ErdxProjectStore::encode(editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    CHECK(root["format_version"].toInt() == 28);
    for (const auto& value : root["project"].toObject()["relationships"].toArray())
        for (const auto& side : value.toObject()["participants"].toArray()) {
            CHECK(side.toObject().contains("cardinality_confirmed"));
            CHECK(side.toObject().contains("participation_confirmed"));
        }
    const auto reread = ErdxProjectStore::decode(encoded);
    CHECK(reread);
    CHECK(*reread.project == editor.project());
    // Specifically: the side nobody answered comes back unanswered, rather than
    // being quietly promoted by the round trip.
    const auto& back = reread.project->relationships.at(mentors);
    CHECK(back.participants.front().cardinality_confirmed);
    CHECK(!back.participants.back().cardinality_confirmed);
    CHECK(!back.participants.back().participation_confirmed);

    // A file written before version 19 says nothing about who answered what.
    // Every side in it is read as answered, so a finished diagram is not
    // greeted with a readiness question about every line on it.
    auto older = root;
    older["format_version"] = 18;
    auto older_project = older["project"].toObject();
    QJsonArray kept;
    for (const auto& value : older_project["relationships"].toArray()) {
        auto entry = value.toObject();
        QJsonArray sides;
        for (const auto& side : entry["participants"].toArray()) {
            auto one = side.toObject();
            one.remove("cardinality_confirmed");
            one.remove("participation_confirmed");
            sides.append(one);
        }
        entry["participants"] = sides;
        kept.append(entry);
    }
    older_project["relationships"] = kept;
    forget_bridge_keys(older_project);
    forget_project_description(older_project);
    forget_schema_layout(older_project);
    forget_auto_increment(older_project);
    forget_schema_edits(older_project);
    forget_decisions(older_project);
    older["project"] = older_project;
    const auto from_older = ErdxProjectStore::decode(bytes(older));
    CHECK(from_older);
    for (const auto& [id, relationship] : from_older.project->relationships) {
        (void)id;
        for (const auto& side : relationship.participants) {
            CHECK(side.cardinality_confirmed);
            CHECK(side.participation_confirmed);
        }
    }

    // The version and the fields agree in both directions.
    auto stale = root;
    stale["format_version"] = 18;
    reject(bytes(stale));
    auto wrong = root;
    auto wrong_project = wrong["project"].toObject();
    auto relationships = wrong_project["relationships"].toArray();
    auto entry = relationships[0].toObject();
    auto sides = entry["participants"].toArray();
    auto one = sides[0].toObject();
    one["cardinality_confirmed"] = "yes";
    sides[0] = one;
    entry["participants"] = sides;
    relationships[0] = entry;
    wrong_project["relationships"] = relationships;
    wrong["project"] = wrong_project;
    reject(bytes(wrong));
}

// The answers to what a conversion cannot decide for itself travel with the
// project from version 20, keyed by stable identity so that a decision survives
// renaming and is never asked a second time.
void conversion_decisions_persist() {
    Fixture fixture;
    auto& editor = fixture.editor;

    // Nothing is answered to begin with, which is not the same as every
    // question being answered the default way.
    CHECK(editor.project().decisions.isa.empty());
    CHECK(editor.project().decisions.naming == TableNaming::Plural);

    CHECK(editor.set_table_naming(TableNaming::AsDrawn));
    CHECK(editor.set_isa_strategy(fixture.specialisation, IsaStrategy::SingleTable));
    CHECK(editor.set_composite_mode(fixture.address, CompositeMode::Both));
    const auto side = editor.project().relationships.at(fixture.supervises).participants.front().id;
    CHECK(editor.set_one_to_one_key(fixture.supervises, side));
    CHECK(editor.set_junction_name(fixture.supervises, "EmployeeSupervisor"));
    CHECK(editor.set_table_name(ElementRef{fixture.employee}, "Staff"));

    // A decision is an edit like any other, so it undoes.
    const auto before = editor.project();
    CHECK(editor.set_table_naming(TableNaming::Plural));
    CHECK(editor.project().decisions.naming == TableNaming::Plural);
    CHECK(editor.undo());
    CHECK(editor.project() == before);

    const auto encoded = ErdxProjectStore::encode(editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    CHECK(root["format_version"].toInt() == 28);
    CHECK(root["project"].toObject().contains("decisions"));

    const auto reread = ErdxProjectStore::decode(encoded);
    CHECK(reread);
    CHECK(*reread.project == editor.project());
    const auto& decided = reread.project->decisions;
    CHECK(decided.naming == TableNaming::AsDrawn);
    CHECK(decided.isa.at(fixture.specialisation) == IsaStrategy::SingleTable);
    CHECK(decided.composite.at(fixture.address) == CompositeMode::Both);
    CHECK(decided.one_to_one_key.at(fixture.supervises) == side);
    CHECK(decided.junction_name.at(fixture.supervises) == "EmployeeSupervisor");
    CHECK(decided.table_name.at(relation_from(ElementRef{fixture.employee})) == "Staff");

    // Taking an answer back returns that question to its default rather than
    // recording a different answer.
    CHECK(editor.set_isa_strategy(fixture.specialisation, std::nullopt));
    CHECK(editor.project().decisions.isa.empty());

    // A decision cannot point at something that is not there, nor at the wrong
    // kind of thing: a composite decision belongs to a composite alone.
    const auto plain = editor.create_attribute("Grade", {}, ElementRef{fixture.employee});
    CHECK(plain);
    CHECK(!editor.set_composite_mode(std::get<AttributeId>(*plain.created), CompositeMode::Parts));
    CHECK(!editor.set_one_to_one_key(fixture.supervises, ParticipantId{}));

    // A file written before version 20 answered nothing at all.
    auto older = root;
    older["format_version"] = 19;
    auto older_project = older["project"].toObject();
    forget_bridge_keys(older_project);
    forget_project_description(older_project);
    forget_schema_layout(older_project);
    forget_auto_increment(older_project);
    forget_schema_edits(older_project);
    forget_decisions(older_project);
    older["project"] = older_project;
    const auto from_older = ErdxProjectStore::decode(bytes(older));
    CHECK(from_older);
    CHECK(from_older.project->decisions.isa.empty());
    CHECK(from_older.project->decisions.table_name.empty());
    CHECK(from_older.project->decisions.naming == TableNaming::Plural);

    // The version and the fields agree in both directions.
    auto stale = root;
    stale["format_version"] = 19;
    reject(bytes(stale));
}

// The paper a diagram is drawn on travels with it from version 14: the style,
// how strongly it shows, and a picture of the user's own when there is one.
void backgrounds_persist() {
    Fixture fixture;
    CHECK(fixture.editor.set_background(Background{BackgroundStyle::Squares, 100, {}}));
    auto encoded = ErdxProjectStore::encode(fixture.editor.project());
    auto root = QJsonDocument::fromJson(encoded).object();
    const auto paper = root["project"].toObject()["background"].toObject();
    CHECK(paper.size() == 3);
    CHECK(paper["style"].toString() == "squares" && paper["strength"].toInt() == 100);
    CHECK(paper["image"].toString().isEmpty());
    auto reopened = ErdxProjectStore::decode(encoded);
    CHECK(reopened);
    CHECK(reopened.project->background.style == BackgroundStyle::Squares);
    CHECK(reopened.project->background.strength == 100);
    CHECK(*reopened.project == fixture.editor.project());

    // A picture of one's own comes back byte for byte.
    const std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13};
    CHECK(fixture.editor.set_background(Background{BackgroundStyle::Image, 70, png}));
    encoded = ErdxProjectStore::encode(fixture.editor.project());
    reopened = ErdxProjectStore::decode(encoded);
    CHECK(reopened && reopened.project->background.image == png);
    CHECK(reopened.project->background.style == BackgroundStyle::Image);

    // Only a picture background carries one, and the style and strength have
    // to be ones the format knows.
    root = QJsonDocument::fromJson(encoded).object();
    auto stray = root;
    change_project(stray, [](QJsonObject& item) {
        auto paper = item["background"].toObject();
        paper["style"] = "lines";
        item["background"] = paper;
    });
    reject_because(bytes(stray), "Only a picture background");
    auto unknown = root;
    change_project(unknown, [](QJsonObject& item) {
        auto paper = item["background"].toObject();
        paper["style"] = "marble";
        item["background"] = paper;
    });
    reject_because(bytes(unknown), "Unsupported background style");
    auto too_strong = root;
    change_project(too_strong, [](QJsonObject& item) {
        auto paper = item["background"].toObject();
        paper["strength"] = 140;
        item["background"] = paper;
    });
    reject_because(bytes(too_strong), "0 to 100");
    auto not_an_image = root;
    change_project(not_an_image, [](QJsonObject& item) {
        auto paper = item["background"].toObject();
        paper["image"] = QString::fromLatin1(QByteArray("hello world").toBase64());
        item["background"] = paper;
    });
    reject_because(bytes(not_an_image), "PNG or JPEG");

    // The version and the field have to agree, in both directions.
    auto stale = root;
    stale["format_version"] = 13;
    reject(bytes(stale));
    auto missing = root;
    change_project(missing, [](QJsonObject& item) { item.remove("background"); });
    reject(bytes(missing));
    CHECK(fixture.editor.set_background(Background{}));
}

void invalid_connector_shapes() {
    Fixture fixture;
    const auto side = fixture.editor.project().relationships.at(fixture.supervises).participants.front().id;
    CHECK(fixture.editor.bend_connector(ConnectorRef{side}, 20));
    const auto valid = uuid_text(side.value);

    // The link must exist, and its type must be one the format defines.
    for (const auto& link : {QJsonObject{{"type", "participant"}, {"id", uuid_text(fixture.employee.value)}},
                             QJsonObject{{"type", "attribute"}, {"id", valid}},
                             QJsonObject{{"type", "entity"}, {"id", valid}},
                             QJsonObject{{"type", "relationship"}, {"id", valid}}}) {
        auto root = fixture.document();
        change_project(root, [&](QJsonObject& project) {
            auto list = project["connectors"].toArray();
            auto first = list[0].toObject();
            first["link"] = link;
            list[0] = first;
            project["connectors"] = list;
        });
        reject(bytes(root));
    }
    // Offsets must be finite numbers inside the supported canvas.
    for (const auto& offset : {QJsonValue("20"), QJsonValue(true), QJsonValue(QJsonValue::Null), QJsonValue(1e9)}) {
        auto root = fixture.document();
        change_project(root, [&](QJsonObject& project) {
            auto list = project["connectors"].toArray();
            auto first = list[0].toObject();
            first["offset"] = offset;
            list[0] = first;
            project["connectors"] = list;
        });
        reject(bytes(root));
    }
    // Two shapes for one connector are contradictory rather than last-wins.
    auto root = fixture.document();
    change_project(root, [](QJsonObject& project) {
        auto list = project["connectors"].toArray();
        list.append(list.first());
        project["connectors"] = list;
    });
    reject(bytes(root));
    // Unknown and missing fields are refused like everywhere else.
    root = fixture.document();
    change_first(root, "connectors", [](QJsonObject& item) { item["future_extension"] = 1; });
    reject(bytes(root));
    root = fixture.document();
    change_first(root, "connectors", [](QJsonObject& item) { item.remove("offset"); });
    reject(bytes(root));
}

void invalid_identifiers_references_and_enums() {
    Fixture fixture;
    const auto valid_id = uuid_text(fixture.employee.value);
    for (const auto& bad_id : {QString("not-an-id"), QString("019947BA-7111-7000-8000-000000000001"), "{" + valid_id + "}", QString("019947b9-7111-4000-8000-000000000001")}) {
        auto root = fixture.document();
        change_first(root, "entities", [&](QJsonObject& entity) { entity["id"] = bad_id; });
        reject(bytes(root));
    }
    auto root = fixture.document();
    change_project(root, [&](QJsonObject& project) { project["id"] = valid_id; });
    reject(bytes(root));
    root = fixture.document();
    change_project(root, [](QJsonObject& project) {
        auto list = project["entities"].toArray(); list.append(list.first()); project["entities"] = list;
    });
    reject(bytes(root));
    root = fixture.document();
    change_first(root, "attributes", [](QJsonObject& attribute) { attribute["kind"] = "future-kind"; });
    reject(bytes(root));
    root = fixture.document();
    change_first(root, "attributes", [](QJsonObject& attribute) {
        attribute["owner"] = QJsonObject{{"type", "entity"}, {"id", "019947b9-7111-7000-8000-000000000001"}};
    });
    reject(bytes(root));
    root = fixture.document();
    change_first(root, "attributes", [](QJsonObject& attribute) {
        auto owner = attribute["owner"].toObject(); owner["type"] = "page"; attribute["owner"] = owner;
    });
    reject(bytes(root));
    for (const auto* property : {"maximum", "participation"}) {
        root = fixture.document();
        change_first(root, "relationships", [&](QJsonObject& relationship) {
            auto list = relationship["participants"].toArray(); auto p = list[0].toObject(); p[property] = "unsupported"; list[0] = p; relationship["participants"] = list;
        });
        reject(bytes(root));
    }
    root = fixture.document();
    change_first(root, "relationships", [](QJsonObject& relationship) {
        auto list = relationship["participants"].toArray(); list[1] = list[0]; relationship["participants"] = list;
    });
    reject(bytes(root));
    root = fixture.document();
    change_first(root, "layout", [](QJsonObject& layout) { layout["width"] = -1; });
    reject(bytes(root));
    root = fixture.document();
    change_first(root, "layout", [](QJsonObject& layout) { layout["x"] = "0"; });
    reject(bytes(root));
    root = fixture.document();
    change_project(root, [](QJsonObject& project) { project["layout"] = QJsonArray{}; });
    reject(bytes(root));
}

void resource_limits() {
    reject(QByteArray(ErdxProjectStore::max_file_bytes + 1, ' '));
    reject(QByteArray(33, '[') + QByteArray(33, ']'));
    Fixture fixture;
    auto root = fixture.document();
    change_project(root, [](QJsonObject& project) {
        QJsonArray list;
        for (std::size_t i = 0; i <= max_elements; ++i) list.append(QJsonValue::Null);
        project["entities"] = list;
    });
    reject(bytes(root));
    // Braces and brackets inside strings do not increase structural depth.
    CHECK(fixture.editor.rename_project(std::string(100, '[')));
    CHECK(ErdxProjectStore::decode(ErdxProjectStore::encode(fixture.editor.project())));
}

void failed_saves_preserve_existing_destination() {
    Fixture fixture;
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    ErdxProjectStore store;
    const auto path = temporary.filePath("existing.erdx");
    CHECK(store.save(path.toStdString(), fixture.editor.project()));
    const auto existing = read_file(path);
    auto invalid = fixture.editor.project();
    invalid.name.assign(max_name_bytes + 1, 'x');
    const auto failed = store.save(path.toStdString(), invalid);
    CHECK(!failed);
    CHECK(!failed.error.empty());
    CHECK(read_file(path) == existing);
    auto oversized = fixture.editor.project();
    for (std::size_t i = 0; i < 520; ++i) {
        const EntityId id{fixture.ids.next()};
        oversized.entities.emplace(id, Entity{.id = id, .name = "Entity", .description = std::string(max_description_bytes, 'x')});
        oversized.layout.emplace(ElementRef{id}, Rect{});
    }
    CHECK(!store.save(path.toStdString(), oversized));
    CHECK(read_file(path) == existing);
    CHECK(!store.save(temporary.filePath("missing/folder.erdx").toStdString(), fixture.editor.project()));
    CHECK(read_file(path) == existing);
    // A non-empty directory cannot be replaced at commit. Its existing child
    // remains intact, and direct-write fallback is disabled by the adapter.
    const auto sentinel = temporary.filePath("sentinel");
    write_file(sentinel, "keep me");
    CHECK(!store.save(temporary.path().toStdString(), fixture.editor.project()));
    CHECK(read_file(sentinel) == "keep me");
    CHECK(read_file(path) == existing);
    CHECK(fixture.editor.rename_project("Latest"));
    CHECK(save_project(fixture.editor, store, path.toStdString()));
    CHECK(!fixture.editor.dirty());
    CHECK(read_file(path) != existing);
    const auto reloaded = store.load(path.toStdString());
    CHECK(reloaded);
    CHECK(*reloaded.project == fixture.editor.project());
}

void failed_load_and_save_preserve_session() {
    Fixture fixture;
    QTemporaryDir temporary;
    CHECK(temporary.isValid());
    ErdxProjectStore store;
    CHECK(fixture.editor.rename_project("Temporary"));
    CHECK(fixture.editor.undo());
    const auto current = fixture.editor.project();
    const auto revision = fixture.editor.revision();
    const auto history = fixture.editor.history_bytes();
    const auto redo = fixture.editor.redo_label();
    const auto dirty = fixture.editor.dirty();
    const auto corrupt = temporary.filePath("corrupt.erdx");
    write_file(corrupt, "this is not JSON");
    CHECK(!open_project(fixture.editor, store, corrupt.toStdString()));
    CHECK(!open_project(fixture.editor, store, temporary.filePath("absent.erdx").toStdString()));
    CHECK(!save_project(fixture.editor, store, temporary.filePath("absent/project.erdx").toStdString()));
    CHECK(fixture.editor.project() == current);
    CHECK(fixture.editor.revision() == revision);
    CHECK(fixture.editor.history_bytes() == history);
    CHECK(fixture.editor.redo_label() == redo);
    CHECK(fixture.editor.dirty() == dirty);

    // A store that hands over a project it should not. Handing over bytes is
    // not what it is testing, so it refuses to, which is what a store with
    // nothing to give is expected to say.
    struct InvalidStore final : ProjectStore {
        Project candidate;
        LoadResult load(const std::string&) override { return {candidate, {}}; }
        SaveResult save(const std::string&, const Project&) override { return {false, "Injected save failure"}; }
        EncodeResult project_bytes(const Project&) override { return {{}, "Injected encode failure"}; }
        LoadResult project_from_bytes(const std::string&) override { return {candidate, {}}; }
    } invalid_store;
    invalid_store.candidate = current;
    invalid_store.candidate.layout.clear();
    CHECK(!open_project(fixture.editor, invalid_store, "unused"));
    CHECK(fixture.editor.project() == current);
    CHECK(fixture.editor.revision() == revision);

    struct ChangingStore final : ProjectStore {
        Editor& editor;
        explicit ChangingStore(Editor& value) : editor(value) {}
        LoadResult load(const std::string&) override { return {{}, "unused"}; }
        SaveResult save(const std::string&, const Project&) override {
            CHECK(editor.rename_project("Edit during save"));
            return {true, {}};
        }
        EncodeResult project_bytes(const Project&) override { return {{}, "Injected encode failure"}; }
        LoadResult project_from_bytes(const std::string&) override { return {{}, "unused"}; }
    } changing_store(fixture.editor);
    CHECK(save_project(fixture.editor, changing_store, "unused"));
    CHECK(fixture.editor.dirty());
    CHECK(fixture.editor.project().name == "Edit during save");
}
} // namespace

// Comments travel with the project, including the three different things one
// can be pinned to, and a file written before they existed carries none.
// What a model says about what it will become travels with it.
// Where the schema has been edited away from the diagram, that difference is
// part of the document: it was deliberate, so losing it on save would be
// losing work. A file written before version 21 has no such section, and reads
// correctly as the two levels agreeing about everything.
void schema_divergence_persists() {
    Fixture fixture;
    auto& editor = fixture.editor;
    CHECK(editor.add_schema_column(ElementRef{fixture.employee}, "Nickname"));
    const auto added = editor.project().schema.added.at(relation_from(ElementRef{fixture.employee})).front().id;
    CHECK(editor.hide_in_schema(fixture.address, true));

    const auto encoded = ErdxProjectStore::encode(editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    CHECK(root["format_version"].toInt() == 28);
    const auto reopened = ErdxProjectStore::decode(encoded);
    CHECK(reopened);
    CHECK(*reopened.project == editor.project());
    CHECK(reopened.project->schema.added.at(relation_from(ElementRef{fixture.employee})).front().id == added);
    CHECK(reopened.project->schema.added.at(relation_from(ElementRef{fixture.employee})).front().name == "Nickname");
    CHECK(reopened.project->schema.hidden.contains(fixture.address));
    // The diagram is untouched by either: a hidden attribute is still on it.
    CHECK(reopened.project->attributes.contains(fixture.address));

    // A file from before the schema could differ opens with the two agreeing.
    auto older = root;
    auto older_project = older["project"].toObject();
    forget_bridge_keys(older_project);
    forget_project_description(older_project);
    forget_schema_layout(older_project);
    forget_auto_increment(older_project);
    forget_schema_edits(older_project);
    older["project"] = older_project;
    older["format_version"] = 20;
    const auto from_older = ErdxProjectStore::decode(bytes(older));
    CHECK(from_older);
    CHECK(from_older.project->schema.empty());

    // And a version 21 file that omits the section is refused, rather than
    // read leniently: a document must look like the version it claims.
    auto liar = root;
    auto liar_project = liar["project"].toObject();
    forget_schema_edits(liar_project);
    liar["project"] = liar_project;
    reject(bytes(liar));
}

// How the schema has been arranged by hand is part of the document, and that
// includes how tall a table has been pulled as well as how wide.
void schema_arrangement_persists() {
    Fixture fixture;
    auto& editor = fixture.editor;
    const ElementRef table{fixture.employee};
    CHECK(editor.resize_schema_tables({{table, SchemaTableBox{340, 260, Point{80, 120}}}}));
    CHECK(editor.shape_schema_line(LinkSource{fixture.address},
                                   SchemaLine{{Point{10, 10}, Point{10, 60}}, std::nullopt, std::nullopt}));

    const auto encoded = ErdxProjectStore::encode(editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    CHECK(root["format_version"].toInt() == 28);
    const auto reopened = ErdxProjectStore::decode(encoded);
    CHECK(reopened);
    CHECK(*reopened.project == editor.project());
    CHECK(reopened.project->schema_layout.widths.at(relation_from(table)) == 340);
    CHECK(reopened.project->schema_layout.heights.at(relation_from(table)) == 260);
    CHECK((reopened.project->schema_layout.tables.at(relation_from(table)) == Point{80, 120}));

    // A table that has only been made taller still has a row of its own, so
    // its height is not lost for want of anything else to write beside it.
    Fixture only_taller;
    CHECK(only_taller.editor.resize_schema_tables(
        {{ElementRef{only_taller.employee}, SchemaTableBox{0, 300, std::nullopt}}}));
    const auto tall = ErdxProjectStore::decode(ErdxProjectStore::encode(only_taller.editor.project()));
    CHECK(tall);
    CHECK(tall.project->schema_layout.widths.empty());
    CHECK(tall.project->schema_layout.heights.at(relation_from(ElementRef{only_taller.employee})) == 300);

    // A version 22 file says only how wide a table was pulled, which reads
    // correctly as one still as tall as its own rows make it.
    auto older = root;
    auto older_project = older["project"].toObject();
    auto layout = older_project["schema_layout"].toObject();
    QJsonArray tables;
    for (const auto& value : layout["tables"].toArray()) {
        auto entry = value.toObject();
        entry.remove("height");
        tables.append(entry);
    }
    layout["tables"] = tables;
    older_project["schema_layout"] = layout;
    // A file of that age carries no names for invented keys either, that being
    // a version 24 section.
    auto older_schema = older_project["schema"].toObject();
    older_schema.remove("keys");
    older_project["schema"] = older_schema;
    forget_bridge_keys(older_project);
    forget_project_description(older_project);
    forget_auto_increment(older_project);
    forget_relational(older_project);
    older["project"] = older_project;
    older["format_version"] = 22;
    const auto from_older = ErdxProjectStore::decode(bytes(older));
    CHECK(from_older);
    CHECK(from_older.project->schema_layout.heights.empty());
    CHECK(from_older.project->schema_layout.widths.at(relation_from(table)) == 340);

    // And a version 23 file that omits the height is refused rather than read
    // leniently: a document must look like the version it claims.
    auto liar = older;
    liar["format_version"] = 23;
    reject(bytes(liar));

    // A height outside what a table may be is refused, as a width is.
    auto impossible = root;
    auto impossible_project = impossible["project"].toObject();
    auto impossible_layout = impossible_project["schema_layout"].toObject();
    QJsonArray stretched;
    for (const auto& value : impossible_layout["tables"].toArray()) {
        auto entry = value.toObject();
        if (entry["height"].toDouble() > 0) entry["height"] = max_table_height + 1;
        stretched.append(entry);
    }
    impossible_layout["tables"] = stretched;
    impossible_project["schema_layout"] = impossible_layout;
    impossible["project"] = impossible_project;
    reject_because(bytes(impossible), "A schema table height is outside what a table may be.");
}

// A file written before the Relational Schema had identities of its own is
// migrated on the way in, and must come out meaning exactly what it meant.
void legacy_schema_state_migrates() {
    Fixture fixture;
    auto& editor = fixture.editor;
    // Work of every kind that used to be kept against a conceptual element.
    CHECK(editor.set_table_name(ElementRef{fixture.employee}, "Staff"));
    CHECK(editor.add_schema_column(ElementRef{fixture.employee}, "Nickname"));
    CHECK(editor.rename_schema_key(ElementRef{fixture.employee}, "StaffNo"));
    CHECK(editor.set_key_auto_increment(ElementRef{fixture.employee}, true));
    CHECK(editor.resize_schema_tables(
        {{ElementRef{fixture.employee}, SchemaTableBox{340, 260, Point{80, 120}}}}));

    const auto current = ErdxProjectStore::encode(editor.project());
    const auto root = QJsonDocument::fromJson(current).object();
    CHECK(root["format_version"].toInt() == 28);

    // The same document, said the way a version 25 file said it.
    auto older = root;
    auto older_project = older["project"].toObject();
    forget_bridge_keys(older_project);
    forget_project_description(older_project);
    forget_relational(older_project);
    older["project"] = older_project;
    older["format_version"] = 25;

    const auto migrated = ErdxProjectStore::decode(bytes(older));
    CHECK(migrated);
    const auto relation = relation_from(ElementRef{fixture.employee});
    // Everything that was kept against the element is now kept against the
    // relation it produces, and says the same thing.
    CHECK(migrated.project->decisions.table_name.at(relation) == "Staff");
    CHECK(migrated.project->schema.key_names.at(relation) == "StaffNo");
    CHECK(migrated.project->schema.counting_keys.contains(relation));
    CHECK(migrated.project->schema.added.at(relation).front().name == "Nickname");
    CHECK((migrated.project->schema_layout.tables.at(relation) == Point{80, 120}));
    CHECK(migrated.project->schema_layout.widths.at(relation) == 340);
    CHECK(migrated.project->schema_layout.heights.at(relation) == 260);

    // The same input migrated twice gives the same identities. An identity
    // that changed on each load would lose everything kept against it.
    const auto again = ErdxProjectStore::decode(bytes(older));
    CHECK(again);
    CHECK(again.project->schema_layout.tables == migrated.project->schema_layout.tables);
    CHECK(again.project->schema.added.begin()->first == migrated.project->schema.added.begin()->first);

    // And the schema it produces is the schema it produced before: the whole
    // point of the round is that nothing a reader sees has changed.
    CHECK(schema_preview(*migrated.project) == schema_preview(editor.project()));

    // A migrated file written out again is a version 26 file, and reads back
    // without needing migrating a second time.
    const auto rewritten = ErdxProjectStore::decode(ErdxProjectStore::encode(*migrated.project));
    CHECK(rewritten);
    CHECK(*rewritten.project == *migrated.project);
}

void schema_metadata_persists() {
    Fixture fixture;
    auto& editor = fixture.editor;
    CHECK(editor.set_logical_type(fixture.address, LogicalType::Varchar, 240));
    CHECK(editor.set_attribute_rules(fixture.address, true, true, false));
    // A column that counts itself up, so the version 25 field makes the trip.
    const auto counter = std::get<AttributeId>(
        *editor.create_attribute("Ticket", {}, AttributeOwner{ElementRef{fixture.employee}}).created);
    CHECK(editor.set_logical_type(counter, LogicalType::Int));
    CHECK(editor.set_auto_increment(counter, true));
    CHECK(editor.set_schema_comment(ElementRef{fixture.address}, "Where they live."));
    CHECK(editor.set_schema_comment(ElementRef{fixture.employee}, "A person on the payroll."));
    CHECK(editor.set_schema_comment(ElementRef{fixture.supervises}, "Who reports to whom."));

    const auto encoded = ErdxProjectStore::encode(editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    CHECK(root["format_version"].toInt() == 28);
    // Version 18 has no conceptual mode to write, because there are no modes.
    CHECK(!root["project"].toObject().contains("mode"));

    const auto reread = ErdxProjectStore::decode(encoded);
    CHECK(reread);
    CHECK(*reread.project == editor.project());
    CHECK(reread.project->attributes.at(fixture.address).logical_type == LogicalType::Varchar);
    CHECK(reread.project->attributes.at(fixture.address).length == 240);
    CHECK(reread.project->attributes.at(fixture.address).identifier);
    CHECK(reread.project->attributes.at(counter).auto_increment);
    CHECK(!reread.project->attributes.at(fixture.address).auto_increment);
    CHECK(!reread.project->attributes.at(fixture.address).unique);
    CHECK(reread.project->entities.at(fixture.employee).comment == "A person on the payroll.");
    CHECK(ErdxProjectStore::encode(*reread.project) == encoded);

    // Hostile values are refused rather than half-read.
    for (const char* field : {"type", "length", "identifier"}) {
        auto broken = root;
        auto project = broken["project"].toObject();
        auto attributes = project["attributes"].toArray();
        auto first = attributes[0].toObject();
        first[field] = QString(field) == "type" ? QJsonValue("nonsense")
                     : QString(field) == "length" ? QJsonValue(-3) : QJsonValue("yes");
        attributes[0] = first;
        project["attributes"] = attributes;
        broken["project"] = project;
        reject(bytes(broken));
    }
    // Only the measured types carry a number, whatever a file says.
    auto stray = root;
    auto project = stray["project"].toObject();
    auto attributes = project["attributes"].toArray();
    auto first = attributes[0].toObject();
    first["type"] = "boolean";
    first["length"] = 9;
    attributes[0] = first;
    project["attributes"] = attributes;
    stray["project"] = project;
    reject(bytes(stray));

    // A version 17 file names a conceptual mode. Modes are gone, but the files
    // are not: such a file still opens, everything it says about what it
    // becomes is kept, and only the mode itself is dropped on the floor.
    for (const char* named : {"basic", "convertible"}) {
        auto moded = root;
        moded["format_version"] = 17;
        auto moded_project = moded["project"].toObject();
        moded_project["mode"] = QLatin1String(named);
        forget_project_description(moded_project);
        forget_answered_sides(moded_project);
        forget_schema_layout(moded_project);
        forget_auto_increment(moded_project);
        forget_schema_edits(moded_project);
        forget_decisions(moded_project);
        moded["project"] = moded_project;
        const auto from_moded = ErdxProjectStore::decode(bytes(moded));
        CHECK(from_moded);
        CHECK(from_moded.project->attributes.at(fixture.address).logical_type == LogicalType::Varchar);
        CHECK(from_moded.project->attributes.at(fixture.address).length == 240);
        CHECK(from_moded.project->attributes.at(fixture.address).identifier);
        CHECK(from_moded.project->entities.at(fixture.employee).comment == "A person on the payroll.");
        // Read back, it is a version 18 project like any other, carrying no mode.
        const auto again = QJsonDocument::fromJson(ErdxProjectStore::encode(*from_moded.project)).object();
        CHECK(again["format_version"].toInt() == 28);
        CHECK(!again["project"].toObject().contains("mode"));
    }

    // A version 17 file whose mode is nonsense is still refused, exactly as
    // that version always refused it.
    auto wrong_mode = root;
    wrong_mode["format_version"] = 17;
    auto wrong_project = wrong_mode["project"].toObject();
    wrong_project["mode"] = "engineering";
    wrong_mode["project"] = wrong_project;
    reject(bytes(wrong_mode));

    // And a version 18 file may not carry one at all.
    auto stray_mode = root;
    auto stray_project = stray_mode["project"].toObject();
    stray_project["mode"] = "basic";
    stray_mode["project"] = stray_project;
    reject(bytes(stray_mode));
}

void comments_persist() {
    Fixture fixture;
    auto& editor = fixture.editor;
    const auto side = editor.project().relationships.at(fixture.supervises).participants.front().id;
    CHECK(editor.create_comment("Two remarks, one diagram.",
                                {CommentTarget{ElementRef{fixture.employee}}, CommentTarget{ConnectorRef{side}}}));
    CHECK(editor.create_comment("About the first word only.",
                                {CommentTarget{TextAnchor{ElementRef{fixture.employee}, TextField::Name, 0, 3}}}));
    const auto put_away = editor.project().comments.begin()->first;
    CHECK(editor.set_comment_hidden(put_away, true));

    const auto encoded = ErdxProjectStore::encode(editor.project());
    const auto root = QJsonDocument::fromJson(encoded).object();
    CHECK(root["format_version"].toInt() == 28);
    const auto written = root["project"].toObject()["comments"].toArray();
    CHECK(written.size() == 2);

    const auto reread = ErdxProjectStore::decode(encoded);
    CHECK(reread);
    CHECK(reread.project->comments == editor.project().comments);
    CHECK(*reread.project == editor.project());
    // Written again from what was read, the bytes are the same, so a project
    // kept in version control shows a change only where one was made.
    CHECK(ErdxProjectStore::encode(*reread.project) == encoded);

    // A remark pinned to a line and a remark pinned into writing must not be
    // confused for one another: an attribute identifier means one thing as an
    // element and another as the line that owns it.
    bool saw_element = false, saw_connector = false, saw_text = false;
    for (const auto& value : written)
        for (const auto& target : value.toObject()["targets"].toArray()) {
            const auto kind = target.toObject()["kind"].toString();
            saw_element = saw_element || kind == "element";
            saw_connector = saw_connector || kind == "connector";
            saw_text = saw_text || kind == "text";
        }
    CHECK(saw_element && saw_connector && saw_text);

    // Hostile documents are refused rather than half-read.
    auto broken = root;
    auto project = broken["project"].toObject();
    auto comments = project["comments"].toArray();
    auto first = comments[0].toObject();
    first["targets"] = QJsonArray{};
    comments[0] = first;
    project["comments"] = comments;
    broken["project"] = project;
    reject(bytes(broken));

    auto ranged = root;
    project = ranged["project"].toObject();
    comments = project["comments"].toArray();
    for (int i = 0; i < comments.size(); ++i) {
        auto entry = comments[i].toObject();
        auto targets = entry["targets"].toArray();
        for (int t = 0; t < targets.size(); ++t) {
            auto target = targets[t].toObject();
            if (target["kind"].toString() != "text") continue;
            target["length"] = 9999;
            targets[t] = target;
        }
        entry["targets"] = targets;
        comments[i] = entry;
    }
    project["comments"] = comments;
    ranged["project"] = project;
    reject(bytes(ranged));

    // A file written before version 16 carries no comments, and must not have
    // any invented for it.
    auto older = root;
    older["format_version"] = 15;
    auto older_project = older["project"].toObject();
    older_project.remove("comments");
    // A document claiming to be version 15 must look like one throughout: it
    // knew nothing of comments and nothing of what a model becomes.
    older_project.remove("mode");
    for (const char* group : {"entities", "attributes", "relationships"}) {
        QJsonArray kept;
        for (const auto& value : older_project[group].toArray()) {
            auto entry = value.toObject();
            for (const char* field : {"comment", "type", "length", "identifier", "required", "unique"})
                entry.remove(field);
            kept.append(entry);
        }
        older_project[group] = kept;
    }
    forget_bridge_keys(older_project);
    forget_project_description(older_project);
    forget_answered_sides(older_project);
    forget_schema_layout(older_project);
    forget_auto_increment(older_project);
    forget_schema_edits(older_project);
    forget_decisions(older_project);
    older["project"] = older_project;
    const auto from_older = ErdxProjectStore::decode(bytes(older));
    CHECK(from_older);
    CHECK(from_older.project->comments.empty());
}

int main() {
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"UUIDv7 and exact graph roundtrip", uuid_generation_and_roundtrip},
        {"incomplete draft save/open", incomplete_models_save_and_open},
        {"project description across the version boundary", project_description_persists_across_the_version_boundary},
        {"malformed JSON and Unicode", malformed_json_and_text},
        {"strict version and field contract", strict_version_and_field_contract},
        {"escaped field name decoding", escaped_field_names},
        {"connector shapes persist across versions", connector_shapes_persist_and_older_versions_still_open},
        {"pictures and notes persist", pictures_and_notes_persist},
        {"comments persist", comments_persist},
        {"legacy schema state migrates", legacy_schema_state_migrates},
        {"schema metadata persists", schema_metadata_persists},
        {"schema divergence persists", schema_divergence_persists},
        {"schema arrangement persists", schema_arrangement_persists},
        {"transparency persists", transparency_persists},
        {"weak entities and identifying relationships persist", weak_entities_and_identifying_relationships_persist},
        {"answered sides persist", answered_sides_persist},
        {"conversion decisions persist", conversion_decisions_persist},
        {"backgrounds persist", backgrounds_persist},
        {"invalid connector shapes", invalid_connector_shapes},
        {"invalid IDs, references, enums and layout", invalid_identifiers_references_and_enums},
        {"bounded input and structural depth", resource_limits},
        {"failed saves preserve destination", failed_saves_preserve_existing_destination},
        {"failed loads/saves preserve session", failed_load_and_save_preserve_session},
    };
    std::size_t failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures == 0 ? 0 : 1;
}
