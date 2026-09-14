#pragma once

#include "domain/model.hpp"

#include <memory>
#include <string>
#include <utility>

namespace erdflow::application {

class IdGenerator {
public:
    virtual ~IdGenerator() = default;
    virtual domain::Uuid next() = 0;
};

struct EditResult {
    bool ok = true;
    std::string error;
    std::optional<domain::ElementRef> created;
    std::optional<domain::ParticipantId> participant;
    explicit operator bool() const { return ok; }
};

class Editor {
public:
    explicit Editor(IdGenerator& ids);
    ~Editor();
    Editor(const Editor&) = delete;
    Editor& operator=(const Editor&) = delete;

    [[nodiscard]] const domain::Project& project() const;
    [[nodiscard]] bool dirty() const;
    [[nodiscard]] std::uint64_t revision() const;
    [[nodiscard]] bool can_undo() const;
    [[nodiscard]] bool can_redo() const;
    [[nodiscard]] std::string undo_label() const;
    [[nodiscard]] std::string redo_label() const;
    [[nodiscard]] std::size_t history_bytes() const;

    void new_project();
    EditResult replace_project(domain::Project project);
    void mark_saved(std::uint64_t revision);
    EditResult rename_project(std::string name);
    EditResult create_entity(std::string name, domain::Rect rect);
    EditResult create_attribute(std::string name, domain::Rect rect,
                                std::optional<domain::AttributeOwner> owner = {});
    EditResult create_relationship(std::string name, domain::Rect rect);
    // An ISA triangle: one supertype, and the subtypes attached to it. The
    // constraint and completeness decide how it converts to relations later.
    // The triangle is placed first and wired up afterwards, so it starts with
    // neither a supertype nor subtypes.
    EditResult create_specialization(std::string name, domain::Rect rect, domain::Inheritance direction);
    EditResult set_supertype(domain::SpecializationId specialization, std::optional<domain::EntityId> supertype);
    EditResult set_inheritance_direction(domain::SpecializationId specialization, domain::Inheritance direction);
    EditResult attach_subtype(domain::SpecializationId specialization, domain::EntityId subtype);
    EditResult detach_subtype(domain::SpecializationId specialization, domain::EntityId subtype);
    EditResult set_specialization_rules(domain::SpecializationId specialization,
                                        domain::Disjointness constraint, domain::Completeness completeness);
    EditResult rename(domain::ElementRef ref, std::string name);
    EditResult describe(domain::ElementRef ref, std::string description);
    EditResult set_attribute_kind(domain::AttributeId id, domain::AttributeKind kind);
    EditResult set_attribute_owner(domain::AttributeId id,
                                   std::optional<domain::AttributeOwner> owner);
    EditResult connect(domain::RelationshipId relationship, domain::ParticipantTarget target);
    // An associative relationship carries its own identity and may then take
    // part in further relationships, as an entity would.
    // Passing a body resizes the relationship in the same edit, so adopting or
    // dropping the associative shape is a single undo step.
    EditResult set_associative(domain::RelationshipId relationship, bool associative,
                               std::optional<domain::Rect> body = {});
    EditResult update_participant(domain::RelationshipId relationship,
                                   domain::ParticipantId participant,
                                   domain::Cardinality maximum,
                                   domain::Participation participation,
                                   std::string role);
    // Whether one side's constraints are drawn. This changes the diagram, not
    // the model: the side keeps its maximum and minimum either way.
    EditResult show_participant_constraints(domain::RelationshipId relationship,
                                            domain::ParticipantId participant, bool shown);
    // A binary relationship's ratio is its two participants' maximums read
    // together, so 1:1, 1:M, M:1 and M:M are set as one edit rather than two.
    EditResult set_ratio(domain::RelationshipId relationship,
                         domain::Cardinality first, domain::Cardinality second);
    // Swap the constraints between the two participants, turning 1:M into M:1
    // without having to work out which participant is listed first.
    EditResult reverse_participants(domain::RelationshipId relationship);
    EditResult disconnect(domain::RelationshipId relationship, domain::ParticipantId participant);
    EditResult move(const std::map<domain::ElementRef, domain::Rect>& positions);
    // A connector carries one signed perpendicular bend. Passing no offset
    // restores automatic routing rather than storing a zero-length bend.
    EditResult bend_connector(domain::ConnectorRef ref, std::optional<double> offset);
    // Gives every named element the same surface colour, or clears the colour
    // from all of them, as one edit. An element with no colour of its own is
    // drawn in whatever its theme gives its kind.
    EditResult recolour(const std::vector<domain::ElementRef>& elements,
                        std::optional<domain::Colour> colour);
    // Routes a connector through a list of points, in the order they are met
    // walking from its source to its target. Passing none straightens it. A
    // route supersedes the single bend, which is cleared in the same edit.
    EditResult route_connector(domain::ConnectorRef ref, std::vector<domain::Point> waypoints);
    // Pins where a connector meets each shape, as a direction in radians from
    // that shape's centre, so the joins stop sliding as the shapes are moved.
    // Passing no anchors unpins it. The bend is left alone either way.
    EditResult pin_connector(domain::ConnectorRef ref, std::optional<double> owner_anchor,
                             std::optional<double> child_anchor);
    EditResult erase(const std::vector<domain::ElementRef>& elements,
                     const std::vector<std::pair<domain::RelationshipId, domain::ParticipantId>>& participants = {},
                     const std::vector<domain::AttributeId>& detached_attributes = {});
    EditResult duplicate(const std::vector<domain::ElementRef>& elements, double dx = 32, double dy = 32);
    EditResult undo();
    EditResult redo();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace erdflow::application
