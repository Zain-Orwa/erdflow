#pragma once

#include <QColor>
#include <QString>
#include <array>

class QApplication;

namespace erdflow::desktop {

enum class ThemeId {
    // ERDFlow's own set leads: Office Light is the default appearance, and
    // these are the only light themes and the only high-contrast one.
    OfficeLight, WarmLight, Graphite, Midnight, DraculaClassic, HighContrast,
    // The imported palette families, all dark.
    Forest, Mono, Dracula, OneDarkPro, TokyoNight, CatppuccinMocha,
    Gruvbox, Solarized, GitHubDark, MaterialOcean, Nord, Monokai
};

inline constexpr std::size_t theme_count = 18;

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

const std::array<Theme, theme_count>& themes();
const Theme& theme(ThemeId id);
ThemeId theme_from_key(const QString& key);
void apply_theme(QApplication& app, ThemeId id);

} // namespace erdflow::desktop
