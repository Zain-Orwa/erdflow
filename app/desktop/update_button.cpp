// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/update_button.hpp"

#include "app/desktop/home_sidebar.hpp"

#include <QEvent>
#include <QFontMetrics>
#include <QPainter>
#include <QPainterPath>

#include <array>
#include <cstddef>
#include <utility>

namespace erdflow::desktop {
namespace {
// Small enough for the ribbon's row of tabs, which is thirty pixels tall and
// stays so, with air above and below.
constexpr int tall = 22;
// The air at either end of it, its mark, and the gap between mark and words.
constexpr int inset = 10;
constexpr int mark = 12;
constexpr int gap = 6;
// How far it keeps from whatever stands either side of the room it is given.
constexpr int clear = 10;

QFont lettering(const Tokens& t, const QFont& base) {
    auto font = base;
    font.setFamilies(t.family);
    font.setPixelSize(12);
    font.setWeight(QFont::DemiBold);
    return font;
}

// An arrow turning back on itself, which is what a restart is drawn as: most
// of a ring, begun at three o'clock and carried clockwise round to the upper
// right, and a head where it ends. Drawn rather than loaded, in a box twelve
// across, so it is sharp at any scale and takes whatever ink its button has.
QPainterPath turning_arrow() {
    QPainterPath path;
    const QRectF ring(1.5, 1.5, 9, 9);
    path.arcMoveTo(ring, 0);
    path.arcTo(ring, 0, -315);
    path.lineTo(10.5, 4);
    path.moveTo(10.5, 1.5);
    path.lineTo(10.5, 4);
    path.lineTo(8, 4);
    return path;
}

// One step further the way the theme's own steps go, from its primary to its
// pressed primary: darker in most themes, lighter in one whose steps lighten.
// For when the button already rests on the last step the theme names.
QColor beyond(const Tokens& t, const QColor& colour) {
    return t.primary_pressed.lightness() <= t.primary.lightness() ? colour.darker(113) : colour.lighter(113);
}
} // namespace

UpdateButton::UpdateButton(QWidget* row, std::function<int()> from, std::function<int()> to)
    : QAbstractButton(row), from_(std::move(from)), to_(std::move(to)) {
    setText(QStringLiteral("Update and Restart"));
    setAccessibleName(text());
    setCursor(Qt::PointingHandCursor);
    setAttribute(Qt::WA_Hover, true);
    // The buttons of a ribbon are not stops for the Tab key, and neither is
    // this unless whoever stands it on a row says it is.
    setFocusPolicy(Qt::NoFocus);
    setVisible(false);
    again_.setSingleShot(true);
    again_.setInterval(0);
    connect(&again_, &QTimer::timeout, this, [this] { settle(); });
    row->installEventFilter(this);
}

void UpdateButton::wear(ThemeId id) {
    theme_ = id;
    // A theme may letter in a family of another width.
    settle();
    update();
}

void UpdateButton::offer(const QString& version) {
    version_ = version;
    const auto says = version.isEmpty()
        ? QString()
        : QStringLiteral("ERDFlow %1 has been fetched and is ready. Update and Restart closes ERDFlow, puts it "
                         "in place and opens ERDFlow again. You are asked about unsaved changes first. Closing "
                         "ERDFlow yourself updates it just the same.")
              .arg(version);
    setToolTip(says);
    setAccessibleDescription(says);
    settle();
}

QSize UpdateButton::sizeHint() const {
    const QFontMetrics metrics(lettering(tokens(theme_), font()));
    return {inset + mark + gap + metrics.horizontalAdvance(text()) + inset, tall};
}

void UpdateButton::settle() {
    auto* row = parentWidget();
    if (!row) return;
    const int from = from_() + clear;
    const int to = to_() - clear;
    const int room = to - from;
    // Nothing ready, or no room on the row even for its mark: it is away.
    if (version_.isEmpty() || room < tall) {
        if (!isHidden()) setVisible(false);
        return;
    }
    const int whole = sizeHint().width();
    const bool only_mark = room < whole;
    if (mark_only_ != only_mark) {
        mark_only_ = only_mark;
        update();
    }
    const int wide = only_mark ? tall : whole;
    setGeometry(to - wide, (row->height() - tall) / 2, wide, tall);
    if (isHidden()) {
        setVisible(true);
        // Over the row, whatever was made on it after this was.
        raise();
    }
}

bool UpdateButton::eventFilter(QObject* watched, QEvent* event) {
    const auto kind = event->type();
    // What stands on the row has been put where it belongs by the time this
    // is heard, or will have been once the row itself has heard it: so it is
    // settled now, and once more after.
    if (watched == parentWidget() && !version_.isEmpty() &&
        (kind == QEvent::Resize || kind == QEvent::LayoutRequest || kind == QEvent::Show)) {
        settle();
        again_.start();
    }
    return QAbstractButton::eventFilter(watched, event);
}

void UpdateButton::paintEvent(QPaintEvent*) {
    const auto& t = tokens(theme_);
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);

    // The fill a chosen row takes: the theme's own primary, at the strength
    // lettering reads on (ADR-022 section 9.3) -- so under Plain it is grey,
    // as everything the application inks is. Under the pointer it takes the
    // theme's next step from there, and held down the step after that, so
    // each is seen whichever step it rests on; and the lettering is whichever
    // of black and white reads on the fill it then has.
    const std::array<QColor, 5> steps{t.primary, t.primary_hover, t.primary_pressed, beyond(t, t.primary_pressed),
                                      beyond(t, beyond(t, t.primary_pressed))};
    const auto rest = chosen_row_fill(t);
    std::size_t at = 0;
    while (at < 2 && steps[at] != rest) ++at;
    const auto fill = steps[at + (isDown() ? 2 : underMouse() ? 1 : 0)];
    const auto ink = readable_on(fill);

    const QRectF box(rect());
    QPainterPath pill;
    pill.addRoundedRect(box, box.height() / 2.0, box.height() / 2.0);
    painter.fillPath(pill, fill);

    painter.save();
    painter.translate(mark_only_ ? (width() - mark) / 2.0 : inset, (height() - mark) / 2.0);
    painter.scale(mark / 12.0, mark / 12.0);
    painter.setPen(QPen(ink, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(turning_arrow());
    painter.restore();

    if (!mark_only_) {
        const int words = inset + mark + gap;
        painter.setFont(lettering(t, font()));
        painter.setPen(ink);
        painter.drawText(QRectF(words, 0, width() - words, height()), Qt::AlignVCenter | Qt::AlignLeft, text());
    }

    // Where the keyboard is: a ring just inside its edge, in its own ink.
    if (hasFocus()) {
        auto ring = ink;
        ring.setAlphaF(0.85f);
        painter.setPen(QPen(ring, 1.5));
        painter.setBrush(Qt::NoBrush);
        const auto inner = box.adjusted(2.25, 2.25, -2.25, -2.25);
        painter.drawRoundedRect(inner, inner.height() / 2.0, inner.height() / 2.0);
    }
}

} // namespace erdflow::desktop
