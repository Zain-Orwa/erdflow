// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/welcome_flow_illustration.hpp"

#include <QFontMetricsF>
#include <QHideEvent>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QShowEvent>

#include <algorithm>
#include <array>
#include <cmath>
#include <numeric>
#include <utility>

namespace erdflow::desktop {
namespace {
// The drawing is authored at this size and painted into whatever it is given,
// at one scale on both axes, so its proportions hold at any size. Everything
// else in this file is in these units.
constexpr double stage_width = 520;
constexpr double stage_height = 280;

// The database, and the centre every panel travels round. The database stays
// where it is; only the panels move.
constexpr double axis_x = 262;
constexpr double orbit_centre_y = 146;
constexpr double cylinder_radius = 46;
constexpr double cylinder_face = 15;
constexpr double tier_top = 92;
constexpr double tier_height = 22;
constexpr double tier_gap = 7;
constexpr int tiers = 3;
constexpr double cylinder_base = tier_top + (tiers - 1) * (tier_height + tier_gap) + tier_height;
// The platform it stands on: an upper plate, a rounded square seen from above
// and to one side, over a lower one set a little below it.
constexpr double platform_y = 184;
constexpr double platform_half = 66;
constexpr double platform_round = 16;
constexpr double platform_squash = 0.44;
constexpr double platform_thickness = 9;
// Where a line meets the platform: a ring just inside the upper plate's rim.
constexpr double platform_rim_x = 80;
constexpr double platform_rim_y = 34;
// The one orbit the product's panels share: an ellipse, wider than it is
// tall, because the drawing is (Zain, 2026-09-24).
constexpr double orbit_x = 178;
constexpr double orbit_y = 80;

// How often the picture is drawn while it moves: about thirty times a second,
// smooth enough for a slow orbit and no dearer than it needs to be.
constexpr int frame_ms = 33;
// How much the lines' ink and the database's size breathe, as a fraction of
// their resting value, and how many times a revolution. A mood, not a message.
constexpr double connector_pulse_amount = 0.08;
constexpr double connector_pulse_cycles = 2.0;
constexpr double breathe_amount = 0.018;
constexpr double breathe_cycles = 1.0;
// A panel at the back of the orbit is smaller and fainter than one at the
// front: enough to say "further away", never enough to read as a carousel.
constexpr double far_scale = 0.92;
constexpr double far_opacity = 0.78;
// How far a panel at the side leans to face the database, as a card stood on
// the floor and turned towards the middle would.
constexpr double side_lean = 0.13;
// A line leaves its panel and enters the platform by short straight runs of the
// same length, pointing the same way, with the two bends between them (Zain,
// 2026-09-24). Every line uses the same length, whichever panel it belongs to.
constexpr double connector_run = 26;
// No run is more than a third of the distance, so the middle segment is always
// a slant and never folds back on itself when a panel comes close.
constexpr double connector_run_share = 1.0 / 3.0;
// The bends are rounded slightly: small corners, not a curve.
constexpr double connector_corner = 4;

QColor mix(const QColor& colour, const QColor& with, double share) {
    const auto keep = 1.0 - share;
    return QColor::fromRgbF(static_cast<float>(colour.redF() * keep + with.redF() * share),
                            static_cast<float>(colour.greenF() * keep + with.greenF() * share),
                            static_cast<float>(colour.blueF() * keep + with.blueF() * share));
}

QColor with_alpha(QColor colour, double alpha) {
    colour.setAlphaF(static_cast<float>(std::clamp(alpha, 0.0, 1.0)));
    return colour;
}

double smooth(double from, double to, double x) {
    const auto t = std::clamp((x - from) / (to - from), 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

// Whether a point, in authored units, lies inside the database's outline as
// seen from the front, including its top and its foot. A line drawn there is
// hidden, because the database is painted over the lines.
bool behind_database(QPointF p) {
    const auto across = (p.x() - axis_x) / cylinder_radius;
    if (std::abs(across) > 1.0) return false;
    const auto cap = cylinder_face * std::sqrt(1.0 - across * across);
    return p.y() >= tier_top - cap && p.y() <= cylinder_base + cap;
}

HeroOrbitItem panel(const char* id, HeroIcon icon, const char* title, const QColor& accent,
                    double phase, QSizeF size, bool labelled) {
    HeroOrbitItem item;
    item.id = QString::fromLatin1(id);
    item.icon = icon;
    item.title = QString::fromLatin1(title);
    item.accent = accent;
    item.orbit_phase = phase;
    item.size = size;
    item.labelled = labelled;
    return item;
}
} // namespace

WelcomeFlowIllustration::WelcomeFlowIllustration(QWidget* parent) : QWidget(parent) {
    setObjectName("welcomeFlow");
    setMinimumSize(300, 162);
    setAccessibleName("ERDFlow");
    setAccessibleDescription("A database with the stages of the work travelling round it: "
                             "conceptual model, relationships, relational design and SQL.");
    // Mouse tracking, so a panel knows when the pointer is over it and can
    // show it. It keeps moving: nothing but hiding or stillness stops it.
    setMouseTracking(true);
    moving_ = !platform_prefers_stillness();
    clock_ = new QTimer(this);
    clock_->setInterval(frame_ms);
    connect(clock_, &QTimer::timeout, this, [this] {
        seconds_ = seconds_at_start_ + static_cast<double>(since_.elapsed()) / 1000.0;
        update();
    });
    show_items({});
}

bool WelcomeFlowIllustration::platform_prefers_stillness() {
    // No Qt version this project builds against exposes one cross-platform
    // "prefers reduced motion" hint -- macOS, Windows and Linux each keep the
    // setting differently, and reading it natively is a platform call this
    // project has no seam for yet. Until one is wired in behind it, this
    // environment variable is that seam: a person, a test, or later a native
    // query all say the same thing through it.
    const auto asked = qgetenv("ERDFLOW_REDUCED_MOTION");
    return asked == "1" || asked.toLower() == "true";
}

void WelcomeFlowIllustration::set_moving(bool on) {
    if (moving_ == on) return;
    moving_ = on;
    if (moving_) {
        if (isVisible()) {
            seconds_at_start_ = seconds_;
            since_.start();
            clock_->start();
        }
    } else {
        clock_->stop();
        // Turning motion off is turning it fully off: every panel goes back to
        // where it starts rather than freezing mid-way, so a still picture and
        // a moving one never disagree about the composition.
        seconds_ = 0.0;
        update();
    }
}

void WelcomeFlowIllustration::set_clock(double seconds) {
    seconds_ = seconds;
    seconds_at_start_ = seconds;
    if (since_.isValid()) since_.restart();
    update();
}

void WelcomeFlowIllustration::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);
    if (moving_ && !clock_->isActive()) {
        // Picked up from where it was when it was last hidden, not from where
        // it would have been had it kept going while nobody could see it.
        seconds_at_start_ = seconds_;
        since_.start();
        clock_->start();
    }
}

void WelcomeFlowIllustration::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    clock_->stop();
}

void WelcomeFlowIllustration::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    backdrop_ = {};
}

int WelcomeFlowIllustration::card_at(QPointF at) const {
    // Front-most first, so where two panels overlap the one in front answers.
    std::vector<std::size_t> order(items_.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(),
              [this](std::size_t a, std::size_t b) { return card_depth(a) > card_depth(b); });
    for (const auto i : order) {
        if (!items_[i].visible) continue;
        if (card_outline(i).containsPoint(at, Qt::OddEvenFill)) return static_cast<int>(i);
    }
    return -1;
}

void WelcomeFlowIllustration::mouseMoveEvent(QMouseEvent* event) {
    QWidget::mouseMoveEvent(event);
    const auto found = card_at(event->position());
    if (found != under_pointer_) {
        under_pointer_ = found;
        // Only a panel that does something on a press is shown as pressable.
        if (found >= 0 && items_[static_cast<std::size_t>(found)].on_press)
            setCursor(Qt::PointingHandCursor);
        else
            unsetCursor();
        update();
    }
}

void WelcomeFlowIllustration::mousePressEvent(QMouseEvent* event) {
    const auto found = card_at(event->position());
    pressed_ = event->button() == Qt::LeftButton && found >= 0
                   && items_[static_cast<std::size_t>(found)].on_press
               ? found : -1;
    if (pressed_ < 0) QWidget::mousePressEvent(event);
}

void WelcomeFlowIllustration::mouseReleaseEvent(QMouseEvent* event) {
    const auto pressed = std::exchange(pressed_, -1);
    if (pressed < 0) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    if (event->button() != Qt::LeftButton || card_at(event->position()) != pressed) return;
    // A copy, so whatever it does -- showing other panels, say -- cannot pull
    // it out from under itself.
    const auto act = items_[static_cast<std::size_t>(pressed)].on_press;
    if (act) act();
}

void WelcomeFlowIllustration::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    under_pointer_ = -1;
    unsetCursor();
    update();
}

QTransform WelcomeFlowIllustration::stage_transform() const {
    const auto scale = drawing_scale();
    QTransform stage;
    stage.translate((width() - stage_width * scale) / 2, (height() - stage_height * scale) / 2);
    stage.scale(scale, scale);
    return stage;
}

double WelcomeFlowIllustration::drawing_scale() const {
    // One scale for both axes, whichever the room allows: extra room in one
    // direction is left empty rather than stretching the drawing into it.
    return std::min(width() / stage_width, height() / stage_height);
}

QPointF WelcomeFlowIllustration::orbit_centre() const {
    return stage_transform().map(QPointF(axis_x, orbit_centre_y));
}

double WelcomeFlowIllustration::radius_x(std::size_t which) const {
    return items_[which].orbit_radius_x > 0 ? items_[which].orbit_radius_x : orbit_x;
}

double WelcomeFlowIllustration::radius_y(std::size_t which) const {
    return items_[which].orbit_radius_y > 0 ? items_[which].orbit_radius_y : orbit_y;
}

QSizeF WelcomeFlowIllustration::orbit_radii(std::size_t which) const {
    if (which >= items_.size()) return {};
    return {radius_x(which), radius_y(which)};
}

double WelcomeFlowIllustration::angle_of_card(std::size_t which) const {
    if (which >= items_.size()) return 0.0;
    // A constant speed all the way round: a panel does not ease in and out of
    // an orbit, it travels it.
    const auto travelled = moving_ ? 360.0 * seconds_ / revolution_seconds : 0.0;
    return std::fmod(items_[which].orbit_phase * 360.0 + travelled, 360.0);
}

double WelcomeFlowIllustration::radians_of(std::size_t which) const {
    return angle_of_card(which) * M_PI / 180.0;
}

QPointF WelcomeFlowIllustration::authored_centre(std::size_t which) const {
    const auto angle = radians_of(which);
    return {axis_x + radius_x(which) * std::cos(angle),
            orbit_centre_y + radius_y(which) * std::sin(angle)};
}

double WelcomeFlowIllustration::card_depth(std::size_t which) const {
    if (which >= items_.size()) return 0.0;
    // Below the centre on screen is nearer; above it is further away.
    return (1.0 + std::sin(radians_of(which))) / 2.0;
}

bool WelcomeFlowIllustration::card_behind(std::size_t which) const {
    return which < items_.size() && std::sin(radians_of(which)) < 0.0;
}

double WelcomeFlowIllustration::lean_of(std::size_t which) const {
    // Leaning towards the middle: the most at either side, none straight in
    // front or straight behind. Only ever a lean -- the panel stays upright and
    // never turns to face along its path.
    return side_lean * std::cos(radians_of(which));
}

double WelcomeFlowIllustration::scale_of(std::size_t which) const {
    return far_scale + (1.0 - far_scale) * card_depth(which);
}

QTransform WelcomeFlowIllustration::card_transform(std::size_t which) const {
    const auto at = authored_centre(which);
    const auto scale = scale_of(which);
    QTransform local;
    local.translate(at.x(), at.y());
    local.scale(scale, scale);
    local.shear(0, lean_of(which));
    return local * stage_transform();
}

QPointF WelcomeFlowIllustration::card_centre(std::size_t which) const {
    if (which >= items_.size()) return {};
    return stage_transform().map(authored_centre(which));
}

QPolygonF WelcomeFlowIllustration::card_outline(std::size_t which) const {
    if (which >= items_.size()) return {};
    const auto& size = items_[which].size;
    return card_transform(which).map(
        QPolygonF(QRectF(-size.width() / 2, -size.height() / 2, size.width(), size.height())));
}

QPointF WelcomeFlowIllustration::platform_point_towards(std::size_t which) const {
    const auto at = authored_centre(which);
    const auto angle = std::atan2((at.y() - platform_y) / platform_rim_y,
                                  (at.x() - axis_x) / platform_rim_x);
    return {axis_x + platform_rim_x * std::cos(angle), platform_y + platform_rim_y * std::sin(angle)};
}

WelcomeFlowIllustration::Anchor WelcomeFlowIllustration::anchor_of(std::size_t which) const {
    // Where the line leaves the panel: the point on its edge that faces where
    // the line is going. A panel beside the database is left from its inner
    // side, one above it from its foot, one below it from its top -- and as
    // the panel travels, the point slides round the edge with it rather than
    // jumping from one edge to the next.
    const auto& size = items_[which].size;
    const auto at = authored_centre(which);
    const auto to = platform_point_towards(which);
    const auto scale = scale_of(which);
    const auto lean = lean_of(which);
    // The direction, taken back into the panel's own upright space.
    const auto dx = (to.x() - at.x()) / scale;
    const auto dy = (to.y() - at.y()) / scale - lean * dx;
    const auto half_w = size.width() / 2;
    const auto half_h = size.height() / 2;
    const auto across = std::abs(dx) < 1e-9 ? 1e9 : half_w / std::abs(dx);
    const auto down = std::abs(dy) < 1e-9 ? 1e9 : half_h / std::abs(dy);
    const auto reach = std::min(across, down);
    // Near a corner the line could go either way; how sideways it goes turns
    // gradually from one to the other so its bend never snaps.
    const auto sideways = 1.0 - smooth(0.75, 1.33, across / down);
    return {QPointF(dx * reach, dy * reach), sideways};
}

QPointF WelcomeFlowIllustration::connector_start(std::size_t which) const {
    if (which >= items_.size()) return {};
    return connector(which).back();
}

double WelcomeFlowIllustration::hidden_length(QPointF to, QPointF unit, double limit) const {
    // How far back from `to`, against `unit`, the line stays behind the
    // database: the part of the last run nobody sees. The outline is convex,
    // so the line leaves it once and does not come back in. That point is
    // found by halving the interval.
    const auto authored = stage_transform().inverted();
    const auto hidden = [&](double back) { return behind_database(authored.map(to - unit * back)); };
    if (limit <= 0.0 || !hidden(0.0)) return 0.0;
    if (hidden(limit)) return limit;
    double in = 0.0;
    double out = limit;
    for (int step = 0; step < 30; ++step) {
        const auto middle = (in + out) / 2;
        (hidden(middle) ? in : out) = middle;
    }
    return (in + out) / 2;
}

QPointF WelcomeFlowIllustration::connector_end(std::size_t which) const {
    if (which >= items_.size()) return {};
    return card_transform(which).map(anchor_of(which).at);
}

QPolygonF WelcomeFlowIllustration::connector(std::size_t which) const {
    if (which >= items_.size()) return {};
    // Straight, with two bends. The line leaves the panel the way that panel
    // faces: across from a panel at the side, down or up from one behind or in
    // front. It then slants over towards the platform and enters it running
    // the same way again, for the same distance it ran at the start. Both ends
    // match, so all four lines have one shape wherever their panels are.
    const auto anchor = anchor_of(which);
    const auto from = card_transform(which).map(anchor.at);
    const auto to = stage_transform().map(platform_point_towards(which));
    // Which way the two runs point. It is taken from the side of the panel the
    // line leaves by and blends across a corner as `sideways` does, so it
    // turns gradually and never flips. Where a sign changes, its share is
    // already nought.
    const QPointF way(anchor.sideways * (anchor.at.x() < 0 ? -1.0 : 1.0),
                      (1.0 - anchor.sideways) * (anchor.at.y() < 0 ? -1.0 : 1.0));
    const auto unit = way / std::hypot(way.x(), way.y());
    // How far the platform lies in that direction. Where the panel covers the
    // point the line ends on, that distance is nought and so are the runs: the
    // line lies under the panel, hidden, rather than hooking out and back.
    const auto ahead = QPointF::dotProduct(to - from, unit);
    // For a panel behind, the platform point is often behind the database too.
    // The line then ends where it goes behind the database, so the two runs
    // you can see are equal and the line appears to plug into its side.
    // Where there is no room for that, the line stays where it was, hidden.
    // This matters only for runs that go across. As a panel moves round
    // behind and its runs turn to go down, the adjustment fades with them.
    // Otherwise the hidden part would be measured up through the whole height
    // of the database, and the line's end would race up its side.
    const auto hidden = hidden_length(to, unit, ahead) * anchor.sideways;
    const auto run = std::clamp((ahead - hidden) * connector_run_share, 0.0,
                                connector_run * drawing_scale());
    const auto end = to - unit * std::clamp(std::min(hidden, ahead - 2 * run), 0.0, hidden);
    return QPolygonF{} << from << from + unit * run << end - unit * run << end;
}

QSize WelcomeFlowIllustration::sizeHint() const {
    return {static_cast<int>(stage_width * 0.82), static_cast<int>(stage_height * 0.82)};
}

QColor WelcomeFlowIllustration::hero_blue() const {
    // The reference's database blue for Azure; any other theme's own primary,
    // so the drawing belongs to whichever palette is on.
    return theme_ == ThemeId::Azure ? QColor("#3B82F6") : tokens(theme_).primary;
}

std::vector<HeroOrbitItem> WelcomeFlowIllustration::product_cards(const Tokens& t) {
    // Four panels a quarter-turn apart, read by their marks: the conceptual
    // model's structure, the relationships in it, the relational design it
    // becomes, and its SQL. They start with the two blue ones behind the
    // database and the hierarchy and the SQL page in front at either side,
    // in the order the reference reads left to right. Their titles are what a
    // screen reader says.
    return {
        panel("conceptual", HeroIcon::Structure, "Conceptual ERD", t.primary, 0.375,
              {78, 96}, false),
        panel("relationships", HeroIcon::Conceptual, "Relationships", t.primary, 0.625,
              {78, 92}, false),
        panel("relational", HeroIcon::Relational, "Relational Design", t.primary, 0.875,
              {74, 90}, false),
        panel("sql", HeroIcon::Sql, "SQL", t.gold, 0.125, {80, 100}, false),
    };
}

std::vector<HeroOrbitItem> WelcomeFlowIllustration::marketing_cards(const Tokens& t) {
    // The same drawing saying something else, in words. Proof that the
    // component is a component: nothing about the orbit, the database or the
    // lines knows what the panels are called.
    return {
        panel("design", HeroIcon::Conceptual, "Design", t.primary, 0.42, {84, 100}, true),
        panel("convert", HeroIcon::Relational, "Convert", t.primary, 0.75, {80, 98}, true),
        panel("generate", HeroIcon::Sql, "Generate", t.gold, 0.08, {84, 104}, true),
    };
}

void WelcomeFlowIllustration::show_items(std::vector<HeroOrbitItem> items) {
    items_ = items.empty() ? product_cards(tokens(theme_)) : std::move(items);
    // A caller who gave no starting places gets its panels spread evenly
    // round the orbit, starting from the left, so none sits on another.
    const bool placed = std::any_of(items_.begin(), items_.end(),
                                    [](const HeroOrbitItem& item) { return item.orbit_phase != 0.0; });
    if (!placed && items_.size() > 0)
        for (std::size_t i = 0; i < items_.size(); ++i)
            items_[i].orbit_phase = std::fmod(0.5 + static_cast<double>(i) / static_cast<double>(items_.size()), 1.0);
    under_pointer_ = -1;
    pressed_ = -1;
    unsetCursor();
    update();
}

void WelcomeFlowIllustration::wear(ThemeId id) {
    const bool was_product = !items_.empty() && items_.front().id == "conceptual"
                          && items_.size() == product_cards(tokens(theme_)).size();
    theme_ = id;
    // A caller's own panels keep the accents that caller chose; the ready-made
    // set follows the theme, because it is the product's own and the product
    // is what changed.
    if (was_product) items_ = product_cards(tokens(id));
    backdrop_ = {};
    update();
}

void WelcomeFlowIllustration::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    // Back to front: what does not move, the lines, the panels behind the
    // database, the database, and the panels in front of it -- so a panel
    // travelling round the back passes behind the database and comes out in
    // front of it on the other side.
    painter.drawPixmap(0, 0, backdrop());
    draw_connectors(painter);
    std::vector<std::size_t> order(items_.size());
    std::iota(order.begin(), order.end(), std::size_t{0});
    std::sort(order.begin(), order.end(),
              [this](std::size_t a, std::size_t b) { return card_depth(a) < card_depth(b); });
    for (const auto i : order)
        if (items_[i].visible && card_behind(i)) draw_card(painter, i);
    draw_database(painter);
    for (const auto i : order)
        if (items_[i].visible && !card_behind(i)) draw_card(painter, i);
}

const QPixmap& WelcomeFlowIllustration::backdrop() const {
    const auto ratio = devicePixelRatioF();
    if (backdrop_.isNull() || backdrop_size_ != size() || backdrop_theme_ != theme_
        || backdrop_ratio_ != ratio) {
        backdrop_ = QPixmap(size() * ratio);
        backdrop_.setDevicePixelRatio(ratio);
        backdrop_.fill(Qt::transparent);
        QPainter painter(&backdrop_);
        painter.setRenderHint(QPainter::Antialiasing, true);
        draw_backdrop(painter);
        backdrop_size_ = size();
        backdrop_theme_ = theme_;
        backdrop_ratio_ = ratio;
    }
    return backdrop_;
}

void WelcomeFlowIllustration::draw_backdrop(QPainter& painter) const {
    const auto& t = tokens(theme_);
    const auto blue = hero_blue();
    painter.save();
    painter.setTransform(stage_transform());
    painter.setPen(Qt::NoPen);

    // A soft blue-white cloud behind everything, with no edge of its own, so
    // the drawing sits in the page rather than on it.
    painter.save();
    painter.translate(axis_x, orbit_centre_y);
    painter.scale(1.0, 0.54);
    QRadialGradient cloud(QPointF(0, 0), 256);
    cloud.setColorAt(0.0, t.primary_soft);
    cloud.setColorAt(0.55, with_alpha(t.primary_soft, 0.55));
    cloud.setColorAt(1.0, with_alpha(t.surface, 0.0));
    painter.setBrush(cloud);
    painter.drawEllipse(QPointF(0, 0), 256, 256);
    painter.restore();

    // The soft light the platform casts on the page beneath it.
    painter.save();
    painter.translate(axis_x, platform_y + 14);
    painter.scale(1.0, 0.36);
    QRadialGradient under(QPointF(0, 0), 132);
    under.setColorAt(0.0, with_alpha(blue, 0.20));
    under.setColorAt(1.0, with_alpha(blue, 0.0));
    painter.setBrush(under);
    painter.drawEllipse(QPointF(0, 0), 132, 132);
    painter.restore();

    // The platform: an upper plate over a lower one set a little below it.
    QPainterPath square;
    square.addRoundedRect(QRectF(-platform_half, -platform_half, platform_half * 2, platform_half * 2),
                          platform_round, platform_round);
    QTransform seen;
    seen.translate(axis_x, platform_y);
    seen.scale(1.0, platform_squash);
    seen.rotate(45);
    const auto top = seen.map(square);
    painter.setBrush(mix(blue, Qt::white, 0.70));
    painter.drawPath(top.translated(0, platform_thickness));
    QLinearGradient face(QPointF(0, platform_y - 42), QPointF(0, platform_y + 42));
    face.setColorAt(0.0, mix(blue, Qt::white, 0.95));
    face.setColorAt(1.0, mix(blue, Qt::white, 0.83));
    painter.setBrush(face);
    painter.setPen(QPen(with_alpha(Qt::white, 0.9), 1.3));
    painter.drawPath(top);
    // An inner rim, which is what makes it read as a tray rather than a tile.
    QPainterPath inner;
    inner.addRoundedRect(QRectF(-platform_half + 11, -platform_half + 11,
                                (platform_half - 11) * 2, (platform_half - 11) * 2),
                         platform_round - 5, platform_round - 5);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(mix(blue, Qt::white, 0.78), 1.0));
    painter.drawPath(seen.map(inner));
    painter.restore();
}

void WelcomeFlowIllustration::draw_connectors(QPainter& painter) const {
    // A line from each panel into the platform: three straight segments and
    // two bends, fixed to the panel at one end and worked out afresh every
    // frame, so as the panel travels round the line goes with it. A faint
    // pulse in the ink says something flows.
    const auto blue = hero_blue();
    const auto turned = std::fmod(seconds_ / revolution_seconds, 1.0);
    const auto pulse = moving_
        ? connector_pulse_amount * std::sin(turned * 2.0 * M_PI * connector_pulse_cycles)
        : 0.0;
    const auto base = theme_ == ThemeId::Azure ? QColor("#79BDF2") : mix(blue, Qt::white, 0.42);
    const auto scale = drawing_scale();
    painter.save();
    painter.setPen(QPen(with_alpha(base, 0.9 + pulse), std::clamp(2.2 * scale, 2.0, 2.4),
                        Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    // Each bend is rounded a little. The line stays straight segments, only
    // without sharp corners. A short segment gets a smaller corner, so a
    // rounding never uses up more than half of either segment beside it.
    const auto corner = connector_corner * scale;
    std::vector<QPolygonF> lines;
    for (std::size_t i = 0; i < items_.size(); ++i)
        if (items_[i].visible) lines.push_back(connector(i));
    for (const auto& line : lines) {
        QPainterPath path(line.front());
        for (qsizetype k = 1; k + 1 < line.size(); ++k) {
            const auto back = line[k - 1] - line[k];
            const auto on = line[k + 1] - line[k];
            const auto back_length = std::hypot(back.x(), back.y());
            const auto on_length = std::hypot(on.x(), on.y());
            const auto r = std::min({corner, back_length / 2, on_length / 2});
            if (r < 0.01) {
                path.lineTo(line[k]);
                continue;
            }
            path.lineTo(line[k] + back * (r / back_length));
            path.quadTo(line[k], line[k] + on * (r / on_length));
        }
        path.lineTo(line.back());
        painter.drawPath(path);
    }
    // Where each line meets the platform, a small bead, as a plug would have.
    painter.setPen(Qt::NoPen);
    painter.setBrush(mix(blue, Qt::white, 0.3));
    for (const auto& line : lines) painter.drawEllipse(line.back(), 2.4 * scale, 2.4 * scale);
    painter.restore();
}

void WelcomeFlowIllustration::draw_database(QPainter& painter) const {
    const auto blue = hero_blue();
    painter.save();
    painter.setTransform(stage_transform());
    // A very slight breathing in the cylinder's size, about its foot so it
    // stays standing on the platform -- alive without being read as moving.
    const auto turned = std::fmod(seconds_ / revolution_seconds, 1.0);
    const auto breathe = moving_
        ? 1.0 + breathe_amount * std::sin(turned * 2.0 * M_PI * breathe_cycles)
        : 1.0;
    painter.translate(axis_x, cylinder_base);
    painter.scale(breathe, breathe);
    painter.translate(-axis_x, -cylinder_base);

    // Its shadow on the platform.
    painter.save();
    painter.translate(axis_x, cylinder_base + 2);
    painter.scale(1.0, 0.34);
    QRadialGradient shade(QPointF(0, 0), cylinder_radius * 1.25);
    shade.setColorAt(0.0, with_alpha(blue.darker(140), 0.22));
    shade.setColorAt(1.0, with_alpha(blue, 0.0));
    painter.setPen(Qt::NoPen);
    painter.setBrush(shade);
    painter.drawEllipse(QPointF(0, 0), cylinder_radius * 1.25, cylinder_radius * 1.25);
    painter.restore();

    // Three tiers, drawn from the bottom up so each sits over the one beneath,
    // with a gap between them where the lower tier's top shows: that is what
    // makes a stack of discs read as a database.
    const auto left = axis_x - cylinder_radius;
    const auto right = axis_x + cylinder_radius;
    for (int k = tiers - 1; k >= 0; --k) {
        const auto y = tier_top + k * (tier_height + tier_gap);
        QPainterPath side;
        side.moveTo(left, y);
        side.lineTo(left, y + tier_height);
        side.arcTo(QRectF(left, y + tier_height - cylinder_face, cylinder_radius * 2, cylinder_face * 2),
                   180, 180);
        side.lineTo(right, y);
        side.closeSubpath();
        QLinearGradient round(QPointF(left, 0), QPointF(right, 0));
        round.setColorAt(0.0, mix(blue, Qt::white, 0.34));
        round.setColorAt(0.32, mix(blue, Qt::white, 0.48));
        round.setColorAt(0.68, mix(blue, Qt::white, 0.14));
        round.setColorAt(1.0, blue.darker(112));
        painter.setPen(Qt::NoPen);
        painter.setBrush(round);
        painter.drawPath(side);
        QLinearGradient lid(QPointF(0, y - cylinder_face), QPointF(0, y + cylinder_face));
        lid.setColorAt(0.0, mix(blue, Qt::white, 0.90));
        lid.setColorAt(1.0, mix(blue, Qt::white, 0.58));
        painter.setBrush(lid);
        painter.setPen(QPen(mix(blue, Qt::white, 0.94), 1.2));
        painter.drawEllipse(QPointF(axis_x, y), cylinder_radius, cylinder_face);
    }
    painter.restore();
}

void WelcomeFlowIllustration::draw_card(QPainter& painter, std::size_t which) const {
    const auto& t = tokens(theme_);
    const auto& item = items_[which];
    const auto blue = hero_blue();
    const bool warm = item.icon == HeroIcon::Sql || item.accent == t.gold;
    const bool filled = !warm && item.icon == HeroIcon::Relational;
    const bool pointed_at = static_cast<int>(which) == under_pointer_;
    const QRectF card(-item.size.width() / 2, -item.size.height() / 2, item.size.width(),
                      item.size.height());
    const auto radius = 9.0;

    painter.save();
    painter.setTransform(card_transform(which));
    const auto depth = card_depth(which);
    painter.setOpacity(far_opacity + (1.0 - far_opacity) * depth);

    // A faint shadow, a few widening washes rather than a blur.
    for (int step = 3; step >= 1; --step) {
        QPainterPath under;
        under.addRoundedRect(card.adjusted(-step * 0.5, step * 1.6, step * 0.5, step * 2.2),
                             radius + step, radius + step);
        painter.fillPath(under, with_alpha(blue.darker(150), 0.035));
    }
    QPainterPath shape;
    shape.addRoundedRect(card, radius, radius);
    QLinearGradient fill(card.topLeft(), card.bottomLeft());
    QColor edge;
    if (warm) {
        fill.setColorAt(0.0, mix(t.gold, Qt::white, 0.84));
        fill.setColorAt(1.0, mix(t.gold, Qt::white, 0.64));
        edge = mix(t.gold, Qt::white, 0.30);
    } else if (filled) {
        fill.setColorAt(0.0, mix(blue, Qt::white, 0.36));
        fill.setColorAt(1.0, mix(blue, Qt::white, 0.10));
        edge = mix(blue, Qt::white, 0.22);
    } else {
        fill.setColorAt(0.0, with_alpha(mix(blue, Qt::white, 0.965), 0.96));
        fill.setColorAt(1.0, with_alpha(mix(blue, Qt::white, 0.885), 0.96));
        edge = mix(blue, Qt::white, 0.70);
    }
    painter.fillPath(shape, fill);
    // Pointed at, the border comes up a little: a highlight, and nothing
    // else. The panel keeps travelling.
    painter.setPen(QPen(pointed_at ? mix(blue, Qt::white, filled ? 0.0 : 0.25) : edge,
                        pointed_at ? 1.8 : 1.1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);
    // A catch of light along the top edge, which is most of what makes it
    // read as a thing standing in the light rather than a flat rectangle.
    painter.setPen(QPen(with_alpha(Qt::white, filled ? 0.45 : 0.85), 1.0));
    painter.drawLine(QPointF(card.left() + radius, card.top() + 1.2),
                     QPointF(card.right() - radius, card.top() + 1.2));

    if (item.draw) {
        // A caller's own content, kept inside the panel.
        painter.save();
        painter.setClipPath(shape, Qt::IntersectClip);
        item.draw(painter, card);
        painter.restore();
    } else {
        draw_mark(painter, item, card);
    }

    if (item.labelled) {
        auto lettering = font();
        lettering.setFamilies(t.family);
        lettering.setPixelSize(std::max(1, static_cast<int>(std::round(item.size.width() * 0.15))));
        lettering.setWeight(QFont::DemiBold);
        painter.setFont(lettering);
        painter.setPen(filled ? QColor(Qt::white) : warm ? QColor("#8A5B00") : t.text_secondary);
        painter.drawText(QRectF(card.left(), card.bottom() - item.size.height() * 0.3,
                                card.width(), item.size.height() * 0.2),
                         Qt::AlignCenter, item.title);
        if (!item.subtitle.isEmpty()) {
            lettering.setPixelSize(std::max(1, static_cast<int>(std::round(item.size.width() * 0.11))));
            lettering.setWeight(QFont::Normal);
            painter.setFont(lettering);
            painter.setPen(filled ? QColor(Qt::white) : t.text_muted);
            painter.drawText(QRectF(card.left(), card.bottom() - item.size.height() * 0.12,
                                    card.width(), item.size.height() * 0.1),
                             Qt::AlignCenter, item.subtitle);
        }
    }
    painter.restore();
}

void WelcomeFlowIllustration::draw_mark(QPainter& painter, const HeroOrbitItem& item,
                                        const QRectF& card) const {
    const auto& t = tokens(theme_);
    const auto blue = hero_blue();
    const auto u = card.width() * 0.155;
    // The mark sits in the upper part of the panel, higher where there are
    // words to leave room for.
    const auto my = card.center().y() - card.height() * (item.labelled ? 0.14 : 0.04);
    const auto cx = card.center().x();
    switch (item.icon) {
    case HeroIcon::Conceptual: {
        // An entity, a relationship and an entity. The gold is small on
        // purpose: the one warm note in a blue drawing.
        painter.setPen(QPen(t.text_muted, 1.0));
        painter.drawLine(QPointF(cx - 1.0 * u, my), QPointF(cx - 0.72 * u, my));
        painter.drawLine(QPointF(cx + 0.72 * u, my), QPointF(cx + 1.0 * u, my));
        painter.setPen(QPen(blue, 1.2));
        painter.setBrush(mix(blue, Qt::white, 0.82));
        painter.drawRoundedRect(QRectF(cx - 2.45 * u, my - 0.55 * u, 1.45 * u, 1.1 * u), 2.5, 2.5);
        painter.drawRoundedRect(QRectF(cx + 1.0 * u, my - 0.55 * u, 1.45 * u, 1.1 * u), 2.5, 2.5);
        painter.setPen(QPen(t.gold, 1.4));
        painter.setBrush(t.gold_soft);
        QPolygonF diamond;
        diamond << QPointF(cx, my - 0.72 * u) << QPointF(cx + 0.72 * u, my)
                << QPointF(cx, my + 0.72 * u) << QPointF(cx - 0.72 * u, my);
        painter.drawPolygon(diamond);
        break;
    }
    case HeroIcon::Structure: {
        // A small structure of boxes, one over two, joined by elbows: the
        // shape of a model before it has names.
        const auto line = mix(blue, Qt::white, 0.35);
        painter.setPen(QPen(line, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.setBrush(Qt::NoBrush);
        QPainterPath joins;
        joins.moveTo(cx, my - 0.9 * u);
        joins.lineTo(cx, my - 0.2 * u);
        joins.moveTo(cx - 1.35 * u, my + 0.45 * u);
        joins.lineTo(cx - 1.35 * u, my - 0.2 * u);
        joins.lineTo(cx + 1.35 * u, my - 0.2 * u);
        joins.lineTo(cx + 1.35 * u, my + 0.45 * u);
        joins.moveTo(cx - 1.35 * u, my + 1.35 * u);
        joins.lineTo(cx - 1.35 * u, my + 1.75 * u);
        joins.lineTo(cx - 0.35 * u, my + 1.75 * u);
        painter.drawPath(joins);
        painter.setPen(QPen(mix(blue, Qt::white, 0.1), 1.0));
        painter.setBrush(mix(blue, Qt::white, 0.3));
        painter.drawRoundedRect(QRectF(cx - 0.75 * u, my - 1.75 * u, 1.5 * u, 0.9 * u), 2, 2);
        painter.drawRoundedRect(QRectF(cx - 2.1 * u, my + 0.45 * u, 1.5 * u, 0.9 * u), 2, 2);
        painter.drawRoundedRect(QRectF(cx + 0.6 * u, my + 0.45 * u, 1.5 * u, 0.9 * u), 2, 2);
        painter.setBrush(mix(blue, Qt::white, 0.62));
        painter.drawRoundedRect(QRectF(cx - 0.35 * u, my + 1.35 * u, 1.3 * u, 0.8 * u), 2, 2);
        break;
    }
    case HeroIcon::Relational: {
        // A table, drawn in white on the blue panel: a header row and cells.
        const QRectF grid(cx - 2.0 * u, my - 1.55 * u, 4.0 * u, 3.1 * u);
        painter.setPen(Qt::NoPen);
        painter.setBrush(with_alpha(Qt::white, 0.22));
        painter.drawRoundedRect(grid, 3, 3);
        painter.setBrush(with_alpha(Qt::white, 0.92));
        QPainterPath header;
        header.addRoundedRect(QRectF(grid.left(), grid.top(), grid.width(), 0.8 * u), 3, 3);
        painter.drawPath(header);
        painter.setPen(QPen(with_alpha(Qt::white, 0.92), 1.2));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(grid, 3, 3);
        painter.setPen(QPen(with_alpha(Qt::white, 0.75), 1.0));
        painter.drawLine(QPointF(grid.left() + grid.width() * 0.42, grid.top() + 0.8 * u),
                         QPointF(grid.left() + grid.width() * 0.42, grid.bottom()));
        for (const auto row : {1.55, 2.3})
            painter.drawLine(QPointF(grid.left(), grid.top() + row * u),
                             QPointF(grid.right(), grid.top() + row * u));
        break;
    }
    case HeroIcon::Sql: {
        // A page with its corner turned, and the word under it: the one mark
        // that is a word, because SQL is one.
        const auto top = my - (item.labelled ? 1.9 : 2.3) * u;
        const auto page_left = cx - 1.35 * u;
        const auto page_wide = 2.7 * u;
        const auto page_tall = 3.0 * u;
        const auto fold = 0.8 * u;
        QPainterPath page;
        page.moveTo(page_left, top);
        page.lineTo(page_left + page_wide - fold, top);
        page.lineTo(page_left + page_wide, top + fold);
        page.lineTo(page_left + page_wide, top + page_tall);
        page.lineTo(page_left, top + page_tall);
        page.closeSubpath();
        painter.setPen(QPen(t.gold.darker(112), 1.1));
        painter.setBrush(mix(t.gold, Qt::white, 0.08));
        painter.drawPath(page);
        QPainterPath corner;
        corner.moveTo(page_left + page_wide - fold, top);
        corner.lineTo(page_left + page_wide - fold, top + fold);
        corner.lineTo(page_left + page_wide, top + fold);
        painter.setBrush(mix(t.gold, Qt::white, 0.55));
        painter.drawPath(corner);
        painter.setPen(QPen(with_alpha(Qt::white, 0.9), 1.3, Qt::SolidLine, Qt::RoundCap));
        for (const auto row : {1.35, 1.85, 2.35})
            painter.drawLine(QPointF(page_left + 0.45 * u, top + row * u),
                             QPointF(page_left + page_wide - (row < 1.5 ? 1.1 : 0.45) * u,
                                     top + row * u));
        if (!item.labelled) {
            auto word = font();
            word.setFamilies(t.family);
            word.setPixelSize(std::max(1, static_cast<int>(std::round(1.45 * u))));
            word.setWeight(QFont::Bold);
            painter.setFont(word);
            painter.setPen(theme_ == ThemeId::Azure ? QColor("#4A3500") : t.text_heading);
            painter.drawText(QRectF(card.left(), top + page_tall + 0.25 * u, card.width(), 1.9 * u),
                             Qt::AlignCenter, QStringLiteral("SQL"));
        }
        break;
    }
    case HeroIcon::None:
        break;
    }
}

} // namespace erdflow::desktop
