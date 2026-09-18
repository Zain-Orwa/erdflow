#include "diagram_view.hpp"

#include <QApplication>
#include <QColorDialog>
#include <QIcon>
#include <QContextMenuEvent>
#include <QMenu>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QTransform>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollBar>
#include <QSlider>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>
#include <QWidgetAction>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <utility>

namespace erdflow::desktop {
namespace {
using namespace domain;

constexpr qreal grid_spacing = 20;
// A body of the given size centred on a point.
Rect centred(const QPointF& centre, const BodySize& size) {
    return {centre.x() - size.width / 2, centre.y() - size.height / 2, size.width, size.height};
}
constexpr qreal minimum_zoom = 0.15;
constexpr qreal maximum_zoom = 3.0;

// What the pointer shows when it comes to rest on something carrying remarks:
// what the thing is, then what was said about it. Rich text, because a remark
// is prose somebody wrote and may run to several lines.
QString comment_tooltip(const Project& project, const std::vector<CommentId>& ids, const QString& about,
                        bool shown) {
    if (ids.empty() || !shown) return about;
    QStringList said;
    for (const auto& id : ids) {
        const auto found = project.comments.find(id);
        // A remark put away on its own stays put away while the rest are shown.
        if (found == project.comments.end() || found->second.hidden) continue;
        said << QString::fromStdString(found->second.text).toHtmlEscaped().replace('\n', QStringLiteral("<br>"));
    }
    if (said.isEmpty()) return about;
    return QStringLiteral("<p>%1</p><hr><p>%2</p>")
        .arg(about.toHtmlEscaped(), said.join(QStringLiteral("</p><p>")));
}
// The mark that says somebody has left a remark here: a small speech bubble in
// the theme's warning colour, which is the one hue no shape on the canvas
// wears, so a mark is never mistaken for part of the diagram.
//
// It is drawn whenever a remark is pinned here, whether remarks are being shown
// or not, because a remark nobody can see is a remark nobody can find: hiding
// is meant to quiet the diagram, not to lose what a reviewer said. It is solid
// when pointing at it would say something and hollow when every remark here has
// been put away or the whole lot switched off.
constexpr qreal comment_badge_size = 13;
void draw_comment_badge(QPainter* painter, const QPointF& corner, const QColor& ink, bool solid) {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing);
    const QRectF bubble(corner.x(), corner.y(), comment_badge_size, comment_badge_size * 0.78);
    QPainterPath path;
    path.addRoundedRect(bubble, 3, 3);
    // The tail, so the mark reads as something said rather than as a sticker.
    QPainterPath tail(QPointF(bubble.left() + bubble.width() * 0.26, bubble.bottom() - 0.5));
    tail.lineTo(bubble.left() + bubble.width() * 0.22, bubble.bottom() + bubble.height() * 0.42);
    tail.lineTo(bubble.left() + bubble.width() * 0.55, bubble.bottom() - 0.5);
    tail.closeSubpath();
    path = path.united(tail);
    QPen pen(ink, 1.1);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(solid ? QBrush(ink) : Qt::NoBrush);
    painter->drawPath(path);
    painter->restore();
}


// The palette offered on the canvas. These are surface colours rather than ink,
// so each is light enough to write on and distinct from its neighbours at the
// size an element is actually drawn. The names are what the menu reads out, so
// they say what the eye sees rather than naming a hex value.
const std::array<std::pair<const char*, QColor>, 10>& swatches() {
    static const std::array<std::pair<const char*, QColor>, 10> palette{{
        {"Butter", QColor(0xFF, 0xE0, 0x8A)}, {"Apricot", QColor(0xFF, 0xC2, 0x8A)},
        {"Coral", QColor(0xFF, 0xA8, 0xA8)}, {"Rose", QColor(0xF7, 0xA8, 0xD8)},
        {"Lilac", QColor(0xC9, 0xB0, 0xFF)}, {"Periwinkle", QColor(0xA8, 0xBD, 0xFF)},
        {"Sky", QColor(0x9A, 0xDC, 0xFF)}, {"Mint", QColor(0x9E, 0xE8, 0xC4)},
        {"Sage", QColor(0xC3, 0xE0, 0x9E)}, {"Stone", QColor(0xD6, 0xD6, 0xD6)},
    }};
    return palette;
}

// A swatch drawn as its own icon, so the menu shows the colour rather than only
// naming it.
QIcon swatch_icon(const QColor& colour) {
    QPixmap pixmap(32, 32);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setBrush(colour);
    painter.setPen(QPen(colour.darker(150), 1.2));
    painter.drawEllipse(QRectF(1.5, 1.5, 13, 13));
    return QIcon(pixmap);
}

// The shape a new link is given when its ends are pinned where the user
// clicked. Either end may be left to route itself.
Connector joined(std::optional<double> owner, std::optional<double> child) {
    Connector shape;
    shape.owner_anchor = owner;
    shape.child_anchor = child;
    return shape;
}

QPointF normal(const QPointF& delta) {
    const auto length = std::hypot(delta.x(), delta.y());
    return length > 0.001 ? QPointF(-delta.y() / length, delta.x() / length) : QPointF(0, 1);
}

// One definition of how a participant end is drawn, used both by the canvas and
// by the previews in the notation picker, so a picker can never show something
// the diagram does not draw. Crow's foot places the maximum against the entity
// and the minimum just inboard of it; Bachman uses an arrowhead for "many" and
// a circle whose fill states whether the side is mandatory.
void draw_participant_end(QPainter* painter, Notation notation, bool many, bool mandatory,
                          const QPointF& end, const QPointF& outward,
                          const QColor& ink, const QColor& paper) {
    if (notation != Notation::CrowsFoot && notation != Notation::Bachman) return;
    const QPointF u = outward;
    const QPointF n = normal(u);
    painter->save();
    QPen pen(ink, 1.6);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    if (notation == Notation::CrowsFoot) {
        if (many) {
            const auto apex = end + u * 15;
            painter->drawLine(apex, end + n * 7);
            painter->drawLine(apex, end - n * 7);
            painter->drawLine(apex, end);
        } else {
            const auto bar = end + u * 13;
            painter->drawLine(bar + n * 7, bar - n * 7);
        }
        const auto inner = end + u * 25;
        if (mandatory) {
            painter->drawLine(inner + n * 7, inner - n * 7);
        } else {
            painter->setBrush(paper);
            painter->drawEllipse(inner, 4.5, 4.5);
        }
    } else {
        if (many) {
            QPolygonF head;
            head << end << end + u * 13 + n * 5 << end + u * 13 - n * 5;
            painter->setBrush(ink);
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(head);
            if (mandatory) painter->drawEllipse(end + u * 20, 4.5, 4.5);
        } else {
            painter->setBrush(mandatory ? ink : paper);
            painter->drawEllipse(end + u * 9, 5, 5);
        }
    }
    painter->restore();
}

// These items are projections only: all persistent changes go through Editor.
class NodeItem final : public QGraphicsItem {
public:
    NodeItem(ElementRef reference, const Theme& colors) : ref(std::move(reference)) {
        setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
        // A picture lies beneath the lines and the elements, so it can serve
        // as a background they are drawn over; everything else sits above the lines.
        setZValue(std::holds_alternative<PictureId>(ref) ? -2 : 1);
        set_theme(colors);
    }

    ElementRef ref;
    QString label;
    // How many remarks are pinned here, and how many of those would be shown
    // if the pointer came to rest on this. The second is zero when every one of
    // them has been put away, which is drawn differently from having none.
    int comments = 0;
    int comments_to_show = 0;
    // Whether a search found this. A found element keeps its full strength
    // while the rest recede, which is most of what makes it stand out; the
    // ring says which of the things still at full strength were asked for and
    // which are only there because they are next to one.
    bool found = false;
    // A note's text, drawn beneath its title.
    QString body;
    // A plain note is one character standing on its own, drawn as the
    // character alone: no card, no border, no title.
    bool plain = false;
    AttributeKind attribute_kind = AttributeKind::Normal;
    // An associative relationship is drawn as its diamond inside a rectangle,
    // because it takes part in further relationships as an entity would.
    bool associative = false;
    // A weak entity wears a second border and an identifying relationship a
    // second diamond: the pair that identify together are marked alike. A
    // weak entity's key is only a partial key, underlined in dashes.
    bool weak = false;
    bool identifying = false;
    bool partial_key = false;
    // Whether an ISA triangle's subtypes are disjoint, for the letter it wears.
    bool disjoint = true;
    // An ISA triangle points up when generalising and down when specialising.
    bool generalising = false;
    std::function<void(NodeItem*)> moved;
    std::function<QPointF(QPointF)> constrain;

    void set_theme(const Theme& colors) {
        colors_ = &colors;
        apply_colors();
    }
    // Colour depends on the associative flag, so changing it re-picks the palette.
    void set_associative(bool value) {
        if (associative == value) return;
        associative = value;
        apply_colors();
    }

    // A colour the user chose for this element, which overrides the theme's.
    void set_chosen_colour(std::optional<QColor> colour) {
        if (chosen_ == colour) return;
        chosen_ = colour;
        apply_colors();
    }
    [[nodiscard]] const std::optional<QColor>& chosen_colour() const { return chosen_; }
    // How see-through the surface is, in percent, over whatever colour it has.
    void set_transparency(int percent) {
        if (transparency_ == percent) return;
        transparency_ = percent;
        apply_colors();
    }
    [[nodiscard]] int transparency() const { return transparency_; }

    // A picture's own pixels, decoded once from the bytes the model holds.
    void set_image(const std::vector<std::uint8_t>& bytes) {
        image_.loadFromData(bytes.data(), static_cast<uint>(bytes.size()));
        update();
    }
    [[nodiscard]] bool has_image() const { return !image_.isNull(); }

    void apply_colors() {
        const auto& colors = *colors_;
        fill_ = colors.attribute_fill;
        border_ = colors.attribute_border;
        if (std::holds_alternative<EntityId>(ref)) {
            fill_ = colors.entity_fill;
            border_ = colors.entity_border;
        } else if (std::holds_alternative<PictureId>(ref)) {
            // A picture is its own surface. A frame is drawn only around a
            // selection or a chosen colour, which gives it a mount to sit on.
            fill_ = colors.panel;
            border_ = colors.border;
        } else if (std::holds_alternative<NoteId>(ref)) {
            fill_ = note_surface(colors);
            border_ = colors.warning;
        } else if (std::holds_alternative<SpecializationId>(ref)) {
            fill_ = colors.relationship_fill;
            border_ = colors.relationship_border;
        } else if (std::holds_alternative<RelationshipId>(ref)) {
            // An associative entity converts to a relation of its own, so it
            // wears the entity palette. Its unfilled surrounding rectangle,
            // not its colour, is what keeps it distinct from an entity.
            const bool as_entity = associative;
            fill_ = as_entity ? colors.entity_fill : colors.relationship_fill;
            border_ = as_entity ? colors.entity_border : colors.relationship_border;
        }
        text_ = colors.node_text;
        // A note's surface is derived rather than chosen by the theme, so its
        // text is picked against it the way a chosen colour's is.
        if (std::holds_alternative<NoteId>(ref)) text_ = readable_on(fill_);
        if (chosen_) {
            // A chosen colour replaces the fill and takes the border with it, or
            // the surface would sit inside an outline from a palette it has left.
            fill_ = *chosen_;
            border_ = chosen_->darker(145);
            // The label has to stay readable on a colour the theme knows nothing
            // about, so it is chosen against the surface rather than the theme.
            text_ = readable_on(*chosen_);
        }
        if (transparency_ > 0) {
            // The fill fades; the outline stays, so the shape can still be read.
            // The label is chosen against what the eye sees, which is the
            // canvas showing through the faded surface.
            fill_.setAlphaF(static_cast<float>(1.0 - transparency_ / 100.0));
            text_ = readable_on(over(colors.canvas, fill_));
        }
        selection_ = colors.accent;
        update();
    }

    // A symbol's corner grips straddle its corners and so reach further out
    // than the box itself. Only a symbol carries them, and the margin stays
    // where it was for everything else: a body's bounding rect is read as the
    // shape it draws, and padding it would move every join on the diagram.
    QRectF boundingRect() const override {
        const auto margin = plain ? grip : 4;
        return bounds_.adjusted(-margin, -margin, margin, margin);
    }
    QRectF body_rect() const { return bounds_; }
    // The element's own Chen outline, without the rectangle that surrounds an
    // associative one. This is what carries the fill.
    QPainterPath body_path() const {
        QPainterPath path;
        if (std::holds_alternative<EntityId>(ref) || std::holds_alternative<PictureId>(ref)) {
            path.addRect(bounds_);
        } else if (std::holds_alternative<NoteId>(ref)) {
            path.addRoundedRect(bounds_, 6, 6);
        } else if (std::holds_alternative<AttributeId>(ref)) {
            path.addEllipse(bounds_);
        } else if (std::holds_alternative<SpecializationId>(ref)) {
            // The triangle points the way the hierarchy was read: up at the
            // supertype when generalising, down at the subtypes when specialising.
            if (generalising) {
                path.moveTo(bounds_.center().x(), bounds_.top());
                path.lineTo(bounds_.right(), bounds_.bottom());
                path.lineTo(bounds_.left(), bounds_.bottom());
            } else {
                path.moveTo(bounds_.center().x(), bounds_.bottom());
                path.lineTo(bounds_.right(), bounds_.top());
                path.lineTo(bounds_.left(), bounds_.top());
            }
            path.closeSubpath();
        } else {
            path = diamond(bounds_);
        }
        return path;
    }
    static QPainterPath diamond(const QRectF& box) {
        QPainterPath path;
        path.moveTo(box.center().x(), box.top());
        path.lineTo(box.right(), box.center().y());
        path.lineTo(box.center().x(), box.bottom());
        path.lineTo(box.left(), box.center().y());
        path.closeSubpath();
        return path;
    }
    QPainterPath shape() const override {
        auto path = body_path();
        // Hit testing covers the surrounding rectangle so its corners can be
        // clicked, even though only the diamond inside it is filled.
        if (associative) {
            path.setFillRule(Qt::WindingFill);
            path.addRect(bounds_);
        }
        // A grip hangs half outside the box, so the half that does would miss
        // the item entirely and the pointer would fall through to the canvas.
        if (sizeable()) {
            path.setFillRule(Qt::WindingFill);
            for (const auto& corner : grips()) path.addRect(corner);
        }
        return path;
    }
    // A symbol is drawn as its character grown to fill its box, so the box is
    // how big the character is, and hauling a corner is how it is made bigger.
    // Nothing else on the diagram is sized by hand: an entity's box is sized
    // by the name it has to hold, so only a symbol carries grips.
    static constexpr qreal grip = 9;
    [[nodiscard]] bool sizeable() const { return plain && isSelected(); }
    // Clockwise from the top left, which is the order the corners are named in
    // everywhere below, so a corner's number says which one it is.
    [[nodiscard]] std::array<QRectF, 4> grips() const {
        const std::array<QPointF, 4> corners{bounds_.topLeft(), bounds_.topRight(),
                                             bounds_.bottomRight(), bounds_.bottomLeft()};
        std::array<QRectF, 4> rects{};
        for (std::size_t i = 0; i < corners.size(); ++i)
            rects[i] = QRectF(corners[i] - QPointF(grip / 2, grip / 2), QSizeF(grip, grip));
        return rects;
    }
    // Which grip is under a point given in this item's own coordinates, or -1.
    [[nodiscard]] int grip_at(const QPointF& point) const {
        if (!sizeable()) return -1;
        const auto corners = grips();
        for (std::size_t i = 0; i < corners.size(); ++i)
            if (corners[i].contains(point)) return static_cast<int>(i);
        return -1;
    }
    // prepareGeometryChange is the scene's business and is protected, so the
    // projection that changes what an item is asks for it by name.
    void about_to_change_geometry() { prepareGeometryChange(); }
    void set_size(qreal width, qreal height) {
        const QRectF next{0, 0, width, height};
        if (bounds_ == next) return;
        prepareGeometryChange();
        bounds_ = next;
        update();
    }
    // Attributes owned by this element, so their links can be laid out against
    // the body they belong to rather than each being routed on its own.
    std::vector<NodeItem*> attribute_children;

    // Where a link to an attribute leaves this body, and the point just outside
    // it where the line straightens out.
    //
    // The anchor rides the outline itself, at whatever point the attribute's own
    // direction crosses it, so moving the attribute slides the join smoothly
    // around the body and carries it around the corners. Snapping to the middle
    // of whichever face is nearest is what made the line jump: the anchor would
    // sit still while the attribute moved, then leap the width of the body the
    // moment the nearest face changed.
    void attribute_trunk(const NodeItem* child, const std::optional<double>& pinned,
                         QPointF& anchor, QPointF& junction) const {
        const QPointF centre = scenePos() + bounds_.center();
        anchor = pinned ? boundary_at(*pinned)
                        : boundary_toward(child->scenePos() + child->bounds_.center());
        // The stub leaves along the outline's own outward direction rather than
        // pointing straight back at the attribute, so the line looks like it
        // leaves the body squarely and still has somewhere to curve from.
        // Dividing each axis by its own radius turns the corners smoothly
        // instead of snapping between four fixed headings.
        const auto rx = std::max(bounds_.width() / 2, 0.001);
        const auto ry = std::max(bounds_.height() / 2, 0.001);
        QPointF out{(anchor.x() - centre.x()) / (rx * rx), (anchor.y() - centre.y()) / (ry * ry)};
        const auto length = std::hypot(out.x(), out.y());
        junction = anchor + (length > 0.000001 ? out / length : QPointF(1, 0)) * 22;
    }

    // The ISA triangle attaches at fixed points rather than wherever a ray
    // happens to cross it: the supertype meets its top, the subtypes its bottom.
    // Which of those is the apex follows the direction the triangle points, so
    // the line never slides off the point as things move around it.
    QPointF isa_anchor(bool toward_supertype) const {
        const QPointF centre = scenePos() + bounds_.center();
        return {centre.x(), scenePos().y() + (toward_supertype ? bounds_.top() : bounds_.bottom())};
    }
    // A pinned join is stored as a direction rather than a point, so that it
    // keeps its place on the outline when the shape is moved or resized. These
    // two convert between that direction and the point it names.
    [[nodiscard]] QPointF boundary_at(double radians) const {
        const QPointF centre = scenePos() + bounds_.center();
        return boundary_toward(centre + QPointF(std::cos(radians), std::sin(radians)) * 1000.0);
    }
    [[nodiscard]] double direction_of(const QPointF& point) const {
        const QPointF centre = scenePos() + bounds_.center();
        return std::atan2(point.y() - centre.y(), point.x() - centre.x());
    }
    // Intersection of a ray from the node center with its actual Chen shape.
    QPointF boundary_toward(const QPointF& target) const {
        const QPointF center = scenePos() + bounds_.center();
        auto delta = target - center;
        if (std::hypot(delta.x(), delta.y()) < 0.001) delta = {1, 0};
        const auto rx = bounds_.width() / 2;
        const auto ry = bounds_.height() / 2;
        qreal divisor;
        if (std::holds_alternative<AttributeId>(ref)) {
            divisor = std::hypot(delta.x() / rx, delta.y() / ry);
        } else if (std::holds_alternative<SpecializationId>(ref)) {
            divisor = std::max(std::abs(delta.x()) / rx, std::abs(delta.y()) / ry);
        } else if (std::holds_alternative<RelationshipId>(ref) && !associative) {
            divisor = std::abs(delta.x()) / rx + std::abs(delta.y()) / ry;
        } else {
            divisor = std::max(std::abs(delta.x()) / rx, std::abs(delta.y()) / ry);
        }
        return center + delta / divisor;
    }
    // A picture sits straight on the canvas; only a selection or a chosen
    // colour draws a frame, and the colour gives it a mount to sit on.
    void paint_picture(QPainter* painter) {
        const bool mounted = chosen_.has_value();
        // A faded picture is the whole picture faded, mount and all.
        painter->setOpacity(1.0 - transparency_ / 100.0);
        if (mounted || isSelected()) {
            painter->setPen(QPen(isSelected() ? selection_ : border_, isSelected() ? 2.4 : 1.6));
            painter->setBrush(mounted ? QBrush(fill_) : QBrush(Qt::NoBrush));
            painter->drawRect(bounds_);
        }
        if (image_.isNull()) {
            painter->setPen(text_);
            painter->drawText(bounds_, Qt::AlignCenter, QStringLiteral("Picture"));
            return;
        }
        // Fitted to the body without being stretched, so resizing the body
        // never distorts the picture, and drawn from the pixels themselves so
        // zooming in shows more of them rather than a scaled copy.
        const auto inset = mounted ? bounds_.adjusted(6, 6, -6, -6) : bounds_;
        const auto size = QSizeF(image_.size()).scaled(inset.size(), Qt::KeepAspectRatio);
        const QRectF target(inset.center() - QPointF(size.width() / 2, size.height() / 2), size);
        painter->setRenderHint(QPainter::SmoothPixmapTransform);
        painter->drawPixmap(target, image_, QRectF(QPointF(0, 0), QSizeF(image_.size())));
    }
    // A note: its title in bold across the top, and its text wrapped beneath.
    // A plain note: the character and nothing else, sized to the room it has,
    // the way an emoji sits in a line of chat. Only a selection draws anything
    // around it, and only so that it can be seen to be selected.
    void paint_plain_note(QPainter* painter) {
        if (isSelected()) {
            painter->setPen(QPen(selection_, 1.2, Qt::DashLine));
            painter->setBrush(Qt::NoBrush);
            painter->drawRoundedRect(bounds_.adjusted(-2, -2, 2, 2), 4, 4);
            // The corners are drawn as grips so that a symbol says it can be
            // made bigger. Filled with the paper rather than the selection
            // colour: a solid square on each corner would read as part of the
            // character when the character is small.
            painter->setPen(QPen(selection_, 1.0));
            painter->setBrush(colors_->canvas);
            for (const auto& corner : grips()) painter->drawRect(corner);
        }
        if (label.isEmpty()) return;
        auto font = painter->font();
        // Grown until it fills the box rather than set to a fixed size, so the
        // character follows the box when the box is resized, as a picture does.
        // Emoji are square, so the height is what binds in practice; the width
        // is checked too, for a character that is wider than it is tall.
        font.setPointSizeF(10);
        font.setWeight(QFont::Normal);
        const QFontMetricsF small(font);
        const auto tall = small.tightBoundingRect(label).height();
        const auto wide = small.horizontalAdvance(label);
        if (tall > 0.1 && wide > 0.1) {
            const auto room = std::min(bounds_.height() * 0.86 / tall, bounds_.width() * 0.86 / wide);
            font.setPointSizeF(std::clamp(10 * room, 6.0, 260.0));
        }
        painter->setFont(font);
        // The character is drawn in whatever colour was chosen for it, and
        // otherwise in the ink the diagram writes with, so it reads on the
        // paper rather than on a card that is no longer there.
        painter->setPen(chosen_ ? *chosen_ : colors_->node_text);
        // Centred on the ink rather than on the line the character sits in.
        // A line is mostly space above and below the letter, and different
        // characters use different parts of it, so centring the line would
        // leave one character high and the next one low.
        const QFontMetricsF grown(font);
        const auto ink = grown.tightBoundingRect(label);
        painter->drawText(QPointF(bounds_.center().x() - ink.center().x(),
                                  bounds_.center().y() - ink.center().y()),
                          label);
    }
    void paint_note(QPainter* painter) {
        if (plain) { paint_plain_note(painter); return; }
        painter->setPen(QPen(isSelected() ? selection_ : border_, isSelected() ? 2.4 : 1.6));
        painter->setBrush(fill_);
        painter->drawPath(body_path());
        const auto inner = bounds_.adjusted(10, 8, -10, -8);
        painter->setClipRect(bounds_.adjusted(2, 2, -2, -2));
        painter->setPen(text_);
        auto font = painter->font();
        qreal used = 0;
        if (!label.isEmpty()) {
            font.setPointSizeF(12.5);
            font.setWeight(QFont::Bold);
            painter->setFont(font);
            const QRectF title(inner.left(), inner.top(), inner.width(), QFontMetricsF(font).height() * 1.25);
            painter->drawText(title, Qt::AlignLeft | Qt::AlignVCenter,
                              QFontMetricsF(font).elidedText(label, Qt::ElideRight, title.width()));
            used = title.height() + 3;
        }
        font.setPointSizeF(11);
        font.setWeight(QFont::Normal);
        painter->setFont(font);
        painter->drawText(QRectF(inner.left(), inner.top() + used, inner.width(), inner.height() - used),
                          Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, body);
    }
    // The element's own shape, drawn small and without its name. Anything that
    // wants to show what this element is uses this, so what it shows and what
    // the canvas draws are the same drawing and cannot drift apart.
    void paint_shape(QPainter* painter, const QRectF& into, bool fill_the_room = false) const {
        if (bounds_.width() < 1 || bounds_.height() < 1 || into.isEmpty()) return;
        // The shape is mapped into the room rather than the painter scaled, so
        // an outline keeps its weight even where the room is a different shape
        // from the element and the two axes are stretched by different amounts.
        const auto across = into.width() / bounds_.width();
        const auto down = into.height() / bounds_.height();
        const auto sideways = fill_the_room ? across : std::min(across, down);
        const auto upright = fill_the_room ? down : std::min(across, down);
        QTransform into_place;
        into_place.translate(into.center().x(), into.center().y());
        into_place.scale(sideways, upright);
        into_place.translate(-bounds_.center().x(), -bounds_.center().y());
        painter->save();
        if (std::holds_alternative<PictureId>(ref) && !image_.isNull()) {
            // A picture is its own picture, fitted into the same room.
            const auto room = into_place.mapRect(bounds_);
            const auto fitted = QSizeF(image_.size()).scaled(room.size(), Qt::KeepAspectRatio);
            painter->setRenderHint(QPainter::SmoothPixmapTransform);
            painter->drawPixmap(QRectF(room.center() - QPointF(fitted.width() / 2, fitted.height() / 2), fitted),
                                image_, QRectF(QPointF(0, 0), QSizeF(image_.size())));
            painter->restore();
            return;
        }
        if (plain) {
            // A plain note has no shape of its own; it is the character. Any
            // panel that shows what this element is shows exactly that, or it
            // would draw a card the diagram no longer has.
            const auto room = into_place.mapRect(bounds_);
            auto lettering = painter->font();
            lettering.setPointSizeF(10);
            const QFontMetricsF small(lettering);
            const auto tall = small.tightBoundingRect(label).height();
            const auto wide = small.horizontalAdvance(label);
            if (!label.isEmpty() && tall > 0.1 && wide > 0.1) {
                lettering.setPointSizeF(std::clamp(
                    10 * std::min(room.height() * 0.86 / tall, room.width() * 0.86 / wide), 4.0, 260.0));
                painter->setFont(lettering);
                painter->setPen(chosen_ ? *chosen_ : colors_->node_text);
                const QFontMetricsF grown(lettering);
                const auto ink = grown.tightBoundingRect(label);
                painter->drawText(QPointF(room.center().x() - ink.center().x(),
                                          room.center().y() - ink.center().y()), label);
            }
            painter->restore();
            return;
        }
        QPen pen(border_, 1.6);
        if (std::holds_alternative<AttributeId>(ref) && attribute_kind == AttributeKind::Derived)
            pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
        painter->setBrush(fill_.alpha() == 0 ? QBrush(Qt::NoBrush) : QBrush(fill_));
        painter->drawPath(into_place.map(body_path()));
        painter->setBrush(Qt::NoBrush);
        // The marks that say what kind of thing it is, exactly as on the canvas.
        if (associative) painter->drawRect(into_place.mapRect(bounds_));
        if (std::holds_alternative<AttributeId>(ref) && attribute_kind == AttributeKind::Multivalued)
            painter->drawEllipse(into_place.mapRect(bounds_.adjusted(5, 5, -5, -5)));
        if (weak) painter->drawRect(into_place.mapRect(bounds_.adjusted(5, 5, -5, -5)));
        if (identifying)
            painter->drawPath(into_place.map(diamond(bounds_.adjusted(bounds_.width() * 0.07, bounds_.height() * 0.09,
                                                                     -bounds_.width() * 0.07, -bounds_.height() * 0.09))));
        painter->restore();
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);
        if (std::holds_alternative<PictureId>(ref)) { paint_picture(painter); return; }
        if (std::holds_alternative<NoteId>(ref)) { paint_note(painter); return; }
        QPen pen(isSelected() ? selection_ : border_, isSelected() ? 2.4 : 1.6);
        if (std::holds_alternative<AttributeId>(ref) && attribute_kind == AttributeKind::Derived)
            pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
        painter->setBrush(fill_);
        painter->drawPath(body_path());
        // An associative entity's surrounding rectangle stays unfilled, so it
        // reads as a relationship wearing a box rather than as a solid entity.
        if (associative) {
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(bounds_);
        }
        if (std::holds_alternative<AttributeId>(ref) && attribute_kind == AttributeKind::Multivalued)
            painter->drawEllipse(bounds_.adjusted(5, 5, -5, -5));
        if (weak) {
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(bounds_.adjusted(5, 5, -5, -5));
        }
        if (identifying) {
            painter->setBrush(Qt::NoBrush);
            painter->drawPath(diamond(bounds_.adjusted(bounds_.width() * 0.07, bounds_.height() * 0.09,
                                                       -bounds_.width() * 0.07, -bounds_.height() * 0.09)));
        }
        // Names on the canvas are read at a glance and often at less than full
        // zoom, so they sit a step above the interface's own type and never
        // below medium weight.
        auto font = painter->font();
        font.setPointSizeF(12.5);
        font.setWeight(std::holds_alternative<EntityId>(ref) ? QFont::Bold : QFont::Medium);
        font.setUnderline(std::holds_alternative<AttributeId>(ref) && attribute_kind == AttributeKind::Key && !partial_key);
        painter->setFont(font);
        painter->setPen(text_);
        const auto inset = std::holds_alternative<RelationshipId>(ref) ? bounds_.width() * 0.22 : 15.0;
        auto text_rect = bounds_.adjusted(inset, 8, -inset, -8);
        // A triangle only has room for text across its base.
        if (std::holds_alternative<SpecializationId>(ref))
            text_rect = QRectF(bounds_.left() + 6, generalising ? bounds_.center().y() : bounds_.top() + 4,
                               bounds_.width() - 12, bounds_.height() / 2 - 4);
        const auto text = QFontMetricsF(font).elidedText(label, Qt::ElideRight, text_rect.width());
        painter->drawText(text_rect, Qt::AlignCenter, text);
        if (partial_key) {
            // A partial key is underlined in dashes: it identifies the weak
            // entity only together with its owner's key.
            const QFontMetricsF metrics(font);
            const auto width = metrics.horizontalAdvance(text);
            const auto baseline = text_rect.center().y() + (metrics.ascent() - metrics.descent()) / 2;
            QPen dashes(text_, 1.2, Qt::DashLine);
            painter->setPen(dashes);
            painter->drawLine(QPointF(text_rect.center().x() - width / 2, baseline + metrics.underlinePos() + 1),
                              QPointF(text_rect.center().x() + width / 2, baseline + metrics.underlinePos() + 1));
        }
        if (std::holds_alternative<SpecializationId>(ref)) {
            // The letter for the constraint, as Elmasri writes it: d for
            // disjoint, o for overlapping. It sits in the corner of the box the
            // triangle leaves free, beside its base.
            const qreal radius = std::clamp(bounds_.height() * 0.15, 7.0, 10.0);
            const QPointF at(bounds_.right() - radius - 1,
                             generalising ? bounds_.top() + radius + 1 : bounds_.bottom() - radius - 1);
            painter->setPen(QPen(isSelected() ? selection_ : border_, 1.2));
            painter->setBrush(fill_);
            painter->drawEllipse(at, radius, radius);
            auto small = painter->font();
            small.setPointSizeF(9.5);
            small.setWeight(QFont::DemiBold);
            small.setUnderline(false);
            painter->setFont(small);
            painter->setPen(text_);
            painter->drawText(QRectF(at.x() - radius, at.y() - radius, radius * 2, radius * 2), Qt::AlignCenter,
                              disjoint ? QStringLiteral("d") : QStringLiteral("o"));
        }
        paint_comment_badge(painter);
        paint_found_ring(painter);
    }
    // Drawn outside the shape rather than over it, so nothing a search found is
    // harder to read for having been found.
    void paint_found_ring(QPainter* painter) const {
        if (!found || !colors_) return;
        painter->save();
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setBrush(Qt::NoBrush);
        QColor halo = colors_->accent;
        halo.setAlpha(150);
        QPen ring(halo, 2.4);
        ring.setCosmetic(true);
        painter->setPen(ring);
        painter->drawRoundedRect(bounds_.adjusted(-3, -3, 3, 3), 5, 5);
        painter->restore();
    }
    // In the top-right of the box the shape is drawn in. It stays inside that
    // box rather than straddling its corner: the bounding rectangle is read as
    // the shape the element draws, and padding it to make room would move every
    // join on the diagram. Inside an ellipse or a diamond that corner is empty
    // anyway, and inside a rectangle it clears the centred name.
    void paint_comment_badge(QPainter* painter) const {
        if (comments <= 0 || !colors_) return;
        draw_comment_badge(painter, QPointF(bounds_.right() - comment_badge_size - 3, bounds_.top() + 3),
                           colors_->warning, comments_to_show > 0);
    }
protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == ItemPositionChange && constrain) return constrain(value.toPointF());
        const auto result = QGraphicsItem::itemChange(change, value);
        if (change == ItemPositionHasChanged && moved) moved(this);
        return result;
    }
private:
    QRectF bounds_{0, 0, 160, 80};
    const Theme* colors_ = nullptr;
    QColor fill_, border_, text_, selection_;
    std::optional<QColor> chosen_;
    int transparency_ = 0;
    QPixmap image_;
};

// An edge is keyed by the record that draws it. Attribute and participant links
// use the Domain's connector identity, so a canvas edge and a stored bend share
// one key. An inheritance link is keyed by its specialization and the entity it
// reaches, and carries no stored bend.
struct InheritanceKey {
    SpecializationId specialization;
    std::optional<EntityId> subtype;  // absent for the link up to the supertype
    auto operator<=>(const InheritanceKey&) const = default;
};
using EdgeKey = std::variant<AttributeId, ParticipantId, InheritanceKey>;
struct EdgeDescription {
    EdgeKey key;
    ElementRef from;
    ElementRef to;
    std::optional<RelationshipId> relationship;
    Cardinality cardinality = Cardinality::Many;
    Participation participation = Participation::Partial;
    // Whether this side's constraints are drawn. The constraints themselves are
    // unaffected; the line is simply bare at that end.
    bool show_constraints = true;
    QString role;
    qreal offset = 0;
    // Where the line is pinned to meet each shape, when the user has locked it.
    std::optional<double> owner_anchor;
    std::optional<double> child_anchor;
    // Corners the line is routed through, when the user has given it any.
    std::vector<QPointF> waypoints;
    // Which time this side meets the same element: 0 the first, 1 the second,
    // and so on. A relationship that meets one entity twice is recursive, and
    // its later sides cannot run straight or they would lie on the first.
    int recursion = 0;
    // A total specialization's link to its supertype, drawn as a double line.
    bool total = false;
    bool operator==(const EdgeDescription&) const = default;
};

class EdgeItem final : public QGraphicsItem {
public:
    EdgeItem(EdgeDescription description, NodeItem* from, NodeItem* to, const Theme& colors)
        : descriptor(std::move(description)), source(from), target(to) {
        setFlag(ItemIsSelectable);
        setZValue(-1);
        set_theme(colors);
        refresh();
    }
    EdgeDescription descriptor;
    // How many remarks are pinned to this line, and how many of them would be
    // shown if the pointer came to rest on it.
    int comments = 0;
    int comments_to_show = 0;
    // What this link is, without any remark pinned to it.
    QString plain_tooltip;
    NodeItem* source;
    NodeItem* target;
    Notation notation = Notation::Chen;
    LineStyle style = LineStyle::Elbow;

    // Every notation reads the same two values: the minimum from participation
    // and the maximum from cardinality.
    [[nodiscard]] bool mandatory() const { return descriptor.participation == Participation::Total; }
    [[nodiscard]] bool many() const { return descriptor.cardinality == Cardinality::Many; }
    // How far from the entity the drawn symbols extend, so labels sit clear of them.
    [[nodiscard]] qreal symbol_reach() const {
        if (!descriptor.relationship) return 0;
        if (notation == Notation::CrowsFoot) return mandatory() || !many() ? 26 : 16;
        if (notation == Notation::Bachman) return many() && mandatory() ? 26 : 16;
        return 0;
    }
    void paint_end_symbols(QPainter* painter, const QColor& ink, const QColor& paper) const {
        if (!descriptor.relationship) return;
        draw_participant_end(painter, notation, many(), mandatory(), end_, outward_, ink, paper);
    }
    [[nodiscard]] QString end_label() const {
        if (!descriptor.relationship) return {};
        if (notation == Notation::Chen) return many() ? QStringLiteral("M") : QStringLiteral("1");
        if (notation == Notation::MinMax)
            return QStringLiteral("(%1,%2)").arg(mandatory() ? "1" : "0", many() ? "M" : "1");
        return {};
    }

    void set_theme(const Theme& colors) {
        connector_ = colors.connector;
        selection_ = colors.accent;
        canvas_ = colors.canvas;
        text_ = colors.node_text;
        warning_ = colors.warning;
        update();
    }

    QRectF boundingRect() const override { return bounds_; }
    [[nodiscard]] QRectF comment_rect() const {
        return comments > 0 ? QRectF(comment_at_, QSizeF(comment_badge_size, comment_badge_size)) : QRectF();
    }
    QPainterPath shape() const override {
        QPainterPathStroker stroker;
        stroker.setWidth(12);
        auto hit = stroker.createStroke(path_);
        if (descriptor.relationship) hit.addRect(cardinality_rect_);
        if (!descriptor.role.isEmpty()) hit.addRect(role_rect_);
        if (isSelected() && shapeable()) hit.addRect(handle_rect_);
        if (isSelected() && shapeable()) for (const auto& corner : corner_rects_) hit.addRect(corner);
        if (isSelected() && shapeable()) { hit.addRect(owner_end_rect_); hit.addRect(child_end_rect_); }
        if (isSelected() && lockable()) hit.addRect(lock_rect_);
        return hit;
    }
    // An inheritance link is anchored to the triangle, so it carries no bend and
    // must not offer a handle that would do nothing.
    [[nodiscard]] bool shapeable() const { return !std::holds_alternative<InheritanceKey>(descriptor.key); }
    // An attribute's link runs from its owner to the attribute, the other way
    // round from how it is keyed, so the two ends do not map onto source and
    // target the same way for every kind of connector.
    [[nodiscard]] bool from_owner() const {
        return std::holds_alternative<AttributeId>(descriptor.key)
            && std::holds_alternative<AttributeId>(descriptor.from);
    }
    [[nodiscard]] NodeItem* head_node() const { return from_owner() ? target : source; }
    [[nodiscard]] NodeItem* tail_node() const { return from_owner() ? source : target; }
    // Every connector whose joins slide can have them pinned. Only inheritance
    // is excluded, since it is already anchored at fixed points on the triangle.
    [[nodiscard]] bool lockable() const { return shapeable(); }
    [[nodiscard]] bool locked() const { return descriptor.owner_anchor || descriptor.child_anchor; }
    [[nodiscard]] QRectF lock_rect() const { return lock_rect_; }
    // The two ends of a selected line can be taken hold of and put down at
    // another point on the same shape. Which end a point grabs: true for the
    // owner end, false for the child end, nothing for neither.
    [[nodiscard]] std::optional<bool> end_at(const QPointF& scene_point) const {
        if (!shapeable()) return std::nullopt;
        if (owner_end_rect_.contains(scene_point)) return true;
        if (child_end_rect_.contains(scene_point)) return false;
        return std::nullopt;
    }
    [[nodiscard]] QRectF owner_end_rect() const { return owner_end_rect_; }
    [[nodiscard]] QRectF child_end_rect() const { return child_end_rect_; }
    // Where the line currently meets each shape, as the direction that would
    // pin it exactly where it is now.
    [[nodiscard]] double owner_direction() const { return head_node()->direction_of(owner_join_); }
    [[nodiscard]] double child_direction() const { return tail_node()->direction_of(child_join_); }
    // A link touching the selection is drawn heavier and above the other links,
    // so what an element connects to can be read without tracing each line.
    void set_highlighted(bool value) {
        if (highlighted == value) return;
        highlighted = value;
        setZValue(value ? -0.5 : -1);
        update();
    }
    bool highlighted = false;
    // The bend handle only exists while the connector is selected, so an
    // unselected diagram stays free of grab targets.
    [[nodiscard]] QRectF handle_rect() const { return handle_rect_; }
    [[nodiscard]] const std::vector<QRectF>& corner_rects() const { return corner_rects_; }
    // Which corner a point grabs, or where a new one would be inserted to put a
    // corner at that point: the index of the segment it lies on.
    [[nodiscard]] std::optional<std::size_t> corner_at(const QPointF& scene_point) const {
        for (std::size_t index = 0; index < corner_rects_.size(); ++index)
            if (corner_rects_[index].contains(scene_point)) return index;
        return std::nullopt;
    }
    // The route already has corners before this point, walking the path from its
    // start, so a new corner dropped here belongs at that index.
    [[nodiscard]] std::size_t insertion_for(const QPointF& scene_point) const {
        std::size_t best = descriptor.waypoints.size();
        qreal nearest = std::numeric_limits<qreal>::max();
        QPointF previous = path_.pointAtPercent(0);
        for (std::size_t index = 0; index <= descriptor.waypoints.size(); ++index) {
            const auto next = index < descriptor.waypoints.size() ? descriptor.waypoints[index]
                                                                  : path_.currentPosition();
            const auto along = next - previous;
            const auto length = std::hypot(along.x(), along.y());
            if (length > 0.01) {
                const auto t = std::clamp(QPointF::dotProduct(scene_point - previous, along) / (length * length), 0.0, 1.0);
                const auto foot = previous + along * t;
                const auto gap = std::hypot(scene_point.x() - foot.x(), scene_point.y() - foot.y());
                if (gap < nearest) { nearest = gap; best = index; }
            }
            previous = next;
        }
        return best;
    }
    // Which way a line leaves a shape at a given point: along whichever axis
    // that point's face runs squarely to. A rectangle's bottom edge sends the
    // line straight down, its right edge straight out to the side; a diamond's
    // corner sends it out along its own axis. This is what keeps a right-angle
    // line from setting off along the edge it is standing on.
    [[nodiscard]] static bool leaves_vertically(const NodeItem* node, const QPointF& at) {
        const auto box = node->body_rect().translated(node->scenePos());
        const auto across = std::abs(at.x() - box.center().x()) / std::max(box.width() / 2, 0.001);
        const auto down = std::abs(at.y() - box.center().y()) / std::max(box.height() / 2, 0.001);
        return down > across;
    }
    // The face of a shape that looks at a point, met at the middle of it. An
    // end that has not been pinned leaves from here, which is what makes two
    // shapes standing in line join by one straight run.
    [[nodiscard]] static QPointF facing_point(const NodeItem* node, const QPointF& toward) {
        const auto box = node->body_rect().translated(node->scenePos());
        const auto delta = toward - box.center();
        const auto horizontal = std::abs(delta.x()) / std::max(box.width() / 2, 0.001)
                             >= std::abs(delta.y()) / std::max(box.height() / 2, 0.001);
        const QPointF axis = horizontal ? QPointF(delta.x() >= 0 ? 1000.0 : -1000.0, 0.0)
                                        : QPointF(0.0, delta.y() >= 0 ? 1000.0 : -1000.0);
        return node->boundary_toward(box.center() + axis);
    }
    // A line that breaks at right angles: out of one shape along the axis its
    // face runs to, across, and in to the other the same way. Two ends leaving
    // on different axes need one corner; two on the same axis need two, with
    // the crossing midway between them.
    [[nodiscard]] std::vector<QPointF> elbow_route() const {
        auto* head = head_node();
        auto* tail = tail_node();
        const auto head_centre = head->scenePos() + head->body_rect().center();
        const auto tail_centre = tail->scenePos() + tail->body_rect().center();
        const auto from = descriptor.owner_anchor ? head->boundary_at(*descriptor.owner_anchor)
                                                  : facing_point(head, tail_centre);
        const auto to = descriptor.child_anchor ? tail->boundary_at(*descriptor.child_anchor)
                                                : facing_point(tail, head_centre);
        const bool from_vertical = leaves_vertically(head, from);
        const bool to_vertical = leaves_vertically(tail, to);
        std::vector<QPointF> corners;
        if (from_vertical != to_vertical) {
            corners.emplace_back(from_vertical ? from.x() : to.x(), from_vertical ? to.y() : from.y());
        } else if (from_vertical) {
            const auto middle = (from.y() + to.y()) / 2;
            corners.emplace_back(from.x(), middle);
            corners.emplace_back(to.x(), middle);
        } else {
            const auto middle = (from.x() + to.x()) / 2;
            corners.emplace_back(middle, from.y());
            corners.emplace_back(middle, to.y());
        }
        // Two shapes standing in line need no corner at all, and a corner that
        // sits on an end is one the line does not turn at.
        const auto pointless = [&](const QPointF& corner) {
            return std::hypot(corner.x() - from.x(), corner.y() - from.y()) < 0.5
                || std::hypot(corner.x() - to.x(), corner.y() - to.y()) < 0.5;
        };
        std::erase_if(corners, pointless);
        if (corners.size() == 2 && std::hypot(corners[0].x() - corners[1].x(), corners[0].y() - corners[1].y()) < 0.5)
            corners.pop_back();
        return corners;
    }
    // A relationship that meets the same entity twice is drawn the way it is
    // drawn by hand: the first side runs straight between the two shapes, and
    // every later one leaves the diamond by its far corner and comes back
    // around the entity at right angles, into another of its faces. The
    // corners are worked out from where the two shapes are, so the loop
    // follows them as they move; it is not stored until the user takes hold
    // of it.
    [[nodiscard]] std::vector<QPointF> loop_route() const {
        const auto body_of = [](const NodeItem* node) {
            return node->body_rect().translated(node->scenePos());
        };
        const auto diamond = body_of(head_node());
        const auto box = body_of(tail_node());
        const qreal gap = 26 + (descriptor.recursion - 1) * 22;
        // The loop keeps to whichever side of the entity carries fewer of its
        // own attributes, so it runs through the empty part of the diagram
        // rather than across what is already drawn there. A second loop takes
        // the other side, since the first has it.
        const auto emptier = [&](bool vertical) {
            int before = 0;
            int after = 0;
            for (const auto* child : tail_node()->attribute_children) {
                const auto centre = child->scenePos() + child->body_rect().center();
                const auto along = vertical ? centre.y() : centre.x();
                const auto middle = vertical ? box.center().y() : box.center().x();
                if (along < middle) ++before; else ++after;
            }
            const bool second = after <= before;
            return descriptor.recursion % 2 == 1 ? second : !second;
        };
        const auto delta = diamond.center() - box.center();
        std::vector<QPointF> corners;
        if (std::abs(delta.x()) >= std::abs(delta.y())) {
            // The diamond lies to one side, so the loop leaves past it and
            // returns under or over the entity, into its top or bottom face.
            const qreal out = delta.x() >= 0 ? diamond.right() + gap : diamond.left() - gap;
            const bool below = emptier(true);
            const qreal along = below ? std::max(box.bottom(), diamond.bottom()) + gap
                                      : std::min(box.top(), diamond.top()) - gap;
            corners.emplace_back(out, diamond.center().y());
            corners.emplace_back(out, along);
            corners.emplace_back(box.center().x(), along);
        } else {
            const qreal out = delta.y() >= 0 ? diamond.bottom() + gap : diamond.top() - gap;
            const bool beyond = emptier(false);
            const qreal along = beyond ? std::max(box.right(), diamond.right()) + gap
                                       : std::min(box.left(), diamond.left()) - gap;
            corners.emplace_back(diamond.center().x(), out);
            corners.emplace_back(along, out);
            corners.emplace_back(along, box.center().y());
        }
        return corners;
    }
    // A loop the canvas worked out becomes the user's the moment they take
    // hold of it, so shaping one starts from the shape they can see rather
    // than from the straight line underneath it.
    void adopt_route() {
        if (!descriptor.waypoints.empty() || computed_.empty()) return;
        descriptor.waypoints = computed_;
        refresh();
    }
    [[nodiscard]] const std::vector<QPointF>& computed_route() const { return computed_; }
    [[nodiscard]] QPointF midpoint() const { return midpoint_; }
    [[nodiscard]] QPointF perpendicular() const { return perpendicular_; }
    void refresh() {
        prepareGeometryChange();
        const auto* inheritance = std::get_if<InheritanceKey>(&descriptor.key);
        const auto first = source->scenePos() + source->body_rect().center();
        const auto last = target->scenePos() + target->body_rect().center();
        perpendicular_ = normal(last - first);
        midpoint_ = (first + last) / 2;
        auto bend = midpoint_ + perpendicular_ * descriptor.offset;
        if (std::hypot(last.x() - first.x(), last.y() - first.y()) < 1)
            bend = first + QPointF(80, -80);
        QPointF start;
        QPointF end;
        // The point the line arrives from, which is not always the bend: an
        // elbow turns the corner before it reaches its end, and a route comes
        // in from its last corner. The end symbols and the labels are laid out
        // along this, so they sit square to the line as it actually arrives
        // rather than at an angle to it.
        QPointF approach;
        // An attribute link leaves its owner at the point shared by every
        // attribute on that side, runs a short trunk, then branches to its own.
        const bool owned_attribute = std::holds_alternative<AttributeId>(descriptor.key)
            && std::holds_alternative<AttributeId>(descriptor.from);
        if (owned_attribute) {
            QPointF anchor;
            QPointF junction;
            target->attribute_trunk(source, descriptor.owner_anchor, anchor, junction);
            end = descriptor.child_anchor ? source->boundary_at(*descriptor.child_anchor)
                                          : source->boundary_toward(junction);
            owner_join_ = anchor;
            child_join_ = end;
            path_ = QPainterPath(anchor);
            path_.lineTo(junction);
            if (style != LineStyle::Curved) {
                path_.lineTo(end);
            } else {
                const auto reach = std::clamp(std::hypot(end.x() - junction.x(), end.y() - junction.y()) * 0.45, 18.0, 90.0);
                const auto out = junction - anchor;
                const auto length = std::hypot(out.x(), out.y());
                const auto lead = length > 0.01 ? out / length : QPointF(0, -1);
                path_.cubicTo(junction + lead * reach, end + normal(lead) * 0, end);
            }
            bend = path_.pointAtPercent(0.5);
            midpoint_ = bend;
            perpendicular_ = normal(end - anchor);
            start = anchor;
        } else if (inheritance) {
            // Anchored at the triangle and curved to wherever the entity is, so
            // moving either one bends the line instead of dragging the anchor.
            const bool to_supertype = !inheritance->subtype.has_value();
            start = source->isa_anchor(to_supertype);
            end = target->boundary_toward(start);
            const qreal reach = std::clamp(std::abs(end.y() - start.y()) * 0.55, 26.0, 110.0);
            const qreal away = to_supertype ? -1.0 : 1.0;
            path_ = QPainterPath(start);
            if (style != LineStyle::Curved) path_.lineTo(end);
            else path_.cubicTo(start + QPointF(0, away * reach), end - QPointF(0, away * reach), end);
            bend = path_.pointAtPercent(0.5);
            midpoint_ = bend;
            perpendicular_ = normal(end - start);
        } else {
            start = descriptor.owner_anchor ? source->boundary_at(*descriptor.owner_anchor)
                                            : source->boundary_toward(bend);
            end = descriptor.child_anchor ? target->boundary_at(*descriptor.child_anchor)
                                          : target->boundary_toward(bend);
            owner_join_ = start;
            child_join_ = end;
            path_ = QPainterPath(start);
            path_.lineTo(bend);
            path_.lineTo(end);
            approach = bend;
        }
        // A recursive side loops back on its own while nothing has been stored
        // for it. The moment anything has been -- a bend, a route, a pinned
        // join -- that is the user's shape and it is drawn instead.
        // What the canvas works out for a line nobody has shaped: a loop back
        // for a recursive side, and right angles for every other line while
        // that is the style. Either becomes the user's the moment they take
        // hold of it, which is why it is kept rather than only drawn.
        computed_.clear();
        if (!inheritance && !owned_attribute && descriptor.waypoints.empty() && descriptor.offset == 0) {
            if (descriptor.recursion > 0) computed_ = loop_route();
            else if (style == LineStyle::Elbow) computed_ = elbow_route();
        }
        // A route replaces whatever the automatic shape would have been. Each
        // end aims at the corner nearest it rather than at the far endpoint, so
        // the first and last segments leave and arrive along the route itself.
        if (const auto& corners = descriptor.waypoints.empty() ? computed_ : descriptor.waypoints;
            !corners.empty() && !inheritance) {
            // The route is listed from the path's start to its end, and an
            // attribute's line runs from its owner, so head and tail are not
            // always source and target. Getting this the wrong way round would
            // reverse the cardinality symbols the moment a line was routed.
            auto* head = head_node();
            auto* tail = tail_node();
            start = descriptor.owner_anchor ? head->boundary_at(*descriptor.owner_anchor)
                                            : head->boundary_toward(corners.front());
            end = descriptor.child_anchor ? tail->boundary_at(*descriptor.child_anchor)
                                          : tail->boundary_toward(corners.back());
            owner_join_ = start;
            child_join_ = end;
            path_ = QPainterPath(start);
            for (const auto& corner : corners) path_.lineTo(corner);
            path_.lineTo(end);
            approach = corners.back();
            bend = path_.pointAtPercent(0.5);
            midpoint_ = bend;
            perpendicular_ = normal(end - corners.back());
        }
        // A routed line is shaped by its corners, so it shows a grip on each
        // instead of the single bend grip a plain one carries.
        corner_rects_.clear();
        for (const auto& corner : descriptor.waypoints)
            corner_rects_.push_back(QRectF(corner - QPointF(5, 5), QSizeF(10, 10)));
        handle_rect_ = descriptor.waypoints.empty() && computed_.empty()
            ? QRectF(bend - QPointF(5, 5), QSizeF(10, 10)) : QRectF();
        // The padlock sits off to one side of the bend grip rather than on it,
        // so the two controls on a selected link never overlap.
        lock_rect_ = lockable() ? QRectF(bend + perpendicular_ * 16 - QPointF(6, 6), QSizeF(12, 12)) : QRectF();
        // The remark mark takes the other side of the bend, so it never sits
        // under the padlock on a selected line. Unlike the padlock it is drawn
        // whether the line is selected or not, since it is how a remark on a
        // line is found at all.
        comment_at_ = bend - perpendicular_ * 16 - QPointF(comment_badge_size / 2, comment_badge_size / 2);
        // A grip on each end, where the line meets its shape. Squares, so they
        // are not mistaken for the round grips that bend and route the line.
        owner_end_rect_ = shapeable() ? QRectF(owner_join_ - QPointF(4.5, 4.5), QSizeF(9, 9)) : QRectF();
        child_end_rect_ = shapeable() ? QRectF(child_join_ - QPointF(4.5, 4.5), QSizeF(9, 9)) : QRectF();
        // An attribute's link and an inheritance link carry no labels, so the
        // bend serves them; everything else arrives from its own last corner.
        if (approach.isNull()) approach = bend;
        const auto entity_direction = approach - end;
        const auto distance = std::hypot(entity_direction.x(), entity_direction.y());
        // Labels are filled, so one overlapping the line hides it and the
        // connector reads as detached. An axis-aligned box clears a horizontal
        // line at its half-height but a diagonal one only at its corner, so
        // offset each label by its own support distance along that normal.
        const auto clearance = [](const QPointF& unit, qreal half_width, qreal half_height) {
            return half_width * std::abs(unit.x()) + half_height * std::abs(unit.y()) + 4;
        };
        const auto entity_normal = normal(entity_direction);
        // The unit vector pointing from the entity back along the connector.
        // Every end symbol is laid out along it, so the notation turns with the
        // line and stays correct on whichever side the entity has been moved to.
        outward_ = distance > 0.01 ? entity_direction / distance : QPointF(-1, 0);
        end_ = end;
        QFont label_font;
        label_font.setPointSizeF(10);
        const auto label = end_label();
        const auto label_width = label.isEmpty()
            ? 22.0 : QFontMetricsF(label_font).horizontalAdvance(label) + 10;
        const auto label_center = end + outward_ * symbol_reach() + 20.0 * outward_
            + entity_normal * clearance(entity_normal, label_width / 2, 10);
        cardinality_rect_ = QRectF(label_center - QPointF(label_width / 2, 10), QSizeF(label_width, 20));
        // Size the role box to its text. A fixed-width box blanketed the area
        // around the entity end and hid the connector arriving there.
        const auto role_width = descriptor.role.isEmpty()
            ? 0.0 : std::min(140.0, QFontMetricsF(label_font).horizontalAdvance(descriptor.role) + 12);
        // The role names this side, so it belongs beside the shape it names,
        // opposite the number, rather than sitting in the middle of the line
        // and cutting it. It is pushed far enough along to clear the shape.
        const auto role_center = end + outward_ * (symbol_reach() + 20.0 + role_width / 2)
            - entity_normal * clearance(entity_normal, role_width / 2, 10);
        role_rect_ = QRectF(role_center - QPointF(role_width / 2, 10), QSizeF(role_width, 20));
        bounds_ = path_.boundingRect().adjusted(-14, -14, 14, 14);
        if (descriptor.relationship && !end_label().isEmpty()) bounds_ = bounds_.united(cardinality_rect_);
        if (!descriptor.role.isEmpty()) bounds_ = bounds_.united(role_rect_);
        if (shapeable()) bounds_ = bounds_.united(handle_rect_.adjusted(-2, -2, 2, 2));
        if (shapeable()) bounds_ = bounds_.united(owner_end_rect_.adjusted(-2, -2, 2, 2)).united(child_end_rect_.adjusted(-2, -2, 2, 2));
        for (const auto& corner : corner_rects_) bounds_ = bounds_.united(corner.adjusted(-2, -2, 2, 2));
        if (lockable()) bounds_ = bounds_.united(lock_rect_.adjusted(-2, -2, 2, 2));
        if (comments > 0)
            bounds_ = bounds_.united(QRectF(comment_at_, QSizeF(comment_badge_size, comment_badge_size)).adjusted(-2, -2, 2, 4));
        // What this link is, kept as its own sentence so that a remark can be
        // shown beneath it without the two being rebuilt into one another.
        if (inheritance) {
            plain_tooltip = inheritance->subtype ? QStringLiteral("Inheritance — subtype")
                                                 : QStringLiteral("Inheritance — supertype");
        } else if (descriptor.relationship) {
            plain_tooltip = QStringLiteral("Participant: %1 · %2%3").arg(descriptor.cardinality == Cardinality::One ? "One" : "Many",
                descriptor.participation == Participation::Total ? "total participation" : "partial participation",
                descriptor.role.isEmpty() ? QString{} : QStringLiteral(" · ") + descriptor.role);
        } else {
            plain_tooltip = QStringLiteral("Attribute ownership — select and delete to detach");
        }
        setToolTip(plain_tooltip);
        update();
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);
        const QColor ink = isSelected() ? selection_ : highlighted ? text_ : connector_;
        // A connector is the thing a reader traces with their eye, so it is
        // drawn heavily enough to follow across a crowded diagram rather than
        // as the hairline it used to be.
        const qreal weight = isSelected() ? 4.0 : highlighted ? 3.4 : 2.2;
        painter->setPen(QPen(ink, weight));
        painter->setBrush(Qt::NoBrush);
        // A total participation in Chen, and a total specialization in every
        // notation, are drawn as a double line.
        const bool doubled = (descriptor.relationship && notation == Notation::Chen && descriptor.participation == Participation::Total)
            || (std::holds_alternative<InheritanceKey>(descriptor.key) && descriptor.total);
        if (doubled) {
            painter->save();
            painter->translate(perpendicular_ * 3);
            painter->drawPath(path_);
            painter->translate(perpendicular_ * -6);
            painter->drawPath(path_);
            painter->restore();
        } else {
            painter->drawPath(path_);
        }
        auto font = painter->font();
        font.setPointSizeF(10);
        painter->setFont(font);
        auto draw_label = [&](const QRectF& rect, const QString& text) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(canvas_);
            painter->drawRoundedRect(rect, 3, 3);
            painter->setPen(isSelected() ? selection_ : text_);
            painter->drawText(rect, Qt::AlignCenter, QFontMetricsF(font).elidedText(text, Qt::ElideRight, rect.width() - 6));
        };
        // A side asked to be drawn bare shows neither its symbols nor the
        // number beside them. Its role is still drawn: a role names the side,
        // it does not constrain it.
        if (descriptor.show_constraints) {
            if (const auto label = end_label(); !label.isEmpty()) draw_label(cardinality_rect_, label);
        }
        if (!descriptor.role.isEmpty()) draw_label(role_rect_, descriptor.role);
        if (descriptor.show_constraints) paint_end_symbols(painter, ink, canvas_);
        if (isSelected() && shapeable()) {
            painter->setPen(QPen(selection_, 1.4));
            painter->setBrush(canvas_);
            if (!handle_rect_.isNull()) painter->drawEllipse(handle_rect_);
            for (const auto& corner : corner_rects_) painter->drawEllipse(corner);
            painter->drawRect(owner_end_rect_);
            painter->drawRect(child_end_rect_);
        }
        if (isSelected() && lockable()) paint_lock(painter);
        if (comments > 0) draw_comment_badge(painter, comment_at_, warning_, comments_to_show > 0);
    }
    // A padlock, filled when the joins are pinned and hollow when they are not,
    // so the control shows its own state rather than needing a legend.
    void paint_lock(QPainter* painter) const {
        const auto body = QRectF(lock_rect_.left(), lock_rect_.center().y() - 1,
                                 lock_rect_.width(), lock_rect_.height() / 2 + 1);
        const auto shackle = QRectF(lock_rect_.left() + lock_rect_.width() * 0.22, lock_rect_.top(),
                                    lock_rect_.width() * 0.56, lock_rect_.height() * 0.62);
        painter->setPen(QPen(selection_, 1.4));
        painter->setBrush(Qt::NoBrush);
        // An open padlock is drawn with its shackle lifted clear on one side,
        // which reads as unlocked at this size where a tilted one does not.
        painter->drawArc(locked() ? shackle : shackle.translated(2.5, -1.5), 0, 180 * 16);
        painter->setBrush(locked() ? selection_ : canvas_);
        painter->drawRoundedRect(body, 1.5, 1.5);
    }
private:
    QPainterPath path_;
    QRectF bounds_;
    QRectF cardinality_rect_;
    QRectF role_rect_;
    QRectF handle_rect_;
    std::vector<QRectF> corner_rects_;
    // The corners this side loops through while nothing has been stored for it.
    std::vector<QPointF> computed_;
    QRectF lock_rect_;
    QPointF comment_at_;
    QRectF owner_end_rect_;
    QRectF child_end_rect_;
    QPointF owner_join_;
    QPointF child_join_;
    QPointF midpoint_;
    QPointF perpendicular_;
    QPointF outward_{-1, 0};
    QPointF end_;
    QColor connector_, selection_, canvas_, text_, warning_;
};

} // namespace

struct DiagramView::Impl {
    DiagramView& view;
    application::Editor& editor;
    QGraphicsScene* scene;
    std::map<ElementRef, NodeItem*> nodes;
    std::map<EdgeKey, EdgeItem*> edges;
    std::map<ElementRef, std::set<EdgeItem*>> incident;
    Tool active_tool = Tool::Select;
    ThemeId theme_id = ThemeId::OfficeLight;
    Notation notation = Notation::Chen;
    LineStyle style = LineStyle::Elbow;
    JoinMode join_mode = JoinMode::WhereClicked;
    bool tool_locked = false;
    bool grid = true;
    // The paper as the projection has it, and the picture decoded from it once
    // rather than on every repaint.
    domain::Background background;
    QPixmap paper;
    bool align_to_grid = false;
    bool synchronizing = false;
    // Whether remarks are being shown. Not part of the document.
    bool comments_shown = true;
    // What the diagram is being narrowed to. Not part of the document either.
    DiagramSearch search;
    std::set<ElementRef> found;

    // Whether one element is the kind a search is asking for.
    [[nodiscard]] static bool of_kind(const ElementRef& ref, SearchKind kind) {
        switch (kind) {
            case SearchKind::Entities: return std::holds_alternative<EntityId>(ref);
            case SearchKind::Attributes: return std::holds_alternative<AttributeId>(ref);
            case SearchKind::Relationships: return std::holds_alternative<RelationshipId>(ref);
            case SearchKind::Hierarchies: return std::holds_alternative<SpecializationId>(ref);
            case SearchKind::Everything: break;
        }
        return true;
    }

    // Everything the search asks for, before any relatives are added.
    [[nodiscard]] std::set<ElementRef> matches() const {
        std::set<ElementRef> hits;
        if (!search.looking()) return hits;
        const auto& project = editor.project();
        for (const auto& [ref, node] : nodes) {
            (void)node;
            if (!of_kind(ref, search.kind)) continue;
            if (!search.text.isEmpty()
                && !QString::fromStdString(name(project, ref)).contains(search.text, Qt::CaseInsensitive))
                continue;
            hits.insert(ref);
        }
        return hits;
    }

    // One step out from what was found: what belongs to it, and what it is
    // joined to. A relationship or a hierarchy brought in this way brings its
    // own far side with it, since either of them says nothing on its own.
    [[nodiscard]] std::set<ElementRef> with_relatives(const std::set<ElementRef>& hits) const {
        const auto& project = editor.project();
        auto shown = hits;
        auto keep = [&](const ElementRef& ref) { if (exists(project, ref)) shown.insert(ref); };
        for (const auto& ref : hits) {
            // What belongs to it, following composites down to their parts.
            std::vector<ElementRef> owners{ref};
            for (std::size_t i = 0; i < owners.size(); ++i)
                for (const auto& [id, attribute] : project.attributes)
                    if (attribute.owner && *attribute.owner == owners[i] && shown.insert(ElementRef{id}).second)
                        owners.emplace_back(id);
            // An attribute's own owner, so a found attribute is not left
            // floating away from the thing it describes.
            if (const auto* attribute = std::get_if<AttributeId>(&ref)) {
                const auto found_attribute = project.attributes.find(*attribute);
                if (found_attribute != project.attributes.end() && found_attribute->second.owner)
                    keep(*found_attribute->second.owner);
            }
            for (const auto& [id, relationship] : project.relationships) {
                const auto joins = std::any_of(relationship.participants.begin(), relationship.participants.end(),
                                               [&](const Participant& side) { return target_ref(side.target) == ref; });
                if (!joins) continue;
                keep(ElementRef{id});
                for (const auto& side : relationship.participants) keep(target_ref(side.target));
            }
            for (const auto& [id, hierarchy] : project.specializations) {
                const bool belongs = (hierarchy.supertype && ElementRef{*hierarchy.supertype} == ref)
                    || std::any_of(hierarchy.subtypes.begin(), hierarchy.subtypes.end(),
                                   [&](const EntityId& subtype) { return ElementRef{subtype} == ref; });
                if (!belongs) continue;
                keep(ElementRef{id});
                if (hierarchy.supertype) keep(ElementRef{*hierarchy.supertype});
                for (const auto& subtype : hierarchy.subtypes) keep(ElementRef{subtype});
            }
        }
        return shown;
    }

    // Puts the diagram in step with the search: what was found stands out, what
    // was not recedes or goes, and a line goes with whichever of its ends goes.
    void refresh_search() {
        found = matches();
        const auto shown = search.looking() && search.with_relatives ? with_relatives(found) : found;
        constexpr qreal receded = 0.16;
        for (auto& [ref, node] : nodes) {
            const bool idle = !search.looking();
            const bool keep = idle || shown.contains(ref);
            node->found = !idle && found.contains(ref);
            node->setVisible(keep || !search.hide_the_rest);
            node->setOpacity(keep ? 1.0 : receded);
            node->update();
        }
        for (auto& [key, edge] : edges) {
            // A line is only as visible as the shapes it joins: it recedes or
            // goes with whichever end recedes or goes, so no line is ever left
            // hanging from something that is not there.
            const bool keep = !search.looking()
                || (edge->source->isVisible() && edge->target->isVisible()
                    && edge->source->opacity() > receded && edge->target->opacity() > receded);
            edge->setVisible(edge->source->isVisible() && edge->target->isVisible());
            edge->setOpacity(keep ? 1.0 : receded);
        }
    }

    // Puts the marks and what the pointer says in step with the remarks the
    // project holds and with the switch. It runs after an edit and when the
    // switch is thrown, so those two can never disagree about what is marked.
    void refresh_comments() {
        const auto& project = editor.project();
        const auto awake_count = [&](const std::vector<CommentId>& ids) {
            return static_cast<int>(std::count_if(ids.begin(), ids.end(), [&](const CommentId& id) {
                const auto found = project.comments.find(id);
                return found != project.comments.end() && !found->second.hidden;
            }));
        };
        for (auto& [ref, node] : nodes) {
            // A remark pinned into an element's own writing counts as a remark
            // on it, so the mark is on the shape a reader is looking at rather
            // than buried in a panel.
            const auto pinned = comments_on(project, ref);
            const auto count = static_cast<int>(pinned.size());
            const auto shown = comments_shown ? awake_count(pinned) : 0;
            if (node->comments != count || node->comments_to_show != shown) {
                node->comments = count;
                node->comments_to_show = shown;
                node->update();
            }
            node->setToolTip(comment_tooltip(project, pinned, node->label, comments_shown));
        }
        // An inheritance link carries none: it is anchored to its triangle
        // rather than being a connector, so a remark about one goes on the
        // triangle instead.
        for (auto& [key, edge] : edges) {
            std::vector<CommentId> pinned;
            if (const auto* attribute = std::get_if<AttributeId>(&key))
                pinned = comments_on_connector(project, ConnectorRef{*attribute});
            else if (const auto* participant = std::get_if<ParticipantId>(&key))
                pinned = comments_on_connector(project, ConnectorRef{*participant});
            const auto count = static_cast<int>(pinned.size());
            const auto shown = comments_shown ? awake_count(pinned) : 0;
            if (edge->comments != count || edge->comments_to_show != shown) {
                edge->comments = count;
                edge->comments_to_show = shown;
                // The mark takes room beside the line, so the shape is measured
                // again rather than only repainted.
                edge->refresh();
            }
            edge->setToolTip(comment_tooltip(project, pinned, edge->plain_tooltip, comments_shown));
        }
    }
    bool panning = false;
    QPoint pan_start;
    // An editor placed over the node being renamed. It is a viewport child
    // rather than a scene item so it keeps ordinary text-field behaviour, and
    // it is repositioned whenever the view scrolls or zooms.
    QLineEdit* inline_editor = nullptr;
    // Where the pointer last was over the canvas, in scene coordinates.
    std::optional<QPointF> pointer_place;
    std::optional<ElementRef> renaming;
    std::optional<ElementRef> connect_start;
    // Where on its shape the source was clicked, as the direction the new
    // line's join will be pinned to; nothing when joins are automatic.
    std::optional<double> connect_start_direction;
    // While a connection is being made the pointer carries a preview line, so
    // the same gesture works as click-then-click or as one press-drag-release.
    std::optional<QPointF> connect_pointer;
    std::optional<ElementRef> connect_hover;
    std::map<ElementRef, Rect> drag_start;
    QPointF drag_anchor;
    // The element the drag was begun on. Everything selected moves by one
    // translation, and it is this one's edges that are lined up with the rest
    // of the diagram, so the group keeps its own spacing.
    std::optional<ElementRef> drag_anchor_ref;
    // The lines drawn while a drag is meeting something: an edge or a middle
    // that has come into line with another element's.
    std::vector<QLineF> guides;
    // The connector being reshaped, previewed on its item until release.
    std::optional<EdgeKey> bending;
    // A corner being dragged. It is only created once the pointer has actually
    // travelled, so a plain click on a line still just selects it.
    struct RouteDrag {
        EdgeKey key;
        std::size_t index = 0;
        QPointF press;
        bool grabbed = false;
    };
    std::optional<RouteDrag> routing;
    // An end of a selected line being carried to another point on its shape,
    // previewed on the item and pinned there on release.
    struct EndDrag {
        EdgeKey key;
        bool owner_end = false;
        // Whether the pointer has left the shape, and so whether a corner is
        // being carried at it. An end let go in open canvas stops there.
        bool placed = false;
    };
    std::optional<EndDrag> rejoining;
    // A symbol's corner being hauled to make its character bigger or smaller.
    // The box it started at is kept so that the size follows the pointer from
    // where the grip was grabbed, and so that Escape can put it back.
    struct SizeDrag {
        ElementRef ref;
        Rect start;
        int corner = 0;
    };
    std::optional<SizeDrag> sizing;
    // The box a hauled grip asks for: the corner opposite the one being
    // dragged stays where it is, and the box keeps the proportions it had.
    // A symbol's character is grown to the smaller of the box's two sides, so
    // a box let out of proportion would only pad the character with air; the
    // panel's own width and height fields are there for anyone who wants that.
    [[nodiscard]] static Rect sized_box(const SizeDrag& drag, const QPointF& pointer) {
        const QRectF start(drag.start.x, drag.start.y, drag.start.width, drag.start.height);
        const std::array<QPointF, 4> corners{start.topLeft(), start.topRight(),
                                             start.bottomRight(), start.bottomLeft()};
        const auto anchor = corners[static_cast<std::size_t>((drag.corner + 2) % 4)];
        // Which way the dragged corner lies from the anchor. It is fixed for
        // the whole drag, so hauling a corner past the anchor shrinks the
        // symbol to its smallest rather than turning it inside out.
        const auto horizontal = drag.corner == 1 || drag.corner == 2 ? 1.0 : -1.0;
        const auto vertical = drag.corner == 2 || drag.corner == 3 ? 1.0 : -1.0;
        const auto aspect = start.height() > 0.1 ? start.width() / start.height() : 1.0;
        const auto across = std::abs(pointer.x() - anchor.x());
        const auto down = std::abs(pointer.y() - anchor.y());
        // The larger of the two sizes the pointer implies, so the box follows
        // the hand rather than lagging behind whichever way it moved less.
        auto width = std::max(across, down * aspect);
        width = std::clamp(width, min_symbol_size, max_symbol_size);
        auto height = std::clamp(width / aspect, min_symbol_size, max_symbol_size);
        width = std::clamp(height * aspect, min_symbol_size, max_symbol_size);
        return Rect{horizontal > 0 ? anchor.x() : anchor.x() - width,
                    vertical > 0 ? anchor.y() : anchor.y() - height, width, height};
    }
    QPointF minimum_drag;
    QPointF maximum_drag;
    std::optional<std::uint64_t> displayed_revision;

    Impl(DiagramView& owner, application::Editor& controller)
        : view(owner), editor(controller), scene(new QGraphicsScene(&owner)) {}

    void status(const QString& text) const { if (view.on_status) view.on_status(text); }
    // The ruling or the picture the diagram is drawn on, laid over the colour
    // the theme gives the canvas and as strong as the background says. A
    // ruling is drawn in the theme's own grid colour, so it belongs to
    // whatever palette is on rather than to one of them.
    void paint_paper(QPainter* painter, const QRectF& rect, const Theme& colors) const {
        if (background.style == domain::BackgroundStyle::Theme || background.strength == 0) return;
        painter->save();
        painter->setOpacity(background.strength / 100.0);
        if (background.style == domain::BackgroundStyle::Image) {
            // One picture behind everything, covering the view and cropped to
            // it rather than repeated across it. It is drawn in the view's own
            // coordinates rather than the diagram's, so it is never magnified
            // by zooming in: at any zoom it is the picture at its own
            // resolution, which is what keeps it sharp. The canvas has no
            // edges to fit a picture to, so the view is what it fills.
            if (!paper.isNull()) {
                painter->resetTransform();
                painter->setRenderHint(QPainter::SmoothPixmapTransform);
                const auto room = view.viewport()->rect();
                const auto covered = paper.size().scaled(room.size(), Qt::KeepAspectRatioByExpanding);
                painter->drawPixmap(QRect(QPoint((room.width() - covered.width()) / 2,
                                                 (room.height() - covered.height()) / 2), covered),
                                    paper);
            }
            painter->restore();
            return;
        }
        QPen pen(colors.grid);
        pen.setCosmetic(true);
        painter->setPen(pen);
        if (background.style == domain::BackgroundStyle::Dots) {
            // Rounder, darker and wider apart than the editing grid's own
            // marks, which are meant to be barely there: this is paper, and
            // has to read as paper rather than as the faint aid it replaces.
            // The grid colour is carried towards the ink for the same reason a
            // ruling is drawn at full strength -- it was chosen to disappear.
            constexpr qreal step = grid_spacing * 1.5;
            QPen marks(over(colors.grid, QColor(colors.text.red(), colors.text.green(), colors.text.blue(), 110)), 4.0);
            marks.setCapStyle(Qt::RoundCap);
            marks.setCosmetic(true);
            painter->setPen(marks);
            for (qreal x = std::floor(rect.left() / step) * step; x <= rect.right(); x += step)
                for (qreal y = std::floor(rect.top() / step) * step; y <= rect.bottom(); y += step)
                    painter->drawPoint(QPointF(x, y));
            painter->restore();
            return;
        }
        if (background.style == domain::BackgroundStyle::Lines) {
            // Ruled like a notebook: across only, at a line's height apart.
            constexpr qreal ruling = 26;
            for (qreal y = std::floor(rect.top() / ruling) * ruling; y <= rect.bottom(); y += ruling)
                painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
            painter->restore();
            return;
        }
        // Squares, as on graph paper: a fine mesh with every fifth line heavier,
        // which is what makes a square countable at a glance.
        const auto mesh = [&](qreal step, qreal weight) {
            QPen ruled(colors.grid, weight);
            ruled.setCosmetic(true);
            painter->setPen(ruled);
            for (qreal x = std::floor(rect.left() / step) * step; x <= rect.right(); x += step)
                painter->drawLine(QPointF(x, rect.top()), QPointF(x, rect.bottom()));
            for (qreal y = std::floor(rect.top() / step) * step; y <= rect.bottom(); y += step)
                painter->drawLine(QPointF(rect.left(), y), QPointF(rect.right(), y));
        };
        if (view.zoom_factor() >= 0.4) mesh(grid_spacing, 1.0);
        mesh(grid_spacing * 5, 1.6);
        painter->restore();
    }
    // A note put down centred on a point, selected and opened for its title,
    // since a note is for writing on. Used by the tool and by the menus.
    void place_note(QPointF centre) {
        if (snap_enabled()) centre = {std::round(centre.x() / grid_spacing) * grid_spacing,
                                      std::round(centre.y() / grid_spacing) * grid_spacing};
        const auto result = editor.create_note("Note", centred(centre, note_body));
        publish(result);
        if (result && result.created) {
            view.select_elements({*result.created});
            begin_inline_edit(*result.created);
        }
    }
    [[nodiscard]] bool snap_enabled() const { return align_to_grid; }
    // The Insert entries a right-click menu offers, placing at the point clicked.
    void add_insert_menu(QMenu& menu, const QPointF& at) {
        auto* insert = menu.addMenu("Insert");
        insert->setObjectName("contextInsert");
        auto* picture = insert->addAction("Picture…");
        picture->setObjectName("contextInsertPicture");
        QObject::connect(picture, &QAction::triggered, &view, [this, at] { if (view.on_insert_picture) view.on_insert_picture(at); });
        auto* note = insert->addAction("Note");
        note->setObjectName("contextInsertNote");
        QObject::connect(note, &QAction::triggered, &view, [this, at] { place_note(at); });
    }
    void publish(const application::EditResult& result) {
        view.synchronize();
        if (view.on_edit) view.on_edit(result);
        if (!result) status(QString::fromStdString(result.error));
    }
    void selection_changed() {
        // Selection changes also arrive while the scene is being rebuilt or torn
        // down, when the item containers still name items that are going away.
        // The projection refreshes the highlight itself once it has finished.
        if (synchronizing) return;
        refresh_highlight();
        if (view.on_selection) view.on_selection(view.selected_elements());
    }
    void refresh_highlight() {
        std::set<EdgeItem*> touching;
        for (const auto& ref : view.selected_elements()) {
            const auto found = incident.find(ref);
            if (found != incident.end()) touching.insert(found->second.begin(), found->second.end());
        }
        for (auto& [key, edge] : edges) {
            (void)key;
            edge->set_highlighted(touching.contains(edge));
        }
    }
    void refresh_incident(NodeItem* item) {
        if (synchronizing) return;
        const auto found = incident.find(item->ref);
        if (found != incident.end()) for (auto* edge : found->second) edge->refresh();
    }
    // Elements line up with their neighbours as they are dragged, the way a
    // page layout does: an edge or a middle that comes within a few pixels of
    // another element's snaps to it, and a guide is drawn along what they now
    // share. Nothing is snapped to while nothing is near, so a deliberate
    // placement is never pulled off its mark.
    [[nodiscard]] std::optional<QPointF> aligned_delta(const QPointF& delta) {
        const auto previous = guides;
        guides.clear();
        const auto redraw = [&] { if (guides != previous) view.viewport()->update(); };
        if (!drag_anchor_ref || !drag_start.contains(*drag_anchor_ref)) { redraw(); return std::nullopt; }
        const auto& origin = drag_start.at(*drag_anchor_ref);
        const QRectF moving(origin.x + delta.x(), origin.y + delta.y(), origin.width, origin.height);
        // A few pixels on screen, whatever the diagram is zoomed to.
        const auto reach = 7.0 / std::max(view.zoom_factor(), 0.05);
        struct Match {
            bool found = false;
            qreal shift = 0;
            qreal line = 0;
            qreal from = 0;
            qreal to = 0;
        };
        Match across;
        Match down;
        const auto consider = [&](Match& best, qreal mine, qreal theirs, qreal from, qreal to) {
            const auto gap = theirs - mine;
            if (std::abs(gap) > reach || (best.found && std::abs(gap) >= std::abs(best.shift))) return;
            best = Match{true, gap, theirs, from, to};
        };
        for (const auto& [ref, item] : nodes) {
            if (drag_start.contains(ref)) continue;
            const QRectF other = item->body_rect().translated(item->scenePos());
            const auto top = std::min(moving.top(), other.top());
            const auto bottom = std::max(moving.bottom(), other.bottom());
            const auto left = std::min(moving.left(), other.left());
            const auto right = std::max(moving.right(), other.right());
            for (const auto mine : {moving.left(), moving.center().x(), moving.right()})
                for (const auto theirs : {other.left(), other.center().x(), other.right()})
                    consider(across, mine, theirs, top, bottom);
            for (const auto mine : {moving.top(), moving.center().y(), moving.bottom()})
                for (const auto theirs : {other.top(), other.center().y(), other.bottom()})
                    consider(down, mine, theirs, left, right);
        }
        if (!across.found && !down.found) { redraw(); return std::nullopt; }
        auto snapped = delta;
        if (across.found) {
            snapped.setX(snapped.x() + across.shift);
            guides.emplace_back(QPointF(across.line, across.from - 26), QPointF(across.line, across.to + 26));
        }
        if (down.found) {
            snapped.setY(snapped.y() + down.shift);
            guides.emplace_back(QPointF(down.from - 26, down.line), QPointF(down.to + 26, down.line));
        }
        redraw();
        return snapped;
    }
    void clear_guides() {
        if (guides.empty()) return;
        guides.clear();
        view.viewport()->update();
    }
    // Lining a selection up on one edge or one middle, as one edit. The line
    // they meet on is the far edge of what is selected, or its middle, so
    // nothing leaves the ground the selection already covers.
    void align_selection(const std::vector<ElementRef>& chosen, bool along_x, int which) {
        const auto& project = editor.project();
        QRectF bounds;
        for (const auto& ref : chosen) {
            const auto found = project.layout.find(ref);
            if (found != project.layout.end())
                bounds = bounds.united(QRectF(found->second.x, found->second.y,
                                              found->second.width, found->second.height));
        }
        if (bounds.isNull()) return;
        std::map<ElementRef, Rect> moved;
        for (const auto& ref : chosen) {
            const auto found = project.layout.find(ref);
            if (found == project.layout.end()) continue;
            auto rect = found->second;
            if (along_x)
                rect.x = which == 0 ? bounds.left()
                       : which == 1 ? bounds.center().x() - rect.width / 2 : bounds.right() - rect.width;
            else
                rect.y = which == 0 ? bounds.top()
                       : which == 1 ? bounds.center().y() - rect.height / 2 : bounds.bottom() - rect.height;
            if (rect != found->second) moved.emplace(ref, rect);
        }
        if (!moved.empty()) publish(editor.move(moved));
    }
    NodeItem* node_at(const QPoint& viewport_position) const {
        for (auto* item : view.items(viewport_position)) {
            if (auto* node = dynamic_cast<NodeItem*>(item)) return node;
        }
        return nullptr;
    }
    void place_inline_editor() {
        if (!renaming || !inline_editor) return;
        const auto found = nodes.find(*renaming);
        if (found == nodes.end()) return;
        const auto box = found->second->sceneBoundingRect();
        // Match the inset the node uses for its own label so the text does not
        // jump when the editor opens, and keep a diamond's text off its points.
        const auto inset = std::holds_alternative<RelationshipId>(*renaming) ? box.width() * 0.22 : 12.0;
        const QRect area(view.mapFromScene(box.topLeft() + QPointF(inset, 0)),
                         view.mapFromScene(box.bottomRight() - QPointF(inset, 0)));
        auto font = inline_editor->font();
        font.setPointSizeF(std::clamp(11.0 * view.zoom_factor(), 7.0, 28.0));
        font.setWeight(std::holds_alternative<EntityId>(*renaming) ? QFont::DemiBold : QFont::Normal);
        inline_editor->setFont(font);
        const auto height = std::min(area.height(), inline_editor->sizeHint().height());
        inline_editor->setGeometry(area.x(), area.center().y() - height / 2, std::max(area.width(), 24), height);
    }
    void begin_inline_edit(const ElementRef& element) {
        if (!exists(editor.project(), element)) return;
        commit_inline_edit();
        if (!inline_editor) {
            inline_editor = new QLineEdit(view.viewport());
            inline_editor->setObjectName("inlineName");
            inline_editor->setAlignment(Qt::AlignCenter);
            inline_editor->setFrame(false);
            inline_editor->installEventFilter(&view);
            // editingFinished covers both Return and losing focus; Escape is
            // handled by the filter and clears the target before it fires.
            QObject::connect(inline_editor, &QLineEdit::editingFinished, &view,
                             [this] { commit_inline_edit(); });
        }
        renaming = element;
        inline_editor->setText(QString::fromStdString(name(editor.project(), element)));
        place_inline_editor();
        inline_editor->show();
        inline_editor->selectAll();
        inline_editor->setFocus(Qt::MouseFocusReason);
        status(QStringLiteral("Type a name, then press Return. Escape keeps the previous name."));
    }
    void commit_inline_edit() {
        if (!renaming || !inline_editor) return;
        const auto element = *renaming;
        const auto value = inline_editor->text().toStdString();
        renaming.reset();
        // Hiding the box while it holds the keyboard makes Qt hand the keyboard
        // to whatever comes next in the tab order, which is a field in the
        // properties panel. The diagram is what the user is looking at, so the
        // keyboard goes back to it. When the box is closing because the user
        // clicked somewhere else, they have already said where they want it.
        // Asked of the window rather than through hasFocus, which is false
        // whenever the window is not the desktop's active one even though the
        // keyboard would come straight back to this box.
        const auto* keyboard = view.window() ? view.window()->focusWidget() : nullptr;
        const bool holding = keyboard == inline_editor;
        inline_editor->hide();
        if (holding) view.setFocus(Qt::OtherFocusReason);
        if (!exists(editor.project(), element) || value == name(editor.project(), element)) return;
        publish(editor.rename(element, value));
    }
    void cancel_inline_edit() {
        if (!renaming) return;
        renaming.reset();
        if (!inline_editor) return;
        const auto* keyboard = view.window() ? view.window()->focusWidget() : nullptr;
        const bool holding = keyboard == inline_editor;
        inline_editor->hide();
        if (holding) view.setFocus(Qt::OtherFocusReason);
    }
    EdgeItem* edge_at(const QPoint& viewport_position) const {
        for (auto* item : view.items(viewport_position)) {
            if (auto* edge = dynamic_cast<EdgeItem*>(item)) return edge;
        }
        return nullptr;
    }
    EdgeItem* selected_edge_at(const QPoint& viewport_position) const {
        for (auto* item : view.items(viewport_position)) {
            auto* edge = dynamic_cast<EdgeItem*>(item);
            if (edge && edge->isSelected() && edge->shapeable()) return edge;
        }
        return nullptr;
    }
    // Only a selected connector shows a handle, so only it can be grabbed.
    EdgeItem* lock_at(const QPoint& viewport_position) const {
        const auto scene_position = view.mapToScene(viewport_position);
        for (auto* item : view.items(viewport_position)) {
            auto* edge = dynamic_cast<EdgeItem*>(item);
            if (edge && edge->isSelected() && edge->lockable() && edge->lock_rect().contains(scene_position))
                return edge;
        }
        return nullptr;
    }
    EdgeItem* handle_at(const QPoint& viewport_position) const {
        const auto scene_position = view.mapToScene(viewport_position);
        for (auto* item : view.items(viewport_position)) {
            auto* edge = dynamic_cast<EdgeItem*>(item);
            if (edge && edge->isSelected() && edge->handle_rect().contains(scene_position)) return edge;
        }
        return nullptr;
    }
    // An end grip of a selected line under the pointer, and which end it is.
    EdgeItem* end_at(const QPoint& viewport_position, bool& owner_end) const {
        const auto scene_position = view.mapToScene(viewport_position);
        for (auto* item : view.items(viewport_position)) {
            auto* edge = dynamic_cast<EdgeItem*>(item);
            if (!edge || !edge->isSelected()) continue;
            if (const auto end = edge->end_at(scene_position)) {
                owner_end = *end;
                return edge;
            }
        }
        return nullptr;
    }
    // The direction a click on a shape pins the join to, or nothing when joins
    // are automatic. A click on the exact centre has no direction to give, so
    // it too leaves the join to route itself.
    [[nodiscard]] std::optional<double> join_direction(const NodeItem* node, const QPointF& scene_point) const {
        if (join_mode != JoinMode::WhereClicked) return std::nullopt;
        const auto centre = node->scenePos() + node->body_rect().center();
        if (std::hypot(scene_point.x() - centre.x(), scene_point.y() - centre.y()) < 0.5) return std::nullopt;
        return node->direction_of(scene_point);
    }
    void remove_edge(std::map<EdgeKey, EdgeItem*>::iterator& iterator) {
        auto* edge = iterator->second;
        incident[edge->descriptor.from].erase(edge);
        incident[edge->descriptor.to].erase(edge);
        // Drop it from the lookup before destroying it. Deleting a graphics item
        // can emit selectionChanged, which walks these containers; a pointer left
        // in one of them would be read after it was freed.
        iterator = edges.erase(iterator);
        scene->removeItem(edge);
        delete edge;
    }
    void zoom(qreal requested) {
        const auto bounded = std::clamp(requested, minimum_zoom, maximum_zoom);
        view.scale(bounded / view.zoom_factor(), bounded / view.zoom_factor());
        place_inline_editor();
        if (view.on_zoom) view.on_zoom(view.zoom_factor());
    }
    void connect_node(NodeItem* node, const QPointF& scene_point) {
        if (!node) {
            // A click on empty canvas spends the tool's one use, whether it
            // abandons a half-made connection or lands before one was started.
            // Every other tool hands back to Select on such a click, and this
            // one behaving differently was read as it being stuck.
            connect_start.reset();
            connect_start_direction.reset();
            connect_pointer.reset();
            if (!tool_locked) { view.set_tool(Tool::Select); return; }
            status(QStringLiteral("Connection cancelled. Select the first object."));
            return;
        }
        if (!exists(editor.project(), node->ref)
            || (connect_start && !exists(editor.project(), *connect_start))) {
            connect_start.reset();
            connect_start_direction.reset();
            connect_pointer.reset();
            view.synchronize();
            status(QStringLiteral("The selected object was removed. Select the first object again."));
            return;
        }
        if (!connect_start) {
            connect_start = node->ref;
            // The point clicked is where this end of the line will be pinned.
            connect_start_direction = join_direction(node, scene_point);
            view.select_elements({node->ref});
            status(QStringLiteral("Select an entity and relationship, or an attribute and its owner."));
            return;
        }
        const auto from = *connect_start;
        const auto to = node->ref;
        const auto from_join = connect_start_direction;
        const auto to_join = join_direction(node, scene_point);
        connect_start.reset();
        connect_start_direction.reset();
        connect_pointer.reset();
        application::EditResult result{false, "Connect an entity to a relationship, or an attribute to its owner.", {}, {}};
        if (from == to) {
            result.error = "Select two different objects. For a recursive relationship, connect the same entity to its relationship twice.";
        } else if (const auto plan = plan_connection(from, to)) {
            result = (*plan)(from_join, to_join);
        }
        // The attempt is the tool's one use whether or not the pair was legal,
        // so an unlocked Connect hands back to Select the way every other tool
        // does. Handing back first matters: the new tool announces itself, and
        // doing it afterwards would overwrite the reason a pair was refused.
        if (!tool_locked) view.set_tool(Tool::Select);
        publish(result);
        // A pair that needed a relationship of its own has one now, which the
        // user did not place: say so, and leave it selected and open for its
        // name, since an unnamed relationship says nothing.
        if (result && result.created) {
            view.select_elements({*result.created});
            // The editor announces itself as it opens, so the reason a diamond
            // appeared is said after it, or it would be the message wiped out.
            begin_inline_edit(*result.created);
            status(QStringLiteral("Two entities read through a relationship, so one was created between them. "
                                  "Type its name, then press Return."));
            return;
        }
        if (result && tool_locked)
            status(QStringLiteral("Connected. Locked: select the first object of the next connection."));
    }
    // The connectors a selection owns: every line touching one of the chosen
    // elements, and any line chosen outright. An inheritance link is left out,
    // since it is anchored to its triangle and has no joins to pin.
    [[nodiscard]] std::vector<EdgeItem*> connectors_of(const std::vector<ElementRef>& chosen) const {
        std::set<EdgeItem*> touching;
        for (const auto& ref : chosen) {
            const auto found = incident.find(ref);
            if (found != incident.end()) touching.insert(found->second.begin(), found->second.end());
        }
        for (auto* item : scene->selectedItems())
            if (auto* edge = dynamic_cast<EdgeItem*>(item)) touching.insert(edge);
        std::vector<EdgeItem*> lines;
        for (auto* edge : touching)
            if (edge->lockable()) lines.push_back(edge);
        return lines;
    }
    [[nodiscard]] std::vector<EdgeItem*> every_connector() const {
        std::vector<EdgeItem*> lines;
        for (const auto& [key, edge] : edges) {
            (void)key;
            if (edge->lockable()) lines.push_back(edge);
        }
        return lines;
    }
    // Locking pins each line where it is drawn now, so it stops sliding as the
    // shapes around it are moved; releasing hands the joins back. Whatever
    // else a line carries -- its bend, its corners -- is left as it is, and
    // the whole set is one edit however many lines it covers.
    void set_connectors_locked(const std::vector<EdgeItem*>& lines, bool locked) {
        std::map<ConnectorRef, domain::Connector> shapes;
        for (auto* edge : lines) {
            const auto& key = edge->descriptor.key;
            const auto ref = std::holds_alternative<AttributeId>(key)
                ? ConnectorRef{std::get<AttributeId>(key)} : ConnectorRef{std::get<ParticipantId>(key)};
            domain::Connector shape;
            shape.offset = edge->descriptor.offset;
            for (const auto& corner : edge->descriptor.waypoints)
                shape.waypoints.push_back(domain::Point{corner.x(), corner.y()});
            if (locked) {
                shape.owner_anchor = edge->owner_direction();
                shape.child_anchor = edge->child_direction();
            }
            shapes.emplace(ref, std::move(shape));
        }
        if (!shapes.empty())
            publish(editor.shape_connectors(shapes, locked ? "Lock connectors" : "Release connectors"));
    }
    // The entries that lock and release a set of lines. Each is offered only
    // while it has something to do, and each acts as it is triggered, so the
    // keyboard reaches it as surely as the pointer does.
    void add_lock_entries(QMenu& menu, const std::vector<EdgeItem*>& lines, const QString& what) {
        if (lines.empty()) return;
        menu.addSeparator();
        const auto entry = [&](const QString& text, const char* named, bool locking, bool enabled) {
            auto* action = menu.addAction(text);
            action->setObjectName(QString::fromLatin1(named));
            action->setEnabled(enabled);
            action->setToolTip(locking
                ? QStringLiteral("Pin each line where it meets its shapes, so it stops sliding as they are moved.")
                : QStringLiteral("Let each line find its own way to its shapes again."));
            QObject::connect(action, &QAction::triggered, &view,
                             [this, lines, locking] { set_connectors_locked(lines, locking); });
        };
        entry("Lock " + what, "contextLockConnectors", true,
              std::any_of(lines.begin(), lines.end(), [](EdgeItem* edge) { return !edge->locked(); }));
        entry("Release " + what, "contextUnlockConnectors", false,
              std::any_of(lines.begin(), lines.end(), [](EdgeItem* edge) { return edge->locked(); }));
    }

    // Where a new relationship between two entities belongs. It goes midway
    // between them, and steps aside, along the perpendicular to the line
    // joining them, once for each relationship already reading that same pair,
    // so a second reading does not land on top of the first. The sides
    // alternate as a diagram reads them: the second below the first, the third
    // above, and so on. It is only a starting place; the diamond is an element
    // like any other and can be dragged or given coordinates afterwards.
    [[nodiscard]] QPointF relationship_place(const ElementRef& first, const ElementRef& second) const {
        const auto& project = editor.project();
        const auto centre_of = [&](const ElementRef& ref) {
            const auto found = project.layout.find(ref);
            if (found == project.layout.end()) return QPointF(0, 0);
            return QPointF(found->second.x + found->second.width / 2,
                           found->second.y + found->second.height / 2);
        };
        const auto from = centre_of(first);
        const auto to = centre_of(second);
        const auto midway = (from + to) / 2;
        int existing = 0;
        for (const auto& [id, relationship] : project.relationships) {
            (void)id;
            bool touches_first = false;
            bool touches_second = false;
            for (const auto& participant : relationship.participants) {
                if (target_ref(participant.target) == first) touches_first = true;
                if (target_ref(participant.target) == second) touches_second = true;
            }
            if (touches_first && touches_second) ++existing;
        }
        if (existing == 0) return midway;
        const auto step = static_cast<qreal>((existing + 1) / 2);
        const auto side = existing % 2 == 1 ? 1.0 : -1.0;
        return midway + normal(to - from) * side * step * (relationship_body.height + 70);
    }

    // The edit a pair of elements would produce, or nothing when the pair means
    // nothing. The hover highlight and the committed connection read the same
    // rule here, so what the pointer promises is what the release performs.
    //
    // A plan is given the direction each of the two shapes was clicked in, and
    // pins the new line's ends there. Which click is the owner end and which
    // the child depends on the pair: a participant link is owned by its
    // relationship, an attribute link by the element the attribute belongs to.
    using Joins = std::optional<double>;
    using Plan = std::function<application::EditResult(Joins from_join, Joins to_join)>;
    [[nodiscard]] std::optional<Plan> plan_connection(const ElementRef& from, const ElementRef& to) const {
        const auto& project = editor.project();
        if (from == to || !exists(project, from) || !exists(project, to)) return {};
        // A picture or a note is not in the model, so no line can join one.
        if (is_figure(from) || is_figure(to)) return {};
        if (const auto* relationship = std::get_if<RelationshipId>(&from); relationship && std::holds_alternative<EntityId>(to)) {
            return Plan{[this, id = *relationship, entity = std::get<EntityId>(to)](Joins from_join, Joins to_join) {
                return editor.connect(id, entity, joined(from_join, to_join)); }};
        }
        if (const auto* relationship = std::get_if<RelationshipId>(&to); relationship && std::holds_alternative<EntityId>(from)) {
            return Plan{[this, id = *relationship, entity = std::get<EntityId>(from)](Joins from_join, Joins to_join) {
                return editor.connect(id, entity, joined(to_join, from_join)); }};
        }
        // An associative relationship acts as an entity, so it may join another
        // relationship. The plain one of the pair is the one that gains a participant.
        if (std::holds_alternative<RelationshipId>(from) && std::holds_alternative<RelationshipId>(to)) {
            const auto first = std::get<RelationshipId>(from);
            const auto second = std::get<RelationshipId>(to);
            if (project.relationships.at(first).associative && !project.relationships.at(second).associative)
                return Plan{[this, id = second, target = first](Joins from_join, Joins to_join) {
                    return editor.connect(id, ParticipantTarget{target}, joined(to_join, from_join)); }};
            if (project.relationships.at(second).associative && !project.relationships.at(first).associative)
                return Plan{[this, id = first, target = second](Joins from_join, Joins to_join) {
                    return editor.connect(id, ParticipantTarget{target}, joined(from_join, to_join)); }};
            return {};
        }
        // The first entity connected to a triangle is what it generalises; every
        // one after that is a subtype. Properties can change either afterwards.
        // An inheritance link is anchored to the triangle, so the clicks carry
        // nothing for it.
        const auto isa_link = [this, &project](SpecializationId id, EntityId entity) -> std::optional<Plan> {
            const auto found = project.specializations.find(id);
            if (found == project.specializations.end()) return {};
            if (!found->second.supertype)
                return Plan{[this, id, entity](Joins, Joins) { return editor.set_supertype(id, entity); }};
            return Plan{[this, id, entity](Joins, Joins) { return editor.attach_subtype(id, entity); }};
        };
        if (const auto* isa = std::get_if<SpecializationId>(&from); isa && std::holds_alternative<EntityId>(to))
            return isa_link(*isa, std::get<EntityId>(to));
        if (const auto* isa = std::get_if<SpecializationId>(&to); isa && std::holds_alternative<EntityId>(from))
            return isa_link(*isa, std::get<EntityId>(from));
        if (std::holds_alternative<SpecializationId>(from) || std::holds_alternative<SpecializationId>(to)) return {};
        // Two entities have no line of their own in Chen notation: they are
        // read through a relationship. Rather than refuse the pair, the
        // relationship they obviously mean is made between them, midway, with
        // each line joined to the entity where it was clicked.
        if (std::holds_alternative<EntityId>(from) && std::holds_alternative<EntityId>(to)) {
            return Plan{[this, first = std::get<EntityId>(from), second = std::get<EntityId>(to)]
                        (Joins from_join, Joins to_join) {
                // Asked for as the edit is made rather than as the plan is
                // built, so a pair connected twice in a row steps the second
                // diamond past the first.
                const auto place = relationship_place(ElementRef{first}, ElementRef{second});
                return editor.relate(first, second, centred(place, relationship_body), "Relationship",
                                     joined(std::nullopt, from_join), joined(std::nullopt, to_join));
            }};
        }
        if (std::holds_alternative<AttributeId>(from) && std::holds_alternative<AttributeId>(to)
            && project.attributes.at(std::get<AttributeId>(from)).kind == AttributeKind::Composite
            && project.attributes.at(std::get<AttributeId>(to)).kind != AttributeKind::Composite) {
            return Plan{[this, id = std::get<AttributeId>(to), owner = from](Joins from_join, Joins to_join) {
                return editor.set_attribute_owner(id, owner, joined(from_join, to_join)); }};
        }
        if (const auto* attribute = std::get_if<AttributeId>(&from)) {
            return Plan{[this, id = *attribute, owner = to](Joins from_join, Joins to_join) {
                return editor.set_attribute_owner(id, owner, joined(to_join, from_join)); }};
        }
        if (const auto* attribute = std::get_if<AttributeId>(&to)) {
            return Plan{[this, id = *attribute, owner = from](Joins from_join, Joins to_join) {
                return editor.set_attribute_owner(id, owner, joined(from_join, to_join)); }};
        }
        return {};
    }
};

DiagramView::DiagramView(application::Editor& editor, QWidget* parent)
    : QGraphicsView(parent), impl_(std::make_unique<Impl>(*this, editor)) {
    setScene(impl_->scene);
    setObjectName(QStringLiteral("conceptualCanvas"));
    setAccessibleName(QStringLiteral("Conceptual ERD canvas"));
    setAccessibleDescription(QStringLiteral("Create, select, connect and move entities, attributes and relationships. Use Explorer for keyboard navigation."));
    setRenderHints(QPainter::Antialiasing | QPainter::TextAntialiasing);
    setViewportUpdateMode(BoundingRectViewportUpdate);
    setTransformationAnchor(AnchorUnderMouse);
    setResizeAnchor(AnchorViewCenter);
    setDragMode(RubberBandDrag);
    setRubberBandSelectionMode(Qt::IntersectsItemShape);
    setBackgroundBrush(theme(impl_->theme_id).canvas);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    impl_->scene->setSceneRect(-3000, -2200, 6000, 4400);
    connect(impl_->scene, &QGraphicsScene::selectionChanged, this, [this] { impl_->selection_changed(); });
    synchronize();
    centerOn(0, 0);
}

DiagramView::~DiagramView() {
    // Scene items have callbacks into Impl. Destroy them before Impl itself.
    impl_->synchronizing = true;
    impl_->scene->clear();
}

void DiagramView::synchronize() {
    if (impl_->displayed_revision == impl_->editor.revision()) return;
    // Undo, New, or Open can arrive while a pointer gesture is in progress.
    // Discard that preview before projecting the newer authoritative revision.
    if (!impl_->drag_start.empty()) cancel_interaction();
    impl_->synchronizing = true;
    const auto& project = impl_->editor.project();
    std::map<EdgeKey, EdgeDescription> desired_edges;
    // A stored shape is the user's choice and overrides automatic routing.
    const auto shaped = [&](const ConnectorRef& key, qreal automatic) {
        const auto found = project.connectors.find(key);
        if (found == project.connectors.end()) return domain::Connector{automatic, {}, {}, {}};
        return found->second;
    };
    const auto route_of = [](const domain::Connector& connector) {
        std::vector<QPointF> route;
        route.reserve(connector.waypoints.size());
        for (const auto& point : connector.waypoints) route.emplace_back(point.x, point.y);
        return route;
    };
    for (const auto& [id, attribute] : project.attributes) {
        if (attribute.owner && exists(project, *attribute.owner)) {
            const auto shape = shaped(id, 0);
            // Named rather than positional: the description has grown enough
            // fields that a list of them silently means the wrong thing the
            // moment one is inserted.
            desired_edges.emplace(id, EdgeDescription{.key = id, .from = id, .to = *attribute.owner,
                                                      .offset = shape.offset,
                                                      .owner_anchor = shape.owner_anchor,
                                                      .child_anchor = shape.child_anchor,
                                                      .waypoints = route_of(shape)});
        }
    }
    for (const auto& [id, relationship] : project.relationships) {
        // Which time each side meets the same element. The first runs straight
        // between the two shapes; a second or third is a recursion and loops
        // back around instead, which the item works out from where they are.
        std::map<ElementRef, std::size_t> index;
        for (const auto& participant : relationship.participants) {
            if (!exists(project, target_ref(participant.target))) continue;
            const auto ordinal = index[target_ref(participant.target)]++;
            const auto shape = shaped(participant.id, 0);
            desired_edges.emplace(participant.id, EdgeDescription{
                .key = participant.id, .from = id, .to = target_ref(participant.target), .relationship = id,
                .cardinality = participant.maximum, .participation = participant.participation,
                .show_constraints = participant.show_constraints,
                .role = QString::fromStdString(participant.role), .offset = shape.offset,
                .owner_anchor = shape.owner_anchor, .child_anchor = shape.child_anchor,
                .waypoints = route_of(shape), .recursion = static_cast<int>(ordinal)});
        }
    }
    for (const auto& [id, specialization] : project.specializations) {
        if (specialization.supertype && project.entities.contains(*specialization.supertype))
            desired_edges.emplace(EdgeKey{InheritanceKey{id, {}}},
                EdgeDescription{.key = EdgeKey{InheritanceKey{id, {}}}, .from = id,
                                .to = ElementRef{*specialization.supertype},
                                .total = specialization.completeness == Completeness::Total});
        for (const auto& subtype : specialization.subtypes) {
            if (!project.entities.contains(subtype)) continue;
            const EdgeKey key{InheritanceKey{id, subtype}};
            desired_edges.emplace(key, EdgeDescription{.key = key, .from = id, .to = ElementRef{subtype}});
        }
    }
    for (auto it = impl_->edges.begin(); it != impl_->edges.end();) {
        const auto wanted = desired_edges.find(it->first);
        if (wanted == desired_edges.end() || wanted->second.from != it->second->descriptor.from || wanted->second.to != it->second->descriptor.to)
            impl_->remove_edge(it);
        else ++it;
    }
    for (auto it = impl_->nodes.begin(); it != impl_->nodes.end();) {
        if (!exists(project, it->first)) {
            impl_->incident.erase(it->first);
            auto* node = it->second;
            it = impl_->nodes.erase(it);
            impl_->scene->removeItem(node);
            delete node;
        } else ++it;
    }
    for (auto& [ref, node] : impl_->nodes) { (void)ref; node->attribute_children.clear(); }
    std::set<EdgeItem*> dirty_edges;
    const auto sync_node = [&](const ElementRef& ref) {
        auto [iterator, created] = impl_->nodes.try_emplace(ref, nullptr);
        auto*& node = iterator->second;
        if (created) {
            node = new NodeItem(ref, theme(impl_->theme_id));
            node->moved = [this](NodeItem* changed) { impl_->refresh_incident(changed); };
            node->constrain = [this, node](QPointF position) {
                if (impl_->synchronizing) return position;
                const auto original = impl_->drag_start.find(node->ref);
                if (original != impl_->drag_start.end()) {
                    const QPointF start{original->second.x, original->second.y};
                    auto delta = position - start;
                    // Meeting a neighbour's edge or middle comes first, since
                    // that is what the eye is going for; the grid catches
                    // whatever is not meeting anything.
                    if (const auto guided = impl_->aligned_delta(delta)) {
                        delta = *guided;
                    } else if (impl_->align_to_grid) {
                        const auto anchor = impl_->drag_anchor + delta;
                        delta = QPointF(std::round(anchor.x() / grid_spacing) * grid_spacing,
                                        std::round(anchor.y() / grid_spacing) * grid_spacing) - impl_->drag_anchor;
                    }
                    // All selected nodes use one translation, preserving their
                    // relative spacing even when aligning or reaching a boundary.
                    delta.setX(std::clamp(delta.x(), impl_->minimum_drag.x(), impl_->maximum_drag.x()));
                    delta.setY(std::clamp(delta.y(), impl_->minimum_drag.y(), impl_->maximum_drag.y()));
                    return start + delta;
                }
                if (impl_->align_to_grid) position = {std::round(position.x() / grid_spacing) * grid_spacing, std::round(position.y() / grid_spacing) * grid_spacing};
                return QPointF(std::clamp(position.x(), -max_coordinate, max_coordinate - node->body_rect().width()),
                               std::clamp(position.y(), -max_coordinate, max_coordinate - node->body_rect().height()));
            };
            impl_->scene->addItem(node);
        }
        const auto layout = project.layout.find(ref);
        const auto rect = layout == project.layout.end() ? Rect{} : layout->second;
        const bool geometry_changed = node->pos() != QPointF(rect.x, rect.y) || node->body_rect().size() != QSizeF(rect.width, rect.height);
        node->set_size(rect.width, rect.height);
        node->setPos(rect.x, rect.y);
        const auto label = QString::fromStdString(name(project, ref));
        AttributeKind kind = AttributeKind::Normal;
        if (const auto* attribute_id = std::get_if<AttributeId>(&ref)) kind = project.attributes.at(*attribute_id).kind;
        bool associative = false;
        bool identifying = false;
        if (const auto* relationship_id = std::get_if<RelationshipId>(&ref)) {
            associative = project.relationships.at(*relationship_id).associative;
            identifying = project.relationships.at(*relationship_id).identifying;
        }
        bool weak = false;
        if (const auto* entity_id = std::get_if<EntityId>(&ref)) weak = project.entities.at(*entity_id).weak;
        // A key attribute of a weak entity is only a partial key.
        bool partial_key = false;
        if (const auto* attribute_id = std::get_if<AttributeId>(&ref)) {
            const auto& attribute = project.attributes.at(*attribute_id);
            if (const auto* owner = attribute.owner ? std::get_if<EntityId>(&*attribute.owner) : nullptr)
                partial_key = kind == AttributeKind::Key && project.entities.contains(*owner) && project.entities.at(*owner).weak;
        }
        bool generalising = false;
        bool disjoint = true;
        if (const auto* specialization_id = std::get_if<SpecializationId>(&ref)) {
            generalising = project.specializations.at(*specialization_id).direction == Inheritance::Generalization;
            disjoint = project.specializations.at(*specialization_id).constraint == Disjointness::Disjoint;
        }
        QString body;
        bool plain = false;
        if (const auto* note_id = std::get_if<NoteId>(&ref)) {
            body = QString::fromStdString(project.notes.at(*note_id).description);
            plain = project.notes.at(*note_id).plain;
        }
        // A picture's bytes never change once it is placed, so they are decoded
        // once, when its node is first built.
        if (const auto* picture_id = std::get_if<PictureId>(&ref); picture_id && !node->has_image())
            node->set_image(project.pictures.at(*picture_id).image);
        if (node->label != label || node->body != body || node->plain != plain || node->attribute_kind != kind
            || node->associative != associative
            || node->generalising != generalising || node->weak != weak || node->identifying != identifying
            || node->partial_key != partial_key || node->disjoint != disjoint) {
            // The margin around a symbol is wider than around anything else,
            // for the grips, so becoming one changes the bounding rect and the
            // scene has to be told before it does.
            if (node->plain != plain) node->about_to_change_geometry();
            node->label = label;
            node->body = body;
            node->plain = plain;
            node->attribute_kind = kind;
            node->generalising = generalising;
            node->weak = weak;
            node->identifying = identifying;
            node->partial_key = partial_key;
            node->disjoint = disjoint;
            node->set_associative(associative);
            node->update();
        }
        node->setToolTip(label);
        const auto chosen = project.colours.find(ref);
        node->set_chosen_colour(chosen == project.colours.end()
            ? std::optional<QColor>{}
            : std::optional{QColor(chosen->second.red, chosen->second.green, chosen->second.blue)});
        const auto faded = project.transparency.find(ref);
        node->set_transparency(faded == project.transparency.end() ? 0 : faded->second);
        if (geometry_changed) {
            const auto incident = impl_->incident.find(ref);
            if (incident != impl_->incident.end()) dirty_edges.insert(incident->second.begin(), incident->second.end());
        }
    };
    for (const auto& [id, entity] : project.entities) { (void)entity; sync_node(id); }
    for (const auto& [id, attribute] : project.attributes) { (void)attribute; sync_node(id); }
    for (const auto& [id, relationship] : project.relationships) { (void)relationship; sync_node(id); }
    for (const auto& [id, specialization] : project.specializations) { (void)specialization; sync_node(id); }
    for (const auto& [id, picture] : project.pictures) { (void)picture; sync_node(id); }
    for (const auto& [id, note] : project.notes) { (void)note; sync_node(id); }
    for (const auto& [key, description] : desired_edges) {
        auto found = impl_->edges.find(key);
        if (found == impl_->edges.end()) {
            auto* edge = new EdgeItem(description, impl_->nodes.at(description.from), impl_->nodes.at(description.to), theme(impl_->theme_id));
            edge->notation = impl_->notation;
            impl_->edges.emplace(key, edge);
            edge->style = impl_->style;
            impl_->incident[description.from].insert(edge);
            impl_->incident[description.to].insert(edge);
            impl_->scene->addItem(edge);
        } else if (!(found->second->descriptor == description)) {
            found->second->descriptor = description;
            dirty_edges.insert(found->second);
        }
    }
    for (auto* edge : dirty_edges) edge->refresh();
    impl_->refresh_comments();
    impl_->refresh_search();
    // The workspace grows only at command boundaries, never during pointer movement.
    const auto content = impl_->scene->itemsBoundingRect().adjusted(-800, -800, 800, 800);
    impl_->scene->setSceneRect(QRectF(-3000, -2200, 6000, 4400).united(content));
    for (const auto& [key, description] : desired_edges) {
        (void)key;
        if (!std::holds_alternative<AttributeId>(description.key)) continue;
        const auto owner = impl_->nodes.find(description.to);
        const auto child = impl_->nodes.find(description.from);
        if (owner != impl_->nodes.end() && child != impl_->nodes.end())
            owner->second->attribute_children.push_back(child->second);
    }
    for (auto& [key, edge] : impl_->edges) { (void)key; edge->refresh(); }
    if (impl_->background != project.background) {
        impl_->background = project.background;
        impl_->paper = QPixmap();
        if (impl_->background.style == domain::BackgroundStyle::Image)
            impl_->paper.loadFromData(impl_->background.image.data(),
                                      static_cast<uint>(impl_->background.image.size()));
        viewport()->update();
    }
    impl_->displayed_revision = impl_->editor.revision();
    impl_->synchronizing = false;
    if (impl_->connect_start && !exists(project, *impl_->connect_start)) {
        impl_->connect_start.reset();
        impl_->connect_start_direction.reset();
        impl_->connect_pointer.reset();
        impl_->connect_hover.reset();
    }
    // An element can disappear under an open editor through undo or a reload.
    if (impl_->renaming && !exists(project, *impl_->renaming)) impl_->cancel_inline_edit();
    impl_->place_inline_editor();
    // Also reapplies the highlight to whatever items this projection rebuilt.
    impl_->selection_changed();
}

void DiagramView::set_tool(Tool tool, bool locked) {
    cancel_interaction();
    impl_->active_tool = tool;
    // Select has nothing to repeat, so it is never a locked tool.
    impl_->tool_locked = locked && tool != Tool::Select;
    setDragMode(tool == Tool::Select ? RubberBandDrag : NoDrag);
    setCursor(tool == Tool::Pan ? Qt::OpenHandCursor : tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    if (on_tool) on_tool(tool);
    if (impl_->tool_locked) {
        impl_->status(QStringLiteral("Locked. Keep placing; choose another tool or press Escape to stop."));
        return;
    }
    switch (tool) {
    case Tool::Select: {
        // Qt renders this modifier as the platform's own key, so the hint names
        // Command on a Mac and Control elsewhere without guessing which.
        const auto extend = QKeySequence(Qt::ControlModifier).toString(QKeySequence::NativeText);
        impl_->status(QStringLiteral("Select objects to edit. Drag to move; %1click or Shift-click to add to the selection.")
                          .arg(extend));
        break;
    }
    case Tool::Entity: impl_->status(QStringLiteral("Click the canvas to create an entity.")); break;
    case Tool::Attribute: impl_->status(QStringLiteral("Click to add an attribute to the selected owner, or an unattached attribute.")); break;
    case Tool::Relationship: impl_->status(QStringLiteral("Click the canvas to create a relationship, then use Connect to add participants.")); break;
    case Tool::Specialization: impl_->status(QStringLiteral("Click the canvas to place an ISA triangle pointing down, then connect its supertype and subtypes.")); break;
    case Tool::Generalization: impl_->status(QStringLiteral("Click the canvas to place an ISA triangle pointing up, then connect its supertype and subtypes.")); break;
    case Tool::Connect:
        impl_->status(impl_->join_mode == JoinMode::WhereClicked
            ? QStringLiteral("Select an entity and relationship, an attribute and its owner, or a subtype and its triangle. The line joins each shape where you click.")
            : QStringLiteral("Select an entity and relationship, an attribute and its owner, or a subtype and its triangle."));
        break;
    case Tool::Pan: impl_->status(QStringLiteral("Drag to pan. The middle mouse button pans in every tool.")); break;
    case Tool::Note: impl_->status(QStringLiteral("Click the canvas to place a note, then type its title. Its text is written in Properties.")); break;
    }
}
bool DiagramView::tool_locked() const { return impl_->tool_locked; }
Tool DiagramView::tool() const { return impl_->active_tool; }

std::vector<ElementRef> DiagramView::selected_elements() const {
    std::vector<ElementRef> selected;
    for (auto* item : impl_->scene->selectedItems())
        if (auto* node = dynamic_cast<NodeItem*>(item)) selected.push_back(node->ref);
    std::sort(selected.begin(), selected.end());
    return selected;
}
void DiagramView::select_elements(const std::vector<ElementRef>& elements, bool bring_into_view) {
    auto requested = elements;
    std::erase_if(requested, [this](const auto& ref) { return !impl_->nodes.contains(ref); });
    std::sort(requested.begin(), requested.end());
    requested.erase(std::unique(requested.begin(), requested.end()), requested.end());
    QRectF selected_bounds;
    for (const auto& ref : requested) selected_bounds = selected_bounds.united(impl_->nodes.at(ref)->sceneBoundingRect());
    if (requested == selected_elements()
        && static_cast<std::size_t>(impl_->scene->selectedItems().size()) == requested.size()) {
        if (bring_into_view && !selected_bounds.isEmpty()) ensureVisible(selected_bounds, 60, 60);
        return;
    }
    impl_->synchronizing = true;
    impl_->scene->clearSelection();
    for (const auto& ref : requested) impl_->nodes.at(ref)->setSelected(true);
    impl_->synchronizing = false;
    if (bring_into_view && !selected_bounds.isEmpty()) ensureVisible(selected_bounds, 60, 60);
    impl_->selection_changed();
}
void DiagramView::fit_diagram() {
    if (impl_->nodes.empty()) { actual_size(); centerOn(0, 0); return; }
    fitInView(impl_->scene->itemsBoundingRect().adjusted(-70, -70, 70, 70), Qt::KeepAspectRatio);
    impl_->zoom(zoom_factor());
}
QRectF DiagramView::diagram_bounds() const { return impl_->scene->itemsBoundingRect(); }

QRectF DiagramView::selection_bounds() const {
    QRectF bounds;
    for (const auto* item : impl_->scene->selectedItems()) bounds = bounds.united(item->sceneBoundingRect());
    return bounds;
}

QRectF DiagramView::view_bounds() const { return mapToScene(viewport()->rect()).boundingRect(); }

QColor DiagramView::canvas_colour() const { return theme(impl_->theme_id).canvas; }

void DiagramView::render_diagram(QPainter& painter, const QRectF& target, const QRectF& source) {
    // A picture is of the diagram, not of the editor that happens to be looking
    // at it. Selection rings, the grips that come with them and a connector's
    // padlock are all drawn because something is chosen, so what is chosen is
    // put down for the length of the drawing and picked up again afterwards.
    // The guard is the one the canvas already uses while it rearranges its own
    // selection, so nothing downstream hears a selection that never changed.
    const auto chosen = impl_->scene->selectedItems();
    impl_->synchronizing = true;
    impl_->scene->clearSelection();
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::TextAntialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);
    // The scene draws its items and nothing else: the paper and the editing
    // grid belong to the view's own background, which is not consulted here.
    impl_->scene->render(&painter, target, source, Qt::IgnoreAspectRatio);
    for (auto* item : chosen) item->setSelected(true);
    impl_->synchronizing = false;
}

void DiagramView::actual_size() { impl_->zoom(1); }
void DiagramView::zoom_in() { impl_->zoom(zoom_factor() * 1.2); }
void DiagramView::zoom_out() { impl_->zoom(zoom_factor() / 1.2); }
void DiagramView::set_notation(Notation notation) {
    if (impl_->notation == notation) return;
    impl_->notation = notation;
    for (auto& [key, edge] : impl_->edges) {
        (void)key;
        edge->notation = notation;
        edge->refresh();
    }
    viewport()->update();
}
Notation DiagramView::notation() const { return impl_->notation; }

void DiagramView::set_line_style(LineStyle style) {
    if (impl_->style == style) return;
    impl_->style = style;
    for (auto& [key, edge] : impl_->edges) {
        (void)key;
        edge->style = style;
        edge->refresh();
    }
    viewport()->update();
}
LineStyle DiagramView::line_style() const { return impl_->style; }
void DiagramView::set_join_mode(JoinMode mode) { impl_->join_mode = mode; }
JoinMode DiagramView::join_mode() const { return impl_->join_mode; }

// Both pickers draw a sample of a line. Drawn in the connector's own muted
// grey at the canvas weight, those samples came out as hairlines that could not
// be told apart in a menu, so they use the theme's accent and a heavier stroke:
// a sample has to read as the thing it stands for, not match it pixel for pixel.
namespace {
constexpr qreal preview_weight = 2.8;
}

QPixmap DiagramView::line_style_preview(LineStyle style, QSize size, std::optional<QColor> ink) const {
    const auto& colors = theme(impl_->theme_id);
    QPixmap pixmap(size * devicePixelRatioF());
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(ink.value_or(colors.accent), preview_weight);
    pen.setCapStyle(Qt::RoundCap);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QPointF from(4, size.height() - 4.0);
    const QPointF to(size.width() - 4.0, 4);
    if (style == LineStyle::Straight) {
        painter.drawLine(from, to);
    } else if (style == LineStyle::Elbow) {
        QPainterPath path(from);
        path.lineTo((from.x() + to.x()) / 2, from.y());
        path.lineTo((from.x() + to.x()) / 2, to.y());
        path.lineTo(to);
        painter.drawPath(path);
    } else {
        QPainterPath path(from);
        path.cubicTo(from + QPointF(size.width() * 0.45, 0), to - QPointF(size.width() * 0.45, 0), to);
        painter.drawPath(path);
    }
    return pixmap;
}

QPixmap DiagramView::element_preview(const ElementRef& ref, QSize size, bool fill_the_room) const {
    QPixmap pixmap(size * devicePixelRatioF());
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    pixmap.fill(Qt::transparent);
    const auto found = impl_->nodes.find(ref);
    if (found == impl_->nodes.end()) return pixmap;
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    found->second->paint_shape(&painter, QRectF(1, 1, size.width() - 2.0, size.height() - 2.0), fill_the_room);
    return pixmap;
}

QPixmap DiagramView::notation_preview(Notation notation, QSize size, std::optional<QColor> ink) const {
    const auto& colors = theme(impl_->theme_id);
    const auto drawn = ink.value_or(colors.accent);
    QPixmap pixmap(size * devicePixelRatioF());
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal middle = size.height() / 2.0;
    auto font = painter.font();
    font.setPointSizeF(9);
    painter.setFont(font);
    // Chen and min-max say the pair in writing, and that writing is the sample:
    // it is the end of the line rather than a caption over it. Room is taken
    // for it at the right and the line stops short, so the line never runs
    // through the very characters the reader is being shown.
    const auto reading = notation == Notation::Chen ? QStringLiteral("M")
                       : notation == Notation::MinMax ? QStringLiteral("(1,M)") : QString();
    const qreal written = reading.isEmpty()
        ? 0.0 : QFontMetricsF(font).horizontalAdvance(reading) + 5;
    // A mandatory "many" end exercises both symbols in every notation.
    const QPointF end(size.width() - 6.0 - written, middle);
    QPen line(drawn, preview_weight);
    line.setCapStyle(Qt::RoundCap);
    painter.setPen(line);
    painter.drawLine(QPointF(4, middle), end);
    // The fill behind a solid end is the canvas the diagram is drawn on, unless
    // the sample has been given an ink of its own -- on a highlighted row the
    // canvas colour is not what lies behind it.
    draw_participant_end(&painter, notation, true, true, end, QPointF(-1, 0), drawn,
                         ink ? Qt::transparent : QColor(colors.canvas));
    if (!reading.isEmpty()) {
        // Written in the same ink as the line it belongs to, since the two say
        // one thing together.
        painter.setPen(ink.value_or(colors.node_text));
        painter.drawText(QRectF(size.width() - written - 2, 0, written, size.height()),
                         Qt::AlignRight | Qt::AlignVCenter, reading);
    }
    return pixmap;
}
void DiagramView::set_search(const DiagramSearch& search) {
    if (impl_->search == search) return;
    impl_->search = search;
    // Nothing about the document changed, so this goes straight to the
    // projection rather than through synchronize, which would see the same
    // revision and do nothing.
    impl_->refresh_search();
}
const DiagramSearch& DiagramView::search() const { return impl_->search; }

std::vector<ElementRef> DiagramView::found_elements() const {
    return {impl_->found.begin(), impl_->found.end()};
}

void DiagramView::frame_found() {
    QRectF bounds;
    for (const auto& ref : impl_->found) {
        const auto node = impl_->nodes.find(ref);
        if (node != impl_->nodes.end()) bounds = bounds.united(node->second->sceneBoundingRect());
    }
    if (bounds.isEmpty()) return;
    bounds = bounds.adjusted(-90, -90, 90, 90);
    // Brought to the middle, and no closer than it already was. Zooming in on a
    // match would take the reader somewhere they did not ask to go; zooming out
    // is done only when what was found will not otherwise fit, because showing
    // part of an answer is worse than showing it small.
    const auto in_view = mapToScene(viewport()->rect()).boundingRect();
    if (bounds.width() > in_view.width() || bounds.height() > in_view.height()) {
        fitInView(bounds, Qt::KeepAspectRatio);
        impl_->zoom(zoom_factor());
    }
    centerOn(bounds.center());
}

void DiagramView::set_comments_visible(bool shown) {
    if (impl_->comments_shown == shown) return;
    impl_->comments_shown = shown;
    // Nothing about the document changed, so synchronize would see the same
    // revision and do nothing. The marks and what the pointer says are put in
    // step directly instead.
    impl_->refresh_comments();
}
bool DiagramView::comments_visible() const { return impl_->comments_shown; }

std::optional<CommentTarget> DiagramView::target_at(const QPoint& viewport_position) const {
    if (auto* node = impl_->node_at(viewport_position)) return CommentTarget{node->ref};
    const auto place = mapToScene(viewport_position);
    for (auto* item : impl_->scene->items(place)) {
        auto* edge = dynamic_cast<EdgeItem*>(item);
        if (!edge) continue;
        // An inheritance link is not a connector, so nothing can be pinned to
        // it; the triangle it belongs to is what a remark goes on instead.
        if (const auto* attribute = std::get_if<AttributeId>(&edge->descriptor.key))
            return CommentTarget{ConnectorRef{*attribute}};
        if (const auto* participant = std::get_if<ParticipantId>(&edge->descriptor.key))
            return CommentTarget{ConnectorRef{*participant}};
    }
    return std::nullopt;
}

std::vector<CommentId> DiagramView::comments_at(const QPoint& viewport_position) const {
    const auto& project = impl_->editor.project();
    const auto target = target_at(viewport_position);
    if (!target) return {};
    if (const auto* element = std::get_if<ElementRef>(&*target)) return comments_on(project, *element);
    return comments_on_connector(project, std::get<ConnectorRef>(*target));
}

std::vector<ConnectorRef> DiagramView::selected_connectors() const {
    std::vector<ConnectorRef> found;
    for (auto* item : impl_->scene->selectedItems()) {
        const auto* edge = dynamic_cast<EdgeItem*>(item);
        if (!edge) continue;
        if (const auto* attribute = std::get_if<AttributeId>(&edge->descriptor.key))
            found.emplace_back(ConnectorRef{*attribute});
        else if (const auto* participant = std::get_if<ParticipantId>(&edge->descriptor.key))
            found.emplace_back(ConnectorRef{*participant});
    }
    return found;
}

void DiagramView::set_grid_visible(bool enabled) { impl_->grid = enabled; viewport()->update(); }
void DiagramView::set_align_to_grid(bool enabled) { impl_->align_to_grid = enabled; }
void DiagramView::set_theme(ThemeId id) {
    const auto& colors = theme(id);
    if (impl_->theme_id == colors.id) return;
    impl_->theme_id = colors.id;
    // Appearance is local presentation state. Repaint existing projections without
    // touching revision, selection, viewport, or any in-progress pointer gesture.
    for (auto& [ref, node] : impl_->nodes) { (void)ref; node->set_theme(colors); }
    for (auto& [key, edge] : impl_->edges) { (void)key; edge->set_theme(colors); }
    setBackgroundBrush(colors.canvas);
    viewport()->update();
}
ThemeId DiagramView::theme_id() const { return impl_->theme_id; }
double DiagramView::zoom_factor() const { return transform().m11(); }

void DiagramView::delete_selection() {
    cancel_interaction();
    const auto elements = selected_elements();
    std::vector<std::pair<RelationshipId, ParticipantId>> participants;
    std::vector<AttributeId> detached_attributes;
    std::vector<application::Editor::InheritanceLink> detached_inheritance;
    for (auto* item : impl_->scene->selectedItems()) {
        if (const auto* edge = dynamic_cast<EdgeItem*>(item)) {
            // Review 2026-09-15, finding 1: every link that was not a
            // participant was read as an attribute link, and an inheritance
            // link threw bad_variant_access on delete. Each kind is now told
            // apart by its key and cut in the same edit.
            if (const auto* inheritance = std::get_if<InheritanceKey>(&edge->descriptor.key))
                detached_inheritance.emplace_back(inheritance->specialization, inheritance->subtype);
            else if (edge->descriptor.relationship)
                participants.emplace_back(*edge->descriptor.relationship, std::get<ParticipantId>(edge->descriptor.key));
            else detached_attributes.push_back(std::get<AttributeId>(edge->descriptor.key));
        }
    }
    if (!elements.empty() || !participants.empty() || !detached_attributes.empty() || !detached_inheritance.empty())
        impl_->publish(impl_->editor.erase(elements, participants, detached_attributes, detached_inheritance));
}

void DiagramView::align_selection_for_test(const std::vector<ElementRef>& elements, bool along_x, int which) {
    impl_->align_selection(elements, along_x, which);
}
void DiagramView::preview_transparency(const std::vector<ElementRef>& elements, int percent) {
    for (const auto& ref : elements)
        if (const auto found = impl_->nodes.find(ref); found != impl_->nodes.end()) found->second->set_transparency(percent);
}
std::vector<ElementRef> DiagramView::selected_symbols() const {
    const auto& notes = impl_->editor.project().notes;
    std::vector<ElementRef> symbols;
    for (const auto& ref : selected_elements()) {
        const auto* note_id = std::get_if<NoteId>(&ref);
        if (!note_id) continue;
        if (const auto found = notes.find(*note_id); found != notes.end() && found->second.plain)
            symbols.push_back(ref);
    }
    return symbols;
}

void DiagramView::resize_symbols(double factor) {
    const auto symbols = selected_symbols();
    if (symbols.empty() || factor <= 0) return;
    const auto& layout = impl_->editor.project().layout;
    std::map<ElementRef, Rect> boxes;
    for (const auto& ref : symbols) {
        const auto found = layout.find(ref);
        if (found == layout.end()) continue;
        const auto& box = found->second;
        const auto width = std::clamp(box.width * factor, min_symbol_size, max_symbol_size);
        const auto height = std::clamp(box.height * factor, min_symbol_size, max_symbol_size);
        // About its own centre, so a symbol grows in place instead of walking
        // down and to the right as it is enlarged.
        boxes.emplace(ref, Rect{box.x + (box.width - width) / 2, box.y + (box.height - height) / 2,
                                width, height});
    }
    if (boxes.empty()) return;
    const auto result = impl_->editor.resize_symbols(boxes);
    if (!result) impl_->displayed_revision.reset();
    impl_->publish(result);
}

void DiagramView::set_transparency(const std::vector<ElementRef>& elements, int percent) {
    const auto result = impl_->editor.set_transparency(elements, static_cast<std::uint8_t>(std::clamp(percent, 0, 100)));
    if (!result) impl_->displayed_revision.reset(); // Put back the stored value after a refused edit.
    impl_->publish(result);
}

void DiagramView::cancel_interaction() {
    if (!impl_->drag_start.empty()) {
        impl_->synchronizing = true;
        std::set<EdgeItem*> dirty;
        for (const auto& [ref, rect] : impl_->drag_start) {
            const auto found = impl_->nodes.find(ref);
            if (found != impl_->nodes.end()) {
                found->second->setPos(rect.x, rect.y);
                const auto incident = impl_->incident.find(ref);
                if (incident != impl_->incident.end()) dirty.insert(incident->second.begin(), incident->second.end());
            }
        }
        impl_->synchronizing = false;
        for (auto* edge : dirty) edge->refresh();
        impl_->drag_start.clear();
        if (auto* grabber = impl_->scene->mouseGrabberItem()) grabber->ungrabMouse();
    }
    if (impl_->sizing) {
        // The size was only ever previewed on the item, so putting it back is
        // a matter of drawing the stored box again.
        if (const auto found = impl_->nodes.find(impl_->sizing->ref); found != impl_->nodes.end()) {
            impl_->synchronizing = true;
            found->second->setPos(impl_->sizing->start.x, impl_->sizing->start.y);
            found->second->set_size(impl_->sizing->start.width, impl_->sizing->start.height);
            impl_->synchronizing = false;
        }
        impl_->sizing.reset();
    }
    impl_->clear_guides();
    if (impl_->bending || impl_->rejoining) {
        // Discard the previewed bend or join; the next projection restores the stored one.
        impl_->bending.reset();
        impl_->rejoining.reset();
        impl_->displayed_revision.reset();
        synchronize();
    }
    impl_->cancel_inline_edit();
    impl_->connect_start.reset();
    impl_->connect_start_direction.reset();
    impl_->connect_pointer.reset();
    impl_->connect_hover.reset();
    impl_->panning = false;
    setCursor(impl_->active_tool == Tool::Pan ? Qt::OpenHandCursor : impl_->active_tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
}

void DiagramView::drawBackground(QPainter* painter, const QRectF& rect) {
    const auto& colors = theme(impl_->theme_id);
    painter->fillRect(rect, colors.canvas);
    impl_->paint_paper(painter, rect, colors);
    // The dots are an editing aid rather than decoration, so they give way to
    // a paper that rules the canvas itself.
    if (!impl_->grid || impl_->background.style != domain::BackgroundStyle::Theme) return;
    // Keep the grid sparse when zoomed out; its iteration cost stays viewport-bound.
    const qreal step = zoom_factor() < 0.4 ? 100 : grid_spacing;
    const auto left = std::floor(rect.left() / step) * step;
    const auto top = std::floor(rect.top() / step) * step;
    QPen pen(colors.grid);
    pen.setCosmetic(true);
    painter->setPen(pen);
    for (qreal x = left; x <= rect.right(); x += step)
        for (qreal y = top; y <= rect.bottom(); y += step) painter->drawPoint(QPointF(x, y));
}


// Right-clicking an element offers what can be done to it and, mainly, what
// colour it should be. It acts on the whole selection, so several elements can
// be given one colour in a single step.
void DiagramView::contextMenuEvent(QContextMenuEvent* event) {
    auto* node = impl_->node_at(event->pos());
    if (!node) {
        participant_menu(event);
        return;
    }
    // Right-clicking outside the selection acts on what was clicked, which is
    // what anyone expects; right-clicking inside it keeps the selection whole.
    auto chosen = selected_elements();
    if (std::find(chosen.begin(), chosen.end(), node->ref) == chosen.end()) {
        select_elements({node->ref});
        chosen = {node->ref};
    }
    QMenu menu(this);
    const auto several = chosen.size() > 1;
    auto* duplicate = menu.addAction(several ? "Duplicate selection" : "Duplicate");
    duplicate->setObjectName("contextDuplicate");
    auto* remove = menu.addAction(several ? "Delete selection" : "Delete");
    remove->setObjectName("contextDelete");
    // A remark on what was right-clicked, or on the whole selection at once,
    // which is how one remark comes to cover a whole area of a diagram.
    auto* comment = menu.addAction(several ? "Comment on selection…" : "Comment…");
    comment->setObjectName("contextComment");
    // Size, for the one kind of element that has a size of its own to choose.
    // Offered only when everything chosen is a symbol: an entry that would act
    // on part of a selection is worse than no entry at all.
    const auto symbols = selected_symbols();
    QAction* enlarge = nullptr;
    QAction* shrink = nullptr;
    if (!symbols.empty() && symbols.size() == chosen.size()) {
        menu.addSeparator();
        enlarge = menu.addAction(several ? "Enlarge symbols" : "Enlarge");
        enlarge->setObjectName("contextEnlarge");
        shrink = menu.addAction(several ? "Shrink symbols" : "Shrink");
        shrink->setObjectName("contextShrink");
    }
    menu.addSeparator();
    auto* colours = menu.addMenu(several ? "Colour selection" : "Colour");
    colours->setObjectName("contextColour");
    for (const auto& [name, colour] : swatches()) {
        auto* entry = colours->addAction(swatch_icon(colour), QString::fromLatin1(name));
        entry->setData(colour);
        entry->setObjectName("swatch" + QString::fromLatin1(name));
    }
    colours->addSeparator();
    auto* custom = colours->addAction("Custom colour…");
    custom->setObjectName("contextCustomColour");
    auto* clear = colours->addAction("Use theme colour");
    clear->setObjectName("contextClearColour");
    // Nothing to clear when none of the selection has been given a colour.
    const auto& project = impl_->editor.project();
    clear->setEnabled(std::any_of(chosen.begin(), chosen.end(),
                                  [&](const ElementRef& ref) { return project.colours.contains(ref); }));
    // Transparency, as a bar from none to all, below the colours. It applies
    // to the whole selection over whatever colour each element has, previews
    // as it is dragged, and is written once when the slider is let go.
    const auto stored = project.transparency.find(chosen.front());
    int committed = stored == project.transparency.end() ? 0 : stored->second;
    auto* row = new QWidget(&menu);
    auto* row_layout = new QHBoxLayout(row);
    row_layout->setContentsMargins(22, 4, 12, 4);
    row_layout->setSpacing(8);
    auto* caption = new QLabel("Transparency", row);
    auto* slider = new QSlider(Qt::Horizontal, row);
    slider->setObjectName("contextTransparency");
    slider->setRange(0, 100);
    slider->setValue(committed);
    slider->setMinimumWidth(120);
    auto* percent = new QLabel(QString("%1%").arg(committed), row);
    percent->setMinimumWidth(36);
    row_layout->addWidget(caption);
    row_layout->addWidget(slider, 1);
    row_layout->addWidget(percent);
    auto* transparency = new QWidgetAction(&menu);
    transparency->setObjectName("contextTransparencyRow");
    transparency->setDefaultWidget(row);
    menu.addAction(transparency);
    connect(slider, &QSlider::valueChanged, &menu, [this, chosen, percent](int value) {
        percent->setText(QString("%1%").arg(value));
        preview_transparency(chosen, value);
    });
    const auto commit = [this, chosen, slider, &committed] {
        if (slider->value() == committed) return;
        committed = slider->value();
        set_transparency(chosen, committed);
    };
    connect(slider, &QSlider::sliderReleased, &menu, commit);
    // Arrow keys and clicks on the bar move it without a release, so whatever
    // it reads when the menu goes is written then.
    connect(&menu, &QMenu::aboutToHide, &menu, commit);
    // Lining several elements up, which is what makes the lines between them
    // run straight. It is offered only when there is more than one to line up.
    struct Alignment { QAction* action; bool along_x; int which; };
    std::vector<Alignment> alignments;
    if (chosen.size() > 1) {
        menu.addSeparator();
        auto* align = menu.addMenu("Align");
        align->setObjectName("contextAlign");
        const std::array<std::tuple<const char*, const char*, bool, int>, 6> entries{{
            {"Left edges", "alignLeft", true, 0}, {"Centres", "alignCentres", true, 1},
            {"Right edges", "alignRight", true, 2}, {"Top edges", "alignTop", false, 0},
            {"Middles", "alignMiddles", false, 1}, {"Bottom edges", "alignBottom", false, 2}}};
        for (const auto& [label, name, along_x, which] : entries) {
            if (!along_x && which == 0) align->addSeparator();  // the two axes read apart
            auto* entry = align->addAction(QString::fromLatin1(label));
            entry->setObjectName(QString::fromLatin1(name));
            alignments.push_back({entry, along_x, which});
        }
    }
    // Locking the lines a selection touches, which is how a diagram is tidied:
    // a region at a time rather than one line at a time.
    const auto lines = impl_->connectors_of(chosen);
    impl_->add_lock_entries(menu, lines, lines.size() == 1 ? "this connector" : "these connectors");
    menu.addSeparator();
    impl_->add_insert_menu(menu, mapToScene(event->pos()));

    auto* picked = menu.exec(event->globalPos());
    // The Insert and connector entries act as they are triggered, so there is
    // nothing left to do for them here.
    if (!picked || picked->objectName().startsWith("contextInsert")
        || picked->objectName().endsWith("Connectors")) return;
    if (picked == comment) {
        if (on_comment) {
            std::vector<CommentTarget> targets;
            for (const auto& ref : chosen) targets.emplace_back(ref);
            on_comment(std::move(targets));
        }
        return;
    }
    if (picked == duplicate) { impl_->publish(impl_->editor.duplicate(chosen)); return; }
    if (picked == remove) { delete_selection(); return; }
    if (enlarge && picked == enlarge) { resize_symbols(symbol_step); return; }
    if (shrink && picked == shrink) { resize_symbols(1 / symbol_step); return; }
    for (const auto& alignment : alignments)
        if (picked == alignment.action) {
            impl_->align_selection(chosen, alignment.along_x, alignment.which);
            return;
        }
    if (picked == clear) { impl_->publish(impl_->editor.recolour(chosen, {})); return; }
    QColor colour;
    if (picked == custom) {
        // Start from whatever the first selected element already wears, so the
        // dialog opens on the colour being changed rather than on nothing.
        const auto current = project.colours.find(chosen.front());
        const auto initial = current == project.colours.end()
            ? QColor(Qt::white)
            : QColor(current->second.red, current->second.green, current->second.blue);
        colour = QColorDialog::getColor(initial, this, "Choose a surface colour");
        if (!colour.isValid()) return;
    } else {
        colour = picked->data().value<QColor>();
        if (!colour.isValid()) return;
    }
    impl_->publish(impl_->editor.recolour(chosen, domain::Colour{
        static_cast<std::uint8_t>(colour.red()), static_cast<std::uint8_t>(colour.green()),
        static_cast<std::uint8_t>(colour.blue())}));
}


// The constraints on one side of a relationship, offered where that side is
// drawn. They can be read off the line, so this is where the hand goes to
// change them; the properties panel says the same thing in words.
// Right-clicking empty canvas offers what can be put there.
void DiagramView::canvas_menu(QContextMenuEvent* event) {
    QMenu menu(this);
    impl_->add_insert_menu(menu, mapToScene(event->pos()));
    // With nothing chosen, the offer covers the whole diagram.
    impl_->add_lock_entries(menu, impl_->every_connector(), QStringLiteral("every connector"));
    menu.exec(event->globalPos());
}

void DiagramView::participant_menu(QContextMenuEvent* event) {
    auto* edge = impl_->edge_at(event->pos());
    if (!edge) { canvas_menu(event); return; }
    const auto* participant_key = std::get_if<ParticipantId>(&edge->descriptor.key);
    if (!participant_key || !edge->descriptor.relationship) { QGraphicsView::contextMenuEvent(event); return; }
    const auto relationship_id = *edge->descriptor.relationship;
    const auto& project = impl_->editor.project();
    const auto found = project.relationships.find(relationship_id);
    if (found == project.relationships.end()) { QGraphicsView::contextMenuEvent(event); return; }
    const auto side = std::find_if(found->second.participants.begin(), found->second.participants.end(),
                                   [&](const auto& item) { return item.id == *participant_key; });
    if (side == found->second.participants.end()) { QGraphicsView::contextMenuEvent(event); return; }

    // Showing the line as selected makes it plain which side is being changed,
    // which matters when a relationship has two ends a short way apart.
    edge->setSelected(true);
    QMenu menu(this);
    // The same words as the properties panel, so the two never disagree about
    // what a constraint is called.
    auto* maximum = menu.addMenu("Maximum");
    maximum->setObjectName("sideMaximum");
    auto* minimum = menu.addMenu("Minimum");
    minimum->setObjectName("sideMinimum");
    const auto entry = [](QMenu* parent, const QString& text, const QString& name, bool current) {
        auto* action = parent->addAction(text);
        action->setObjectName(name);
        action->setCheckable(true);
        action->setChecked(current);
        return action;
    };
    auto* one = entry(maximum, "1 — One", "sideOne", side->maximum == Cardinality::One);
    auto* many = entry(maximum, "M — Many", "sideMany", side->maximum == Cardinality::Many);
    auto* partial = entry(minimum, "Partial — optional", "sidePartial", side->participation == Participation::Partial);
    auto* total = entry(minimum, "Total — required", "sideTotal", side->participation == Participation::Total);
    auto* shown = menu.addAction("Show constraints on this side");
    shown->setObjectName("sideShowConstraints");
    shown->setCheckable(true);
    shown->setChecked(side->show_constraints);
    menu.addSeparator();
    // A remark about a cardinality usually belongs on the line rather than on
    // either shape it joins, which is why a line can carry one of its own.
    auto* comment = menu.addAction("Comment…");
    comment->setObjectName("sideComment");
    menu.addSeparator();
    auto* reverse = menu.addAction("Reverse sides");
    reverse->setObjectName("sideReverse");
    auto* disconnect = menu.addAction("Disconnect this side");
    disconnect->setObjectName("sideDisconnect");

    auto* picked = menu.exec(event->globalPos());
    if (!picked) return;
    if (picked == comment) {
        if (on_comment) on_comment({CommentTarget{ConnectorRef{*participant_key}}});
        return;
    }
    if (picked == shown) {
        // Hiding a side's constraints changes only what is drawn, so the two
        // submenus above still show what this side holds.
        impl_->publish(impl_->editor.show_participant_constraints(relationship_id, *participant_key,
                                                                  !side->show_constraints));
        return;
    }
    if (picked == reverse) { impl_->publish(impl_->editor.reverse_participants(relationship_id)); return; }
    if (picked == disconnect) {
        impl_->publish(impl_->editor.disconnect(relationship_id, *participant_key));
        return;
    }
    // Only the one constraint the user named changes; the other and the role
    // are carried through, since this menu is not where they are being edited.
    auto chosen_maximum = side->maximum;
    auto chosen_participation = side->participation;
    if (picked == one) chosen_maximum = Cardinality::One;
    else if (picked == many) chosen_maximum = Cardinality::Many;
    else if (picked == partial) chosen_participation = Participation::Partial;
    else if (picked == total) chosen_participation = Participation::Total;
    impl_->publish(impl_->editor.update_participant(relationship_id, *participant_key,
                                                    chosen_maximum, chosen_participation, side->role));
}

std::optional<QPointF> DiagramView::pointer_place() const { return impl_->pointer_place; }

void DiagramView::mousePressEvent(QMouseEvent* event) {
    setFocus();
    if (event->button() == Qt::MiddleButton || (event->button() == Qt::LeftButton && impl_->active_tool == Tool::Pan)) {
        impl_->panning = true;
        impl_->pan_start = event->position().toPoint();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }
    if (event->button() != Qt::LeftButton) { QGraphicsView::mousePressEvent(event); return; }
    if (impl_->active_tool == Tool::Connect) {
        impl_->connect_node(impl_->node_at(event->position().toPoint()), mapToScene(event->position().toPoint()));
        // Arming a source starts carrying a preview line. Releasing over another
        // element completes the connection; releasing where it started leaves it
        // armed, so click-then-click still works for anyone who prefers it.
        if (impl_->connect_start) impl_->connect_pointer = mapToScene(event->position().toPoint());
        impl_->connect_hover.reset();
        viewport()->update();
        event->accept();
        return;
    }
    if (impl_->active_tool == Tool::Specialization || impl_->active_tool == Tool::Generalization) {
        // The triangle is placed like any other element and wired up by hand,
        // so the tool only decides which way its apex points.
        const auto centre = mapToScene(event->position().toPoint());
        const auto result = impl_->editor.create_specialization("IS A", centred(centre, isa_body),
            impl_->active_tool == Tool::Generalization ? Inheritance::Generalization : Inheritance::Specialization);
        impl_->publish(result);
        if (result && result.created) {
            select_elements({*result.created});
            if (!impl_->tool_locked) set_tool(Tool::Select);
            impl_->status(QStringLiteral("Connect the supertype first, then each subtype."));
        }
        event->accept();
        return;
    }
    if (impl_->active_tool == Tool::Entity || impl_->active_tool == Tool::Attribute
        || impl_->active_tool == Tool::Relationship || impl_->active_tool == Tool::Note) {
        auto center = mapToScene(event->position().toPoint());
        if (impl_->align_to_grid) center = {std::round(center.x() / grid_spacing) * grid_spacing, std::round(center.y() / grid_spacing) * grid_spacing};
        application::EditResult result;
        if (impl_->active_tool == Tool::Entity) {
            result = impl_->editor.create_entity("Entity", centred(center, entity_body));
        } else if (impl_->active_tool == Tool::Relationship) {
            result = impl_->editor.create_relationship("Relationship", centred(center, relationship_body));
        } else if (impl_->active_tool == Tool::Note) {
            // The tool hands back first, as the others do, and the note is
            // then placed through the same path the menus use: handing back
            // afterwards would close the title the note opens for.
            if (!impl_->tool_locked) set_tool(Tool::Select);
            impl_->place_note(center);
            event->accept();
            return;
        } else {
            std::optional<AttributeOwner> owner;
            const auto selection = selected_elements();
            if (selection.size() == 1 && exists(impl_->editor.project(), selection.front())
                && !is_figure(selection.front())) {
                const auto* attribute = std::get_if<AttributeId>(&selection.front());
                if (!attribute || impl_->editor.project().attributes.at(*attribute).kind == AttributeKind::Composite) owner = selection.front();
            }
            result = impl_->editor.create_attribute("Attribute", centred(center, attribute_body), owner);
        }
        impl_->publish(result);
        if (result && result.created) {
            select_elements({*result.created});
            if (!impl_->tool_locked) set_tool(Tool::Select);
        }
        event->accept();
        return;
    }
    // Grabbing a selected connector's handle reshapes it instead of starting a
    // rubber band. The bend is previewed on the item and committed on release.
    if (impl_->active_tool == Tool::Select) {
        // A grip on a selected symbol's corner resizes it rather than moving
        // it. Tested before anything else here: the grips are drawn on top of
        // everything the symbol sits over, so that is what they are clicked on.
        if (event->button() == Qt::LeftButton) {
            if (auto* node = impl_->node_at(event->position().toPoint()); node && node->sizeable()) {
                const auto scene_press = mapToScene(event->position().toPoint());
                const auto corner = node->grip_at(node->mapFromScene(scene_press));
                const auto& layout = impl_->editor.project().layout;
                if (const auto found = layout.find(node->ref); corner >= 0 && found != layout.end()) {
                    impl_->sizing = Impl::SizeDrag{node->ref, found->second, corner};
                    event->accept();
                    return;
                }
            }
        }
        // The padlock is tested before the bend grip: it sits beside it, and a
        // click meant for the lock must not start reshaping the line instead.
        if (auto* edge = impl_->lock_at(event->position().toPoint())) {
            const auto& edge_key = edge->descriptor.key;
            const ConnectorRef key = std::holds_alternative<AttributeId>(edge_key)
                ? ConnectorRef{std::get<AttributeId>(edge_key)} : ConnectorRef{std::get<ParticipantId>(edge_key)};
            // Locking pins the joins exactly where they are drawn now, so the
            // line does not move at the moment it is locked.
            impl_->publish(edge->locked()
                ? impl_->editor.pin_connector(key, {}, {})
                : impl_->editor.pin_connector(key, edge->owner_direction(), edge->child_direction()));
            event->accept();
            return;
        }
        if (auto* edge = impl_->handle_at(event->position().toPoint())) {
            // No grab cursor: shaping a line is done by clicking it, and a palm
            // would suggest the old business of finding a grip and hauling it.
            impl_->bending = edge->descriptor.key;
            event->accept();
            return;
        }
        // An end grip carries that end of the line to another point on its
        // shape. It is previewed on the item and pinned there on release.
        if (bool owner_end = false; auto* edge = impl_->end_at(event->position().toPoint(), owner_end)) {
            impl_->rejoining = Impl::EndDrag{edge->descriptor.key, owner_end, false};
            event->accept();
            return;
        }
        const auto scene_press = mapToScene(event->position().toPoint());
        if (auto* edge = impl_->selected_edge_at(event->position().toPoint())) {
            // Shaping a loop starts from the loop, not from the straight line
            // it would otherwise fall back to.
            edge->adopt_route();
            // A grip grabs its own corner; anywhere else along the line arms a
            // drag that will put a new corner there. Neither changes anything
            // until the pointer moves, so a click that only meant to select is
            // still only a selection.
            const auto existing = edge->corner_at(scene_press);
            impl_->routing = Impl::RouteDrag{edge->descriptor.key,
                                             existing.value_or(edge->insertion_for(scene_press)),
                                             scene_press, existing.has_value()};
            event->accept();
            return;
        }
    }
    // Qt's item selection uses Control; also accept Shift as advertised in UI.
    const auto original_modifiers = event->modifiers();
    if (original_modifiers.testFlag(Qt::ShiftModifier)) event->setModifiers(original_modifiers | Qt::ControlModifier);
    QGraphicsView::mousePressEvent(event);
    event->setModifiers(original_modifiers);
    impl_->drag_start.clear();
    if (const auto* anchor = impl_->node_at(event->position().toPoint()); anchor && anchor->isSelected()) {
        const auto& layout = impl_->editor.project().layout;
        impl_->drag_anchor = anchor->pos();
        impl_->drag_anchor_ref = anchor->ref;
        impl_->minimum_drag = {-2 * max_coordinate, -2 * max_coordinate};
        impl_->maximum_drag = {2 * max_coordinate, 2 * max_coordinate};
        for (const auto& ref : selected_elements()) {
            if (const auto found = layout.find(ref); found != layout.end()) {
                impl_->drag_start.emplace(ref, found->second);
                const auto& rect = found->second;
                impl_->minimum_drag.setX(std::max(impl_->minimum_drag.x(), -max_coordinate - rect.x));
                impl_->minimum_drag.setY(std::max(impl_->minimum_drag.y(), -max_coordinate - rect.y));
                impl_->maximum_drag.setX(std::min(impl_->maximum_drag.x(), max_coordinate - rect.x - rect.width));
                impl_->maximum_drag.setY(std::min(impl_->maximum_drag.y(), max_coordinate - rect.y - rect.height));
            }
        }
    }
}
void DiagramView::drawForeground(QPainter* painter, const QRectF& rect) {
    QGraphicsView::drawForeground(painter, rect);
    // What a dragged element has come into line with. Thin and in the theme's
    // own warning colour, which is the one hue no shape on the canvas wears,
    // so a guide is never mistaken for part of the diagram.
    if (!impl_->guides.empty()) {
        painter->save();
        QPen pen(theme(impl_->theme_id).warning, 1.0);
        pen.setCosmetic(true);
        painter->setPen(pen);
        for (const auto& guide : impl_->guides) painter->drawLine(guide);
        painter->restore();
    }
    if (!impl_->connect_start || !impl_->connect_pointer) return;
    const auto source = impl_->nodes.find(*impl_->connect_start);
    if (source == impl_->nodes.end()) return;
    const auto& colors = theme(impl_->theme_id);
    const auto pointer = *impl_->connect_pointer;
    // A valid drop target is outlined, so the pointer states what the release
    // will do before the user commits to it.
    const bool valid = impl_->connect_hover
        && impl_->plan_connection(*impl_->connect_start, *impl_->connect_hover).has_value();
    painter->setRenderHint(QPainter::Antialiasing);
    QPen pen(valid ? colors.accent : colors.connector, 1.8, Qt::DashLine);
    pen.setCosmetic(true);
    painter->setPen(pen);
    painter->setBrush(Qt::NoBrush);
    // The line leaves the source where it was clicked when that is where the
    // join will be pinned, so the preview shows the line that will be drawn.
    const auto leaving = impl_->connect_start_direction
        ? source->second->boundary_at(*impl_->connect_start_direction)
        : source->second->boundary_toward(pointer);
    painter->drawLine(leaving, pointer);
    if (valid) {
        const auto target = impl_->nodes.find(*impl_->connect_hover);
        if (target != impl_->nodes.end()) {
            QPen outline(colors.accent, 2.0);
            outline.setCosmetic(true);
            painter->setPen(outline);
            painter->drawRoundedRect(target->second->sceneBoundingRect().adjusted(-4, -4, 4, 4), 6, 6);
            // And a mark on the outline where this end will be pinned, so the
            // join is seen before the button is released rather than after.
            if (const auto direction = impl_->join_direction(target->second, pointer)) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(colors.accent);
                painter->drawEllipse(target->second->boundary_at(*direction), 4.5, 4.5);
            }
        }
    } else {
        painter->setPen(Qt::NoPen);
        painter->setBrush(colors.connector);
        painter->drawEllipse(pointer, 3.5, 3.5);
    }
}
void DiagramView::begin_rename(const ElementRef& element) { impl_->begin_inline_edit(element); }
bool DiagramView::renaming() const { return impl_->renaming.has_value(); }
void DiagramView::commit_rename() { impl_->commit_inline_edit(); }

void DiagramView::scrollContentsBy(int dx, int dy) {
    QGraphicsView::scrollContentsBy(dx, dy);
    impl_->place_inline_editor();
    // A picture behind the diagram is fixed to the view rather than to the
    // canvas, so scrolling has to repaint all of it rather than the strip the
    // scroll uncovered.
    if (impl_->background.style == domain::BackgroundStyle::Image) viewport()->update();
}

bool DiagramView::eventFilter(QObject* watched, QEvent* event) {
    if (impl_->inline_editor && watched == static_cast<QObject*>(impl_->inline_editor)
        && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        // Discard the pending text before the field can report it as finished.
        impl_->cancel_inline_edit();
        setFocus();
        return true;
    }
    return QGraphicsView::eventFilter(watched, event);
}

void DiagramView::mouseDoubleClickEvent(QMouseEvent* event) {
    // Double-clicking an element renames it in place; double-clicking a
    // connector restores its automatic routing.
    if (event->button() == Qt::LeftButton && impl_->active_tool == Tool::Select) {
        if (auto* node = impl_->node_at(event->position().toPoint())) {
            impl_->begin_inline_edit(node->ref);
            event->accept();
            return;
        }
    }
    if (event->button() == Qt::LeftButton && impl_->active_tool == Tool::Select
        && !impl_->node_at(event->position().toPoint())) {
        if (auto* edge = impl_->edge_at(event->position().toPoint())) {
            if (std::holds_alternative<InheritanceKey>(edge->descriptor.key)) { event->accept(); return; }
            const auto& key = edge->descriptor.key;
            const auto connector = std::holds_alternative<AttributeId>(key)
                ? ConnectorRef{std::get<AttributeId>(key)} : ConnectorRef{std::get<ParticipantId>(key)};
            // Double-clicking one corner takes out that corner; double-clicking
            // the line itself straightens the whole thing. Otherwise a routed
            // line could only ever be undone all at once.
            const auto corner = edge->corner_at(mapToScene(event->position().toPoint()));
            if (corner && *corner < edge->descriptor.waypoints.size()) {
                std::vector<domain::Point> route;
                for (std::size_t index = 0; index < edge->descriptor.waypoints.size(); ++index)
                    if (index != *corner)
                        route.push_back(domain::Point{edge->descriptor.waypoints[index].x(),
                                                      edge->descriptor.waypoints[index].y()});
                impl_->publish(impl_->editor.route_connector(connector, std::move(route)));
            } else if (!edge->descriptor.waypoints.empty()) {
                impl_->publish(impl_->editor.route_connector(connector, {}));
            } else {
                impl_->publish(impl_->editor.bend_connector(connector, {}));
            }
            event->accept();
            return;
        }
    }
    QGraphicsView::mouseDoubleClickEvent(event);
}
void DiagramView::mouseMoveEvent(QMouseEvent* event) {
    // Kept for anything that puts something down where the user was working
    // rather than in the middle of the view. A graphics view tracks the mouse
    // already, so this follows the pointer and not only its clicks.
    impl_->pointer_place = mapToScene(event->position().toPoint());
    if (impl_->panning) {
        const auto delta = event->position().toPoint() - impl_->pan_start;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        impl_->pan_start = event->position().toPoint();
        event->accept();
        return;
    }
    if (impl_->connect_start) {
        impl_->connect_pointer = mapToScene(event->position().toPoint());
        const auto* hovered = impl_->node_at(event->position().toPoint());
        impl_->connect_hover = hovered ? std::optional<ElementRef>{hovered->ref} : std::nullopt;
        viewport()->update();
        event->accept();
        return;
    }
    if (impl_->sizing) {
        // Previewed on the item as the grip is hauled, and written once when
        // it is let go, the way a bend and a move are.
        const auto box = Impl::sized_box(*impl_->sizing, mapToScene(event->position().toPoint()));
        if (const auto found = impl_->nodes.find(impl_->sizing->ref); found != impl_->nodes.end()) {
            impl_->synchronizing = true;
            found->second->setPos(box.x, box.y);
            found->second->set_size(box.width, box.height);
            impl_->synchronizing = false;
        }
        event->accept();
        return;
    }
    if (impl_->routing) {
        const auto found = impl_->edges.find(impl_->routing->key);
        if (found != impl_->edges.end()) {
            auto* edge = found->second;
            const auto here = mapToScene(event->position().toPoint());
            const auto travelled = std::hypot(here.x() - impl_->routing->press.x(),
                                              here.y() - impl_->routing->press.y());
            // Four pixels of travel is what separates shaping the line from
            // clicking it. Below that nothing is created, so the corner cannot
            // appear under a hand that merely twitched on a selected line.
            if (!impl_->routing->grabbed && travelled > 4.0) {
                auto& corners = edge->descriptor.waypoints;
                corners.insert(corners.begin() + static_cast<std::ptrdiff_t>(impl_->routing->index), here);
                impl_->routing->grabbed = true;
            }
            if (impl_->routing->grabbed) {
                auto& corners = edge->descriptor.waypoints;
                if (impl_->routing->index < corners.size()) {
                    corners[impl_->routing->index] =
                        QPointF(std::clamp(here.x(), -max_coordinate, max_coordinate),
                                std::clamp(here.y(), -max_coordinate, max_coordinate));
                    edge->refresh();
                }
            }
        }
        event->accept();
        return;
    }
    if (impl_->rejoining) {
        const auto found = impl_->edges.find(impl_->rejoining->key);
        if (found != impl_->edges.end()) {
            auto* edge = found->second;
            // The end follows the pointer around its own shape's outline: the
            // join is the direction of the pointer from that shape's centre.
            auto* node = impl_->rejoining->owner_end ? edge->head_node() : edge->tail_node();
            const auto here = mapToScene(event->position().toPoint());
            // Over its own shape, the end slides around the outline: the join
            // is the direction of the pointer from that shape's centre. Once
            // the pointer leaves the shape it is carrying a corner instead,
            // and the line stops at wherever it is let go.
            const auto body = node->body_rect().translated(node->scenePos());
            auto& corners = edge->descriptor.waypoints;
            if (body.contains(here)) {
                if (impl_->rejoining->placed) {
                    corners.erase(impl_->rejoining->owner_end ? corners.begin() : corners.end() - 1);
                    impl_->rejoining->placed = false;
                }
                (impl_->rejoining->owner_end ? edge->descriptor.owner_anchor : edge->descriptor.child_anchor)
                    = node->direction_of(here);
            } else {
                const QPointF stop(std::clamp(here.x(), -max_coordinate, max_coordinate),
                                   std::clamp(here.y(), -max_coordinate, max_coordinate));
                if (!impl_->rejoining->placed) {
                    // The shape the canvas had worked out becomes the user's
                    // here, so a stopping point is added to the line they can
                    // see rather than replacing it with a straight one.
                    edge->adopt_route();
                    corners.insert(impl_->rejoining->owner_end ? corners.begin() : corners.end(), stop);
                    impl_->rejoining->placed = true;
                } else {
                    corners[impl_->rejoining->owner_end ? 0 : corners.size() - 1] = stop;
                }
            }
            edge->refresh();
        }
        event->accept();
        return;
    }
    if (impl_->bending) {
        const auto found = impl_->edges.find(*impl_->bending);
        if (found != impl_->edges.end()) {
            auto* edge = found->second;
            // The handle slides along the connector's normal, so the bend is the
            // pointer's signed distance from the straight line between endpoints.
            const auto delta = mapToScene(event->position().toPoint()) - edge->midpoint();
            const auto along = QPointF::dotProduct(delta, edge->perpendicular());
            edge->descriptor.offset = std::clamp(along, -max_coordinate, max_coordinate);
            edge->refresh();
        }
        event->accept();
        return;
    }
    // Nothing is being dragged, so the pointer's job is to say what the thing
    // under it would do. A corner grip resizes along its own diagonal, and the
    // arrow comes back only from one of those two shapes, so this never argues
    // with the hand that pans or the cross that draws.
    if (impl_->active_tool == Tool::Select && event->buttons() == Qt::NoButton) {
        auto* node = impl_->node_at(event->position().toPoint());
        const auto corner = node && node->sizeable()
            ? node->grip_at(node->mapFromScene(mapToScene(event->position().toPoint()))) : -1;
        if (corner >= 0) setCursor(corner == 0 || corner == 2 ? Qt::SizeFDiagCursor : Qt::SizeBDiagCursor);
        else if (cursor().shape() == Qt::SizeFDiagCursor || cursor().shape() == Qt::SizeBDiagCursor)
            setCursor(Qt::ArrowCursor);
    }
    QGraphicsView::mouseMoveEvent(event);
}
void DiagramView::mouseReleaseEvent(QMouseEvent* event) {
    if (impl_->sizing && event->button() == Qt::LeftButton) {
        const auto drag = *impl_->sizing;
        impl_->sizing.reset();
        const auto found = impl_->nodes.find(drag.ref);
        if (found == impl_->nodes.end()) { event->accept(); return; }
        const Rect box{found->second->pos().x(), found->second->pos().y(),
                       found->second->body_rect().width(), found->second->body_rect().height()};
        // A grip clicked and let go without travelling asks for nothing, so
        // nothing is written and the history stays clear of empty steps.
        if (box != drag.start) {
            const auto result = impl_->editor.resize_symbols({{drag.ref, box}});
            if (!result) impl_->displayed_revision.reset(); // Put the stored size back after a refusal.
            impl_->publish(result);
        }
        event->accept();
        return;
    }
    if (impl_->panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        impl_->panning = false;
        if (impl_->active_tool == Tool::Pan && !impl_->tool_locked && event->button() == Qt::LeftButton) {
            set_tool(Tool::Select);
            event->accept();
            return;
        }
        setCursor(impl_->active_tool == Tool::Pan ? Qt::OpenHandCursor : impl_->active_tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        event->accept();
        return;
    }
    if (impl_->connect_start && event->button() == Qt::LeftButton) {
        auto* released_over = impl_->node_at(event->position().toPoint());
        impl_->connect_hover.reset();
        // Releasing on a different element finishes the drag. Releasing on the
        // source is a plain click, which leaves the source armed.
        if (released_over && released_over->ref != *impl_->connect_start)
            impl_->connect_node(released_over, mapToScene(event->position().toPoint()));
        viewport()->update();
        event->accept();
        return;
    }
    if (impl_->routing) {
        const auto drag = *impl_->routing;
        impl_->routing.reset();
        const auto found = impl_->edges.find(drag.key);
        // A click that never travelled still leaves a corner where it landed.
        // Placing one is the whole gesture: the line is fixed at that point, and
        // it can be picked up and moved afterwards like any other corner.
        if (!drag.grabbed && found != impl_->edges.end()) {
            auto& corners = found->second->descriptor.waypoints;
            if (drag.index <= corners.size()) {
                corners.insert(corners.begin() + static_cast<std::ptrdiff_t>(drag.index), drag.press);
                found->second->refresh();
            }
        }
        if (found != impl_->edges.end()) {
            const auto connector = std::holds_alternative<AttributeId>(drag.key)
                ? ConnectorRef{std::get<AttributeId>(drag.key)} : ConnectorRef{std::get<ParticipantId>(drag.key)};
            std::vector<domain::Point> route;
            for (const auto& corner : found->second->descriptor.waypoints)
                route.push_back(domain::Point{corner.x(), corner.y()});
            const auto result = impl_->editor.route_connector(connector, std::move(route));
            // Restore the projection after a rejected route, or the preview
            // would be left standing as though it had been accepted.
            if (!result) impl_->displayed_revision.reset();
            impl_->publish(result);
        }
        event->accept();
        return;
    }
    if (impl_->rejoining) {
        const auto drag = *impl_->rejoining;
        impl_->rejoining.reset();
        const auto found = impl_->edges.find(drag.key);
        if (found != impl_->edges.end() && !std::holds_alternative<InheritanceKey>(drag.key)) {
            const auto connector = std::holds_alternative<AttributeId>(drag.key)
                ? ConnectorRef{std::get<AttributeId>(drag.key)} : ConnectorRef{std::get<ParticipantId>(drag.key)};
            // The join and the stopping point are one thing the user did, so
            // they are written as one edit rather than as two.
            domain::Connector shaped;
            shaped.offset = found->second->descriptor.offset;
            shaped.owner_anchor = found->second->descriptor.owner_anchor;
            shaped.child_anchor = found->second->descriptor.child_anchor;
            for (const auto& corner : found->second->descriptor.waypoints)
                shaped.waypoints.push_back(domain::Point{corner.x(), corner.y()});
            const auto result = impl_->editor.shape_connector(connector, std::move(shaped));
            if (!result) impl_->displayed_revision.reset(); // Restore the projection after a rejected join.
            impl_->publish(result);
        }
        event->accept();
        return;
    }
    if (impl_->bending) {
        const auto key = *impl_->bending;
        impl_->bending.reset();
        setCursor(impl_->active_tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
        const auto found = impl_->edges.find(key);
        if (found != impl_->edges.end()) {
            if (std::holds_alternative<InheritanceKey>(key)) { event->accept(); return; }
            const auto connector = std::holds_alternative<AttributeId>(key)
                ? ConnectorRef{std::get<AttributeId>(key)} : ConnectorRef{std::get<ParticipantId>(key)};
            const auto result = impl_->editor.bend_connector(connector, found->second->descriptor.offset);
            if (!result) impl_->displayed_revision.reset(); // Restore the projection after a rejected bend.
            impl_->publish(result);
        }
        event->accept();
        return;
    }
    const auto original_modifiers = event->modifiers();
    if (original_modifiers.testFlag(Qt::ShiftModifier)) event->setModifiers(original_modifiers | Qt::ControlModifier);
    QGraphicsView::mouseReleaseEvent(event);
    event->setModifiers(original_modifiers);
    impl_->clear_guides();
    if (event->button() != Qt::LeftButton || impl_->drag_start.empty()) return;
    std::map<ElementRef, Rect> positions;
    for (const auto& [ref, start] : impl_->drag_start) {
        const auto found = impl_->nodes.find(ref);
        if (found == impl_->nodes.end()) continue;
        auto rect = start;
        rect.x = found->second->pos().x();
        rect.y = found->second->pos().y();
        if (rect != start) positions.emplace(ref, rect);
    }
    impl_->drag_start.clear();
    if (!positions.empty()) {
        const auto result = impl_->editor.move(positions);
        if (!result) impl_->displayed_revision.reset(); // Restore the projection after a rejected move.
        impl_->publish(result);
    }
}
// A pinch on a trackpad arrives from the system as a native gesture rather
// than as wheel turns. It zooms about the pointer, as the wheel does.
bool DiagramView::viewportEvent(QEvent* event) {
    if (event->type() == QEvent::NativeGesture) {
        auto* gesture = static_cast<QNativeGestureEvent*>(event);
        if (gesture->gestureType() == Qt::ZoomNativeGesture) {
            impl_->zoom(zoom_factor() * (1.0 + gesture->value()));
            event->accept();
            return true;
        }
    }
    return QGraphicsView::viewportEvent(event);
}
void DiagramView::wheelEvent(QWheelEvent* event) {
    // Two fingers travelling together are a palm laid on the diagram: they
    // move it, and it is the pinch above that zooms it. A trackpad says it is
    // one by reporting the distance its fingers actually covered, and by
    // giving its scroll a phase; a mouse wheel reports neither, so its notches
    // still zoom as they always have. The trackpad also fills in an angle for
    // the benefit of anything expecting a wheel, which is why the fingers have
    // to be asked about before the angle rather than after it.
    const bool fingers = !event->pixelDelta().isNull() || event->phase() != Qt::NoScrollPhase;
    if (fingers) {
        // Some devices give the phase without the pixels; a wheel's eighth of
        // a degree is the conventional pixel when that happens.
        const auto moved = event->pixelDelta().isNull() ? event->angleDelta() / 8 : event->pixelDelta();
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - moved.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - moved.y());
        event->accept();
        return;
    }
    const auto delta = event->angleDelta().y();
    if (delta != 0) impl_->zoom(zoom_factor() * std::pow(1.0015, static_cast<qreal>(delta)));
    event->accept();
}
void DiagramView::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) { set_tool(Tool::Select); event->accept(); return; }
    if (event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) { delete_selection(); event->accept(); return; }
    if (event->matches(QKeySequence::SelectAll)) {
        std::vector<ElementRef> all;
        for (const auto& [ref, node] : impl_->nodes) { (void)node; all.push_back(ref); }
        select_elements(all);
        event->accept();
        return;
    }
    QGraphicsView::keyPressEvent(event);
}

} // namespace erdflow::desktop
