// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "schema_selection.hpp"

#include <QIcon>
#include <QMetaType>
#include <QStandardItemModel>
#include <QString>

namespace erdflow::desktop {

class SchemaView;

// The Schema Explorer (Zain, 2026-09-29, Stage 2 of the Schema workspace):
// the schema as a tree, in the schema's own words -- its tables, each one's
// columns, primary key and foreign keys, and every foreign key again under
// Relationships. It is read from what the schema view is showing and holds
// nothing of its own: every row that stands for something carries it as the
// schema view's own selection, so choosing a row and choosing the thing on
// the canvas are the same act.

// The count at the end of a row. The same role the diagram's Explorer counts
// with, so the two trees are painted by the same delegate in the same way.
inline constexpr int explorer_count_role = Qt::UserRole + 1;
// Words written where a count would be: a column's PK and FK. Both, where a
// column is both.
inline constexpr int explorer_note_role = Qt::UserRole + 2;
// What a row stands for on the schema, as a SchemaSelection.
inline constexpr int schema_choice_role = Qt::UserRole + 3;

// A name for what a row stands for, made from the schema's own identities --
// never from a name, a row or a place -- so a row stays itself through a
// rename, and every row for one thing can be found by it. A column listed
// under Columns and under Primary Key has one; so has a foreign key listed
// under its table and under Relationships. Empty for nothing chosen.
[[nodiscard]] QString schema_key(const SchemaSelection& chosen);

// The marks the rows wear, made by whoever knows the theme and the icon set.
struct SchemaExplorerLook {
    QIcon table;   // a table, and the Tables group
    QIcon key;     // a primary key column, and the Primary Key group
    QIcon link;    // a foreign key column or a foreign key
    QIcon relationships; // the Schema Relationships group: three connected nodes
    QIcon both;    // a column that is both keys: the key and the link side by side
    QIcon none;    // an ordinary column: nothing, at the size the others are
};

// Fill the model with the schema the view is showing, in the view's own
// order, replacing whatever it held. Rows are known by schema_key in
// Qt::UserRole; groups by a key of their own there, so what was open can be
// opened again after the tree is made afresh.
void fill_schema_explorer(QStandardItemModel& model, const SchemaView& view, const SchemaExplorerLook& look);

} // namespace erdflow::desktop

Q_DECLARE_METATYPE(erdflow::desktop::SchemaSelection)
