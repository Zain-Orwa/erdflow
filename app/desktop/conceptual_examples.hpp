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

// Two large Conceptual diagrams used for testing and demonstrating the
// workspace. They are built with the Editor's own commands — the same
// entities, attributes, relationships, cardinalities and participation the
// canvas already offers — and they do not change how any of those work.
//
// The original bundled example (Student, Course, Professor) is separate and
// is left as it is.

void build_company_database(application::Editor& editor);
void build_university_database(application::Editor& editor);

} // namespace erdflow::desktop
