#include "diagram_view.hpp"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPathStroker>
#include <QScrollBar>
#include <QStyleOptionGraphicsItem>
#include <QWheelEvent>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace erdflow::desktop {
namespace {
using namespace domain;

constexpr qreal grid_spacing = 20;
constexpr qreal minimum_zoom = 0.15;
constexpr qreal maximum_zoom = 3.0;
const QColor canvas_color{19, 25, 34};
const QColor selection_color{91, 211, 225};

QPointF normal(const QPointF& delta) {
    const auto length = std::hypot(delta.x(), delta.y());
    return length > 0.001 ? QPointF(-delta.y() / length, delta.x() / length) : QPointF(0, 1);
}

// These items are projections only: all persistent changes go through Editor.
class NodeItem final : public QGraphicsItem {
public:
    explicit NodeItem(ElementRef reference) : ref(std::move(reference)) {
        setFlags(ItemIsSelectable | ItemIsMovable | ItemSendsGeometryChanges);
        setZValue(1);
    }

    ElementRef ref;
    QString label;
    AttributeKind attribute_kind = AttributeKind::Normal;
    std::function<void(NodeItem*)> moved;
    std::function<QPointF(QPointF)> constrain;

    QRectF boundingRect() const override { return bounds_.adjusted(-4, -4, 4, 4); }
    QRectF body_rect() const { return bounds_; }
    QPainterPath shape() const override {
        QPainterPath path;
        if (std::holds_alternative<EntityId>(ref)) {
            path.addRect(bounds_);
        } else if (std::holds_alternative<AttributeId>(ref)) {
            path.addEllipse(bounds_);
        } else {
            path.moveTo(bounds_.center().x(), bounds_.top());
            path.lineTo(bounds_.right(), bounds_.center().y());
            path.lineTo(bounds_.center().x(), bounds_.bottom());
            path.lineTo(bounds_.left(), bounds_.center().y());
            path.closeSubpath();
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
        } else if (std::holds_alternative<RelationshipId>(ref)) {
            divisor = std::abs(delta.x()) / rx + std::abs(delta.y()) / ry;
        } else {
            divisor = std::max(std::abs(delta.x()) / rx, std::abs(delta.y()) / ry);
        }
        return center + delta / divisor;
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);
        QColor fill{37, 46, 61};
        QColor border{137, 154, 175};
        if (std::holds_alternative<EntityId>(ref)) { fill = QColor(34, 54, 76); border = QColor(115, 160, 195); }
        if (std::holds_alternative<RelationshipId>(ref)) { fill = QColor(29, 62, 64); border = QColor(97, 167, 161); }
        QPen pen(isSelected() ? selection_color : border, isSelected() ? 2.4 : 1.6);
        if (std::holds_alternative<AttributeId>(ref) && attribute_kind == AttributeKind::Derived)
            pen.setStyle(Qt::DashLine);
        painter->setPen(pen);
        painter->setBrush(fill);
        painter->drawPath(shape());
        if (std::holds_alternative<AttributeId>(ref) && attribute_kind == AttributeKind::Multivalued)
            painter->drawEllipse(bounds_.adjusted(5, 5, -5, -5));
        auto font = painter->font();
        font.setPointSizeF(11);
        font.setWeight(std::holds_alternative<EntityId>(ref) ? QFont::DemiBold : QFont::Normal);
        font.setUnderline(std::holds_alternative<AttributeId>(ref) && attribute_kind == AttributeKind::Key);
        painter->setFont(font);
        painter->setPen(QColor(233, 239, 246));
        const auto inset = std::holds_alternative<RelationshipId>(ref) ? bounds_.width() * 0.22 : 15.0;
        const auto text_rect = bounds_.adjusted(inset, 8, -inset, -8);
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
};

using EdgeKey = std::variant<AttributeId, ParticipantId>;
struct EdgeDescription {
    EdgeKey key;
    ElementRef from;
    ElementRef to;
    std::optional<RelationshipId> relationship;
    Cardinality cardinality = Cardinality::Many;
    Participation participation = Participation::Partial;
    QString role;
    qreal offset = 0;
    bool operator==(const EdgeDescription&) const = default;
};

class EdgeItem final : public QGraphicsItem {
public:
    EdgeItem(EdgeDescription description, NodeItem* from, NodeItem* to)
        : descriptor(std::move(description)), source(from), target(to) {
        setFlag(ItemIsSelectable);
        setZValue(-1);
        refresh();
    }
    EdgeDescription descriptor;
    NodeItem* source;
    NodeItem* target;

    QRectF boundingRect() const override { return bounds_; }
    QPainterPath shape() const override {
        QPainterPathStroker stroker;
        stroker.setWidth(12);
        auto hit = stroker.createStroke(path_);
        if (descriptor.relationship) hit.addRect(cardinality_rect_);
        if (!descriptor.role.isEmpty()) hit.addRect(role_rect_);
        return hit;
    }
    void refresh() {
        prepareGeometryChange();
        const auto first = source->scenePos() + source->body_rect().center();
        const auto last = target->scenePos() + target->body_rect().center();
        perpendicular_ = normal(last - first);
        auto bend = (first + last) / 2 + perpendicular_ * descriptor.offset;
        if (std::hypot(last.x() - first.x(), last.y() - first.y()) < 1)
            bend = first + QPointF(80, -80);
        const auto start = source->boundary_toward(bend);
        const auto end = target->boundary_toward(bend);
        path_ = QPainterPath(start);
        path_.lineTo(bend);
        path_.lineTo(end);
        const auto entity_direction = bend - end;
        const auto distance = std::hypot(entity_direction.x(), entity_direction.y());
        const auto label_center = end + (distance > 0.01 ? entity_direction * (20 / distance) : QPointF(-20, 0))
            + normal(entity_direction) * 13;
        cardinality_rect_ = QRectF(label_center - QPointF(11, 10), QSizeF(22, 20));
        const auto role_center = path_.pointAtPercent(0.45) + perpendicular_ * 15;
        role_rect_ = QRectF(role_center - QPointF(70, 10), QSizeF(140, 20));
        bounds_ = path_.boundingRect().adjusted(-10, -10, 10, 10);
        if (descriptor.relationship) bounds_ = bounds_.united(cardinality_rect_);
        if (!descriptor.role.isEmpty()) bounds_ = bounds_.united(role_rect_);
        setToolTip(descriptor.relationship
            ? QStringLiteral("Participant: %1 · %2%3").arg(descriptor.cardinality == Cardinality::One ? "One" : "Many",
                descriptor.participation == Participation::Total ? "total participation" : "partial participation",
                descriptor.role.isEmpty() ? QString{} : QStringLiteral(" · ") + descriptor.role)
            : QStringLiteral("Attribute ownership — select and delete to detach"));
        update();
    }
    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing);
        painter->setPen(QPen(isSelected() ? selection_color : QColor(139, 157, 177), isSelected() ? 2.2 : 1.6));
        painter->setBrush(Qt::NoBrush);
        if (descriptor.relationship && descriptor.participation == Participation::Total) {
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
            painter->setBrush(canvas_color);
            painter->drawRoundedRect(rect, 3, 3);
            painter->setPen(isSelected() ? selection_color : QColor(208, 220, 232));
            painter->drawText(rect, Qt::AlignCenter, QFontMetricsF(font).elidedText(text, Qt::ElideRight, rect.width() - 6));
        };
        if (descriptor.relationship)
            draw_label(cardinality_rect_, descriptor.cardinality == Cardinality::One ? QStringLiteral("1") : QStringLiteral("M"));
        if (!descriptor.role.isEmpty()) draw_label(role_rect_, descriptor.role);
    }
private:
    QPainterPath path_;
    QRectF bounds_;
    QRectF cardinality_rect_;
    QRectF role_rect_;
    QPointF perpendicular_;
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
    bool grid = true;
    bool snap = false;
    bool synchronizing = false;
    bool panning = false;
    QPoint pan_start;
    std::optional<ElementRef> connect_start;
    std::map<ElementRef, Rect> drag_start;
    QPointF drag_anchor;
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
    void selection_changed() const {
        if (!synchronizing && view.on_selection) view.on_selection(view.selected_elements());
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
    void remove_edge(std::map<EdgeKey, EdgeItem*>::iterator& iterator) {
        auto* edge = iterator->second;
        incident[edge->descriptor.from].erase(edge);
        incident[edge->descriptor.to].erase(edge);
        scene->removeItem(edge);
        delete edge;
        iterator = edges.erase(iterator);
    }
    void zoom(qreal requested) {
        const auto bounded = std::clamp(requested, minimum_zoom, maximum_zoom);
        view.scale(bounded / view.zoom_factor(), bounded / view.zoom_factor());
        if (view.on_zoom) view.on_zoom(view.zoom_factor());
    }
    void connect_node(NodeItem* node) {
        if (!node) { connect_start.reset(); status(QStringLiteral("Connection cancelled. Select the first object.")); return; }
        if (!exists(editor.project(), node->ref)
            || (connect_start && !exists(editor.project(), *connect_start))) {
            connect_start.reset();
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
        application::EditResult result{false, "Connect an entity to a relationship, or an attribute to its owner.", {}, {}};
        if (from == to) {
            result.error = "Select two different objects. For a recursive relationship, connect the same entity to its relationship twice.";
        } else if (auto* relationship = std::get_if<RelationshipId>(&from); relationship && std::holds_alternative<EntityId>(to)) {
            result = editor.connect(*relationship, std::get<EntityId>(to));
        } else if (auto* target_relationship = std::get_if<RelationshipId>(&to); target_relationship && std::holds_alternative<EntityId>(from)) {
            result = editor.connect(*target_relationship, std::get<EntityId>(from));
        } else if (std::holds_alternative<AttributeId>(from) && std::holds_alternative<AttributeId>(to)
                   && editor.project().attributes.at(std::get<AttributeId>(from)).kind == AttributeKind::Composite
                   && editor.project().attributes.at(std::get<AttributeId>(to)).kind != AttributeKind::Composite) {
            result = editor.set_attribute_owner(std::get<AttributeId>(to), from);
        } else if (const auto* attribute = std::get_if<AttributeId>(&from)) {
            result = editor.set_attribute_owner(*attribute, to);
        } else if (const auto* target_attribute = std::get_if<AttributeId>(&to)) {
            result = editor.set_attribute_owner(*target_attribute, from);
        }
        publish(result);
        if (result) status(QStringLiteral("Connected. Select the first object to make another connection."));
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
    setBackgroundBrush(canvas_color);
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
    for (const auto& [id, attribute] : project.attributes) {
        if (attribute.owner && exists(project, *attribute.owner))
            desired_edges.emplace(id, EdgeDescription{id, id, *attribute.owner, {}, Cardinality::Many, Participation::Partial, {}, 0});
    }
    for (const auto& [id, relationship] : project.relationships) {
        std::map<EntityId, std::size_t> counts;
        std::map<EntityId, std::size_t> index;
        for (const auto& participant : relationship.participants) ++counts[participant.entity];
        for (const auto& participant : relationship.participants) {
            if (!project.entities.contains(participant.entity)) continue;
            const auto ordinal = index[participant.entity]++;
            const auto offset = (static_cast<qreal>(ordinal) - (static_cast<qreal>(counts[participant.entity]) - 1) / 2) * 48;
            desired_edges.emplace(participant.id, EdgeDescription{participant.id, id, participant.entity, id,
                participant.maximum, participant.participation, QString::fromStdString(participant.role), offset});
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
            impl_->scene->removeItem(it->second);
            delete it->second;
            it = impl_->nodes.erase(it);
        } else ++it;
    }
    std::set<EdgeItem*> dirty_edges;
    const auto sync_node = [&](const ElementRef& ref) {
        auto [iterator, created] = impl_->nodes.try_emplace(ref, nullptr);
        auto*& node = iterator->second;
        if (created) {
            node = new NodeItem(ref);
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
        if (node->label != label || node->attribute_kind != kind) {
            node->label = label;
            node->attribute_kind = kind;
            node->update();
        }
        node->setToolTip(label);
        if (geometry_changed) {
            const auto incident = impl_->incident.find(ref);
            if (incident != impl_->incident.end()) dirty_edges.insert(incident->second.begin(), incident->second.end());
        }
    };
    for (const auto& [id, entity] : project.entities) { (void)entity; sync_node(id); }
    for (const auto& [id, attribute] : project.attributes) { (void)attribute; sync_node(id); }
    for (const auto& [id, relationship] : project.relationships) { (void)relationship; sync_node(id); }
    for (const auto& [key, description] : desired_edges) {
        auto found = impl_->edges.find(key);
        if (found == impl_->edges.end()) {
            auto* edge = new EdgeItem(description, impl_->nodes.at(description.from), impl_->nodes.at(description.to));
            impl_->edges.emplace(key, edge);
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
    impl_->displayed_revision = impl_->editor.revision();
    impl_->synchronizing = false;
    if (impl_->connect_start && !exists(project, *impl_->connect_start)) impl_->connect_start.reset();
    impl_->selection_changed();
}

void DiagramView::set_tool(Tool tool) {
    cancel_interaction();
    impl_->active_tool = tool;
    setDragMode(tool == Tool::Select ? RubberBandDrag : NoDrag);
    setCursor(tool == Tool::Pan ? Qt::OpenHandCursor : tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
    if (on_tool) on_tool(tool);
    switch (tool) {
    case Tool::Select: impl_->status(QStringLiteral("Select objects to edit. Drag to move; Shift-click to extend selection.")); break;
    case Tool::Entity: impl_->status(QStringLiteral("Click the canvas to create an entity.")); break;
    case Tool::Attribute: impl_->status(QStringLiteral("Click to add an attribute to the selected owner, or an unattached attribute.")); break;
    case Tool::Relationship: impl_->status(QStringLiteral("Click the canvas to create a relationship, then use Connect to add participants.")); break;
    case Tool::Connect: impl_->status(QStringLiteral("Select an entity and relationship, or an attribute and its owner.")); break;
    case Tool::Pan: impl_->status(QStringLiteral("Drag to pan. The middle mouse button pans in every tool.")); break;
    }
}
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
void DiagramView::set_grid_visible(bool enabled) { impl_->grid = enabled; viewport()->update(); }
void DiagramView::set_snap_enabled(bool enabled) { impl_->snap = enabled; }
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
    impl_->connect_start.reset();
    impl_->panning = false;
    setCursor(impl_->active_tool == Tool::Pan ? Qt::OpenHandCursor : impl_->active_tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
}

void DiagramView::drawBackground(QPainter* painter, const QRectF& rect) {
    painter->fillRect(rect, canvas_color);
    if (!impl_->grid) return;
    // Keep the grid sparse when zoomed out; its iteration cost stays viewport-bound.
    const qreal step = zoom_factor() < 0.4 ? 100 : grid_spacing;
    const auto left = std::floor(rect.left() / step) * step;
    const auto top = std::floor(rect.top() / step) * step;
    QPen pen(QColor(51, 62, 76));
    pen.setCosmetic(true);
    painter->setPen(pen);
    for (qreal x = left; x <= rect.right(); x += step)
        for (qreal y = top; y <= rect.bottom(); y += step) painter->drawPoint(QPointF(x, y));
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
        event->accept();
        return;
    }
    if (impl_->active_tool == Tool::Entity || impl_->active_tool == Tool::Attribute || impl_->active_tool == Tool::Relationship) {
        auto center = mapToScene(event->position().toPoint());
        if (impl_->snap) center = {std::round(center.x() / grid_spacing) * grid_spacing, std::round(center.y() / grid_spacing) * grid_spacing};
        application::EditResult result;
        if (impl_->active_tool == Tool::Entity) {
            result = impl_->editor.create_entity("Entity", {center.x() - 80, center.y() - 40, 160, 80});
        } else if (impl_->active_tool == Tool::Relationship) {
            result = impl_->editor.create_relationship("Relationship", {center.x() - 95, center.y() - 55, 190, 110});
        } else {
            std::optional<AttributeOwner> owner;
            const auto selection = selected_elements();
            if (selection.size() == 1 && exists(impl_->editor.project(), selection.front())) {
                const auto* attribute = std::get_if<AttributeId>(&selection.front());
                if (!attribute || impl_->editor.project().attributes.at(*attribute).kind == AttributeKind::Composite) owner = selection.front();
            }
            result = impl_->editor.create_attribute("Attribute", {center.x() - 75, center.y() - 30, 150, 60}, owner);
        }
        impl_->publish(result);
        if (result && result.created) {
            select_elements({*result.created});
            set_tool(Tool::Select);
        }
        event->accept();
        return;
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
void DiagramView::mouseMoveEvent(QMouseEvent* event) {
    if (impl_->panning) {
        const auto delta = event->position().toPoint() - impl_->pan_start;
        horizontalScrollBar()->setValue(horizontalScrollBar()->value() - delta.x());
        verticalScrollBar()->setValue(verticalScrollBar()->value() - delta.y());
        impl_->pan_start = event->position().toPoint();
        event->accept();
        return;
    }
    QGraphicsView::mouseMoveEvent(event);
}
void DiagramView::mouseReleaseEvent(QMouseEvent* event) {
    if (impl_->panning && (event->button() == Qt::MiddleButton || event->button() == Qt::LeftButton)) {
        impl_->panning = false;
        setCursor(impl_->active_tool == Tool::Pan ? Qt::OpenHandCursor : impl_->active_tool == Tool::Select ? Qt::ArrowCursor : Qt::CrossCursor);
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
