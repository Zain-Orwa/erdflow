#include "infrastructure/project_store.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <algorithm>
#include <functional>
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
    for (const auto& version : {QJsonValue(0), QJsonValue(11), QJsonValue(1.5), QJsonValue("1"), QJsonValue(true)}) {
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
    CHECK(root["format_version"].toInt() == 10);
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
        document["project"] = project;
        document["format_version"] = version;
        return document;
    };

    for (const int version : {1, 2, 3, 4, 5, 6, 7, 8, 9}) {
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
        CHECK(QJsonDocument::fromJson(ErdxProjectStore::encode(*opened.project)).object()["format_version"].toInt() == 10);
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
        oversized.entities.emplace(id, Entity{id, "Entity", std::string(max_description_bytes, 'x')});
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

    struct InvalidStore final : ProjectStore {
        Project candidate;
        LoadResult load(const std::string&) override { return {candidate, {}}; }
        SaveResult save(const std::string&, const Project&) override { return {false, "Injected save failure"}; }
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
    } changing_store(fixture.editor);
    CHECK(save_project(fixture.editor, changing_store, "unused"));
    CHECK(fixture.editor.dirty());
    CHECK(fixture.editor.project().name == "Edit during save");
}
} // namespace

int main() {
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"UUIDv7 and exact graph roundtrip", uuid_generation_and_roundtrip},
        {"incomplete draft save/open", incomplete_models_save_and_open},
        {"malformed JSON and Unicode", malformed_json_and_text},
        {"strict version and field contract", strict_version_and_field_contract},
        {"escaped field name decoding", escaped_field_names},
        {"connector shapes persist across versions", connector_shapes_persist_and_older_versions_still_open},
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
