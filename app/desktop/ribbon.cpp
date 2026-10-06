// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "ribbon.hpp"

#include <QAction>
#include <QEvent>
#include <QMainWindow>
#include <QMenu>
#include <QToolBar>
#include <QToolButton>
#include <QWidgetAction>

#include <algorithm>

namespace erdflow::desktop {

Ribbon::Ribbon(QMainWindow& window) : QObject(&window), window_(window) {
    home_ = window.findChild<QToolBar*>("modelTools");
    if (!home_) return;

    tabs_ = new QToolBar("Ribbon tabs", &window);
    tabs_->setObjectName("ribbonTabs");
    tabs_->setMovable(false);
    tabs_->setFloatable(false);
    tabs_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    // A tab that is given an icon wears it small beside its name: no taller
    // than the name itself, so the row of tabs keeps its height (add_tab).
    tabs_->setIconSize(QSize(15, 15));
    // The tabs are the only way to the rows, so the window's context menu is
    // not allowed to close them: a row with no tab to reach it is a dead end.
    tabs_->toggleViewAction()->setVisible(false);
    // Above the tool row, on a line of its own.
    window.insertToolBar(home_, tabs_);
    window.insertToolBarBreak(home_);

    // The three that stand for good (Zain, 2026-10-06).
    auto* file = add_tab("File", "tabFile");
    auto* home = add_tab("Home", "tabHome");
    auto* settings = add_tab("Settings", "tabSettings");
    // Home is the tool row the window already has. What Insert carried -- a
    // picture from a file, and symbols -- is on it too, behind one button of
    // the window's own, so the tab is no longer needed.
    rows_.emplace_back(home, home_);
    // Then a line, and the chosen tab's own row tabs beyond it.
    rule_ = tabs_->addSeparator();

    // File used to drop its menu from its tab: New, Open, Save and the rest
    // of what is done with the whole project. The tab now brings up Export
    // and Import, so the menu stands beside them, the first thing there.
    file_menu_button_ = new QToolButton(tabs_);
    file_menu_button_->setObjectName("fileMenuButton");
    file_menu_button_->setText("Open && Save");
    file_menu_button_->setToolTip("The File menu: a new project, Open, Save, Save as, the examples and templates.");
    file_menu_button_->setMenu(window.findChild<QMenu*>("fileMenu"));
    file_menu_button_->setPopupMode(QToolButton::InstantPopup);
    file_menu_button_->setToolButtonStyle(Qt::ToolButtonTextOnly);
    file_menu_button_->setFocusPolicy(Qt::NoFocus);
    auto* beside_file = tabs_->addWidget(file_menu_button_);

    // Export is how work leaves ERDFlow. It waited until there was something
    // to hand on, which there now is. Convert is still waiting, because there
    // is nothing to convert to.
    auto* exporting = add_row("Export", "tabExport", "exportTools");
    exporting->setToolButtonStyle(home_->toolButtonStyle());
    connect(home_, &QToolBar::toolButtonStyleChanged, exporting, &QToolBar::setToolButtonStyle);
    if (auto* export_menu = window.findChild<QMenu*>("exportMenu"))
        for (auto* action : export_menu->actions()) {
            // A menu's headings are entries that cannot be chosen, which is
            // what makes them readable in a menu and useless on a row: the row
            // already separates its groups with a line, and a heading here
            // would be a button nobody can press.
            if (action->objectName().endsWith(QLatin1String("Heading"))) {
                exporting->addSeparator();
                continue;
            }
            exporting->addAction(action);
            // An entry carrying a submenu, as the rarer picture formats do, is
            // a button that drops its list: a click on it asks for the list,
            // not for the action that merely names it.
            dress_button(exporting, action);
        }
    auto* export_tab = rows_.back().first;

    // Import has a tab of its own beside Export, because it is its pair and a
    // reader looking for one expects the other in the same place. Its row is
    // short, which is the truth about it: ERDFlow reads what ERDFlow writes,
    // and the entry for reading what other tools write stands there greyed
    // with the reason on it rather than being left out and silent.
    auto* importing = add_row("Import", "tabImport", "importTools");
    importing->setToolButtonStyle(home_->toolButtonStyle());
    connect(home_, &QToolBar::toolButtonStyleChanged, importing, &QToolBar::setToolButtonStyle);
    if (auto* import_menu = window.findChild<QMenu*>("importMenu"))
        for (auto* action : import_menu->actions()) {
            if (action->objectName().endsWith(QLatin1String("Heading"))) { importing->addSeparator(); continue; }
            importing->addAction(action);
        }
    auto* import_tab = rows_.back().first;

    // Design is how the diagram looks: the choices the View menu keeps in its
    // submenus, and the line style Connect keeps on its arrow.
    auto* view_menu = window.findChild<QMenu*>("viewMenu");
    auto* design = add_row("Design", "tabDesign", "designTools");
    if (view_menu)
        for (auto* action : view_menu->actions())
            if (action->menu()) {
                design->addAction(action);
                // A click on the button is a request for the menu, not for
                // the action that merely names it.
                dress_button(design, action);
            }
    if (auto* connect_button = window.findChild<QToolButton*>("connectButton"))
        add_menu_button(design, "Lines", "designLinesButton", connect_button->menu())
            ->setToolTip("How connectors are drawn.");
    auto* design_tab = rows_.back().first;

    // View is what the window shows and how much of it: the panels, the
    // framing and the grid, which is the rest of the View menu.
    auto* view = add_row("View", "tabView", "viewTools");
    if (view_menu)
        for (auto* action : view_menu->actions())
            if (!action->menu()) view->addAction(action);
    auto* view_tab = rows_.back().first;

    auto* help = add_row("Help", "tabHelp", "helpTools");
    if (auto* help_menu = window.findChild<QMenu*>("helpMenu"))
        for (auto* action : help_menu->actions()) help->addAction(action);
    auto* help_tab = rows_.back().first;

    // File gathers what goes in and out of the project, Settings how the
    // window looks, what it shows, and help. Each opens on its first row and
    // after that on whichever was last chosen.
    groups_.push_back({file, {export_tab, import_tab}, export_tab, beside_file});
    groups_.push_back({home, {}, home, nullptr});
    groups_.push_back({settings, {design_tab, view_tab, help_tab}, design_tab, nullptr});
    for (const auto& group : groups_) {
        connect(group.tab, &QAction::triggered, this, [this, tab = group.tab] {
            for (const auto& candidate : groups_)
                if (candidate.tab == tab) show_row(candidate.chosen);
        });
        for (auto* section : group.sections)
            connect(section, &QAction::triggered, this, [this, section] { show_row(section); });
    }

    // Fitting the window changes the size of Home's buttons, and every row
    // has to change with it.
    home_->installEventFilter(this);
    connect(home_, &QToolBar::iconSizeChanged, this, [this] { match_home_height(); });
    connect(home_, &QToolBar::toolButtonStyleChanged, this, [this] { match_home_height(); });
    match_home_height();
    show_row(home);
}

QAction* Ribbon::add_tab(const QString& label, const char* name) {
    auto* tab = tabs_->addAction(label);
    tab->setObjectName(name);
    // Checked while it is the one chosen; show_row keeps exactly the chosen
    // tabs checked, so a second press on a chosen tab leaves it chosen.
    tab->setCheckable(true);
    // A tab with an icon shows it before its name; one without stays as it
    // was, its name alone, and no wider for the icon it does not have.
    connect(tab, &QAction::changed, this, [this, tab] {
        if (auto* button = qobject_cast<QToolButton*>(tabs_->widgetForAction(tab)))
            button->setToolButtonStyle(tab->icon().isNull() ? Qt::ToolButtonTextOnly
                                                            : Qt::ToolButtonTextBesideIcon);
    });
    return tab;
}

QToolBar* Ribbon::add_row(const QString& label, const char* tab_name, const char* row_name) {
    auto* tab = add_tab(label, tab_name);
    auto* row = new QToolBar(label, &window_);
    row->setObjectName(row_name);
    // Marked as one of the rows that belong to a tab, so the stylesheet can
    // set all of them at once and any row added later is set with them. Home
    // is not marked: it is the drawing tools, which carry icons and are
    // already told apart by them.
    row->setProperty("ribbonRow", true);
    row->setMovable(false);
    row->setFloatable(false);
    row->toggleViewAction()->setVisible(false);
    // Drawn at Home's icon size, and following it as the window is fitted, so
    // every row is the same height and switching tabs moves nothing beneath
    // them. A row whose buttons have no icons keeps its names whatever Home
    // does with its own: with no icon to fall back on, a bare button would
    // be a blank.
    row->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    row->setIconSize(home_->iconSize());
    connect(home_, &QToolBar::iconSizeChanged, row, &QToolBar::setIconSize);
    window_.addToolBar(row);
    row->hide();
    rows_.emplace_back(tab, row);
    return row;
}

QToolButton* Ribbon::add_menu_button(QToolBar* row, const QString& label, const char* name, QMenu* menu,
                                     QAction* default_action) {
    auto* button = new QToolButton(row);
    button->setObjectName(name);
    if (default_action) {
        button->setDefaultAction(default_action);
        button->setPopupMode(QToolButton::MenuButtonPopup);
    } else {
        button->setText(label);
        button->setPopupMode(QToolButton::InstantPopup);
    }
    button->setMenu(menu);
    // A widget on a toolbar inherits none of its presentation and has to be
    // told to match it, now and whenever the row changes.
    button->setToolButtonStyle(row->toolButtonStyle());
    button->setIconSize(row->iconSize());
    connect(row, &QToolBar::iconSizeChanged, button, &QToolButton::setIconSize);
    connect(row, &QToolBar::toolButtonStyleChanged, button, &QToolButton::setToolButtonStyle);
    row->addWidget(button);
    return button;
}

void Ribbon::set_row_icon(QAction* action, const QIcon& icon) {
    if (!action) return;
    const bool first = !row_icons_.contains(action);
    row_icons_[action] = icon;
    // A button restates its action's icon whenever the action changes -- its
    // name, whether it is checked, whether it can be pressed -- and for these
    // that icon is none. The action says it has changed only once its buttons
    // have been told, so the ribbon's icon is put back then.
    if (first) connect(action, &QAction::changed, this, [this, action] { wear_row_icon(action); });
    wear_row_icon(action);
}

void Ribbon::wear_row_icon(QAction* action) const {
    const auto found = row_icons_.find(action);
    if (found == row_icons_.end()) return;
    for (const auto& [tab, row] : rows_)
        if (auto* button = qobject_cast<QToolButton*>(row->widgetForAction(action))) button->setIcon(found->second);
}

void Ribbon::show_row(QAction* tab) {
    const Group* chosen = nullptr;
    for (auto& group : groups_)
        if (group.tab == tab || std::find(group.sections.begin(), group.sections.end(), tab) != group.sections.end()) {
            group.chosen = tab;
            chosen = &group;
        }
    if (!chosen) return;
    current_ = tab;
    for (const auto& group : groups_) {
        const bool in_front = &group == chosen;
        group.tab->setChecked(in_front);
        for (auto* section : group.sections) {
            section->setVisible(in_front);
            section->setChecked(section == tab);
        }
        if (group.beside) group.beside->setVisible(in_front);
    }
    rule_->setVisible(!chosen->sections.empty());
    settle();
}

void Ribbon::settle() {
    if (!tabs_) return;
    tabs_->setVisible(!away_);
    for (const auto& [tab, row] : rows_) row->setVisible(!away_ && tab == current_ && !(schema_ && row == home_));
}

void Ribbon::set_put_away(bool away) {
    away_ = away;
    settle();
}

void Ribbon::set_schema_in_front(bool schema) {
    if (!tabs_ || schema_ == schema) return;
    schema_ = schema;
    for (const auto& [row, order] : orders_) rescope(row);
    settle();
    match_home_height();
}

std::vector<QAction*>& Ribbon::order_of(QToolBar* row) {
    const auto [found, fresh] = orders_.try_emplace(row);
    if (fresh) {
        const auto standing = row->actions();
        found->second.assign(standing.begin(), standing.end());
    }
    return found->second;
}

void Ribbon::keep_to_conceptual(QAction* action) {
    if (!action) return;
    conceptual_only_.insert(action);
    for (const auto& [tab, row] : rows_)
        if (row != home_ && row->actions().contains(action)) {
            order_of(row);
            if (schema_) rescope(row);
        }
}

void Ribbon::add_for_schema(const QString& tab_name, QAction* action, QAction* after) {
    if (!action) return;
    QToolBar* row = nullptr;
    for (const auto& [tab, candidate] : rows_)
        if (tab->objectName() == tab_name) row = candidate;
    if (!row || row == home_) return;
    auto& order = order_of(row);
    const auto at = std::find(order.begin(), order.end(), after);
    order.insert(at == order.end() ? order.end() : at + 1, action);
    schema_only_.insert(action);
    rescope(row);
}

void Ribbon::rescope(QToolBar* row) {
    auto& order = order_of(row);
    const auto in_scope = [this](QAction* action) {
        return schema_ ? !conceptual_only_.contains(action) : !schema_only_.contains(action);
    };
    // A line between groups stays while there is something on either side of
    // it: one whose group beyond is all left out goes with it, so no two lines
    // meet and none is left trailing. With nothing left out, every line the
    // row had stands, as it did.
    std::vector<bool> wanted(order.size());
    for (std::size_t index = 0; index < order.size(); ++index) {
        auto* action = order[index];
        if (!action->isSeparator()) {
            wanted[index] = in_scope(action);
            continue;
        }
        bool before_any = false, before_kept = false;
        for (std::size_t back = 0; back < index; ++back)
            if (!order[back]->isSeparator()) {
                before_any = true;
                before_kept = before_kept || in_scope(order[back]);
            }
        bool after_any = false, after_kept = false;
        for (std::size_t ahead = index + 1; ahead < order.size() && !order[ahead]->isSeparator(); ++ahead) {
            after_any = true;
            after_kept = after_kept || in_scope(order[ahead]);
        }
        wanted[index] = (!before_any || before_kept) && (!after_any || after_kept);
    }
    for (std::size_t index = 0; index < order.size(); ++index) {
        auto* action = order[index];
        const bool standing = row->actions().contains(action);
        if (standing == wanted[index]) continue;
        if (!wanted[index]) {
            row->removeAction(action);
            continue;
        }
        QAction* before = nullptr;
        for (std::size_t ahead = index + 1; ahead < order.size() && !before; ++ahead)
            if (row->actions().contains(order[ahead])) before = order[ahead];
        row->insertAction(before, action);
        dress_button(row, action);
        wear_row_icon(action);
    }
}

void Ribbon::dress_button(QToolBar* row, QAction* action) {
    // A command that carries a submenu is a button that drops its list: a
    // click on it is a request for the list, not for the action that merely
    // names it.
    if (action->menu())
        if (auto* button = qobject_cast<QToolButton*>(row->widgetForAction(action)))
            button->setPopupMode(QToolButton::InstantPopup);
}

// A button with no icon is only as tall as its name, so a row of them would
// be shorter than Home and the canvas would jump as the tabs were switched.
// Every button on every row is held to the height of Home's tallest, which
// is what makes the rows one ribbon rather than several toolbars.
void Ribbon::match_home_height() {
    // Measured on the buttons Home makes for its own actions. Those are given
    // a new icon size before word of it reaches here, whereas the widgets
    // added to Home by hand are only brought up to date afterwards.
    int tallest = 0;
    for (auto* action : home_->actions())
        if (!action->isSeparator() && !qobject_cast<QWidgetAction*>(action))
            if (auto* button = home_->widgetForAction(action)) tallest = std::max(tallest, button->sizeHint().height());
    // Home is taller than its own buttons whenever a widget it carries is
    // taller than they are, as the notation picker is; and it is shorter than
    // it asks to be whenever the window gives it less. A row measured from
    // either would sit at the wrong height, and everything beneath the ribbon
    // would jump as the tabs were changed. So Home is measured as it actually
    // stands, while it is showing, and that measurement is what the other rows
    // are held to until Home is measured again.
    if (home_->isVisible() && home_->height() > 0) home_height_ = home_->height();
    const int together = std::max(tallest, home_height_);
    for (const auto& [tab, row] : rows_)
        if (row != home_) {
            for (auto* button : row->findChildren<QToolButton*>()) button->setMinimumHeight(tallest);
            row->setFixedHeight(together);
        }
}

bool Ribbon::eventFilter(QObject* watched, QEvent* event) {
    // Home changes height when the window is resized and its icons with it,
    // and there is no signal for that, so it is watched.
    if (watched == home_ && (event->type() == QEvent::Resize || event->type() == QEvent::Show))
        match_home_height();
    return QObject::eventFilter(watched, event);
}

void Ribbon::show_tab(const QString& name) {
    for (const auto& group : groups_) {
        if (group.tab->objectName() == name) { show_row(group.chosen); return; }
        for (auto* section : group.sections)
            if (section->objectName() == name) { show_row(section); return; }
    }
}

} // namespace erdflow::desktop
