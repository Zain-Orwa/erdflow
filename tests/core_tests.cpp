#include "application/editor.hpp"

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
}

// A picture and a note are placed elements without being database objects:
// named, described, moved, coloured, copied and deleted through the same
// commands as everything else, owning nothing and connected to nothing.
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
        relationships.relationships.emplace(rel, Relationship{rel, "Associative", {}, true, {}});
        relationships.layout.emplace(ElementRef{rel}, Rect{});
        const EntityId ent{ids.next()};
        entity_ids.push_back(ent);
        inheritance.entities.emplace(ent, Entity{ent, "Entity", {}});
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
        {"connections can be pinned as they are made", connections_can_be_pinned_as_they_are_made},
        {"pictures and notes are placed like elements", pictures_and_notes_are_placed_like_elements},
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
    };
    std::size_t failures = 0;
    for (const auto& [name, test] : tests) {
        try { test(); std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { ++failures; std::cerr << "FAIL " << name << ": " << error.what() << '\n'; }
    }
    return failures == 0 ? 0 : 1;
}
