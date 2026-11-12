// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/theme.hpp"

#include <QAbstractButton>
#include <QString>
#include <QTimer>

#include <functional>

namespace erdflow::desktop {

// Update and Restart: the small button at the top of the window that says a
// newer ERDFlow has been fetched and is ready (ADR-025; Zain, 2026-11-12).
//
// ERDFlow still fetches an update without a word, and still puts it in place
// when it is closed (ADR-024). This is there only while one is waiting, for
// whoever would rather have it now: pressed, ERDFlow closes -- asking about
// unsaved work as any closing does -- the update goes in, and ERDFlow opens
// again. The rest of the time it is not there at all.
//
// It stands on a row that is already at the top of the window -- Home's bar,
// or the ribbon's row of tabs -- at the right, in room the row was not using.
// It is laid over the row rather than added to what the row lays out, so
// nothing already standing there is moved, resized or reordered by it, and
// the row is no taller for it. Where the row has no room for its words it
// keeps its mark alone; where there is no room for that either it stays away
// rather than cover anything.
class UpdateButton final : public QAbstractButton {
public:
    // `from` and `to` say, in the row's own coordinates, where the room it
    // may stand in begins and ends. They are asked again whenever the row
    // changes shape, since what stands on a row comes and goes.
    UpdateButton(QWidget* row, std::function<int()> from, std::function<int()> to);

    void wear(ThemeId id);
    // The version that is ready, or nothing, which puts the button away.
    void offer(const QString& version);
    [[nodiscard]] QString offered() const { return version_; }
    // Whether its mark alone is showing, the row being too narrow for its
    // words. For tests, which cannot read pixels.
    [[nodiscard]] bool mark_only() const { return mark_only_; }

    [[nodiscard]] QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    // Sizes it for the room there is and stands it at the right of that room.
    void settle();

    ThemeId theme_ = ThemeId::Azure;
    QString version_;
    bool mark_only_ = false;
    std::function<int()> from_;
    std::function<int()> to_;
    // The row is looked at once more after it has finished changing shape:
    // several changes in a row are answered once.
    QTimer again_;
};

} // namespace erdflow::desktop
