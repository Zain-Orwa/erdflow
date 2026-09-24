// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/start_route_card.hpp"

#include "app/desktop/home_sidebar.hpp"
#include "app/desktop/home_live_demo.hpp"

#include <QEnterEvent>
#include <QFontMetricsF>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>

namespace erdflow::desktop {
namespace {
constexpr int card_width = 180;
constexpr int card_height = 292;
constexpr int min_card_width = 150;
constexpr double card_padding = 18;
// The drawings are authored in a box this tall, and shown in a box between
// 88 and 105 tall depending on the card's width (Zain, 2026-09-24), under the
// title and the row where a card's badge sits.
constexpr double icon_top = 30;
constexpr double icon_height = 80;
constexpr double smallest_icon = 88;
constexpr double largest_icon = 105;
// On a short window the drawing is made smaller and the words close up under
// it, so the whole Home screen stays on one page. The words never shrink.
constexpr double compact_icon_height = 40;
// A card is a little taller than wide, like a door: its height is this many
// times its width, whatever width the line gives it (Zain, 2026-09-24).
constexpr double door = 1.10;
// The row under the title where Coming soon is said, and the button at the
// foot.
constexpr double badge_height = 19;
constexpr double button_height = 34;
constexpr double button_padding = 20;
} // namespace

const std::vector<StartRouteDefinition>& start_routes() {
    // The order is fixed by the specification and the copy is verbatim. Both
    // are product language: neither is to be reworded here. The titles are
    // Zain's (2026-09-24, ADR-022 section 9.19); the lines under them are the
    // specification's. The helper line is kept here but no longer drawn: the
    // card's foot is now its + Create button.
    static const std::vector<StartRouteDefinition> routes{
        {StartRoute::Conceptual, "startRouteConceptual",
         "Conceptual Design (ERD)",
         "Start with entities, attributes and relationships (ERD).",
         "Best for brainstorming and early design.", true, nullptr, nullptr},
        {StartRoute::RelationalDesign, "startRouteRelational",
         "Relational Schema",
         "Start directly with tables, columns, keys and constraints.",
         "Best when you already know your data structure.", false, "Coming soon",
         "Starting directly from a relational schema is not enabled yet. "
         "It needs relations that can be made by hand, which is still being built."},
        {StartRoute::Sql, "startRouteSql",
         "SQL Script (DDL)",
         "Write, paste or import SQL to build the relational design.",
         "Best for existing databases or SQL scripts.", false, "Coming soon",
         "Starting from SQL is not enabled yet. It needs SQL to be read into a "
         "relational design and written back out again, which is still being built."},
    };
    return routes;
}

StartRouteCard::StartRouteCard(const StartRouteDefinition& what, QWidget* parent)
    : QAbstractButton(parent), what_(what) {
    setObjectName(QString::fromLatin1(what.object_name));
    setCheckable(true);
    setEnabled(what.enabled);
    setCursor(what.enabled ? Qt::PointingHandCursor : Qt::ArrowCursor);
    setFocusPolicy(Qt::StrongFocus);
    // What the platform reads out. A card is a control and has to say so.
    setText(QString::fromLatin1(what.title));
    setAccessibleName(QString::fromLatin1(what.title));
    // Still read out where a live demo takes its place on the card's face: the
    // demo is a picture, and whatever reads the screen needs the words.
    setAccessibleDescription(QString::fromLatin1(what.body));
    if (!what.enabled && what.reason)
        setToolTip(QString::fromLatin1(what.reason));
    // The card's own action. Pressing the card only chooses it; this is what
    // starts the project. It is the card's child, so a card that cannot be
    // taken yet disables it too, and Tab reaches it straight after its card.
    create_ = new QPushButton("+ Create", this);
    create_->setObjectName(QString::fromLatin1(what.object_name) + "Create");
    create_->setAccessibleName("Create " + QString::fromLatin1(what.title));
    create_->setCursor(Qt::PointingHandCursor);
    create_->setFocusPolicy(Qt::StrongFocus);
    if (!what.enabled && what.reason) create_->setToolTip(QString::fromLatin1(what.reason));
    wear(theme_);
}

void StartRouteCard::set_live_demo(HomeLiveDemo* demo) {
    demo_ = demo;
    demo_->setParent(this);
    demo_->setAttribute(Qt::WA_TransparentForMouseEvents, true);
    // Inside the card it stands on the card, not on a floor of its own.
    demo_->set_floor(false);
    demo_->wear(theme_);
    demo_->show();
    updateGeometry();
    place_button();
}

void StartRouteCard::resizeEvent(QResizeEvent* event) {
    QAbstractButton::resizeEvent(event);
    place_button();
}

void StartRouteCard::place_button() {
    if (!create_) return;
    // At the foot of the card, however much room its shape gives beyond its
    // words; the room opens above the button rather than below it.
    auto at = flow_at(width()).button;
    at.moveBottom(height() - (compact_ ? 12.0 : card_padding));
    create_->setGeometry(at.toAlignedRect());
    if (demo_) {
        const auto top = flow_at(width()).icon_top;
        const auto room = std::max(0.0, at.top() - 8 - top);
        const auto wide = width() - 2 * card_padding;
        const auto tall = std::min(room, wide * HomeLiveDemo::authored_height / HomeLiveDemo::authored_width);
        demo_->setGeometry(QRectF(card_padding, top + (room - tall) / 2, wide, tall).toAlignedRect());
    }
}

// The reference card. What a card is actually given is decided by the line the
// cards share; this is only what one asks for when nothing else decides.
QSize StartRouteCard::sizeHint() const { return {card_width, card_height}; }

// Narrower than this and the body breaks a word per line, which has stopped
// being readable.
QSize StartRouteCard::minimumSizeHint() const {
    return {min_card_width, height_for(min_card_width)};
}

void StartRouteCard::set_compact(bool on) {
    if (compact_ == on) return;
    compact_ = on;
    place_button();
    update();
}

void StartRouteCard::wear(ThemeId id) {
    theme_ = id;
    if (demo_) demo_->wear(id);
    const auto& t = tokens(id);
    // Filled in the primary as the chosen sidebar row is, deepened where white
    // on it would read under 4.5:1. Not yet available, it stands back with
    // its card rather than being greyed past reading.
    const auto fill = chosen_row_fill(t);
    auto ring = t.primary_soft;
    create_->setStyleSheet(QStringLiteral(R"(
        QPushButton {
            background: %1; color: %2; border: 2px solid %1; border-radius: %3px;
            font-size: 14px; font-weight: 600; padding: 0;
        }
        QPushButton:hover { background: %4; border-color: %4; }
        QPushButton:pressed { background: %5; border-color: %5; }
        QPushButton:focus { border-color: %6; }
        QPushButton:disabled { background: %7; color: %8; border: 1px solid %9; }
    )").arg(fill.name(), readable_on(fill).name())
       .arg(t.radius_button)
       .arg(fill.darker(112).name(), fill.darker(124).name(), ring.name(),
            t.surface.name(), t.text_muted.name(), t.border_soft.name()));
    auto lettering = create_->font();
    lettering.setFamilies(t.family);
    create_->setFont(lettering);
    place_button();
    update();
}

void StartRouteCard::enterEvent(QEnterEvent* event) {
    QAbstractButton::enterEvent(event);
    under_pointer_ = true;
    update();
}

void StartRouteCard::leaveEvent(QEvent* event) {
    QAbstractButton::leaveEvent(event);
    under_pointer_ = false;
    update();
}

void StartRouteCard::paintEvent(QPaintEvent*) {
    const auto& t = tokens(theme_);
    // A colour of the card's own rather than the theme's, which a theme with
    // no colour shows as its grey.
    const auto own = [this](const QColor& colour) { return colourless(theme_) ? greyed(colour) : colour; };
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    const QRectF box(0.5, 0.5, width() - 1.0, height() - 1.0);
    const bool lit = isChecked();
    const bool hovered = under_pointer_ && isEnabled();

    // A chosen card is bordered in the primary and filled a shade towards it.
    // Never a dark filled card: the interface stays mostly white, and a card
    // that inverts would shout over the others beside it.
    // One that cannot be taken yet sits a shade off white, between the page
    // and a card: set back, not greyed out, so it reads as not yet rather
    // than as broken.
    QColor fill = lit ? t.selected_card_surface : t.surface;
    if (!isEnabled())
        fill = QColor::fromRgbF((t.surface.redF() + t.window_background.redF()) / 2,
                                (t.surface.greenF() + t.window_background.greenF()) / 2,
                                (t.surface.blueF() + t.window_background.blueF()) / 2);
    QPainterPath shape;
    shape.addRoundedRect(box, t.radius_large_card, t.radius_large_card);

    if (hovered && !lit) {
        // The gentle lift the specification asks for, drawn rather than
        // translated: nothing moves, so a row of cards does not twitch as the
        // pointer crosses it.
        const auto& shade = t.shadow_card_hover;
        for (int step = 6; step >= 1; --step) {
            auto wash = shade.ink;
            wash.setAlphaF(static_cast<float>(wash.alphaF() / (step * 2.4)));
            QPainterPath under;
            under.addRoundedRect(box.adjusted(-step, -step + shade.dy / 3.0, step, step),
                                 t.radius_large_card + step, t.radius_large_card + step);
            painter.fillPath(under, wash);
        }
    }
    painter.fillPath(shape, fill);
    if (lit) {
        // The pale-blue highlight a chosen card carries, strongest at the top
        // and gone by the middle: a light on it, not a colour poured into it.
        QLinearGradient light(box.topLeft(), QPointF(box.left(), box.top() + box.height() * 0.55));
        light.setColorAt(0.0, t.primary_faint);
        auto clear = t.primary_faint;
        clear.setAlpha(0);
        light.setColorAt(1.0, clear);
        painter.fillPath(shape, light);
    }
    const auto edge = !isEnabled() ? t.border_soft
                    : lit ? t.primary
                    : hovered ? own(QColor("#B8D8FB")) : t.border_soft;
    painter.setPen(QPen(edge, lit ? 2.0 : 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);

    // The ring that says where the keyboard is. Drawn inside the border so a
    // focused card is never confused with a chosen one.
    if (hasFocus()) {
        auto ring = t.primary;
        ring.setAlphaF(0.45f);
        painter.setPen(QPen(ring, 2.0));
        painter.drawRoundedRect(box.adjusted(3, 3, -3, -3),
                                t.radius_large_card - 3, t.radius_large_card - 3);
    }

    // The drawing, scaled about the middle of where it sits: larger on a wide
    // card so it fills the card as the words do, smaller on a short window.
    // A card that cannot be taken yet shows it at half strength.
    if (!demo_) {
        const auto scale = icon_scale(width(), compact_);
        painter.save();
        if (!isEnabled()) painter.setOpacity(0.6);
        painter.translate(width() / 2.0, flow_at(width()).icon_top);
        painter.scale(scale, scale);
        painter.translate(-width() / 2.0, -icon_top);
        paint_icon(painter, QRectF(0, icon_top, width(), icon_height));
        painter.restore();
    }

    // A route that cannot be taken yet says so on its face rather than only in
    // a tooltip, which nobody finds unless they already suspect something.
    if (what_.badge) {
        auto small = font();
        small.setFamilies(t.family);
        small.setPixelSize(12);
        small.setWeight(QFont::DemiBold);
        painter.setFont(small);
        const QFontMetricsF measured(small);
        const auto words = QString::fromLatin1(what_.badge);
        const auto row = flow_at(width()).badge;
        const QRectF pill(row.left(), row.top(), measured.horizontalAdvance(words) + 16, badge_height);
        painter.setPen(Qt::NoPen);
        painter.setBrush(t.gold_soft);
        painter.drawRoundedRect(pill, 9.5, 9.5);
        painter.setPen(own(QColor("#8A5B00")));
        painter.drawText(pill, Qt::AlignCenter, words);
        painter.setBrush(Qt::NoBrush);
    }

    // Not yet available is said by the badge and by the words standing back a
    // little -- title at three quarters, body just under -- never
    // by fading them past reading. The badge itself is at full strength.
    const auto words = Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop;
    const auto flow = flow_at(width());
    painter.setFont(flow.title_font);
    painter.setPen(t.text_primary);
    painter.setOpacity(isEnabled() ? 1.0 : 0.76);
    painter.drawText(flow.title, words, QString::fromLatin1(what_.title));
    painter.setFont(flow.body_font);
    painter.setPen(t.text_secondary);
    painter.setOpacity(isEnabled() ? 1.0 : 0.74);
    if (!demo_)
        painter.drawText(flow.body, words, QString::fromLatin1(what_.body));
    painter.setOpacity(1.0);
}

// The words run down the card one after another, each taking the lines it
// needs at the width the card has been given, rather than sitting at fixed
// heights. A card is no longer one width: the cards share a single line and
// grow or narrow with it, so a title that takes two lines on a narrow card
// takes one on a wide one, and whatever follows it moves up.
StartRouteCard::Flow StartRouteCard::flow_at(int card_width_now) const {
    return flow_at(card_width_now, compact_);
}

StartRouteCard::Flow StartRouteCard::flow_at(int card_width_now, bool compact) const {
    const auto& t = tokens(theme_);
    Flow flow;
    const auto room = std::max(40.0, card_width_now - card_padding * 2);
    const auto measure = [&](const QFont& font, double top, const char* words) {
        const auto took = QFontMetricsF(font).boundingRect(
            QRectF(card_padding, top, room, 4000), Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop,
            QString::fromLatin1(words));
        return QRectF(card_padding, top, room, std::ceil(took.height()));
    };
    flow.title_font = font();
    flow.title_font.setFamilies(t.family);
    flow.title_font.setPixelSize(static_cast<int>(t.card_title.size));
    flow.title_font.setWeight(QFont::Bold);
    flow.body_font = font();
    flow.body_font.setFamilies(t.family);
    flow.body_font.setPixelSize(static_cast<int>(t.body.size));
    // Title, the Coming soon row, the drawing, the description, the button.
    // The row is kept on every card, said or not, so the three drawings and
    // the three descriptions stand level across the line.
    flow.title = measure(flow.title_font, compact ? 12 : card_padding, what_.title);
    flow.badge = QRectF(card_padding, flow.title.bottom() + (compact ? 4 : 6), room, badge_height);
    flow.icon_top = flow.badge.bottom() + (compact ? 4 : 8);
    flow.body = measure(flow.body_font,
                        flow.icon_top + icon_height * icon_scale(card_width_now, compact)
                            + (compact ? 6 : 10),
                        what_.body);
    if (demo_) {
        flow.body = measure(flow.body_font, flow.badge.bottom() + (compact ? 4 : 8), what_.body);
        flow.icon_top = flow.body.bottom() + 8;
    }
    auto lettering = font();
    lettering.setFamilies(t.family);
    lettering.setPixelSize(14);
    lettering.setWeight(QFont::DemiBold);
    const auto button_width = std::ceil(QFontMetricsF(lettering).horizontalAdvance("+ Create"))
                            + button_padding * 2;
    flow.button = QRectF(card_width_now / 2.0 - button_width / 2.0,
                         flow.body.bottom() + (compact ? 8 : 14), button_width, button_height);
    if (demo_)
        flow.button.moveTop(flow.icon_top + (compact ? 120 : 160) + 8);
    flow.bottom = flow.button.bottom() + (compact ? 12 : card_padding);
    // Keep the previous responsive height budget so this presentation change
    // cannot resize the other cards. Reclaim the unpainted description's room
    // for the demo after the card/button measurements have been calculated.
    if (demo_)
        flow.icon_top = flow.badge.bottom() + 8;
    return flow;
}

// How large the drawing is: from 88 to 105 tall as the card widens, so it
// holds its place in the card rather than sitting small in a wide one; small
// on a short window.
double StartRouteCard::icon_scale(int card_width_now, bool compact) {
    if (compact) return compact_icon_height / icon_height;
    const auto shown = std::clamp(smallest_icon + (card_width_now - 250) * 17.0 / 65.0,
                                  smallest_icon, largest_icon);
    return shown / icon_height;
}

int StartRouteCard::height_for(int card_width_now) const {
    return height_for(card_width_now, compact_);
}

int StartRouteCard::height_for(int card_width_now, bool compact) const {
    // The door's height, unless the words need more at this width -- which
    // only a card narrower than the line normally gives ever does. A card is
    // never shorter than its shape, so it is never wider than it is tall.
    return std::max(static_cast<int>(std::ceil(card_width_now * door)),
                    content_height_for(card_width_now, compact));
}

int StartRouteCard::content_height_for(int card_width_now, bool compact) const {
    return static_cast<int>(std::ceil(flow_at(card_width_now, compact).bottom));
}

// The three marks, drawn rather than fetched. Each is the smallest picture that
// says what its route starts from: two entities and a relationship, a table, a
// page of SQL.
void StartRouteCard::paint_icon(QPainter& painter, const QRectF& into) const {
    const auto& t = tokens(theme_);
    const auto own = [this](const QColor& colour) { return colourless(theme_) ? greyed(colour) : colour; };
    // Drawn in its own colours whether or not the route can be taken yet; a
    // card that cannot is drawn fainter as a whole by the caller, so its mark
    // still says what it is rather than going grey and saying nothing.
    const auto blue = own(QColor("#3B82F6"));
    const auto faint = t.primary_faint;
    const auto gold = t.gold;
    const auto middle = into.center().x();
    painter.setRenderHint(QPainter::Antialiasing, true);

    switch (what_.route) {
    case StartRoute::Conceptual: break; // The child live demo replaces this drawing.
    case StartRoute::RelationalDesign: {
        // A table: a header row and its cells.
        const QRectF grid(middle - 44, into.top() + 6, 88, 66);
        painter.setPen(QPen(blue, 1.4));
        painter.setBrush(faint);
        painter.drawRoundedRect(grid, 6, 6);
        QPainterPath header;
        header.addRoundedRect(QRectF(grid.left(), grid.top(), grid.width(), 20), 6, 6);
        painter.fillPath(header.intersected([&] {
            QPainterPath whole;
            whole.addRect(QRectF(grid.left(), grid.top(), grid.width(), 20));
            return whole;
        }()), own(QColor("#93C5FD")));
        painter.setPen(QPen(own(QColor("#B8D8FB")), 1.0));
        painter.drawLine(QPointF(grid.left() + 29, grid.top()), QPointF(grid.left() + 29, grid.bottom()));
        painter.drawLine(QPointF(grid.left() + 58, grid.top()), QPointF(grid.left() + 58, grid.bottom()));
        painter.drawLine(QPointF(grid.left(), grid.top() + 41), QPointF(grid.right(), grid.top() + 41));
        break;
    }
    case StartRoute::Sql: {
        // A page of SQL, in the warm accent rather than the blue, so the three
        // modelling routes and the writing route are told apart at a glance.
        painter.setPen(QPen(gold, 1.4));
        painter.setBrush(t.gold_soft);
        painter.drawRoundedRect(QRectF(middle - 29, into.top() + 4, 58, 72), 7, 7);
        auto lettering = font();
        lettering.setFamilies(t.family);
        lettering.setPixelSize(19);
        lettering.setWeight(QFont::Bold);
        painter.setFont(lettering);
        painter.setPen(own(QColor("#8A5B00")));
        painter.drawText(QRectF(middle - 29, into.top() + 4, 58, 72), Qt::AlignCenter, "SQL");
        break;
    }
    }
}

} // namespace erdflow::desktop
