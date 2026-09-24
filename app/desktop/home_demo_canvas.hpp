// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/theme.hpp"

#include <QPicture>
#include <QRectF>

namespace erdflow::application {
class Editor;
}

namespace erdflow::desktop {

// The Conceptual canvas itself, drawn small for the Home screen's first card
// (Zain, 2026-09-25; ADR-022 section 9.21).
//
// Not a painting of the canvas but the canvas: a small example built with
// the Editor's own commands and drawn by the Conceptual workspace's own view,
// at the sizes every element is really made at, in the theme worn. What the
// card shows is therefore exactly what somebody will meet when they draw one.
// It is built and drawn once for each theme and kept.

// Student and Course joined many to many by Enrolled, each with its ID, a key,
// and its Name, and Enrollment Date on the relationship.
void build_conceptual_example(application::Editor& editor);

// The example, as the canvas draws it: a recording that replays crisply at
// any size, and the part of the canvas it covers.
struct CanvasPicture {
    QPicture picture;
    QRectF source;
};
[[nodiscard]] const CanvasPicture& conceptual_canvas_picture(ThemeId theme);

} // namespace erdflow::desktop
