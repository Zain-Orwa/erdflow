// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/home_live_demo.hpp"
#include "app/desktop/theme.hpp"

#include <QPointF>
#include <QPainterPath>
#include <QRectF>
#include <QString>

#include <cstddef>
#include <vector>

class QPainter;

namespace erdflow::desktop {

// What the Home screen's live demos show (ADR-022 section 9.21): each kind's
// scene, as the steps it goes through and the pieces that arrive in them.
//
// A scene is data rather than drawing. Every piece says what it is, where it
// stands and in which step it arrives, and one painter draws any scene at any
// moment from that. So the three demos share one way of being drawn and
// timed, and what a demo shows at a moment can be asked without looking at
// pixels.
//
// Everything is placed in the demo's authored box, HomeLiveDemo's 280 x 200,
// and only ever scaled evenly into the room a demo is given.

// One step of a scene, and how long it lasts.
struct DemoStep {
    const char* name;
    double seconds;
};

// Where in its steps a scene stands: which step, and how far through it.
struct DemoMoment {
    std::size_t step = 0;
    double progress = 1.0;
};

// The shapes a scene is made of. The conceptual ones are drawn as the
// Conceptual workspace draws them; the relational ones as the Relational
// Design view draws its tables, reduced to what reads at this size.
enum class DemoShape {
    // Conceptual.
    Entity, Attribute, Relationship, Connector, KeyMark, OptionalMany,
    // Relational: a table and its header, one of its columns, the mark in a
    // column's key gutter, and a foreign key's line from the key it
    // references to the column that references it.
    Table, Column, PrimaryKey, ForeignKey, Reference,
    // SQL: the editor the script is written in, a run of the script typed
    // in one step, and the line saying what running it made.
    Editor, Code, Result,
};

struct DemoElement {
    DemoShape shape;
    QString label;
    // Where it stands. A connector runs from `from` to `to` instead, and a
    // key mark underlines the label of the attribute whose box this is. A
    // column's box is its row; a key mark's, the row it marks.
    QRectF box;
    QPointF from, to;
    // Reference: orthogonal row-to-row route. Conceptual Connector: two
    // points for a short common trunk and the branch's cubic control.
    // OptionalMany uses from as the entity anchor and to as its outward unit vector.
    std::vector<QPointF> route;
    // A table drawn in the relationship's colours: a bridge.
    bool bridge = false;
    // Arriving at one steady rate rather than easing in and out, as typing
    // does. A run of code is typed at this rate, a character at a time.
    bool steady = false;
    // The step it arrives in, and when within that step it starts and has
    // finished arriving, as shares of the step.
    std::size_t step = 0;
    double starts = 0.0;
    double ends = 1.0;
};

// The authored connector path, shared by painting and geometry checks.
[[nodiscard]] QPainterPath demo_connector_path(const DemoElement& element);

[[nodiscard]] const std::vector<DemoStep>& demo_steps(HomeDemoKind kind);
[[nodiscard]] const std::vector<DemoElement>& demo_elements(HomeDemoKind kind);
// How long one pass through a scene takes, in seconds.
[[nodiscard]] double demo_loop_seconds(HomeDemoKind kind);
// The step at which a scene is complete and held, which is what is shown
// where it is not moving.
[[nodiscard]] std::size_t demo_finished_step(HomeDemoKind kind);
// The moment a given time into a pass falls on, going round again past its end.
[[nodiscard]] DemoMoment demo_moment_at(HomeDemoKind kind, double seconds);

// How far a piece has arrived at a moment: nothing before its step, rising
// through it, all of it after.
[[nodiscard]] double demo_arrival(const DemoElement& element, DemoMoment moment);
// Whether nothing changes on screen through a step -- the empty start and
// the hold -- so the clock need not repaint while a demo stands in one.
[[nodiscard]] bool demo_step_is_still(HomeDemoKind kind, std::size_t step);

// How much of the whole scene is showing: all of it, except while it fades
// out at the end of a pass.
[[nodiscard]] double demo_scene_opacity(HomeDemoKind kind, DemoMoment moment);

// The script typed so far at a moment: every run of code in order, as much
// of each as has been typed. Empty for a scene that types nothing.
[[nodiscard]] QString demo_code_at(HomeDemoKind kind, DemoMoment moment);

// The part of the authored box a scene's pieces take up, a little margin
// round them: what is fitted into a screen, so the scene fills it. For SQL it
// is the editor's inside, since the screen is then the editor.
[[nodiscard]] QRectF demo_scene_bounds(HomeDemoKind kind);

// Draws a scene at a moment, in its authored box, in the theme's colours.
// Framed, it is being shown on a screen that is its frame, so a scene that
// draws a frame of its own -- the SQL editor -- leaves it out.
void paint_demo_scene(QPainter& painter, HomeDemoKind kind, ThemeId theme_id, DemoMoment moment,
                      bool framed = false);

} // namespace erdflow::desktop
