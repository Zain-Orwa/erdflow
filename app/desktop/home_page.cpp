// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/home_page.hpp"

#include <QApplication>
#include <QFontMetricsF>
#include <QHBoxLayout>
#include <QLabel>
#include <QLinearGradient>
#include <QPaintEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QRadialGradient>
#include <QRegion>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QSpacerItem>
#include <QStyle>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <vector>

namespace erdflow::desktop {
namespace {
// The reference geometry the canonical drawing is authored at. These are the
// proportions the design was decided in; the page is laid out responsively
// around them rather than scaled to them.
constexpr int sidebar_width = 206;
constexpr int learning_width = 306;
constexpr int centre_side_padding = 24;

// Where the page stops trying to hold three columns. The learning panel is the
// first to go, being the one nothing depends on; the sidebar follows. Both are
// deliberate points rather than a gradual squeeze, because a column that
// shrinks until its words break is worse than one that is not there.
constexpr int learning_survives_above = 1180;
constexpr int sidebar_survives_above = 880;

constexpr double hero_aspect = 520.0 / 280.0;
// The room kept above the welcome for the illustration to rise into, and how
// much of it a short window gives up.
constexpr int header_room = 84;
constexpr int header_room_compact = 4;
// Between the hero and "Create a new project", and between that heading and
// the cards it introduces (Zain, 2026-09-25): room enough for the hero's
// pictures to end clear of the heading, and the heading close over the cards.
// A short window gives most of it back.
constexpr int greeting_gap = 74;
constexpr int greeting_gap_compact = 14;
constexpr int section_gap = 12;
constexpr int section_gap_compact = 6;

// The cards share one line and never wrap (Zain, 2026-09-23). Each keeps a
// door's shape -- its height 1.10 times its width -- and its width stays
// between these: on a wide window the cards stop growing and the room left
// over becomes wider gaps and then a margin either side of the group, never
// wider cards (Zain, 2026-09-24). Each gap is a column of its own holding the
// way from one card to the next and back (Zain, 2026-09-25), never narrower
// than its words need.
constexpr int least_bridge = 62;
constexpr int widest_bridge = 92;
constexpr int widest_card = 315;
constexpr int narrowest_card = 150;
// The room kept under the cards for their shadow.
constexpr int card_shadow_room = 24;

// The hero is three columns read as one line: a product page on the left, the
// words in the middle, the database illustration on the right, spread across
// nearly the whole of the centre (Zain, 2026-09-25). The two pictures are
// given the same column, so neither outweighs the other, and the words keep
// the middle to themselves: where there is not room for a column this wide
// either side of them, the pictures step aside rather than crowd the title.
// On a very wide window the columns stop growing and the three stay together
// in the middle.
constexpr double widest_wing = 320;
constexpr double narrowest_wing = 150;
constexpr double wing_gutter = 20;
// How much of its column each picture takes: the page a little less than the
// column, the illustration, whose drawing fills about five sixths of its
// width, a little more.
constexpr double page_share = 0.92;
constexpr double database_share = 1.30;
// The rules either side of "Welcome to ERDFlow": how long, and how far from it.
constexpr double rule_reach = 56;
constexpr double rule_gap = 12;

// A colour some share of the way from one to another, and one at an opacity.
QColor blend(const QColor& from, const QColor& to, double share) {
    const auto at = [share](int a, int b) { return static_cast<int>(std::lround(a + (b - a) * share)); };
    return {at(from.red(), to.red()), at(from.green(), to.green()), at(from.blue(), to.blue())};
}
QColor tint(QColor colour, int alpha) {
    colour.setAlpha(alpha);
    return colour;
}
// A soft pool of light: an ellipse of colour fading to nothing at its rim.
void glow(QPainter& p, QPointF at, double across, double down, const QColor& ink) {
    if (across <= 0 || down <= 0) return;
    p.save();
    p.translate(at);
    p.scale(1.0, down / across);
    QRadialGradient pool(QPointF(0, 0), across);
    pool.setColorAt(0.0, ink);
    pool.setColorAt(1.0, tint(ink, 0));
    p.setPen(Qt::NoPen);
    p.setBrush(pool);
    p.drawEllipse(QPointF(0, 0), across, across);
    p.restore();
}

// What is drawn behind the centre: the product-entry page, the rules beside
// "Welcome to ERDFlow", the flowing line from the page, under the words, to
// the platform of the database illustration, the light behind the pictures,
// and the cards' shadows. HomePage::place_hero decides where everything
// stands and hands it over as a Scene; this only draws it. The hero does not
// move, so it is drawn once into a picture and kept until the scene, the size
// or the theme changes -- the orbit animating on top of it costs a copy, not
// a repaint.
class HomeHeroBackdrop final : public QWidget {
public:
    // Everything in this widget's own pixels, which are the centre's.
    struct Scene {
        bool decorated = false;  // the page, the line and the light, as well as the rules
        QPointF page_centre;
        double page_scale = 1.0;
        QRectF words;    // the centred copy, which the line passes beneath
        QRectF subtitle; // the line of words lowest in it, which the line must clear
        QRectF eyebrow;  // "Welcome to ERDFlow" itself
        QPointF landing; // on the database's platform, where the line ends
        QPointF database; // the database's own centre, and its drawing's scale
        double database_unit = 1.0;
        int hero_bottom = 0; // where the hero ends and the kept picture with it
        std::vector<QRectF> cards;

        [[nodiscard]] bool operator==(const Scene&) const = default;
    };
    // The page is authored at this size and scaled as a whole.
    static constexpr double page_width = 216;
    static constexpr double page_height = 120;

    explicit HomeHeroBackdrop(QWidget* parent) : QWidget(parent) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        ticking_.setInterval(33);
        connect(&ticking_, &QTimer::timeout, this, [this] { tick(); });
    }
    // The illustration whose clock the tube's lights keep.
    void follow(const WelcomeFlowIllustration* clock) { clock_ = clock; }
    void set_theme(ThemeId id) {
        theme_ = id;
        kept_ = {};
        update();
    }
    void set_scene(const Scene& scene) {
        if (scene == scene_) return;
        scene_ = scene;
        route();
        kept_ = {};
        update();
    }

protected:
    void paintEvent(QPaintEvent* event) override {
        const auto& t = tokens(theme_);
        const auto ratio = devicePixelRatioF();
        const auto device = (QSizeF(width(), std::max(1, scene_.hero_bottom)) * ratio).toSize();
        if (kept_.size() != device) {
            kept_ = QPixmap(device);
            kept_.setDevicePixelRatio(ratio);
            kept_.fill(Qt::transparent);
            QPainter into(&kept_);
            draw_hero(into, t);
        }
        QPainter p(this);
        p.drawPixmap(0, 0, kept_);
        // Stood still, the electrons rest where the illustration's clock
        // rests; moving, where they were last asked to be.
        const auto seconds = clock_ && !clock_->moving() ? clock_->clock() : shown_seconds_;
        p.setRenderHint(QPainter::Antialiasing);
        if (const auto glow = page_glow(seconds, t); glow.strength > 0 && event->rect().intersects(page_box()))
            draw_page_glow(p, glow);
        if (!wave_.isEmpty()
            && event->rect().intersects(wave_.boundingRect().adjusted(-12, -12, 12, 12).toAlignedRect())) {
            // Inside the page the tube is under it, and so are they.
            QPainterPath everywhere;
            everywhere.addRect(rect());
            QPainterPath page;
            page.addRoundedRect(QRectF(-page_width / 2, -page_height / 2, page_width, page_height), 10, 10);
            p.save();
            p.setClipPath(everywhere.subtracted(lean().map(page)));
            draw_lights(p, t, seconds);
            p.restore();
        }
        // The cards' shadows are few and cheap, and drawn as they are needed:
        // only where what is being repainted reaches them.
        QRectF row;
        for (const auto& card : scene_.cards) row = row.united(card);
        if (!row.isEmpty() && event->rect().intersects(row.adjusted(-30, -30, 30, 40).toAlignedRect())) {
            p.setRenderHint(QPainter::Antialiasing);
            draw_cards(p, t, row);
        }
    }

private:
    // The page floats turned a little, anticlockwise, and a little away from
    // the viewer, its far edge towards the words.
    [[nodiscard]] QTransform lean() const {
        QTransform turned;
        turned.translate(scene_.page_centre.x(), scene_.page_centre.y());
        turned.rotate(-2.5);
        turned.rotate(-12, Qt::YAxis, 700);
        turned.scale(scene_.page_scale, scene_.page_scale);
        return turned;
    }

    void draw_hero(QPainter& p, const Tokens& t) const {
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        if (scene_.decorated) draw_light(p, t);
        draw_rules(p, t);
        if (!scene_.decorated) return;
        draw_line(p, t);
        p.save();
        p.setTransform(lean());
        draw_page(p, t);
        p.restore();
    }

    // Faint light behind the page, the line and the database, and a soft
    // shadow under the database's platform. Pale and wide, so the page stays
    // white and the pictures only stand a little out of it.
    void draw_light(QPainter& p, const Tokens& t) const {
        const auto page = scene_.page_scale * page_width;
        glow(p, scene_.page_centre, page * 0.85, page * 0.55, tint(t.primary, 22));
        glow(p, QPointF(scene_.words.center().x(), scene_.words.bottom() + 20), scene_.words.width() * 0.62,
             40, tint(t.primary, 12));
        const auto unit = scene_.database_unit;
        glow(p, scene_.database, 250 * unit, 150 * unit, tint(t.primary, 34));
        glow(p, scene_.database + QPointF(0, 70 * unit), 120 * unit, 20 * unit,
             tint(blend(t.primary, t.shadow_card.ink, 0.5), 60));
    }

    void draw_rules(QPainter& p, const Tokens& t) const {
        if (scene_.eyebrow.isEmpty()) return;
        const auto y = scene_.eyebrow.center().y() + 0.5;
        for (const int side : {-1, 1}) {
            const auto near = side < 0 ? scene_.eyebrow.left() - rule_gap : scene_.eyebrow.right() + rule_gap;
            const auto far = near + side * rule_reach;
            QLinearGradient fade(QPointF(far, y), QPointF(near, y));
            fade.setColorAt(0, tint(t.primary, 0));
            fade.setColorAt(1, tint(t.primary, 130));
            p.setPen(QPen(QBrush(fade), 1));
            p.drawLine(QPointF(far, y), QPointF(near, y));
        }
    }

    // The line's route: one smooth curve through a handful of points, out of
    // the page's right edge, down under the words, over a crest in the
    // middle, and up onto the platform. A Catmull-Rom curve, so it passes
    // through each point without a corner, and it leaves the page and meets
    // the platform level. Where either end would meet the words' last line,
    // it first passes under that end of it, so it never crosses a word.
    // Worked out once for each scene; the tube and its lights share it.
    void route() {
        wave_ = {};
        if (!scene_.decorated) return;
        const auto& words = scene_.words;
        const auto under = words.bottom();
        const auto& text = scene_.subtitle;
        // It starts a little inside the page, which is drawn over it, so the
        // tube comes out from under the page's edge.
        const auto start = lean().map(QPointF(page_width / 2 - 14, page_height * 0.22));
        std::vector<QPointF> through{start};
        if (start.y() < under + 12) through.emplace_back(text.left() - 6, under + 12);
        through.emplace_back(std::max(words.left() + words.width() * 0.20, text.left() + 34), under + 28);
        through.emplace_back(words.center().x(), under + 17);
        through.emplace_back(std::min(words.right() - words.width() * 0.24, text.right() - 40), under + 26);
        if (scene_.landing.y() < under + 12) through.emplace_back(text.right() + 6, under + 12);
        through.push_back(scene_.landing);
        const auto last = static_cast<int>(through.size()) - 1;
        const auto at = [&](int i) {
            if (i < 0) return QPointF(2 * through[0].x() - through[1].x(), through[0].y());
            if (i > last)
                return QPointF(2 * through.back().x() - through[through.size() - 2].x(), through.back().y());
            return through[static_cast<std::size_t>(i)];
        };
        wave_ = QPainterPath(through[0]);
        for (int i = 0; i < last; ++i)
            wave_.cubicTo(at(i) + (at(i + 1) - at(i - 1)) / 6.0, at(i + 1) - (at(i + 2) - at(i)) / 6.0,
                          at(i + 1));
    }

    // The line between the page and the database is a tube (Zain,
    // 2026-09-25): one light blue, shaded across its width as a rounded tube
    // is -- deeper at its edges, paler towards its middle, with light caught
    // along its centre -- over a soft glow. Nothing is fixed on it, not even
    // at its ends: it comes out from under the page and goes in under the
    // database's platform. What travels along it is drawn live, over this.
    void draw_line(QPainter& p, const Tokens& t) const {
        if (wave_.isEmpty()) return;
        const auto blue = t.primary;
        const QColor white(Qt::white);
        const auto pen = [](const QColor& colour, double width) {
            return QPen(colour, width, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        };
        p.setBrush(Qt::NoBrush);
        p.setPen(pen(tint(blue, 24), 17));
        p.drawPath(wave_);
        const std::array<std::pair<double, double>, 4> across{{{9.0, 0.60}, {7.2, 0.42}, {5.2, 0.26}, {3.2, 0.12}}};
        for (const auto& [wide, share] : across) {
            p.setPen(pen(blend(white, blue, share), wide));
            p.drawPath(wave_);
        }
        p.setPen(pen(tint(white, 235), 1.4));
        p.drawPath(wave_);
    }

    // Electrons travelling along the tube both ways, each in a colour of its
    // own: two going from the page to the database, in cyan and violet, and
    // two coming back, in amber and green, a lap every seven seconds. Each
    // fades in as it sets off and is taken in as it arrives. They keep the
    // illustration's clock, so they move when its orbit moves and rest where
    // it rests. The colours are the theme's, grey where it has none.
    static constexpr double lap_seconds = 7.0;
    struct Traveller {
        double start;    // how far round its lap it is at the clock's nought
        bool coming_back; // from the database to the page
    };
    static constexpr std::array<Traveller, 4> travellers{{{0.00, false}, {0.50, false}, {0.25, true}, {0.75, true}}};
    [[nodiscard]] std::array<QColor, 4> electron_colours(const Tokens& t) const {
        const auto turned = [&](int by) {
            if (colourless(theme_) || t.primary.hsvHue() < 0) return t.primary;
            return QColor::fromHsv((t.primary.hsvHue() + by + 360) % 360, t.primary.hsvSaturation(),
                                   std::min(255, t.primary.value() + 10));
        };
        return {turned(-20), turned(50), t.amber, t.green};
    }
    struct Light {
        QPointF at;
        QColor colour;
        double strength;
    };
    [[nodiscard]] std::vector<Light> lights(double seconds, const Tokens& t) const {
        std::vector<Light> shown;
        if (wave_.isEmpty()) return shown;
        const auto colours = electron_colours(t);
        for (std::size_t k = 0; k < travellers.size(); ++k) {
            const auto along = std::fmod(seconds / lap_seconds + travellers[k].start, 1.0);
            const auto strength = std::min(1.0, along / 0.08) * std::min(1.0, (1.0 - along) / 0.03);
            shown.push_back({wave_.pointAtPercent(travellers[k].coming_back ? 1.0 - along : along), colours[k],
                             strength});
        }
        return shown;
    }
    static QRectF light_box(QPointF at) { return {at - QPointF(10, 10), QSizeF(20, 20)}; }
    void draw_lights(QPainter& p, const Tokens& t, double seconds) const {
        p.setPen(Qt::NoPen);
        for (const auto& light : lights(seconds, t)) {
            p.setBrush(tint(light.colour, static_cast<int>(70 * light.strength)));
            p.drawEllipse(light.at, 7.5, 7.5);
            p.setBrush(tint(light.colour, static_cast<int>(200 * light.strength)));
            p.drawEllipse(light.at, 3.8, 3.8);
            p.setBrush(tint(QColor(Qt::white), static_cast<int>(255 * light.strength)));
            p.drawEllipse(light.at, 1.6, 1.6);
        }
    }
    [[nodiscard]] QRegion lit_at(double seconds) const {
        QRegion lit;
        for (const auto& light : lights(seconds, tokens(theme_))) lit += light_box(light.at).toAlignedRect();
        return lit;
    }

    // When an electron coming back from the database reaches the page, the
    // page lights up in that electron's colour, and the light fades over a
    // second and a half (Zain, 2026-09-25).
    static constexpr double glow_seconds = 1.5;
    struct Glow {
        QColor colour;
        double strength = 0;
    };
    [[nodiscard]] Glow page_glow(double seconds, const Tokens& t) const {
        Glow brightest;
        if (wave_.isEmpty()) return brightest;
        const auto colours = electron_colours(t);
        for (std::size_t k = 0; k < travellers.size(); ++k) {
            if (!travellers[k].coming_back) continue;
            // Its lap starts afresh the moment it arrives, so how far into
            // this lap it is says how long ago it arrived.
            const auto since = std::fmod(seconds / lap_seconds + travellers[k].start, 1.0) * lap_seconds;
            if (since >= glow_seconds) continue;
            const auto strength = std::pow(1.0 - since / glow_seconds, 1.6);
            if (strength > brightest.strength) brightest = {colours[k], strength};
        }
        return brightest;
    }
    [[nodiscard]] QRect page_box() const {
        const QRectF page(-page_width / 2, -page_height / 2, page_width, page_height);
        return lean().mapRect(page).adjusted(-26, -26, 26, 26).toAlignedRect();
    }
    void draw_page_glow(QPainter& p, const Glow& glow) const {
        p.save();
        p.setTransform(lean());
        const QRectF page(-page_width / 2, -page_height / 2, page_width, page_height);
        p.setBrush(Qt::NoBrush);
        for (int spread = 8; spread >= 1; --spread) {
            p.setPen(QPen(tint(glow.colour, static_cast<int>(16 * glow.strength)), spread * 2.2));
            p.drawRoundedRect(page, 10, 10);
        }
        p.setPen(QPen(tint(glow.colour, static_cast<int>(210 * glow.strength)), 1.6));
        p.setBrush(tint(glow.colour, static_cast<int>(18 * glow.strength)));
        p.drawRoundedRect(page, 10, 10);
        p.restore();
    }

    // Asked about thirty times a second while the illustration moves: only
    // the few pixels round each electron, where it was and where it is now,
    // are drawn again -- and the page, while it glows.
    void tick() {
        if (!clock_ || !scene_.decorated || !isVisible()) return;
        ticking_.setInterval(clock_->moving() ? 33 : 500);
        const auto now = clock_->clock();
        if (now == shown_seconds_) return;
        const auto& t = tokens(theme_);
        auto dirty = lit_at(shown_seconds_) + lit_at(now);
        if (page_glow(shown_seconds_, t).strength > 0 || page_glow(now, t).strength > 0) dirty += page_box();
        shown_seconds_ = now;
        update(dirty);
    }

    // A small web page that says what ERDFlow is for: its name, one line, and
    // the four things it does. No tables or diagrams -- those are the cards'.
    // It floats: a soft blue light round it, a shadow under it, and a glass
    // face with light caught along its top.
    void draw_page(QPainter& p, const Tokens& t) const {
        const QRectF page(-page_width / 2, -page_height / 2, page_width, page_height);
        p.setPen(Qt::NoPen);
        for (int spread = 12; spread >= 1; --spread) {
            p.setBrush(tint(t.primary, 4));
            p.drawRoundedRect(page.adjusted(-spread, -spread, spread, spread), 10 + spread, 10 + spread);
        }
        for (int spread = 9; spread >= 1; --spread) {
            p.setBrush(tint(t.shadow_card.ink, 7));
            p.drawRoundedRect(page.adjusted(-spread * 0.8, spread * 0.6 + 5, spread * 0.8, spread * 1.3 + 6),
                              10 + spread, 10 + spread);
        }
        QLinearGradient glass(page.topLeft(), page.bottomRight());
        glass.setColorAt(0, tint(t.surface, 252));
        glass.setColorAt(1, blend(t.surface, t.primary_soft, 0.95));
        p.setBrush(glass);
        p.setPen(QPen(tint(t.primary, 60), 0.8));
        p.drawRoundedRect(page, 10, 10);
        p.setPen(QPen(tint(t.surface, 240), 1.0));
        p.drawLine(QPointF(page.left() + 10, page.top() + 1.2), QPointF(page.right() - 10, page.top() + 1.2));

        // The window's own bar: three lights, an address, and a few places
        // to go, the last of them lit.
        const auto bar = page.top() + 17;
        p.setPen(QPen(tint(t.primary, 36), 0.7));
        p.drawLine(QPointF(page.left(), bar), QPointF(page.right(), bar));
        p.setPen(Qt::NoPen);
        const std::array<QColor, 3> lights{t.red, t.amber, t.green};
        for (std::size_t i = 0; i < lights.size(); ++i) {
            p.setBrush(tint(lights[i], 200));
            p.drawEllipse(QPointF(page.left() + 11 + 7.0 * static_cast<double>(i), bar - 8.5), 2.2, 2.2);
        }
        p.setBrush(tint(t.primary, 22));
        p.drawRoundedRect(QRectF(-52, bar - 12, 70, 7), 3.5, 3.5);
        for (int place = 0; place < 2; ++place)
            p.drawRoundedRect(QRectF(34 + place * 17.0, bar - 11, 13, 5), 2.5, 2.5);
        p.setBrush(tint(t.primary, 170));
        p.drawRoundedRect(QRectF(page.right() - 30, bar - 12.5, 20, 8), 4, 4);

        auto font = p.font();
        font.setFamilies(t.family);
        const auto set = [&](double size, QFont::Weight weight) {
            font.setPixelSize(std::max(1, qRound(size)));
            font.setWeight(weight);
            p.setFont(font);
        };
        const auto inner = page.adjusted(10, 0, -10, 0);
        set(14, QFont::Bold);
        p.setPen(t.text_heading);
        p.drawText(QRectF(inner.left(), bar + 5, inner.width(), 20), Qt::AlignCenter, "Welcome to ERDFlow");
        set(9.5, QFont::Normal);
        p.setPen(t.text_secondary);
        p.drawText(QRectF(inner.left(), bar + 26, inner.width(), 26), Qt::AlignCenter,
                   "Turn your ideas into real databases\nwith the power of AI.");

        // The four things it does, as small tiles in one row, each with its
        // own mark. The lettering is as large as the row allows.
        struct Tile {
            const char* word;
            QColor colour;
        };
        const auto violet = colourless(theme_) || t.primary.hsvHue() < 0
            ? t.primary
            : QColor::fromHsv((t.primary.hsvHue() + 50) % 360, t.primary.hsvSaturation(), t.primary.value());
        const std::array<Tile, 4> tiles{{{"Design", violet},
                                         {"Convert", t.green},
                                         {"Generate", t.amber},
                                         {"AI", t.primary}}};
        constexpr double pad = 5, mark = 7, space = 3.5, apart = 4;
        auto size = 9.0;
        std::array<double, 4> wide{};
        for (;; size -= 0.25) {
            set(size, QFont::DemiBold);
            const QFontMetricsF metrics(font);
            auto total = apart * (tiles.size() - 1);
            for (std::size_t i = 0; i < tiles.size(); ++i) {
                wide[i] = pad + mark + space + metrics.horizontalAdvance(tiles[i].word) + pad + 1;
                total += wide[i];
            }
            if (total <= inner.width() || size <= 7) {
                auto x = -total / 2;
                const auto top = page.bottom() - 30;
                for (std::size_t i = 0; i < tiles.size(); ++i) {
                    const QRectF tile(x, top, wide[i], 20);
                    p.setPen(QPen(tint(t.primary, 38), 0.7));
                    p.setBrush(tint(t.surface, 225));
                    p.drawRoundedRect(tile, 6, 6);
                    const QPointF dot(tile.left() + pad + mark / 2, tile.center().y());
                    p.setPen(Qt::NoPen);
                    if (i + 1 == tiles.size()) {
                        // AI is marked with a spark rather than a dot.
                        QPainterPath spark;
                        const auto r = mark / 2 + 0.6;
                        spark.moveTo(dot + QPointF(0, -r));
                        spark.quadTo(dot, dot + QPointF(r, 0));
                        spark.quadTo(dot, dot + QPointF(0, r));
                        spark.quadTo(dot, dot + QPointF(-r, 0));
                        spark.quadTo(dot, dot + QPointF(0, -r));
                        p.setBrush(tiles[i].colour);
                        p.drawPath(spark);
                    } else {
                        p.setBrush(tint(tiles[i].colour, 60));
                        p.drawEllipse(dot, mark / 2, mark / 2);
                        p.setBrush(tiles[i].colour);
                        p.drawEllipse(dot, mark / 4, mark / 4);
                    }
                    p.setPen(t.text_primary);
                    p.drawText(QRectF(dot.x() + mark / 2 + space, tile.top(), wide[i], tile.height()),
                               Qt::AlignLeft | Qt::AlignVCenter, tiles[i].word);
                    x += wide[i] + apart;
                }
                break;
            }
        }
    }

    // Under the cards, the faintest wash of the accent across the row, and a
    // soft, wide shadow under each card that lifts it off the page.
    void draw_cards(QPainter& p, const Tokens& t, const QRectF& row) const {
        glow(p, row.center() + QPointF(0, 12), row.width() * 0.6, row.height() * 0.62, tint(t.primary, 14));
        // The shadow is where the light does not reach, so it is drawn in the
        // theme's shadow ink rather than in its accent (Zain, 2026-09-27): an
        // accent under a card is a coloured halo, not a lift, and on a
        // high-contrast or dark theme it glared -- yellow round every card,
        // or cyan. A breath of the accent is kept where the page is light,
        // and none where it is dark. It lies under the card and a little to
        // its sides, as light from above casts it, and fades out over many
        // fine steps, so it has no edge anywhere.
        const bool dark = t.window_background.lightness() < 128;
        const auto shade = dark ? QColor(0, 0, 0) : blend(QColor(15, 23, 42), t.primary, 0.15);
        const int steps = 16;
        const double darkest = dark ? 0.30 : 0.10;
        p.setPen(Qt::NoPen);
        for (const auto& card : scene_.cards)
            for (int step = steps; step >= 1; --step) {
                const auto spread = static_cast<double>(step);
                QPainterPath under;
                under.addRoundedRect(card.adjusted(-spread * 0.55, 6 + spread * 0.25, spread * 0.55, 4 + spread * 0.9),
                                     t.radius_large_card + spread * 0.6, t.radius_large_card + spread * 0.6);
                // Each ring adds a little, so the middle is darkest and the
                // rim is nothing.
                p.fillPath(under, tint(shade, static_cast<int>(std::lround(255 * darkest / steps * 1.6))));
            }
    }

    void showEvent(QShowEvent* event) override {
        QWidget::showEvent(event);
        ticking_.start();
    }
    void hideEvent(QHideEvent* event) override {
        QWidget::hideEvent(event);
        ticking_.stop();
    }

    ThemeId theme_ = ThemeId::Azure;
    Scene scene_;
    QPixmap kept_;
    QPainterPath wave_;
    const WelcomeFlowIllustration* clock_ = nullptr;
    QTimer ticking_;
    double shown_seconds_ = 0.0;
};

// Between two cards, the way from one to the next and back (Zain,
// 2026-09-25): what the step does, over a blue arrow pointing on, and a grey
// arrow pointing back, over what it undoes. It is a sign rather than a
// control -- the conversions themselves are made in the workspaces -- so
// nothing here takes a press or the keyboard. It stands in its own column of
// the card line, never over a card, level with the middle of the previews
// either side of it.
class HomeFlowBridge final : public QWidget {
public:
    HomeFlowBridge(const char* forward, const char* back, QWidget* parent)
        : QWidget(parent), forward_(QString::fromLatin1(forward)), back_(QString::fromLatin1(back)) {
        setAttribute(Qt::WA_TransparentForMouseEvents);
        setFocusPolicy(Qt::NoFocus);
        auto said = forward_ + ", " + back_;
        said.replace('\n', ' ');
        setAccessibleName(said);
    }
    StartRouteCard* before = nullptr;
    StartRouteCard* after = nullptr;
    void wear(ThemeId id) {
        theme_ = id;
        update();
    }

protected:
    void paintEvent(QPaintEvent*) override {
        const auto& t = tokens(theme_);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.setRenderHint(QPainter::TextAntialiasing);
        auto middle = height() / 2.0;
        if (before && after) {
            const auto one = before->preview();
            const auto other = after->preview();
            if (!one.isEmpty() && !other.isEmpty()) middle = (one.center().y() + other.center().y()) / 2.0;
        }
        const auto across = width() / 2.0;
        const auto reach = std::min(38.0, width() - 20.0);
        const auto on = middle - 16;
        const auto back = middle + 16;
        // Forward in the accent; back in the violet the interface turns from
        // it -- the Create with AI spark's -- rather than grey (Zain,
        // 2026-09-25), a little lighter so the way on still leads.
        const auto violet = colourless(theme_) || t.primary.hsvHue() < 0
            ? blend(t.primary, Qt::white, 0.25)
            : QColor::fromHsv((t.primary.hsvHue() + 50) % 360, t.primary.hsvSaturation(),
                              std::min(255, t.primary.value() + 10));
        arrow(p, QPointF(across - reach / 2, on), QPointF(across + reach / 2, on), t.primary, 3.2,
              t.shadow_card.ink);
        arrow(p, QPointF(across + reach / 2, back), QPointF(across - reach / 2, back),
              blend(violet, Qt::white, 0.15), 2.8, t.shadow_card.ink);
        auto font = p.font();
        font.setFamilies(t.family);
        font.setPixelSize(12);
        font.setWeight(QFont::DemiBold);
        p.setFont(font);
        p.setPen(legible_on(t.primary, t.surface));
        p.drawText(QRectF(0, on - 12 - 34, width(), 34), Qt::AlignHCenter | Qt::AlignBottom, forward_);
        font.setWeight(QFont::Normal);
        p.setFont(font);
        // Its words in the violet too, softened a little towards the quiet
        // lettering, so the way back reads after the way on.
        p.setPen(blend(legible_on(violet, t.surface), t.text_muted, 0.3));
        p.drawText(QRectF(0, back + 12, width(), 34), Qt::AlignHCenter | Qt::AlignTop, back_);
    }

private:
    // A small rounded arrow standing up off the page (Zain, 2026-09-25): a
    // shaft shaded as a tube is, deeper at its edges and lit along its top; a
    // solid head shaded the same way; two beads trailing from its tail; and a
    // soft shadow under it all.
    static void arrow(QPainter& p, QPointF from, QPointF to, const QColor& ink, double weight,
                      const QColor& shade) {
        const auto way = to.x() > from.x() ? 1.0 : -1.0;
        const QPointF tail(from.x() + way * 12.0, from.y());
        const QPointF neck(to.x() - way * 7.5, to.y());
        QPainterPath head;
        head.moveTo(to + QPointF(way * 1.5, 0));
        head.lineTo(neck + QPointF(0, -6.4));
        head.lineTo(neck + QPointF(0, 6.4));
        head.closeSubpath();
        const auto line = [](const QColor& colour, double wide) {
            return QPen(colour, wide, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
        };
        // The shadow, a little below.
        const QPointF below(0, 2.2);
        p.setPen(line(tint(shade, 38), weight + 1.8));
        p.setBrush(tint(shade, 38));
        p.drawLine(tail + below, neck + below);
        p.drawPath(head.translated(below));
        // The beads, each a small sphere lit from above, nearer the shaft the
        // larger and the stronger.
        p.setPen(Qt::NoPen);
        for (int bead = 0; bead < 2; ++bead) {
            const QPointF at(from.x() + way * bead * 6.0, from.y());
            const auto radius = 1.9 + 0.4 * bead;
            QRadialGradient round(at + QPointF(-0.5, -0.6), radius * 1.3);
            round.setColorAt(0.0, tint(blend(ink, Qt::white, 0.6), 150 + 60 * bead));
            round.setColorAt(1.0, tint(ink, 150 + 60 * bead));
            p.setBrush(round);
            p.drawEllipse(at, radius, radius);
        }
        // The shaft: its edge, its body, and light along its top.
        p.setBrush(Qt::NoBrush);
        p.setPen(line(ink.darker(118), weight + 1.2));
        p.drawLine(tail, neck);
        p.setPen(line(ink, weight));
        p.drawLine(tail, neck);
        p.setPen(line(tint(blend(ink, Qt::white, 0.55), 230), weight * 0.38));
        p.drawLine(tail - QPointF(0, weight * 0.2), neck - QPointF(0, weight * 0.2));
        // The head, lit from above.
        QLinearGradient lit(QPointF(0, to.y() - 6.4), QPointF(0, to.y() + 6.4));
        lit.setColorAt(0.0, blend(ink, Qt::white, 0.35));
        lit.setColorAt(1.0, ink.darker(112));
        p.setPen(QPen(ink.darker(118), 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(lit);
        p.drawPath(head);
        p.setPen(line(tint(QColor(Qt::white), 150), 0.9));
        p.drawLine(neck + QPointF(way * 0.6, -5.2), to + QPointF(-way * 0.8, -0.9));
    }

    QString forward_;
    QString back_;
    ThemeId theme_ = ThemeId::Azure;
};

// The row of start cards: one line, the cards dividing it between them, and
// between each card and the next, the way from one to the other.
class CardLine final : public QWidget {
public:
    explicit CardLine(QWidget* parent) : QWidget(parent) {
        setObjectName("startRouteGrid");
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }
    std::vector<StartRouteCard*> cards;
    std::vector<HomeFlowBridge*> bridges; // between each card and the next
    // Told where the first card stands each time the line is placed, so the
    // heading over the cards can stand level with it.
    std::function<void(int)> placed;

    [[nodiscard]] int count() const { return std::max<int>(1, static_cast<int>(cards.size())); }
    // The width the cards are sized from: the line's, less the room a scroll
    // bar would take whether or not one is showing. A card's height follows
    // from its width, so if a scroll bar appearing narrowed the cards, the
    // shorter cards could make it go again -- and the page would never settle.
    void set_steady_width(int wide) {
        if (steady_ == wide) return;
        steady_ = wide;
        place();
    }
    [[nodiscard]] int sizing_width() const { return steady_ > 0 ? std::min(width(), steady_) : width(); }
    // As wide as the line allows, beside the narrowest the ways between them
    // can be, up to the widest a card is.
    [[nodiscard]] int row_width(int line) const {
        return std::clamp((line - least_bridge * (count() - 1)) / count(), 1, widest_card);
    }
    [[nodiscard]] int height_at(int each, bool compact) const {
        auto tallest = 0;
        for (auto* card : cards) tallest = std::max(tallest, card->height_for(each, compact));
        return tallest;
    }
    // How tall the line is with nothing but its width to decide, in either
    // mode. What fit() weighs against the room there is.
    [[nodiscard]] int natural_height(bool compact) const {
        return height_at(row_width(sizing_width()), compact);
    }
    // A height the line must not pass, on a window too short for the cards at
    // the width the line would give them. They are narrowed to fit it, keeping
    // their shape -- but no narrower than the point where their words would
    // need more than their shape, since a narrower card is then a taller one.
    // Nothing at all is the same as no limit.
    void set_limit(int tallest) {
        if (limit_ == tallest) return;
        limit_ = tallest;
        place();
    }
    [[nodiscard]] int chosen_width() const {
        auto each = row_width(sizing_width());
        if (limit_ <= 0) return each;
        const bool compact = !cards.empty() && cards.front()->compact();
        while (each > narrowest_card && height_at(each, compact) > limit_) {
            auto content = 0;
            for (auto* card : cards)
                content = std::max(content, card->content_height_for(each - 2, compact));
            if (content > height_at(each, compact)) break;
            each -= 2;
        }
        return each;
    }
    [[nodiscard]] QSize minimumSizeHint() const override {
        const auto narrowest = narrowest_card * count() + least_bridge * (count() - 1);
        return {narrowest, height()};
    }
    [[nodiscard]] QSize sizeHint() const override { return {width(), height()}; }
    void place() {
        // Setting the height below resizes the line, which asks for this
        // again; once is enough.
        if (placing_) return;
        placing_ = true;
        const auto each = chosen_width();
        const auto compact = !cards.empty() && cards.front()->compact();
        const auto tall = height_at(each, compact);
        // Cards that fill the line start at its left, under the heading. Cards
        // held narrower -- at their widest on a wide window, or narrowed to a
        // short one -- leave room over: it widens the ways between them first,
        // a little, and then sits either side of the group so the group stays
        // centred.
        const bool filling = each == (sizing_width() - least_bridge * (count() - 1)) / count();
        const auto spare = std::max(0, sizing_width() - each * count() - least_bridge * (count() - 1));
        // Filling, the width held back for a scroll bar that is not showing
        // goes into the ways between them, so the row still ends where it did.
        const auto slack = std::max(0, width() - sizing_width());
        const auto gap = count() <= 1 ? 0
            : filling ? std::min(widest_bridge, least_bridge + slack / (count() - 1))
                      : std::min(widest_bridge, least_bridge + spare / (count() - 1));
        const auto group = each * count() + gap * (count() - 1);
        auto x = filling ? 0 : std::max(0, (width() - group) / 2);
        for (auto* card : cards) {
            card->setGeometry(x, 0, each, tall);
            x += each + gap;
        }
        for (std::size_t i = 0; i < bridges.size() && i + 1 < cards.size(); ++i) {
            bridges[i]->setGeometry(cards[i]->geometry().right() + 1, 0, gap, tall);
            bridges[i]->update();
        }
        if (height() != tall) setFixedHeight(tall);
        placing_ = false;
        if (placed && !cards.empty()) placed(cards.front()->x());
    }

protected:
    void resizeEvent(QResizeEvent* event) override {
        QWidget::resizeEvent(event);
        place();
    }

private:
    int limit_ = 0;
    int steady_ = 0;
    bool placing_ = false;
};

// A label given the size and weight a token names, in the theme's own face.
void dress(QLabel* label, const TextStyle& style, const QColor& ink, const QStringList& family) {
    auto font = label->font();
    font.setFamilies(family);
    font.setPixelSize(static_cast<int>(style.size));
    font.setWeight(static_cast<QFont::Weight>(style.weight));
    label->setFont(font);
    label->setStyleSheet(QStringLiteral("color: %1; background: transparent;").arg(ink.name()));
}
} // namespace

HomePage::HomePage(QWidget* parent) : QWidget(parent) {
    setObjectName("homePage");
    setAutoFillBackground(false);
    // Made in the order they are read -- the bar, the rail, the centre, the
    // panel -- which is the order Tab moves through them.
    top_bar_ = new AppTopBar(this);
    sidebar_ = new HomeSidebar(this);
    build_centre();
    learning_ = new HomeLearningPanel(this);
    wear(theme_);
}

void HomePage::build_centre() {
    centre_scroll_ = new QScrollArea(this);
    centre_scroll_->setObjectName("homeCentreScroll");
    centre_scroll_->setFrameShape(QFrame::NoFrame);
    centre_scroll_->setWidgetResizable(true);
    centre_scroll_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    centre_ = new QWidget(centre_scroll_);
    centre_->setObjectName("homeCentre");
    centre_scroll_->setWidget(centre_);

    auto* column = new QVBoxLayout(centre_);
    column->setContentsMargins(centre_side_padding, 0, centre_side_padding, 0);
    column->setSpacing(0);
    // Every gap is a floor and a stretch: the page opens out on a tall window
    // and closes up on a short one, which is how it stays one page. Most of
    // what a tall window has to spare goes under the form rather than over
    // the welcome, so the page reads from the top rather than floating.
    top_space_ = new QSpacerItem(0, header_room, QSizePolicy::Minimum, QSizePolicy::Fixed);
    column->addSpacerItem(top_space_);
    column->addStretch(1);

    // The small welcome and both illustrations sit around the greeting, and
    // "Create a new project" stands under the hero, over the cards.
    auto* top = new QHBoxLayout;
    top->setSpacing(24);
    auto* greeting = new QVBoxLayout;
    greeting->setSpacing(0);
    welcome_ = new QLabel("Welcome to ERDFlow", centre_);
    welcome_->setObjectName("homeWelcome");
    welcome_->setAlignment(Qt::AlignCenter);
    title_ = new QLabel("Design. Convert. Generate.", centre_);
    title_->setObjectName("homeTitle");
    title_->setAlignment(Qt::AlignCenter);
    // Two of its words are lettered in the accent, which is why it is rich
    // text; what it says is the same plain words either way.
    title_->setTextFormat(Qt::RichText);
    title_->setAccessibleName("Design. Convert. Generate.");
    subtitle_ = new QLabel("Balance ideas, schemas, and SQL in one visual workflow.", centre_);
    subtitle_->setObjectName("homeSubtitle");
    subtitle_->setAlignment(Qt::AlignCenter);
    greeting->addWidget(title_);
    greeting->addSpacing(4);
    greeting->addWidget(subtitle_);
    greeting->addStretch(1);
    greeting_space_ = new QSpacerItem(0, greeting_gap, QSizePolicy::Minimum, QSizePolicy::Fixed);
    greeting->addSpacerItem(greeting_space_);
    top->addLayout(greeting, 1);
    hero_backdrop_ = new HomeHeroBackdrop(centre_);
    hero_backdrop_->setObjectName("homeHeroBackdrop");
    hero_backdrop_->lower();
    // The drawing sits in a holder of the size it is shown at, rather than
    // being fixed to it, so it stays a component that draws at whatever size
    // any other caller gives it.
    hero_holder_ = new QWidget(centre_);
    hero_holder_->setObjectName("homeHeroHolder");
    auto* holding = new QVBoxLayout(hero_holder_);
    holding->setContentsMargins(0, 0, 0, 0);
    hero_ = new WelcomeFlowIllustration(hero_holder_);
    static_cast<HomeHeroBackdrop*>(hero_backdrop_)->follow(hero_);
    // Told to fill the holder whatever it would ask for on its own, or its own
    // idea of a minimum would push it past the holder's edge and clip it.
    hero_->setMinimumSize(0, 0);
    hero_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    holding->addWidget(hero_);
    column->addLayout(top);
    // No heading stands over the cards: each card's own + Create says what
    // it does (Zain, 2026-09-25). The room the heading had is kept, so the
    // cards stand where they stood, and it is still where the hero's pictures
    // are measured to end clear of.
    section_ = new QLabel("Create a new project", centre_);
    section_->setObjectName("homeSectionTitle");
    auto keep_room = section_->sizePolicy();
    keep_room.setRetainSizeWhenHidden(true);
    section_->setSizePolicy(keep_room);
    section_->hide();
    column->addWidget(section_);
    header_space_ = new QSpacerItem(0, section_gap, QSizePolicy::Minimum, QSizePolicy::Fixed);
    column->addSpacerItem(header_space_);

    // The routes, on one line. Remembered in order so the choice can be moved
    // between them by keyboard as well as by pointer.
    auto* line = new CardLine(centre_);
    card_row_ = line;
    for (const auto& what : start_routes()) {
        auto* card = new StartRouteCard(what, line);
        card->setChecked(what.route == chosen_);
        const auto choose = [this, route = what.route] {
            chosen_ = route;
            for (auto* one : cards_) one->setChecked(one->route() == route);
            if (route_selected) route_selected(route);
        };
        connect(card, &QAbstractButton::clicked, this, choose);
        // The card's own + Create is what starts; it chooses its card first,
        // so what is lit is always what was made.
        connect(card->create_button(), &QPushButton::clicked, this, [this, choose, route = what.route] {
            choose();
            if (route_chosen) route_chosen(route);
        });
        cards_.push_back(card);
    }
    line->cards = cards_;
    // Between each card and the next, the way on and the way back.
    const std::array<std::pair<const char*, const char*>, 2> ways{{
        {"Convert\nto Schema", "Back\nto ERD"},
        {"Generate\nto SQL", "Back\nto Schema"},
    }};
    const std::array<const char*, 2> named{"homeWayToSchema", "homeWayToSql"};
    for (std::size_t i = 0; i < ways.size() && i + 1 < cards_.size(); ++i) {
        auto* bridge = new HomeFlowBridge(ways[i].first, ways[i].second, line);
        bridge->setObjectName(named[i]);
        bridge->before = cards_[i];
        bridge->after = cards_[i + 1];
        line->bridges.push_back(bridge);
        bridges_.push_back(bridge);
    }
    line->placed = [this](int first) {
        if (section_->indent() != first) section_->setIndent(first);
        QTimer::singleShot(0, this, [this] { place_hero(); });
    };
    column->addWidget(card_row_);
    cards_space_ = new QSpacerItem(0, 22, QSizePolicy::Minimum, QSizePolicy::Fixed);
    column->addSpacerItem(cards_space_);

    // A live demo inside each card, in place of its drawing and description
    // (Zain, 2026-09-25; ADR-022 section 9.21). One clock plays all three,
    // each starting a second after the one before it, so their busiest
    // moments never fall together.
    demo_clock_ = new HomeDemoClock(this);
    // Which demos move. Zain asked on 2026-09-25 for all three to stand as
    // still pictures of their finished scenes, keeping the motion so that
    // one, two or all of them can be switched back on: set a card's `moves`
    // to true and that demo plays again, a second after the one before it.
    struct Played {
        HomeDemoKind kind;
        double delay;
        bool moves;
    };
    const std::array<Played, 3> played{{
        {HomeDemoKind::Conceptual, 0.0, false},
        {HomeDemoKind::Relational, 1.0, false},
        {HomeDemoKind::Sql, 2.0, false},
    }};
    for (std::size_t i = 0; i < played.size() && i < cards_.size(); ++i) {
        demos_.push_back(new HomeLiveDemo(played[i].kind));
        cards_[i]->set_live_demo(demos_.back());
        demo_clock_->add(demos_.back(), played[i].delay, played[i].moves);
        // Standing still, the Conceptual card shows the Conceptual canvas
        // itself, drawn small (Zain, 2026-09-25); moving, its painted scene.
        demos_.back()->set_real_canvas(!played[i].moves);
    }
    // A demo switched on plays only where motion is welcome, through the one
    // seam that says so (ADR-022 section 9.9), and only while the cards can be
    // seen. With none switched on, nothing ticks at all.
    demo_clock_->follow(card_row_);
    demo_clock_->set_moving(!WelcomeFlowIllustration::platform_prefers_stillness());

    // Nothing is asked under the cards: the page ends with them (Zain,
    // 2026-09-24, ADR-022 section 9.19). Where a project's name and place are
    // asked is decided separately.
    column->addStretch(3);
    title_->installEventFilter(this);
}

bool HomePage::eventFilter(QObject* watched, QEvent* event) {
    if (watched == title_ && (event->type() == QEvent::Move || event->type() == QEvent::Resize)) {
        // Qt can settle the greeting after fit(), especially on first show.
        // Follow its final position so the floating welcome never overlaps it.
        QTimer::singleShot(0, this, [this] { place_hero(); });
    }
    return QWidget::eventFilter(watched, event);
}

void HomePage::set_compact(bool on) {
    if (compact_ == on) return;
    compact_ = on;
    // What gives way, in order: the gaps close to a few pixels, the
    // illustration steps aside and the card drawings shrink. The words on the
    // cards and their buttons keep their size.
    top_space_->changeSize(0, on ? header_room_compact : header_room, QSizePolicy::Minimum,
                           QSizePolicy::Fixed);
    header_space_->changeSize(0, on ? section_gap_compact : section_gap, QSizePolicy::Minimum,
                              QSizePolicy::Fixed);
    greeting_space_->changeSize(0, on ? greeting_gap_compact : greeting_gap,
                                QSizePolicy::Minimum, QSizePolicy::Fixed);
    cards_space_->changeSize(0, on ? 8 : 22, QSizePolicy::Minimum, QSizePolicy::Fixed);
    for (auto* card : cards_) card->set_compact(on);
    static_cast<CardLine*>(card_row_)->place();
    centre_->layout()->invalidate();
    // Once more after the page has been laid out in its new spacing, which is
    // when the room the cards have can be measured rather than estimated.
    QTimer::singleShot(0, this, [this] { fit(); });
}

// How much taller the page is with its full spacing than without.
int HomePage::spacing_saving() const {
    return (header_room - header_room_compact) + (section_gap - section_gap_compact)
        + (greeting_gap - greeting_gap_compact) + (22 - 8);
}

// Lays the hero out as three columns about the middle of the page: the
// eyebrow, title and subtitle centred, the product page in a column to their
// left and the database illustration in a column of the same width to their
// right, both on one level a little below the words' middle, where the line
// joining them runs. The pictures step aside on a short page, or where a
// column beside the words would be too narrow to show them.
void HomePage::place_hero() {
    auto* backdrop = static_cast<HomeHeroBackdrop*>(hero_backdrop_);
    const auto wide = centre_->width();
    const auto middle = wide / 2.0;
    // "Welcome to ERDFlow" stands a little above the title's own lettering,
    // wherever in its box the title's lettering sits.
    const auto title = title_->geometry();
    const auto lettered = title.top() + (title.height() - title_->fontMetrics().height()) / 2;
    welcome_->setGeometry(centre_side_padding, std::max(0, lettered - 26), wide - centre_side_padding * 2, 20);
    welcome_->setVisible(!compact_);
    const auto line = card_row_->geometry();
    backdrop->setGeometry(0, 0, wide, line.bottom() + 1 + card_shadow_room);
    backdrop->lower();

    HomeHeroBackdrop::Scene scene;
    scene.hero_bottom = std::max(1, section_->geometry().top());
    for (auto* card : cards_) scene.cards.push_back(QRectF(card->geometry()).translated(line.topLeft()));
    if (!compact_) {
        const auto eyebrow = welcome_->fontMetrics().horizontalAdvance(welcome_->text());
        scene.eyebrow = QRectF(middle - eyebrow / 2.0, welcome_->y(), eyebrow, welcome_->height());
        // The words' own width, not their labels', which run the width of the page.
        const auto words = std::max({title_->sizeHint().width(), subtitle_->sizeHint().width(),
                                     static_cast<int>(eyebrow + 2 * (rule_gap + rule_reach))});
        scene.words = QRectF(middle - words / 2.0, welcome_->y(), words,
                             subtitle_->geometry().bottom() + 1 - welcome_->y());
        const auto said = subtitle_->sizeHint().width();
        scene.subtitle = QRectF(middle - said / 2.0, subtitle_->y(), said, subtitle_->height());
        // Both pictures stand on one level a little below the words' middle,
        // where the line joining them runs. Each column is as wide as the room
        // beside the words allows, up to the widest -- and no wider than keeps
        // the illustration's top on the page and both pictures' feet clear of
        // "Create a new project".
        const auto level = scene.words.center().y() + 10;
        const auto clear = section_->geometry().top() - 22.0;
        const auto database_tall = database_share / hero_aspect;
        const auto page_tall = page_share * HomeHeroBackdrop::page_height / HomeHeroBackdrop::page_width;
        const auto column = std::min({widest_wing,
                                      (wide - centre_side_padding * 2 - words) / 2.0 - wing_gutter,
                                      (level + 6) / (database_tall / 2),
                                      (clear - level) / (page_tall / 2 + 0.04),
                                      (clear - level) / (database_tall * 0.34)});
        scene.decorated = title_->isVisible() && column >= narrowest_wing;
        if (scene.decorated) {
            const auto out = words / 2.0 + wing_gutter + column / 2.0;
            scene.page_centre = {middle - out, level};
            scene.page_scale = column * page_share / HomeHeroBackdrop::page_width;
            const auto held = qRound(column * database_share);
            const auto tall = qRound(held / hero_aspect);
            hero_holder_->setGeometry(qRound(middle + out - held / 2.0), qRound(level - tall / 2.0), held, tall);
            hero_->setGeometry(hero_holder_->rect());
            const auto unit = hero_->drawing_scale();
            scene.database = QPointF(hero_holder_->pos()) + hero_->orbit_centre();
            scene.database_unit = unit;
            // The line goes in under the platform, a little in from its near
            // corner, so its end is hidden by the platform drawn over it.
            scene.landing = scene.database + QPointF(-68 * unit, 40 * unit);
            hero_holder_->raise();
        }
    }
    hero_holder_->setVisible(scene.decorated);
    backdrop->set_scene(scene);
}

// Whether the page is on one screen, and if not, what gives way. Asked once
// the centre has been given its room, which Qt does after the page itself is
// resized -- so it is asked a moment later rather than in the middle of that.
//
// Worked out rather than tried: what the page needs besides the cards is
// measured, what it would need in full is that plus the saving, and the cards'
// own height at the line's width is asked of them. So the answer is the same
// however often it is asked, and the page never flickers between the two.
void HomePage::fit() {
    auto* line = static_cast<CardLine*>(card_row_);
    const auto room = centre_scroll_->viewport()->height();
    const auto besides = centre_->layout()->minimumSize().height() - line->height();
    const auto besides_full = compact_ ? besides + spacing_saving() : besides;
    if (besides_full + line->natural_height(false) <= room) {
        set_compact(false);
        line->set_limit(0);
    } else {
        set_compact(true);
        // The cards may be as tall as the room the rest leaves them, and are
        // narrowed, keeping their shape, to be no taller. The rest is measured
        // once the page stands in its closer spacing, not estimated.
        centre_->layout()->activate();
        const auto rest = centre_->layout()->minimumSize().height() - line->height();
        line->set_limit(std::max(1, room - rest));
    }
    centre_->layout()->activate();
    place_hero();
}

void HomePage::wear(ThemeId id) {
    theme_ = id;
    const auto& t = tokens(id);
    top_bar_->wear(id);
    sidebar_->wear(id);
    learning_->wear(id);
    // The greeting keeps the height it has always had. Within it, the line
    // under the title takes what it needs and the title has the rest.
    dress(title_, t.hero_title, t.text_heading, t.family);
    dress(subtitle_, t.hero_subtitle, t.text_secondary, t.family);
    const auto greeting = title_->fontMetrics().height() + subtitle_->fontMetrics().height();
    dress(welcome_, {13, 600}, legible_on(t.primary, t.surface), t.family);
    // "Design." in the heading's navy, "Convert." in the accent and
    // "Generate." in a deeper shade of it (Zain, 2026-09-25).
    dress(title_, {31, 800}, t.text_heading, t.family);
    title_->setText(QStringLiteral("Design. <span style=\"color: %1;\">Convert.</span> "
                                   "<span style=\"color: %2;\">Generate.</span>")
                        .arg(t.primary.name(), blend(t.primary, t.text_heading, 0.28).name()));
    dress(subtitle_, {14, 400}, t.text_secondary, t.family);
    const auto one_line = subtitle_->fontMetrics().lineSpacing() + 4;
    subtitle_->setFixedHeight(one_line);
    title_->setFixedHeight(std::max(title_->fontMetrics().height(), greeting - one_line));
    dress(section_, {22, 700}, t.text_heading, t.family);
    static_cast<HomeHeroBackdrop*>(hero_backdrop_)->set_theme(id);
    for (auto* bridge : bridges_) static_cast<HomeFlowBridge*>(bridge)->wear(id);
    for (auto* card : cards_) card->wear(id);
    for (auto* demo : demos_) demo->wear(id);
    hero_->wear(id);
    centre_scroll_->setStyleSheet(QStringLiteral(
        "QScrollArea#homeCentreScroll, QWidget#homeCentre { background: %1; }").arg(t.surface.name()));
    lay_out();
    update();
}

void HomePage::lay_out() {
    const bool with_learning = width() >= learning_survives_above;
    const bool with_sidebar = width() >= sidebar_survives_above;
    const auto bar = top_bar_->sizeHint().height();
    top_bar_->setGeometry(0, 0, width(), bar);
    const auto tall = std::max(0, height() - bar);

    auto left = 0;
    sidebar_->setVisible(with_sidebar);
    if (with_sidebar) {
        sidebar_->setGeometry(0, bar, sidebar_width, tall);
        left = sidebar_width;
    }
    const auto right = with_learning ? learning_width : 0;
    learning_->setVisible(with_learning);
    if (with_learning) learning_->setGeometry(width() - learning_width, bar, learning_width, tall);
    const auto centre_width = std::max(0, width() - left - right);
    centre_scroll_->setGeometry(left, bar, centre_width, tall);
    static_cast<CardLine*>(card_row_)->set_steady_width(
        centre_width - centre_side_padding * 2
        - centre_scroll_->style()->pixelMetric(QStyle::PM_ScrollBarExtent) - 2);
    QTimer::singleShot(0, this, [this] { fit(); });
}

bool HomePage::centre_needs_scrolling() const {
    return centre_->layout()->minimumSize().height() > centre_scroll_->viewport()->height();
}

void HomePage::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    // The keyboard starts on the chosen card's + Create, so Return starts it.
    // Left to Qt it lands on whatever was made first, which is Theme, now
    // that Settings is only the sidebar's row.
    QTimer::singleShot(0, this, [this] {
        if (!isVisible()) return;
        const auto* holder = QApplication::focusWidget();
        if (holder && isAncestorOf(holder) && holder != top_bar_->theme_button()) return;
        for (auto* card : cards_)
            if (card->route() == chosen_) card->create_button()->setFocus(Qt::OtherFocusReason);
    });
}

void HomePage::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    lay_out();
}

void HomePage::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), tokens(theme_).window_background);
}

int HomePage::chosen_row() const {
    return static_cast<int>(sidebar_->selected());
}

QStringList HomePage::sidebar_labels() const { return sidebar_->labels(); }

} // namespace erdflow::desktop
