// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "application/editor.hpp"
#include "domain/schema_preview.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <type_traits>

using namespace erdflow::domain;
using namespace erdflow::application;

namespace {
#define CHECK(condition) do { if (!(condition)) throw std::runtime_error(std::string(__func__) + ":" + std::to_string(__LINE__) + ": " #condition); } while (false)

struct TestIds final : IdGenerator {
    std::uint64_t counter = 1;
    bool fail = false;
    std::optional<Uuid> fixed;
    Uuid next() override {
        if (fail) throw std::runtime_error("Injected identifier failure");
        if (fixed) return *fixed;
        Uuid id;
        id.bytes[6] = 0x70;
        id.bytes[8] = 0x80;
        const auto value = counter++;
        for (unsigned i = 0; i < 7; ++i) id.bytes[15 - i] = static_cast<std::uint8_t>((value >> (8U * i)) & 0xffU);
        return id;
    }
};

EntityId entity(Editor& editor, const std::string& name = "Entity") {
    const auto result = editor.create_entity(name, {});
    CHECK(result);
    CHECK(result.created);
    return std::get<EntityId>(*result.created);
}
AttributeId attribute(Editor& editor, const std::string& name, std::optional<AttributeOwner> owner = {}) {
    const auto result = editor.create_attribute(name, {}, owner);
    CHECK(result);
    CHECK(result.created);
    return std::get<AttributeId>(*result.created);
}
RelationshipId relationship(Editor& editor, const std::string& name = "Relationship") {
    const auto result = editor.create_relationship(name, {});
    CHECK(result);
    CHECK(result.created);
    return std::get<RelationshipId>(*result.created);
}
ParticipantId connect(Editor& editor, RelationshipId rel, EntityId ent) {
    const auto result = editor.connect(rel, ent);
    CHECK(result);
    CHECK(result.participant);
    return *result.participant;
}
bool blocks(const Project& project) {
    const auto issues = validate(project);
    return std::any_of(issues.begin(), issues.end(), [](const auto& issue) { return issue.blocks_save; });
}
bool has_issue(const Project& project, const std::string& code) {
    const auto issues = validate(project);
    return std::any_of(issues.begin(), issues.end(), [&](const auto& issue) { return issue.code == code; });
}

// A failed graph edit must preserve the complete document and the pending redo.
void check_rejection_preserves_history(Editor& editor, const std::function<EditResult()>& edit) {
    const auto original = editor.project();
    const auto revision = editor.revision();
    const auto bytes = editor.history_bytes();
    const auto undo_label = editor.undo_label();
    const auto redo_label = editor.redo_label();
    const auto dirty = editor.dirty();
    const auto result = edit();
    CHECK(!result);
    CHECK(!result.error.empty());
    CHECK(editor.project() == original);
    CHECK(editor.revision() == revision);
    CHECK(editor.history_bytes() == bytes);
    CHECK(editor.undo_label() == undo_label);
    CHECK(editor.redo_label() == redo_label);
    CHECK(editor.dirty() == dirty);
    CHECK(editor.can_redo());
}

void identity_and_work_in_progress() {
    static_assert(!std::is_convertible_v<EntityId, RelationshipId>);
    TestIds ids;
    CHECK(ids.next().valid());
    CHECK(!Uuid{}.valid());
    auto invalid = ids.next();
    invalid.bytes[6] = 0x40;
    CHECK(!invalid.valid());
    invalid = ids.next();
    invalid.bytes[8] = 0;
    CHECK(!invalid.valid());
    Editor editor(ids);
    const auto ent = entity(editor, "");
    const auto attr = attribute(editor, "");
    const auto rel = relationship(editor, "");
    CHECK(!blocks(editor.project()));
    CHECK(has_issue(editor.project(), "name.missing"));
    CHECK(has_issue(editor.project(), "attribute.owner.missing"));
    CHECK(has_issue(editor.project(), "relationship.participants.incomplete"));
    CHECK(exists(editor.project(), ent));
    CHECK(name(editor.project(), attr).empty());
    CHECK(description(editor.project(), rel).empty());
    const auto snapshot = editor.project();
    CHECK(editor.replace_project(snapshot));
    CHECK(editor.project() == snapshot);
    CHECK(!editor.dirty());
}

void commands_and_stable_undo() {
    TestIds ids;
    Editor editor(ids);
    const auto initial = editor.project();
    const auto ent = entity(editor, "Student");
    CHECK(editor.dirty());
    CHECK(editor.rename(ent, "UniversityStudent"));
    CHECK(editor.describe(ent, "First line\nSecond line\tNotes"));
    CHECK(editor.move({{ElementRef{ent}, {120, -90, 160, 80}}}));
    CHECK(editor.project().entities.at(ent).id == ent);
    const auto finished = editor.project();
    CHECK(editor.undo_label() == "Move elements");
    CHECK(editor.undo());
    CHECK(editor.project().layout.at(ent).x == 0);
    CHECK(editor.undo());
    CHECK(editor.project().entities.at(ent).description.empty());
    CHECK(editor.undo());
    CHECK(editor.project().entities.at(ent).name == "Student");
    CHECK(editor.undo());
    CHECK(editor.project() == initial);
    CHECK(!editor.dirty());
    CHECK(!editor.undo());
    for (unsigned i = 0; i < 4; ++i) CHECK(editor.redo());
    CHECK(editor.project() == finished);
    CHECK(!editor.redo());
}

void project_description_is_a_bounded_undoable_edit() {
    TestIds ids;
    Editor editor(ids);
    const std::string prose(max_description_bytes, 'd');
    const auto before = editor.history_bytes();

    CHECK(editor.project().description.empty());
    CHECK(editor.describe_project(prose));
    CHECK(editor.project().description == prose);
    CHECK(editor.undo_label() == "Describe project");
    // The retained string is part of the undo budget, rather than an
    // unaccounted project-sized edit.
    CHECK(editor.history_bytes() >= before + prose.size());

    const auto revision = editor.revision();
    const auto history = editor.history_bytes();
    CHECK(editor.describe_project(prose)); // An exact no-op makes no history.
    CHECK(editor.revision() == revision);
    CHECK(editor.history_bytes() == history);

    CHECK(editor.undo());
    CHECK(editor.project().description.empty());
    CHECK(editor.redo_label() == "Describe project");
    check_rejection_preserves_history(editor, [&] {
        return editor.describe_project(std::string(max_description_bytes + 1, 'x'));
    });
    check_rejection_preserves_history(editor, [&] {
        return editor.describe_project(std::string("bad\0description", 15));
    });
    CHECK(editor.redo());
    CHECK(editor.project().description == prose);
}

void clean_state_branching_and_revisions() {
    TestIds ids;
    Editor editor(ids);
    const auto ent = entity(editor);
    const auto saved_revision = editor.revision();
    editor.mark_saved(saved_revision);
    CHECK(!editor.dirty());
    CHECK(editor.rename(ent, "After save"));
    editor.mark_saved(saved_revision);
    CHECK(editor.dirty());
    const auto before_undo = editor.revision();
    CHECK(editor.undo());
    CHECK(editor.revision() > before_undo);
    CHECK(!editor.dirty());
    CHECK(editor.can_redo());
    const auto branch_revision = editor.revision();
    CHECK(editor.rename(ent, "Entity")); // Exact no-op preserves redo.
    CHECK(editor.revision() == branch_revision);
    CHECK(editor.can_redo());
    CHECK(editor.rename(ent, "Branch"));
    CHECK(!editor.can_redo());
    CHECK(editor.dirty());
    CHECK(editor.undo());
    CHECK(!editor.dirty());
    CHECK(editor.redo());
    CHECK(editor.dirty());
    editor.mark_saved(editor.revision());
    CHECK(!editor.dirty());
    CHECK(editor.undo());
    CHECK(editor.dirty());
    CHECK(editor.redo());
    CHECK(!editor.dirty());
    const auto old_revision = editor.revision();
    editor.new_project();
    CHECK(editor.revision() > old_revision);
    CHECK(!editor.dirty());
    CHECK(!editor.can_undo());
    CHECK(!editor.can_redo());
    entity(editor);
    editor.mark_saved(old_revision);
    CHECK(editor.dirty());
}

void rejection_is_atomic() {
    TestIds ids;
    Editor editor(ids);
    const auto ent = entity(editor);
    CHECK(editor.rename(ent, "Temporary"));
    CHECK(editor.undo());
    editor.mark_saved(editor.revision());
    const auto original = editor.project();
    const auto revision = editor.revision();
    const auto bytes = editor.history_bytes();
    const auto redo_label = editor.redo_label();
    auto rejected = [&](const EditResult& result) {
        CHECK(!result);
        CHECK(!result.error.empty());
        CHECK(editor.project() == original);
        CHECK(editor.revision() == revision);
        CHECK(editor.history_bytes() == bytes);
        CHECK(editor.redo_label() == redo_label);
        CHECK(!editor.dirty());
    };
    rejected(editor.rename(ent, std::string(max_name_bytes + 1, 'x')));
    rejected(editor.rename(ent, std::string("bad\0name", 8)));
    rejected(editor.rename(ent, std::string("\xc0\x80", 2)));
    rejected(editor.rename(ent, std::string("\xed\xa0\x80", 3)));
    rejected(editor.rename(ent, std::string("\xf4\x90\x80\x80", 4)));
    rejected(editor.rename(ent, "Line\nBreak"));
    rejected(editor.describe(ent, std::string(max_description_bytes + 1, 'x')));
    rejected(editor.move({{ElementRef{ent}, {std::numeric_limits<double>::quiet_NaN(), 0, 160, 80}}}));
    rejected(editor.move({{ElementRef{ent}, {0, 0, std::numeric_limits<double>::infinity(), 80}}}));
    rejected(editor.move({{ElementRef{ent}, {0, 0, 0, 80}}}));
    rejected(editor.move({{ElementRef{ent}, {max_coordinate, 0, 160, 80}}}));
    const EntityId missing{ids.next()};
    rejected(editor.move({{ElementRef{ent}, {40, 20, 160, 80}}, {ElementRef{missing}, {}}}));
    rejected(editor.erase({ent, missing}));
    rejected(editor.duplicate({ent}, std::numeric_limits<double>::infinity(), 0));
    rejected(editor.create_attribute("Lost", {}, ElementRef{missing}));
    auto malformed = original;
    malformed.entities.at(ent).id = missing;
    rejected(editor.replace_project(malformed));
    ids.fail = true;
    rejected(editor.create_entity("Failure", {}));
    ids.fail = false;
    ids.fixed = ent.value;
    rejected(editor.create_entity("Reused identity", {}));
    ids.fixed.reset();
    CHECK(editor.rename(ent, "Étudiant 学生 طالب"));
}

void composite_ownership_and_kind_rules() {
    TestIds ids;
    Editor editor(ids);
    const auto ent = entity(editor);
    const auto rel = relationship(editor);
    const auto address = attribute(editor, "Address", ElementRef{ent});
    const auto street = attribute(editor, "Street");
    CHECK(!editor.set_attribute_owner(street, ElementRef{address}));
    CHECK(editor.set_attribute_kind(address, AttributeKind::Composite));
    CHECK(editor.set_attribute_owner(street, ElementRef{address}));
    CHECK(!editor.set_attribute_kind(address, AttributeKind::Normal));
    CHECK(editor.set_attribute_kind(street, AttributeKind::Composite));
    CHECK(!editor.set_attribute_owner(address, ElementRef{street}));
    CHECK(!editor.set_attribute_owner(address, ElementRef{address}));
    CHECK(!editor.set_attribute_kind(street, static_cast<AttributeKind>(100)));
    CHECK(editor.set_attribute_owner(street, {}));
    CHECK(editor.set_attribute_kind(address, AttributeKind::Key));
    CHECK(!editor.set_attribute_owner(address, ElementRef{rel}));
    CHECK(editor.set_attribute_kind(address, AttributeKind::Multivalued));
    CHECK(editor.set_attribute_owner(address, ElementRef{rel}));
    CHECK(editor.set_attribute_kind(address, AttributeKind::Derived));
    CHECK(!blocks(editor.project()));
}

void participants_recursive_roles_and_bounds() {
    TestIds ids;
    Editor editor(ids);
    const auto ent = entity(editor, "Employee");
    const auto rel = relationship(editor, "Supervises");
    const auto other = relationship(editor, "Other");
    const auto supervisor = connect(editor, rel, ent);
    const auto report = connect(editor, rel, ent);
    CHECK(supervisor != report);
    CHECK(has_issue(editor.project(), "relationship.recursive.roles"));
    CHECK(editor.update_participant(rel, supervisor, Cardinality::One, Participation::Partial, "Supervisor"));
    CHECK(editor.update_participant(rel, report, Cardinality::Many, Participation::Total, "Report"));
    CHECK(!has_issue(editor.project(), "relationship.recursive.roles"));
    const auto before = editor.project();
    CHECK(!editor.update_participant(other, supervisor, Cardinality::One, Participation::Total, "Wrong relationship"));
    CHECK(!editor.disconnect(other, report));
    CHECK(!editor.update_participant(rel, report, static_cast<Cardinality>(3), Participation::Total, "Report"));
    CHECK(!editor.update_participant(rel, report, Cardinality::Many, static_cast<Participation>(5), "Report"));
    CHECK(!editor.update_participant(rel, report, Cardinality::Many, Participation::Total, std::string(max_name_bytes + 1, 'x')));
    CHECK(editor.project() == before);
    CHECK(editor.disconnect(rel, report));
    CHECK(editor.project().relationships.at(rel).participants.size() == 1);
    CHECK(editor.undo());
    CHECK(editor.project() == before);

}

void deletion_restores_complete_graph() {
    TestIds ids;
    Editor editor(ids);
    const auto ent = entity(editor, "Student");
    const auto course = entity(editor, "Course");
    const auto rel = relationship(editor, "Enrollment");
    const auto ent_participant = connect(editor, rel, ent);
    connect(editor, rel, course);
    const auto address = attribute(editor, "Address", ElementRef{ent});
    CHECK(editor.set_attribute_kind(address, AttributeKind::Composite));
    const auto street = attribute(editor, "Street", ElementRef{address});
    const auto date = attribute(editor, "Date", ElementRef{rel});
    const auto before = editor.project();
    CHECK(editor.erase({ent, ent}));
    CHECK(!exists(editor.project(), ent));
    CHECK(!exists(editor.project(), address));
    CHECK(!exists(editor.project(), street));
    CHECK(exists(editor.project(), rel));
    CHECK(exists(editor.project(), date));
    CHECK(editor.project().relationships.at(rel).participants.size() == 1);
    CHECK(!blocks(editor.project()));
    CHECK(editor.undo());
    CHECK(editor.project() == before);
    CHECK(editor.redo());
    CHECK(editor.undo());
    CHECK(editor.erase({rel}));
    CHECK(!exists(editor.project(), date));
    CHECK(exists(editor.project(), ent));
    CHECK(editor.undo());
    CHECK(editor.project() == before);
    CHECK(editor.erase({ent}, {{rel, ent_participant}}, {date, address}));
    CHECK(!exists(editor.project(), address));
    CHECK(!editor.project().attributes.at(date).owner);
    CHECK(editor.undo());
    CHECK(editor.project() == before);
    CHECK(editor.erase({}, {{rel, ent_participant}}, {street}));
    CHECK(!editor.project().attributes.at(street).owner);
    CHECK(editor.project().relationships.at(rel).participants.size() == 1);
    CHECK(editor.undo());
    CHECK(editor.project() == before);
}

void duplicate_remaps_selected_subgraph() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const auto course = entity(editor, "Course");
    const auto rel = relationship(editor, "Enrollment");
    const auto p1 = connect(editor, rel, student);
    const auto p2 = connect(editor, rel, course);
    const auto address = attribute(editor, "Address", ElementRef{student});
    CHECK(editor.set_attribute_kind(address, AttributeKind::Composite));
    attribute(editor, "Street", ElementRef{address});
    attribute(editor, "Date", ElementRef{rel});
    const auto before = editor.project();
    const auto result = editor.duplicate({ElementRef{student}, ElementRef{rel}});
    CHECK(result);
    const auto duplicate_student = std::get<EntityId>(*result.created);
    CHECK(duplicate_student != student);
    CHECK(editor.project().entities.size() == 3);
    CHECK(editor.project().attributes.size() == 6);
    CHECK(editor.project().relationships.size() == 2);
    const auto copied_rel = std::find_if(editor.project().relationships.begin(), editor.project().relationships.end(), [&](const auto& entry) { return entry.first != rel; });
    CHECK(copied_rel != editor.project().relationships.end());
    CHECK(copied_rel->second.participants[0].target == ParticipantTarget{duplicate_student});
    CHECK(copied_rel->second.participants[1].target == ParticipantTarget{course});
    CHECK(copied_rel->second.participants[0].id != p1);
    CHECK(copied_rel->second.participants[1].id != p2);
    CHECK(editor.project().layout.at(duplicate_student).x == 32);
    const auto copied_address = std::find_if(editor.project().attributes.begin(), editor.project().attributes.end(), [&](const auto& entry) {
        return entry.second.name == "Address" && entry.second.owner == std::optional<AttributeOwner>{ElementRef{duplicate_student}};
    });
    CHECK(copied_address != editor.project().attributes.end());
    CHECK(std::any_of(editor.project().attributes.begin(), editor.project().attributes.end(), [&](const auto& entry) {
        return entry.second.name == "Street" && entry.second.owner == std::optional<AttributeOwner>{ElementRef{copied_address->first}};
    }));
    CHECK(std::any_of(editor.project().attributes.begin(), editor.project().attributes.end(), [&](const auto& entry) {
        return entry.second.name == "Date" && entry.second.owner == std::optional<AttributeOwner>{ElementRef{copied_rel->first}};
    }));
    CHECK(!blocks(editor.project()));
    const auto after = editor.project();
    CHECK(editor.undo());
    CHECK(editor.project() == before);
    CHECK(editor.redo());
    CHECK(editor.project() == after);
}

void hostile_models_are_rejected() {
    TestIds ids;
    Editor editor(ids);
    const auto ent = entity(editor);
    const auto rel = relationship(editor);
    const auto other = relationship(editor);
    connect(editor, rel, ent);
    connect(editor, other, ent);
    const auto original = editor.project();
    auto malformed = original;
    malformed.id.value = ent.value;
    CHECK(has_issue(malformed, "identity.duplicate"));
    CHECK(!editor.replace_project(malformed));
    malformed = original;
    malformed.relationships.at(other).participants.front().id = malformed.relationships.at(rel).participants.front().id;
    CHECK(has_issue(malformed, "identity.duplicate"));
    malformed = original;
    malformed.relationships.at(rel).participants.front().target = EntityId{ids.next()};
    CHECK(has_issue(malformed, "participant.entity.missing"));
    malformed = original;
    malformed.layout.erase(ent);
    CHECK(has_issue(malformed, "layout.element.missing"));
    malformed = original;
    malformed.layout.emplace(ElementRef{EntityId{ids.next()}}, Rect{});
    CHECK(has_issue(malformed, "layout.reference.missing"));
    CHECK(editor.project() == original);
}

// The schema half of the model was never asked whether it made sense. A column
// could be added to a table whose element was gone, a line could be shaped from
// a participant that no longer existed, and a table could be placed at
// infinity; validate() had no opinion about any of it, so all of it was written
// to the file and read back. It is asked the same questions the diagram is.
void hostile_schema_state_is_rejected() {
    TestIds ids;
    Editor editor(ids);
    const auto person = entity(editor, "Person");
    const auto name = std::get<AttributeId>(
        *editor.create_attribute("Name", {}, AttributeOwner{ElementRef{person}}).created);
    CHECK(editor.add_schema_column(ElementRef{person}, "Nickname"));
    CHECK(editor.move_schema_tables({{ElementRef{person}, Point{40, 40}}}));
    const auto original = editor.project();
    CHECK(validate(original).empty());

    // A column the schema added has to be added to something, and is held to
    // the same name, identity and type rules a drawn attribute is held to.
    auto malformed = original;
    malformed.schema.added[relation_from(ElementRef{EntityId{ids.next()}})] = malformed.schema.added.at(relation_from(ElementRef{person}));
    CHECK(has_issue(malformed, "schema.table.missing"));
    CHECK(!editor.replace_project(malformed));
    malformed = original;
    malformed.schema.added.at(relation_from(ElementRef{person})).front().name.clear();
    CHECK(has_issue(malformed, "schema.column.name.invalid"));
    malformed = original;
    malformed.schema.added.at(relation_from(ElementRef{person})).front().id = SchemaColumnId{person.value};
    CHECK(has_issue(malformed, "identity.duplicate"));
    malformed = original;
    {
        auto& column = malformed.schema.added.at(relation_from(ElementRef{person})).front();
        column.logical_type = LogicalType::Decimal;
        column.length = 4;
        column.scale = 9;
    }
    CHECK(has_issue(malformed, "schema.column.scale.invalid"));

    // An attribute the schema hides has to be an attribute the diagram has.
    malformed = original;
    malformed.schema.hidden.insert(AttributeId{ids.next()});
    CHECK(has_issue(malformed, "schema.hidden.missing"));

    // The arrangement is held to the bounds the diagram's layout is held to.
    malformed = original;
    malformed.schema_layout.tables[relation_from(ElementRef{EntityId{ids.next()}})] = Point{5, 5};
    CHECK(has_issue(malformed, "schema.layout.reference.missing"));
    malformed = original;
    malformed.schema_layout.tables.at(relation_from(ElementRef{person})) =
        Point{std::numeric_limits<double>::infinity(), 0};
    CHECK(has_issue(malformed, "schema.layout.bounds.invalid"));
    malformed = original;
    malformed.schema_layout.widths[relation_from(ElementRef{person})] = -30;
    CHECK(has_issue(malformed, "schema.layout.size.invalid"));

    // And a shaped line has to be drawn from a link that is still there.
    malformed = original;
    malformed.schema_layout.lines[foreign_key_from(LinkSource{ParticipantId{ids.next()}})] = SchemaLine{{Point{1, 1}}, {}, {}};
    CHECK(has_issue(malformed, "schema.line.reference.missing"));
    malformed = original;
    malformed.schema_layout.lines[foreign_key_from(LinkSource{name})] =
        SchemaLine{{Point{std::numeric_limits<double>::quiet_NaN(), 0}}, {}, {}};
    CHECK(has_issue(malformed, "schema.line.route.invalid"));
    malformed = original;
    malformed.schema_layout.lines[foreign_key_from(LinkSource{name})] = SchemaLine{{}, SchemaEnd{false, Point{1e12, 0}}, {}};
    CHECK(has_issue(malformed, "schema.line.end.invalid"));

    // Nothing hostile was let through, so the editor still holds what it held.
    CHECK(editor.project() == original);
}

// A column the schema added carries a persistent identity like everything else,
// so it must come from the guarded generator rather than the raw one, and it
// must be remembered when its project is opened. Taken raw, a column could be
// given an identity that was not a UUIDv7, or one already in use, and be stored
// either way; forgotten on open, its identity was free to be handed to a new
// element, leaving two things sharing one.
void schema_column_identities_are_guarded() {
    TestIds ids;
    Editor editor(ids);
    const auto person = entity(editor, "Person");
    const auto untouched = editor.project();
    auto refused = [&](const EditResult& result) {
        CHECK(!result);
        CHECK(!result.error.empty());
        CHECK(editor.project() == untouched);
    };

    // A generator stuck on an identity already in use, and one that hands back
    // something that is not a UUIDv7, are both refused where the identity is
    // asked for rather than stored and found to be wrong afterwards.
    ids.fixed = person.value;
    refused(editor.add_schema_column(ElementRef{person}, "Nickname"));
    ids.fixed = Uuid{};
    refused(editor.add_schema_column(ElementRef{person}, "Nickname"));
    ids.fixed.reset();
    CHECK(editor.add_schema_column(ElementRef{person}, "Nickname"));

    // Opening a project hands the guard the schema's identities too, so a
    // column's identity cannot afterwards be given to something new.
    const auto saved = editor.project();
    const auto column = saved.schema.added.at(relation_from(ElementRef{person})).front().id;
    Editor opened(ids);
    CHECK(opened.replace_project(saved));
    const auto reopened = opened.project();
    ids.fixed = column.value;
    const auto made = opened.create_entity("Course", {});
    CHECK(!made);
    CHECK(opened.project() == reopened);
    ids.fixed.reset();
}

// The conversion answers, the schema's own edits and its arrangement are the
// three fields history swaps whole rather than key by key, so each such delta
// retains a copy of the entire field beside the project's. That makes them the
// most expensive deltas there are, and they were the only ones the undo budget
// counted as costing nothing: a history of them could grow past the 32 MiB it
// is allowed without the budget ever noticing.
void whole_object_edits_cost_the_budget() {
    TestIds ids;
    Editor editor(ids);
    std::vector<ElementRef> drawn;
    for (std::size_t i = 0; i < 200; ++i)
        drawn.push_back(*editor.create_entity("Entity " + std::to_string(i), {}).created);
    const auto drawing = editor.history_bytes();
    for (std::size_t i = 0; i < drawn.size(); ++i)
        CHECK(editor.set_table_name(drawn[i], "A typed table name " + std::to_string(i)));
    const auto naming = editor.history_bytes() - drawing;
    // Two hundred answers, each delta holding all two hundred twice over, has
    // to cost more than two hundred entities with a name apiece. Uncounted,
    // these deltas cost less than the entities did.
    CHECK(naming > drawing);
    CHECK(editor.history_bytes() <= 32U * 1024U * 1024U);

    // The schema's own edits are held to the same accounting.
    Editor second(ids);
    const auto table = *second.create_entity("Person", {}).created;
    const auto before = second.history_bytes();
    for (std::size_t i = 0; i < 200; ++i)
        CHECK(second.add_schema_column(table, "Column " + std::to_string(i)));
    CHECK(second.history_bytes() - before > before);
    CHECK(second.history_bytes() <= 32U * 1024U * 1024U);

    // And a line given a long route costs what the route costs.
    Editor third(ids);
    const auto owner = std::get<EntityId>(*third.create_entity("Owner", {}).created);
    const auto child = std::get<AttributeId>(
        *third.create_attribute("Many", {}, AttributeOwner{ElementRef{owner}}).created);
    const auto plain = third.history_bytes();
    SchemaLine route;
    for (std::size_t i = 0; i < 500; ++i) route.route.push_back(Point{double(i), double(i)});
    CHECK(third.shape_schema_line(LinkSource{child}, route));
    CHECK(third.history_bytes() - plain > 500 * sizeof(Point));
}

// A comment carries a persistent identity like everything else, and the guard
// that refuses to reissue one can only refuse what it was told was issued.
// Left out when a project is opened, a comment's identity was forgotten and
// free to be handed to a new element.
void comment_identities_survive_opening() {
    TestIds ids;
    Editor editor(ids);
    const auto person = entity(editor, "Person");
    CHECK(editor.create_comment("Check this", {CommentTarget{ElementRef{person}}}));
    const auto saved = editor.project();
    const auto remark = saved.comments.begin()->first;

    Editor opened(ids);
    CHECK(opened.replace_project(saved));
    const auto reopened = opened.project();
    ids.fixed = remark.value;
    CHECK(!opened.create_entity("Course", {}));
    CHECK(opened.project() == reopened);
    ids.fixed.reset();
}

// A name typed on the schema is the name on the diagram. The two are views of
// one model, so a rename travels to whatever the thing was made from rather
// than being recorded as a difference beside it -- which would leave the two
// levels disagreeing about what something is called. Only a key the conversion
// invented has nothing behind it to rename, and is given a name of its own.
// The primary key, set from the schema, reaches the diagram. Zain's rule is
// that nothing on the schema is read-only because it was derived, and that a
// change made there shows on the conceptual ERD; the key is the case that
// proves it, because it is both a rule the table enforces and a shape the
// diagram draws.
void schema_keys_reach_the_diagram() {
    TestIds ids;
    Editor editor(ids);
    const auto person = entity(editor, "Person");
    const auto owner = AttributeOwner{ElementRef{person}};
    const auto code = std::get<AttributeId>(*editor.create_attribute("Code", {}, owner).created);
    const auto email = std::get<AttributeId>(*editor.create_attribute("Email", {}, owner).created);

    const auto key_of = [&](const std::string& column) {
        for (const auto& table : schema_preview(editor.project()).tables)
            for (const auto& one : table.columns)
                if (one.name == column) return one.primary_key;
        return false;
    };

    // Nothing identifies the entity yet, so neither column is the key.
    CHECK(!key_of("Code"));
    CHECK(!key_of("Email"));

    // Making one the key from the schema sets the rule the table enforces and
    // draws the attribute as a key on the diagram, in one edit.
    CHECK(editor.set_primary_key(code, true));
    CHECK(key_of("Code"));
    CHECK(editor.project().attributes.at(code).identifier);
    CHECK(editor.project().attributes.at(code).kind == AttributeKind::Key);
    // A key is never empty, so it was made required in the same edit.
    CHECK(editor.project().attributes.at(code).required);

    // One step of history takes the whole of it back, rather than leaving the
    // oval drawn with the rule undone or the other way about.
    CHECK(editor.undo());
    CHECK(!key_of("Code"));
    CHECK(!editor.project().attributes.at(code).identifier);
    CHECK(editor.project().attributes.at(code).kind == AttributeKind::Normal);
    CHECK(editor.redo());

    // The key can be put out and put on another column, which is what moving
    // it means: two deliberate acts rather than one that guesses.
    CHECK(editor.set_primary_key(code, false));
    CHECK(!key_of("Code"));
    CHECK(editor.project().attributes.at(code).kind == AttributeKind::Normal);
    CHECK(editor.set_primary_key(email, true));
    CHECK(key_of("Email"));

    // What cannot be a key says so rather than being quietly reshaped.
    const auto address = std::get<AttributeId>(*editor.create_attribute("Address", {}, owner).created);
    CHECK(editor.set_attribute_kind(address, AttributeKind::Composite));
    CHECK(!editor.set_primary_key(address, true).ok);
    CHECK(editor.project().attributes.at(address).kind == AttributeKind::Composite);
}

// The constraints a row carries, changed from the schema. Nullability on an
// ordinary column is the attribute's own rule; on a foreign key it is not a
// fact about the column at all but about the relationship's participation, so
// changing it there changes the diagram.
void schema_constraints_reach_the_diagram() {
    TestIds ids;
    Editor editor(ids);
    const auto person = entity(editor, "Person");
    const auto city = entity(editor, "City");
    const auto owner = AttributeOwner{ElementRef{person}};
    const auto email = std::get<AttributeId>(*editor.create_attribute("Email", {}, owner).created);
    const auto made = editor.relate(person, city, {}, "lives in");
    CHECK(made);
    const auto lives_in = std::get<RelationshipId>(*made.created);
    // Person many, City one, so the foreign key lands on Person and points at
    // City. Its nullability is City's participation.
    CHECK(editor.set_ratio(lives_in, Cardinality::Many, Cardinality::One));

    const auto column_named = [&](const std::string& name) -> std::optional<PreviewColumn> {
        for (const auto& table : schema_preview(editor.project()).tables)
            for (const auto& one : table.columns)
                if (one.name == name) return one;
        return std::nullopt;
    };

    // An ordinary column's rules are the attribute's own, and setting them
    // from the schema sets them on the attribute.
    CHECK(!column_named("Email")->required);
    CHECK(!column_named("Email")->unique);
    CHECK(editor.set_attribute_rules(email, false, true, true));
    CHECK(column_named("Email")->required);
    CHECK(column_named("Email")->unique);
    CHECK(editor.project().attributes.at(email).required);

    // The foreign key on Person. The side it points at starts partial, so it
    // may be empty, and it remembers which participant put it there.
    const auto foreign_key_now = [&]() -> std::optional<PreviewColumn> {
        for (const auto& table : schema_preview(editor.project()).tables)
            for (const auto& one : table.columns) if (one.foreign_key) return one;
        return std::nullopt;
    };
    const auto foreign = foreign_key_now();
    CHECK(foreign.has_value());
    CHECK(!foreign->required);
    CHECK(foreign->link.has_value());
    CHECK(std::holds_alternative<ParticipantId>(*foreign->link));

    // Saying it may not be empty says the side it points at is total, and the
    // diagram is where that fact lives.
    const auto side = std::get<ParticipantId>(*foreign->link);
    CHECK(editor.set_participation(side, Participation::Total));
    const auto& sides = editor.project().relationships.at(lives_in).participants;
    const auto found = std::find_if(sides.begin(), sides.end(),
                                    [&](const auto& one) { return one.id == side; });
    CHECK(found != sides.end());
    CHECK(found->participation == Participation::Total);
    // And it was answered, not merely left at a value that happens to match.
    CHECK(found->participation_confirmed);
    // The cardinality was not what was said, so it keeps what it had.
    CHECK(found->maximum == Cardinality::One);

    CHECK(foreign_key_now()->required);

    // And back, in one step of history.
    CHECK(editor.undo());
    CHECK(!foreign_key_now()->required);
}

// Counting a column up. The one constraint with nothing on the diagram behind
// it, and the one that refuses: a generated column of the wrong type is SQL
// that will not run, so it is refused with a reason rather than accepted and
// left to fail later.
void columns_can_count_themselves_up() {
    TestIds ids;
    Editor editor(ids);
    const auto person = entity(editor, "Person");
    const auto owner = AttributeOwner{ElementRef{person}};
    const auto id = std::get<AttributeId>(*editor.create_attribute("Id", {}, owner).created);
    const auto name = std::get<AttributeId>(*editor.create_attribute("Name", {}, owner).created);

    const auto counting = [&](const std::string& column) {
        for (const auto& table : schema_preview(editor.project()).tables)
            for (const auto& one : table.columns)
                if (one.name == column) return one.auto_increment;
        return false;
    };

    // A column with no type yet is asked for one first: there is no telling
    // whether it could count up until somebody says what it holds.
    CHECK(!editor.set_auto_increment(id, true).ok);

    CHECK(editor.set_logical_type(id, LogicalType::BigInt));
    CHECK(editor.set_auto_increment(id, true));
    CHECK(counting("Id"));
    // What the database fills in is never empty, so it was made required too.
    CHECK(editor.project().attributes.at(id).required);

    // Text cannot count up, and says so.
    CHECK(editor.set_logical_type(name, LogicalType::NVarchar, 40));
    const auto refused = editor.set_auto_increment(name, true);
    CHECK(!refused.ok);
    CHECK(!refused.error.empty());
    CHECK(!counting("Name"));

    // A decimal can, but only where it keeps no digits after the point.
    const auto tally = std::get<AttributeId>(*editor.create_attribute("Tally", {}, owner).created);
    CHECK(editor.set_logical_type(tally, LogicalType::Decimal));
    CHECK(editor.set_type_size(tally, 10, 2));
    CHECK(!editor.set_auto_increment(tally, true).ok);
    CHECK(editor.set_type_size(tally, 10, 0));
    CHECK(editor.set_auto_increment(tally, true));
    CHECK(counting("Tally"));

    // And it undoes like anything else.
    CHECK(editor.undo());
    CHECK(!counting("Tally"));
}

// A key the conversion invented can count itself up, and a one-to-one key
// reads as unique. Both are columns with no attribute behind them, which is
// exactly where pressing a constraint used to do nothing at all.
void invented_keys_carry_constraints() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const auto card = entity(editor, "Card");
    const auto made = editor.relate(student, card, {}, "holds");
    CHECK(made);
    const auto holds = std::get<RelationshipId>(*made.created);

    const auto column_named = [&](const std::string& name) -> std::optional<PreviewColumn> {
        for (const auto& table : schema_preview(editor.project()).tables)
            for (const auto& one : table.columns)
                if (one.name == name) return one;
        return std::nullopt;
    };

    // Nothing identifies either entity, so each table is given a key of its
    // own. It has no attribute behind it and no identity of its own.
    const auto key = column_named("StudentID");
    CHECK(key.has_value());
    CHECK(key->primary_key);
    CHECK(key->origin_kind == ColumnOrigin::Generated);
    CHECK(!key->origin.has_value());
    CHECK(!key->added.has_value());
    CHECK(!key->auto_increment);

    // It is remembered against the table it belongs to, as its name is.
    CHECK(editor.set_key_auto_increment(ElementRef{student}, true));
    CHECK(column_named("StudentID")->auto_increment);
    CHECK(editor.undo());
    CHECK(!column_named("StudentID")->auto_increment);
    CHECK(editor.redo());
    CHECK(column_named("StudentID")->auto_increment);
    // And it survives the key being renamed, being keyed by the table.
    CHECK(editor.rename_schema_key(ElementRef{student}, "Matric"));
    CHECK(column_named("Matric")->auto_increment);

    // A one-to-one foreign key holds one row and no more, which is what
    // unique says. It is read from the relationship's shape, not typed.
    CHECK(editor.set_ratio(holds, Cardinality::One, Cardinality::One));
    const auto foreign = [&]() -> std::optional<PreviewColumn> {
        for (const auto& table : schema_preview(editor.project()).tables)
            for (const auto& one : table.columns) if (one.foreign_key) return one;
        return std::nullopt;
    };
    CHECK(foreign().has_value());
    CHECK(foreign()->unique);
    // Made one-to-many, and it is no longer unique.
    CHECK(editor.set_ratio(holds, Cardinality::Many, Cardinality::One));
    CHECK(!foreign()->unique);

    // Which is the other way round too: saying the key is unique from the
    // schema says the side carrying it sees one row, and that is the
    // diagram's cardinality.
    const auto side = *foreign()->link;
    CHECK(std::holds_alternative<ParticipantId>(side));
    const auto& sides = editor.project().relationships.at(holds).participants;
    CHECK(sides.size() == 2);
    const auto carrier = sides[0].id == std::get<ParticipantId>(side) ? sides[1].id : sides[0].id;
    CHECK(editor.set_cardinality(carrier, Cardinality::One));
    CHECK(foreign()->unique);
    const auto& after = editor.project().relationships.at(holds).participants;
    const auto found = std::find_if(after.begin(), after.end(),
                                    [&](const auto& one) { return one.id == carrier; });
    CHECK(found != after.end());
    CHECK(found->maximum == Cardinality::One);
    CHECK(found->cardinality_confirmed);
}

// A relation has an identity of its own, derived from where it came from.
//
// ADR-008 forbids a level reusing the identity of the level above it, and
// ADR-001 forbids identity that moves. These are the two properties that must
// hold together, and the second is what everything kept about a relation --
// its place, its width, its typed name, the columns added to it -- depends on.
void relations_have_their_own_identity() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const auto course = entity(editor, "Course");
    const auto made = editor.relate(student, course, {}, "takes");
    CHECK(made);
    const auto takes = std::get<RelationshipId>(*made.created);

    // Its own value, of its own type, and never the conceptual one.
    const auto relation = relation_from(ElementRef{student});
    CHECK(relation.value != student.value);
    CHECK(relation.value.valid());
    // The same every time it is asked, which is what lets a file keep things
    // against it.
    CHECK(relation_from(ElementRef{student}) == relation);
    // And different for different elements, and for different kinds.
    CHECK(relation_from(ElementRef{course}) != relation);
    CHECK(relation_from(ElementRef{takes}) != relation);

    // It does not move when a conversion decision is answered differently. An
    // identity that did would orphan the layout the moment somebody chose a
    // different strategy, which is why the rule is provenance and not identity.
    CHECK(editor.set_ratio(takes, Cardinality::Many, Cardinality::Many));
    CHECK(relation_from(ElementRef{student}) == relation);
    CHECK(editor.set_table_naming(TableNaming::AsDrawn));
    CHECK(relation_from(ElementRef{student}) == relation);

    // The preview hands out the identity and says where the relation came
    // from, including which rule made it.
    const auto preview = schema_preview(editor.project());
    bool found_entity = false;
    bool found_bridge = false;
    for (const auto& table : preview.tables) {
        CHECK(table.id.value.valid());
        CHECK(table.provenance.has_value());
        if (table.provenance->source == ElementRef{student}) {
            found_entity = true;
            CHECK(table.id == relation);
            CHECK(table.provenance->rule == ConversionRule::EntityToRelation);
        }
        if (table.provenance->source == ElementRef{takes}) {
            found_bridge = true;
            CHECK(table.provenance->rule == ConversionRule::ManyToManyToBridge);
        }
    }
    CHECK(found_entity);
    CHECK(found_bridge);

    // Two relations never share an identity.
    std::set<RelationId> seen;
    for (const auto& table : preview.tables) CHECK(seen.insert(table.id).second);

    // A foreign key has one of its own for the same reason.
    std::set<ForeignKeyId> keys;
    for (const auto& table : preview.tables)
        for (const auto& column : table.columns)
            if (column.link) {
                CHECK(column.key_id.has_value());
                CHECK(*column.key_id == foreign_key_from(*column.link));
                CHECK(keys.insert(*column.key_id).second);
            }
    CHECK(!keys.empty());

    // What is kept against a relation is still found after the project has
    // been through the model, which is the whole point of the identity.
    CHECK(editor.set_table_name(ElementRef{student}, "Pupils"));
    CHECK(editor.project().decisions.table_name.at(relation) == "Pupils");
    CHECK(editor.erase({ElementRef{student}}));
    CHECK(!editor.project().decisions.table_name.contains(relation));
}

void schema_names_reach_the_diagram() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const auto course = entity(editor, "Course");
    const auto title = std::get<AttributeId>(
        *editor.create_attribute("Title", {}, AttributeOwner{ElementRef{course}}).created);
    CHECK(editor.relate(student, course, {}, "takes"));

    const auto named = [&](const std::string& table, const std::string& column) {
        for (const auto& one : schema_preview(editor.project()).tables) {
            if (one.name != table) continue;
            for (const auto& value : one.columns) if (value.name == column) return true;
        }
        return false;
    };

    // Nothing identifies the entity, so the conversion makes a key and names it
    // after the table. The name is a guess and can be taken back, and every
    // foreign key pointing at that table follows it.
    CHECK(named("Students", "StudentID"));
    CHECK(named("takes", "StudentID"));
    CHECK(editor.rename_schema_key(ElementRef{student}, "MatricNo"));
    CHECK(named("Students", "MatricNo"));
    // The foreign key is named for the table as well, since a bare key name
    // could point anywhere (Zain, 2026-09-24).
    CHECK(named("takes", "StudentMatricNo"));
    CHECK(editor.rename_schema_key(ElementRef{student}, ""));
    CHECK(named("Students", "StudentID"));
    CHECK(editor.rename_schema_key(ElementRef{student}, "MatricNo"));

    // Renaming the table renames the entity it came from.
    CHECK(editor.rename_table(ElementRef{student}, "Pupil"));
    CHECK(editor.project().entities.at(student).name == "Pupil");
    CHECK(named("Pupils", "MatricNo"));

    // Renaming a derived column renames its attribute.
    CHECK(editor.rename(ElementRef{title}, "CourseTitle"));
    CHECK(editor.project().attributes.at(title).name == "CourseTitle");
    CHECK(named("Courses", "CourseTitle"));

    // A name typed over the derived one is given up when the table is renamed,
    // or it would go on masking the name just chosen.
    CHECK(editor.set_table_name(ElementRef{course}, "Modules"));
    CHECK(named("Modules", "CourseTitle"));
    CHECK(editor.rename_table(ElementRef{course}, "Unit"));
    CHECK(editor.project().decisions.table_name.empty());
    CHECK(named("Units", "CourseTitle"));
    CHECK(named("Units", "UnitID"));

    // One undo takes back the rename and the name it gave up together.
    CHECK(editor.undo());
    CHECK(editor.project().entities.at(course).name == "Course");
    CHECK(named("Modules", "CourseTitle"));

    // A key name belongs to an element that is still there, and goes with it.
    CHECK(editor.erase({ElementRef{student}}, {}, {}, {}));
    CHECK(editor.project().schema.key_names.empty());
    auto malformed = editor.project();
    malformed.schema.key_names[relation_from(ElementRef{EntityId{ids.next()}})] = "Orphan";
    CHECK(has_issue(malformed, "schema.key_name.missing"));
    malformed = editor.project();
    malformed.schema.key_names[relation_from(ElementRef{course})] = std::string(max_name_bytes + 1, 'x');
    CHECK(has_issue(malformed, "schema.key_name.invalid"));
}

void limits_deep_ownership_and_compact_history() {
    TestIds ids;
    Editor editor(ids);
    Project large;
    large.id = ProjectId{ids.next()};
    EntityId first;
    for (std::size_t i = 0; i < max_elements; ++i) {
        const EntityId id{ids.next()};
        if (i == 0) first = id;
        large.entities.emplace(id, Entity{.id = id, .name = "Entity"});
        large.layout.emplace(ElementRef{id}, Rect{});
    }
    CHECK(editor.replace_project(std::move(large)));
    CHECK(!editor.create_entity("Over limit", {}));
    CHECK(editor.project().entities.size() == max_elements);
    CHECK(editor.rename(first, "Renamed"));
    CHECK(editor.history_bytes() < 4096); // No whole-project history snapshot.
    CHECK(editor.undo());
    CHECK(!editor.dirty());

    Project nested;
    nested.id = ProjectId{ids.next()};
    std::optional<AttributeOwner> owner;
    AttributeId first_attribute;
    AttributeId last_attribute;
    for (std::size_t i = 0; i < 2000; ++i) {
        const AttributeId id{ids.next()};
        if (i == 0) first_attribute = id;
        last_attribute = id;
        nested.attributes.emplace(id, Attribute{.id = id, .name = "Composite", .kind = AttributeKind::Composite, .owner = owner});
        nested.layout.emplace(ElementRef{id}, Rect{});
        owner = ElementRef{id};
    }
    CHECK(!blocks(nested));
    nested.attributes.at(first_attribute).owner = ElementRef{last_attribute};
    CHECK(has_issue(nested, "attribute.owner.cycle"));
    CHECK(blocks(nested));

    editor.new_project();
    const auto ent = entity(editor);
    editor.mark_saved(editor.revision());
    std::string text(max_description_bytes, 'a');
    for (std::size_t i = 0; i < 1400; ++i) {
        text.front() = i % 2 == 0 ? 'a' : 'b';
        CHECK(editor.describe(ent, text));
        CHECK(editor.history_bytes() <= 32U * 1024U * 1024U);
    }
    std::size_t undos = 0;
    while (editor.can_undo()) { CHECK(editor.undo()); ++undos; }
    CHECK(undos > 0);
    CHECK(undos < 1400);
    CHECK(editor.dirty()); // The clean state has been evicted, never falsely clean.
}
// A connector shape is layout state attached to the link that draws it, so it
// must follow that link through undo, detach, delete and duplicate.
void connector_shapes_follow_their_link() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const auto course = entity(editor, "Course");
    const auto enrolled = relationship(editor, "Enrolled");
    const auto side = connect(editor, enrolled, student);
    connect(editor, enrolled, course);
    const auto grade = attribute(editor, "Grade", AttributeOwner{ElementRef{enrolled}});

    // Bending is stored per connector; absence means automatic routing.
    CHECK(editor.project().connectors.empty());
    CHECK(editor.bend_connector(ConnectorRef{side}, 40));
    CHECK(editor.project().connectors.at(ConnectorRef{side}).offset == 40);
    CHECK(editor.bend_connector(ConnectorRef{grade}, -25));
    CHECK(editor.project().connectors.size() == 2);

    // Passing no offset restores automatic routing rather than storing zero.
    CHECK(editor.bend_connector(ConnectorRef{grade}, {}));
    CHECK(!editor.project().connectors.contains(ConnectorRef{grade}));
    CHECK(editor.undo());
    CHECK(editor.project().connectors.at(ConnectorRef{grade}).offset == -25);

    // Re-bending the same connector to its current shape is not an edit.
    const auto revision = editor.revision();
    CHECK(editor.bend_connector(ConnectorRef{side}, 40));
    CHECK(editor.revision() == revision);

    // A shape cannot outlive its link, and undo restores both together.
    CHECK(editor.set_attribute_owner(grade, {}));
    CHECK(!editor.project().connectors.contains(ConnectorRef{grade}));
    CHECK(!blocks(editor.project()));
    CHECK(editor.undo());
    CHECK(editor.project().connectors.at(ConnectorRef{grade}).offset == -25);

    CHECK(editor.disconnect(enrolled, side));
    CHECK(!editor.project().connectors.contains(ConnectorRef{side}));
    CHECK(!blocks(editor.project()));
    CHECK(editor.undo());
    CHECK(editor.project().connectors.at(ConnectorRef{side}).offset == 40);

    // Deleting the relationship drops every participant shape it drew.
    CHECK(editor.erase({ElementRef{enrolled}}));
    CHECK(editor.project().connectors.empty());
    CHECK(!blocks(editor.project()));
    CHECK(editor.undo());
    CHECK(editor.project().connectors.size() == 2);

    // A copy keeps the shape under its own new connector identity.
    const auto copy = editor.duplicate({ElementRef{enrolled}});
    CHECK(copy);
    const auto& copied = editor.project().relationships.at(std::get<RelationshipId>(*copy.created));
    CHECK(copied.participants.size() == 2);
    std::size_t carried = 0;
    for (const auto& participant : copied.participants) {
        CHECK(participant.id != side);
        if (editor.project().connectors.contains(ConnectorRef{participant.id})) {
            CHECK(editor.project().connectors.at(ConnectorRef{participant.id}).offset == 40);
            ++carried;
        }
    }
    CHECK(carried == 1);
    CHECK(!blocks(editor.project()));
}

// A link can be given its shape as it is made, in the same edit as the link,
// so a connection drawn by clicking two points is one step to undo.
void connections_can_be_pinned_as_they_are_made() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const auto enrolled = relationship(editor, "Enrolled");
    Connector shape;
    shape.owner_anchor = 0.5;
    shape.child_anchor = -2.0;
    const auto side = editor.connect(enrolled, student, shape);
    CHECK(side && side.participant);
    CHECK(editor.project().connectors.at(ConnectorRef{*side.participant}) == shape);
    CHECK(editor.undo_label() == "Connect participant");
    CHECK(editor.undo());
    CHECK(editor.project().connectors.empty());
    CHECK(editor.project().relationships.at(enrolled).participants.empty());
    CHECK(editor.redo());
    CHECK(editor.project().connectors.size() == 1);
    // An automatic shape stores nothing, as the plain call always did.
    const auto plain = editor.connect(enrolled, student, Connector{});
    CHECK(plain && plain.participant);
    CHECK(!editor.project().connectors.contains(ConnectorRef{*plain.participant}));

    // The same for an attribute's link, pinned at either end or both.
    const auto born = attribute(editor, "Born");
    Connector link;
    link.child_anchor = 1.0;
    CHECK(editor.set_attribute_owner(born, AttributeOwner{ElementRef{student}}, link));
    CHECK(editor.project().connectors.at(ConnectorRef{born}) == link);
    CHECK(editor.undo_label() == "Change attribute owner");
    CHECK(editor.undo());
    CHECK(!editor.project().attributes.at(born).owner);
    CHECK(!editor.project().connectors.contains(ConnectorRef{born}));
    // Detaching has no link to pin, so a shape given with it is not stored.
    CHECK(editor.redo());
    CHECK(editor.set_attribute_owner(born, std::nullopt, link));
    CHECK(!editor.project().attributes.at(born).owner);
    CHECK(editor.project().connectors.size() == 1);
    CHECK(!blocks(editor.project()));

    // An attribute placed on its owner can be pinned as it is made, in the
    // same step of history, which is how the attributes placed on a locked
    // owner all leave it from one point.
    Connector exit;
    exit.owner_anchor = -1.2;
    const auto placed = editor.create_attribute("Grade", {0, -200, 150, 60}, AttributeOwner{ElementRef{student}}, exit);
    CHECK(placed && placed.created);
    const auto grade = std::get<AttributeId>(*placed.created);
    CHECK(editor.project().connectors.at(ConnectorRef{grade}) == exit);
    CHECK(editor.undo_label() == "Create attribute");
    CHECK(editor.undo());
    CHECK(!editor.project().attributes.contains(grade));
    CHECK(!editor.project().connectors.contains(ConnectorRef{grade}));
    // With no owner there is no link, so the shape is not stored.
    const auto alone = editor.create_attribute("Alone", {0, 200, 150, 60}, std::nullopt, exit);
    CHECK(alone && !editor.project().connectors.contains(ConnectorRef{std::get<AttributeId>(*alone.created)}));
    CHECK(!blocks(editor.project()));
}

// A picture and a note are placed elements without being database objects:
// named, described, moved, coloured, copied and deleted through the same
// commands as everything else, owning nothing and connected to nothing.
// The first time an entity, relationship or attribute is made another size by
// hand, the size it had is kept as the size its name is drawn for, and the
// name follows the box from there (Zain, 2026-09-26). Nothing else keeps one.
void lettering_follows_resizing_by_hand() {
    // The smaller of the two changes, held between a quarter and sixteen.
    const LetteringBase base{148, 86};
    CHECK(lettering_factor(base, 148, 86) == 1.0);
    CHECK(lettering_factor(base, 296, 86) == 1.0);   // wider only: room, not size
    CHECK(lettering_factor(base, 296, 172) == 2.0);
    CHECK(lettering_factor(base, 74, 86) == 0.5);
    CHECK(lettering_factor(base, 1, 1) == 0.25);
    CHECK(lettering_factor(base, 100000, 100000) == 16.0);

    TestIds ids;
    Editor editor(ids);
    const ElementRef student{entity(editor, "Student")};
    const ElementRef mentor{relationship(editor, "Mentor")};
    const ElementRef born{attribute(editor, "Born")};
    const Rect was{0, 0, 160, 80};
    CHECK(editor.project().layout.at(student) == was);
    CHECK(editor.project().lettering.empty());

    // The first resize keeps the size it started from, in the same edit.
    CHECK(editor.resize_entities({{student, {0, 0, 320, 160}}}));
    CHECK(editor.project().lettering.at(student) == (LetteringBase{160, 80}));
    // A second keeps the first's, so the name follows the box both ways.
    CHECK(editor.resize_entities({{student, {0, 0, 240, 120}}}));
    CHECK(editor.project().lettering.at(student) == (LetteringBase{160, 80}));
    CHECK(editor.undo());
    CHECK(editor.undo());
    CHECK(!editor.project().lettering.contains(student));
    CHECK(editor.redo());

    // Relationships and attributes alike.
    CHECK(editor.resize_relationships({{mentor, {0, 0, 380, 220}}}));
    CHECK(editor.resize_attributes({{born, {0, 0, 300, 120}}}));
    CHECK(editor.project().lettering.at(mentor) == (LetteringBase{160, 80}));
    CHECK(editor.project().lettering.at(born) == (LetteringBase{160, 80}));

    // Moving keeps no size; giving a new size through Properties does.
    const ElementRef course{entity(editor, "Course")};
    CHECK(editor.move({{course, {40, 40, 160, 80}}}));
    CHECK(!editor.project().lettering.contains(course));
    CHECK(editor.move({{course, {40, 40, 200, 100}}}));
    CHECK(editor.project().lettering.at(course) == (LetteringBase{160, 80}));

    // A copy draws its name as the original does; a deletion takes it away.
    const auto copied = editor.duplicate({student});
    CHECK(copied && copied.created);
    CHECK(editor.project().lettering.at(*copied.created) == editor.project().lettering.at(student));
    CHECK(editor.erase({student}));
    CHECK(!editor.project().lettering.contains(student));
    CHECK(!blocks(editor.project()));
    CHECK(editor.undo());
    CHECK(editor.project().lettering.contains(student));

    // Only those three have lettering of their own.
    auto project = editor.project();
    const auto note = editor.create_note("Note", {});
    CHECK(note && note.created);
    project = editor.project();
    project.lettering.emplace(*note.created, LetteringBase{200, 120});
    CHECK(has_issue(project, "lettering.kind"));
    project = editor.project();
    project.lettering.at(course) = LetteringBase{0, 80};
    CHECK(has_issue(project, "lettering.invalid"));
}

void pictures_and_notes_are_placed_like_elements() {
    TestIds ids;
    Editor editor(ids);
    const std::vector<std::uint8_t> png{0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a, 0, 0, 0, 13};
    const auto placed = editor.create_picture("Map", {10, 20, 200, 120}, png);
    CHECK(placed && placed.created && std::holds_alternative<PictureId>(*placed.created));
    const auto picture = std::get<PictureId>(*placed.created);
    CHECK(editor.undo_label() == "Insert picture");
    CHECK(editor.project().pictures.at(picture).image == png);
    CHECK((editor.project().layout.at(*placed.created) == Rect{10, 20, 200, 120}));
    const auto noted = editor.create_note("Assumptions", {300, 20, 200, 120}, "Each student enrols each term.");
    CHECK(noted && noted.created);
    const auto note = std::get<NoteId>(*noted.created);
    CHECK(editor.undo_label() == "Insert note");
    CHECK(name(editor.project(), *noted.created) == "Assumptions");
    CHECK(description(editor.project(), *noted.created) == "Each student enrols each term.");

    CHECK(editor.rename(*placed.created, "Campus map"));
    CHECK(editor.project().pictures.at(picture).name == "Campus map");
    CHECK(editor.describe(*noted.created, "Grades are per enrolment."));
    CHECK(editor.project().notes.at(note).description == "Grades are per enrolment.");
    CHECK(editor.move({{*placed.created, {40, 60, 200, 120}}}));
    CHECK((editor.project().layout.at(*placed.created) == Rect{40, 60, 200, 120}));
    CHECK(editor.recolour({*noted.created}, Colour{255, 224, 138}));
    CHECK((editor.project().colours.at(*noted.created) == Colour{255, 224, 138}));
    const auto copy = editor.duplicate({*placed.created, *noted.created});
    CHECK(copy);
    CHECK(editor.project().pictures.size() == 2 && editor.project().notes.size() == 2);
    for (const auto& [id, value] : editor.project().pictures) { (void)id; CHECK(value.image == png); }
    CHECK(editor.erase({*placed.created, *noted.created}));
    CHECK(editor.project().pictures.size() == 1 && editor.project().notes.size() == 1);
    CHECK(!editor.project().colours.contains(*noted.created));
    CHECK(!blocks(editor.project()));
    CHECK(editor.undo());
    CHECK(editor.project().pictures.contains(picture) && editor.project().notes.contains(note));
    CHECK(editor.project().colours.contains(*noted.created));

    // How see-through a surface is goes with it: copied, dropped on delete and
    // restored by undo, over whatever colour it has. Zero stores nothing.
    CHECK(editor.set_transparency({*noted.created, *placed.created}, 45));
    CHECK(editor.undo_label() == "Set transparency");
    CHECK(editor.project().transparency.at(*noted.created) == 45 && editor.project().transparency.at(*placed.created) == 45);
    CHECK(!editor.set_transparency({*noted.created}, 101));
    const auto faded_copy = editor.duplicate({*noted.created});
    CHECK(faded_copy && editor.project().transparency.at(*faded_copy.created) == 45);
    CHECK(editor.erase({*faded_copy.created}));
    CHECK(!editor.project().transparency.contains(*faded_copy.created));
    CHECK(!blocks(editor.project()));
    CHECK(editor.set_transparency({*noted.created, *placed.created}, 0));
    CHECK(editor.project().transparency.empty());
    CHECK(editor.undo());
    CHECK(editor.project().transparency.size() == 2);
    CHECK(editor.redo());

    // Neither is in the model, so no attribute belongs to one.
    const auto born = attribute(editor, "Born");
    CHECK(!editor.set_attribute_owner(born, AttributeOwner{*noted.created}));
    CHECK(!editor.set_attribute_owner(born, AttributeOwner{*placed.created}));
    CHECK(!editor.project().attributes.at(born).owner);
    // An unnamed figure is not a finding: a picture speaks for itself.
    CHECK(editor.rename(*noted.created, ""));
    const auto issues = validate(editor.project());
    CHECK(std::none_of(issues.begin(), issues.end(), [&](const Issue& issue) {
        return issue.code == "name.missing" && issue.element == std::optional<ElementRef>{*noted.created};
    }));
    // A picture holds an image or nothing at all: bytes that could not be one
    // are refused, and so is more than a project file has room for.
    CHECK(!editor.create_picture("Not an image", {0, 0, 10, 10}, {'h', 'e', 'l', 'l', 'o'}));
    CHECK(!editor.create_picture("Empty", {0, 0, 10, 10}, {}));
    std::vector<std::uint8_t> huge(max_image_bytes + 1, 0);
    std::copy(png.begin(), png.end(), huge.begin());
    CHECK(!editor.create_picture("Huge", {0, 0, 10, 10}, huge));
    CHECK(editor.project().pictures.size() == 2);
    CHECK(!blocks(editor.project()));
}

// Relating two entities creates the relationship and both of its sides as one
// edit, so it is one step of history rather than three.
void relating_two_entities_is_one_edit() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const auto course = entity(editor, "Course");
    const auto revision = editor.revision();
    const auto made = editor.relate(student, course, Rect{0, 0, 180, 100}, "Enrolled");
    CHECK(made && made.created);
    CHECK(editor.revision() == revision + 1);
    CHECK(editor.undo_label() == "Relate entities");
    const auto id = std::get<RelationshipId>(*made.created);
    const auto& relationship = editor.project().relationships.at(id);
    CHECK(relationship.name == "Enrolled");
    CHECK(relationship.participants.size() == 2);
    CHECK(relationship.participants.front().target == ParticipantTarget{student});
    CHECK(relationship.participants.back().target == ParticipantTarget{course});
    // Every identity is fresh and distinct, participants included.
    CHECK(relationship.participants.front().id != relationship.participants.back().id);
    CHECK((editor.project().layout.at(*made.created) == Rect{0, 0, 180, 100}));
    CHECK(!blocks(editor.project()));
    CHECK(editor.undo());
    CHECK(editor.project().relationships.empty());
    CHECK(editor.project().entities.size() == 2);
    CHECK(editor.redo());
    CHECK(editor.project().relationships.size() == 1);

    // Either side may be pinned in the same edit, and a missing entity is refused.
    Connector pinned;
    pinned.child_anchor = 1.25;
    const auto second = editor.relate(student, course, Rect{0, 300, 180, 100}, "Advises", pinned, Connector{});
    CHECK(second && second.created);
    const auto& sides = editor.project().relationships.at(std::get<RelationshipId>(*second.created)).participants;
    CHECK(editor.project().connectors.at(ConnectorRef{sides.front().id}).child_anchor == 1.25);
    CHECK(!editor.project().connectors.contains(ConnectorRef{sides.back().id}));
    CHECK(editor.erase({ElementRef{course}}));
    CHECK(!editor.relate(student, course, Rect{0, 0, 180, 100}));
}

// A weak entity and the identifying relationship it is identified through
// are two flags that undo, copy and validate like the rest of the model.
void weak_entities_and_identifying_relationships() {
    TestIds ids;
    Editor editor(ids);
    const auto employee = entity(editor, "Employee");
    const auto dependant = entity(editor, "Dependant");
    const auto has = relationship(editor, "Has");
    CHECK(!editor.project().entities.at(dependant).weak);
    CHECK(relationship_kind(editor.project().relationships.at(has)) == RelationshipKind::Regular);

    CHECK(editor.set_entity_weak(dependant, true));
    CHECK(editor.undo_label() == "Change entity kind");
    CHECK(editor.project().entities.at(dependant).weak);
    auto issues = validate(editor.project());
    CHECK(std::any_of(issues.begin(), issues.end(), [](const Issue& issue) { return issue.code == "entity.weak.unidentified"; }));

    CHECK(editor.set_relationship_kind(has, RelationshipKind::Identifying));
    CHECK(editor.undo_label() == "Change relationship kind");
    CHECK(editor.project().relationships.at(has).identifying && !editor.project().relationships.at(has).associative);
    issues = validate(editor.project());
    CHECK(std::any_of(issues.begin(), issues.end(), [](const Issue& issue) { return issue.code == "relationship.identifying.no_weak"; }));
    CHECK(editor.connect(has, employee) && editor.connect(has, dependant));
    issues = validate(editor.project());
    CHECK(std::none_of(issues.begin(), issues.end(), [](const Issue& issue) {
        return issue.code == "relationship.identifying.no_weak" || issue.code == "entity.weak.unidentified";
    }));
    CHECK(!blocks(editor.project()));

    // The kinds are exclusive: making an identifying relationship associative
    // through the older command is refused rather than leaving it both.
    CHECK(!editor.set_associative(has, true));
    CHECK(editor.project().relationships.at(has).identifying);
    // Through the kind, one replaces the other in a single edit, with the
    // body resized as an associative entity is.
    CHECK(editor.set_relationship_kind(has, RelationshipKind::Associative, Rect{0, 0, 160, 80}));
    CHECK(relationship_kind(editor.project().relationships.at(has)) == RelationshipKind::Associative);
    CHECK((editor.project().layout.at(ElementRef{has}) == Rect{0, 0, 160, 80}));
    CHECK(editor.undo());
    CHECK(relationship_kind(editor.project().relationships.at(has)) == RelationshipKind::Identifying);
    CHECK(editor.set_relationship_kind(has, RelationshipKind::Regular));
    CHECK(!editor.project().relationships.at(has).identifying && !editor.project().relationships.at(has).associative);
    // Setting what is already set is not an edit.
    const auto revision = editor.revision();
    CHECK(editor.set_entity_weak(dependant, true));
    CHECK(editor.set_relationship_kind(has, RelationshipKind::Regular));
    CHECK(editor.revision() == revision);
    // A copy is as weak as its original.
    const auto copy = editor.duplicate({ElementRef{dependant}});
    CHECK(copy && editor.project().entities.at(std::get<EntityId>(*copy.created)).weak);
    CHECK(editor.set_entity_weak(dependant, false));
    CHECK(!editor.project().entities.at(dependant).weak);
    CHECK(editor.undo());
    CHECK(editor.project().entities.at(dependant).weak);
}

// Connector shapes are validated like any other persisted reference.
void hostile_connector_shapes_are_rejected() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const auto enrolled = relationship(editor, "Enrolled");
    const auto side = connect(editor, enrolled, student);
    const auto orphan = attribute(editor, "Loose");

    auto project = editor.project();
    CHECK(!blocks(project));
    // An unowned attribute draws no link, so it can carry no shape.
    project.connectors.emplace(ConnectorRef{orphan}, 10.0);
    CHECK(blocks(project));

    project = editor.project();
    project.connectors.emplace(ConnectorRef{ParticipantId{}}, Connector{10.0, {}, {}});
    CHECK(blocks(project));

    for (const auto bad : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(), 1e9}) {
        project = editor.project();
        project.connectors.insert_or_assign(ConnectorRef{side}, Connector{bad, {}, {}});
        CHECK(blocks(project));
        // A pinned join is a direction, and must be a usable one: an angle that
        // is not finite leaves the line nowhere to meet its shape. Unlike the
        // bend it has no range to exceed, so a large angle is merely a wound-up
        // one and stays acceptable.
        project = editor.project();
        project.connectors.insert_or_assign(ConnectorRef{side}, Connector{0, bad, {}});
        CHECK(blocks(project) == !std::isfinite(bad));
    }
    // A bend the editor would accept must also survive validation directly.
    project = editor.project();
    project.connectors.insert_or_assign(ConnectorRef{side}, Connector{-99999.0, {}, {}});
    CHECK(!blocks(project));
}

// An associative relationship keeps its own identity and may take part in
// further relationships, as the Chen "Enrolled participates in Teach" shape does.
void associative_relationships_act_as_entities() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const auto course = entity(editor, "Course");
    const auto teacher = entity(editor, "Teacher");
    const auto enrolled = relationship(editor, "Enrolled");
    const auto teach = relationship(editor, "Teach");
    connect(editor, enrolled, student);
    connect(editor, enrolled, course);
    connect(editor, teach, teacher);

    // A plain relationship cannot take part in another one.
    CHECK(!editor.project().relationships.at(enrolled).associative);
    CHECK(!editor.connect(teach, ParticipantTarget{enrolled}));
    CHECK(!blocks(editor.project()));

    // Adopting the associative shape may resize the body in the same edit, so
    // one undo restores both the flag and the previous size.
    CHECK(editor.move({{ElementRef{enrolled}, {0, 0, 190, 110}}}));
    CHECK(editor.set_associative(enrolled, true, Rect{15, 15, 160, 80}));
    CHECK(editor.project().relationships.at(enrolled).associative);
    CHECK(editor.project().layout.at(ElementRef{enrolled}) == (Rect{15, 15, 160, 80}));
    CHECK(editor.undo());
    CHECK(!editor.project().relationships.at(enrolled).associative);
    CHECK(editor.project().layout.at(ElementRef{enrolled}) == (Rect{0, 0, 190, 110}));
    CHECK(editor.redo());
    CHECK(editor.project().relationships.at(enrolled).associative);
    CHECK(editor.connect(teach, ParticipantTarget{enrolled}));
    CHECK(editor.project().relationships.at(teach).participants.size() == 2);
    CHECK(editor.project().relationships.at(teach).participants.back().target == ParticipantTarget{enrolled});
    CHECK(!blocks(editor.project()));

    // Clearing the flag while it is still in use would strand that participant.
    CHECK(!editor.set_associative(enrolled, false));
    CHECK(editor.project().relationships.at(enrolled).associative);

    // Undo restores the plain relationship together with the participation.
    CHECK(editor.undo());
    CHECK(editor.project().relationships.at(teach).participants.size() == 1);
    CHECK(editor.undo());
    CHECK(!editor.project().relationships.at(enrolled).associative);

    // A relationship may not take part in itself, nor form a cycle.
    CHECK(editor.redo());
    auto malformed = editor.project();
    malformed.relationships.at(enrolled).participants.push_back(
        {ParticipantId{ids.next()}, ParticipantTarget{enrolled}, Cardinality::Many, Participation::Partial, {}});
    CHECK(has_issue(malformed, "participant.self"));

    malformed = editor.project();
    CHECK(editor.redo());
    auto cyclic = editor.project();
    cyclic.relationships.at(teach).associative = true;
    cyclic.relationships.at(enrolled).participants.push_back(
        {ParticipantId{ids.next()}, ParticipantTarget{teach}, Cardinality::Many, Participation::Partial, {}});
    CHECK(has_issue(cyclic, "participant.cycle"));

    // A participant pointing at a plain relationship is rejected outright.
    auto plain = editor.project();
    plain.relationships.at(enrolled).associative = false;
    CHECK(has_issue(plain, "participant.not_associative"));
}

void associative_graph_branches_and_cycles() {
    // Vary map iteration order as well as participant order: the cycle must be
    // found even when A -> B is visited before the cyclic A -> C -> A branch.
    std::array<unsigned, 4> order{0, 1, 2, 3};
    do {
        for (const bool reverse : {false, true}) {
            TestIds ids;
            Editor editor(ids);
            const std::array<RelationshipId, 4> nodes{
                relationship(editor), relationship(editor), relationship(editor), relationship(editor)};
            const auto a = nodes[order[0]], b = nodes[order[1]], c = nodes[order[2]], d = nodes[order[3]];
            for (const auto node : nodes) CHECK(editor.set_associative(node, true));
            // Both branches share D, which is valid and must not look cyclic.
            CHECK(editor.connect(a, ParticipantTarget{reverse ? c : b}));
            CHECK(editor.connect(a, ParticipantTarget{reverse ? b : c}));
            CHECK(editor.connect(b, ParticipantTarget{d}));
            CHECK(editor.connect(c, ParticipantTarget{d}));
            CHECK(!blocks(editor.project()));
            CHECK(!has_issue(editor.project(), "participant.cycle"));

            auto cyclic = editor.project();
            cyclic.relationships.at(c).participants.push_back(
                {ParticipantId{ids.next()}, ParticipantTarget{a}, Cardinality::Many, Participation::Partial, {}});
            CHECK(has_issue(cyclic, "participant.cycle"));
            CHECK(blocks(cyclic));

            CHECK(editor.rename(a, "Pending redo"));
            const auto redone = editor.project();
            CHECK(editor.undo());
            editor.mark_saved(editor.revision());
            check_rejection_preserves_history(editor, [&] { return editor.connect(c, ParticipantTarget{a}); });
            check_rejection_preserves_history(editor, [&] { return editor.replace_project(cyclic); });
            CHECK(editor.redo());
            CHECK(editor.project() == redone);
        }
    } while (std::next_permutation(order.begin(), order.end()));
}

void inheritance_graph_branches_and_cycles() {
    // A inherits from B and C, which both inherit from D. Try every entity ID
    // order and both orders of A's parents, so shared ancestors are always legal.
    std::array<unsigned, 4> order{0, 1, 2, 3};
    do {
        for (const bool reverse : {false, true}) {
            TestIds ids;
            Editor editor(ids);
            const std::array<EntityId, 4> nodes{entity(editor), entity(editor), entity(editor), entity(editor)};
            const auto a = nodes[order[0]], b = nodes[order[1]], c = nodes[order[2]], d = nodes[order[3]];
            const auto triangle = [&](std::optional<EntityId> parent) {
                const auto result = editor.create_specialization("IS A", {}, Inheritance::Specialization);
                CHECK(result && result.created);
                const auto id = std::get<SpecializationId>(*result.created);
                CHECK(editor.set_supertype(id, parent));
                return id;
            };
            const auto root = triangle(d);
            CHECK(editor.attach_subtype(root, b));
            CHECK(editor.attach_subtype(root, c));
            const auto first = triangle(reverse ? c : b);
            const auto second = triangle(reverse ? b : c);
            CHECK(editor.attach_subtype(first, a));
            const auto before_diamond = editor.project();
            CHECK(editor.attach_subtype(second, a));
            const auto diamond = editor.project();
            CHECK(!blocks(diamond));
            CHECK(!has_issue(diamond, "specialization.cycle"));
            CHECK(editor.undo());
            CHECK(editor.project() == before_diamond);
            CHECK(editor.redo());
            CHECK(editor.project() == diamond);

            const auto back_edge = triangle(a);
            const auto unfinished = triangle({});
            CHECK(editor.attach_subtype(unfinished, d));
            auto cyclic = editor.project();
            cyclic.specializations.at(back_edge).subtypes.push_back(d);
            CHECK(has_issue(cyclic, "specialization.cycle"));
            CHECK(blocks(cyclic));
            CHECK(has_issue(editor.project(), "specialization.supertype.incomplete"));
            CHECK(!blocks(editor.project()));

            CHECK(editor.rename(a, "Pending redo"));
            const auto redone = editor.project();
            CHECK(editor.undo());
            editor.mark_saved(editor.revision());
            check_rejection_preserves_history(editor, [&] { return editor.attach_subtype(back_edge, d); });
            check_rejection_preserves_history(editor, [&] { return editor.set_supertype(unfinished, a); });
            check_rejection_preserves_history(editor, [&] { return editor.replace_project(cyclic); });
            CHECK(editor.redo());
            CHECK(editor.project() == redone);
        }
    } while (std::next_permutation(order.begin(), order.end()));
}

void deep_relationship_and_inheritance_graphs() {
    TestIds ids;
    Project relationships;
    relationships.id = ProjectId{ids.next()};
    Project inheritance;
    inheritance.id = ProjectId{ids.next()};
    std::vector<RelationshipId> relationship_ids;
    std::vector<EntityId> entity_ids;
    for (std::size_t i = 0; i < 2000; ++i) {
        const RelationshipId rel{ids.next()};
        relationship_ids.push_back(rel);
        relationships.relationships.emplace(rel, Relationship{.id = rel, .name = "Associative", .associative = true});
        relationships.layout.emplace(ElementRef{rel}, Rect{});
        const EntityId ent{ids.next()};
        entity_ids.push_back(ent);
        inheritance.entities.emplace(ent, Entity{.id = ent, .name = "Entity"});
        inheritance.layout.emplace(ElementRef{ent}, Rect{});
    }
    // The lowest IDs lead to the next highest, forcing the full depth to be
    // visited before any vertex can finish. No recursive call stack is needed.
    for (std::size_t i = 1; i < relationship_ids.size(); ++i) {
        relationships.relationships.at(relationship_ids[i - 1]).participants.push_back(
            {ParticipantId{ids.next()}, ParticipantTarget{relationship_ids[i]}, Cardinality::Many, Participation::Partial, {}});
        const SpecializationId spec{ids.next()};
        inheritance.specializations.emplace(spec, Specialization{spec, "IS A", {}, Inheritance::Specialization,
                                                                  entity_ids[i], {entity_ids[i - 1]}});
        inheritance.layout.emplace(ElementRef{spec}, Rect{});
    }
    CHECK(!blocks(relationships));
    CHECK(!blocks(inheritance));
    relationships.relationships.at(relationship_ids.back()).participants.push_back(
        {ParticipantId{ids.next()}, ParticipantTarget{relationship_ids.front()}, Cardinality::Many, Participation::Partial, {}});
    const SpecializationId spec{ids.next()};
    inheritance.specializations.emplace(spec, Specialization{spec, "IS A", {}, Inheritance::Specialization,
                                                              entity_ids.front(), {entity_ids.back()}});
    inheritance.layout.emplace(ElementRef{spec}, Rect{});
    CHECK(has_issue(relationships, "participant.cycle"));
    CHECK(has_issue(inheritance, "specialization.cycle"));
}

void coloured_specialization_cascade_restores_exactly() {
    TestIds ids;
    Editor editor(ids);
    const auto parent = entity(editor, "Person");
    const auto child = entity(editor, "Student");
    const auto result = editor.create_specialization("IS A", {20, 100, 96, 74}, Inheritance::Generalization);
    CHECK(result && result.created);
    const auto spec = std::get<SpecializationId>(*result.created);
    CHECK(editor.set_supertype(spec, parent));
    CHECK(editor.attach_subtype(spec, child));
    CHECK(editor.recolour({ElementRef{spec}}, Colour{1, 2, 3}));
    CHECK(editor.recolour({ElementRef{child}}, Colour{40, 50, 60}));
    editor.mark_saved(editor.revision());
    const auto before = editor.project();
    CHECK(editor.erase({ElementRef{parent}}));
    CHECK(!editor.project().entities.contains(parent));
    CHECK(editor.project().entities.contains(child));
    CHECK(!editor.project().specializations.contains(spec));
    CHECK(!editor.project().layout.contains(ElementRef{spec}));
    CHECK(!editor.project().colours.contains(ElementRef{spec}));
    CHECK(editor.project().colours.at(ElementRef{child}) == (Colour{40, 50, 60}));
    CHECK(!blocks(editor.project()));
    const auto after = editor.project();
    CHECK(editor.undo());
    CHECK(editor.project() == before);
    CHECK(!editor.dirty());
    CHECK(editor.redo());
    CHECK(editor.project() == after);
    CHECK(editor.dirty());
}

// Generalization and specialization: one supertype, its subtypes, and the two
// rules a later conversion needs in order to choose a relational mapping.
void specializations_carry_inheritance_rules() {
    TestIds ids;
    Editor editor(ids);
    const auto person = entity(editor, "Person");
    const auto student = entity(editor, "Student");
    const auto employee = entity(editor, "Employee");
    const auto teacher = entity(editor, "Teacher");

    const auto created = editor.create_specialization("IS A", {0, 200, 96, 74}, Inheritance::Specialization);
    CHECK(created);
    const auto role = std::get<SpecializationId>(*created.created);
    // A placed triangle waits to be wired up; that is a warning, not an error.
    CHECK(!blocks(editor.project()));
    CHECK(has_issue(editor.project(), "specialization.supertype.incomplete"));
    CHECK(editor.set_supertype(role, person));
    // A triangle with no subtypes yet is work in progress, not an error.
    CHECK(!blocks(editor.project()));
    CHECK(has_issue(editor.project(), "specialization.subtypes.incomplete"));
    CHECK(name(editor.project(), ElementRef{role}) == "IS A");

    CHECK(editor.attach_subtype(role, student));
    CHECK(editor.attach_subtype(role, employee));
    CHECK(editor.project().specializations.at(role).subtypes.size() == 2);
    CHECK(!blocks(editor.project()));
    // The same entity cannot be attached twice, and the edit is refused whole.
    CHECK(!editor.attach_subtype(role, student));
    CHECK(editor.project().specializations.at(role).subtypes.size() == 2);

    // Disjoint/partial by default; both rules are editable together.
    const auto& defaults = editor.project().specializations.at(role);
    CHECK(defaults.constraint == Disjointness::Disjoint);
    CHECK(defaults.completeness == Completeness::Partial);
    CHECK(editor.set_specialization_rules(role, Disjointness::Overlapping, Completeness::Total));
    CHECK(editor.project().specializations.at(role).constraint == Disjointness::Overlapping);
    CHECK(editor.project().specializations.at(role).completeness == Completeness::Total);
    CHECK(editor.undo());
    CHECK(editor.project().specializations.at(role).constraint == Disjointness::Disjoint);

    // The direction is stored, because the triangle points the way it was read.
    CHECK(editor.project().specializations.at(role).direction == Inheritance::Specialization);
    CHECK(editor.set_inheritance_direction(role, Inheritance::Generalization));
    CHECK(editor.project().specializations.at(role).direction == Inheritance::Generalization);
    CHECK(editor.undo());
    CHECK(editor.project().specializations.at(role).direction == Inheritance::Specialization);
    auto wrong = editor.project();
    wrong.specializations.at(role).direction = static_cast<Inheritance>(7);
    CHECK(has_issue(wrong, "specialization.direction.invalid"));

    // Specialization nests: an Employee may itself be generalised further.
    const auto job = std::get<SpecializationId>(*editor.create_specialization("IS A", {0, 400, 96, 74}, Inheritance::Generalization).created);
    CHECK(editor.set_supertype(job, employee));
    CHECK(editor.attach_subtype(job, teacher));
    CHECK(!blocks(editor.project()));

    // An entity cannot be its own subtype, nor inherit from itself in a cycle.
    auto malformed = editor.project();
    malformed.specializations.at(role).subtypes.push_back(person);
    CHECK(has_issue(malformed, "specialization.self"));
    malformed = editor.project();
    malformed.specializations.at(job).subtypes.push_back(person);
    CHECK(has_issue(malformed, "specialization.cycle"));

    // A specialization holds no attributes of its own.
    malformed = editor.project();
    const auto loose = attribute(editor, "Stray");
    malformed = editor.project();
    malformed.attributes.at(loose).owner = ElementRef{role};
    CHECK(has_issue(malformed, "attribute.owner.specialization"));

    // Detaching and deleting both leave the model valid and are undoable.
    CHECK(editor.detach_subtype(role, employee));
    CHECK(editor.project().specializations.at(role).subtypes.size() == 1);
    CHECK(editor.undo());
    CHECK(editor.project().specializations.at(role).subtypes.size() == 2);
    CHECK(editor.erase({ElementRef{role}}));
    CHECK(!editor.project().specializations.contains(role));
    CHECK(!blocks(editor.project()));
    CHECK(editor.undo());
    CHECK(editor.project().specializations.at(role).subtypes.size() == 2);

    // Deleting a subtype entity must not leave the triangle pointing at nothing.
    CHECK(editor.erase({ElementRef{student}}));
    CHECK(!blocks(editor.project()));
}

// The four binary ratios are the two maximums read together. Setting one writes
// both sides in a single edit, which is what keeps every notation consistent:
// they all read the same participant records.
void binary_ratios_and_reversal() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const auto course = entity(editor, "Course");
    const auto enrolled = relationship(editor, "Enrolled");
    const auto first = connect(editor, enrolled, student);
    const auto second = connect(editor, enrolled, course);

    const auto sides = [&] { return editor.project().relationships.at(enrolled).participants; };
    const auto maximums = [&] { return std::make_pair(sides()[0].maximum, sides()[1].maximum); };

    // All four are reachable, each in one edit.
    const std::pair<Cardinality, Cardinality> all[] = {
        {Cardinality::One, Cardinality::One}, {Cardinality::One, Cardinality::Many},
        {Cardinality::Many, Cardinality::One}, {Cardinality::Many, Cardinality::Many}};
    for (const auto& [a, b] : all) {
        const auto before = editor.revision();
        CHECK(editor.set_ratio(enrolled, a, b));
        CHECK(maximums() == std::make_pair(a, b));
        CHECK(editor.revision() == before + 1);
        CHECK(!blocks(editor.project()));
    }
    // Setting the ratio already in use is not an edit at all.
    const auto settled = editor.revision();
    CHECK(editor.set_ratio(enrolled, Cardinality::Many, Cardinality::Many));
    CHECK(editor.revision() == settled);

    // Participation is the other half of each side and is left alone.
    CHECK(editor.update_participant(enrolled, first, Cardinality::One, Participation::Total, "student"));
    CHECK(editor.set_ratio(enrolled, Cardinality::Many, Cardinality::One));
    CHECK(sides()[0].participation == Participation::Total);
    CHECK(sides()[0].role == "student");
    CHECK(sides()[0].id == first && sides()[1].id == second);

    // Reversing swaps both halves between the sides, so M:1 becomes 1:M.
    CHECK(editor.reverse_participants(enrolled));
    CHECK(maximums() == std::make_pair(Cardinality::One, Cardinality::Many));
    CHECK(sides()[0].participation == Participation::Partial);
    CHECK(sides()[1].participation == Participation::Total);
    // Identity and the entity each side names are untouched by a reversal.
    CHECK(sides()[0].id == first && sides()[1].id == second);
    CHECK(sides()[0].target == ParticipantTarget{student});
    CHECK(sides()[1].target == ParticipantTarget{course});
    CHECK(editor.undo());
    CHECK(maximums() == std::make_pair(Cardinality::Many, Cardinality::One));

    // A ratio describes exactly two sides, so neither operation applies to a
    // relationship that does not have two.
    const auto teacher = entity(editor, "Teacher");
    connect(editor, enrolled, teacher);
    CHECK(!editor.set_ratio(enrolled, Cardinality::One, Cardinality::One));
    CHECK(!editor.reverse_participants(enrolled));
    CHECK(maximums() == std::make_pair(Cardinality::Many, Cardinality::One));
}

} // namespace

// A comment is a remark about the work rather than part of it: pinned to things
// instead of placed, able to cover several at once, and able to be put away
// without being deleted. None of that is true of a Note, which is a card on the
// canvas, or of a description, which documents the model itself.
void comments_are_pinned_to_things() {
    TestIds ids;
    Editor editor(ids);
    const auto first = editor.create_entity("Student", {0, 0, 160, 80});
    const auto second = editor.create_entity("Course", {400, 0, 160, 80});
    const auto joined = editor.create_relationship("Enrolled", {200, 200, 190, 110});
    CHECK(first && second && joined);
    const auto student = std::get<EntityId>(*first.created);
    const auto course = std::get<EntityId>(*second.created);
    const auto enrolled = std::get<RelationshipId>(*joined.created);
    const auto side = editor.connect(enrolled, student);
    CHECK(side);
    const auto participant = *side.participant;

    // One remark over several things at once, which is the point: a reviewer
    // says a thing once and it appears everywhere it applies.
    CHECK(editor.create_comment("These two need a join table.",
                                {CommentTarget{ElementRef{student}}, CommentTarget{ElementRef{course}}}));
    CHECK(editor.project().comments.size() == 1);
    const auto id = editor.project().comments.begin()->first;
    CHECK(comments_on(editor.project(), ElementRef{student}) == std::vector<CommentId>{id});
    CHECK(comments_on(editor.project(), ElementRef{course}) == std::vector<CommentId>{id});
    CHECK(comments_on(editor.project(), ElementRef{enrolled}).empty());

    // A line can carry one of its own: a remark about a cardinality belongs on
    // the line rather than on either shape it joins.
    CHECK(editor.create_comment("Should this be total?", {CommentTarget{ConnectorRef{participant}}}));
    CHECK(comments_on_connector(editor.project(), ConnectorRef{participant}).size() == 1);

    // And a remark can be pinned into part of what somebody wrote.
    TextAnchor anchor{ElementRef{student}, TextField::Name, 0, 7};
    CHECK(editor.create_comment("Is this the right word?", {CommentTarget{anchor}}));
    // A remark pinned into an element's own writing counts as a remark on it,
    // so the mark appears on the shape rather than being buried in a panel.
    CHECK(comments_on(editor.project(), ElementRef{student}).size() == 2);

    // Pinned to nothing is refused rather than saved: it could never be found.
    CHECK(!editor.create_comment("Nowhere", {}));
    CHECK(!editor.create_comment("Gone", {CommentTarget{ElementRef{EntityId{Uuid{}}}}}));
    // A range outside the text it is pinned into is refused too.
    CHECK(!editor.create_comment("Past the end", {CommentTarget{TextAnchor{ElementRef{student}, TextField::Name, 0, 99}}}));

    // Put away without being deleted, and brought back.
    CHECK(editor.set_comment_hidden(id, true));
    CHECK(editor.project().comments.at(id).hidden);
    CHECK(editor.set_comment_hidden(id, false));
    CHECK(!editor.project().comments.at(id).hidden);

    // Shortening the text a remark is pinned into must not refuse the edit, and
    // must not lose the remark: the range is held inside what the text now is.
    CHECK(editor.rename(ElementRef{student}, "Stu"));
    const auto held = std::find_if(editor.project().comments.begin(), editor.project().comments.end(),
                                   [](const auto& entry) {
                                       return entry.second.text == "Is this the right word?";
                                   });
    CHECK(held != editor.project().comments.end());
    const auto& moved = std::get<TextAnchor>(held->second.targets.front());
    CHECK(moved.begin + moved.length <= character_count(name(editor.project(), ElementRef{student})));
    // The half-joined relationship in this fixture is a warning, as an
    // unfinished draft should be; nothing about the held range blocks a save.
    const auto findings = validate(editor.project());
    CHECK(std::none_of(findings.begin(), findings.end(), [](const Issue& issue) { return issue.blocks_save; }));

    // Deleting a thing unpins every remark on it, and a remark left pinned to
    // nothing goes with it -- in the same edit, so one undo brings back the
    // element, the line and what was said about them together.
    const auto before = editor.project();
    CHECK(editor.erase({ElementRef{student}}));
    CHECK(!editor.project().comments.contains(held->first));
    const auto pair = editor.project().comments.find(id);
    CHECK(pair != editor.project().comments.end());
    CHECK(pair->second.targets.size() == 1);
    CHECK(std::get<ElementRef>(pair->second.targets.front()) == ElementRef{course});
    // The line went with the entity, so the remark about the line went too.
    CHECK(comments_on_connector(editor.project(), ConnectorRef{participant}).empty());
    CHECK(editor.undo());
    CHECK(editor.project() == before);

    // A triangle goes when its supertype does, though nobody asked for the
    // triangle. A remark pinned to it has to go with it in that same edit, or
    // the deletion would be refused for a remark pointing at nothing.
    const auto parent = editor.create_entity("Person", {0, 400, 160, 80});
    const auto triangle = editor.create_specialization("Kind", {0, 520, 96, 74}, Inheritance::Specialization);
    CHECK(parent && triangle);
    const auto person = std::get<EntityId>(*parent.created);
    const auto isa = std::get<SpecializationId>(*triangle.created);
    CHECK(editor.set_supertype(isa, person));
    CHECK(editor.create_comment("Is this hierarchy worth it?", {CommentTarget{ElementRef{isa}}}));
    const auto on_triangle = comments_on(editor.project(), ElementRef{isa});
    CHECK(on_triangle.size() == 1);
    const auto counted = editor.project().comments.size();
    CHECK(editor.erase({ElementRef{person}}));
    CHECK(!editor.project().specializations.contains(isa));
    CHECK(editor.project().comments.size() == counted - 1);
    CHECK(editor.undo());
    CHECK(editor.project().comments.size() == counted);
}

// One model, asked what it will become. There is no mode: the fields that
// conversion needs are part of every model and are always there to be set.
void the_model_says_what_it_becomes() {
    TestIds ids;
    Editor editor(ids);
    const auto made = editor.create_entity("Student", {0, 0, 160, 80});
    CHECK(made);
    const auto student = std::get<EntityId>(*made.created);
    const auto attribute = editor.create_attribute("ID", {0, -160, 150, 60}, ElementRef{student});
    CHECK(attribute);
    const auto id = std::get<AttributeId>(*attribute.created);

    // An attribute starts with the question open rather than with an answer,
    // and nothing has to be switched on before it can be answered.
    CHECK(editor.project().attributes.at(id).logical_type == LogicalType::Unset);

    // A type, and a length only where the type takes one.
    CHECK(editor.set_logical_type(id, LogicalType::Varchar, 100));
    CHECK(editor.project().attributes.at(id).logical_type == LogicalType::Varchar);
    CHECK(editor.project().attributes.at(id).length == 100);
    // Changed to a type that is not measured, the number goes rather than being
    // carried along to mean nothing later.
    CHECK(editor.set_logical_type(id, LogicalType::Bit, 100));
    CHECK(editor.project().attributes.at(id).length == 0);
    CHECK(editor.set_logical_type(id, LogicalType::Decimal, 12));
    CHECK(editor.project().attributes.at(id).length == 12);

    // What the table will enforce. Being the identifier and being drawn as a
    // key are one fact (Zain, 2026-09-24), so marking it the identifier draws
    // it as a key, and taking that off draws it as an ordinary attribute again.
    CHECK(editor.set_attribute_rules(id, true, true, true));
    const auto& ruled = editor.project().attributes.at(id);
    CHECK(ruled.identifier && ruled.required && ruled.unique);
    CHECK(ruled.kind == AttributeKind::Key);
    CHECK(editor.set_attribute_rules(id, false, true, true));
    CHECK(editor.project().attributes.at(id).kind == AttributeKind::Normal);
    CHECK(editor.set_attribute_rules(id, true, true, true));

    // The comment written for the database, on the three things that become
    // tables and columns and on nothing else.
    CHECK(editor.set_schema_comment(ElementRef{id}, "Permanent identifier."));
    CHECK(editor.project().attributes.at(id).comment == "Permanent identifier.");
    CHECK(editor.set_schema_comment(ElementRef{student}, "A registered person."));
    const auto note = editor.create_note("Note", {400, 400, 200, 120});
    CHECK(note);
    CHECK(!editor.set_schema_comment(*note.created, "Nobody will ever read this."));

    // An attribute with no type yet has left a question open, and an open
    // question is not a fault. validate() says whether the model is sound;
    // whether it is ready to become tables is readiness()' question, and
    // Phase 14 is where it gets asked.
    const auto unanswered = editor.create_attribute("Name", {200, -160, 150, 60}, ElementRef{student});
    CHECK(unanswered);
    const auto issues = validate(editor.project());
    CHECK(std::none_of(issues.begin(), issues.end(), [](const Issue& issue) {
        return issue.code == "attribute.type.missing";
    }));
    // A half-answered model is still a model, and still saveable.
    CHECK(std::none_of(issues.begin(), issues.end(), [](const Issue& issue) { return issue.blocks_save; }));
    // And everything the model was told is still there to be read.
    CHECK(editor.project().attributes.at(id).logical_type == LogicalType::Decimal);
    CHECK(editor.project().attributes.at(id).comment == "Permanent identifier.");
}

// The mapping. A Conceptual ERD read as tables, by the rules the course
// material teaches -- including the ones where a textbook alternative exists
// and ERDFlow deliberately follows this one.
void the_diagram_becomes_tables() {
    TestIds ids;
    Editor editor(ids);

    const auto student = std::get<EntityId>(*editor.create_entity("Student", {0, 0, 160, 80}).created);
    const auto course = std::get<EntityId>(*editor.create_entity("Course", {400, 0, 160, 80}).created);

    // A key, a composite with parts, one that is worked out, and one held more
    // than once: between them every attribute rule.
    const auto key = std::get<AttributeId>(*editor.create_attribute("StudentID", {}, ElementRef{student}).created);
    CHECK(editor.set_attribute_kind(key, AttributeKind::Key));
    CHECK(editor.set_attribute_rules(key, true, true, true));
    CHECK(editor.set_logical_type(key, LogicalType::Int));
    const auto full = std::get<AttributeId>(*editor.create_attribute("Name", {}, ElementRef{student}).created);
    CHECK(editor.set_attribute_kind(full, AttributeKind::Composite));
    CHECK(editor.create_attribute("First", {}, ElementRef{full}));
    CHECK(editor.create_attribute("Last", {}, ElementRef{full}));
    const auto age = std::get<AttributeId>(*editor.create_attribute("Age", {}, ElementRef{student}).created);
    CHECK(editor.set_attribute_kind(age, AttributeKind::Derived));
    const auto phone = std::get<AttributeId>(*editor.create_attribute("Phone", {}, ElementRef{student}).created);
    CHECK(editor.set_attribute_kind(phone, AttributeKind::Multivalued));

    const auto course_key = std::get<AttributeId>(*editor.create_attribute("CourseID", {}, ElementRef{course}).created);
    CHECK(editor.set_attribute_kind(course_key, AttributeKind::Key));
    CHECK(editor.set_attribute_rules(course_key, true, true, true));

    // Many to many, which becomes a bridge.
    const auto enrolled = std::get<RelationshipId>(*editor.create_relationship("Enrolled", {200, 200, 190, 110}).created);
    const auto left = editor.connect(enrolled, student);
    const auto right = editor.connect(enrolled, course);
    CHECK(left && right);
    CHECK(editor.update_participant(enrolled, *left.participant, Cardinality::Many, Participation::Partial, ""));
    CHECK(editor.update_participant(enrolled, *right.participant, Cardinality::Many, Participation::Partial, ""));
    CHECK(editor.create_attribute("Grade", {}, ElementRef{enrolled}));

    const auto preview = schema_preview(editor.project());
    const auto table_named = [&](const std::string& wanted) -> const PreviewTable* {
        for (const auto& table : preview.tables) if (table.name == wanted) return &table;
        return nullptr;
    };
    const auto column_named = [](const PreviewTable& table, const std::string& wanted) -> const PreviewColumn* {
        for (const auto& column : table.columns) if (column.name == wanted) return &column;
        return nullptr;
    };

    // A table for each entity, named for the many rows it holds.
    const auto* students = table_named("Students");
    const auto* courses = table_named("Courses");
    CHECK(students && courses);

    // The composite gives its roots and becomes no column itself; the derived
    // attribute becomes nothing at all, and is listed saying so rather than
    // quietly left out; the multivalued one becomes a table.
    CHECK(column_named(*students, "First"));
    CHECK(column_named(*students, "Last"));
    CHECK(!column_named(*students, "Name"));
    const auto* worked_out = column_named(*students, "Age");
    CHECK(worked_out && worked_out->ignored);
    CHECK(!worked_out->primary_key && !worked_out->required && !worked_out->unique);
    CHECK(!column_named(*students, "Phone"));
    const auto* phones = table_named("Phones");
    CHECK(phones);
    CHECK(phones->origin_kind == TableOrigin::Multivalued);
    // Its own key, the value, and a foreign key back to the student.
    CHECK(phones->columns.front().primary_key);
    CHECK(column_named(*phones, "Phone"));
    const auto* back = column_named(*phones, "StudentID");
    CHECK(back && back->foreign_key && !back->primary_key);

    // The key attribute is the primary key, and carries the type it was given.
    const auto* identifier = column_named(*students, "StudentID");
    CHECK(identifier && identifier->primary_key);
    CHECK(identifier->type == LogicalType::Int);

    // Many to many becomes a bridge with a key of its own beside the two
    // foreign keys -- not a composite key made of the pair.
    const auto* bridge = table_named("Enrolleds");
    CHECK(bridge);
    CHECK(bridge->origin_kind == TableOrigin::Bridge);
    CHECK(bridge->columns.front().name == "EnrolledID");
    CHECK(bridge->columns.front().primary_key);
    const auto* to_student = column_named(*bridge, "StudentID");
    const auto* to_course = column_named(*bridge, "CourseID");
    CHECK(to_student && to_student->foreign_key && !to_student->primary_key);
    CHECK(to_course && to_course->foreign_key && !to_course->primary_key);
    CHECK(column_named(*bridge, "Grade"));

    // A composite kept whole gives one column and no parts.
    CHECK(editor.set_composite_mode(full, CompositeMode::Whole));
    const auto whole = schema_preview(editor.project());
    for (const auto& table : whole.tables) {
        if (table.name != "Students") continue;
        CHECK(column_named(table, "Name"));
        CHECK(!column_named(table, "First"));
    }

    // And named as drawn, a table keeps the entity's own name.
    CHECK(editor.set_table_naming(TableNaming::AsDrawn));
    const auto drawn = schema_preview(editor.project());
    bool singular = false;
    for (const auto& table : drawn.tables) if (table.name == "Student") singular = true;
    CHECK(singular);

    // A name typed over it wins over both.
    CHECK(editor.set_table_name(ElementRef{student}, "Pupils"));
    const auto renamed = schema_preview(editor.project());
    bool typed = false;
    for (const auto& table : renamed.tables) if (table.name == "Pupils") typed = true;
    CHECK(typed);
}

// The key a person drew is the table's key (Zain, 2026-09-24). Drawing an
// attribute as a key makes it the primary key and nothing is invented beside
// it; several drawn as keys make one key between them; a foreign key carries
// every column of the key it points at; and a key is made only for an entity
// that has none of its own.
void drawn_keys_are_the_primary_key() {
    TestIds ids;
    Editor editor(ids);
    const auto student = std::get<EntityId>(*editor.create_entity("Student", {0, 0, 160, 80}).created);
    const auto course = std::get<EntityId>(*editor.create_entity("Course", {400, 0, 160, 80}).created);
    const auto room = std::get<EntityId>(*editor.create_entity("Room", {0, 400, 160, 80}).created);

    // Drawn as a key, and nothing else said: that is the primary key, and the
    // two flags are one fact.
    const auto id = std::get<AttributeId>(*editor.create_attribute("ID", {}, ElementRef{student}).created);
    CHECK(editor.set_attribute_kind(id, AttributeKind::Key));
    CHECK(editor.project().attributes.at(id).identifier);
    CHECK(editor.project().attributes.at(id).required);
    CHECK(editor.create_attribute("Surname", {}, ElementRef{student}));
    // A key of two attributes.
    const auto code = std::get<AttributeId>(*editor.create_attribute("Code", {}, ElementRef{course}).created);
    const auto term = std::get<AttributeId>(*editor.create_attribute("Term", {}, ElementRef{course}).created);
    CHECK(editor.set_attribute_kind(code, AttributeKind::Key));
    CHECK(editor.set_attribute_kind(term, AttributeKind::Key));
    // And an entity with no key at all.
    CHECK(editor.create_attribute("Floor", {}, ElementRef{room}));

    // Many students to one room, and students and courses many to many.
    const auto in = std::get<RelationshipId>(*editor.create_relationship("In", {200, 200, 190, 110}).created);
    const auto lodger = editor.connect(in, student);
    const auto lodging = editor.connect(in, room);
    CHECK(lodger && lodging);
    CHECK(editor.update_participant(in, *lodger.participant, Cardinality::Many, Participation::Partial, ""));
    CHECK(editor.update_participant(in, *lodging.participant, Cardinality::One, Participation::Total, ""));
    const auto takes = std::get<RelationshipId>(*editor.create_relationship("Takes", {400, 200, 190, 110}).created);
    const auto taker = editor.connect(takes, student);
    const auto taken = editor.connect(takes, course);
    CHECK(taker && taken);
    CHECK(editor.update_participant(takes, *taker.participant, Cardinality::Many, Participation::Partial, ""));
    CHECK(editor.update_participant(takes, *taken.participant, Cardinality::Many, Participation::Partial, ""));

    const auto preview = schema_preview(editor.project());
    const auto table_from = [&](const ElementRef& origin) -> const PreviewTable* {
        for (const auto& table : preview.tables) if (table.origin == origin) return &table;
        return nullptr;
    };
    const auto key_names = [](const PreviewTable& table) {
        std::vector<std::string> names;
        for (const auto& column : table.columns) if (column.primary_key) names.push_back(column.name);
        return names;
    };
    const auto* students = table_from(ElementRef{student});
    const auto* courses = table_from(ElementRef{course});
    const auto* rooms = table_from(ElementRef{room});
    const auto* bridge = table_from(ElementRef{takes});
    CHECK(students && courses && rooms && bridge);

    // The drawn key is the key, and no StudentID is made beside it.
    CHECK((key_names(*students) == std::vector<std::string>{"ID"}));
    CHECK(std::none_of(students->columns.begin(), students->columns.end(),
                       [](const PreviewColumn& column) { return column.origin_kind == ColumnOrigin::Generated; }));
    // Two drawn keys are one key of two columns.
    CHECK((key_names(*courses) == std::vector<std::string>{"Code", "Term"}));
    // With nothing drawn, a key is made -- and only there.
    CHECK((key_names(*rooms) == std::vector<std::string>{"RoomID"}));
    CHECK(rooms->columns.front().origin_kind == ColumnOrigin::Generated && !rooms->columns.front().origin);

    // A foreign key points at the key that is there, column for column.
    const auto references = [&](const PreviewTable& from, const PreviewTable& to) {
        std::vector<std::string> pointed_at;
        for (const auto& column : from.columns)
            if (column.foreign_key && column.references
                && &preview.tables[*column.references] == &to)
                pointed_at.push_back(to.columns[column.references_column].name);
        return pointed_at;
    };
    CHECK((references(*students, *rooms) == std::vector<std::string>{"RoomID"}));
    CHECK((references(*bridge, *students) == std::vector<std::string>{"ID"}));
    CHECK((references(*bridge, *courses) == std::vector<std::string>{"Code", "Term"}));
    // The bridge keeps a key of its own, as the course rules give it.
    CHECK(bridge->columns.front().primary_key && bridge->columns.front().origin_kind == ColumnOrigin::Generated);

    // Participant keys are an explicit bridge strategy, never the default.
    const auto bridge_now = [&]() {
        const auto current = schema_preview(editor.project());
        for (const auto& table : current.tables)
            if (table.origin == ElementRef{takes}) return table;
        throw std::runtime_error("Missing bridge");
    };
    CHECK(bridge_now().decisions.size() == 1);
    CHECK(bridge_now().decisions.front().chosen == 0);
    CHECK(editor.set_bridge_key(takes, BridgeKey::Pair));
    auto paired = bridge_now();
    CHECK(key_names(paired).size() == 3); // Student.ID and both parts of Course's key
    CHECK(std::all_of(paired.columns.begin(), paired.columns.end(), [](const PreviewColumn& column) {
        return column.primary_key && column.foreign_key && column.required;
    }));
    CHECK(paired.decisions.front().answered && paired.decisions.front().chosen == 1);
    CHECK(editor.undo());
    CHECK((key_names(bridge_now()) == std::vector<std::string>{"TakesID"}));
    CHECK(editor.redo());
    CHECK(key_names(bridge_now()).size() == 3);
    CHECK(editor.set_bridge_key(takes, BridgeKey::Own));
    CHECK((key_names(bridge_now()) == std::vector<std::string>{"TakesID"}));
    CHECK(editor.set_bridge_key(takes, BridgeKey::Pair));
    const auto enrollment_id = std::get<AttributeId>(
        *editor.create_attribute("EnrollmentNumber", {}, ElementRef{takes}).created);
    CHECK(editor.set_attribute_kind(enrollment_id, AttributeKind::Key));
    const auto identified = bridge_now();
    CHECK((key_names(identified) == std::vector<std::string>{"EnrollmentNumber"}));
    CHECK(identified.decisions.empty()); // No strategy offers to replace the conceptual identifier.
    for (const auto& column : identified.columns)
        if (column.foreign_key) CHECK(!column.primary_key);

    // The same fact from the other side: taking the key off the drawing takes
    // it off the table, and a key is then made for it.
    CHECK(editor.set_attribute_kind(id, AttributeKind::Normal));
    CHECK(!editor.project().attributes.at(id).identifier);
    const auto keyless = schema_preview(editor.project());
    for (const auto& table : keyless.tables)
        if (table.origin == ElementRef{student})
            CHECK((key_names(table) == std::vector<std::string>{"StudentID"}));
    // And marking it the identifier draws it as a key again.
    CHECK(editor.set_attribute_rules(id, true, true, false));
    CHECK(editor.project().attributes.at(id).kind == AttributeKind::Key);

    // A project from before the two were one fact is brought into agreement
    // when it is opened: a key oval is the primary key, and an identifier is
    // drawn as a key.
    auto older = editor.project();
    older.attributes.at(id).identifier = false;
    older.attributes.at(code).kind = AttributeKind::Normal;
    CHECK(editor.replace_project(older));
    CHECK(editor.project().attributes.at(id).identifier);
    CHECK(editor.project().attributes.at(code).kind == AttributeKind::Key);

    // A composite that identifies a row does so through its parts.
    const auto person = std::get<EntityId>(*editor.create_entity("Person", {800, 0, 160, 80}).created);
    const auto name = std::get<AttributeId>(*editor.create_attribute("Name", {}, ElementRef{person}).created);
    CHECK(editor.set_attribute_kind(name, AttributeKind::Composite));
    CHECK(editor.create_attribute("First", {}, ElementRef{name}));
    CHECK(editor.create_attribute("Last", {}, ElementRef{name}));
    CHECK(editor.set_attribute_rules(name, true, true, false));
    CHECK(editor.project().attributes.at(name).kind == AttributeKind::Composite);
    for (const auto& table : schema_preview(editor.project()).tables)
        if (table.origin == ElementRef{person})
            CHECK((key_names(table) == std::vector<std::string>{"First", "Last"}));
}

// An answer about an element, and a column the schema added to it, are as much
// part of that element's story as a remark pinned to it. Deleting the element
// used to leave them behind: validate() refuses a decision that points at
// nothing, so the deletion itself was refused, and the schema -- which nothing
// validates -- simply kept columns under a table that was no longer there.
void deletion_carries_away_answers_and_schema_edits() {
    TestIds ids;
    Editor editor(ids);

    // A table renamed in the schema does not prevent deleting what it came from.
    const auto student = entity(editor, "Student");
    CHECK(editor.set_table_name(ElementRef{student}, "Pupils"));
    CHECK(editor.erase({ElementRef{student}}, {}, {}, {}));
    CHECK(editor.project().decisions.table_name.empty());
    CHECK(editor.undo());
    CHECK(editor.project().entities.contains(student));
    CHECK(editor.project().decisions.table_name.at(relation_from(ElementRef{student})) == "Pupils");

    // Nor does the attribute an entity was told to identify itself by.
    const auto code = std::get<AttributeId>(
        *editor.create_attribute("Code", {}, AttributeOwner{ElementRef{student}}).created);
    CHECK(editor.set_entity_identifier(student, code));
    CHECK(editor.erase({ElementRef{code}}, {}, {}, {}));
    CHECK(editor.project().decisions.identifier.empty());

    // Nor the side of a relationship that was named as carrying the key.
    const auto desk = entity(editor, "Desk");
    const auto uses = std::get<RelationshipId>(*editor.relate(student, desk, {}, "uses").created);
    const auto side = editor.project().relationships.at(uses).participants.front().id;
    CHECK(editor.set_one_to_one_key(uses, side));
    CHECK(editor.erase({}, {{uses, side}}, {}, {}));
    CHECK(editor.project().decisions.one_to_one_key.empty());

    // A column the schema added, an attribute it was told to hide, and the
    // arrangement a hand gave the table all go with the element as well.
    const auto course = entity(editor, "Course");
    CHECK(editor.add_schema_column(ElementRef{course}, "Credits"));
    const auto draft = std::get<AttributeId>(
        *editor.create_attribute("Draft", {}, AttributeOwner{ElementRef{course}}).created);
    CHECK(editor.hide_in_schema(draft, true));
    CHECK(editor.move_schema_tables({{ElementRef{course}, Point{120, 80}}}));
    CHECK(editor.project().schema_layout.tables.size() == 1);
    CHECK(editor.erase({ElementRef{course}}, {}, {}, {}));
    CHECK(editor.project().schema.added.empty());
    CHECK(editor.project().schema.hidden.empty());
    CHECK(editor.project().schema_layout.tables.empty());

    // And one undo brings the table, its column, its hidden attribute and its
    // place back together, because all of it was one edit.
    CHECK(editor.undo());
    CHECK(editor.project().entities.contains(course));
    CHECK(editor.project().schema.added.size() == 1);
    CHECK(editor.project().schema.hidden.contains(draft));
    CHECK(editor.project().schema_layout.tables.size() == 1);
}

// The schema may be edited away from the diagram it came from. ADR-010 allows
// the two levels to differ, so a column added here only, or an attribute
// hidden from here only, is recorded rather than refused -- and the diagram is
// left exactly as it was.
void the_schema_may_differ_from_the_diagram() {
    TestIds ids;
    Editor editor(ids);
    const auto student = std::get<EntityId>(*editor.create_entity("Student", {0, 0, 160, 80}).created);
    const auto key = std::get<AttributeId>(*editor.create_attribute("StudentID", {}, ElementRef{student}).created);
    CHECK(editor.set_attribute_rules(key, true, true, true));
    const auto name = std::get<AttributeId>(*editor.create_attribute("Name", {}, ElementRef{student}).created);

    const auto columns_of = [&](const std::string& table) {
        std::vector<std::string> names;
        for (const auto& one : schema_preview(editor.project()).tables)
            if (one.name == table)
                for (const auto& column : one.columns) names.push_back(column.name);
        return names;
    };
    CHECK(columns_of("Students") == std::vector<std::string>({"StudentID", "Name"}));
    CHECK(editor.project().schema.empty());

    // A column added on the schema alone. The diagram gains nothing: no
    // attribute is created, and nothing it draws changes.
    const auto before = editor.project().attributes;
    CHECK(editor.add_schema_column(ElementRef{student}, "Nickname"));
    CHECK(editor.project().attributes == before);
    CHECK(columns_of("Students") == std::vector<std::string>({"StudentID", "Name", "Nickname"}));
    CHECK(editor.project().schema.added.at(relation_from(ElementRef{student})).size() == 1);

    // It has an identity of its own, so it can be renamed and removed by it.
    const auto added = editor.project().schema.added.at(relation_from(ElementRef{student})).front().id;
    CHECK(editor.rename_schema_column(added, "AlsoKnownAs"));
    CHECK(columns_of("Students") == std::vector<std::string>({"StudentID", "Name", "AlsoKnownAs"}));

    // An attribute hidden from the schema. Again the diagram keeps it: this is
    // a difference between the levels, not a deletion.
    CHECK(editor.hide_in_schema(name, true));
    CHECK(editor.project().attributes.contains(name));
    CHECK(editor.project().attributes.at(name).name == "Name");
    CHECK(columns_of("Students") == std::vector<std::string>({"StudentID", "AlsoKnownAs"}));

    // Every one of these is an ordinary edit, so every one of them undoes.
    CHECK(editor.undo());
    CHECK(columns_of("Students") == std::vector<std::string>({"StudentID", "Name", "AlsoKnownAs"}));
    CHECK(editor.undo());
    CHECK(columns_of("Students") == std::vector<std::string>({"StudentID", "Name", "Nickname"}));
    CHECK(editor.undo());
    CHECK(columns_of("Students") == std::vector<std::string>({"StudentID", "Name"}));
    CHECK(editor.project().schema.empty());
    CHECK(editor.redo());
    CHECK(columns_of("Students") == std::vector<std::string>({"StudentID", "Name", "Nickname"}));

    // Showing it again is the way back, and leaves nothing behind.
    CHECK(editor.erase_schema_column(added));
    CHECK(editor.project().schema.added.empty());
    CHECK(editor.hide_in_schema(name, true));
    CHECK(editor.hide_in_schema(name, false));
    CHECK(editor.project().schema.empty());
    CHECK(columns_of("Students") == std::vector<std::string>({"StudentID", "Name"}));

    // Reflecting a change instead needs none of this. It is an edit to the
    // model, and the schema follows because it is derived from it.
    CHECK(editor.create_attribute("Email", {}, ElementRef{student}));
    CHECK(columns_of("Students") == std::vector<std::string>({"StudentID", "Name", "Email"}));
    CHECK(editor.project().schema.empty());

    // A column cannot be added to something that is not there any more.
    CHECK(!editor.add_schema_column(ElementRef{EntityId{}}, "Nowhere"));
    CHECK(!editor.add_schema_column(ElementRef{student}, ""));
}

// The three ways a hierarchy becomes tables. All three are offered, so all
// three have to produce a schema somebody could build, not only the default.
void a_hierarchy_maps_three_ways() {
    TestIds ids;
    Editor editor(ids);
    const auto person = std::get<EntityId>(*editor.create_entity("Person", {0, 0, 160, 80}).created);
    const auto key = std::get<AttributeId>(*editor.create_attribute("PersonID", {}, ElementRef{person}).created);
    CHECK(editor.set_attribute_rules(key, true, true, true));
    CHECK(editor.create_attribute("BirthDate", {}, ElementRef{person}));
    const auto student = std::get<EntityId>(*editor.create_entity("Student", {0, 200, 160, 80}).created);
    CHECK(editor.create_attribute("Intake", {}, ElementRef{student}));
    const auto isa = std::get<SpecializationId>(
        *editor.create_specialization("IS A", {0, 120, 96, 74}, Inheritance::Generalization).created);
    CHECK(editor.set_supertype(isa, person));
    CHECK(editor.attach_subtype(isa, student));

    const auto tables_of = [&] {
        std::vector<std::string> names;
        for (const auto& table : schema_preview(editor.project()).tables) names.push_back(table.name);
        std::sort(names.begin(), names.end());
        return names;
    };
    const auto columns_of = [&](const std::string& wanted) {
        std::vector<std::string> names;
        for (const auto& table : schema_preview(editor.project()).tables)
            if (table.name == wanted)
                for (const auto& column : table.columns) names.push_back(column.name);
        return names;
    };

    // A table each, and the child takes the parent's key as a foreign key.
    CHECK(editor.set_isa_strategy(isa, IsaStrategy::PerSubclass));
    CHECK(tables_of() == std::vector<std::string>({"Persons", "Students"}));
    const auto per_subclass = schema_preview(editor.project());
    const auto child = std::find_if(per_subclass.tables.begin(), per_subclass.tables.end(),
                                    [](const PreviewTable& table) { return table.name == "Students"; });
    CHECK(child != per_subclass.tables.end());
    CHECK(child->origin_kind == TableOrigin::Subtype);
    CHECK(child->derives_from && *child->derives_from == ElementRef{person});

    // One table for the lot: the children stop being tables, their columns
    // move into the parent, and a discriminator says which kind a row is.
    CHECK(editor.set_isa_strategy(isa, IsaStrategy::SingleTable));
    CHECK(tables_of() == std::vector<std::string>({"Persons"}));
    const auto single = columns_of("Persons");
    CHECK(std::find(single.begin(), single.end(), "Intake") != single.end());
    CHECK(std::find(single.begin(), single.end(), "Type") != single.end());
    for (const auto& table : schema_preview(editor.project()).tables)
        for (const auto& column : table.columns)
            if (column.name == "Intake")
                CHECK(!column.required);   // only some rows have one

    // A table per concrete class: no parent table, and each child carries the
    // parent's columns itself.
    CHECK(editor.set_isa_strategy(isa, IsaStrategy::PerConcrete));
    CHECK(tables_of() == std::vector<std::string>({"Students"}));
    const auto concrete = columns_of("Students");
    CHECK(std::find(concrete.begin(), concrete.end(), "PersonID") != concrete.end());
    CHECK(std::find(concrete.begin(), concrete.end(), "BirthDate") != concrete.end());
    CHECK(std::find(concrete.begin(), concrete.end(), "Intake") != concrete.end());

    // Whichever strategy is in force, the question is still asked somewhere,
    // or the answer that removed a table could not be taken back.
    for (const auto strategy : {IsaStrategy::PerSubclass, IsaStrategy::SingleTable, IsaStrategy::PerConcrete}) {
        CHECK(editor.set_isa_strategy(isa, strategy));
        bool asked = false;
        for (const auto& table : schema_preview(editor.project()).tables)
            for (const auto& decision : table.decisions)
                if (decision.kind == DecisionKind::IsaStrategy && decision.about == ElementRef{isa})
                    asked = true;
        CHECK(asked);
    }

    // No foreign key is left pointing at a table that is no longer there.
    for (const auto strategy : {IsaStrategy::PerSubclass, IsaStrategy::SingleTable, IsaStrategy::PerConcrete}) {
        CHECK(editor.set_isa_strategy(isa, strategy));
        const auto preview = schema_preview(editor.project());
        for (const auto& table : preview.tables)
            for (const auto& column : table.columns)
                CHECK(!column.references || *column.references < preview.tables.size());
    }
}

// A name repeated across the schema is almost always one question asked over
// and over, so the repeats are gathered to be answered once.
void repeated_names_are_gathered() {
    TestIds ids;
    Editor editor(ids);
    const auto person = std::get<EntityId>(*editor.create_entity("Person", {0, 0, 160, 80}).created);
    const auto course = std::get<EntityId>(*editor.create_entity("Course", {400, 0, 160, 80}).created);
    const auto one = std::get<AttributeId>(*editor.create_attribute("Code", {}, ElementRef{person}).created);
    const auto two = std::get<AttributeId>(*editor.create_attribute("Code", {}, ElementRef{course}).created);
    CHECK(editor.create_attribute("Alone", {}, ElementRef{person}));

    auto groups = shared_names(schema_preview(editor.project()));
    CHECK(groups.size() == 1);
    CHECK(groups.front().name == "Code");
    CHECK(groups.front().columns == 2);
    CHECK(groups.front().attributes.size() == 2);

    // Answered once for all of them, in one edit, so one undo takes it back.
    CHECK(editor.set_logical_types(groups.front().attributes, LogicalType::Varchar));
    CHECK(editor.project().attributes.at(one).logical_type == LogicalType::Varchar);
    CHECK(editor.project().attributes.at(two).logical_type == LogicalType::Varchar);
    CHECK(shared_names(schema_preview(editor.project())).empty());
    CHECK(editor.undo());
    CHECK(editor.project().attributes.at(one).logical_type == LogicalType::Unset);
    CHECK(editor.project().attributes.at(two).logical_type == LogicalType::Unset);
    CHECK(shared_names(schema_preview(editor.project())).size() == 1);
}

// Two links to the same table are told apart by the roles they were given,
// which is the only thing that can tell them apart.
void roles_name_the_keys_they_carry() {
    TestIds ids;
    Editor editor(ids);
    const auto airport = std::get<EntityId>(*editor.create_entity("Airport", {0, 0, 160, 80}).created);
    const auto flight = std::get<EntityId>(*editor.create_entity("Flight", {400, 0, 160, 80}).created);
    const auto key = std::get<AttributeId>(*editor.create_attribute("AirportID", {}, ElementRef{airport}).created);
    CHECK(editor.set_attribute_rules(key, true, true, true));

    const auto leaves = std::get<RelationshipId>(*editor.create_relationship("Departs", {200, 100, 190, 110}).created);
    const auto from_side = editor.connect(leaves, flight);
    const auto to_side = editor.connect(leaves, airport);
    CHECK(from_side && to_side);
    CHECK(editor.update_participant(leaves, *from_side.participant, Cardinality::Many, Participation::Total, ""));
    CHECK(editor.update_participant(leaves, *to_side.participant, Cardinality::One, Participation::Total, "Departure"));

    const auto arrives = std::get<RelationshipId>(*editor.create_relationship("Arrives", {200, 260, 190, 110}).created);
    const auto back_side = editor.connect(arrives, flight);
    const auto at_side = editor.connect(arrives, airport);
    CHECK(back_side && at_side);
    CHECK(editor.update_participant(arrives, *back_side.participant, Cardinality::Many, Participation::Total, ""));
    CHECK(editor.update_participant(arrives, *at_side.participant, Cardinality::One, Participation::Total, "Arrival"));

    std::vector<std::string> names;
    for (const auto& table : schema_preview(editor.project()).tables)
        if (table.name == "Flights")
            for (const auto& column : table.columns)
                if (column.foreign_key) names.push_back(column.name);
    std::sort(names.begin(), names.end());
    CHECK(names == std::vector<std::string>({"ArrivalAirportID", "DepartureAirportID"}));
}

// English is irregular, so the plural a table is named by needs rules and a
// word list, and has to leave alone the words that have no plural.
void tables_are_named_for_many_rows() {
    CHECK(plural_of("Student") == "Students");
    CHECK(plural_of("Course") == "Courses");
    CHECK(plural_of("Company") == "Companies");
    CHECK(plural_of("Address") == "Addresses");
    CHECK(plural_of("Box") == "Boxes");
    CHECK(plural_of("Knife") == "Knives");
    CHECK(plural_of("Hero") == "Heroes");
    CHECK(plural_of("Analysis") == "Analyses");
    CHECK(plural_of("Child") == "Children");
    CHECK(plural_of("AccessCard") == "AccessCards");
    CHECK(plural_of("Person") == "Persons");
    // Already plural, or no plural at all.
    CHECK(plural_of("Employees") == "Employees");
    CHECK(plural_of("Media") == "Media");
    CHECK(plural_of("JSON") == "JSON");
    CHECK(plural_of("Series") == "Series");
}

// A box is pulled by whichever edge the hand reaches for, on the diagram and
// on the schema alike: the side that is pulled moves and the side opposite it
// stays where it was, which is why a pull on the left carries a place with it.
void boxes_are_pulled_by_any_of_their_edges() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const ElementRef ref{student};
    CHECK(editor.move({{ref, {100, 100, 160, 80}}}));

    // The right edge: wider, and standing where it always stood.
    CHECK(editor.resize_entities({{ref, {100, 100, 260, 80}}}));
    CHECK(editor.undo_label() == "Resize entity");
    CHECK((editor.project().layout.at(ref) == Rect{100, 100, 260, 80}));
    // The left edge: the right-hand side stays put and the box reaches left.
    CHECK(editor.resize_entities({{ref, {40, 100, 320, 80}}}));
    CHECK((editor.project().layout.at(ref) == Rect{40, 100, 320, 80}));
    // The top edge, which moves the corner in the other direction.
    CHECK(editor.resize_entities({{ref, {40, 60, 320, 120}}}));
    CHECK((editor.project().layout.at(ref) == Rect{40, 60, 320, 120}));
    // Undo walks back one pull at a time, and redo puts it out again.
    CHECK(editor.undo());
    CHECK((editor.project().layout.at(ref) == Rect{40, 100, 320, 80}));
    CHECK(editor.redo());
    CHECK((editor.project().layout.at(ref) == Rect{40, 60, 320, 120}));

    // A pull that runs past what a box may be asks for the end of the range
    // rather than for nothing to happen.
    CHECK(editor.resize_entities({{ref, {40, 60, 4, 4}}}));
    CHECK(editor.project().layout.at(ref).width == min_entity_width);
    CHECK(editor.project().layout.at(ref).height == min_entity_height);
    CHECK(editor.resize_entities({{ref, {40, 60, 90000, 90000}}}));
    CHECK(editor.project().layout.at(ref).width == max_entity_width);
    CHECK(editor.project().layout.at(ref).height == max_entity_height);

    // And nothing else on the diagram is pulled about this way.
    const auto note = editor.create_note("Note", {0, 0, 200, 120}, "");
    CHECK(note && note.created);
    CHECK(!editor.resize_entities({{*note.created, {0, 0, 300, 200}}}));
    CHECK((editor.project().layout.at(*note.created) == Rect{0, 0, 200, 120}));
}

// A schema table is pulled the same way, and its arrangement is part of the
// document: the size it was given and the place a left or top edge moved it to
// are one edit, so one undo takes both back together.
void schema_tables_are_pulled_by_any_of_their_edges() {
    TestIds ids;
    Editor editor(ids);
    const auto student = entity(editor, "Student");
    const ElementRef ref{student};

    // The right edge alone: a width, and no place at all, so the table still
    // follows the automatic arrangement.
    CHECK(editor.resize_schema_tables({{ref, SchemaTableBox{300, 0, std::nullopt}}}));
    CHECK(editor.undo_label() == "Resize on the schema");
    CHECK(editor.project().schema_layout.widths.at(relation_from(ref)) == 300);
    CHECK(editor.project().schema_layout.heights.empty());
    CHECK(editor.project().schema_layout.tables.empty());

    // The bottom edge: a height beside the width, and still no place.
    CHECK(editor.resize_schema_tables({{ref, SchemaTableBox{300, 240, std::nullopt}}}));
    CHECK(editor.project().schema_layout.heights.at(relation_from(ref)) == 240);
    CHECK(editor.project().schema_layout.tables.empty());

    // The left edge, which moves the table as it sizes it. Both arrive in one
    // edit, so one undo takes the size and the place back together.
    CHECK(editor.resize_schema_tables({{ref, SchemaTableBox{380, 240, Point{60, 40}}}}));
    CHECK(editor.project().schema_layout.widths.at(relation_from(ref)) == 380);
    CHECK((editor.project().schema_layout.tables.at(relation_from(ref)) == Point{60, 40}));
    CHECK(editor.undo());
    CHECK(editor.project().schema_layout.widths.at(relation_from(ref)) == 300);
    CHECK(editor.project().schema_layout.tables.empty());
    CHECK(editor.redo());
    CHECK((editor.project().schema_layout.tables.at(relation_from(ref)) == Point{60, 40}));

    // Pulled past what a table may be, it stops at the end of the range.
    CHECK(editor.resize_schema_tables({{ref, SchemaTableBox{9000, 9000, std::nullopt}}}));
    CHECK(editor.project().schema_layout.widths.at(relation_from(ref)) == max_table_width);
    CHECK(editor.project().schema_layout.heights.at(relation_from(ref)) == max_table_height);
    CHECK(editor.resize_schema_tables({{ref, SchemaTableBox{1, 1, std::nullopt}}}));
    CHECK(editor.project().schema_layout.widths.at(relation_from(ref)) == min_table_width);
    CHECK(editor.project().schema_layout.heights.at(relation_from(ref)) == min_table_height);

    // Nothing said about a size is the same as never having been pulled that
    // way, so the entry goes rather than holding a figure that means nothing.
    CHECK(editor.resize_schema_tables({{ref, SchemaTableBox{0, 0, std::nullopt}}}));
    CHECK(editor.project().schema_layout.widths.empty());
    CHECK(editor.project().schema_layout.heights.empty());
}

int main() {
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"identity and work in progress", identity_and_work_in_progress},
        {"the diagram becomes tables", the_diagram_becomes_tables},
        {"drawn keys are the primary key", drawn_keys_are_the_primary_key},
        {"tables are named for many rows", tables_are_named_for_many_rows},
        {"commands and stable undo", commands_and_stable_undo},
        {"project descriptions are bounded undoable edits", project_description_is_a_bounded_undoable_edit},
        {"clean states, branching and revisions", clean_state_branching_and_revisions},
        {"atomic command rejection", rejection_is_atomic},
        {"composite ownership and attribute kinds", composite_ownership_and_kind_rules},
        {"recursive participant identity and bounds", participants_recursive_roles_and_bounds},
        {"complete deletion undo", deletion_restores_complete_graph},
        {"selected subgraph duplication", duplicate_remaps_selected_subgraph},
        {"hostile model validation", hostile_models_are_rejected},
        {"hostile schema state validation", hostile_schema_state_is_rejected},
        {"schema column identities are guarded", schema_column_identities_are_guarded},
        {"whole-object edits cost the budget", whole_object_edits_cost_the_budget},
        {"comment identities survive opening", comment_identities_survive_opening},
        {"schema names reach the diagram", schema_names_reach_the_diagram},
        {"relations have their own identity", relations_have_their_own_identity},
        {"schema keys reach the diagram", schema_keys_reach_the_diagram},
        {"schema constraints reach the diagram", schema_constraints_reach_the_diagram},
        {"columns can count themselves up", columns_can_count_themselves_up},
        {"invented keys carry constraints", invented_keys_carry_constraints},
        {"limits, deep ownership and compact history", limits_deep_ownership_and_compact_history},
        {"connector shapes follow their link", connector_shapes_follow_their_link},
        {"connections can be pinned as they are made", connections_can_be_pinned_as_they_are_made},
        {"pictures and notes are placed like elements", pictures_and_notes_are_placed_like_elements},
        {"lettering follows resizing by hand", lettering_follows_resizing_by_hand},
        {"relating two entities is one edit", relating_two_entities_is_one_edit},
        {"weak entities and identifying relationships", weak_entities_and_identifying_relationships},
        {"hostile connector shapes", hostile_connector_shapes_are_rejected},
        {"associative relationships act as entities", associative_relationships_act_as_entities},
        {"associative graph branches and cycles", associative_graph_branches_and_cycles},
        {"inheritance graph branches and cycles", inheritance_graph_branches_and_cycles},
        {"deep relationship and inheritance graphs", deep_relationship_and_inheritance_graphs},
        {"coloured specialization cascade restores exactly", coloured_specialization_cascade_restores_exactly},
        {"specializations carry inheritance rules", specializations_carry_inheritance_rules},
        {"binary ratios and reversal", binary_ratios_and_reversal},
        {"comments are pinned to things", comments_are_pinned_to_things},
        {"convertible mode asks what it becomes", the_model_says_what_it_becomes},
        {"the schema may differ from the diagram", the_schema_may_differ_from_the_diagram},
        {"a hierarchy maps three ways", a_hierarchy_maps_three_ways},
        {"repeated names are gathered", repeated_names_are_gathered},
        {"boxes are pulled by any of their edges", boxes_are_pulled_by_any_of_their_edges},
        {"schema tables are pulled by any of their edges", schema_tables_are_pulled_by_any_of_their_edges},
        {"roles name the keys they carry", roles_name_the_keys_they_carry},
        {"deletion carries away answers and schema edits", deletion_carries_away_answers_and_schema_edits},
    };
    std::size_t failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures == 0 ? 0 : 1;
}
