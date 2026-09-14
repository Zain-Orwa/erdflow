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
    CHECK(copied.entities.size() == 2);
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
    for (const auto& version : {QJsonValue(0), QJsonValue(2), QJsonValue(1.5), QJsonValue("1"), QJsonValue(true)}) {
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
    auto duplicate = bytes(fixture.document());
    duplicate.replace("\"format_version\":1", "\"format_version\":2,\"format_version\":1");
    reject(duplicate);
    duplicate = bytes(fixture.document());
    duplicate.replace("\"name\":\"University design\"", "\"name\":\"discarded\",\"name\":\"University design\"");
    reject(duplicate);
    // Escaped property names are the same key after JSON unescaping.
    duplicate = bytes(fixture.document());
    duplicate.replace("\"format_version\":1", "\"format_version\":1,\"format_\\u0076ersion\":1");
    reject(duplicate);
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
