// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/theme.hpp"

#include <QElapsedTimer>
#include <QObject>
#include <QRectF>
#include <QTimer>
#include <QWidget>

#include <utility>
#include <vector>

namespace erdflow::desktop {

// Which kind of project a live demo shows being made. One for each start card,
// in the cards' order.
enum class HomeDemoKind { Conceptual, Relational, Sql };

// A miniature of one kind of project being made on Home. Conceptual now
// lives inside its card; the other kinds await steps B and C.
//
// It is decoration, not an editor: painted, never built from the real
// workspaces, and nothing in it can be pressed. It floats on the page rather
// than sitting in a box, over a soft glow of floor.
//
// The shell is kept apart from what is shown. It owns the size, the theme,
// the floor and which moment of its scene is showing; what each kind shows is
// its own scene (home_demo_scenes), drawn into the stage the shell gives it,
// and what moves it along is the one clock all three share
// (HomeDemoClock). Stood still, a demo shows its scene finished.
class HomeLiveDemo final : public QWidget {
public:
    HomeLiveDemo(HomeDemoKind kind, QWidget* parent = nullptr);

    void wear(ThemeId id);
    [[nodiscard]] HomeDemoKind kind() const { return kind_; }

    // The box the miniature is drawn in, in this widget's own coordinates: as
    // large as the widget holds keeping the authored shape, centred across it
    // and hung from its top, so it stays under its card however it is scaled.
    [[nodiscard]] QRectF stage() const;
    // Whether there is room to draw the miniature large enough to be read. On
    // a window too short for that, nothing is drawn rather than a smudge.
    [[nodiscard]] bool shown() const;

    // Which moment of its scene is showing: a step, and how far through it.
    // Set directly for tests and for pictures that must come out the same
    // every time; the clock that plays a scene sets it too.
    void show_step(std::size_t step, double progress = 1.0);
    // The moment a given time into a pass falls on.
    void show_time(double seconds);
    [[nodiscard]] std::size_t step() const { return step_; }
    // Whether it stands on a floor of its own: under the cards it does; held
    // inside a card, the card is its ground, and it is shown on a small screen
    // raised off the card instead (Zain, 2026-09-25).
    void set_floor(bool on);
    // For the Conceptual demo: show the Conceptual canvas itself, drawn small,
    // rather than its painted scene. What a still card shows; a demo that
    // moves shows its painted scene, which is what can be moved.
    void set_real_canvas(bool on);
    [[nodiscard]] bool real_canvas() const { return real_canvas_; }
    // The screen it is shown on inside a card, and the part of it the scene
    // or the canvas is drawn in, in this widget's own coordinates.
    [[nodiscard]] QRectF screen() const;
    [[nodiscard]] QRectF screen_inside() const;
    [[nodiscard]] double progress() const { return progress_; }

    // The size every scene is authored at. It is only ever scaled evenly.
    static constexpr double authored_width = 280;
    static constexpr double authored_height = 200;
    // Below this share of its authored size a miniature is not drawn.
    static constexpr double smallest_scale = 0.45;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    void paint_floor(QPainter& painter, const QRectF& stage) const;
    void paint_screen(QPainter& painter) const;

    HomeDemoKind kind_;
    ThemeId theme_ = ThemeId::Azure;
    std::size_t step_ = 0;
    double progress_ = 1.0;
    bool floor_ = true;
    bool real_canvas_ = false;
};

// The one clock the Home screen's live demos are played by. A single timer
// moves every demo along its own pass, each starting a little after the one
// before it so the three are never all busy at once, and each going round
// again at its own length after that.
//
// Time is always given, never read inside a demo: the timer gives it every
// frame, and a test or a picture gives any moment it wants without waiting
// for it. Stopped, it shows every demo finished; started, every demo begins
// again from its start, in step with the others.
//
// Whether the demos are to play and whether the clock is ticking are two
// things. It ticks only while what it plays can be seen: hidden, it stops and
// costs nothing, and shown again it starts every demo from the beginning, the
// same way for all three.
class HomeDemoClock final : public QObject {
public:
    explicit HomeDemoClock(QObject* parent = nullptr);

    // A demo to play, starting this many seconds after the clock does.
    // `plays` says whether it moves at all: one that does not stands as a
    // still picture of its scene finished, whatever the others do.
    void add(HomeLiveDemo* demo, double delay, bool plays = true);
    // Switches one demo's motion on or off, leaving the others as they are.
    void set_playing(HomeLiveDemo* demo, bool on);
    [[nodiscard]] bool playing(const HomeLiveDemo* demo) const;
    // Whether the demos are to play at all. Off, they stand finished.
    void set_moving(bool on);
    [[nodiscard]] bool moving() const { return wanted_; }
    // Whether the clock is actually ticking: only while the demos are to play,
    // at least one of them moves, and they can be seen.
    [[nodiscard]] bool running() const { return timer_.isActive(); }
    // The widget whose being shown or hidden starts or stops the ticking.
    void follow(QWidget* shown_while);
    // Where every demo stands this many seconds after the clock started: a
    // demo whose turn has not yet come stands at its empty start.
    void show_at(double seconds);
    // How long the clock has been running.
    [[nodiscard]] double seconds() const;
    [[nodiscard]] double delay_of(const HomeLiveDemo* demo) const;

    // Thirty frames a second: smooth enough for motion this calm, and no
    // more work than that.
    static constexpr int frame_ms = 33;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void start_from_the_beginning();
    [[nodiscard]] bool seen() const;
    [[nodiscard]] bool any_playing() const;

    struct Played {
        HomeLiveDemo* demo;
        double delay;
        bool plays;
    };
    QTimer timer_;
    QElapsedTimer since_;
    std::vector<Played> demos_;
    QWidget* seen_ = nullptr;
    bool wanted_ = false;
};

} // namespace erdflow::desktop
