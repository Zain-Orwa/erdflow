#include "theme.hpp"

#include <QApplication>
#include <QPalette>

#include <algorithm>

namespace erdflow::desktop {
namespace {

QColor color(const char* value) { return QColor(QString::fromLatin1(value)); }

// Application chrome and diagram colors live together so switching appearance
// never changes the document or leaves a light canvas in a dark workspace.
const std::array<Theme, 6> theme_table{{
    {ThemeId::OfficeLight, QStringLiteral("office-light"), QStringLiteral("Office Light"),
     color("#F1F1F1"), color("#F6F6F6"), color("#FFFFFF"), color("#202020"),
     color("#595959"), color("#B7B7B7"), color("#006AA6"), color("#FFFFFF"),
     color("#FFFFFF"), color("#E0E0E0"), color("#FFB575"), color("#613714"),
     color("#FFF0DD"), color("#765034"), color("#FFE1BD"), color("#765034"),
     color("#241D16"), color("#545454")},
    {ThemeId::WarmLight, QStringLiteral("warm-light"), QStringLiteral("Warm Light"),
     color("#EEE8D5"), color("#F4EFDE"), color("#FFFBEF"), color("#3D494A"),
     color("#60676A"), color("#BAB4A3"), color("#006B68"), color("#FFFFFF"),
     color("#FFFDF5"), color("#DFD9C6"), color("#EBD5A0"), color("#665535"),
     color("#F4E9CC"), color("#665535"), color("#D8E6CC"), color("#4C6240"),
     color("#263532"), color("#526260")},
    {ThemeId::Graphite, QStringLiteral("graphite"), QStringLiteral("Graphite"),
     color("#272B31"), color("#30353D"), color("#20242A"), color("#ECEFF4"),
     color("#B4BDC9"), color("#5C6775"), color("#7DB7ED"), color("#17212B"),
     color("#20242A"), color("#353C45"), color("#414B58"), color("#BDCADD"),
     color("#303A45"), color("#ADBFD1"), color("#354C59"), color("#A4C3D1"),
     color("#F3F5F7"), color("#C4CDD8")},
    {ThemeId::Midnight, QStringLiteral("midnight"), QStringLiteral("Midnight"),
     color("#101B2B"), color("#18263A"), color("#0D1726"), color("#E6EEF8"),
     color("#AAB9CD"), color("#53657D"), color("#67C3DF"), color("#10202D"),
     color("#101C2B"), color("#26364C"), color("#253F5D"), color("#9FBDE5"),
     color("#1F344A"), color("#A8BFD8"), color("#253F4B"), color("#99C9D7"),
     color("#EDF4FC"), color("#BBCBE0")},
    {ThemeId::Dracula, QStringLiteral("dracula"), QStringLiteral("Dracula"),
     color("#282A36"), color("#303341"), color("#242631"), color("#F8F8F2"),
     color("#BDCADB"), color("#676D84"), color("#BD93F9"), color("#282A36"),
     color("#282A36"), color("#414555"), color("#443B5A"), color("#BD93F9"),
     color("#35434C"), color("#8BE9FD"), color("#4D3B51"), color("#FF79C6"),
     color("#F8F8F2"), color("#DADBE5")},
    {ThemeId::HighContrast, QStringLiteral("high-contrast"), QStringLiteral("High Contrast"),
     color("#000000"), color("#000000"), color("#000000"), color("#FFFFFF"),
     color("#E6E6E6"), color("#FFFFFF"), color("#FFFF00"), color("#000000"),
     color("#000000"), color("#3F3F3F"), color("#000000"), color("#FFFFFF"),
     color("#000000"), color("#FFFFFF"), color("#000000"), color("#FFFFFF"),
     color("#FFFFFF"), color("#FFFFFF")},
}};

QString style_sheet(const Theme& colors) {
    // Keep geometry compact and consistent between themes. Color placeholders
    // reference the table above; the stylesheet contains no second palette.
    QString sheet = QStringLiteral(R"(
QMainWindow, QDialog { background: @window@; color: @text@; }
QWidget { selection-background-color: @accent@; selection-color: @selected@; }
QMenuBar { background: @window@; color: @text@; border-bottom: 1px solid @border@; }
QMenuBar::item { padding: 5px 10px; background: transparent; }
QMenuBar::item:selected, QMenuBar::item:pressed { background: @accent@; color: @selected@; }
QMenu { background: @panel@; color: @text@; border: 1px solid @border@; padding: 3px; }
QMenu::item { padding: 5px 22px; }
QMenu::item:selected { background: @accent@; color: @selected@; }
QMenu::item:disabled { color: @muted@; }
QMenu::separator { height: 1px; background: @border@; margin: 4px 5px; }
QToolBar { background: @window@; color: @text@; border: none; border-bottom: 1px solid @border@; spacing: 3px; padding: 4px 6px; }
QToolBar::separator { background: @border@; width: 1px; margin: 4px 7px; }
QToolButton { background: transparent; color: @text@; border: 1px solid transparent; border-radius: 2px; padding: 5px 8px; }
QToolButton:hover { background: @base@; border-color: @border@; }
QToolButton:checked, QToolButton:pressed { background: @accent@; color: @selected@; border-color: @accent@; }
QToolButton:focus { border-color: @accent@; }
QToolButton:disabled { color: @muted@; }
QToolBar#diagramTools { background: @panel@; spacing: 2px; padding: 4px; }
QToolBar#diagramTools QToolButton { text-align: left; padding: 6px 8px; }
QToolButton#themeButton { border-color: @border@; background: @panel@; padding: 5px 9px; }
QToolButton#themeButton:hover, QToolButton#themeButton:pressed { border-color: @accent@; background: @base@; color: @text@; }
QDockWidget { background: @panel@; color: @text@; }
QDockWidget::title { background: @window@; color: @text@; padding: 6px 8px; border-bottom: 1px solid @border@; font-weight: 600; }
QDockWidget::close-button, QDockWidget::float-button { border: 1px solid transparent; padding: 2px; }
QDockWidget::close-button:hover, QDockWidget::float-button:hover { background: @base@; border-color: @border@; }
QMainWindow::separator { background: @border@; width: 1px; height: 1px; }
QTreeView { background: @panel@; alternate-background-color: @base@; color: @text@; border: none; padding: 3px; outline: 0; }
QTreeView::item { padding: 4px 2px; border: 1px solid transparent; }
QTreeView::item:hover { border-color: @border@; }
QTreeView::item:selected { background: @accent@; color: @selected@; }
QTreeView::item:focus { border-color: @accent@; }
QHeaderView::section { background: @window@; color: @text@; border: none; border-bottom: 1px solid @border@; border-right: 1px solid @border@; padding: 5px 7px; }
QScrollArea { background: @panel@; border: none; }
QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox, QPlainTextEdit { background: @base@; color: @text@; border: 1px solid @border@; border-radius: 2px; padding: 4px 6px; }
QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QSpinBox:focus, QPlainTextEdit:focus { border-color: @accent@; }
QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled, QSpinBox:disabled, QPlainTextEdit:disabled { background: @panel@; color: @muted@; }
QComboBox { padding-right: 22px; }
QComboBox QAbstractItemView { background: @base@; color: @text@; border: 1px solid @border@; selection-background-color: @accent@; selection-color: @selected@; }
QPushButton { background: @window@; color: @text@; border: 1px solid @border@; border-radius: 2px; padding: 5px 11px; }
QPushButton:hover, QPushButton:focus { border-color: @accent@; }
QPushButton:pressed, QPushButton:checked { background: @accent@; color: @selected@; border-color: @accent@; }
QPushButton:default { border-color: @accent@; }
QPushButton:disabled { color: @muted@; }
QLabel { color: @text@; }
QLabel:disabled { color: @muted@; }
QLabel#hint { color: @muted@; font-size: 12px; }
QWidget#workspaceHeader { background: @panel@; border-bottom: 1px solid @border@; }
QLabel#workspaceBadge { color: @accent@; font-size: 11px; font-weight: 700; padding-right: 10px; }
QLabel#documentTitle { color: @text@; font-size: 13px; font-weight: 600; }
QLabel#propertyHeading { color: @accent@; font-size: 13px; font-weight: 600; }
QWidget#participantCard { background: @panel@; border: 1px solid @border@; border-radius: 2px; }
QTabBar::tab { background: @window@; color: @text@; border: 1px solid @border@; padding: 6px 14px; }
QTabBar::tab:selected { background: @base@; color: @text@; border-bottom: 2px solid @accent@; }
QTabBar::tab:hover { color: @accent@; }
QTabWidget::pane { border: 1px solid @border@; }
QStatusBar { background: @window@; color: @text@; border-top: 1px solid @border@; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @muted@; padding: 2px 6px; }
QToolTip { background: @panel@; color: @text@; border: 1px solid @border@; padding: 4px 6px; }
)");
    const std::array<std::pair<QString, QColor>, 9> replacements{{
        {QStringLiteral("@window@"), colors.window}, {QStringLiteral("@panel@"), colors.panel},
        {QStringLiteral("@base@"), colors.base}, {QStringLiteral("@text@"), colors.text},
        {QStringLiteral("@muted@"), colors.muted}, {QStringLiteral("@border@"), colors.border},
        {QStringLiteral("@accent@"), colors.accent}, {QStringLiteral("@selected@"), colors.selected_text},
        {QStringLiteral("@canvas@"), colors.canvas},
    }};
    for (const auto& [placeholder, value] : replacements) sheet.replace(placeholder, value.name());
    return sheet;
}

} // namespace

const std::array<Theme, 6>& themes() { return theme_table; }

const Theme& theme(ThemeId id) {
    const auto found = std::find_if(theme_table.begin(), theme_table.end(),
                                    [id](const Theme& candidate) { return candidate.id == id; });
    return found == theme_table.end() ? theme_table.front() : *found;
}

ThemeId theme_from_key(const QString& key) {
    const auto found = std::find_if(theme_table.begin(), theme_table.end(),
                                    [&key](const Theme& candidate) { return candidate.key == key; });
    return found == theme_table.end() ? ThemeId::OfficeLight : found->id;
}

void apply_theme(QApplication& app, ThemeId id) {
    const auto& colors = theme(id);
    QPalette palette;
    for (const auto group : {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
        const auto foreground = group == QPalette::Disabled ? colors.muted : colors.text;
        palette.setColor(group, QPalette::Window, colors.window);
        palette.setColor(group, QPalette::WindowText, foreground);
        palette.setColor(group, QPalette::Base, colors.base);
        palette.setColor(group, QPalette::AlternateBase, colors.panel);
        palette.setColor(group, QPalette::Text, foreground);
        palette.setColor(group, QPalette::Button, colors.panel);
        palette.setColor(group, QPalette::ButtonText, foreground);
        palette.setColor(group, QPalette::BrightText, colors.text);
        palette.setColor(group, QPalette::Light, colors.panel.lighter(120));
        palette.setColor(group, QPalette::Midlight, colors.panel.lighter(110));
        palette.setColor(group, QPalette::Mid, colors.border);
        palette.setColor(group, QPalette::Dark, colors.border.darker(130));
        palette.setColor(group, QPalette::Shadow, colors.border.darker(180));
        palette.setColor(group, QPalette::Highlight, colors.accent);
        palette.setColor(group, QPalette::HighlightedText, colors.selected_text);
        palette.setColor(group, QPalette::Link, colors.accent);
        palette.setColor(group, QPalette::LinkVisited, colors.accent);
        palette.setColor(group, QPalette::ToolTipBase, colors.panel);
        palette.setColor(group, QPalette::ToolTipText, colors.text);
        palette.setColor(group, QPalette::PlaceholderText, colors.muted);
        palette.setColor(group, QPalette::Accent, colors.accent);
    }
    app.setPalette(palette);
    app.setStyleSheet(style_sheet(colors));
}

} // namespace erdflow::desktop
