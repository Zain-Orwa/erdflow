#include "ribbon.hpp"

#include <QAction>
#include <QActionGroup>
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
    // The tabs are the only way to the rows, so the window's context menu is
    // not allowed to close them: a row with no tab to reach it is a dead end.
    tabs_->toggleViewAction()->setVisible(false);
    // Above the tool row, on a line of its own.
    window.insertToolBar(home_, tabs_);
    window.insertToolBarBreak(home_);

    // File is a list of things to do with the whole project rather than a row
    // of tools, so it drops down from its tab.
    add_menu_tab("File", "tabFile", window.findChild<QMenu*>("fileMenu"));

    // Home is the tool row the window already has.
    rows_.emplace_back(add_tab("Home", "tabHome"), home_);

    // Insert carries what is brought in from outside the model: for now a
    // picture from a file. The model's own elements, and the note placed like
    // one, stay on Home.
    auto* insert = add_row("Insert", "tabInsert", "insertTools");
    insert->setToolButtonStyle(home_->toolButtonStyle());
    connect(home_, &QToolBar::toolButtonStyleChanged, insert, &QToolBar::setToolButtonStyle);
    if (auto* insert_menu = window.findChild<QMenu*>("insertMenu"))
        for (auto* action : insert_menu->actions()) {
            insert->addAction(action);
            // An entry that carries a submenu, as Symbols does, is a button
            // that drops its list: a click on it is a request for the list,
            // not for the action that merely names it.
            if (action->menu())
                if (auto* button = qobject_cast<QToolButton*>(insert->widgetForAction(action)))
                    button->setPopupMode(QToolButton::InstantPopup);
        }

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
                if (auto* button = qobject_cast<QToolButton*>(design->widgetForAction(action)))
                    button->setPopupMode(QToolButton::InstantPopup);
            }
    if (auto* connect_button = window.findChild<QToolButton*>("connectButton"))
        add_menu_button(design, "Lines", "designLinesButton", connect_button->menu())
            ->setToolTip("How connectors are drawn.");

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
            if (action->menu())
                if (auto* button = qobject_cast<QToolButton*>(exporting->widgetForAction(action)))
                    button->setPopupMode(QToolButton::InstantPopup);
        }

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

    // View is what the window shows and how much of it: the panels, the
    // framing and the grid, which is the rest of the View menu.
    auto* view = add_row("View", "tabView", "viewTools");
    if (view_menu)
        for (auto* action : view_menu->actions())
            if (!action->menu()) view->addAction(action);

    auto* help = add_row("Help", "tabHelp", "helpTools");
    if (auto* help_menu = window.findChild<QMenu*>("helpMenu"))
        for (auto* action : help_menu->actions()) help->addAction(action);

    // Fitting the window changes the size of Home's buttons, and every row
    // has to change with it.
    home_->installEventFilter(this);
    connect(home_, &QToolBar::iconSizeChanged, this, [this] { match_home_height(); });
    connect(home_, &QToolBar::toolButtonStyleChanged, this, [this] { match_home_height(); });
    match_home_height();
    show_row(rows_.front().first);
}

QAction* Ribbon::add_tab(const QString& label, const char* name) {
    static constexpr const char* group_name = "ribbonTabGroup";
    auto* group = tabs_->findChild<QActionGroup*>(group_name);
    if (!group) {
        group = new QActionGroup(tabs_);
        group->setObjectName(group_name);
        group->setExclusive(true);
    }
    auto* tab = tabs_->addAction(label);
    tab->setObjectName(name);
    tab->setCheckable(true);
    tab->setActionGroup(group);
    connect(tab, &QAction::triggered, this, [this, tab] { show_row(tab); });
    return tab;
}

void Ribbon::add_menu_tab(const QString& label, const char* name, QMenu* menu) {
    auto* tab = new QToolButton(tabs_);
    tab->setObjectName(name);
    tab->setText(label);
    tab->setMenu(menu);
    tab->setPopupMode(QToolButton::InstantPopup);
    tab->setToolButtonStyle(Qt::ToolButtonTextOnly);
    tab->setFocusPolicy(Qt::NoFocus);
    tabs_->addWidget(tab);
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

void Ribbon::show_row(QAction* tab) {
    for (const auto& [candidate, row] : rows_) row->setVisible(candidate == tab);
    tab->setChecked(true);
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
    for (const auto& [tab, row] : rows_)
        if (tab->objectName() == name) { show_row(tab); return; }
}

} // namespace erdflow::desktop
