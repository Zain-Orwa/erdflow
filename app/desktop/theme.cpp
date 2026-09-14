#include "theme.hpp"

#include <QApplication>
#include <QPalette>

#include <algorithm>
#include <cmath>

namespace erdflow::desktop {
namespace {

QColor color(const char* value) { return QColor(QString::fromLatin1(value)); }

// Blends one colour into another. A shape needs a surface of its own, not just
// a coloured edge, but a palette's accent used neat is far too bright to carry
// a label, so the accent is mixed into a panel shade instead.
// How bright a colour is to the eye, and how far apart two of them are. Both
// are the definitions the theme tests hold every palette to, kept here so that
// anything deriving a colour can check its own work against the same rule.
double relative_luminance(const QColor& colour) {
    const auto channel = [](double value) {
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(colour.redF()) + 0.7152 * channel(colour.greenF())
         + 0.0722 * channel(colour.blueF());
}
double contrast(const QColor& first, const QColor& second) {
    const auto one = relative_luminance(first);
    const auto other = relative_luminance(second);
    return (std::max(one, other) + 0.05) / (std::min(one, other) + 0.05);
}

QColor mix(const QColor& base, const QColor& tint, double amount) {
    return QColor::fromRgbF(base.redF() * (1 - amount) + tint.redF() * amount,
                            base.greenF() * (1 - amount) + tint.greenF() * amount,
                            base.blueF() * (1 - amount) + tint.blueF() * amount);
}

// How far a shape's fill is carried towards its accent, and how far the label
// over it is lifted towards white. The pair is chosen together: every fill this
// produces holds at least 5:1 against the label, across all twelve families.
constexpr double shape_tint = 0.28;
constexpr double label_lift = 0.6;

// The Study Ledger palette roles, kept under their upstream names so a family
// can be checked against the source it was taken from. Only the last three are
// ERDFlow's own: the shared palettes carry no colour for a failed rule.
struct Palette {
    const char *bg, *bg2, *card, *card2, *card3, *line, *text, *muted;
    const char *accent, *accent_soft, *mint, *cyan, *gold, *active_text;
    const char *valid, *warning, *error;
};

// One palette becomes one theme by assignment alone. Every family is shaded the
// same way, so a new palette needs no drawing code and cannot drift from the
// others in how it uses depth, and the diagram colours are derived here rather
// than chosen twice: switching appearance never edits the document.
//
// The board is the darkest surface and the chrome around it is lifted a step,
// rather than the two sharing a shade. These palettes step gently, so without
// that separation the canvas and the panels read as one continuous field and
// the diagram loses its edge.
Theme derive(ThemeId id, const char* key, const char* label, const Palette& p) {
    return Theme{
        id, QString::fromLatin1(key), QString::fromLatin1(label),
        color(p.card), color(p.card2), color(p.card3), color(p.text),
        color(p.muted), color(p.line), color(p.accent), color(p.active_text),
        color(p.bg), color(p.card2),
        mix(color(p.card), color(p.accent), shape_tint), color(p.accent),
        mix(color(p.card), color(p.mint), shape_tint), color(p.mint),
        mix(color(p.card), color(p.gold), shape_tint), color(p.gold),
        mix(color(p.card), color(p.cyan), shape_tint), color(p.cyan),
        mix(color(p.text), QColor(Qt::white), label_lift), color(p.muted), color(p.accent_soft),
        color(p.valid), color(p.warning), color(p.error)};
}

// ERDFlow's original themes, restored unchanged beside the palette families.
// They were written before the struct carried ISA, selection and verdict
// colours, so those are filled in the way the code used to behave: ISA borrowed
// the relationship's shading and selection was the accent. Only the three
// verdict colours are new, chosen to clear the contrast gate on each canvas.
Theme legacy(ThemeId id, const char* key, const char* label,
             const char* window, const char* panel, const char* base, const char* text,
             const char* muted, const char* border, const char* accent, const char* selected_text,
             const char* canvas, const char* grid, const char* entity_fill, const char* entity_border,
             const char* attribute_fill, const char* attribute_border,
             const char* relationship_fill, const char* relationship_border,
             const char* node_text, const char* connector,
             const char* valid, const char* warning, const char* error) {
    return Theme{
        id, QString::fromLatin1(key), QString::fromLatin1(label),
        color(window), color(panel), color(base), color(text),
        color(muted), color(border), color(accent), color(selected_text),
        color(canvas), color(grid),
        color(entity_fill), color(entity_border),
        color(attribute_fill), color(attribute_border),
        color(relationship_fill), color(relationship_border),
        color(relationship_fill), color(relationship_border),
        color(node_text), color(connector), color(accent),
        color(valid), color(warning), color(error)};
}

// Solarized Dark is deliberately low contrast: its body foreground over the
// panel shades falls below the 4.5:1 the theme tests require, so the family
// uses its own base2 for text instead of base1. Every other palette is verbatim.
const std::array<Theme, theme_count> theme_table{{
    legacy(ThemeId::OfficeLight, "office-light", "Normal",
           "#F1F1F1", "#F6F6F6", "#FFFFFF", "#202020", "#595959", "#B7B7B7", "#006AA6", "#FFFFFF",
           "#FFFFFF", "#E0E0E0", "#FFB575", "#613714", "#FFF0DD", "#765034", "#FFE1BD", "#765034",
           "#241D16", "#545454", "#1B7A33", "#8A5A00", "#B3261E"),
    legacy(ThemeId::WarmLight, "warm-light", "Warm Light",
           "#EEE8D5", "#F4EFDE", "#FFFBEF", "#3D494A", "#60676A", "#BAB4A3", "#006B68", "#FFFFFF",
           "#FFFDF5", "#DFD9C6", "#EBD5A0", "#665535", "#F4E9CC", "#665535", "#D8E6CC", "#4C6240",
           "#263532", "#526260", "#3D6B2E", "#8A5A00", "#A8322A"),
    // Paper and ink, the way an ERD is drawn on a board or printed in a book.
    // The shapes are told apart by how grey they are rather than by hue, so the
    // diagram survives being photocopied or read by anyone who cannot rely on
    // colour, and nothing on it competes with the model for attention.
    legacy(ThemeId::Plain, "plain", "Plain",
           "#ECECEC", "#F4F4F4", "#FFFFFF", "#000000", "#595959", "#9A9A9A", "#333333", "#FFFFFF",
           "#FFFFFF", "#E0E0E0", "#DCDCDC", "#000000", "#F2F2F2", "#000000", "#E7E7E7", "#000000",
           "#000000", "#000000", "#3C3C3C", "#6E6E6E", "#1A1A1A"),
    legacy(ThemeId::Graphite, "graphite", "Graphite",
           "#272B31", "#30353D", "#20242A", "#ECEFF4", "#B4BDC9", "#5C6775", "#7DB7ED", "#17212B",
           "#20242A", "#353C45", "#414B58", "#BDCADD", "#303A45", "#ADBFD1", "#354C59", "#A4C3D1",
           "#F3F5F7", "#C4CDD8", "#7DCF97", "#E8B75A", "#F08A8F"),
    legacy(ThemeId::Midnight, "midnight", "Midnight",
           "#101B2B", "#18263A", "#0D1726", "#E6EEF8", "#AAB9CD", "#53657D", "#67C3DF", "#10202D",
           "#101C2B", "#26364C", "#253F5D", "#9FBDE5", "#1F344A", "#A8BFD8", "#253F4B", "#99C9D7",
           "#EDF4FC", "#BBCBE0", "#7BD7A0", "#E8C06A", "#F0949A"),
    legacy(ThemeId::DraculaClassic, "dracula-classic", "Dracula (Classic)",
           "#282A36", "#303341", "#242631", "#F8F8F2", "#BDCADB", "#676D84", "#BD93F9", "#282A36",
           "#282A36", "#414555", "#443B5A", "#BD93F9", "#35434C", "#8BE9FD", "#4D3B51", "#FF79C6",
           "#F8F8F2", "#DADBE5", "#50FA7B", "#F1FA8C", "#FF5555"),
    legacy(ThemeId::HighContrast, "high-contrast", "High Contrast",
           "#000000", "#000000", "#000000", "#FFFFFF", "#E6E6E6", "#FFFFFF", "#FFFF00", "#000000",
           "#000000", "#3F3F3F", "#000000", "#FFFFFF", "#000000", "#FFFFFF", "#000000", "#FFFFFF",
           "#FFFFFF", "#FFFFFF", "#00FF00", "#FFFF00", "#FF6B6B"),
    derive(ThemeId::Forest, "forest", "Forest",
           {"#06110c", "#0a1a12", "#0d2419", "#113020", "#153b28", "#24543a", "#f2fbf5", "#9ab8a6",
            "#4ade80", "#22c55e", "#86efac", "#67e8f9", "#facc15", "#041008",
            "#4ade80", "#facc15", "#ef4444"}),
    derive(ThemeId::Mono, "mono", "Black & White",
           {"#050505", "#0d0d0d", "#141414", "#1b1b1b", "#242424", "#4a4a4a", "#ffffff", "#a3a3a3",
            "#f5f5f5", "#d4d4d4", "#ffffff", "#cfcfcf", "#bdbdbd", "#050505",
            "#ffffff", "#bdbdbd", "#8a8a8a"}),
    derive(ThemeId::Dracula, "dracula", "Dracula",
           {"#1e1f29", "#282a36", "#2d303d", "#343746", "#3d4050", "#6272a4", "#f8f8f2", "#a6accd",
            "#bd93f9", "#ff79c6", "#8be9fd", "#8be9fd", "#f1fa8c", "#1e1f29",
            "#50fa7b", "#f1fa8c", "#ff5555"}),
    derive(ThemeId::OneDarkPro, "one-dark-pro", "One Dark Pro",
           {"#20232a", "#282c34", "#2c313a", "#333842", "#3b414c", "#4b5363", "#abb2bf", "#7f8796",
            "#61afef", "#528bff", "#56b6c2", "#56b6c2", "#e5c07b", "#15181d",
            "#98c379", "#e5c07b", "#e06c75"}),
    derive(ThemeId::TokyoNight, "tokyo-night", "Tokyo Night",
           {"#15161e", "#1a1b26", "#1f2335", "#24283b", "#2f3549", "#3d59a1", "#c0caf5", "#959cbd",
            "#7aa2f7", "#3d59a1", "#7dcfff", "#7dcfff", "#e0af68", "#101117",
            "#9ece6a", "#e0af68", "#f7768e"}),
    derive(ThemeId::CatppuccinMocha, "catppuccin-mocha", "Catppuccin Mocha",
           {"#11111b", "#181825", "#1e1e2e", "#242438", "#313244", "#45475a", "#cdd6f4", "#a6adc8",
            "#cba6f7", "#b4befe", "#89dceb", "#89dceb", "#f9e2af", "#11111b",
            "#a6e3a1", "#f9e2af", "#f38ba8"}),
    derive(ThemeId::Gruvbox, "gruvbox", "Gruvbox Dark",
           {"#1d2021", "#282828", "#32302f", "#3c3836", "#504945", "#665c54", "#ebdbb2", "#a89984",
            "#b8bb26", "#98971a", "#8ec07c", "#83a598", "#fabd2f", "#1d2021",
            "#b8bb26", "#fabd2f", "#fb4934"}),
    derive(ThemeId::Solarized, "solarized", "Solarized Dark",
           {"#001f27", "#002b36", "#073642", "#0b414d", "#124d59", "#586e75", "#eee8d5", "#839496",
            "#268bd2", "#2aa198", "#2aa198", "#268bd2", "#b58900", "#00171d",
            "#859900", "#b58900", "#dc322f"}),
    derive(ThemeId::GitHubDark, "github-dark", "GitHub Dark",
           {"#090c10", "#0d1117", "#161b22", "#1c2128", "#242c36", "#30363d", "#c9d1d9", "#8b949e",
            "#58a6ff", "#1f6feb", "#79c0ff", "#56d4dd", "#d29922", "#06090d",
            "#3fb950", "#d29922", "#f85149"}),
    derive(ThemeId::MaterialOcean, "material-ocean", "Material Ocean",
           {"#1c2529", "#263238", "#2b3a40", "#314249", "#3a4d55", "#546e7a", "#eeffff", "#b0bec5",
            "#89ddff", "#80cbc4", "#89ddff", "#82aaff", "#ffcb6b", "#11191c",
            "#c3e88d", "#ffcb6b", "#f07178"}),
    derive(ThemeId::Nord, "nord", "Nord",
           {"#242933", "#2e3440", "#3b4252", "#434c5e", "#4c566a", "#5e6b82", "#eceff4", "#d8dee9",
            "#88c0d0", "#81a1c1", "#8fbcbb", "#88c0d0", "#ebcb8b", "#1d222b",
            "#a3be8c", "#ebcb8b", "#bf616a"}),
    derive(ThemeId::Monokai, "monokai", "Monokai",
           {"#1f201b", "#272822", "#2f3028", "#383930", "#43443a", "#5b5c50", "#f8f8f2", "#a6a69e",
            "#a6e22e", "#7fb51d", "#66d9ef", "#66d9ef", "#e6db74", "#171813",
            "#a6e22e", "#e6db74", "#f92672"}),
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
QToolButton:hover { background: @hover@; border-color: @hoveredge@; }
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
QTreeView::item { padding: 5px 3px; border: 1px solid transparent; border-radius: 2px; }
QTreeView::item:hover { background: @hover@; border-color: @hoveredge@; }
QTreeView::item:selected { background: @accent@; color: @selected@; }
QTreeView::item:focus { border-color: @accent@; }
QHeaderView::section { background: @window@; color: @text@; border: none; border-bottom: 1px solid @border@; border-right: 1px solid @border@; padding: 5px 7px; }
QScrollArea { background: @panel@; border: none; }
QWidget#canvasControls { background: @panel@; border: 1px solid @border@; border-radius: 8px; }
QWidget#canvasControls QToolButton { background: transparent; color: @text@; border: 1px solid transparent; border-radius: 5px; font-size: 16px; font-weight: 700; }
QWidget#canvasControls QToolButton:hover { background: @hover@; border-color: @hoveredge@; }
QWidget#canvasControls QToolButton:checked { background: @accent@; color: @selected@; border-color: @accent@; }
QGraphicsView#diagramCanvas { background: @canvas@; border: 1px solid @border@; }
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
QLabel#propertyHeading { color: @accent@; font-size: 15px; font-weight: 800; padding-bottom: 2px; }
QWidget#participantCard { background: @panel@; border: 1px solid @border@; border-radius: 3px; margin-bottom: 4px; }
QWidget#participantCard:hover { background: @hover@; border-color: @hoveredge@; }
QTabBar::tab { background: @window@; color: @text@; border: 1px solid @border@; padding: 6px 14px; }
QTabBar::tab:selected { background: @base@; color: @text@; border-bottom: 2px solid @accent@; }
QTabBar::tab:hover { color: @accent@; }
QTabWidget::pane { border: 1px solid @border@; }
QStatusBar { background: @window@; color: @text@; border-top: 1px solid @border@; }
QStatusBar::item { border: none; }
QStatusBar QLabel { color: @muted@; padding: 2px 6px; }
QToolTip { background: @panel@; color: @text@; border: 1px solid @border@; padding: 4px 6px; }
)");
    const std::array<std::pair<QString, QColor>, 11> replacements{{
        {QStringLiteral("@window@"), colors.window}, {QStringLiteral("@panel@"), colors.panel},
        {QStringLiteral("@base@"), colors.base}, {QStringLiteral("@text@"), colors.text},
        {QStringLiteral("@muted@"), colors.muted}, {QStringLiteral("@border@"), colors.border},
        {QStringLiteral("@accent@"), colors.accent}, {QStringLiteral("@selected@"), colors.selected_text},
        {QStringLiteral("@canvas@"), colors.canvas},
        {QStringLiteral("@hover@"), hover_surface(colors)},
        {QStringLiteral("@hoveredge@"), mix(colors.border, colors.accent, 0.6)},
    }};
    for (const auto& [placeholder, value] : replacements) sheet.replace(placeholder, value.name());
    return sheet;
}

} // namespace

// How far a hovered row is carried towards the accent. A strong tint is wanted,
// but a theme whose text is already dim cannot afford one, so the strongest
// that still leaves the row readable is taken rather than one figure being
// imposed on every palette and pitched to the weakest of them.
QColor hover_surface(const Theme& colors) {
    constexpr double readable = 4.5;
    for (double amount = 0.20; amount > 0.04; amount -= 0.02) {
        const auto candidate = mix(colors.panel, colors.accent, amount);
        if (contrast(colors.text, candidate) >= readable) return candidate;
    }
    return mix(colors.panel, colors.accent, 0.04);
}

QColor readable_on(const QColor& surface) {
    return relative_luminance(surface) > 0.36 ? QColor(0x1a, 0x1a, 0x1a) : QColor(0xff, 0xff, 0xff);
}

const std::array<Theme, theme_count>& themes() { return theme_table; }

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
