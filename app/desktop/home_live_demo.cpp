// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/home_live_demo.hpp"

#include "app/desktop/home_demo_canvas.hpp"
#include "app/desktop/home_demo_scenes.hpp"

#include <QEvent>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>

#include <algorithm>

namespace erdflow::desktop {
namespace {
// What each is called to whatever reads the screen. The miniature says in
// pictures what its card says in words, so it is described rather than named
// as a control: nothing in it can be pressed.
const char* description_of(HomeDemoKind kind) {
    switch (kind) {
    case HomeDemoKind::Conceptual: return "A small conceptual design being drawn.";
    case HomeDemoKind::Relational: return "A small relational schema being laid out.";
    case HomeDemoKind::Sql: return "A short SQL script being written.";
    }
    return "";
}
} // namespace

HomeLiveDemo::HomeLiveDemo(HomeDemoKind kind, QWidget* parent) : QWidget(parent), kind_(kind) {
    static constexpr const char* names[] = {"homeDemoConceptual", "homeDemoRelational", "homeDemoSql"};
    setObjectName(QString::fromLatin1(names[static_cast<int>(kind)]));
    setAccessibleDescription(QString::fromLatin1(description_of(kind)));
    // Decoration: it is never focused, and a press on it is a press on the page.
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    // Finished, until something plays it.
    step_ = demo_finished_step(kind_);
    progress_ = 0.0;
}

void HomeLiveDemo::show_step(std::size_t step, double progress) {
    const auto& steps = demo_steps(kind_);
    const auto to = steps.empty() ? std::size_t{0} : std::min(step, steps.size() - 1);
    // Through a step in which nothing changes, a new moment draws the same
    // picture, so it is not drawn again.
    const bool unchanged = to == step_ && demo_step_is_still(kind_, to);
    step_ = to;
    progress_ = std::clamp(progress, 0.0, 1.0);
    if (!unchanged) update();
}

void HomeLiveDemo::show_time(double seconds) {
    const auto moment = demo_moment_at(kind_, seconds);
    show_step(moment.step, moment.progress);
}

void HomeLiveDemo::set_floor(bool on) {
    floor_ = on;
    update();
}

void HomeLiveDemo::set_real_canvas(bool on) {
    real_canvas_ = on && kind_ == HomeDemoKind::Conceptual;
    update();
}

namespace {
// The screen's measurements, in pixels: room kept under it for its depth and
// its shadow, its corners, its header, and the margin round what it shows.
constexpr double screen_depth = 4;
constexpr double screen_radius = 8;
constexpr double screen_header = 16;
constexpr double screen_margin = 4;

QColor blend(const QColor& from, const QColor& to, double share) {
    return QColor::fromRgbF(static_cast<float>(from.redF() + (to.redF() - from.redF()) * share),
                            static_cast<float>(from.greenF() + (to.greenF() - from.greenF()) * share),
                            static_cast<float>(from.blueF() + (to.blueF() - from.blueF()) * share),
                            static_cast<float>(from.alphaF() + (to.alphaF() - from.alphaF()) * share));
}

// What each screen is headed with: what the workspace is called, as the Home
// screen's own illustration names it, or the file the script is written in.
const char* heading_of(HomeDemoKind kind) {
    switch (kind) {
    case HomeDemoKind::Conceptual: return "Conceptual ERD";
    case HomeDemoKind::Relational: return "Relational Design";
    case HomeDemoKind::Sql: return "university.sql";
    }
    return "";
}
} // namespace

QRectF HomeLiveDemo::screen() const {
    return QRectF(rect()).adjusted(3, 2, -3, -(screen_depth + 6));
}

QRectF HomeLiveDemo::screen_inside() const {
    return screen().adjusted(screen_margin, screen_header, -screen_margin, -screen_margin);
}

// A small screen raised off the card, drawn as the Home screen's own floating
// panels are drawn: a pale face lit from above, a catch of light along its top
// edge, a soft shadow under it -- and a few pixels of its own edge showing
// beneath, which is what makes it stand out of the card rather than lie on it.
void HomeLiveDemo::paint_screen(QPainter& painter) const {
    const auto& t = tokens(theme_);
    const auto own = [this](const QColor& colour) { return colourless(theme_) ? greyed(colour) : colour; };
    const auto blue = own(t.primary);
    const auto face = screen();
    painter.save();
    for (int step = 3; step >= 1; --step) {
        QPainterPath under;
        under.addRoundedRect(face.adjusted(-step * 0.6, step * 2.0 + screen_depth, step * 0.6,
                                           step * 2.4 + screen_depth),
                             screen_radius + step, screen_radius + step);
        auto wash = blue.darker(150);
        wash.setAlphaF(0.05f);
        painter.fillPath(under, wash);
    }
    QPainterPath depth;
    depth.addRoundedRect(face.translated(0, screen_depth), screen_radius, screen_radius);
    painter.fillPath(depth, blend(blue, Qt::white, 0.62));
    QPainterPath shape;
    shape.addRoundedRect(face, screen_radius, screen_radius);
    QLinearGradient light(face.topLeft(), face.bottomLeft());
    light.setColorAt(0.0, blend(blue, Qt::white, 0.965));
    light.setColorAt(1.0, blend(blue, Qt::white, 0.90));
    painter.fillPath(shape, light);
    // What it shows, on the workspace's own ground: the canvas for a model,
    // the editor's page for a script.
    const auto inside = screen_inside();
    QPainterPath well;
    well.addRoundedRect(inside, 3, 3);
    painter.fillPath(well, kind_ == HomeDemoKind::Sql ? t.surface : theme(theme_).canvas);
    painter.setPen(QPen(blend(blue, Qt::white, 0.78), 0.8));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(well);
    painter.setPen(QPen(blend(blue, Qt::white, 0.66), 1.0));
    painter.drawPath(shape);
    painter.setPen(QPen(QColor(255, 255, 255, 220), 1.0));
    painter.drawLine(QPointF(face.left() + screen_radius, face.top() + 1.2),
                     QPointF(face.right() - screen_radius, face.top() + 1.2));
    // The header: what is on the screen, small and quiet.
    auto lettering = font();
    lettering.setFamilies(t.family);
    lettering.setPixelSize(10);
    lettering.setWeight(QFont::DemiBold);
    painter.setFont(lettering);
    painter.setPen(t.text_muted);
    painter.drawText(QRectF(face.left() + 10, face.top(), face.width() - 20, screen_header),
                     Qt::AlignLeft | Qt::AlignVCenter, QString::fromLatin1(heading_of(kind_)));
    for (int dot = 0; dot < 3; ++dot) {
        auto shade = blend(blue, Qt::white, 0.72);
        painter.setPen(Qt::NoPen);
        painter.setBrush(shade);
        painter.drawEllipse(QPointF(face.right() - 12 - dot * 7.0, face.top() + screen_header / 2.0), 2.0, 2.0);
    }
    painter.restore();
}

void HomeLiveDemo::wear(ThemeId id) {
    theme_ = id;
    update();
}

QRectF HomeLiveDemo::stage() const {
    const auto scale = std::min(width() / authored_width, height() / authored_height);
    const QSizeF size(authored_width * scale, authored_height * scale);
    return {(width() - size.width()) / 2.0, 0, size.width(), size.height()};
}

bool HomeLiveDemo::shown() const {
    return stage().width() >= authored_width * smallest_scale;
}

void HomeLiveDemo::paintEvent(QPaintEvent*) {
    if (!shown()) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    if (floor_) {
        const auto box = stage();
        paint_floor(painter, box);
        // The scene is authored at one size and scaled evenly into the stage.
        painter.translate(box.topLeft());
        painter.scale(box.width() / authored_width, box.height() / authored_height);
        paint_demo_scene(painter, kind_, theme_, DemoMoment{step_, progress_});
        return;
    }
    // Held in a card: shown on a small screen raised off the card, and fitted
    // into it evenly, never stretched.
    paint_screen(painter);
    const auto inside = screen_inside().adjusted(3, 3, -3, -3);
    painter.setClipRect(screen_inside());
    const auto fit = [&](const QRectF& source) {
        const auto scale = std::min(inside.width() / source.width(), inside.height() / source.height());
        painter.translate(inside.center());
        painter.scale(scale, scale);
        painter.translate(-source.center());
    };
    if (real_canvas_) {
        const auto& canvas = conceptual_canvas_picture(theme_);
        fit(QRectF(QPointF(0, 0), canvas.source.size()));
        painter.drawPicture(QPointF(0, 0), canvas.picture);
        return;
    }
    fit(demo_scene_bounds(kind_));
    paint_demo_scene(painter, kind_, theme_, DemoMoment{step_, progress_}, true);
}

HomeDemoClock::HomeDemoClock(QObject* parent) : QObject(parent) {
    timer_.setInterval(frame_ms);
    connect(&timer_, &QTimer::timeout, this, [this] { show_at(seconds()); });
}

void HomeDemoClock::add(HomeLiveDemo* demo, double delay, bool plays) {
    demos_.push_back({demo, delay, plays});
    if (running()) show_at(seconds());
    else demo->show_step(demo_finished_step(demo->kind()), 0.0);
}

void HomeDemoClock::set_playing(HomeLiveDemo* demo, bool on) {
    for (auto& one : demos_)
        if (one.demo == demo) one.plays = on;
    if (!on) demo->show_step(demo_finished_step(demo->kind()), 0.0);
    if (!any_playing()) timer_.stop();
    else if (wanted_ && seen() && !running()) start_from_the_beginning();
}

bool HomeDemoClock::playing(const HomeLiveDemo* demo) const {
    for (const auto& one : demos_)
        if (one.demo == demo) return one.plays;
    return false;
}

bool HomeDemoClock::any_playing() const {
    return std::any_of(demos_.begin(), demos_.end(), [](const Played& one) { return one.plays; });
}

void HomeDemoClock::set_moving(bool on) {
    if (on == wanted_) return;
    wanted_ = on;
    if (on) {
        if (seen()) start_from_the_beginning();
        return;
    }
    timer_.stop();
    for (auto& one : demos_) one.demo->show_step(demo_finished_step(one.demo->kind()), 0.0);
}

void HomeDemoClock::follow(QWidget* shown_while) {
    if (seen_) seen_->removeEventFilter(this);
    seen_ = shown_while;
    if (seen_) seen_->installEventFilter(this);
    if (wanted_ && seen() && !running()) start_from_the_beginning();
    if (!seen()) timer_.stop();
}

bool HomeDemoClock::seen() const { return !seen_ || seen_->isVisible(); }

void HomeDemoClock::start_from_the_beginning() {
    // With nothing to move, nothing ticks: every demo is a still picture.
    if (!any_playing()) {
        show_at(0.0);
        return;
    }
    since_.start();
    timer_.start();
    show_at(0.0);
}

bool HomeDemoClock::eventFilter(QObject* watched, QEvent* event) {
    if (watched == seen_) {
        // Hidden, it stops ticking and costs nothing. Shown again, every demo
        // starts again from the beginning, in step with the others.
        if (event->type() == QEvent::Hide) timer_.stop();
        else if (event->type() == QEvent::Show && wanted_ && !running()) start_from_the_beginning();
    }
    return QObject::eventFilter(watched, event);
}

double HomeDemoClock::seconds() const {
    return since_.isValid() ? static_cast<double>(since_.elapsed()) / 1000.0 : 0.0;
}

double HomeDemoClock::delay_of(const HomeLiveDemo* demo) const {
    for (const auto& one : demos_)
        if (one.demo == demo) return one.delay;
    return 0.0;
}

void HomeDemoClock::show_at(double seconds) {
    for (auto& one : demos_) {
        // A demo that does not move is its scene finished, a still picture.
        if (!one.plays) one.demo->show_step(demo_finished_step(one.demo->kind()), 0.0);
        else if (seconds < one.delay) one.demo->show_step(0, 0.0);
        else one.demo->show_time(seconds - one.delay);
    }
}

// The floor the miniature stands on: a soft pool of the theme's pale primary,
// strongest under the middle and gone well before the edges, so nothing
// draws a box round it. Everything a scene draws will stand on this.
void HomeLiveDemo::paint_floor(QPainter& painter, const QRectF& stage) const {
    const auto& t = tokens(theme_);
    const QPointF middle(stage.center().x(), stage.top() + stage.height() * 0.87);
    const auto across = stage.width() * 0.46;
    const auto deep = stage.height() * 0.10;
    painter.save();
    painter.translate(middle);
    painter.scale(1.0, deep / across);
    QRadialGradient pool(QPointF(0, 0), across);
    auto centre = t.primary_soft;
    centre.setAlphaF(0.55f);
    auto edge = t.primary_faint;
    edge.setAlphaF(0.0f);
    pool.setColorAt(0.0, centre);
    pool.setColorAt(0.55, [&] {
        auto half = t.primary_faint;
        half.setAlphaF(0.45f);
        return half;
    }());
    pool.setColorAt(1.0, edge);
    painter.setPen(Qt::NoPen);
    painter.setBrush(pool);
    painter.drawEllipse(QPointF(0, 0), across, across);
    painter.restore();
}

} // namespace erdflow::desktop
