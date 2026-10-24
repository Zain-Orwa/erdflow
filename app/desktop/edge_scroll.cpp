// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "edge_scroll.hpp"

#include <QAbstractScrollArea>
#include <QScrollBar>

#include <algorithm>
#include <cstdlib>
#include <utility>

namespace erdflow::desktop {

namespace {

// One axis of a step. A viewport too small for two whole strips gives each a
// quarter of itself, so the middle of it is still somewhere nothing scrolls.
int along(int at, int length) {
    const auto strip = std::max(1, std::min(EdgeScroll::reach, length / 4));
    int depth = 0;
    if (at < strip) depth = strip - at;
    else if (at >= length - strip) depth = -(at - (length - strip) + 1);
    if (depth == 0) return 0;
    const auto pace = std::clamp(1 + (std::abs(depth) - 1) / 3, 1, EdgeScroll::fastest);
    return depth > 0 ? -pace : pace;
}

} // namespace

EdgeScroll::EdgeScroll(QAbstractScrollArea& area, std::function<void()> scrolled)
    : area_(area), scrolled_(std::move(scrolled)) {
    timer_.setInterval(interval_ms);
    QObject::connect(&timer_, &QTimer::timeout, &timer_, [this] { advance(); });
}

QPoint EdgeScroll::step(QSize size, QPoint at) {
    return {along(at.x(), size.width()), along(at.y(), size.height())};
}

QPoint EdgeScroll::late_step(QPoint step, qint64 waited_ms) {
    const auto steps = std::max<qint64>(1, waited_ms / interval_ms);
    const auto stretched = [&](int one) {
        return static_cast<int>(std::clamp<qint64>(one * steps, -fastest, fastest));
    };
    return {stretched(step.x()), stretched(step.y())};
}

void EdgeScroll::follow(QPoint in_viewport) {
    pointer_ = in_viewport;
    if (step(area_.viewport()->size(), pointer_).isNull()) {
        stop();
    } else if (!timer_.isActive()) {
        since_step_.start();
        timer_.start();
    }
}

void EdgeScroll::stop() { timer_.stop(); }

void EdgeScroll::advance() {
    const auto by = late_step(step(area_.viewport()->size(), pointer_), since_step_.restart());
    auto* across = area_.horizontalScrollBar();
    auto* down = area_.verticalScrollBar();
    const QPoint was(across->value(), down->value());
    if (by.x()) across->setValue(across->value() + by.x());
    if (by.y()) down->setValue(down->value() + by.y());
    // Nothing left to scroll towards: the view has reached the end of the
    // canvas that way, and stepping on would only spend time.
    if (QPoint(across->value(), down->value()) == was) {
        stop();
        return;
    }
    if (scrolled_) scrolled_();
}

} // namespace erdflow::desktop
