// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "domain/model.hpp"

#include <variant>
#include <vector>

namespace erdflow::desktop {

// What is chosen on the schema (Zain, 2026-09-27, Stage 1 of the Schema
// workspace): one thing, told in the schema's own terms, which the canvas,
// the Properties panel and later the Explorer all read from the same place.
//
// Everything here is named by the schema's own identities, never by a name, a
// row number or a place on the screen, so it survives a rename and a fresh
// preview. And never by the diagram's: a table is its RelationId, not the
// entity it came from; a line is its ForeignKeyId, not the relationship.
//
// It is how the schema is being looked at, not part of it: nothing here is
// saved with the project or passes through the history.

// A column has no identity of its own unless it was made on the schema, so
// the handle for one is whatever the schema keeps it by -- its table, and:
//  - a column the schema holds itself: its own SchemaColumnId;
//  - one column of a foreign key: the key and which of its parts;
//  - the key the conversion generated for a table, or its discriminator:
//    the table alone says which, since a table has at most one of each;
//  - a column worked out from something on the diagram: what it was worked
//    out from, as its provenance -- where it came from, not what it is.
struct GeneratedKey {
    auto operator<=>(const GeneratedKey&) const = default;
};
struct Discriminator {
    auto operator<=>(const Discriminator&) const = default;
};
struct WorkedOutFrom {
    domain::AttributeId attribute;
    auto operator<=>(const WorkedOutFrom&) const = default;
};
using ColumnSource = std::variant<domain::SchemaColumnId, domain::ForeignKeyColumn, GeneratedKey,
                                  Discriminator, WorkedOutFrom>;
struct SchemaColumnRef {
    domain::RelationId table;
    ColumnSource source;
    auto operator<=>(const SchemaColumnRef&) const = default;
};

// Nothing is chosen.
struct NothingChosen {
    auto operator<=>(const NothingChosen&) const = default;
};
// One table.
struct ChosenTable {
    domain::RelationId table;
    auto operator<=>(const ChosenTable&) const = default;
};
// Several tables gathered together, which the schema has allowed since it
// was asked to be handled like the diagram. Kept as what it is rather than
// pretending one of them is the one chosen.
struct ChosenTables {
    std::vector<domain::RelationId> tables;
    auto operator<=>(const ChosenTables&) const = default;
};
// One column. A column may be a primary key and a foreign key at once; which
// it is, is read from the schema, not from the kind of handle it has.
struct ChosenColumn {
    SchemaColumnRef column;
    auto operator<=>(const ChosenColumn&) const = default;
};
// One line between tables, which is a foreign key: a reference from the rows
// of one table to the key of another, not merely two tables joined.
struct ChosenForeignKey {
    domain::ForeignKeyId key;
    auto operator<=>(const ChosenForeignKey&) const = default;
};

using SchemaSelection = std::variant<NothingChosen, ChosenTable, ChosenTables, ChosenColumn, ChosenForeignKey>;

} // namespace erdflow::desktop
