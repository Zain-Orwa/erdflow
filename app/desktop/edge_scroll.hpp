// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include <QElapsedTimer>
#include <QPoint>
#include <QSize>
#include <QTimer>

#include <functional>

class QAbstractScrollArea;

namespace erdflow::desktop {

// Scrolls a view on while a held gesture -- a band being drawn round things,
// or things being carried -- has its pointer near or past an edge of what is
// in view, so the gesture goes on into what was not showing without being let
// go (C6, 2026-10-12). It goes on while the pointer is held still there, and
// stops when the pointer comes back in, when the view can go no further that
// way, or when the view says the gesture is over.
//
// It only scrolls. What the gesture does with the part of the canvas that has
// come into view is the view's own business, and it is told after each step
// that moved anything.
class EdgeScroll {
public:
    EdgeScroll(QAbstractScrollArea& area, std::function<void()> scrolled);
    EdgeScroll(const EdgeScroll&) = delete;
    EdgeScroll& operator=(const EdgeScroll&) = delete;

    // Where the pointer is now, in the area's viewport: near an edge, or past
    // it, this starts the scrolling, and anywhere else it stops it.
    void follow(QPoint in_viewport);
    void stop();
    [[nodiscard]] bool running() const { return timer_.isActive(); }
    // Where the pointer was last said to be, in the viewport. The pointer
    // stays put while the canvas moves under it.
    [[nodiscard]] QPoint pointer() const { return pointer_; }
    [[nodiscard]] const QAbstractScrollArea& area() const { return area_; }

    // How far one step goes, across and down, for a pointer at `at` in a
    // viewport of `size`: nothing away from the edges; one pixel at the inside
    // of the strip along an edge, more nearer the edge, and more again the
    // further past it the pointer has gone, up to a limit, so the hand sets
    // the pace without the canvas ever leaping.
    [[nodiscard]] static QPoint step(QSize size, QPoint at);

    // The strip along each edge, in pixels, where scrolling begins.
    static constexpr int reach = 24;
    // The fastest one step goes, in pixels.
    static constexpr int fastest = 40;
    // Time between steps: about one a frame. A step that comes late -- the
    // canvas having spent longer than that on the last one, as a large schema
    // routing its lines does -- goes as far as the steps it stands for, up to
    // the fastest, so a slow canvas still scrolls at the pace the hand asks
    // for without a step ever leaping further than a fast one could.
    static constexpr int interval_ms = 16;
    [[nodiscard]] static QPoint late_step(QPoint step, qint64 waited_ms);

private:
    void advance();

    QAbstractScrollArea& area_;
    std::function<void()> scrolled_;
    QTimer timer_;
    QElapsedTimer since_step_;
    QPoint pointer_;
};

} // namespace erdflow::desktop
