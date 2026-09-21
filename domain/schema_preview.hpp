#pragma once

#include "domain/model.hpp"

#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace erdflow::domain {

// What a Conceptual ERD becomes when it is read as tables.
//
// This is a picture of the schema, not the schema itself. It is worked out
// fresh from the project every time it is asked for, it holds no identity of
// its own, and nothing here is stored: the Relational Schema workspace that
// owns real relational objects, with their own identities and their own
// history, is a later and separate thing. Editing a column in a preview edits
// the attribute it came from, through the Editor like any other change.
//
// The rules are the ones the course material teaches, and where a textbook
// alternative exists this is the one ERDFlow follows:
//
//   entity                  -> table, carrying its attributes
//   composite attribute     -> its roots become the columns; see CompositeMode
//   derived attribute       -> ignored, becoming no column at all
//   multivalued attribute   -> its own table, with its own key and a foreign
//                              key back to the entity it belongs to
//   one to many             -> the one side's key becomes a foreign key on the
//                              many side
//   one to one              -> the key goes into whichever side was chosen;
//                              either is correct, which is why it is a decision
//   many to many            -> a bridge table holding both foreign keys, and
//                              given a primary key of its own
//   associative entity      -> as many to many, and the associated relation's
//                              key goes into the bridge as well
//   self reference          -> a foreign key in the same table, pointing at
//                              that table's own primary key
//   weak entity             -> the owner's key together with the partial key
//   generalization          -> always one to one; the parent's key becomes a
//                              foreign key in each child, subject to the
//                              chosen mapping strategy

// Where a table came from, which decides the colour it is drawn in and what it
// is called in the panel that lists it.
enum class TableOrigin { Entity, Subtype, Bridge, Multivalued, Associative };

// Why a column exists. A column the conversion invented -- a bridge's own key,
// or a surrogate for an entity with no identifier -- has no attribute behind it
// and so cannot be edited or coloured from one. A column that was added on the
// schema and nowhere else has no attribute behind it either, but it does have
// an identity of its own, and it can be edited by it.
enum class ColumnOrigin { Attribute, ForeignKey, Generated, Discriminator, SchemaOnly };


struct PreviewColumn {
    std::string name;
    LogicalType type = LogicalType::Unset;
    std::uint32_t length = 0;
    std::uint32_t scale = 0;
    bool primary_key = false;
    bool foreign_key = false;
    bool required = false;
    bool unique = false;
    // Whether the database counts this one up for itself. Unlike the rules
    // above it answers to nothing on the diagram, so a column the conversion
    // invented never carries it until somebody says so.
    bool auto_increment = false;
    ColumnOrigin origin_kind = ColumnOrigin::Attribute;
    // Listed to show what the conversion left out, rather than being a column
    // at all. A derived attribute becomes nothing, and a reader who is learning
    // the rules is better served by seeing that it became nothing than by
    // finding it quietly absent. Nothing counts an ignored column: it holds no
    // type question, takes no key and is referenced by nothing.
    bool ignored = false;
    // The attribute this column came from, where one did. This is what makes
    // the preview a lens: a type set here is set on that attribute.
    std::optional<AttributeId> origin;
    // The schema-only column this came from, where it was added here rather
    // than derived from the diagram. Never set at the same time as origin.
    std::optional<SchemaColumnId> added;
    // The table a foreign key references, by its position in the preview, and
    // the column within it. A self reference points at its own table.
    std::optional<std::size_t> references;
    std::size_t references_column = 0;
    // Which of the model's own links put this foreign key here, so the line
    // drawn for it keeps its identity across a fresh preview.
    std::optional<LinkSource> link;
    // Whether the row it points at need not exist, and whether the relationship
    // behind it is one to one. Together these decide how the line is drawn.
    bool optional_link = false;
    bool one_to_one = false;
    auto operator<=>(const PreviewColumn&) const = default;
};

// A question the conversion cannot settle for itself, carried by the table it
// concerns so that it can be asked where its answer will be seen.
//
// Every one has a working default and none of them blocks anything. They are
// asked because a default is not a decision, and they are attached to a table
// rather than gathered into a dialog because the model is edited where the
// model is (ADR-009 §63).
enum class DecisionKind { IsaStrategy, CompositeMode, OneToOneKey };

struct OpenDecision {
    DecisionKind kind = DecisionKind::IsaStrategy;
    // What the question is about: the specialization, the composite attribute,
    // or the relationship whose key could sit on either side.
    ElementRef about;
    // Which answer is in force, as a position in the answers the kind offers.
    std::size_t chosen = 0;
    // Whether that answer was given, or is only what the conversion does when
    // nobody has said. The two look alike and are not alike.
    bool answered = false;
    // For a one-to-one key, the two sides in the order they are offered, and
    // the element each one is attached to, so they can be named.
    std::vector<ParticipantId> sides;
    std::vector<ElementRef> side_targets;
    auto operator<=>(const OpenDecision&) const = default;
};

struct PreviewTable {
    std::string name;
    TableOrigin origin_kind = TableOrigin::Entity;
    // The element the table came from: an entity, the relationship that became
    // a bridge, or the multivalued attribute that became a table of its own.
    std::optional<ElementRef> origin;
    // The element the table's kind is about, where that is a different element
    // from the one it came from: a subtype's parent. Absent otherwise. Kept as
    // a reference rather than as words, because what to call it belongs to
    // whatever is showing it (ADR-009 §64).
    std::optional<ElementRef> derives_from;
    std::vector<PreviewColumn> columns;
    // The questions this table is where to answer.
    std::vector<OpenDecision> decisions;
    auto operator<=>(const PreviewTable&) const = default;
};

struct SchemaPreview {
    std::vector<PreviewTable> tables;
    auto operator<=>(const SchemaPreview&) const = default;
};

// Read the project as tables. Never fails and never refuses: a model with
// questions still open is drawn with those questions visible, because a schema
// you cannot see is no help in deciding what it should be.
//
// Where the schema has been edited away from the diagram -- a column added
// here only, an attribute hidden from here only -- those differences are
// applied on top of what the diagram says. The preview still stores nothing
// itself: the differences live in the project, beside the conversion decisions
// and for the same reason.
[[nodiscard]] SchemaPreview schema_preview(const Project& project);

// Columns in different tables that carry the same name and are still waiting
// for a type. They are almost always the same thing said several times over --
// every ID, every Name, every Code -- so they are gathered up here to be
// answered once for all of them rather than one table at a time.
struct SharedName {
    std::string name;
    // The attributes behind them, which is what answering actually sets. A
    // column with no attribute behind it cannot be answered and is counted but
    // not listed here.
    std::vector<AttributeId> attributes;
    std::size_t columns = 0;
    auto operator<=>(const SharedName&) const = default;
};

// The groups worth answering at once: a name has to be carried by more than
// one column, and at least one of them has to be answerable.
[[nodiscard]] std::vector<SharedName> shared_names(const SchemaPreview& preview);

// The plural a table is named by, under the project's naming convention.
// English is irregular enough that this needs rules and a word list, and
// neither is ever complete, which is why a typed name always wins over it.
[[nodiscard]] std::string plural_of(const std::string& word);

} // namespace erdflow::domain
