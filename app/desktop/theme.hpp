#pragma once

#include <QColor>
#include <QString>
#include <array>

class QApplication;

namespace erdflow::desktop {

enum class ThemeId { OfficeLight, WarmLight, Graphite, Midnight, Dracula, HighContrast };

struct Theme {
    ThemeId id;
    QString key;
    QString label;
    QColor window, panel, base, text, muted, border, accent, selected_text;
    QColor canvas, grid, entity_fill, entity_border, attribute_fill, attribute_border;
    QColor relationship_fill, relationship_border, node_text, connector;
};

const std::array<Theme, 6>& themes();
const Theme& theme(ThemeId id);
ThemeId theme_from_key(const QString& key);
void apply_theme(QApplication& app, ThemeId id);

} // namespace erdflow::desktop
