#include "app/desktop/theme.hpp"

#include <QApplication>
#include <QLineEdit>
#include <QPalette>
#include <QStringList>

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
    require(themes().size() == theme_count, "Every palette family must be offered");
    require(themes().front().id == ThemeId::OfficeLight,
            "The default appearance is offered first");
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
    require(theme_from_key(QString{}) == ThemeId::OfficeLight, "Missing preference falls back to Normal");
    require(theme_from_key("removed-or-invalid-theme") == ThemeId::OfficeLight,
            "An obsolete preference falls back to Normal");
}

// Plain is defined by the absence of hue: it tells its shapes apart by how
// grey they are, so that it survives a photocopier and does not ask anyone to
// rely on colour. A stray tint in one of its surfaces would be invisible in
// review but would break exactly that.
void plain_theme_tests() {
    const auto& plain = theme(ThemeId::Plain);
    for (const auto& [use, colour] : {std::pair{"canvas", plain.canvas}, std::pair{"entity", plain.entity_fill},
                                      std::pair{"attribute", plain.attribute_fill},
                                      std::pair{"relationship", plain.relationship_fill},
                                      std::pair{"isa", plain.isa_fill}, std::pair{"connector", plain.connector},
                                      std::pair{"node text", plain.node_text}, std::pair{"window", plain.window}})
        require(colour.red() == colour.green() && colour.green() == colour.blue(),
                std::string("Plain draws its ") + use + " without a tint");
    // And the shapes are still told apart, or being grey costs the reader the
    // distinction it was meant to preserve.
    require(plain.entity_fill != plain.attribute_fill && plain.attribute_fill != plain.relationship_fill
                && plain.entity_fill != plain.relationship_fill,
            "Plain gives each kind of shape its own level of grey");
    require(plain.canvas.red() > plain.entity_fill.red(), "And the board is lighter than what sits on it");
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
        require_contrast(candidate, "isa label", candidate.node_text, candidate.isa_fill, 4.5);
        require_contrast(candidate, "connector on canvas", candidate.connector, candidate.canvas, 3.0);
        // A row under the pointer is still a row to be read, and the tint must
        // stay clear of the accent itself or hovering would look like selecting.
        require_contrast(candidate, "text on a hovered row", candidate.text, hover_surface(candidate), 4.5);
        require_contrast(candidate, "hovered row against the panel", hover_surface(candidate), candidate.panel, 1.03);
        require(hover_surface(candidate) != candidate.accent,
                candidate.key.toStdString() + ": a hovered row must not wear the selection colour");
        // The tint has to be visible, or hovering says nothing at all.
        require(hover_surface(candidate) != candidate.panel,
                candidate.key.toStdString() + ": a hovered row must differ from an unhovered one");
        // A rule's verdict is read off the canvas, so its colour has to carry
        // there as well as any element does.
        for (const auto& [use, colour] : {std::pair{"valid", candidate.valid},
                                          std::pair{"warning", candidate.warning},
                                          std::pair{"error", candidate.error}})
            require_contrast(candidate, use, colour, candidate.canvas, 3.0);
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
            "Switching back restores Normal");
}

// Specialization and Connect carry their choice on a split arrow, and the
// section that arrow sits in is drawn from the platform's own defaults
// whenever it has no rule of its own -- which paints it solid black on
// Windows. Qt drops a stylesheet rule whole when any one of its declarations
// will not parse, so piling decoration onto the rule that keeps that section
// quiet is what brings the black back: one declaration a platform's Qt
// dislikes costs the entire rule, and an unstyled menu-button is the bug.
// The resting rule is therefore held to the two declarations it needs, and
// anything else belongs in a rule of its own, where the worst it can cost is
// its own effect.
void split_arrow_tests(QApplication& app) {
    const QString selector = "QToolBar#modelTools QToolButton::menu-button";
    for (const auto& candidate : themes()) {
        apply_theme(app, candidate.id);
        bool found = false;
        for (const QString& line : app.styleSheet().split('\n')) {
            const QString rule = line.trimmed();
            if (!rule.startsWith(selector + " {")) continue;
            found = true;
            const auto open = rule.indexOf('{');
            const auto close = rule.lastIndexOf('}');
            require(open >= 0 && close > open, "The resting menu-button rule needs a body");
            int declarations = 0;
            for (const QString& part : rule.mid(open + 1, close - open - 1).split(';'))
                if (!part.trimmed().isEmpty()) ++declarations;
            require(declarations <= 2, candidate.key.toStdString() +
                    ": the resting menu-button rule must stay at the two declarations that keep "
                    "the section quiet, so a declaration one platform's Qt will not parse cannot "
                    "drop the rule and let that platform paint the section black");
        }
        require(found, candidate.key.toStdString() +
                ": the split arrow's section needs a rule of its own, or the platform draws it");
    }
    apply_theme(app, ThemeId::OfficeLight);
}
} // namespace

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setStyle("Fusion");
    try {
        identity_tests();
        plain_theme_tests();
        contrast_tests();
        live_palette_tests(app);
        split_arrow_tests(app);
        std::cout << "Theme identity, contrast, live palette, and split arrow tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "Theme test failed: " << error.what() << '\n';
        return 1;
    }
}
