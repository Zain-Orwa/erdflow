#include "icons.hpp"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QPolygonF>

namespace erdflow::desktop {
namespace {

// An arrowhead at `tip`, opening back along `back`, used by the several glyphs
// that are an arrow of some kind.
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
    const QRectF box(2.5, 2.5, side - 5, side - 5);
    const auto centre = box.center();
    QPen pen(ink, 1.5);
    pen.setJoinStyle(Qt::RoundJoin);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);

    switch (glyph) {
    case Glyph::New:
        painter.drawRect(box.adjusted(2, 0, -2, 0));
        break;
    case Glyph::Open: {
        // A folder: a tab along the top edge and the body beneath it.
        QPainterPath folder(QPointF(box.left(), box.bottom()));
        folder.lineTo(box.left(), box.top() + 2);
        folder.lineTo(box.left() + box.width() * 0.4, box.top() + 2);
        folder.lineTo(box.left() + box.width() * 0.5, box.top() + 4.5);
        folder.lineTo(box.right(), box.top() + 4.5);
        folder.lineTo(box.right(), box.bottom());
        folder.closeSubpath();
        painter.drawPath(folder);
        break;
    }
    case Glyph::Save:
        painter.drawRect(box);
        painter.setBrush(accent);
        painter.setPen(Qt::NoPen);
        painter.drawRect(QRectF(box.left() + 3, box.top(), box.width() - 6, box.height() * 0.34));
        break;
    case Glyph::Undo:
    case Glyph::Redo: {
        // A hooked arrow. Redo is the same path mirrored about the centre.
        const bool forward = glyph == Glyph::Redo;
        painter.save();
        if (forward) {
            painter.translate(centre.x() * 2, 0);
            painter.scale(-1, 1);
        }
        QPainterPath hook(QPointF(box.right(), box.bottom() - 1));
        hook.cubicTo(QPointF(box.right(), box.top() + 3), QPointF(box.left() + 3, box.top() + 1),
                     QPointF(box.left() + 1.5, box.top() + 4));
        painter.drawPath(hook);
        arrow_head(painter, QPointF(box.left() + 1, box.top() + 6.5), QPointF(0.6, -1), 2.4, ink);
        painter.restore();
        break;
    }
    case Glyph::Select: {
        QPolygonF cursor;
        cursor << QPointF(box.left() + 2, box.top() + 1) << QPointF(box.left() + 2, box.bottom() - 1)
               << QPointF(box.left() + 5.5, box.bottom() - 4.5) << QPointF(box.right() - 3, box.bottom() - 5);
        painter.setBrush(ink);
        painter.drawPolygon(cursor);
        break;
    }
    case Glyph::Entity:
        painter.setBrush(colors.entity_fill);
        painter.setPen(QPen(colors.entity_border, 1.5));
        painter.drawRect(box.adjusted(0, 2.5, 0, -2.5));
        break;
    case Glyph::Attribute:
        painter.setBrush(colors.attribute_fill);
        painter.setPen(QPen(colors.attribute_border, 1.5));
        painter.drawEllipse(box.adjusted(0, 3, 0, -3));
        break;
    case Glyph::Relationship: {
        QPolygonF diamond;
        diamond << QPointF(centre.x(), box.top()) << QPointF(box.right(), centre.y())
                << QPointF(centre.x(), box.bottom()) << QPointF(box.left(), centre.y());
        painter.setBrush(colors.relationship_fill);
        painter.setPen(QPen(colors.relationship_border, 1.5));
        painter.drawPolygon(diamond);
        break;
    }
    case Glyph::Isa: {
        QPolygonF triangle;
        triangle << QPointF(centre.x(), box.top() + 1) << QPointF(box.right(), box.bottom() - 1)
                 << QPointF(box.left(), box.bottom() - 1);
        painter.setBrush(colors.relationship_fill);
        painter.setPen(QPen(colors.relationship_border, 1.5));
        painter.drawPolygon(triangle);
        break;
    }
    case Glyph::Connect: {
        const QPointF from(box.left() + 2, box.bottom() - 2);
        const QPointF to(box.right() - 2, box.top() + 2);
        painter.setPen(QPen(colors.connector, 1.6));
        painter.drawLine(from, to);
        painter.setPen(Qt::NoPen);
        painter.setBrush(accent);
        painter.drawEllipse(from, 2.4, 2.4);
        painter.drawEllipse(to, 2.4, 2.4);
        break;
    }
    case Glyph::Pan:
        // Four short arms, which reads as "move in any direction".
        painter.drawLine(QPointF(centre.x(), box.top() + 1), QPointF(centre.x(), box.bottom() - 1));
        painter.drawLine(QPointF(box.left() + 1, centre.y()), QPointF(box.right() - 1, centre.y()));
        arrow_head(painter, QPointF(centre.x(), box.top()), QPointF(0, 1), 2.2, ink);
        arrow_head(painter, QPointF(centre.x(), box.bottom()), QPointF(0, -1), 2.2, ink);
        arrow_head(painter, QPointF(box.left(), centre.y()), QPointF(1, 0), 2.2, ink);
        arrow_head(painter, QPointF(box.right(), centre.y()), QPointF(-1, 0), 2.2, ink);
        break;
    case Glyph::Fit: {
        painter.setPen(QPen(ink, 1.3, Qt::DashLine));
        painter.drawRect(box);
        painter.setPen(pen);
        const qreal arm = box.width() * 0.28;
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
        QPainterPath tick(QPointF(box.left() + 1, centre.y()));
        tick.lineTo(centre.x() - 1, box.bottom() - 2);
        tick.lineTo(box.right(), box.top() + 1);
        painter.setPen(QPen(accent, 2.2, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter.drawPath(tick);
        break;
    }
    case Glyph::Duplicate:
        painter.drawRect(QRectF(box.left(), box.top() + 3, box.width() - 3, box.height() - 3));
        painter.setBrush(colors.panel);
        painter.drawRect(QRectF(box.left() + 3, box.top(), box.width() - 3, box.height() - 3));
        break;
    case Glyph::Rename: {
        painter.drawLine(QPointF(box.left() + 1, box.bottom()), QPointF(box.right() - 3, box.bottom()));
        QPainterPath nib(QPointF(box.left() + 2, box.bottom() - 3));
        nib.lineTo(box.right() - 2, box.top() + 1);
        painter.setPen(QPen(accent, 2.0, Qt::SolidLine, Qt::RoundCap));
        painter.drawPath(nib);
        break;
    }
    case Glyph::Delete:
        painter.drawLine(QPointF(box.left() + 1, box.top() + 3), QPointF(box.right() - 1, box.top() + 3));
        painter.drawRect(QRectF(box.left() + 2.5, box.top() + 3, box.width() - 5, box.height() - 3));
        break;
    }
}

} // namespace

QIcon glyph_icon(Glyph glyph, const Theme& colors, int size) {
    QPixmap pixmap(QSize(size, size) * 3);
    pixmap.setDevicePixelRatio(3);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    draw(painter, glyph, colors, size);
    return QIcon(pixmap);
}

} // namespace erdflow::desktop
