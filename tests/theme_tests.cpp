// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
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

// ERDFlow Azure, and the resolver that keeps every other theme working
// without having been edited.
void azure_and_token_tests() {
    // Azure exists, is named as the specification names it, and is what a
    // fresh profile is given.
    const auto& azure = theme(ThemeId::Azure);
    require(azure.key == "erdflow.azure", "Azure carries its specified id");
    require(azure.label == "ERDFlow Azure", "And its specified display name");
    require(default_theme_key() == azure.key, "A fresh profile is given Azure");
    require(theme_from_key(default_theme_key()) == ThemeId::Azure,
            "And that key resolves back to it");

    // The nineteen that were here before are all still here, still selectable,
    // and still carry the keys anybody's settings may already name.
    for (const auto* key : {"office-light", "warm-light", "plain", "graphite", "midnight",
                            "dracula-classic", "high-contrast", "forest", "mono", "dracula",
                            "one-dark-pro", "tokyo-night", "catppuccin-mocha", "gruvbox",
                            "solarized", "github-dark", "material-ocean", "nord", "monokai"}) {
        const auto found = std::find_if(themes().begin(), themes().end(),
                                        [&](const Theme& one) { return one.key == key; });
        require(found != themes().end(), std::string("Theme still offered: ") + key);
    }
    require(themes().size() == theme_count, "Twenty themes, the nineteen and Azure");

    // Azure states its tokens outright; these are the canonical values.
    const auto& a = tokens(ThemeId::Azure);
    require(a.primary == QColor("#1E88E5"), "Azure's primary is the specified blue");
    require(a.primary_hover == QColor("#1976D2"), "And its hover");
    require(a.primary_pressed == QColor("#1565C0"), "And its pressed");
    require(a.hover_surface == QColor("#EEF6FF"), "And its hover surface");
    require(a.selected_card_surface == QColor("#FAFDFF"), "And its selected card");
    require(a.gold == QColor("#F4B740") && a.lavender == QColor("#8B7CF6"),
            "And both small accents");
    require(a.text_heading == QColor("#0B1F44"), "And its heading ink");
    require(a.radius_large_card == 12 && a.radius_nav_item == 9 && a.radius_input == 8,
            "And the specified radii");
    require(a.hero_title.size == 38 && a.hero_title.weight == 800, "And the hero type");

    // Every other theme resolves to a complete set without having been edited.
    for (const auto& one : themes()) {
        if (one.id == ThemeId::Azure) continue;
        const auto& t = tokens(one.id);
        const auto named = one.key.toStdString();
        require(t.primary == one.accent, named + ": primary is the theme's own accent");
        require(t.surface == one.base, named + ": surface is the theme's own base");
        require(t.text_primary == one.text, named + ": ink is the theme's own");
        require(t.green == one.valid && t.red == one.error,
                named + ": validity colours are the theme's own");
        // Derived values must be real colours in the theme's family, not
        // defaults left at nothing.
        for (const auto& derived : {t.primary_hover, t.primary_pressed, t.primary_soft,
                                    t.border_medium, t.text_heading, t.text_disabled,
                                    t.lavender, t.gold_soft, t.selected_card_surface})
            require(derived.isValid(), named + ": every token resolves to a colour");
        require(t.primary_hover != t.primary_pressed,
                named + ": hover and pressed are told apart");
        require(t.radius_large_card == 12, named + ": radii are shared");
        require(!t.family.isEmpty(), named + ": a face is named");
    }

    // Resolved once and kept, rather than worked out again on every ask.
    require(&tokens(ThemeId::Azure) == &tokens(ThemeId::Azure), "Tokens are resolved once");
    require(&tokens(ThemeId::Nord) == &tokens(ThemeId::Nord), "For every theme, not only Azure");
}

void identity_tests() {
    require(themes().size() == theme_count, "Every palette family must be offered");
    require(themes().front().id == ThemeId::Azure,
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

    // Nothing the interface is dressed in brings a hue back either: the tokens
    // derived from Plain are greys too, the lavender and the shadows included.
    const auto& dressed = tokens(ThemeId::Plain);
    for (const auto& colour : {dressed.primary, dressed.primary_soft, dressed.primary_faint, dressed.gold,
                               dressed.gold_soft, dressed.lavender, dressed.green, dressed.amber, dressed.red,
                               dressed.hover_surface, dressed.selected_card_surface, dressed.learning_surface,
                               dressed.shadow_card.ink, dressed.shadow_card_hover.ink,
                               dressed.shadow_primary_button.ink, dressed.shadow_selected_nav.ink})
        require(colour.red() == colour.green() && colour.green() == colour.blue(),
                "Every token Plain resolves to is a grey");
    require(colourless(ThemeId::Plain), "Plain is the theme that shows no colour");
    for (const auto& other : themes())
        if (other.id != ThemeId::Plain)
            require(!colourless(other.id), "And no other theme is made colourless by it");
    const auto faded = greyed(QColor(59, 130, 246, 90));
    require(faded.red() == faded.green() && faded.green() == faded.blue() && faded.alpha() == 90
                && faded.red() == qGray(59, 130, 246),
            "A colour greyed keeps its brightness and its opacity");
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
        azure_and_token_tests();
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
