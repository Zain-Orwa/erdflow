#include "icons.hpp"

#include <QFile>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QSvgRenderer>
#include <cmath>

namespace erdflow::desktop {
namespace {

// A fill that runs from a lifted tint down to the base colour. The shading is
// what gives a flat shape its sense of depth without resorting to bitmaps.
QLinearGradient depth(const QRectF& box, const QColor& base) {
    QLinearGradient gradient(box.topLeft(), box.bottomRight());
    gradient.setColorAt(0.0, base.lighter(118));
    gradient.setColorAt(1.0, base);
    return gradient;
}

// Every outline uses the same weight and rounded ends, which is most of what
// makes a set of glyphs read as one family.
QPen outline(const QColor& colour, qreal weight) {
    QPen pen(colour, weight);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    return pen;
}

void arrow_head(QPainter& painter, const QPointF& tip, const QPointF& back, qreal spread, const QColor& ink) {
    const auto length = std::hypot(back.x(), back.y());
    if (length < 0.001) return;
    const QPointF unit = back / length;
    const QPointF side{-unit.y() * spread, unit.x() * spread};
    QPolygonF head;
    head << tip << tip + unit * (spread * 2) + side << tip + unit * (spread * 2) - side;
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink);
    painter.drawPolygon(head);
}

void draw(QPainter& painter, Glyph glyph, const Theme& colors, qreal side) {
    const auto ink = colors.text;
    const auto accent = colors.accent;
    const auto weight = side / 8.5;
    const QRectF box(weight, weight, side - weight * 2, side - weight * 2);
    const auto centre = box.center();
    painter.setPen(outline(ink, weight));
    painter.setBrush(Qt::NoBrush);

    switch (glyph) {
    case Glyph::New: {
        const auto fold = box.width() * 0.3;
        const QRectF page(box.left() + box.width() * 0.1, box.top(), box.width() * 0.8, box.height());
        QPainterPath sheet(QPointF(page.left(), page.top()));
        sheet.lineTo(page.right() - fold, page.top());
        sheet.lineTo(page.right(), page.top() + fold);
        sheet.lineTo(page.right(), page.bottom());
        sheet.lineTo(page.left(), page.bottom());
        sheet.closeSubpath();
        painter.setBrush(depth(box, colors.base));
        painter.setPen(outline(colors.muted, weight * 0.85));
        painter.drawPath(sheet);
        painter.drawLine(QPointF(page.right() - fold, page.top()), QPointF(page.right() - fold, page.top() + fold));
        painter.drawLine(QPointF(page.right() - fold, page.top() + fold), QPointF(page.right(), page.top() + fold));
        break;
    }
    case Glyph::Open: {
        QPainterPath folder(QPointF(box.left(), box.bottom()));
        folder.lineTo(box.left(), box.top() + box.height() * 0.18);
        folder.lineTo(box.left() + box.width() * 0.4, box.top() + box.height() * 0.18);
        folder.lineTo(box.left() + box.width() * 0.52, box.top() + box.height() * 0.34);
        folder.lineTo(box.right(), box.top() + box.height() * 0.34);
        folder.lineTo(box.right(), box.bottom());
        folder.closeSubpath();
        painter.setBrush(depth(box, accent.lighter(155)));
        painter.setPen(outline(accent.darker(125), weight));
        painter.drawPath(folder);
        break;
    }
    case Glyph::Save:
        painter.setBrush(depth(box, accent));
        painter.setPen(outline(accent.darker(140), weight));
        painter.drawRoundedRect(box, 2.5, 2.5);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.base);
        painter.drawRect(QRectF(box.left() + box.width() * 0.24, box.top() + weight * 0.4,
                                box.width() * 0.52, box.height() * 0.3));
        break;
    case Glyph::Undo:
    case Glyph::Redo: {
        const bool forward = glyph == Glyph::Redo;
        painter.save();
        if (forward) {
            painter.translate(centre.x() * 2, 0);
            painter.scale(-1, 1);
        }
        QPainterPath hook(QPointF(box.right() - box.width() * 0.08, box.bottom()));
        hook.cubicTo(QPointF(box.right(), box.top() + box.height() * 0.34),
                     QPointF(box.left() + box.width() * 0.42, box.top() + box.height() * 0.12),
                     QPointF(box.left() + box.width() * 0.26, box.top() + box.height() * 0.3));
        painter.setPen(outline(ink, weight * 0.95));
        painter.drawPath(hook);
        // The head sits at the arc's end and points back along its tangent, so
        // the two never separate however the curve is tuned.
        arrow_head(painter, QPointF(box.left() + box.width() * 0.16, box.top() + box.height() * 0.42),
                   QPointF(0.66, -0.75), weight * 1.15, ink);
        painter.restore();
        break;
    }
    case Glyph::Select: {
        // A pointer with the motion ticks that say it is the thing you move with.
        QPainterPath cursor(QPointF(box.left() + box.width() * 0.1, box.top()));
        cursor.lineTo(box.left() + box.width() * 0.1, box.bottom());
        cursor.lineTo(box.left() + box.width() * 0.38, box.bottom() - box.height() * 0.28);
        cursor.lineTo(box.right() - box.width() * 0.18, box.bottom() - box.height() * 0.32);
        cursor.closeSubpath();
        painter.setBrush(depth(box, accent));
        painter.setPen(outline(accent.darker(150), weight * 0.9));
        painter.drawPath(cursor);
        painter.setPen(outline(accent, weight * 0.8));
        painter.drawLine(QPointF(box.right() - box.width() * 0.2, box.top()),
                         QPointF(box.right() - box.width() * 0.05, box.top() - weight * 0.2));
        painter.drawLine(QPointF(box.right() - box.width() * 0.02, box.top() + box.height() * 0.2),
                         QPointF(box.right() + weight * 0.3, box.top() + box.height() * 0.18));
        break;
    }
    case Glyph::Entity:
        painter.setBrush(depth(box, colors.entity_fill));
        painter.setPen(outline(colors.entity_border, weight));
        painter.drawRoundedRect(box.adjusted(0, box.height() * 0.14, 0, -box.height() * 0.14), 2.5, 2.5);
        break;
    case Glyph::Attribute:
        painter.setBrush(depth(box, colors.attribute_fill));
        painter.setPen(outline(colors.attribute_border, weight));
        painter.drawEllipse(box.adjusted(0, box.height() * 0.16, 0, -box.height() * 0.16));
        break;
    case Glyph::Relationship: {
        QPolygonF diamond;
        diamond << QPointF(centre.x(), box.top()) << QPointF(box.right(), centre.y())
                << QPointF(centre.x(), box.bottom()) << QPointF(box.left(), centre.y());
        painter.setBrush(depth(box, colors.relationship_fill));
        painter.setPen(outline(colors.relationship_border, weight));
        painter.drawPolygon(diamond);
        break;
    }
    case Glyph::Isa: {
        QPolygonF triangle;
        triangle << QPointF(centre.x(), box.top()) << QPointF(box.right(), box.bottom())
                 << QPointF(box.left(), box.bottom());
        painter.setBrush(depth(box, colors.relationship_fill));
        painter.setPen(outline(colors.relationship_border, weight));
        painter.drawPolygon(triangle);
        break;
    }
    case Glyph::Connect: {
        const QPointF from(box.left() + weight, box.bottom() - weight);
        const QPointF to(box.right() - weight, box.top() + weight);
        painter.setPen(outline(accent, weight * 1.1));
        painter.drawLine(from, to);
        painter.setPen(outline(accent.darker(150), weight * 0.8));
        painter.setBrush(depth(box, accent));
        painter.drawEllipse(from, weight * 1.5, weight * 1.5);
        painter.drawEllipse(to, weight * 1.5, weight * 1.5);
        break;
    }
    case Glyph::Pan: {
        // A hand, as every tool that grabs a canvas uses: palm plus four fingers.
        // The fingers have to be wider than the stroke that outlines them, or
        // the whole hand fills in and reads as a blob at toolbar size.
        const auto finger = box.width() * 0.2;
        QPainterPath hand;
        hand.addRoundedRect(QRectF(box.left() + box.width() * 0.08, centre.y() - box.height() * 0.08,
                                   box.width() * 0.84, box.height() * 0.58), finger * 0.8, finger * 0.8);
        for (int index = 0; index < 4; ++index) {
            const auto x = box.left() + box.width() * (0.09 + index * 0.21);
            const auto top = box.top() + box.height() * (index == 0 || index == 3 ? 0.26 : 0.08);
            hand.addRoundedRect(QRectF(x, top, finger, centre.y() + box.height() * 0.16 - top),
                                finger * 0.5, finger * 0.5);
        }
        painter.setBrush(depth(box, colors.base));
        painter.setPen(outline(colors.muted, weight * 0.5));
        painter.drawPath(hand.simplified());
        break;
    }
    case Glyph::Fit: {
        painter.setPen(outline(colors.border, weight * 0.8));
        painter.drawRoundedRect(box, 2, 2);
        painter.setPen(outline(accent, weight * 1.1));
        const qreal arm = box.width() * 0.22;
        for (const auto& [corner, dx, dy] : std::initializer_list<std::tuple<QPointF, qreal, qreal>>{
                 {box.topLeft(), 1, 1}, {box.topRight(), -1, 1},
                 {box.bottomLeft(), 1, -1}, {box.bottomRight(), -1, -1}}) {
            const QPointF inner(corner.x() + dx * arm, corner.y() + dy * arm);
            painter.drawLine(inner, QPointF(inner.x() - dx * arm, inner.y()));
            painter.drawLine(inner, QPointF(inner.x(), inner.y() - dy * arm));
        }
        break;
    }
    case Glyph::Check: {
        painter.setBrush(depth(box, QColor(0x2e, 0xa0, 0x62)));
        painter.setPen(outline(QColor(0x1d, 0x6f, 0x42), weight * 0.9));
        painter.drawEllipse(box);
        QPainterPath tick(QPointF(box.left() + box.width() * 0.26, centre.y() + box.height() * 0.02));
        tick.lineTo(centre.x() - box.width() * 0.03, box.bottom() - box.height() * 0.26);
        tick.lineTo(box.right() - box.width() * 0.22, box.top() + box.height() * 0.28);
        painter.setPen(outline(Qt::white, weight * 1.15));
        painter.setBrush(Qt::NoBrush);
        painter.drawPath(tick);
        break;
    }
    case Glyph::Dismiss: {
        // The answer to the tick: the same disc, in the colour of a fault,
        // with a cross on it. It is what the button offers once the findings
        // are open, which is to put them away again.
        painter.setBrush(depth(box, QColor(0xc8, 0x45, 0x45)));
        painter.setPen(outline(QColor(0x8f, 0x2b, 0x2b), weight * 0.9));
        painter.drawEllipse(box);
        const auto arm = box.width() * 0.22;
        painter.setPen(outline(Qt::white, weight * 1.15));
        painter.setBrush(Qt::NoBrush);
        painter.drawLine(centre - QPointF(arm, arm), centre + QPointF(arm, arm));
        painter.drawLine(centre - QPointF(arm, -arm), centre + QPointF(arm, -arm));
        break;
    }
    case Glyph::Symbols: {
        // Omega: what a document editor has meant by "symbols" for thirty
        // years. It is a letter, so it is one stroked path rather than a
        // filled shape -- a foot, up into a bowl left open at the bottom, and
        // down to the other foot.
        const QRectF ring(box.left() + box.width() * 0.16, box.top() + box.height() * 0.05,
                          box.width() * 0.68, box.height() * 0.66);
        // Qt measures arc angles anticlockwise from three o'clock, so six
        // o'clock is 270 and the bowl is left open either side of it.
        constexpr double left_end = 250.0, right_end = 290.0;
        const auto at = [&](double degrees) {
            const auto radians = degrees * std::acos(-1.0) / 180.0;
            return QPointF(ring.center().x() + ring.width() / 2 * std::cos(radians),
                           ring.center().y() - ring.height() / 2 * std::sin(radians));
        };
        const auto base = box.bottom() - weight * 0.55;
        const auto foot = box.width() * 0.15;
        QPainterPath omega(QPointF(at(left_end).x() - foot, base));
        omega.lineTo(at(left_end).x() - foot * 0.16, base);
        // A negative sweep runs clockwise, which is the way round the top.
        omega.arcTo(ring, left_end, -(360.0 - (right_end - left_end)));
        omega.lineTo(at(right_end).x() + foot * 0.16, base);
        omega.lineTo(at(right_end).x() + foot, base);
        painter.setBrush(Qt::NoBrush);
        painter.setPen(outline(colors.accent, weight * 0.95));
        painter.drawPath(omega);
        break;
    }
    case Glyph::Duplicate:
        painter.setBrush(depth(box, colors.base));
        painter.setPen(outline(colors.muted, weight * 0.9));
        painter.drawRoundedRect(QRectF(box.left(), box.top() + box.height() * 0.2,
                                       box.width() * 0.8, box.height() * 0.8), 2, 2);
        painter.setBrush(depth(box, accent.lighter(165)));
        painter.setPen(outline(accent.darker(120), weight * 0.9));
        painter.drawRoundedRect(QRectF(box.left() + box.width() * 0.2, box.top(),
                                       box.width() * 0.8, box.height() * 0.8), 2, 2);
        break;
    case Glyph::Rename: {
        painter.setPen(outline(colors.muted, weight * 0.9));
        painter.drawLine(QPointF(box.left(), box.bottom()), QPointF(box.right() - box.width() * 0.1, box.bottom()));
        // The pen is a quadrilateral barrel closed by a tip, so it reads as a
        // pencil rather than as a stray diagonal line.
        const QPointF tip(box.left() + box.width() * 0.08, box.bottom() - box.height() * 0.14);
        const auto barrel = box.width() * 0.22;
        QPainterPath pen(tip);
        pen.lineTo(tip.x() + barrel * 0.5, tip.y() - barrel * 0.85);
        pen.lineTo(box.right() - box.width() * 0.06, box.top() + box.height() * 0.12);
        pen.lineTo(box.right() - box.width() * 0.22, box.top());
        pen.closeSubpath();
        painter.setBrush(depth(box, accent));
        painter.setPen(outline(accent.darker(145), weight * 0.75));
        painter.drawPath(pen);
        break;
    }
    case Glyph::Theme: {
        // Half the disc carries the page colour and half the ink, which is the
        // one picture that says "appearance" without naming a single theme.
        painter.setBrush(depth(box, colors.base));
        painter.setPen(outline(colors.muted, weight * 0.8));
        painter.drawEllipse(box);
        QPainterPath half(QPointF(centre.x(), box.top()));
        half.arcTo(box, 90, -180);
        half.closeSubpath();
        painter.setPen(Qt::NoPen);
        painter.setBrush(ink);
        painter.drawPath(half);
        break;
    }
    case Glyph::Delete: {
        const auto lip = box.top() + box.height() * 0.26;
        const QColor rim(0xc0, 0x3b, 0x3b);
        // Handle first, then lid, then a body that tapers: the silhouette is
        // what identifies a bin, so none of the three can be dropped.
        painter.setPen(outline(rim, weight * 0.85));
        painter.drawLine(QPointF(centre.x() - box.width() * 0.16, box.top() + box.height() * 0.08),
                         QPointF(centre.x() + box.width() * 0.16, box.top() + box.height() * 0.08));
        painter.setBrush(depth(box, QColor(0xe2, 0x6a, 0x6a)));
        painter.drawRoundedRect(QRectF(box.left(), lip - box.height() * 0.1,
                                       box.width(), box.height() * 0.16), 1.5, 1.5);
        QPainterPath body(QPointF(box.left() + box.width() * 0.12, lip + box.height() * 0.06));
        body.lineTo(box.right() - box.width() * 0.12, lip + box.height() * 0.06);
        body.lineTo(box.right() - box.width() * 0.2, box.bottom());
        body.lineTo(box.left() + box.width() * 0.2, box.bottom());
        body.closeSubpath();
        painter.drawPath(body);
        break;
    }
    case Glyph::Download: {
        // Work leaving: an arrow rising out of a tray. It is the same motif the
        // coloured set draws, so the two sets say the same thing about it.
        painter.setBrush(Qt::NoBrush);
        painter.setPen(outline(colors.muted, weight * 0.85));
        QPainterPath tray(QPointF(box.left(), box.top() + box.height() * 0.62));
        tray.lineTo(box.left(), box.bottom());
        tray.lineTo(box.right(), box.bottom());
        tray.lineTo(box.right(), box.top() + box.height() * 0.62);
        painter.drawPath(tray);
        painter.setPen(outline(accent, weight));
        const auto stem = box.center().x();
        painter.drawLine(QPointF(stem, box.top() + box.height() * 0.54), QPointF(stem, box.top()));
        QPainterPath head(QPointF(stem - box.width() * 0.2, box.top() + box.height() * 0.2));
        head.lineTo(stem, box.top());
        head.lineTo(stem + box.width() * 0.2, box.top() + box.height() * 0.2);
        painter.drawPath(head);
        break;
    }
    case Glyph::Picture: {
        // A framed landscape, which is the picture everyone draws for a picture.
        painter.setBrush(depth(box, colors.base));
        painter.setPen(outline(colors.muted, weight * 0.85));
        painter.drawRoundedRect(box, 2, 2);
        QPainterPath hills(QPointF(box.left(), box.bottom()));
        hills.lineTo(box.left() + box.width() * 0.36, box.top() + box.height() * 0.42);
        hills.lineTo(box.left() + box.width() * 0.56, box.top() + box.height() * 0.68);
        hills.lineTo(box.left() + box.width() * 0.72, box.top() + box.height() * 0.52);
        hills.lineTo(box.right(), box.bottom());
        hills.closeSubpath();
        painter.setBrush(depth(box, accent));
        painter.setPen(outline(accent.darker(140), weight * 0.7));
        painter.drawPath(hills);
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.warning);
        painter.drawEllipse(QPointF(box.right() - box.width() * 0.26, box.top() + box.height() * 0.28),
                            weight * 1.2, weight * 1.2);
        break;
    }
    case Glyph::FullView: {
        // A window with its side panels drawn as empty margins and its middle
        // filled: what is left when the panels are put away.
        painter.setBrush(Qt::NoBrush);
        painter.setPen(outline(colors.muted, weight * 0.85));
        painter.drawRoundedRect(box, 2, 2);
        const auto margin = box.width() * 0.22;
        const QRectF middle(box.left() + margin, box.top() + weight * 0.5,
                            box.width() - margin * 2, box.height() - weight);
        painter.setPen(Qt::NoPen);
        painter.setBrush(depth(box, accent));
        painter.drawRect(middle);
        painter.setPen(outline(colors.muted, weight * 0.7));
        painter.drawLine(middle.topLeft(), middle.bottomLeft());
        painter.drawLine(middle.topRight(), middle.bottomRight());
        break;
    }
    case Glyph::Note: {
        // A slip with a folded corner and two lines of writing on it.
        const auto fold = box.width() * 0.28;
        const QRectF slip(box.left() + box.width() * 0.08, box.top(), box.width() * 0.84, box.height());
        QPainterPath sheet(QPointF(slip.left(), slip.top()));
        sheet.lineTo(slip.right() - fold, slip.top());
        sheet.lineTo(slip.right(), slip.top() + fold);
        sheet.lineTo(slip.right(), slip.bottom());
        sheet.lineTo(slip.left(), slip.bottom());
        sheet.closeSubpath();
        painter.setBrush(depth(box, note_surface(colors)));
        painter.setPen(outline(colors.warning.darker(115), weight * 0.85));
        painter.drawPath(sheet);
        painter.drawLine(QPointF(slip.right() - fold, slip.top()), QPointF(slip.right() - fold, slip.top() + fold));
        painter.drawLine(QPointF(slip.right() - fold, slip.top() + fold), QPointF(slip.right(), slip.top() + fold));
        painter.setPen(outline(ink, weight * 0.7));
        painter.drawLine(QPointF(slip.left() + slip.width() * 0.22, centre.y()),
                         QPointF(slip.right() - slip.width() * 0.22, centre.y()));
        painter.drawLine(QPointF(slip.left() + slip.width() * 0.22, centre.y() + box.height() * 0.2),
                         QPointF(slip.left() + slip.width() * 0.55, centre.y() + box.height() * 0.2));
        break;
    }
    }
}

} // namespace

QString icon_mode_key(IconMode mode) {
    switch (mode) {
    case IconMode::Modern: return QStringLiteral("modern");
    case IconMode::Outline: return QStringLiteral("outline");
    case IconMode::Normal: break;
    }
    return QStringLiteral("normal");
}
IconMode icon_mode_from_key(const QString& key) {
    if (key == QStringLiteral("modern")) return IconMode::Modern;
    if (key == QStringLiteral("outline")) return IconMode::Outline;
    return IconMode::Normal;
}

QString icon_name(Glyph glyph) {
    switch (glyph) {
    case Glyph::New: return QStringLiteral("new-project");
    case Glyph::Open: return QStringLiteral("open");
    case Glyph::Save: return QStringLiteral("save");
    case Glyph::Undo: return QStringLiteral("undo");
    case Glyph::Redo: return QStringLiteral("redo");
    case Glyph::Select: return QStringLiteral("select");
    case Glyph::Entity: return QStringLiteral("entity");
    case Glyph::Attribute: return QStringLiteral("attribute");
    case Glyph::Relationship: return QStringLiteral("relationship");
    case Glyph::Isa: return QStringLiteral("isa");
    case Glyph::Connect: return QStringLiteral("connect");
    case Glyph::Pan: return QStringLiteral("pan");
    case Glyph::Fit: return QStringLiteral("zoom");
    case Glyph::Check: return QStringLiteral("validate");
    case Glyph::Duplicate: return QStringLiteral("duplicate");
    // The set has no pencil of its own, so renaming borrows the properties
    // artwork: both are about the details of an element rather than its shape.
    case Glyph::Rename: return QStringLiteral("properties");
    case Glyph::Delete: return QStringLiteral("delete");
    case Glyph::Theme: return QStringLiteral("theme");
    case Glyph::Picture: return QStringLiteral("picture");
    case Glyph::Note: return QStringLiteral("note");
    case Glyph::FullView: return QStringLiteral("full-view");
    case Glyph::Dismiss: return QStringLiteral("close");
    case Glyph::Symbols: return QStringLiteral("symbols");
    // The artwork is the arrow leaving a tray, filed under the older word
    // for it; the command it draws is Download.
    case Glyph::Download: return QStringLiteral("export");
    }
    return QStringLiteral("select");
}

QIcon glyph_icon(Glyph glyph, const Theme& colors, int size, IconMode mode) {
    if (mode == IconMode::Outline) {
        // The line art is drawn in one colour, named in the file as the colour
        // of the surrounding text. Qt's renderer does not resolve that itself,
        // so the ink asked for is put in its place before the file is drawn --
        // which is what makes one set of files serve every palette.
        QFile file(QStringLiteral(":/erdflow/icons-outline/%1.svg").arg(icon_name(glyph)));
        if (file.open(QIODevice::ReadOnly)) {
            const auto source = file.readAll();
            const auto inked = [&](const QColor& ink) {
                auto drawing = source;
                drawing.replace("currentColor", ink.name().toLatin1());
                QSvgRenderer renderer(drawing);
                if (!renderer.isValid()) return QPixmap();
                QPixmap pixmap(QSize(size, size) * 3);
                pixmap.setDevicePixelRatio(3);
                pixmap.fill(Qt::transparent);
                QPainter painter(&pixmap);
                painter.setRenderHint(QPainter::Antialiasing);
                // A little air around the drawing, so a button's edge never
                // crowds the line the way a full-bleed glyph would.
                const qreal inset = size * 0.08;
                renderer.render(&painter, QRectF(inset, inset, size - inset * 2, size - inset * 2));
                return pixmap;
            };
            const auto resting = inked(colors.text);
            if (!resting.isNull()) {
                QIcon icon(resting);
                // A tool that is on sits on a chip of the theme's accent, and a
                // line inked for the panel can all but vanish against it. The
                // set is a single colour, so the same file is drawn again in
                // the ink that reads on the accent and kept as the icon's "on"
                // state, which is what Qt asks for when a button is checked.
                const auto lit = inked(readable_on(colors.accent));
                if (!lit.isNull()) {
                    icon.addPixmap(lit, QIcon::Normal, QIcon::On);
                    icon.addPixmap(lit, QIcon::Active, QIcon::On);
                    icon.addPixmap(lit, QIcon::Selected, QIcon::On);
                }
                return icon;
            }
        }
        // A missing file must not leave a button blank; the drawn glyph stands in.
    }
    if (mode == IconMode::Modern) {
        // The artwork is square and carries its own plate, so it is rendered at
        // the pixel size it will be shown at rather than scaled from a pixmap.
        // A flat icon has no plate to sit on, so the set comes in two inks and
        // the theme's own panel decides which: the same rule that picks the
        // lettering over a colour picks the lettering of the icons.
        const bool on_dark = readable_on(colors.panel) == QColor(0xff, 0xff, 0xff);
        // Scalable artwork reports no fixed sizes of its own, so whether it
        // loaded is asked by rendering it rather than by listing what it offers.
        QIcon artwork(QStringLiteral(":/erdflow/%1/%2.svg")
                          .arg(on_dark ? QStringLiteral("icons-on-dark") : QStringLiteral("icons"),
                               icon_name(glyph)));
        if (!artwork.pixmap(size).isNull()) return artwork;
        // A missing file must not leave a button blank, so the drawn glyph
        // stands in. Nothing else in the window has to know it happened.
    }
    QPixmap pixmap(QSize(size, size) * 3);
    pixmap.setDevicePixelRatio(3);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    draw(painter, glyph, colors, size);
    return QIcon(pixmap);
}

} // namespace erdflow::desktop
