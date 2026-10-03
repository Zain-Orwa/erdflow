// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/start_route_card.hpp"

#include "app/desktop/home_sidebar.hpp"
#include "app/desktop/home_live_demo.hpp"
#include "app/desktop/icons.hpp"

#include <QEnterEvent>
#include <QFontMetricsF>
#include <QIcon>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QStyle>

#include <algorithm>
#include <cmath>

namespace erdflow::desktop {
namespace {
constexpr int card_width = 180;
constexpr int card_height = 292;
constexpr int min_card_width = 150;
// Kept close round the title, the preview and the buttons, so a card holds
// its parts together without empty margins (Zain, 2026-09-25).
constexpr double card_padding = 14;

// A colour some share of the way from one to another, and a colour at a
// given opacity.
QColor blend(const QColor& from, const QColor& to, double share) {
    const auto at = [share](int a, int b) { return static_cast<int>(std::lround(a + (b - a) * share)); };
    return {at(from.red(), to.red()), at(from.green(), to.green()), at(from.blue(), to.blue())};
}
QColor tint(QColor colour, int alpha) {
    colour.setAlpha(alpha);
    return colour;
}
// The violet a spark shades towards: the accent turned a little along the
// colour wheel, so it is violet beside a blue accent in every theme, and
// grey where the theme has no colour.
QColor violet_of(const QColor& accent, ThemeId id) {
    if (colourless(id) || accent.hsvHue() < 0) return accent;
    return QColor::fromHsv((accent.hsvHue() + 50) % 360, accent.hsvSaturation(), accent.value());
}
// The fallback drawings are authored in a box this tall, and shown in a box between
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
// times its width, following the approved full-window card (2026-09-27).
constexpr double door = 1.24;
// The row under the title where Coming soon is said, and the two buttons at
// the foot: the gap between them, the least room either keeps round its
// words, and the room Create with AI's spark takes before its words.
constexpr double badge_height = 19;
constexpr double button_height = 40;
constexpr double button_gap = 8;
constexpr double least_button_padding = 6;
// The spark is drawn with its own space after it, since a styled button sets
// its icon hard against its words.
constexpr int spark_size = 15;
constexpr int spark_space = 5;
constexpr double spark_room = spark_size + spark_space + 2;
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
    // Beside it, the same project made with AI (Zain, 2026-09-25). On every
    // card so the card is complete, and on none of them yet to be pressed:
    // AI is a later feature, so it stands disabled and says so.
    ai_ = new QPushButton("Create with AI", this);
    ai_->setObjectName(QString::fromLatin1(what.object_name) + "CreateWithAi");
    ai_->setAccessibleName("Create " + QString::fromLatin1(what.title) + " with AI");
    ai_->setFocusPolicy(Qt::StrongFocus);
    ai_->setIconSize(QSize(spark_size + spark_space, spark_size));
    ai_->setEnabled(false);
    ai_->setToolTip("Creating with AI is coming soon.");
    create_->installEventFilter(this);
    ai_->installEventFilter(this);
    wear(theme_);
}

bool StartRouteCard::eventFilter(QObject* watched, QEvent* event) {
    auto* button = qobject_cast<QPushButton*>(watched);
    if ((button == create_ || button == ai_) && button
        && (event->type() == QEvent::Enter || event->type() == QEvent::Leave)) {
        // The style sheet reads the mark only when it is applied again.
        button->setProperty("lit", event->type() == QEvent::Enter);
        button->style()->unpolish(button);
        button->style()->polish(button);
        button->update();
    }
    return QAbstractButton::eventFilter(watched, event);
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
    if (!create_ || !ai_) return;
    // At the foot of the card, however much room its shape gives beyond its
    // words; the room opens above the buttons rather than below them.
    const auto flow = flow_at(width());
    auto at = flow.button;
    auto beside = flow.ai_button;
    const auto drop = height() - 22.0 - at.bottom();
    at.translate(0, drop);
    beside.translate(0, drop);
    create_->setGeometry(at.toAlignedRect());
    ai_->setText(flow.ai_short ? QStringLiteral("AI") : QStringLiteral("Create with AI"));
    ai_->setGeometry(beside.toAlignedRect());
    ai_->setVisible(ai_offered_);
    if (demo_) {
        const auto top = flow_at(width()).icon_top;
        const auto room = std::max(0.0, at.top() - 8 - top);
        const auto wide = width() - 2 * card_padding;
        const auto tall = std::min(room, wide * HomeLiveDemo::authored_height / HomeLiveDemo::authored_width);
        demo_->setGeometry(QRectF(card_padding, top + (room - tall) / 2, wide, tall).toAlignedRect());
    }
}

QRectF StartRouteCard::preview() const {
    if (!demo_ || !demo_->shown()) return {};
    return demo_->screen().translated(demo_->pos());
}

// The reference card. What a card is actually given is decided by the line the
// cards share; this is only what one asks for when nothing else decides.
QSize StartRouteCard::sizeHint() const { return {card_width, card_height}; }

// Narrower than this and the body breaks a word per line, which has stopped
// being readable.
QSize StartRouteCard::minimumSizeHint() const {
    return {min_card_width, height_for(min_card_width)};
}

void StartRouteCard::set_ai_offered(bool on) {
    if (ai_offered_ == on) return;
    ai_offered_ = on;
    place_button();
    update();
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
    // on it would read under 4.5:1. On a card that cannot be taken yet it
    // still uses the theme accent, so all three cards read alike (Zain,
    // 2026-09-25); it is disabled all the same, so it does not answer, and its
    // tooltip says why.
    const auto fill = id == ThemeId::Azure ? QColor("#007BDD") : chosen_row_fill(t);
    auto ring = t.primary_soft;
    // Raised (Zain, 2026-09-25): a little lighter at its top than its foot,
    // as a button standing up in the light is; the card draws its shadow.
    const auto raised = QStringLiteral("qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 %1, stop:1 %2)");
    const auto top = id == ThemeId::Azure ? blend(fill, QColor("#00A6FF"), 0.65)
                                         : blend(fill, Qt::white, 0.10);
    const auto lit = raised.arg(top.name(), fill.darker(106).name());
    const auto lit_hover = raised.arg(fill.name(), fill.darker(116).name());
    // Under the pointer it lights up (Zain, 2026-09-26): the same raised fill
    // drawn brighter, with a pale edge like light caught round it. Its size,
    // words and place are as at rest; pressing it still shows it pressed.
    const auto glowing = raised.arg(blend(fill, Qt::white, 0.26).name(), blend(fill, Qt::white, 0.06).name());
    const auto glow_edge = blend(fill, Qt::white, 0.50);
    create_->setStyleSheet(QStringLiteral(R"(
        QPushButton {
            background: %1; color: %2; border: 2px solid %3; border-radius: %4px;
            font-size: 14px; font-weight: 600; padding: 0;
        }
        QPushButton:hover { background: %5; border-color: %6; }
        QPushButton[lit="true"] { background: %9; border: 2px solid %10; }
        QPushButton:pressed { background: %7; border-color: %7; }
        QPushButton:focus { border-color: %8; }
        QPushButton:disabled { background: %1; color: %2; border: 2px solid %3; }
        QPushButton[lit="true"]:disabled { background: %9; border: 2px solid %10; }
    )").arg(lit, readable_on(fill).name(), fill.darker(104).name())
       .arg(t.radius_button)
       .arg(lit_hover, fill.darker(112).name(), fill.darker(124).name(), ring.name())
       .arg(glowing, glow_edge.name()));
    auto lettering = create_->font();
    lettering.setFamilies(t.family);
    create_->setFont(lettering);
    // Create with AI is the second choice, so it is light: white, a fine
    // pale-blue outline, navy words and a blue-to-violet spark. It is not yet
    // built, so it is disabled on every card, but it looks as it will, as
    // + Create does (Zain, 2026-09-25).
    const auto outline = blend(t.surface, t.primary, 0.24);
    const auto glassy = raised.arg(t.surface.name(), blend(t.surface, t.primary_soft, 0.75).name());
    // Under the pointer it lights up too, in its own lighter way (Zain,
    // 2026-09-26): its glass takes a soft blue tint and its fine outline the
    // accent, so it is plainly the one being pointed at, though it cannot be
    // pressed yet. Size, words, spark and place are as at rest.
    const auto tinted = raised.arg(blend(t.surface, t.primary_soft, 0.55).name(), t.primary_soft.name());
    ai_->setStyleSheet(QStringLiteral(R"(
        QPushButton {
            background: %1; color: %2; border: 1px solid %3; border-radius: %4px;
            font-size: 14px; font-weight: 600; padding: 0;
        }
        QPushButton:hover { background: %5; border-color: %6; }
        QPushButton[lit="true"] { background: %9; border: 1px solid %6; }
        QPushButton:pressed { background: %7; border-color: %6; }
        QPushButton:focus { border: 2px solid %8; }
        QPushButton:disabled { background: %1; color: %2; border: 1px solid %3; }
        QPushButton[lit="true"]:disabled { background: %9; border: 1px solid %6; }
    )").arg(glassy, t.text_heading.name(), outline.name())
       .arg(t.radius_button)
       .arg(t.hover_surface.name(), t.primary.name(), t.primary_soft.name(), ring.name())
       .arg(tinted));
    ai_->setFont(lettering);
    // The spark is the icon set's own, filled, and shaded from the accent at
    // its foot to violet at its tip.
    QPixmap spark(QSize(spark_size + spark_space, spark_size) * 3);
    spark.setDevicePixelRatio(3);
    spark.fill(Qt::transparent);
    {
        QPainter into(&spark);
        into.drawPixmap(0, 0, solid_pixmap(QStringLiteral("sparkles"), Qt::black, spark_size));
        into.setCompositionMode(QPainter::CompositionMode_SourceIn);
        QLinearGradient hue(QPointF(0, spark_size), QPointF(spark_size, 0));
        hue.setColorAt(0.0, t.primary);
        hue.setColorAt(1.0, violet_of(t.primary, id));
        into.fillRect(QRectF(0, 0, spark_size, spark_size), hue);
    }
    QIcon marked;
    marked.addPixmap(spark, QIcon::Normal);
    marked.addPixmap(spark, QIcon::Disabled);
    ai_->setIcon(marked);
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

    const QRectF box(0.5, 0.5, width() - 1.0, height() - 10.0);
    const bool lit = isChecked();
    const bool hovered = under_pointer_ && isEnabled();
    // Pointed at, or reached by the keyboard, a card that can be taken is
    // coloured a little deeper than the others and than itself at rest: the
    // hover colour carried a little further towards the accent (Zain,
    // 2026-09-25).
    const bool highlighted = isEnabled() && (under_pointer_ || hasFocus());

    // Every card is the same pale glass (Zain, 2026-09-25): white with a
    // breath of the accent, a little more of it towards the foot, a fine
    // pale-blue edge, a catch of light along the top and a soft reflection in
    // the top right corner. Never a dark filled card: the interface stays
    // mostly white. A chosen card is told apart only gently -- a slightly
    // firmer edge and a faint light at its top -- and one that cannot be taken
    // yet says so with its badge and its words, not with its surface.
    QLinearGradient glass(box.topLeft(), box.bottomLeft());
    glass.setColorAt(0.0, highlighted ? blend(t.hover_surface, t.primary, 0.10)
                                      : blend(t.surface, t.primary_soft, 0.70));
    glass.setColorAt(1.0, highlighted ? blend(t.hover_surface, t.primary, 0.22)
                                      : blend(t.primary_soft, t.primary, 0.035));
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
    // A softly rounded sidewall (Zain, 2026-09-27). No light along its
    // shoulder -- that read as a light shining up from underneath -- but the
    // face's own colour turning away from the light: a touch deeper just under
    // the rim, deeper again as it rolls under, eased the way a curved surface
    // darkens, and melting into what it stands on over its last pixels rather
    // than ending in a hard line. Still in the theme's own colours.
    const auto edge_top = blend(blend(t.primary_soft, t.primary, 0.035), t.primary, 0.10);
    const auto edge_under = blend(edge_top, Qt::black, 0.24);
    for (int layer = 9; layer >= 1; --layer) {
        const auto depth = (layer - 1) / 8.0;
        const auto roll = 1.0 - std::cos(depth * 1.5707963267948966);
        auto colour = own(blend(edge_top, edge_under, roll));
        colour.setAlphaF(static_cast<float>(layer <= 6 ? 1.0 : layer == 7 ? 0.8 : layer == 8 ? 0.5 : 0.22));
        QPainterPath side;
        side.addRoundedRect(box.translated(0, layer), t.radius_large_card, t.radius_large_card);
        painter.fillPath(side, colour);
    }
    {
        // Rounded along its length too: the sidewall turns a little deeper
        // towards each end, where it curves away round the corners.
        QPainterPath foot;
        foot.addRoundedRect(box.adjusted(0, 0, 0, 7), t.radius_large_card, t.radius_large_card);
        QLinearGradient ends(box.topLeft(), box.topRight());
        ends.setColorAt(0.0, QColor(0, 0, 0, 26));
        ends.setColorAt(0.12, QColor(0, 0, 0, 0));
        ends.setColorAt(0.88, QColor(0, 0, 0, 0));
        ends.setColorAt(1.0, QColor(0, 0, 0, 26));
        painter.fillPath(foot.subtracted(shape), ends);
    }
    painter.fillPath(shape, glass);
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
    {
        // The reflection: a soft, tilted smear of white near the top right,
        // clipped to the card. Painted, so there is nothing in it to press.
        painter.save();
        painter.setClipPath(shape);
        painter.translate(box.right() - 50, box.top() + 14);
        painter.rotate(-12);
        painter.scale(1.0, 0.34);
        QRadialGradient gleam(QPointF(0, 0), 56);
        // Faint on a dark card, where white would glare.
        const bool dark = t.surface.lightness() < 128;
        gleam.setColorAt(0.0, QColor(255, 255, 255, dark ? 40 : 240));
        gleam.setColorAt(0.5, QColor(255, 255, 255, dark ? 18 : 140));
        gleam.setColorAt(1.0, QColor(255, 255, 255, 0));
        painter.fillRect(QRectF(-56, -56, 112, 112), gleam);
        painter.restore();
    }
    // Light caught along the top edge.
    painter.setPen(QPen(QColor(255, 255, 255, t.surface.lightness() < 128 ? 40 : 235), 1.0));
    painter.drawLine(QPointF(box.left() + t.radius_large_card, box.top() + 1.2),
                     QPointF(box.right() - t.radius_large_card, box.top() + 1.2));
    const auto edge = lit ? blend(t.surface, t.primary, 0.42)
                    : highlighted ? own(QColor("#B8D8FB")) : blend(t.surface, t.primary, 0.20);
    painter.setPen(QPen(edge, lit ? 1.4 : 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);

    // One window header at the top of the full panel; no footer divider.
    const auto header_bottom = flow_at(width()).title.bottom() + 10;
    painter.setPen(QPen(tint(t.primary, 22), 0.7));
    painter.drawLine(QPointF(box.left() + 1, header_bottom), QPointF(box.right() - 1, header_bottom));
    painter.setPen(Qt::NoPen);
    painter.setBrush(own(QColor("#9DD9FF")));
    for (int dot = 0; dot < 3; ++dot)
        painter.drawEllipse(QPointF(box.right() - 12 - dot * 8, 10), 2.5, 2.5);

    // And under each button, a small soft shadow, so the two stand up off
    // the card, each on its own: + Create's in its own blue.
    for (auto* button : {static_cast<QWidget*>(create_), static_cast<QWidget*>(ai_)}) {
        if (button->isHidden()) continue;
        const QRectF under(button->geometry());
        const auto shade = button == create_ ? tint(chosen_row_fill(t), 18) : tint(t.shadow_card.ink, 10);
        painter.setPen(Qt::NoPen);
        for (int spread = 5; spread >= 1; --spread) {
            QPainterPath soft;
            soft.addRoundedRect(under.adjusted(-spread * 0.6, spread * 0.5 + 1.5, spread * 0.6, spread * 0.9 + 2.5),
                                t.radius_button + spread, t.radius_button + spread);
            painter.fillPath(soft, shade);
        }
    }

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
    painter.setOpacity(1.0);
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
    // The title follows the reference proportions; dots sit above its baseline.
    const auto title_room = room;
    flow.title_font.setPixelSize(std::clamp(static_cast<int>(card_width_now * 0.055), 11, 17));
    const auto title_bounds = QFontMetricsF(flow.title_font).boundingRect(
        QRectF(card_padding, card_padding, title_room, 4000),
        Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, QString::fromLatin1(what_.title));
    flow.title = QRectF(card_padding, card_padding, title_room, std::ceil(title_bounds.height()));
    flow.badge = QRectF(card_padding, flow.title.bottom() + (compact ? 4 : 6), room, badge_height);
    flow.icon_top = flow.badge.bottom() + (compact ? 4 : 8);
    flow.body = measure(flow.body_font,
                        flow.icon_top + icon_height * icon_scale(card_width_now, compact)
                            + (compact ? 6 : 10),
                        what_.body);
    // A card holding a preview draws no description, and keeps no room for
    // one (Zain, 2026-09-25): its preview follows the badge row directly.
    if (demo_) {
        flow.body = QRectF(card_padding, flow.badge.bottom(), room, 0);
        flow.icon_top = flow.badge.bottom() + 8;
    }
    auto lettering = font();
    lettering.setFamilies(t.family);
    lettering.setPixelSize(14);
    lettering.setWeight(QFont::DemiBold);
    // + Create and Create with AI share one row, which runs between the card's
    // margins and so is centred on it. Each is as wide as its words and an
    // even share of what is left over. Where that would leave Create with AI
    // too little room round its words, it says only "AI".
    const QFontMetricsF lettered(lettering);
    const auto create_words = std::ceil(lettered.horizontalAdvance("+ Create"));
    const auto ai_words = [&](bool short_form) {
        return spark_room + std::ceil(lettered.horizontalAdvance(short_form ? "AI" : "Create with AI"));
    };
    flow.ai_short = (room - button_gap - create_words - ai_words(false)) / 4 < least_button_padding;
    const auto spare = std::max(0.0, room - button_gap - create_words - ai_words(flow.ai_short));
    flow.button = QRectF(card_padding, flow.body.bottom() + (compact ? 8 : 14), create_words + spare / 2,
                         button_height);
    if (demo_)
        flow.button.moveTop(flow.icon_top + (compact ? 120 : 160) + 8);
    flow.ai_button = QRectF(flow.button.right() + button_gap, flow.button.top(),
                            ai_words(flow.ai_short) + spare / 2, button_height);
    // Without Create with AI, the single action spans 52% of the card,
    // matching the approved reference. Its left edge stays on a whole pixel.
    if (!ai_offered_) {
        flow.button.setWidth(std::max(create_words + 24, card_width_now * 0.52));
        flow.button.moveLeft(std::round((card_width_now - flow.button.width()) / 2));
    }
    flow.bottom = flow.button.bottom() + 22;
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
