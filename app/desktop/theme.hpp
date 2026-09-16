#pragma once

#include <QColor>
#include <QString>
#include <array>

class QApplication;

namespace erdflow::desktop {

enum class ThemeId {
    // ERDFlow's own set leads: Office Light is the default appearance, and
    // these are the only light themes and the only high-contrast one.
    OfficeLight, WarmLight, Plain, Graphite, Midnight, DraculaClassic, HighContrast,
    // The imported palette families, all dark.
    Forest, Mono, Dracula, OneDarkPro, TokyoNight, CatppuccinMocha,
    Gruvbox, Solarized, GitHubDark, MaterialOcean, Nord, Monokai
};

inline constexpr std::size_t theme_count = 19;

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

// Black or white, whichever the eye can actually read on a given surface. The
// threshold is on relative luminance rather than on plain brightness, so a
// saturated yellow is treated as the light colour it is. Anything that draws a
// label over a colour the theme did not choose needs this.
[[nodiscard]] QColor readable_on(const QColor& surface);

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
