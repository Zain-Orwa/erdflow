#include "app/desktop/theme.hpp"

#include <QApplication>
#include <QLineEdit>
#include <QPalette>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {
using namespace erdflow::desktop;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

double linear_channel(double value) {
    return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
}

double luminance(const QColor& color) {
    return 0.2126 * linear_channel(color.redF()) +
           0.7152 * linear_channel(color.greenF()) +
           0.0722 * linear_channel(color.blueF());
}

double contrast(const QColor& first, const QColor& second) {
    const auto first_luminance = luminance(first);
    const auto second_luminance = luminance(second);
    return (std::max(first_luminance, second_luminance) + 0.05) /
           (std::min(first_luminance, second_luminance) + 0.05);
}

void require_contrast(const Theme& candidate, const char* use, const QColor& foreground,
                      const QColor& background, double minimum) {
    const auto ratio = contrast(foreground, background);
    require(foreground.isValid() && foreground.alpha() == 255 &&
            background.isValid() && background.alpha() == 255,
            candidate.key.toStdString() + ": invalid or translucent color for " + use);
    require(ratio >= minimum, candidate.key.toStdString() + ": " + use +
            " contrast is " + std::to_string(ratio) + ", expected at least " +
            std::to_string(minimum));
}

void identity_tests() {
    require(themes().size() == 6, "Six appearance choices must be available");
    std::set<QString> keys;
    std::set<QString> labels;
    std::set<ThemeId> ids;
    for (const auto& candidate : themes()) {
        require(!candidate.key.trimmed().isEmpty(), "Every theme needs a persistent key");
        require(!candidate.label.trimmed().isEmpty(), "Every theme needs a visible name");
        require(keys.insert(candidate.key).second, "Theme keys must be unique");
        require(labels.insert(candidate.label).second, "Theme labels must be unique");
        require(ids.insert(candidate.id).second, "Theme IDs must be unique");
        require(theme_from_key(candidate.key) == candidate.id, "Theme keys must round-trip");
        require(theme(candidate.id).key == candidate.key, "Theme IDs must look up their definition");
    }
    require(theme_from_key(QString{}) == ThemeId::OfficeLight, "Missing preference falls back to Office Light");
    require(theme_from_key("removed-or-invalid-theme") == ThemeId::OfficeLight,
            "An obsolete preference falls back to Office Light");
}

void contrast_tests() {
    for (const auto& candidate : themes()) {
        require_contrast(candidate, "window text", candidate.text, candidate.window, 4.5);
        require_contrast(candidate, "panel text", candidate.text, candidate.panel, 4.5);
        require_contrast(candidate, "field text", candidate.text, candidate.base, 4.5);
        require_contrast(candidate, "entity label", candidate.node_text, candidate.entity_fill, 4.5);
        require_contrast(candidate, "attribute label", candidate.node_text, candidate.attribute_fill, 4.5);
        require_contrast(candidate, "relationship label", candidate.node_text, candidate.relationship_fill, 4.5);
        require_contrast(candidate, "selection label", candidate.selected_text, candidate.accent, 4.5);
        require_contrast(candidate, "connector on canvas", candidate.connector, candidate.canvas, 3.0);
    }
}

void live_palette_tests(QApplication& app) {
    QLineEdit field("An existing property field");
    field.show();
    QColor previous_window;
    for (const auto& candidate : themes()) {
        apply_theme(app, candidate.id);
        QApplication::processEvents();
        const auto palette = app.palette();
        for (const auto group : {QPalette::Active, QPalette::Inactive}) {
            require(palette.color(group, QPalette::Window) == candidate.window, "Window palette follows theme");
            require(palette.color(group, QPalette::WindowText) == candidate.text, "Window text follows theme");
            require(palette.color(group, QPalette::Base) == candidate.base, "Field palette follows theme");
            require(palette.color(group, QPalette::Text) == candidate.text, "Field text follows theme");
            require(palette.color(group, QPalette::Highlight) == candidate.accent, "Selection follows theme");
            require(palette.color(group, QPalette::HighlightedText) == candidate.selected_text,
                    "Selected text follows theme");
            require_contrast(candidate, "tooltip text", palette.color(group, QPalette::ToolTipText),
                             palette.color(group, QPalette::ToolTipBase), 4.5);
        }
        require(field.palette().color(QPalette::Base) == candidate.base,
                "An existing field receives the new background without rebuilding");
        require(field.palette().color(QPalette::Text) == candidate.text,
                "An existing field receives the new text color without rebuilding");
        require(!previous_window.isValid() || previous_window != palette.color(QPalette::Window),
                "Switching themes visibly changes the window palette");
        previous_window = palette.color(QPalette::Window);
    }
    apply_theme(app, ThemeId::OfficeLight);
    QApplication::processEvents();
    require(field.text() == "An existing property field", "Theme switching preserves an active field's contents");
    require(app.palette().color(QPalette::Window) == theme(ThemeId::OfficeLight).window,
            "Switching back restores Office Light");
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");
    try {
        identity_tests();
        contrast_tests();
        live_palette_tests(app);
        std::cout << "Theme identity, contrast, and live palette tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Theme test failed: " << error.what() << '\n';
        return 1;
    }
}
