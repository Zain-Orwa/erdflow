// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include <QIcon>
#include <QObject>
#include <QString>
#include <map>
#include <set>
#include <utility>
#include <vector>

class QAction;
class QMainWindow;
class QMenu;
class QToolBar;
class QToolButton;

namespace erdflow::desktop {

// The row of tabs above the tool row, laid out the way an office application
// lays out its commands. Three tabs stand there for good (Zain, 2026-10-06):
// File, for the project and what goes in and out of it; Home, the window's own
// tool row, left exactly as it was; and Settings, for how the window looks and
// what it shows, and help. File and Settings each gather several rows, and
// while one of them is chosen its rows' own tabs stand beside the three --
// Export and Import, or Design, View and Help -- each bringing up its row, as
// every tab used to. The rows are built from actions the window already has,
// so nothing on them can disagree with the menus about what is active or
// checked.
class Ribbon final : public QObject {
public:
    // Built over the window's "modelTools" row, which becomes Home.
    explicit Ribbon(QMainWindow& window);
    // Brings a tab to the front by name. tabFile, tabHome and tabSettings
    // bring up the row they last showed; tabExport, tabImport, tabDesign,
    // tabView and tabHelp bring up their own, with File or Settings chosen.
    void show_tab(const QString& name);

    // Whether the Relational Schema is the workspace in front. Its tools are
    // in its own header, so Home brings up no row while it is, and the rows
    // leave out what acts on the conceptual diagram alone; the tabs stay, so
    // the window is found in the same place in both workspaces.
    void set_schema_in_front(bool schema);
    // A command that acts on the conceptual diagram alone, left off its row
    // while the schema is in front. Menus are not touched.
    void keep_to_conceptual(QAction* action);
    // A command on the named tab's row only while the schema is in front,
    // after `after` there.
    void add_for_schema(const QString& tab_name, QAction* action, QAction* after);
    // Put away with the workspace while the Home screen is showing, and back
    // as it was when the workspace returns.
    void set_put_away(bool away);

protected:
    // Watches Home, so every other row follows its height as the window is
    // resized and the icons with it.
    bool eventFilter(QObject* watched, QEvent* event) override;

public:
    [[nodiscard]] QToolBar* tabs() const { return tabs_; }
    // File's own menu, which stands beside Export and Import while File is
    // chosen, since the File tab now brings up their rows rather than it.
    [[nodiscard]] QToolButton* file_menu_button() const { return file_menu_button_; }
    // Gives the button a row shows for a command an icon of the ribbon's own.
    // The command's action is the menus' as well and keeps none: a menu
    // leaves room for an icon beside every one of its entries as soon as one
    // of their actions has an icon, shown or not, and the menus are to read
    // exactly as they did.
    void set_row_icon(QAction* action, const QIcon& icon);

private:
    QMainWindow& window_;
    QToolBar* tabs_ = nullptr;
    QToolBar* home_ = nullptr;
    QToolButton* file_menu_button_ = nullptr;
    // Each row's own tab with the row it brings up. Home's is the Home tab.
    std::vector<std::pair<QAction*, QToolBar*>> rows_;
    // A tab that stands for good, the row tabs it gathers (none for Home),
    // the one it last showed, and what stands with its row tabs.
    struct Group {
        QAction* tab = nullptr;
        std::vector<QAction*> sections;
        QAction* chosen = nullptr;
        QAction* beside = nullptr;
    };
    std::vector<Group> groups_;
    // The line between the three tabs and the chosen one's row tabs.
    QAction* rule_ = nullptr;
    // The row tab -- or Home's tab -- whose row is in front.
    QAction* current_ = nullptr;
    bool schema_ = false;
    bool away_ = false;
    std::map<QAction*, QIcon> row_icons_;
    std::set<QAction*> conceptual_only_;
    std::set<QAction*> schema_only_;
    // Every row's commands in the order they stand, those left out included,
    // so one put back goes back where it was.
    std::map<QToolBar*, std::vector<QAction*>> orders_;
    void wear_row_icon(QAction* action) const;

    QAction* add_tab(const QString& label, const char* name);
    QToolBar* add_row(const QString& label, const char* tab_name, const char* row_name);
    QToolButton* add_menu_button(QToolBar* row, const QString& label, const char* name, QMenu* menu,
                                 QAction* default_action = nullptr);
    void show_row(QAction* tab);
    // Shows the tabs and the row in front, and nothing else; the one place
    // that decides what of the ribbon is out.
    void settle();
    std::vector<QAction*>& order_of(QToolBar* row);
    // Takes off a row what is not for the workspace in front and puts back
    // what is, each where it stood.
    void rescope(QToolBar* row);
    void dress_button(QToolBar* row, QAction* action);
    void match_home_height();
    // Home's height as last measured while it was showing. A row that is not
    // showing keeps whatever height it last had, so Home has to be measured
    // when it can be, and remembered for when it cannot.
    int home_height_ = 0;
};

} // namespace erdflow::desktop
