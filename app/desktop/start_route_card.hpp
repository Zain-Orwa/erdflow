// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "app/desktop/theme.hpp"

#include <QAbstractButton>
#include <QFont>
#include <QPushButton>
#include <QRectF>
#include <QString>

#include <functional>
#include <vector>

namespace erdflow::desktop {

// The three ways a project can begin, as cards.
//
// The specification drew five, with From Template / Example and Import
// Existing beside these three. Zain settled on 2026-09-23 that those two are
// not cards: they are places to go rather than ways of modelling, and they
// live in the sidebar's Templates and Import rows (ADR-022 section 9.14).
// Neither is to be put back here because a picture shows it.
class HomeLiveDemo;

enum class StartRoute { Conceptual, RelationalDesign, Sql };

// What a card says, and whether it may yet be taken.
//
// `Relational Schema` and `SQL Project` are here from the first
// day the screen exists and are deliberately not enabled: the route behind the first
// needs relations made by hand, which ADR-021 calls Step B, and the route
// behind the second needs SQL parsed into Relational Design and back
// (ADR-022 sections 5 and 9.2). Showing where the product is going is worth
// doing; promising a route that cannot finish is not.
struct StartRouteDefinition {
    StartRoute route;
    const char* object_name;
    const char* title;
    const char* body;
    const char* helper;
    bool enabled;
    // Said on the card itself where a route cannot yet be taken, so nobody has
    // to hover to find out why it will not answer.
    const char* badge;
    // Why it cannot yet be taken, in a sentence, for the tooltip and for
    // whatever reads the interface aloud.
    const char* reason;
};

[[nodiscard]] const std::vector<StartRouteDefinition>& start_routes();

// One card. A real button rather than a painted rectangle, so it can be
// reached by tab, taken by space or return, and read out by whatever the
// platform reads interfaces with -- none of which a painted rectangle can do,
// and all of which the specification asks for.
//
// A card holding a live demo: title, reserved badge row, the demo centred,
// + Create. All three hold theirs on Home; a card given none keeps its own
// drawing and description. Pressing the card chooses it; only the button
// creates.
class StartRouteCard final : public QAbstractButton {
public:
    StartRouteCard(const StartRouteDefinition& what, QWidget* parent);

    void wear(ThemeId id);
    void set_live_demo(HomeLiveDemo* demo);
    [[nodiscard]] StartRoute route() const { return what_.route; }
    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;
    // How tall the card is at a given width: a door's proportion, 1.10 times
    // as tall as wide, unless its words need more. Its height follows from its
    // width and is never set on its own.
    [[nodiscard]] int height_for(int card_width) const;
    [[nodiscard]] int height_for(int card_width, bool compact) const;
    // How tall its words and drawing alone need it to be at a given width.
    [[nodiscard]] int content_height_for(int card_width, bool compact) const;
    // A smaller drawing and closer lines, for a window too short to hold the
    // Home screen otherwise. The words themselves keep their size.
    void set_compact(bool on);
    [[nodiscard]] bool compact() const { return compact_; }
    // What starts the project. Disabled with its card.
    [[nodiscard]] QPushButton* create_button() const { return create_; }

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    struct Flow {
        QFont title_font, body_font;
        QRectF title, badge, body, button;
        double icon_top = 0;
        double bottom = 0;
    };
    [[nodiscard]] Flow flow_at(int card_width) const;
    [[nodiscard]] Flow flow_at(int card_width, bool compact) const;
    [[nodiscard]] static double icon_scale(int card_width, bool compact);
    void paint_icon(QPainter& painter, const QRectF& into) const;
    // Puts the button at the card's foot, centred.
    void place_button();

    StartRouteDefinition what_;
    QPushButton* create_ = nullptr;
    HomeLiveDemo* demo_ = nullptr;
    ThemeId theme_ = ThemeId::Azure;
    bool under_pointer_ = false;
    bool compact_ = false;
};

} // namespace erdflow::desktop
