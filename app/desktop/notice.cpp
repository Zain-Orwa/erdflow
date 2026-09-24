// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/notice.hpp"

#include <QFontMetricsF>
#include <QPainter>
#include <QCursor>
#include <QPainterPath>
#include <algorithm>
#include <cmath>

namespace erdflow::desktop {
namespace {
// How wide it is allowed to grow before the words wrap, and the air inside it.
constexpr double widest = 460;
constexpr double padding = 14;
constexpr double corner = 9;
// How far up from the bottom of what it is laid over it sits. Far enough to
// clear a status bar, near enough to stay in the same glance as the work.
constexpr double up_from_bottom = 28;
// How far from the pointer it stands. Far enough that the pointer and what it
// is resting on are not covered by what is being said about them.
constexpr int from_pointer = 18;
// How far the pointer has to travel before the hand counts as having moved
// on. A few pixels of jitter, or the small shift that letting go of a button
// often causes, must not take a warning away before it has been read at all.
constexpr int stirred = 16;
// How often the pointer is looked at while something is being said. Often
// enough that going away feels like an answer to moving, rare enough to cost
// nothing -- and it runs only while a notice is up, which is seldom.
constexpr int watch_ms = 40;
// Slow enough to be seen arriving and leaving rather than appearing and
// vanishing: the point of a notice fading is that the eye catches the change
// and knows where to look.
constexpr int in_ms = 260;
constexpr int out_ms = 520;
// How far it rises as it arrives. Small: enough to read as coming up out of
// the work, not as something flying in.
constexpr int rises = 7;
} // namespace

Notice::Notice(QWidget* parent) : QWidget(parent) {
    setObjectName("notice");
    // It is a remark about the work, not a place to work: the pointer goes
    // straight through it to whatever is underneath, so a notice that happens
    // to come up over the next thing to be pressed never blocks it.
    setAttribute(Qt::WA_TransparentForMouseEvents);
    setAttribute(Qt::WA_NoSystemBackground);
    setVisible(false);
    watch_.setInterval(watch_ms);
    connect(&watch_, &QTimer::timeout, this, [this] { watch_the_hand(); });
    connect(&fade_, &QVariantAnimation::valueChanged, this, [this](const QVariant& value) {
        opacity_ = value.toDouble();
        // It rises the last few pixels into place as it arrives, and settles
        // back the same way as it goes. Tied to the fade rather than animated
        // beside it, so the two can never come apart.
        settle();
        update();
    });
    connect(&fade_, &QVariantAnimation::finished, this, [this] {
        if (opacity_ <= 0.01) setVisible(false);
    });
}

void Notice::wear(const Theme& theme) {
    theme_ = theme;
    update();
}

QString Notice::saying() const {
    if (!isVisible()) return {};
    return detail_.isEmpty() ? headline_ : headline_ + " " + detail_;
}

void Notice::say(const QString& words, std::optional<QPoint> beside) {
    if (words.isEmpty()) return;
    beside_ = beside;
    // What went wrong and why are read at different speeds, so they are drawn
    // at different weights. The first sentence is what went wrong; everything
    // after it is why. A remark of one sentence is all headline, which is
    // right: there is nothing to explain.
    const auto stop = words.indexOf(". ");
    if (stop > 0) {
        headline_ = words.left(stop + 1);
        detail_ = words.mid(stop + 2).trimmed();
    } else {
        headline_ = words;
        detail_.clear();
    }
    settle();
    setVisible(true);
    raise();
    fade_to(1.0, in_ms);
    // Where the hand is now. It has stopped, or this would not have happened;
    // moving on from here is what will take this away.
    hand_was_ = QCursor::pos();
    watch_.start();
}

void Notice::watch_the_hand() {
    if (!isVisible()) { watch_.stop(); return; }
    if ((QCursor::pos() - hand_was_).manhattanLength() <= stirred) return;
    watch_.stop();
    fade_to(0.0, out_ms);
}

// How far below its place it still is. Full at the start of the arrival and
// nothing once it is fully there, so it rises as it fades in and sinks as it
// fades out.
int Notice::risen() const {
    return static_cast<int>(std::lround((1.0 - std::clamp(opacity_, 0.0, 1.0)) * rises));
}

void Notice::settle_again() {
    if (isVisible()) settle();
}

void Notice::put_away() {
    if (!isVisible()) return;
    watch_.stop();
    fade_to(0.0, out_ms);
}

void Notice::fade_to(double opacity, int over_ms) {
    fade_.stop();
    fade_.setStartValue(opacity_);
    fade_.setEndValue(opacity);
    fade_.setDuration(over_ms);
    fade_.setEasingCurve(QEasingCurve::OutCubic);
    fade_.start();
}

void Notice::settle() {
    auto strong = font();
    strong.setBold(true);
    const QFontMetricsF bold(strong);
    const QFontMetricsF plain(font());

    const auto room = widest - padding * 2;
    const auto head = bold.boundingRect(QRectF(0, 0, room, 1e6),
                                        Qt::TextWordWrap, headline_);
    auto tall = head.height();
    double wide = head.width();
    if (!detail_.isEmpty()) {
        const auto rest = plain.boundingRect(QRectF(0, 0, room, 1e6),
                                             Qt::TextWordWrap, detail_);
        tall += 4 + rest.height();
        wide = std::max(wide, rest.width());
    }
    const auto across = std::min(widest, wide + padding * 2);
    resize(static_cast<int>(std::ceil(across)),
           static_cast<int>(std::ceil(tall + padding * 2)));

    auto* over = parentWidget();
    if (!over) return;
    if (!beside_) {
        // Nothing in particular to stand beside, so it stands where a remark
        // about the window as a whole belongs.
        const auto x = (over->width() - width()) / 2;
        const auto y = over->height() - height() - static_cast<int>(up_from_bottom);
        move(std::max(8, x), std::max(8, y) + risen());
        return;
    }
    // Below and to the right of the hand, clear of the pointer itself so that
    // what it is about is not covered by what is said about it. Where there is
    // no room on that side it turns to the other, which is what keeps it on
    // the screen without ever landing under the pointer.
    auto x = beside_->x() + from_pointer;
    auto y = beside_->y() + from_pointer;
    if (x + width() > over->width() - 8) x = beside_->x() - width() - from_pointer;
    if (y + height() > over->height() - 8) y = beside_->y() - height() - from_pointer;
    move(std::clamp(x, 8, std::max(8, over->width() - width() - 8)),
         std::clamp(y, 8, std::max(8, over->height() - height() - 8)) + risen());
}

void Notice::paintEvent(QPaintEvent*) {
    if (opacity_ <= 0.01) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setOpacity(opacity_);

    // The amber every palette already has, carried over the panel rather than
    // used at full strength: a warning the eye can rest on, not one it has to
    // look away from. The same colour a note on the canvas is drawn in, so the
    // two read as the same kind of thing said in two places.
    const auto surface = note_surface(theme_);
    const QRectF box(0.5, 0.5, width() - 1.0, height() - 1.0);
    QPainterPath shape;
    shape.addRoundedRect(box, corner, corner);

    auto fill = surface;
    fill.setAlphaF(0.94f);
    painter.fillPath(shape, fill);
    auto edge = theme_.warning;
    edge.setAlphaF(0.55f);
    painter.setPen(QPen(edge, 1.0));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(shape);

    // A stripe of the warning colour down the leading edge, clipped to the
    // rounded shape so it follows the corners. It says what kind of remark
    // this is before a word of it has been read.
    painter.save();
    painter.setClipPath(shape);
    painter.fillRect(QRectF(box.left(), box.top(), 4.0, box.height()), theme_.warning);
    painter.restore();

    const auto ink = readable_on(surface);
    const auto room = QRectF(padding + 4, padding, width() - padding * 2 - 4,
                             height() - padding * 2);
    auto strong = font();
    strong.setBold(true);
    const QFontMetricsF bold(strong);
    const auto head = bold.boundingRect(QRectF(0, 0, room.width(), 1e6),
                                        Qt::TextWordWrap, headline_);
    painter.setFont(strong);
    painter.setPen(ink);
    painter.drawText(QRectF(room.left(), room.top(), room.width(), head.height()),
                     Qt::TextWordWrap, headline_);
    if (detail_.isEmpty()) return;
    // Quieter than what it explains, but not so quiet it reads as an aside:
    // this is the half that says what to do about it.
    auto softer = ink;
    softer.setAlphaF(0.78f);
    painter.setFont(font());
    painter.setPen(softer);
    painter.drawText(QRectF(room.left(), room.top() + head.height() + 4,
                            room.width(), room.height() - head.height() - 4),
                     Qt::TextWordWrap, detail_);
}

} // namespace erdflow::desktop
