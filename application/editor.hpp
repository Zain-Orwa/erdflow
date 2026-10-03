// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "application/history.hpp"
#include "domain/diagram_from_schema.hpp"
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
    // The History (Zain, 2026-09-26): every step still kept, oldest first,
    // each described in words. The first history_position() of them are in
    // effect; the rest have been undone and wait to be redone, until a new
    // edit takes their place, exactly as Undo and Redo have always had them.
    [[nodiscard]] std::vector<HistoryEntry> history() const;
    [[nodiscard]] std::size_t history_position() const;
    // Undoes or redoes, one step at a time, until that many steps are in
    // effect: 0 is the project as it was before the oldest step kept. The
    // steps themselves are unchanged by the move, so Undo and Redo carry on
    // from wherever it lands.
    EditResult go_to(std::size_t position);

    void new_project();
    // A project that starts from its schema (Zain, 2026-09-27): no diagram,
    // and tables made on the schema itself until it is converted into one.
    // Like new_project it is a fresh start rather than an edit, so undo does
    // not reach back past it.
    void new_schema_project();
    // Tables made by hand, in a project that starts from its schema. A new
    // table is put where it is asked for, and begins with one column, ID, its
    // primary key, so there is a row to type over and a key to point at.
    EditResult create_relation(std::string name, std::optional<domain::Point> at = {});
    EditResult erase_relation(domain::RelationId id);
    // One column made to point at a table's primary key -- another table's,
    // or its own table's, as a manager's is. The column takes the key's type
    // in the same edit, since a foreign key and what it points at must agree.
    // A column already pointing somewhere keeps its foreign key, pointed
    // at the new place.
    EditResult add_foreign_key(domain::RelationId from, domain::SchemaColumnId column,
                               domain::RelationId to, domain::SchemaColumnId target);
    EditResult erase_foreign_key(domain::ForeignKeyId id);
    // A foreign key made by Connect (Zain, 2026-10-01, Stage 5 of the Schema
    // workspace): the referencing table's column pointing at the referenced
    // table's key, in one edit, so one Undo takes all of it back. Either an
    // existing column of the referencing table becomes the foreign key, or,
    // where none is given, a column is made for it, called `name` and given
    // the key's type and size and nothing else of the key's -- not its key
    // role, its uniqueness or its counting up. Refused whole, never in part:
    // where the column already references anything (a foreign key is never
    // re-pointed here, and never doubled), where its type would have to change
    // to match the key's, where a new column's name is already the table's,
    // or where the key is not the referenced table's only key column. Nothing
    // that already exists loses anything it was: a key column used this way
    // stays a key. `made`, where given, is told the foreign key's identity.
    EditResult connect_foreign_key(domain::RelationId referencing, std::optional<domain::SchemaColumnId> column,
                                   std::string name, domain::RelationId referenced, domain::SchemaColumnId key,
                                   domain::ForeignKeyId* made = nullptr);
    // The schema drawn as the Conceptual ERD it would have come from, in one
    // edit (Zain, 2026-09-27). Afterwards the diagram is the model and the
    // schema follows from it, as in a project begun as a diagram. `tables` is
    // where each table sits on the schema now, where it stays; what could not
    // be carried across exactly is told in `notes`, one sentence each.
    EditResult convert_schema_to_diagram(const std::map<domain::RelationId, domain::Point>& tables,
                                         const domain::DiagramSizes& sizes,
                                         std::vector<std::string>* notes = nullptr);
    EditResult replace_project(domain::Project project);
    void mark_saved(std::uint64_t revision);
    EditResult rename_project(std::string name);
    EditResult describe_project(std::string description);
    EditResult create_entity(std::string name, domain::Rect rect);
    // A shape for the link to its owner can be given with it, as for
    // set_attribute_owner, so an attribute placed pinned to a point on its
    // owner is one step of history.
    EditResult create_attribute(std::string name, domain::Rect rect,
                                std::optional<domain::AttributeOwner> owner = {},
                                domain::Connector shape = {});
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
    // Brings another project's contents into this one, as one edit. Everything
    // arrives with fresh identities, so the two projects can share names and
    // even have been copied from one another without anything colliding, and
    // the whole import undoes in a single step. The offset moves what arrives
    // clear of what is already drawn. The paper and the project's own name are
    // left alone: what is imported is the work, not the document around it.
    EditResult merge_project(const domain::Project& other, double dx, double dy);
    EditResult rename(domain::ElementRef ref, std::string name);
    EditResult describe(domain::ElementRef ref, std::string description);
    // What this becomes in the schema's own words, for the three things that
    // become tables and columns. Refused for anything else, so a note or a
    // picture never carries a comment nothing will ever read.
    EditResult set_schema_comment(domain::ElementRef ref, std::string comment);
    // What an attribute becomes: a portable type, and the length where the type
    // takes one. A type that takes no length is given none, whatever it was
    // told before, so a Text(100) changed to a Boolean does not keep the 100.
    EditResult set_logical_type(domain::AttributeId id, domain::LogicalType type, std::uint32_t length = 0);
    // The same answer given to several attributes in one edit, which is how a
    // name repeated across a schema is answered once. One edit, so one undo
    // takes all of it back rather than unpicking it column by column.
    EditResult set_logical_types(const std::vector<domain::AttributeId>& ids, domain::LogicalType type);
    // How long, or how precise, without touching the type itself. A number is
    // only kept where the type takes one: a scale belongs to a precision, a
    // length to a measured type, and a type that takes neither keeps neither.
    EditResult set_type_size(domain::AttributeId id, std::uint32_t length, std::uint32_t scale);
    EditResult set_schema_column_size(domain::SchemaColumnId id, std::uint32_t length,
                                      std::uint32_t scale);
    // What the table will enforce: part of the identity, must be filled in, no
    // two rows alike. Set together, because they are read together.
    EditResult set_attribute_rules(domain::AttributeId id, bool identifier, bool required, bool unique);
    EditResult set_attribute_kind(domain::AttributeId id, domain::AttributeKind kind);
    // Making a column the primary key, or taking the key off it, as asked for
    // from the Relational Schema.
    //
    // This is both halves at once on purpose. The identifier rule and the Chen
    // key oval are kept apart everywhere else, because on the diagram they are
    // set at different stages by different people; but somebody working on the
    // schema who says "this is the key" means it about the model, and a change
    // made there is meant to show on the diagram. So the rule is set and the
    // attribute is drawn as a key, in one edit and so in one step of history.
    //
    // A key is also never empty, so it is made required in the same edit. A
    // composite or multivalued attribute cannot be a key and is refused with a
    // reason rather than quietly reshaped.
    EditResult set_primary_key(domain::AttributeId id, bool key);
    // Whether a foreign key may be empty, said from the Relational Schema.
    //
    // A foreign key is NOT NULL because the side it points at is total, so
    // this is not a fact about the column at all: it is a fact about the
    // relationship, and changing it here changes the diagram. The participant
    // is found by its own identity, because that is all the schema has -- a
    // foreign key remembers which of the model's links put it there.
    //
    // Only the participation is confirmed. The cardinality was not asked
    // about and is left as it was, answered or not.
    EditResult set_participation(domain::ParticipantId participant,
                                 domain::Participation participation);
    // The same rules as an attribute's, for a column that lives on the schema
    // alone and so has no attribute behind it to carry them.
    EditResult set_schema_column_rules(domain::SchemaColumnId id, bool identifier,
                                       bool required, bool unique);
    // Whether the database counts the column up for itself.
    //
    // This one answers to nothing on the diagram: a Chen ERD has no way of
    // saying that a value is generated rather than recorded, so there is no
    // conceptual fact behind it to reach. It is a statement about the table,
    // and it exists because the schema is what the SQL is generated from.
    //
    // Only a whole-number type can count up, and a Decimal or Numeric only
    // where it keeps no digits after the point. Anything else is refused with
    // a reason, since a generated column of the wrong type is SQL that will
    // not run.
    EditResult set_auto_increment(domain::AttributeId id, bool counting);
    EditResult set_schema_column_auto_increment(domain::SchemaColumnId id, bool counting);
    // The same, for a key the conversion invented. It has no attribute behind
    // it and no identity of its own, so it is remembered against the element
    // whose table it belongs to, exactly as its name is. This is the commonest
    // place of all to want it: a surrogate key is what IDENTITY is for.
    EditResult set_key_auto_increment(domain::ElementRef table, bool counting);
    // A side's maximum, on its own. Setting a foreign key unique from the
    // schema is a statement that the side carrying it sees one row, which is
    // the relationship's cardinality and therefore the diagram's business.
    // Only the maximum is confirmed; the participation was not asked about.
    EditResult set_cardinality(domain::ParticipantId participant, domain::Cardinality maximum);
    // The same, and in the same edit which side of the relationship keeps its
    // foreign key once it is one to one, as set_one_to_one_key records it
    // (Zain, 2026-10-02). Asked for when making a foreign key unique on the
    // schema turns its relationship one to one: either side may keep the key,
    // so the side chosen is recorded with the change rather than left to the
    // conversion's default, and replaces whatever was recorded before. One
    // edit, so one undo takes back both.
    EditResult set_cardinality(domain::ParticipantId participant, domain::Cardinality maximum,
                               domain::ParticipantId keeps_key);
    // The answers to what a conversion cannot decide for itself. Each is an
    // edit like any other, so a decision undoes and travels with the document;
    // and each is keyed by a stable identity, so it survives renaming and is
    // never asked a second time. Passing no value takes the answer back, which
    // returns that question to its default rather than deleting anything.
    EditResult set_table_naming(domain::TableNaming naming);
    EditResult set_isa_strategy(domain::SpecializationId id, std::optional<domain::IsaStrategy> strategy);
    EditResult set_composite_mode(domain::AttributeId id, std::optional<domain::CompositeMode> mode);
    EditResult set_one_to_one_key(domain::RelationshipId id, std::optional<domain::ParticipantId> side);
    // What keys a many-to-many bridge: the pair of foreign keys, or a key of
    // its own. Passing none hands it back to the default, a key of its own.
    // A key drawn on the relationship outranks either answer (ADR-021 §5b).
    EditResult set_bridge_key(domain::RelationshipId id, std::optional<domain::BridgeKey> keyed);
    EditResult set_junction_name(domain::RelationshipId id, std::string chosen);
    EditResult set_entity_identifier(domain::EntityId id, std::optional<domain::AttributeId> chosen);
    // A table name typed over the one that was derived. Empty hands it back to
    // the rules, because a derived name is a suggestion and a typed one is not.
    EditResult set_table_name(domain::ElementRef ref, std::string chosen);

    // Editing the schema away from the diagram it came from. ADR-010 allows
    // the two levels to differ, and these are how a difference is recorded:
    // a column the schema has and the diagram does not, and an attribute the
    // diagram has that the schema does not show. Neither touches the diagram.
    //
    // Reflecting a change instead -- the usual answer -- needs none of these:
    // it is create_attribute or erase on the model itself, and the schema
    // follows because it is derived from it.
    EditResult add_schema_column(domain::ElementRef table, std::string name);
    // Renaming from the schema. A table's name is the name of the element it
    // came from, and a derived column's is its attribute's, so both are renamed
    // through the model rather than recorded as a difference beside it. Only a
    // key the conversion invented has nothing behind it to rename, so it is
    // given a name of its own, remembered against the table it belongs to.
    EditResult rename_table(domain::ElementRef ref, std::string name);
    EditResult rename_schema_key(domain::ElementRef table, std::string chosen);
    // A name typed over a foreign key the conversion made (Zain,
    // 2026-09-27). It is kept, and no longer follows the key it points at;
    // empty hands the name back to the rule.
    EditResult rename_foreign_key(domain::ForeignKeyColumn column, std::string chosen);
    // The order a table worked out from the diagram lists its columns in
    // (Task 4B, 2026-10-02): every column it is to show, each by its identity,
    // in the order it is to show them -- one step, one undo. Only the listing
    // changes: the key and every foreign key stay what they are. A column the
    // order already named that the table does not have at the moment keeps
    // its place beside the column it followed, so it returns there. Empty
    // gives the table back the order the conversion makes. A table drawn by
    // hand is refused: its columns are kept in the order they are listed.
    EditResult set_column_order(domain::RelationId table, std::vector<domain::ColumnIdentity> order);
    EditResult rename_schema_column(domain::SchemaColumnId id, std::string name);
    EditResult set_schema_column_type(domain::SchemaColumnId id, domain::LogicalType type);
    EditResult erase_schema_column(domain::SchemaColumnId id);
    EditResult hide_in_schema(domain::AttributeId id, bool hidden);

    // Arranging the schema by hand. Presentation, like moving a shape on the
    // diagram, and edits for the same reason: somebody did the work, so it
    // undoes and it saves. A drag makes one of these when it is let go, not
    // one per frame, or undo would walk back through every pixel of it.
    // Moving tables, and giving back any lines the move displaced. The two
    // travel together because they are one thing the user did: a line handed
    // back in an edit of its own would leave undo taking them apart.
    // Lines shaped by hand whose two tables both moved travel with them: they
    // arrive here already moved, and are written in the same edit, so one
    // undo takes the tables and the lines back together.
    EditResult move_schema_tables(
        const std::map<domain::ElementRef, domain::Point>& places,
        const std::vector<domain::LinkSource>& give_way = {},
        const std::vector<std::pair<domain::LinkSource, domain::SchemaLine>>& carried = {});
    // Pulling tables by their edges. A table answers to all four of them and
    // to its corners, so a pull carries a width, a height, and -- where the
    // edge that was pulled is one that moves the table's top-left corner --
    // the place it has moved to. All of it arrives as one edit, or undoing a
    // pull on the left edge would put the width back and leave the table
    // standing somewhere it was never put.
    EditResult resize_schema_tables(const std::map<domain::ElementRef, domain::SchemaTableBox>& boxes);
    EditResult shape_schema_line(domain::LinkSource link, domain::SchemaLine shape);
    EditResult release_schema_lines();
    EditResult tidy_schema();
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
    // An entity's box is pulled by its own edges and corners, each of which
    // moves one side and leaves the opposite one where it was. Its own named
    // edit rather than a move that happens to change a size, so the history
    // says which was done; and it refuses anything that is not an entity, so
    // the command cannot quietly reshape the rest of the diagram.
    EditResult resize_entities(const std::map<domain::ElementRef, domain::Rect>& boxes);
    // Relationship diamonds use the same edge and corner gesture as entities.
    EditResult resize_relationships(const std::map<domain::ElementRef, domain::Rect>& boxes);
    // And so do attributes, whose names want width as an entity's does.
    EditResult resize_attributes(const std::map<domain::ElementRef, domain::Rect>& boxes);
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
