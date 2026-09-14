#include "application/editor.hpp"

#include <algorithm>
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

void limits_deep_ownership_and_compact_history() {
    TestIds ids;
    Editor editor(ids);
    Project large;
    large.id = ProjectId{ids.next()};
    EntityId first;
    for (std::size_t i = 0; i < max_elements; ++i) {
        const EntityId id{ids.next()};
        if (i == 0) first = id;
        large.entities.emplace(id, Entity{id, "Entity", {}});
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
        nested.attributes.emplace(id, Attribute{id, "Composite", {}, AttributeKind::Composite, owner});
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
    CHECK(editor.project().connectors.at(ConnectorRef{side}) == 40);
    CHECK(editor.bend_connector(ConnectorRef{grade}, -25));
    CHECK(editor.project().connectors.size() == 2);

    // Passing no offset restores automatic routing rather than storing zero.
    CHECK(editor.bend_connector(ConnectorRef{grade}, {}));
    CHECK(!editor.project().connectors.contains(ConnectorRef{grade}));
    CHECK(editor.undo());
    CHECK(editor.project().connectors.at(ConnectorRef{grade}) == -25);

    // Re-bending the same connector to its current shape is not an edit.
    const auto revision = editor.revision();
    CHECK(editor.bend_connector(ConnectorRef{side}, 40));
    CHECK(editor.revision() == revision);

    // A shape cannot outlive its link, and undo restores both together.
    CHECK(editor.set_attribute_owner(grade, {}));
    CHECK(!editor.project().connectors.contains(ConnectorRef{grade}));
    CHECK(!blocks(editor.project()));
    CHECK(editor.undo());
    CHECK(editor.project().connectors.at(ConnectorRef{grade}) == -25);

    CHECK(editor.disconnect(enrolled, side));
    CHECK(!editor.project().connectors.contains(ConnectorRef{side}));
    CHECK(!blocks(editor.project()));
    CHECK(editor.undo());
    CHECK(editor.project().connectors.at(ConnectorRef{side}) == 40);

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
            CHECK(editor.project().connectors.at(ConnectorRef{participant.id}) == 40);
            ++carried;
        }
    }
    CHECK(carried == 1);
    CHECK(!blocks(editor.project()));
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
    project.connectors.emplace(ConnectorRef{ParticipantId{}}, 10.0);
    CHECK(blocks(project));

    for (const auto bad : {std::numeric_limits<double>::quiet_NaN(),
                           std::numeric_limits<double>::infinity(), 1e9}) {
        project = editor.project();
        project.connectors.insert_or_assign(ConnectorRef{side}, bad);
        CHECK(blocks(project));
    }
    // A bend the editor would accept must also survive validation directly.
    project = editor.project();
    project.connectors.insert_or_assign(ConnectorRef{side}, -99999.0);
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

} // namespace

int main() {
    const std::pair<const char*, std::function<void()>> tests[] = {
        {"identity and work in progress", identity_and_work_in_progress},
        {"commands and stable undo", commands_and_stable_undo},
        {"clean states, branching and revisions", clean_state_branching_and_revisions},
        {"atomic command rejection", rejection_is_atomic},
        {"composite ownership and attribute kinds", composite_ownership_and_kind_rules},
        {"recursive participant identity and bounds", participants_recursive_roles_and_bounds},
        {"complete deletion undo", deletion_restores_complete_graph},
        {"selected subgraph duplication", duplicate_remaps_selected_subgraph},
        {"hostile model validation", hostile_models_are_rejected},
        {"limits, deep ownership and compact history", limits_deep_ownership_and_compact_history},
        {"connector shapes follow their link", connector_shapes_follow_their_link},
        {"hostile connector shapes", hostile_connector_shapes_are_rejected},
        {"associative relationships act as entities", associative_relationships_act_as_entities},
    };
    std::size_t failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures == 0 ? 0 : 1;
}
