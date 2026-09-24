// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include <QColor>
#include <QString>
#include <QStringList>
#include <array>

class QApplication;

namespace erdflow::desktop {

enum class ThemeId {
    // ERDFlow's own set leads: Office Light is the default appearance, and
    // these are the only light themes and the only high-contrast one.
    // ERDFlow Azure leads: it is the theme a fresh profile meets, and the only
    // one that states its whole semantic token set rather than deriving it.
    Azure,
    OfficeLight, WarmLight, Plain, Graphite, Midnight, DraculaClassic, HighContrast,
    // The imported palette families, all dark.
    Forest, Mono, Dracula, OneDarkPro, TokyoNight, CatppuccinMocha,
    Gruvbox, Solarized, GitHubDark, MaterialOcean, Nord, Monokai
};

inline constexpr std::size_t theme_count = 20;

struct Theme {
    ThemeId id;
    QString key;
    QString label;
    QColor window, panel, base, text, muted, border, accent, selected_text;
    QColor canvas, grid, entity_fill, entity_border, attribute_fill, attribute_border;
    QColor relationship_fill, relationship_border, isa_fill, isa_border;
    QColor node_text, connector, selection;
    QColor valid, warning, error;
};

// The semantic surface a shadow is drawn as. Qt wants a colour and three
// numbers where CSS wants a string, so a shadow is kept as what it is.
struct Shadow {
    double dx = 0;
    double dy = 0;
    double blur = 0;
    QColor ink;
};

// How large a piece of text is, and how heavy.
struct TextStyle {
    double size = 14;
    int weight = 400;
};

// Everything the interface needs to know about a theme, beyond the two dozen
// colours a Theme states.
//
// A Theme says what an entity is filled with and what the canvas is; these say
// what a hovered row is, what a pressed button is, what a heading weighs. The
// two are kept apart because they are answered differently: ERDFlow Azure
// states all of this outright, and every other theme has it *derived* from the
// handful of colours it already carries.
//
// That is the whole point of the arrangement. Widening Theme itself would have
// meant giving nineteen working themes values for two dozen new fields, which
// is maintenance with no benefit and a real risk of spoiling themes that are
// already right. A derived token is rarely as good as a chosen one, so a theme
// that wants exact control may state its own later without a migration.
//
// Resolved once, when a theme is applied. Never in a paint path.
struct Tokens {
    QColor primary, primary_hover, primary_pressed, primary_soft, primary_faint;
    QColor window_background, surface, sidebar_surface, learning_surface;
    QColor border_soft, border_medium;
    QColor text_primary, text_heading, text_secondary, text_muted, text_disabled;
    QColor gold, gold_soft, lavender, green, amber, red;
    QColor hover_surface, selected_card_surface;

    double radius_large_card = 12;
    double radius_panel = 12;
    double radius_nav_item = 9;
    double radius_button = 10;
    double radius_input = 8;

    Shadow shadow_card;
    Shadow shadow_card_hover;
    Shadow shadow_primary_button;
    Shadow shadow_selected_nav;

    QStringList family;   // preferred first, native fallback after
    TextStyle hero_title{38, 800};
    TextStyle hero_subtitle{23, 400};
    TextStyle section_title{24, 700};
    TextStyle card_title{17, 700};
    TextStyle body{14, 400};
    TextStyle nav{15, 500};
};

// The tokens a theme resolves to, worked out once and kept.
//
// Ask for these rather than deriving anything at the point of painting: the
// derivation is cheap but it is not free, and a hover surface recomputed on
// every repaint is a cost paid for nothing.
[[nodiscard]] const Tokens& tokens(ThemeId id);

// What a fresh profile is given. A profile that has already chosen keeps its
// choice; this is only what somebody meets who has never chosen at all.
[[nodiscard]] QString default_theme_key();

// Black or white, whichever the eye can actually read on a given surface. The
// threshold is on relative luminance rather than on plain brightness, so a
// saturated yellow is treated as the light colour it is. Anything that draws a
// label over a colour the theme did not choose needs this.
[[nodiscard]] QColor readable_on(const QColor& surface);

// Whether a theme shows no colour of its own. Under it, everything the
// application inks -- icons of every set, drawings, decoration, the lines and
// marks it adds -- is grey. A colour somebody gave one of their own elements is
// theirs, and is kept (Zain, 2026-09-24). Plain is the theme that asks for it.
[[nodiscard]] bool colourless(ThemeId id);

// The same colour with its hue taken out: the grey of the same brightness, at
// the same opacity. What a colourless theme shows wherever something would
// otherwise bring a colour of its own.
[[nodiscard]] QColor greyed(const QColor& colour);

// The surface a row or card takes while the pointer is over it: the panel
// carried a little way towards the theme's accent. It is derived rather than
// chosen so that every theme gets one without having to name it, and it is a
// tint rather than the accent itself so that hovering something never looks
// like having selected it.
[[nodiscard]] QColor hover_surface(const Theme& colors);

// The surface a note is drawn on: the canvas carried a little way towards the
// theme's warning colour, which is the amber every palette has, so a note
// reads as the slip it stands for without a colour of its own in the table.
// Its text is chosen against this surface, as it is for a chosen colour.
[[nodiscard]] QColor note_surface(const Theme& colors);

// A paint laid over a surface by its own alpha: what the eye sees where a
// see-through colour is drawn on the canvas, and so what a label over it, or
// a panel matching it, has to be chosen against.
[[nodiscard]] QColor over(const QColor& surface, const QColor& paint);


const std::array<Theme, theme_count>& themes();
const Theme& theme(ThemeId id);
ThemeId theme_from_key(const QString& key);
void apply_theme(QApplication& app, ThemeId id);

} // namespace erdflow::desktop
