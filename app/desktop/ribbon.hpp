#pragma once

#include <QObject>
#include <QString>
#include <utility>
#include <vector>

class QAction;
class QMainWindow;
class QMenu;
class QToolBar;
class QToolButton;

namespace erdflow::desktop {

// The row of tabs above the tool row, laid out the way an office application
// lays out its commands: File drops its menu down from its tab, and every other
// tab swaps the row beneath it. Home is the window's own tool row, left exactly
// as it was. The other rows are built from actions the window already has, so
// nothing on them can disagree with the menus about what is active or checked.
class Ribbon final : public QObject {
public:
    // Built over the window's "modelTools" row, which becomes Home.
    explicit Ribbon(QMainWindow& window);
    // Brings the named tab's row to the front. The tabs are named tabHome,
    // tabInsert, tabDesign, tabView and tabHelp.
    void show_tab(const QString& name);
    [[nodiscard]] QToolBar* tabs() const { return tabs_; }

private:
    QMainWindow& window_;
    QToolBar* tabs_ = nullptr;
    QToolBar* home_ = nullptr;
    // Each row tab with the row it brings up, in the order the tabs are shown.
    std::vector<std::pair<QAction*, QToolBar*>> rows_;

    QAction* add_tab(const QString& label, const char* name);
    void add_menu_tab(const QString& label, const char* name, QMenu* menu);
    QToolBar* add_row(const QString& label, const char* tab_name, const char* row_name);
    QToolButton* add_menu_button(QToolBar* row, const QString& label, const char* name, QMenu* menu,
                                 QAction* default_action = nullptr);
    void show_row(QAction* tab);
    void match_home_height();
};

} // namespace erdflow::desktop
