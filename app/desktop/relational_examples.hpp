// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

namespace erdflow::application {
class Editor;
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
void build_company_database_relational(application::Editor& editor);
void build_university_database_relational(application::Editor& editor);

// The template: not an example, but the least a relational schema is -- a
// table with its primary key, and another whose foreign key references it --
// named for what each is, ready to be renamed and extended. Opened untitled,
// as the Conceptual template is.
void build_basic_relational_schema(application::Editor& editor);

} // namespace erdflow::desktop
