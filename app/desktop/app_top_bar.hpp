// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/theme.hpp"

#include <QWidget>

class QMenu;
class QPushButton;
class QToolButton;

namespace erdflow::desktop {

// The Home screen's slim bar: the ERDFlow mark on the left, and Theme on the
// right, with the way back into the workspace beside it once there is one.
// (Settings, first here too, is the sidebar's row alone since 2026-09-26.)
//
// Zain settled its shape on 2026-09-23 (ADR-022 section 9.14). The window keeps
// its native frame (section 9.7) and its native menu bar -- File, Home, Edit,
// Insert, Design, View, Help -- which is never hidden in favour of a copy. So
// this bar carries no menus: a second row of the same menus would show them
// twice, and on macOS would sit under the system's own. While Home is showing
// it stands where the ribbon does in the workspace.
class AppTopBar final : public QWidget {
public:
    explicit AppTopBar(QWidget* parent = nullptr);

    void wear(ThemeId id);
    [[nodiscard]] ThemeId worn_theme() const { return theme_; }

    // The menu Theme opens. Attached rather than built here, so it is the
    // window's own menu and can never disagree with it. Settings has no
    // control here: it is the sidebar's row (Zain, 2026-09-26).
    void attach_theme_menu(QMenu* menu);
    // The way back into the workspace Home was come to from (Zain,
    // 2026-09-27), the other half of the workspace's Back to Home: beside
    // Theme, naming the workspace it returns to. Empty hides it, for a Home
    // with no workspace yet to return to.
    void set_return_to(const QString& workspace);
    [[nodiscard]] QPushButton* return_button() const { return return_; }

    [[nodiscard]] QToolButton* brand_button() const { return brand_; }
    [[nodiscard]] QToolButton* theme_button() const { return theme_button_; }

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    ThemeId theme_ = ThemeId::Azure;
    QToolButton* brand_ = nullptr;
    QToolButton* theme_button_ = nullptr;
    QPushButton* return_ = nullptr;
};

} // namespace erdflow::desktop
