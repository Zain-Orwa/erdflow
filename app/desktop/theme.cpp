// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
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
    // ERDFlow Azure. White and soft cool grey, a calm medium blue, pale blue
    // for what is hovered or chosen, and small warm gold and lavender accents
    // used sparingly. The diagram colours are drawn from the same family so a
    // model on the canvas belongs to the interface around it.
    // The accent here is Azure's #1976D2 rather than its #1E88E5, and
    // deliberately so. `accent` is what paints a selected row with
    // `selected_text` over it, at ordinary text size, all through the
    // application; white on #1E88E5 is 3.68:1, short of the 4.5 this project
    // holds itself to. #1976D2 is Azure's own primary-hover and reaches 4.55.
    // The token `primary` remains #1E88E5 exactly as specified, and is what
    // paints the cards, the buttons and the brand. So this is an accessibility
    // override derived from Azure, not a departure from it (ADR-022 §9.3).
    legacy(ThemeId::Azure, "erdflow.azure", "ERDFlow Azure",
           "#F6F9FC", "#F8FBFE", "#FFFFFF", "#0F172A", "#64748B", "#DCE6F2", "#1976D2", "#FFFFFF",
           "#FFFFFF", "#E8EFF8", "#EAF4FF", "#3B82F6", "#F4F9FF", "#60A5FA", "#FFF4D8", "#F4B740",
           "#0F172A", "#64748B", "#15803D", "#B45309", "#DC2626"),
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

// The sheet is the same text for a given theme every time it is asked for,
// and building it walks a ten-kilobyte string once per placeholder. A theme is
// applied far more often than there are themes -- looking down the Theme menu
// applies one per entry passed -- so each is built once and kept. There are
// nineteen of them and the table they come from is fixed, so the cache has a
// ceiling and never needs clearing.
QString build_style_sheet(const Theme& colors) {
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
QToolButton#themeButton { border-color: @border@; background: @panel@; padding: 5px 22px 5px 9px; }
QToolButton#themeButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 4px; width: 12px; }
/* The same again for the search's own options button: a style drops a menu
   arrow into the bottom-right corner unless it is told where to put it, which
   leaves it sitting low beside the word rather than level with it. */
QToolButton#searchSettings { padding: 4px 22px 4px 8px; }
QToolButton#searchSettings::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 5px; width: 12px; }
QToolButton#themeButton:hover, QToolButton#themeButton:pressed { border-color: @accent@; background: @base@; color: @text@; }
/* The schema panel's own menus, and the header's theme button beside them:
   the same trap again, where a style drops a menu arrow into the bottom-right
   corner unless it is told to put it level with the word.

   These are written as rules of their own rather than by adding selectors to
   the rules above. Qt drops a rule whole when any one declaration in it will
   not parse, so a rule that several buttons share is a rule that can take all
   of them down together. Written for the bar rather than for each button, so
   a menu added to that row later is level without anybody remembering this. */
QWidget#schemaBar QToolButton { border-color: @border@; background: @panel@; padding: 5px 22px 5px 9px; }
QWidget#schemaBar QToolButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 4px; width: 12px; }
QWidget#schemaBar QToolButton:hover, QWidget#schemaBar QToolButton:pressed { border-color: @accent@; background: @base@; color: @text@; }
QToolButton#schemaTheme { border-color: @border@; background: @panel@; padding: 5px 22px 5px 9px; }
QToolButton#schemaTheme::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 4px; width: 12px; }
QToolButton#schemaTheme:hover, QToolButton#schemaTheme:pressed { border-color: @accent@; background: @base@; color: @text@; }
/* The header of a schema drawn by hand (Zain, 2026-09-27): quiet controls of
   one height, with rounded corners and a hairline edge on the panel, in
   groups, and only the chosen design lit in the accent. Keyed on the header's
   schemaFirst property, or on containers only that header shows, so the
   header of a diagram is dressed exactly as before.

   The tools are written for their row, and the buttons that drop a menu by a
   property they carry, so a menu added to the row later has its arrow level
   with the word without anybody remembering. Connect's split arrow is drawn
   by the theme, as the diagram's tool row draws it, in rules kept apart for
   the reason given with those. */
QWidget#workspaceHeader[schemaFirst="true"] QPushButton#backToHome { background: @base@; color: @text@; border: 1px solid @border@; border-radius: 7px; padding: 5px 12px; min-height: 20px; }
QWidget#workspaceHeader[schemaFirst="true"] QPushButton#backToHome:hover { background: @hover@; border-color: @hoveredge@; }
QWidget#schemaModeSwitch { background: @base@; border: 1px solid @border@; border-radius: 8px; }
QWidget#schemaModeSwitch QPushButton { background: transparent; color: @text@; border: none; border-radius: 6px; padding: 4px 16px; min-height: 20px; }
QWidget#schemaModeSwitch QPushButton:hover { background: @hover@; }
QWidget#schemaModeSwitch QPushButton#schemaModeSchema:checked { background: @accent@; color: @selected@; font-weight: 600; }
QWidget#schemaModeSwitch QPushButton#previewConceptual:checked { background: @hover@; color: @accent@; }
QFrame#headerRule { background: @border@; border: none; }
QToolButton#renameDocument { background: transparent; border: 1px solid transparent; border-radius: 6px; padding: 4px; }
QToolButton#renameDocument:hover { background: @hover@; border-color: @hoveredge@; }
QWidget#schemaTopTools QToolButton { background: @base@; color: @text@; border: 1px solid @border@; border-radius: 7px; padding: 5px 10px; min-height: 20px; }
QWidget#schemaTopTools QToolButton:hover { background: @hover@; border-color: @hoveredge@; }
QWidget#schemaTopTools QToolButton:checked { background: @accent@; color: @selected@; border-color: @accent@; }
QWidget#schemaTopTools QToolButton[menuButton="true"] { padding: 5px 24px 5px 10px; }
QWidget#schemaTopTools QToolButton[menuButton="true"]::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 6px; width: 12px; }
QWidget#schemaTopTools QToolButton#schemaConnect { padding-right: 22px; }
QWidget#schemaTopTools QToolButton::menu-button { background: transparent; border: none; width: 18px; }
QWidget#schemaTopTools QToolButton::menu-button:hover { background: rgba(0, 0, 0, 13%); }
QWidget#schemaTopTools QToolButton::menu-button:pressed { background: rgba(0, 0, 0, 25%); }
QWidget#workspaceHeader[schemaFirst="true"] QToolButton#schemaUndo { background: transparent; border: 1px solid transparent; border-radius: 7px; padding: 5px 8px; min-height: 20px; }
QWidget#workspaceHeader[schemaFirst="true"] QToolButton#schemaRedo { background: transparent; border: 1px solid transparent; border-radius: 7px; padding: 5px 8px; min-height: 20px; }
QWidget#workspaceHeader[schemaFirst="true"] QToolButton#schemaUndo:hover { background: @hover@; border-color: @hoveredge@; }
QWidget#workspaceHeader[schemaFirst="true"] QToolButton#schemaRedo:hover { background: @hover@; border-color: @hoveredge@; }
QWidget#workspaceHeader[schemaFirst="true"] QLineEdit#schemaSearch { background: @base@; border: 1px solid @border@; border-radius: 7px; padding: 5px 8px; min-height: 20px; }
QWidget#workspaceHeader[schemaFirst="true"] QToolButton#schemaTheme { background: @base@; border-radius: 7px; padding: 5px 24px 5px 10px; min-height: 20px; }
/* Relational Design's Open example drops a menu, so its arrow is placed level
   with its mark, in rules of its own; dressed as the Theme button beside it,
   in either header. */
QToolButton#openRelationalExample { border-color: @border@; background: @panel@; padding: 5px 22px 5px 9px; }
QToolButton#openRelationalExample::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 4px; width: 12px; }
QToolButton#openRelationalExample:hover, QToolButton#openRelationalExample:pressed { border-color: @accent@; background: @base@; color: @text@; }
QWidget#workspaceHeader[schemaFirst="true"] QToolButton#openRelationalExample { background: @base@; border-radius: 7px; padding: 5px 24px 5px 10px; min-height: 20px; }
QToolBar#ribbonTabs { background: @panel@; border-bottom: 1px solid @border@; padding: 0px; spacing: 0px; }
QToolBar#ribbonTabs QToolButton { background: transparent; color: @text@; border: none; border-bottom: 2px solid transparent; border-radius: 0px; padding: 6px 13px 4px 13px; }
QToolBar#ribbonTabs QToolButton:hover { background: @hover@; color: @text@; border-bottom-color: @hoveredge@; }
QToolBar#ribbonTabs QToolButton:checked, QToolBar#ribbonTabs QToolButton:pressed { background: @window@; color: @accent@; border-bottom-color: @accent@; }
QToolBar#ribbonTabs QToolButton::menu-indicator { image: none; width: 0px; }
/* A tab colours itself when it is chosen, but the row it brings up looked the
   same whichever tab was showing: a strip of quiet text that never changed. The
   rows beneath the tabs are words rather than pictures, so they are set in the
   theme's own ink and carried a weight heavier than the interface around them.
   The row in front of you then reads as the thing you just chose.
   Chosen and unavailable entries are restated, because this rule is the more
   specific one and would otherwise take the colour off both. */
QToolBar[ribbonRow="true"] QToolButton { color: @text@; font-weight: 600; }
QToolBar[ribbonRow="true"] QToolButton:checked, QToolBar[ribbonRow="true"] QToolButton:pressed { color: @selected@; }
QToolBar[ribbonRow="true"] QToolButton:disabled { color: @muted@; font-weight: 500; }
QToolBar#designTools QToolButton { padding: 5px 22px 5px 9px; }
QToolBar#designTools QToolButton::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 4px; width: 12px; }
/* The Export row's buttons that open a menu -- More Formats -- place their
   arrow the same way, level with the name rather than in the button's
   corner. Only those are widened for it: an InstantPopup is mode 2. */
QToolBar#exportTools QToolButton[popupMode="2"] { padding: 5px 22px 5px 9px; }
QToolBar#exportTools QToolButton[popupMode="2"]::menu-indicator { subcontrol-origin: padding; subcontrol-position: center right; right: 4px; width: 12px; }
/* Specialization and Connect carry their choice on a split arrow rather than
   an InstantPopup button, so the arrow sits in its own menu-button section
   instead of the padding trick above. Left unstyled, that section is drawn
   from the platform's own defaults rather than the theme's, which is what
   painted it solid black on Windows.

   Telling it to paint nothing at all fixes that, but takes the split button
   with it: the section is what says the arrow opens a menu of its own rather
   than being part of the click beside it, and a button that gives no sign of
   being in two halves is a worse button on every platform than it was on the
   one it was broken on.

   So it is drawn by the theme instead, which is what keeps the platform out
   of it. Nothing at rest, so the button reads as one thing until it is
   reached for; its own surface under the pointer, whose edge is the seam;
   deeper again while it is held. The lift is a wash of black rather than a
   colour out of the palette, because it has to read over whatever is beneath
   it -- the toolbar on a light theme, the toolbar on a dark one, and the
   accent the button fills with once its tool is the chosen one, which no
   fixed colour reads over all three of. The arrow itself is left to the
   style, which centres it in the section on its own.

   The rule that paints nothing is kept to those two declarations, and every
   decoration is put in a rule of its own, because a stylesheet rule is
   dropped whole when any one of its declarations will not parse. Piling the
   decoration onto this rule is what brought the black back on Windows: a
   single declaration its Qt would not take cost the whole rule, and a
   menu-button with no rule at all is exactly the unstyled section that paints
   black there. Split up, the worst a rule its Qt dislikes can cost is its own
   effect -- a section that does not light up, never one that turns black.
   Anything added here later belongs in its own rule for the same reason. */
QToolBar#modelTools QToolButton::menu-button { background: transparent; border: none; }
QToolBar#modelTools QToolButton::menu-button:hover { background: rgba(0, 0, 0, 13%); }
QToolBar#modelTools QToolButton::menu-button:pressed { background: rgba(0, 0, 0, 25%); }
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
QWidget#canvasControls QToolButton { background: transparent; color: @text@; border: 1px solid transparent; border-radius: 5px; padding: 0px; font-size: 16px; font-weight: 700; }
QWidget#canvasControls QToolButton:hover { background: @hover@; border-color: @hoveredge@; }
QWidget#canvasControls QToolButton:checked { background: @accent@; color: @selected@; border-color: @accent@; }
QWidget#canvasControls QFrame#canvasControlsRule { background: @border@; border: none; }
QGraphicsView#diagramCanvas { background: @canvas@; border: 1px solid @border@; }
QGraphicsView#conceptualPreview { background: @canvas@; border: 1px solid @border@; }
QLineEdit, QComboBox, QDoubleSpinBox, QSpinBox, QPlainTextEdit { background: @base@; color: @text@; border: 1px solid @border@; border-radius: 2px; padding: 4px 6px; }
QLineEdit:focus, QComboBox:focus, QDoubleSpinBox:focus, QSpinBox:focus, QPlainTextEdit:focus { border-color: @accent@; }
QLineEdit:disabled, QComboBox:disabled, QDoubleSpinBox:disabled, QSpinBox:disabled, QPlainTextEdit:disabled { background: @panel@; color: @muted@; }
QComboBox { padding-right: 22px; }
QComboBox QAbstractItemView { background: @base@; color: @text@; border: 1px solid @border@; selection-background-color: @accent@; selection-color: @selected@; }
/* A row of a dropped-down list lights up under the pointer, the way a row of
   the Explorer and an entry of a menu already do. Without a rule of its own the
   list styles only the chosen row, and the rest give no sign that the pointer
   is on them. */
QComboBox QAbstractItemView::item { padding: 4px 6px; border: 1px solid transparent; }
QComboBox QAbstractItemView::item:hover { background: @hover@; color: @text@; border-color: @hoveredge@; }
QComboBox QAbstractItemView::item:selected { background: @accent@; color: @selected@; border-color: @accent@; }
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
QLabel#schemaPropertyName { color: @text@; font-size: 14px; font-weight: 700; }
QLabel#schemaPropertiesHeading { color: @accent@; font-size: 15px; font-weight: 800; }
QWidget#schemaTableProperties QLabel#propertyHeading { color: @muted@; font-size: 12px; font-weight: 600; padding: 0px; }
QFrame#schemaPropertySeparator { background: @border@; border: none; }
QFrame#schemaColumnRow { background: @base@; border: 1px solid @border@; border-radius: 5px; }
QFrame#schemaColumnRow:hover, QFrame#schemaColumnRow:focus { border-color: @accent@; }
QLabel#schemaColumnNumber { color: @muted@; font-size: 12px; }
QFrame#schemaColumnRow QLineEdit { padding: 3px 4px; font-size: 12px; }
QComboBox#schemaColumnType { padding: 3px 3px; padding-right: 12px; font-size: 12px; }
QToolButton#schemaColumnConstraints { background: @base@; color: @text@; border: 1px solid @border@; border-radius: 2px; padding: 0px; font-size: 12px; }
QToolButton#schemaColumnConstraints:hover, QToolButton#schemaColumnConstraints:focus { border-color: @accent@; }
QPushButton#schemaPropertiesAddColumn { padding: 4px 8px; text-align: left; }
QPushButton#schemaDerivedValue { color: @muted@; font-style: italic; text-align: left; }
QFrame#schemaFactCard { background: @base@; border: 1px solid @border@; border-radius: 7px; }
QFrame#schemaFactCard QLabel#schemaLowerFactTitle { color: @text@; font-size: 12px; font-weight: 600; }
QFrame#schemaFactCard QLabel#schemaPropertyValue { color: @muted@; font-size: 12px; }
QLabel#schemaFactCount { background: @hover@; color: @muted@; border: none; border-radius: 8px; padding: 1px 6px; font-size: 11px; }
QFrame#schemaColumnIdentity { background: @base@; border: 1px solid @border@; border-radius: 8px; }
QFrame#schemaColumnIdentity QLabel#propertyHeading { padding-bottom: 0px; }
QLabel#schemaColumnIdentityName { color: @text@; font-size: 14px; font-weight: 700; }
QLabel#schemaColumnIdentityPath { color: @muted@; font-size: 12px; }
QFrame#schemaReadOnlyBox { background: @panel@; border: 1px solid @border@; border-radius: 4px; }
QFrame#schemaReadOnlyBox QLabel#schemaPropertyValue { color: @text@; }
QFrame#schemaRelationshipIdentity { background: @base@; border: 1px solid @border@; border-radius: 8px; }
QFrame#schemaRelationshipIdentity QLabel#propertyHeading { padding-bottom: 0px; }
QLabel#schemaRelationshipIdentityEnds { color: @text@; font-size: 13px; font-weight: 700; }
QLabel#schemaRelationshipIdentityKind { color: @muted@; font-size: 12px; }
QFrame#schemaRelationshipFacts, QFrame#schemaRelationshipEnd { background: @base@; border: 1px solid @border@; border-radius: 7px; }
QLabel#schemaRelationshipFactLabel { color: @muted@; font-size: 12px; }
QFrame#schemaRelationshipFacts QLabel#schemaPropertyValue { color: @text@; font-size: 12px; font-weight: 600; }
QLabel#schemaRelationshipTable { color: @text@; font-size: 12px; font-weight: 700; }
QLabel#schemaRelationshipRole { font-size: 11px; }
QLabel#schemaRelationshipEndRole { color: @muted@; font-size: 11px; font-weight: 600; }
QFrame#schemaRelationshipEnd QLabel#schemaPropertyValue { color: @text@; font-size: 12px; font-weight: 600; }
QFrame#schemaConstraintList { background: @base@; border: 1px solid @border@; border-radius: 7px; }
QFrame#schemaConstraintDivider { background: @border@; border: none; }
QLabel#schemaConstraintTitle { color: @text@; font-size: 12px; font-weight: 700; }
QLabel#schemaConstraintNote { color: @muted@; font-size: 11px; }
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

const QString& style_sheet(const Theme& colors) {
    static std::array<QString, theme_count> cache;
    auto& slot = cache[static_cast<std::size_t>(colors.id)];
    if (slot.isEmpty()) slot = build_style_sheet(colors);
    return slot;
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

QColor note_surface(const Theme& colors) { return mix(colors.canvas, colors.warning, 0.22); }
QColor over(const QColor& surface, const QColor& paint) { return mix(surface, paint, paint.alphaF()); }

QColor readable_on(const QColor& surface) {
    return relative_luminance(surface) > 0.36 ? QColor(0x1a, 0x1a, 0x1a) : QColor(0xff, 0xff, 0xff);
}

bool colourless(ThemeId id) { return id == ThemeId::Plain; }

QColor greyed(const QColor& colour) {
    const auto level = qGray(colour.rgb());
    return QColor(level, level, level, colour.alpha());
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

namespace {
// A colour carried part of the way towards another. The whole derivation is
// built out of this: a soft primary is the accent carried most of the way to
// the surface, a medium border is a soft one carried a little towards the ink.
QColor towards(const QColor& from, const QColor& to, double part) {
    const auto mix = [part](int a, int b) {
        return static_cast<int>(std::lround(a + (b - a) * part));
    };
    return QColor(mix(from.red(), to.red()), mix(from.green(), to.green()),
                  mix(from.blue(), to.blue()));
}

// What ERDFlow Azure says about itself.
//
// The one theme that states its whole semantic set rather than deriving it,
// because it is the one whose exact appearance is specified. Every value here
// is from the canonical token file.
Tokens azure_tokens() {
    Tokens t;
    t.primary = QColor("#1E88E5");
    t.primary_hover = QColor("#1976D2");
    t.primary_pressed = QColor("#1565C0");
    t.primary_soft = QColor("#EAF4FF");
    t.primary_faint = QColor("#F4F9FF");
    t.window_background = QColor("#F6F9FC");
    t.surface = QColor("#FFFFFF");
    t.sidebar_surface = QColor("#F8FBFE");
    t.learning_surface = QColor("#F7FBFF");
    t.border_soft = QColor("#DCE6F2");
    t.border_medium = QColor("#CBD5E1");
    t.text_primary = QColor("#0F172A");
    t.text_heading = QColor("#0B1F44");
    t.text_secondary = QColor("#475569");
    t.text_muted = QColor("#64748B");
    t.text_disabled = QColor("#94A3B8");
    t.gold = QColor("#F4B740");
    t.gold_soft = QColor("#FFF4D8");
    t.lavender = QColor("#8B7CF6");
    t.green = QColor("#22C55E");
    t.amber = QColor("#F59E0B");
    t.red = QColor("#EF4444");
    t.hover_surface = QColor("#EEF6FF");
    t.selected_card_surface = QColor("#FAFDFF");
    t.shadow_card = Shadow{0, 8, 24, QColor(15, 23, 42, 15)};
    t.shadow_card_hover = Shadow{0, 10, 28, QColor(30, 136, 229, 26)};
    t.shadow_primary_button = Shadow{0, 4, 12, QColor(30, 136, 229, 46)};
    t.shadow_selected_nav = Shadow{0, 6, 14, QColor(30, 136, 229, 56)};
    return t;
}

// What every other theme resolves to.
//
// Worked out from the handful of colours a Theme already carries, so nineteen
// themes that were right before this are still right after it and none of them
// had to be edited. A derived value will not always be as considered as a
// chosen one -- that is the trade -- but it is always in the theme's own
// family, which is what matters.
Tokens derived_tokens(const Theme& colors) {
    const bool dark = colors.base.lightness() < 128;
    const auto& ink = colors.text;
    Tokens t;
    t.primary = colors.accent;
    // Hover and pressed move the accent away from its own lightness, rather
    // than simply darkening it. Darkening a black accent leaves it black, so a
    // greyscale theme would have had a hover indistinguishable from a press;
    // moving away from where the accent already is always shows.
    const auto away = colors.accent.lightness() < 128 ? QColor(Qt::white) : QColor(Qt::black);
    t.primary_hover = towards(colors.accent, away, 0.13);
    t.primary_pressed = towards(colors.accent, away, 0.28);
    t.primary_soft = towards(colors.base, colors.accent, dark ? 0.22 : 0.12);
    t.primary_faint = towards(colors.base, colors.accent, dark ? 0.11 : 0.05);
    t.window_background = colors.window;
    t.surface = colors.base;
    t.sidebar_surface = colors.panel;
    t.learning_surface = towards(colors.panel, colors.base, 0.35);
    t.border_soft = colors.border;
    t.border_medium = towards(colors.border, ink, 0.22);
    t.text_primary = ink;
    // A heading is the ink taken a little further from the surface it sits on,
    // which reads as weight without inventing a colour the theme never chose.
    t.text_heading = towards(ink, dark ? QColor(Qt::white) : QColor(Qt::black), 0.18);
    t.text_secondary = colors.muted;
    t.text_muted = towards(colors.muted, colors.base, 0.18);
    t.text_disabled = towards(colors.muted, colors.base, 0.42);
    // Every palette already carries an amber, a green and a red for validity,
    // so the accents are those rather than colours from nowhere.
    t.gold = colors.warning;
    t.gold_soft = towards(colors.base, colors.warning, dark ? 0.26 : 0.16);
    // Lavender is the one accent no theme carries. Taken as the accent turned
    // through the wheel, so it is a relative of the theme rather than a stranger.
    t.lavender = QColor::fromHsv((colors.accent.hue() < 0 ? 250 : (colors.accent.hue() + 268) % 360),
                                 std::max(90, colors.accent.saturation()),
                                 dark ? 205 : 170);
    t.green = colors.valid;
    t.amber = colors.warning;
    t.red = colors.error;
    t.hover_surface = hover_surface(colors);
    t.selected_card_surface = towards(colors.base, colors.accent, dark ? 0.14 : 0.05);
    // A shadow in the theme's own ink, and lighter where the surface is dark,
    // because a black shadow on a dark panel is a smudge rather than a lift.
    const auto shade = [dark](int alpha) {
        return dark ? QColor(0, 0, 0, alpha + 30) : QColor(15, 23, 42, alpha);
    };
    t.shadow_card = Shadow{0, 8, 24, shade(15)};
    t.shadow_card_hover = Shadow{0, 10, 28, shade(26)};
    t.shadow_primary_button = Shadow{0, 4, 12, shade(30)};
    t.shadow_selected_nav = Shadow{0, 6, 14, shade(40)};
    return t;
}
// The same tokens with every hue taken out, for a theme that shows no colour.
// A derived theme's tokens are mostly its own greys already; the ones that are
// not are the lavender, turned through the colour wheel from the accent, and
// the faint blue cast in the shadows.
Tokens without_colour(Tokens t) {
    for (auto* colour : {&t.primary, &t.primary_hover, &t.primary_pressed, &t.primary_soft,
                         &t.primary_faint, &t.window_background, &t.surface, &t.sidebar_surface,
                         &t.learning_surface, &t.border_soft, &t.border_medium, &t.text_primary,
                         &t.text_heading, &t.text_secondary, &t.text_muted, &t.text_disabled,
                         &t.gold, &t.gold_soft, &t.lavender, &t.green, &t.amber, &t.red,
                         &t.hover_surface, &t.selected_card_surface, &t.shadow_card.ink,
                         &t.shadow_card_hover.ink, &t.shadow_primary_button.ink,
                         &t.shadow_selected_nav.ink})
        *colour = greyed(*colour);
    return t;
}
} // namespace

const Tokens& tokens(ThemeId id) {
    // Resolved once for each theme and kept. Derivation is cheap but it is not
    // free, and a hover surface worked out again on every repaint is a cost
    // paid for nothing.
    static std::array<std::optional<Tokens>, theme_count> resolved;
    const auto at = static_cast<std::size_t>(id);
    if (at >= resolved.size()) return *resolved.front();
    if (!resolved[at]) {
        auto made = id == ThemeId::Azure ? azure_tokens() : derived_tokens(theme(id));
        // The face and the type sizes are the same question for every theme:
        // they are about legibility rather than about a palette. A theme that
        // wants its own may state them, as Azure could.
        made.family = QStringList{"Inter", "Segoe UI", "SF Pro Text", "Helvetica Neue", "Arial"};
        if (colourless(id)) made = without_colour(made);
        resolved[at] = std::move(made);
    }
    return *resolved[at];
}

QString default_theme_key() { return QStringLiteral("erdflow.azure"); }

void apply_theme(QApplication& app, ThemeId id) {
    // Resolved here, once, so nothing downstream has to work it out again.
    (void)tokens(id);
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
