// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include <functional>

namespace erdflow::application {
class Editor;
}
namespace erdflow::domain {
struct PreviewTable;
}

namespace erdflow::desktop {

// Relational Design's own examples and template (Zain, 2026-10-05). Each is a
// project that starts from its schema, built with the Editor's schema commands
// -- a table placed where it is asked for, its columns typed and ruled, and
// every foreign key connected to the key it references -- exactly as a schema
// drawn by hand is. None of them is converted from a diagram, and none of them
// has one: Convert to Conceptual Design draws it, as for any schema drawn by
// hand.
//
// The two examples are the Company and University domains of the Conceptual
// examples, designed again as tables: junction tables for what is many to many,
// a table for what may be held more than once, and a weak entity keyed through
// its owner. The Conceptual examples are separate and are left as they are.
//
// Every table's height is a matter of its rows, but its width is measured from
// its lettering, which each platform draws at its own size. Given how wide the
// schema will draw a table, each column of tables is stood a set distance
// clear of the widest table in the column before it, so the columns never run
// into one another (2026-10-06). On macOS that is exactly where they are
// placed; with no width given they are left there.
using TableWidth = std::function<double(const domain::PreviewTable&)>;
void build_company_database_relational(application::Editor& editor, const TableWidth& width = {});
void build_university_database_relational(application::Editor& editor, const TableWidth& width = {});

// The template: not an example, but the least a relational schema is -- a
// table with its primary key, and another whose foreign key references it --
// named for what each is, ready to be renamed and extended. Opened untitled,
// as the Conceptual template is.
void build_basic_relational_schema(application::Editor& editor, const TableWidth& width = {});

} // namespace erdflow::desktop
