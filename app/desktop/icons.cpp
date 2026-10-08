// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "icons.hpp"

#include <QFile>
#include <QImage>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>
#include <QSvgRenderer>
#include <array>
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
    case Glyph::Arrange: {
        // Three sheets laid one over another, as a drawing program shows the
        // order things are stacked in: where things are put.
        painter.setPen(outline(accent.darker(150), weight * 0.8));
        for (int sheet = 2; sheet >= 0; --sheet) {
            const auto drop = box.height() * 0.2 * sheet;
            const auto top = box.top() + drop;
            const auto tall = box.height() * 0.55;
            QPolygonF leaf;
            leaf << QPointF(centre.x(), top) << QPointF(box.right(), top + tall / 2)
                 << QPointF(centre.x(), top + tall) << QPointF(box.left(), top + tall / 2);
            if (sheet == 0) painter.setBrush(depth(box, accent));
            else painter.setBrush(accent.lighter(130 + sheet * 20));
            painter.drawPolygon(leaf);
        }
        break;
    }
    case Glyph::Appearance: {
        // A painter's palette with three dabs of colour on it: how things are
        // drawn, as against Theme, which is the whole window's colours.
        QPainterPath palette;
        palette.addEllipse(box.adjusted(0, box.height() * 0.06, 0, -box.height() * 0.06));
        QPainterPath thumb;
        thumb.addEllipse(QPointF(box.left() + box.width() * 0.66, box.top() + box.height() * 0.68),
                         box.width() * 0.13, box.width() * 0.13);
        painter.setBrush(depth(box, colors.base));
        painter.setPen(outline(ink, weight * 0.85));
        painter.drawPath(palette.subtracted(thumb));
        painter.setPen(Qt::NoPen);
        const std::array<QColor, 3> dabs{colors.entity_border, colors.relationship_border, accent};
        const std::array<QPointF, 3> at{QPointF(0.32, 0.36), QPointF(0.58, 0.28), QPointF(0.28, 0.62)};
        for (std::size_t i = 0; i < dabs.size(); ++i) {
            painter.setBrush(dabs[i]);
            painter.drawEllipse(QPointF(box.left() + box.width() * at[i].x(), box.top() + box.height() * at[i].y()),
                                box.width() * 0.1, box.width() * 0.1);
        }
        break;
    }
    case Glyph::Table: {
        // A table as the schema draws one: a heading band over its rows, with
        // the key gutter ruled off down the left. In the entity's colours,
        // since a table wears the colours of what it came from.
        const auto body = box.adjusted(0, box.height() * 0.08, 0, -box.height() * 0.08);
        painter.setBrush(depth(box, colors.entity_fill));
        painter.setPen(outline(colors.entity_border, weight));
        painter.drawRoundedRect(body, 2.5, 2.5);
        const auto band = body.top() + body.height() * 0.3;
        painter.setPen(Qt::NoPen);
        painter.setBrush(colors.entity_border);
        painter.drawRoundedRect(QRectF(body.left(), body.top(), body.width(), band - body.top()), 2.5, 2.5);
        painter.setPen(outline(colors.entity_border, weight * 0.7));
        const auto row = body.top() + body.height() * 0.65;
        painter.drawLine(QPointF(body.left(), row), QPointF(body.right(), row));
        const auto gutter = body.left() + body.width() * 0.3;
        painter.drawLine(QPointF(gutter, band), QPointF(gutter, body.bottom()));
        break;
    }
    case Glyph::Attribute:
        painter.setBrush(depth(box, colors.attribute_fill));
        painter.setPen(outline(colors.attribute_border, weight));
        painter.drawEllipse(box.adjusted(0, box.height() * 0.16, 0, -box.height() * 0.16));
        break;
    case Glyph::SchemaRelationships: {
        const QPointF top(centre.x(), box.top() + box.height() * 0.16);
        const QPointF left(box.left() + box.width() * 0.16, box.bottom() - box.height() * 0.16);
        const QPointF right(box.right() - box.width() * 0.16, left.y());
        const auto radius = box.width() * 0.16;
        painter.setPen(outline(ink, weight));
        painter.drawLine(top, left);
        painter.drawLine(top, right);
        painter.setBrush(colors.panel);
        for (const auto& point : {top, left, right}) painter.drawEllipse(point, radius, radius);
        break;
    }
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
    case Glyph::Search: {
        // A glass: the ring and the handle, which is the drawing everybody
        // already reads as looking for something.
        painter.setBrush(Qt::NoBrush);
        painter.setPen(outline(accent, weight));
        const auto radius = box.width() * 0.3;
        const QPointF centre(box.left() + radius + box.width() * 0.06, box.top() + radius + box.height() * 0.06);
        painter.drawEllipse(centre, radius, radius);
        painter.drawLine(centre + QPointF(radius * 0.72, radius * 0.72),
                         QPointF(box.right() - box.width() * 0.06, box.bottom() - box.height() * 0.06));
        break;
    }
    case Glyph::Key: {
        // A key held as a key is held when it is about to be used: the bow at
        // the top with its hole through it, the shaft below, and the teeth at
        // the bottom. Drawn solid rather than in outline, and in one warm
        // colour, because the padlock the diagram puts on a locked element is
        // solid for the same reason: it is read at a glance in a small space,
        // and a hairline at that size reads as a smudge.
        const auto radius = box.width() * 0.27;
        const QPointF bow(centre.x(), box.top() + radius + box.height() * 0.03);
        const auto shaft = std::max(box.width() * 0.16, weight);
        const auto foot = box.bottom() - box.height() * 0.04;
        QPainterPath key;
        key.addEllipse(bow, radius, radius);
        // The hole, cut by the odd-even rule rather than painted over, so the
        // glyph works on any surface it is put on.
        key.addEllipse(bow, radius * 0.40, radius * 0.40);
        key.setFillRule(Qt::OddEvenFill);
        QPainterPath stem;
        stem.addRect(QRectF(bow.x() - shaft / 2, bow.y() + radius * 0.55, shaft, foot - bow.y() - radius * 0.55));
        // Two teeth on one side, which is what tells a key from a pin.
        const auto tooth = box.width() * 0.34;
        const auto thick = std::max(box.height() * 0.13, weight);
        stem.addRect(QRectF(bow.x() + shaft / 2 - 0.1, foot - thick, tooth, thick));
        stem.addRect(QRectF(bow.x() + shaft / 2 - 0.1, foot - thick * 2.8, tooth * 0.66, thick));
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawPath(key);
        painter.drawPath(stem);
        break;
    }
    case Glyph::Export: {
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
    case Glyph::ExplorerPanel:
    case Glyph::PropertiesPanel:
    case Glyph::SidePanels: {
        // The window Full view draws, with the side panel the button shows or
        // puts away filled -- the left, the right, or both -- where Full view
        // fills what is left between them. The fill goes under the frame, so
        // the two meet with nothing between them.
        const auto margin = box.width() * 0.3;
        const bool left = glyph != Glyph::PropertiesPanel;
        const bool right = glyph != Glyph::ExplorerPanel;
        painter.setPen(Qt::NoPen);
        painter.setBrush(depth(box, accent));
        if (left)
            painter.drawRect(QRectF(box.left(), box.top(), margin, box.height()));
        if (right)
            painter.drawRect(QRectF(box.right() - margin, box.top(), margin, box.height()));
        painter.setBrush(Qt::NoBrush);
        painter.setPen(outline(colors.muted, weight * 0.85));
        painter.drawRoundedRect(box, 2, 2);
        painter.setPen(outline(colors.muted, weight * 0.6));
        if (left)
            painter.drawLine(QPointF(box.left() + margin, box.top()), QPointF(box.left() + margin, box.bottom()));
        if (right)
            painter.drawLine(QPointF(box.right() - margin, box.top()), QPointF(box.right() - margin, box.bottom()));
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
    // The ribbon's own are drawn from their line art in every set
    // (line_art_only), so nothing is painted for them here.
    case Glyph::Import: case Glyph::View: case Glyph::Help:
    case Glyph::Background: case Glyph::IconSet: case Glyph::Notation: case Glyph::Lines:
    case Glyph::ProjectFile: case Glyph::PdfDocument: case Glyph::DataDictionary: case Glyph::HtmlReport:
    case Glyph::CsvListing: case Glyph::SvgPicture: case Glyph::PdfPage: case Glyph::MorePictures:
    case Glyph::CopyPicture: case Glyph::ProjectPicture: case Glyph::OtherTool:
    case Glyph::ActualSize: case Glyph::ZoomIn: case Glyph::ZoomOut: case Glyph::Grid:
    case Glyph::AlignToGrid: case Glyph::CanvasControls: case Glyph::Comments: case Glyph::History:
    case Glyph::Guide: case Glyph::About:
    case Glyph::FileTab: case Glyph::Home: case Glyph::Insert: case Glyph::Settings: case Glyph::NoPanels:
    case Glyph::Model:
        break;
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
    case Glyph::SchemaRelationships: return QStringLiteral("schema-relationships");
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
    // for it; the command it draws is Export.
    case Glyph::Export: return QStringLiteral("export");
    case Glyph::Search: return QStringLiteral("search");
    case Glyph::Key: return QStringLiteral("key");
    // The coloured set's table is filed as the schema it is the unit of.
    case Glyph::Table: return QStringLiteral("schema");
    // Neither has artwork in the coloured set, so the drawn glyph stands in
    // there; the line art has both.
    case Glyph::Arrange: return QStringLiteral("layers");
    case Glyph::Appearance: return QStringLiteral("appearance");
    case Glyph::ExplorerPanel: return QStringLiteral("panel-left");
    case Glyph::PropertiesPanel: return QStringLiteral("panel-right");
    case Glyph::SidePanels: return QStringLiteral("side-panels");
    // The ribbon's, named for what they stand for rather than for the
    // drawing, so another set can draw its own under the same name. The
    // coloured set has artwork for Import and for comments.
    case Glyph::Import: return QStringLiteral("import");
    case Glyph::View: return QStringLiteral("view");
    case Glyph::Help: return QStringLiteral("help");
    case Glyph::Background: return QStringLiteral("background");
    case Glyph::IconSet: return QStringLiteral("icon-set");
    case Glyph::Notation: return QStringLiteral("notation");
    case Glyph::Lines: return QStringLiteral("lines");
    case Glyph::ProjectFile: return QStringLiteral("project-file");
    case Glyph::PdfDocument: return QStringLiteral("pdf-document");
    case Glyph::DataDictionary: return QStringLiteral("data-dictionary");
    case Glyph::HtmlReport: return QStringLiteral("html-report");
    case Glyph::CsvListing: return QStringLiteral("csv-listing");
    case Glyph::SvgPicture: return QStringLiteral("svg-picture");
    case Glyph::PdfPage: return QStringLiteral("pdf-page");
    case Glyph::MorePictures: return QStringLiteral("more-pictures");
    case Glyph::CopyPicture: return QStringLiteral("copy-picture");
    case Glyph::ProjectPicture: return QStringLiteral("project-picture");
    case Glyph::OtherTool: return QStringLiteral("other-tool");
    case Glyph::ActualSize: return QStringLiteral("actual-size");
    case Glyph::ZoomIn: return QStringLiteral("zoom-in");
    case Glyph::ZoomOut: return QStringLiteral("zoom-out");
    case Glyph::Grid: return QStringLiteral("grid");
    case Glyph::AlignToGrid: return QStringLiteral("align-to-grid");
    case Glyph::CanvasControls: return QStringLiteral("canvas-controls");
    case Glyph::Comments: return QStringLiteral("comment");
    case Glyph::History: return QStringLiteral("history");
    case Glyph::Guide: return QStringLiteral("guide");
    case Glyph::About: return QStringLiteral("about");
    // The coloured set has artwork for Settings alone among these.
    case Glyph::FileTab: return QStringLiteral("file");
    case Glyph::Home: return QStringLiteral("house");
    case Glyph::Insert: return QStringLiteral("insert");
    case Glyph::Settings: return QStringLiteral("settings");
    case Glyph::NoPanels: return QStringLiteral("no-panels");
    // Lucide's network, which both sets have as the Home screen's conceptual.
    case Glyph::Model: return QStringLiteral("conceptual");
    }
    return QStringLiteral("select");
}

QPixmap primary_key_mark(int size, qreal ratio, bool greyed) {
    const auto pixels = std::max(1, static_cast<int>(std::lround(size * ratio)));
    QImage image(pixels, pixels, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    QSvgRenderer drawing(QStringLiteral(":/erdflow/marks/primary-key.svg"));
    if (drawing.isValid()) {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        drawing.render(&painter, QRectF(0, 0, pixels, pixels));
    }
    // Plain shows no colour of its own (Zain, 2026-09-24): the key keeps its
    // shape and shading, in greys.
    if (greyed)
        for (int y = 0; y < image.height(); ++y) {
            auto* line = reinterpret_cast<QRgb*>(image.scanLine(y));
            for (int x = 0; x < image.width(); ++x) {
                const auto grey = qGray(line[x]);
                line[x] = qRgba(grey, grey, grey, qAlpha(line[x]));
            }
        }
    image.setDevicePixelRatio(ratio);
    return QPixmap::fromImage(image);
}

QPixmap outline_pixmap(const QString& name, const QColor& ink, int size) {
    QFile file(QStringLiteral(":/erdflow/icons-outline/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly)) return {};
    // The same substitution glyph_icon makes: the file names its colour as the
    // text's, which Qt's renderer does not resolve, so the ink goes in first.
    auto drawing = file.readAll();
    drawing.replace("currentColor", ink.name().toLatin1());
    QSvgRenderer renderer(drawing);
    if (!renderer.isValid()) return {};
    QPixmap pixmap(QSize(size, size) * 3);
    pixmap.setDevicePixelRatio(3);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, size, size));
    return pixmap;
}

QPixmap solid_pixmap(const QString& name, const QColor& ink, int size) {
    QFile file(QStringLiteral(":/erdflow/icons-outline/%1.svg").arg(name));
    if (!file.open(QIODevice::ReadOnly)) return {};
    // The file says its shapes are not filled; filled here in the one ink.
    auto drawing = file.readAll();
    drawing.replace("fill=\"none\"", "fill=\"currentColor\"");
    drawing.replace("currentColor", ink.name().toLatin1());
    QSvgRenderer renderer(drawing);
    if (!renderer.isValid()) return {};
    QPixmap pixmap(QSize(size, size) * 3);
    pixmap.setDevicePixelRatio(3);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    renderer.render(&painter, QRectF(0, 0, size, size));
    return pixmap;
}

namespace {
// The ribbon's glyphs, which have no painted drawing: in the painted set they
// are their line art, and in the coloured set too wherever it has no artwork
// of the same name.
bool line_art_only(Glyph glyph) {
    switch (glyph) {
    case Glyph::Import: case Glyph::View: case Glyph::Help:
    case Glyph::Background: case Glyph::IconSet: case Glyph::Notation: case Glyph::Lines:
    case Glyph::ProjectFile: case Glyph::PdfDocument: case Glyph::DataDictionary: case Glyph::HtmlReport:
    case Glyph::CsvListing: case Glyph::SvgPicture: case Glyph::PdfPage: case Glyph::MorePictures:
    case Glyph::CopyPicture: case Glyph::ProjectPicture: case Glyph::OtherTool:
    case Glyph::ActualSize: case Glyph::ZoomIn: case Glyph::ZoomOut: case Glyph::Grid:
    case Glyph::AlignToGrid: case Glyph::CanvasControls: case Glyph::Comments: case Glyph::History:
    case Glyph::Guide: case Glyph::About:
    case Glyph::FileTab: case Glyph::Home: case Glyph::Insert: case Glyph::Settings: case Glyph::NoPanels:
    case Glyph::Model:
        return true;
    default:
        return false;
    }
}

// A glyph from the line-art set, or nothing where the set has no such file.
QIcon line_art_icon(Glyph glyph, const Theme& colors, int size) {
    // The line art is drawn in one colour, named in the file as the colour
    // of the surrounding text. Qt's renderer does not resolve that itself,
    // so the ink asked for is put in its place before the file is drawn --
    // which is what makes one set of files serve every palette.
    // The line-art set files its table as relational, the level it is
    // the unit of; every other glyph goes by the same name in both sets.
    QFile file(QStringLiteral(":/erdflow/icons-outline/%1.svg")
                   .arg(glyph == Glyph::Table ? QStringLiteral("relational") : icon_name(glyph)));
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
    return {};
}

QIcon inked_icon(Glyph glyph, const Theme& colors, int size, IconMode mode) {
    if (mode == IconMode::Outline) {
        const auto icon = line_art_icon(glyph, colors, size);
        if (!icon.isNull()) return icon;
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
        // A name the set has no file for is not asked for at all: Qt's reader
        // says so on the console every time it is asked.
        const auto path = QStringLiteral(":/erdflow/%1/%2.svg")
                              .arg(on_dark ? QStringLiteral("icons-on-dark") : QStringLiteral("icons"),
                                   icon_name(glyph));
        if (QFile::exists(path)) {
            QIcon artwork(path);
            if (!artwork.pixmap(size).isNull()) return artwork;
        }
        // A missing file must not leave a button blank, so the drawn glyph
        // stands in. Nothing else in the window has to know it happened.
    }
    // A glyph with no drawing of its own is its line art in every set.
    if (line_art_only(glyph)) {
        const auto icon = line_art_icon(glyph, colors, size);
        if (!icon.isNull()) return icon;
    }
    QPixmap pixmap(QSize(size, size) * 3);
    pixmap.setDevicePixelRatio(3);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    draw(painter, glyph, colors, size);
    return QIcon(pixmap);
}

// A drawing with its colour taken out: every pixel the grey of its own
// brightness, its opacity untouched.
QPixmap greyed_pixmap(const QPixmap& drawn) {
    auto image = drawn.toImage().convertToFormat(QImage::Format_ARGB32);
    for (int y = 0; y < image.height(); ++y) {
        auto* row = reinterpret_cast<QRgb*>(image.scanLine(y));
        for (int x = 0; x < image.width(); ++x) {
            const auto level = qGray(row[x]);
            row[x] = qRgba(level, level, level, qAlpha(row[x]));
        }
    }
    auto grey = QPixmap::fromImage(image);
    grey.setDevicePixelRatio(drawn.devicePixelRatio());
    return grey;
}

// An icon in greys, state by state. Only the states it was given are carried
// over; Qt makes the rest from them, as it would have from the coloured one.
QIcon without_colour(const QIcon& icon, int size) {
    QIcon grey;
    for (const auto state : {QIcon::Off, QIcon::On})
        for (const auto mode : {QIcon::Normal, QIcon::Active, QIcon::Selected}) {
            const bool resting = mode == QIcon::Normal && state == QIcon::Off;
            if (!resting && icon.availableSizes(mode, state).isEmpty()) continue;
            const auto drawn = icon.pixmap(QSize(size, size), 3.0, mode, state);
            if (!drawn.isNull()) grey.addPixmap(greyed_pixmap(drawn), mode, state);
        }
    return grey;
}
} // namespace

QIcon glyph_icon(Glyph glyph, const Theme& colors, int size, IconMode mode) {
    // A theme with no colour of its own has none in its icons either, whichever
    // set they come from -- the coloured artwork included, which otherwise
    // keeps its colours under every theme (Zain, 2026-09-24).
    auto icon = inked_icon(glyph, colors, size, mode);
    return colourless(colors.id) ? without_colour(icon, size) : icon;
}

} // namespace erdflow::desktop
