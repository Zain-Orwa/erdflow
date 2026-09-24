// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/theme.hpp"

#include <QPoint>
#include <QString>
#include <QTimer>
#include <QVariantAnimation>
#include <QWidget>

#include <optional>

namespace erdflow::desktop {

// A remark that comes up over the work and goes away again on its own.
//
// The status bar keeps the record; this is for the moment something has gone
// wrong under the hand while the hand is still there, when a line along the
// bottom of the window is too far from where the person is looking to be read
// at all.
//
// Drawn in the theme's own amber -- the colour every palette already has for a
// warning -- laid over the window at a weight low enough to read as a note
// left on the work rather than as a window that has taken it over. What went
// wrong is said in bold and why is said under it, because the two are read at
// different speeds: the first tells you whether to stop, the second tells you
// what to do.
//
// It never asks to be dismissed, and it is never taken away on a clock. A
// warning that has to be clicked away stops the work it is about; one that
// goes after so many seconds is gone before a slow reader has finished, and
// still there long after a quick one has. So it waits for the hand instead:
// something has gone wrong under the pointer, the pointer stops while it is
// read, and moving on again is what says it has been. Until then it stays,
// however long that is.
class Notice final : public QWidget {
public:
    explicit Notice(QWidget* parent);

    // The palette it draws in. Taken by value: a notice outlives the call that
    // showed it, and a theme changed behind it must not leave it reading from
    // something that has gone.
    void wear(const Theme& theme);

    // Say it beside a point, and start the wait before it fades.
    //
    // The point is where the hand was when it caused this, in this widget's
    // own coordinates, and is where the notice goes: a warning about a
    // connection belongs where the connection was attempted, because that is
    // where the person is looking. It sits below and to the right of the
    // point, turning to the other side of it where there is no room, and
    // never leaves the window.
    //
    // Passing no point puts it near the bottom, which is right for a remark
    // about nothing in particular.
    //
    // Saying something while one is already up replaces what it says and
    // starts the wait again, so a run of warnings never queues up behind the
    // first -- the last one is the one the hand just caused, and it is the
    // one worth reading.
    void say(const QString& words, std::optional<QPoint> beside = {});

    // Take it away now, without waiting. Used when the work it was about is
    // undone, so a remark never outlives the thing it remarks on.
    void put_away();

    // Put it back where it belongs after the window under it has changed
    // shape. Does nothing when it is not up.
    void settle_again();

    // What it is saying, or nothing where it is not up. For tests, which
    // cannot read pixels.
    [[nodiscard]] QString saying() const;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    // Size to the words and sit where it belongs: beside the point it was
    // given, or near the bottom of whatever it is laid over where it was
    // given none.
    void settle();
    void fade_to(double opacity, int over_ms);
    // How far below its place it still is while it is arriving or leaving.
    [[nodiscard]] int risen() const;

    // Whether the hand has moved on since this was shown, which is what takes
    // it away. Watched rather than listened for, because the pointer may leave
    // the window entirely and there would be nothing to listen to; and watched
    // only while something is being said, so it costs nothing the rest of the
    // time.
    void watch_the_hand();

    Theme theme_{};
    // Where it was asked to stand, kept so that it can be put there again when
    // the window it is laid over changes size.
    std::optional<QPoint> beside_;
    // Where the pointer was when this was shown, on the screen. What it is
    // compared against to tell whether the hand has moved on.
    QPoint hand_was_;
    QString headline_;
    QString detail_;
    double opacity_ = 0.0;
    QTimer watch_;
    QVariantAnimation fade_;
};

} // namespace erdflow::desktop
