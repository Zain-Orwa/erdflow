#pragma once

#include "domain/model.hpp"

#include <cstdint>
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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
    // Visual aids. A picture is inserted with the encoded bytes of its image,
    // which are kept as given; a note with the text drawn beneath its title.
    // Both are then named, described, moved, coloured, copied and deleted like
    // any other element, through the same commands.
    EditResult create_picture(std::string name, domain::Rect rect, std::vector<std::uint8_t> image);
    EditResult create_note(std::string name, domain::Rect rect, std::string text = {});
    // One character placed on the diagram on its own, drawn bare. It is a note
    // underneath, so everything that can be done to a note can be done to it;
    // it is a separate command only so that the history says "symbol".
    EditResult create_symbol(std::string character, domain::Rect rect);
    EditResult set_supertype(domain::SpecializationId specialization, std::optional<domain::EntityId> supertype);
    EditResult set_inheritance_direction(domain::SpecializationId specialization, domain::Inheritance direction);
    EditResult attach_subtype(domain::SpecializationId specialization, domain::EntityId subtype);
    EditResult detach_subtype(domain::SpecializationId specialization, domain::EntityId subtype);
    EditResult set_specialization_rules(domain::SpecializationId specialization,
                                        domain::Disjointness constraint, domain::Completeness completeness);
    // Review remarks. A comment is pinned to things rather than placed on the
    // canvas, so it is not an element and has commands of its own. One remark
    // may be pinned to several things at once, which is how a single note
    // covers a whole area of a diagram.
    EditResult create_comment(std::string text, std::vector<domain::CommentTarget> targets);
    EditResult set_comment_text(domain::CommentId id, std::string text);
    // What the remark is pinned to, as a whole. Pinning it to nothing is
    // refused: a comment pinned to nothing could never be found again, so a
    // caller that wants it gone deletes it instead.
    EditResult set_comment_targets(domain::CommentId id, std::vector<domain::CommentTarget> targets);
    // Put one remark away, or bring it back, without deleting it. This travels
    // with the document, unlike the switch that hides every comment at once,
    // which is a matter of how the diagram is being looked at.
    EditResult set_comment_hidden(domain::CommentId id, bool hidden);
    EditResult erase_comment(domain::CommentId id);
    EditResult rename(domain::ElementRef ref, std::string name);
    EditResult describe(domain::ElementRef ref, std::string description);
    EditResult set_attribute_kind(domain::AttributeId id, domain::AttributeKind kind);
    // Either link can be given a shape as it is made, so a connection drawn
    // by clicking two points is pinned to those points in the same edit
    // rather than joined automatically and pinned afterwards. An automatic
    // shape stores nothing, which is what the plain call has always done.
    EditResult set_attribute_owner(domain::AttributeId id,
                                   std::optional<domain::AttributeOwner> owner,
                                   domain::Connector shape = {});
    EditResult connect(domain::RelationshipId relationship, domain::ParticipantTarget target,
                       domain::Connector shape = {});
    // Two entities have no line of their own: they are joined through a
    // relationship. This creates that relationship and both of its sides as
    // one edit, so the whole thing is one step of history rather than three,
    // and reports the new relationship as what it created.
    EditResult relate(domain::EntityId first, domain::EntityId second, domain::Rect body,
                      std::string name = "Relationship",
                      domain::Connector first_shape = {}, domain::Connector second_shape = {});
    // An associative relationship carries its own identity and may then take
    // part in further relationships, as an entity would.
    // Passing a body resizes the relationship in the same edit, so adopting or
    // dropping the associative shape is a single undo step.
    EditResult set_associative(domain::RelationshipId relationship, bool associative,
                               std::optional<domain::Rect> body = {});
    // Whether an entity is weak: identified through an identifying relationship
    // rather than by a key of its own.
    EditResult set_entity_weak(domain::EntityId id, bool weak);
    // Regular, identifying or associative, as one edit. A body resizes the
    // relationship in the same edit, as set_associative does, so taking or
    // leaving the associative shape stays a single undo step.
    EditResult set_relationship_kind(domain::RelationshipId relationship, domain::RelationshipKind kind,
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
    // A symbol's box is how big its character is drawn: the character is grown
    // to fill whatever room the box gives it. That makes size a property of a
    // symbol in a way it is not of an entity, whose box is sized by the name
    // it has to hold, so this is its own named edit rather than a move that
    // happens to change a width. It takes several because a selection of
    // symbols is enlarged together, and it refuses anything that is not a
    // symbol so the command cannot quietly reshape the rest of the diagram.
    EditResult resize_symbols(const std::map<domain::ElementRef, domain::Rect>& boxes);
    // A connector carries one signed perpendicular bend. Passing no offset
    // restores automatic routing rather than storing a zero-length bend.
    EditResult bend_connector(domain::ConnectorRef ref, std::optional<double> offset);
    // Gives every named element the same surface colour, or clears the colour
    // from all of them, as one edit. An element with no colour of its own is
    // drawn in whatever its theme gives its kind.
    EditResult recolour(const std::vector<domain::ElementRef>& elements,
                        std::optional<domain::Colour> colour);
    // Makes every named element's surface the given percent see-through, as
    // one edit. Zero is solid and stores nothing; it applies over whatever
    // colour each element has, so the theme's own colour can be faded too.
    EditResult set_transparency(const std::vector<domain::ElementRef>& elements, std::uint8_t percent);
    // The paper the diagram is drawn on: a ruling, a picture of the user's
    // own, or the plain colour the theme gives the canvas.
    EditResult set_background(domain::Background background);
    // Routes a connector through a list of points, in the order they are met
    // walking from its source to its target. Passing none straightens it. A
    // route supersedes the single bend, which is cleared in the same edit.
    EditResult route_connector(domain::ConnectorRef ref, std::vector<domain::Point> waypoints);
    // Pins where a connector meets each shape, as a direction in radians from
    // that shape's centre, so the joins stop sliding as the shapes are moved.
    // Passing no anchors unpins it. The bend is left alone either way.
    EditResult pin_connector(domain::ConnectorRef ref, std::optional<double> owner_anchor,
                             std::optional<double> child_anchor);
    // The whole of a connector's shape at once, for a gesture that settles
    // more than one part of it: an end put down in open canvas pins its join
    // and leaves a corner where it stopped, and that is one thing the user
    // did. A shape that says nothing stores nothing, as always.
    EditResult shape_connector(domain::ConnectorRef ref, domain::Connector shape);
    // Several connectors reshaped together, for a change made to a whole
    // selection rather than to one line. The label is given rather than fixed
    // because the same write serves locking and releasing, and the history
    // ought to say which was done.
    EditResult shape_connectors(const std::map<domain::ConnectorRef, domain::Connector>& shapes,
                                std::string label);
    // Review 2026-09-15, finding 1: an inheritance link is selectable, so it
    // can be deleted alongside anything else. Each entry names the triangle
    // and the subtype whose link goes; no subtype means the link up to the
    // supertype. They are detached in the same edit as the rest, so a mixed
    // deletion stays one step of history.
    using InheritanceLink = std::pair<domain::SpecializationId, std::optional<domain::EntityId>>;
    EditResult erase(const std::vector<domain::ElementRef>& elements,
                     const std::vector<std::pair<domain::RelationshipId, domain::ParticipantId>>& participants = {},
                     const std::vector<domain::AttributeId>& detached_attributes = {},
                     const std::vector<InheritanceLink>& detached_inheritance = {});
    EditResult duplicate(const std::vector<domain::ElementRef>& elements, double dx = 32, double dy = 32);
    EditResult undo();
    EditResult redo();

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace erdflow::application
