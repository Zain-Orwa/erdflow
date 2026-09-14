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
#include <QPainter>
#include <QPainterPathStroker>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>

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

// Black or white, whichever the eye can actually read on a given surface. The
// threshold is on relative luminance rather than on plain brightness, so a
// saturated yellow is treated as the light colour it is.
QColor readable_on(const QColor& surface) {
    const auto channel = [](double value) {
        return value <= 0.04045 ? value / 12.92 : std::pow((value + 0.055) / 1.055, 2.4);
    };
    const auto luminance = 0.2126 * channel(surface.redF()) + 0.7152 * channel(surface.greenF())
                         + 0.0722 * channel(surface.blueF());
    return luminance > 0.36 ? QColor(0x1a, 0x1a, 0x1a) : QColor(0xff, 0xff, 0xff);
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
        setZValue(1);
        set_theme(colors);
    }

    ElementRef ref;
    QString label;
    AttributeKind attribute_kind = AttributeKind::Normal;
    // An associative relationship is drawn as its diamond inside a rectangle,
    // because it takes part in further relationships as an entity would.
    bool associative = false;
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

    void apply_colors() {
        const auto& colors = *colors_;
        fill_ = colors.attribute_fill;
        border_ = colors.attribute_border;
        if (std::holds_alternative<EntityId>(ref)) {
            fill_ = colors.entity_fill;
            border_ = colors.entity_border;
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
        if (chosen_) {
            // A chosen colour replaces the fill and takes the border with it, or
            // the surface would sit inside an outline from a palette it has left.
            fill_ = *chosen_;
            border_ = chosen_->darker(145);
            // The label has to stay readable on a colour the theme knows nothing
            // about, so it is chosen against the surface rather than the theme.
            text_ = readable_on(*chosen_);
        }
        selection_ = colors.accent;
        update();
    }

    QRectF boundingRect() const override { return bounds_.adjusted(-4, -4, 4, 4); }
    QRectF body_rect() const { return bounds_; }
    // The element's own Chen outline, without the rectangle that surrounds an
    // associative one. This is what carries the fill.
    QPainterPath body_path() const {
        QPainterPath path;
        if (std::holds_alternative<EntityId>(ref)) {
            path.addRect(bounds_);
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
            path.moveTo(bounds_.center().x(), bounds_.top());
            path.lineTo(bounds_.right(), bounds_.center().y());
            path.lineTo(bounds_.center().x(), bounds_.bottom());
            path.lineTo(bounds_.left(), bounds_.center().y());
            path.closeSubpath();
        }
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
        return path;
    }
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
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);
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
        // Names on the canvas are read at a glance and often at less than full
        // zoom, so they sit a step above the interface's own type and never
        // below medium weight.
        auto font = painter->font();
        font.setPointSizeF(12.5);
        font.setWeight(std::holds_alternative<EntityId>(ref) ? QFont::Bold : QFont::Medium);
        font.setUnderline(std::holds_alternative<AttributeId>(ref) && attribute_kind == AttributeKind::Key);
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
    QString role;
    qreal offset = 0;
    // Where the line is pinned to meet each shape, when the user has locked it.
    std::optional<double> owner_anchor;
    std::optional<double> child_anchor;
    // Corners the line is routed through, when the user has given it any.
    std::vector<QPointF> waypoints;
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
    NodeItem* source;
    NodeItem* target;
    Notation notation = Notation::Chen;
    LineStyle style = LineStyle::Curved;

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
        update();
    }

    QRectF boundingRect() const override { return bounds_; }
    QPainterPath shape() const override {
        QPainterPathStroker stroker;
        stroker.setWidth(12);
        auto hit = stroker.createStroke(path_);
        if (descriptor.relationship) hit.addRect(cardinality_rect_);
        if (!descriptor.role.isEmpty()) hit.addRect(role_rect_);
        if (isSelected() && shapeable()) hit.addRect(handle_rect_);
        if (isSelected() && shapeable()) for (const auto& corner : corner_rects_) hit.addRect(corner);
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
            if (style == LineStyle::Straight) {
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
            if (style == LineStyle::Straight) path_.lineTo(end);
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
        }
        // A route replaces whatever the automatic shape would have been. Each
        // end aims at the corner nearest it rather than at the far endpoint, so
        // the first and last segments leave and arrive along the route itself.
        if (!descriptor.waypoints.empty() && !inheritance) {
            const auto& corners = descriptor.waypoints;
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
            bend = path_.pointAtPercent(0.5);
            midpoint_ = bend;
            perpendicular_ = normal(end - corners.back());
        }
        // A routed line is shaped by its corners, so it shows a grip on each
        // instead of the single bend grip a plain one carries.
        corner_rects_.clear();
        for (const auto& corner : descriptor.waypoints)
            corner_rects_.push_back(QRectF(corner - QPointF(5, 5), QSizeF(10, 10)));
        handle_rect_ = descriptor.waypoints.empty() ? QRectF(bend - QPointF(5, 5), QSizeF(10, 10)) : QRectF();
        // The padlock sits off to one side of the bend grip rather than on it,
        // so the two controls on a selected link never overlap.
        lock_rect_ = lockable() ? QRectF(bend + perpendicular_ * 16 - QPointF(6, 6), QSizeF(12, 12)) : QRectF();
        const auto entity_direction = bend - end;
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
        const auto role_center = path_.pointAtPercent(0.45)
            + perpendicular_ * clearance(perpendicular_, role_width / 2, 10);
        role_rect_ = QRectF(role_center - QPointF(role_width / 2, 10), QSizeF(role_width, 20));
        bounds_ = path_.boundingRect().adjusted(-14, -14, 14, 14);
        if (descriptor.relationship && !end_label().isEmpty()) bounds_ = bounds_.united(cardinality_rect_);
        if (!descriptor.role.isEmpty()) bounds_ = bounds_.united(role_rect_);
        if (shapeable()) bounds_ = bounds_.united(handle_rect_.adjusted(-2, -2, 2, 2));
        for (const auto& corner : corner_rects_) bounds_ = bounds_.united(corner.adjusted(-2, -2, 2, 2));
        if (lockable()) bounds_ = bounds_.united(lock_rect_.adjusted(-2, -2, 2, 2));
        if (inheritance) {
            setToolTip(inheritance->subtype ? QStringLiteral("Inheritance — subtype")
                                            : QStringLiteral("Inheritance — supertype"));
        } else if (descriptor.relationship) {
            setToolTip(QStringLiteral("Participant: %1 · %2%3").arg(descriptor.cardinality == Cardinality::One ? "One" : "Many",
                descriptor.participation == Participation::Total ? "total participation" : "partial participation",
                descriptor.role.isEmpty() ? QString{} : QStringLiteral(" · ") + descriptor.role));
        } else {
            setToolTip(QStringLiteral("Attribute ownership — select and delete to detach"));
        }
        update();
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);
        const QColor ink = isSelected() ? selection_ : highlighted ? text_ : connector_;
        const qreal weight = isSelected() ? 3.4 : highlighted ? 3.0 : 1.6;
        painter->setPen(QPen(ink, weight));
        painter->setBrush(Qt::NoBrush);
        if (descriptor.relationship && notation == Notation::Chen && descriptor.participation == Participation::Total) {
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
        if (const auto label = end_label(); !label.isEmpty()) draw_label(cardinality_rect_, label);
        if (!descriptor.role.isEmpty()) draw_label(role_rect_, descriptor.role);
        paint_end_symbols(painter, ink, canvas_);
        if (isSelected() && shapeable()) {
            painter->setPen(QPen(selection_, 1.4));
            painter->setBrush(canvas_);
            if (!handle_rect_.isNull()) painter->drawEllipse(handle_rect_);
            for (const auto& corner : corner_rects_) painter->drawEllipse(corner);
        }
        if (isSelected() && lockable()) paint_lock(painter);
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
    QRectF lock_rect_;
    QPointF owner_join_;
    QPointF child_join_;
    QPointF midpoint_;
    QPointF perpendicular_;
    QPointF outward_{-1, 0};
    QPointF end_;
    QColor connector_, selection_, canvas_, text_;
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
    LineStyle style = LineStyle::Curved;
    bool tool_locked = false;
    bool grid = true;
    bool snap = false;
    bool synchronizing = false;
    bool panning = false;
    QPoint pan_start;
    // An editor placed over the node being renamed. It is a viewport child
    // rather than a scene item so it keeps ordinary text-field behaviour, and
    // it is repositioned whenever the view scrolls or zooms.
    QLineEdit* inline_editor = nullptr;
    std::optional<ElementRef> renaming;
    std::optional<ElementRef> connect_start;
    // While a connection is being made the pointer carries a preview line, so
    // the same gesture works as click-then-click or as one press-drag-release.
    std::optional<QPointF> connect_pointer;
    std::optional<ElementRef> connect_hover;
    std::map<ElementRef, Rect> drag_start;
    QPointF drag_anchor;
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
    QPointF minimum_drag;
    QPointF maximum_drag;
    std::optional<std::uint64_t> displayed_revision;

    Impl(DiagramView& owner, application::Editor& controller)
        : view(owner), editor(controller), scene(new QGraphicsScene(&owner)) {}

    void status(const QString& text) const { if (view.on_status) view.on_status(text); }
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
        inline_editor->hide();
        if (!exists(editor.project(), element) || value == name(editor.project(), element)) return;
        publish(editor.rename(element, value));
    }
    void cancel_inline_edit() {
        if (!renaming) return;
        renaming.reset();
        if (inline_editor) inline_editor->hide();
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
    void connect_node(NodeItem* node) {
        if (!node) {
            // Abandoning a half-made connection spends the tool's one use just
            // as completing it does. Otherwise Connect stays armed after the
            // gesture the user gave up on, and their next click on an element
            // starts another connection when they meant to select it. A click
            // on empty canvas with nothing yet armed is only a miss, so it
            // leaves the tool alone.
            const bool abandoned = connect_start.has_value();
            connect_start.reset();
            connect_pointer.reset();
            if (abandoned && !tool_locked) { view.set_tool(Tool::Select); return; }
            status(QStringLiteral("Connection cancelled. Select the first object."));
            return;
        }
        if (!exists(editor.project(), node->ref)
            || (connect_start && !exists(editor.project(), *connect_start))) {
            connect_start.reset();
            connect_pointer.reset();
            view.synchronize();
            status(QStringLiteral("The selected object was removed. Select the first object again."));
            return;
        }
        if (!connect_start) {
            connect_start = node->ref;
            view.select_elements({node->ref});
            status(QStringLiteral("Select an entity and relationship, or an attribute and its owner."));
            return;
        }
        const auto from = *connect_start;
        const auto to = node->ref;
        connect_start.reset();
        connect_pointer.reset();
        application::EditResult result{false, "Connect an entity to a relationship, or an attribute to its owner.", {}, {}};
        if (from == to) {
            result.error = "Select two different objects. For a recursive relationship, connect the same entity to its relationship twice.";
        } else if (const auto plan = plan_connection(from, to)) {
            result = (*plan)();
        }
        // The attempt is the tool's one use whether or not the pair was legal,
        // so an unlocked Connect hands back to Select the way every other tool
        // does. Handing back first matters: the new tool announces itself, and
        // doing it afterwards would overwrite the reason a pair was refused.
        if (!tool_locked) view.set_tool(Tool::Select);
        publish(result);
        if (result && tool_locked)
            status(QStringLiteral("Connected. Locked: select the first object of the next connection."));
    }
    // The edit a pair of elements would produce, or nothing when the pair means
    // nothing. The hover highlight and the committed connection read the same
    // rule here, so what the pointer promises is what the release performs.
    [[nodiscard]] std::optional<std::function<application::EditResult()>>
    plan_connection(const ElementRef& from, const ElementRef& to) const {
        const auto& project = editor.project();
        if (from == to || !exists(project, from) || !exists(project, to)) return {};
        if (const auto* relationship = std::get_if<RelationshipId>(&from); relationship && std::holds_alternative<EntityId>(to)) {
            return [this, id = *relationship, entity = std::get<EntityId>(to)] { return editor.connect(id, entity); };
        }
        if (const auto* relationship = std::get_if<RelationshipId>(&to); relationship && std::holds_alternative<EntityId>(from)) {
            return [this, id = *relationship, entity = std::get<EntityId>(from)] { return editor.connect(id, entity); };
        }
        // An associative relationship acts as an entity, so it may join another
        // relationship. The plain one of the pair is the one that gains a participant.
        if (std::holds_alternative<RelationshipId>(from) && std::holds_alternative<RelationshipId>(to)) {
            const auto first = std::get<RelationshipId>(from);
            const auto second = std::get<RelationshipId>(to);
            if (project.relationships.at(first).associative && !project.relationships.at(second).associative)
                return [this, id = second, target = first] { return editor.connect(id, ParticipantTarget{target}); };
            if (project.relationships.at(second).associative && !project.relationships.at(first).associative)
                return [this, id = first, target = second] { return editor.connect(id, ParticipantTarget{target}); };
            return {};
        }
        // The first entity connected to a triangle is what it generalises; every
        // one after that is a subtype. Properties can change either afterwards.
        const auto isa_link = [this, &project](SpecializationId id, EntityId entity)
            -> std::optional<std::function<application::EditResult()>> {
            const auto found = project.specializations.find(id);
            if (found == project.specializations.end()) return {};
            if (!found->second.supertype)
                return [this, id, entity] { return editor.set_supertype(id, entity); };
            return [this, id, entity] { return editor.attach_subtype(id, entity); };
        };
        if (const auto* isa = std::get_if<SpecializationId>(&from); isa && std::holds_alternative<EntityId>(to))
            return isa_link(*isa, std::get<EntityId>(to));
        if (const auto* isa = std::get_if<SpecializationId>(&to); isa && std::holds_alternative<EntityId>(from))
            return isa_link(*isa, std::get<EntityId>(from));
        if (std::holds_alternative<SpecializationId>(from) || std::holds_alternative<SpecializationId>(to)) return {};
        if (std::holds_alternative<AttributeId>(from) && std::holds_alternative<AttributeId>(to)
            && project.attributes.at(std::get<AttributeId>(from)).kind == AttributeKind::Composite
            && project.attributes.at(std::get<AttributeId>(to)).kind != AttributeKind::Composite) {
            return [this, id = std::get<AttributeId>(to), owner = from] { return editor.set_attribute_owner(id, owner); };
        }
        if (const auto* attribute = std::get_if<AttributeId>(&from)) {
            return [this, id = *attribute, owner = to] { return editor.set_attribute_owner(id, owner); };
        }
        if (const auto* attribute = std::get_if<AttributeId>(&to)) {
            return [this, id = *attribute, owner = from] { return editor.set_attribute_owner(id, owner); };
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
            desired_edges.emplace(id, EdgeDescription{id, id, *attribute.owner, {}, Cardinality::Many,
                                                      Participation::Partial, {}, shape.offset,
                                                      shape.owner_anchor, shape.child_anchor,
                                                      route_of(shape)});
        }
    }
    for (const auto& [id, relationship] : project.relationships) {
        std::map<ElementRef, std::size_t> counts;
        std::map<ElementRef, std::size_t> index;
        for (const auto& participant : relationship.participants) ++counts[target_ref(participant.target)];
        for (const auto& participant : relationship.participants) {
            if (!exists(project, target_ref(participant.target))) continue;
            const auto ordinal = index[target_ref(participant.target)]++;
            const auto offset = (static_cast<qreal>(ordinal) - (static_cast<qreal>(counts[target_ref(participant.target)]) - 1) / 2) * 48;
            const auto shape = shaped(participant.id, offset);
            desired_edges.emplace(participant.id, EdgeDescription{participant.id, id, target_ref(participant.target), id,
                participant.maximum, participant.participation, QString::fromStdString(participant.role),
                shape.offset, shape.owner_anchor, shape.child_anchor, route_of(shape)});
        }
    }
    for (const auto& [id, specialization] : project.specializations) {
        if (specialization.supertype && project.entities.contains(*specialization.supertype))
            desired_edges.emplace(EdgeKey{InheritanceKey{id, {}}},
                EdgeDescription{EdgeKey{InheritanceKey{id, {}}}, id, ElementRef{*specialization.supertype}, {},
                                Cardinality::Many, Participation::Partial, {}, 0});
        for (const auto& subtype : specialization.subtypes) {
            if (!project.entities.contains(subtype)) continue;
            const EdgeKey key{InheritanceKey{id, subtype}};
            desired_edges.emplace(key, EdgeDescription{key, id, ElementRef{subtype}, {},
                                                      Cardinality::Many, Participation::Partial, {}, 0});
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
                    if (impl_->snap) {
                        const auto anchor = impl_->drag_anchor + delta;
                        delta = QPointF(std::round(anchor.x() / grid_spacing) * grid_spacing,
                                        std::round(anchor.y() / grid_spacing) * grid_spacing) - impl_->drag_anchor;
                    }
                    // All selected nodes use one translation, preserving their
                    // relative spacing even when snapping or reaching a boundary.
                    delta.setX(std::clamp(delta.x(), impl_->minimum_drag.x(), impl_->maximum_drag.x()));
                    delta.setY(std::clamp(delta.y(), impl_->minimum_drag.y(), impl_->maximum_drag.y()));
                    return start + delta;
                }
                if (impl_->snap) position = {std::round(position.x() / grid_spacing) * grid_spacing, std::round(position.y() / grid_spacing) * grid_spacing};
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
        if (const auto* relationship_id = std::get_if<RelationshipId>(&ref))
            associative = project.relationships.at(*relationship_id).associative;
        bool generalising = false;
        if (const auto* specialization_id = std::get_if<SpecializationId>(&ref))
            generalising = project.specializations.at(*specialization_id).direction == Inheritance::Generalization;
        if (node->label != label || node->attribute_kind != kind || node->associative != associative
            || node->generalising != generalising) {
            node->label = label;
            node->attribute_kind = kind;
            node->generalising = generalising;
            node->set_associative(associative);
            node->update();
        }
        node->setToolTip(label);
        const auto chosen = project.colours.find(ref);
        node->set_chosen_colour(chosen == project.colours.end()
            ? std::optional<QColor>{}
            : std::optional{QColor(chosen->second.red, chosen->second.green, chosen->second.blue)});
        if (geometry_changed) {
            const auto incident = impl_->incident.find(ref);
            if (incident != impl_->incident.end()) dirty_edges.insert(incident->second.begin(), incident->second.end());
        }
    };
    for (const auto& [id, entity] : project.entities) { (void)entity; sync_node(id); }
    for (const auto& [id, attribute] : project.attributes) { (void)attribute; sync_node(id); }
    for (const auto& [id, relationship] : project.relationships) { (void)relationship; sync_node(id); }
    for (const auto& [id, specialization] : project.specializations) { (void)specialization; sync_node(id); }
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
    impl_->displayed_revision = impl_->editor.revision();
    impl_->synchronizing = false;
    if (impl_->connect_start && !exists(project, *impl_->connect_start)) {
        impl_->connect_start.reset();
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
    case Tool::Connect: impl_->status(QStringLiteral("Select an entity and relationship, an attribute and its owner, or a subtype and its triangle.")); break;
    case Tool::Pan: impl_->status(QStringLiteral("Drag to pan. The middle mouse button pans in every tool.")); break;
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

QPixmap DiagramView::line_style_preview(LineStyle style, QSize size) const {
    const auto& colors = theme(impl_->theme_id);
    QPixmap pixmap(size * devicePixelRatioF());
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    QPen pen(colors.connector, 1.6);
    pen.setCosmetic(true);
    painter.setPen(pen);
    painter.setBrush(Qt::NoBrush);
    const QPointF from(4, size.height() - 4.0);
    const QPointF to(size.width() - 4.0, 4);
    if (style == LineStyle::Straight) {
        painter.drawLine(from, to);
    } else {
        QPainterPath path(from);
        path.cubicTo(from + QPointF(size.width() * 0.45, 0), to - QPointF(size.width() * 0.45, 0), to);
        painter.drawPath(path);
    }
    return pixmap;
}

QPixmap DiagramView::notation_preview(Notation notation, QSize size) const {
    const auto& colors = theme(impl_->theme_id);
    QPixmap pixmap(size * devicePixelRatioF());
    pixmap.setDevicePixelRatio(devicePixelRatioF());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    const qreal middle = size.height() / 2.0;
    // A mandatory "many" end exercises both symbols in every notation.
    const QPointF end(size.width() - 6.0, middle);
    QPen line(colors.connector, 1.6);
    line.setCosmetic(true);
    painter.setPen(line);
    painter.drawLine(QPointF(4, middle), end);
    draw_participant_end(&painter, notation, true, true, end, QPointF(-1, 0), colors.connector, colors.canvas);
    if (notation == Notation::Chen || notation == Notation::MinMax) {
        auto font = painter.font();
        font.setPointSizeF(9);
        painter.setFont(font);
        painter.setPen(colors.node_text);
        painter.drawText(QRectF(0, 0, size.width() - 8, size.height()), Qt::AlignRight | Qt::AlignVCenter,
                         notation == Notation::Chen ? QStringLiteral("M") : QStringLiteral("(1,M)"));
    }
    return pixmap;
}
void DiagramView::set_grid_visible(bool enabled) { impl_->grid = enabled; viewport()->update(); }
void DiagramView::set_snap_enabled(bool enabled) { impl_->snap = enabled; }
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
    for (auto* item : impl_->scene->selectedItems()) {
        if (const auto* edge = dynamic_cast<EdgeItem*>(item)) {
            if (edge->descriptor.relationship)
                participants.emplace_back(*edge->descriptor.relationship, std::get<ParticipantId>(edge->descriptor.key));
            else detached_attributes.push_back(std::get<AttributeId>(edge->descriptor.key));
        }
    }
    if (!elements.empty() || !participants.empty() || !detached_attributes.empty())
        impl_->publish(impl_->editor.erase(elements, participants, detached_attributes));
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
    if (impl_->bending) {
        // Discard the previewed bend; the next projection restores the stored one.
        impl_->bending.reset();
        impl_->displayed_revision.reset();
        synchronize();
    }
    impl_->cancel_inline_edit();
    impl_->connect_start.reset();
    impl_->connect_pointer.reset();
    impl_->connect_hover.reset();
    impl_->panning = false;
    setCursor(impl_->active_tool == Tool::Pan ? Qt::OpenHandCursor : impl_->active_tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
}

void DiagramView::drawBackground(QPainter* painter, const QRectF& rect) {
    const auto& colors = theme(impl_->theme_id);
    painter->fillRect(rect, colors.canvas);
    if (!impl_->grid) return;
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
    if (!node) { QGraphicsView::contextMenuEvent(event); return; }
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

    auto* picked = menu.exec(event->globalPos());
    if (!picked) return;
    if (picked == duplicate) { impl_->publish(impl_->editor.duplicate(chosen)); return; }
    if (picked == remove) { delete_selection(); return; }
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
        impl_->connect_node(impl_->node_at(event->position().toPoint()));
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
    if (impl_->active_tool == Tool::Entity || impl_->active_tool == Tool::Attribute || impl_->active_tool == Tool::Relationship) {
        auto center = mapToScene(event->position().toPoint());
        if (impl_->snap) center = {std::round(center.x() / grid_spacing) * grid_spacing, std::round(center.y() / grid_spacing) * grid_spacing};
        application::EditResult result;
        if (impl_->active_tool == Tool::Entity) {
            result = impl_->editor.create_entity("Entity", centred(center, entity_body));
        } else if (impl_->active_tool == Tool::Relationship) {
            result = impl_->editor.create_relationship("Relationship", centred(center, relationship_body));
        } else {
            std::optional<AttributeOwner> owner;
            const auto selection = selected_elements();
            if (selection.size() == 1 && exists(impl_->editor.project(), selection.front())) {
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
        const auto scene_press = mapToScene(event->position().toPoint());
        if (auto* edge = impl_->selected_edge_at(event->position().toPoint())) {
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
    painter->drawLine(source->second->boundary_toward(pointer), pointer);
    if (valid) {
        const auto target = impl_->nodes.find(*impl_->connect_hover);
        if (target != impl_->nodes.end()) {
            QPen outline(colors.accent, 2.0);
            outline.setCosmetic(true);
            painter->setPen(outline);
            painter->drawRoundedRect(target->second->sceneBoundingRect().adjusted(-4, -4, 4, 4), 6, 6);
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
    QGraphicsView::mouseMoveEvent(event);
}
void DiagramView::mouseReleaseEvent(QMouseEvent* event) {
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
        if (released_over && released_over->ref != *impl_->connect_start) impl_->connect_node(released_over);
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
void DiagramView::wheelEvent(QWheelEvent* event) {
    const auto delta = event->angleDelta().y();
    if (delta != 0) impl_->zoom(zoom_factor() * std::pow(1.0015, static_cast<qreal>(delta)));
    else if (!event->pixelDelta().isNull()) {
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - event->pixelDelta().x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - event->pixelDelta().y());
    }
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
