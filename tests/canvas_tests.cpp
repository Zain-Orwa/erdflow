#include "app/desktop/diagram_view.hpp"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QKeyEvent>
#include <QMouseEvent>

#include <cmath>
#include <iostream>
#include <stdexcept>

namespace {
using namespace erdflow;

class SequentialIds final : public application::IdGenerator {
public:
    domain::Uuid next() override {
        domain::Uuid id;
        id.bytes[6] = 0x70;
        id.bytes[8] = 0x80;
        id.bytes[14] = static_cast<std::uint8_t>(counter_ >> 8);
        id.bytes[15] = static_cast<std::uint8_t>(counter_++);
        return id;
    }
private:
    unsigned counter_ = 1;
};
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void require(const application::EditResult& result, const char* message) { require(result.ok, message); }
QGraphicsItem* find_node(desktop::DiagramView& view, const QString& name) {
    for (auto* item : view.scene()->items())
        if (item->zValue() > 0 && item->toolTip() == name) return item;
    throw std::runtime_error("Missing node");
}
QGraphicsItem* find_edge(desktop::DiagramView& view, const QString& text = QStringLiteral("Participant:")) {
    for (auto* item : view.scene()->items())
        if (item->zValue() < 0 && item->toolTip().startsWith(text)) return item;
    throw std::runtime_error("Missing edge");
}
void mouse(desktop::DiagramView& view, QEvent::Type type, QPoint position, Qt::MouseButton button,
           Qt::MouseButtons buttons, Qt::KeyboardModifiers modifiers = Qt::NoModifier) {
    const QPoint global = view.viewport()->mapToGlobal(position);
    QMouseEvent event(type, QPointF(position), QPointF(global), button, buttons, modifiers);
    QApplication::sendEvent(view.viewport(), &event);
    QApplication::processEvents();
}
void click(desktop::DiagramView& view, const QPointF& point) {
    const auto position = view.mapFromScene(point);
    mouse(view, QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton);
}
void key(desktop::DiagramView& view, int code) {
    QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier);
    QApplication::sendEvent(&view, &event);
}

void group_movement_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto first = *editor.create_entity("First", {13, 17, 160, 80}).created;
    const auto second = *editor.create_entity("Second", {246, 61, 160, 80}).created;
    desktop::DiagramView view(editor);
    view.resize(900, 500);
    view.show();
    view.centerOn(250, 100);
    QApplication::processEvents();
    auto* first_item = find_node(view, "First");
    auto* second_item = find_node(view, "Second");
    view.select_elements({first});
    const auto second_center = view.mapFromScene(second_item->sceneBoundingRect().center());
    mouse(view, QEvent::MouseButtonPress, second_center, Qt::LeftButton, Qt::LeftButton, Qt::ShiftModifier);
    mouse(view, QEvent::MouseButtonRelease, second_center, Qt::LeftButton, Qt::NoButton, Qt::ShiftModifier);
    require(view.selected_elements().size() == 2, "Shift-click extends selection");
    view.set_snap_enabled(true);
    const auto spacing = second_item->pos() - first_item->pos();
    const auto revision = editor.revision();
    const auto start = view.mapFromScene(first_item->sceneBoundingRect().center());
    mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, start + QPoint(47, 32), Qt::NoButton, Qt::LeftButton);
    require(second_item->pos() - first_item->pos() == spacing, "Snap preserves relative group spacing");
    require(std::fmod(first_item->pos().x(), 20) == 0 && std::fmod(first_item->pos().y(), 20) == 0, "Drag anchor snaps to grid");
    mouse(view, QEvent::MouseButtonRelease, start + QPoint(47, 32), Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == revision + 1, "Group drag commits once");
    require(editor.undo(), "Undo group drag");
    view.synchronize();
    require(first_item->pos() == QPointF(13, 17) && second_item->pos() == QPointF(246, 61), "One undo restores both group members");

    view.set_snap_enabled(false);
    require(editor.move({{first, {99800, 0, 80, 40}}, {second, {99920, 70, 60, 40}}}), "Boundary fixture");
    view.synchronize();
    view.centerOn(99880, 60);
    const auto boundary_spacing = second_item->pos() - first_item->pos();
    const auto boundary_start = view.mapFromScene(first_item->sceneBoundingRect().center());
    const auto boundary_revision = editor.revision();
    mouse(view, QEvent::MouseButtonPress, boundary_start, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, boundary_start + QPoint(100, 30), Qt::NoButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, boundary_start + QPoint(100, 30), Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == boundary_revision + 1, "Boundary drag is clamped rather than rejected");
    require(second_item->pos() - first_item->pos() == boundary_spacing, "Boundary clamp preserves group spacing");
    require(second_item->pos().x() == 99940, "Boundary includes full node width");
}

void synchronization_lifetime_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto entity = *editor.create_entity("Owner", {0, 0, 160, 80}).created;
    const auto related = *editor.create_relationship("Related", {300, 0, 190, 110}).created;
    const auto relationship = std::get<domain::RelationshipId>(related);
    require(editor.connect(relationship, std::get<domain::EntityId>(entity)), "Lifetime participant fixture");
    require(editor.create_attribute("Child", {0, -150, 150, 60}, entity), "Lifetime attribute fixture");
    desktop::DiagramView view(editor);
    view.resize(900, 600);
    view.show();
    view.centerOn(230, 0);
    QApplication::processEvents();
    auto* entity_item = find_node(view, "Owner");
    auto* relationship_item = find_node(view, "Related");
    view.select_elements({entity});
    const auto start = view.mapFromScene(entity_item->sceneBoundingRect().center());
    mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, start + QPoint(50, 25), Qt::NoButton, Qt::LeftButton);
    require(editor.rename(entity, "Updated"), "External edit during gesture");
    const auto revision = editor.revision();
    view.synchronize();
    mouse(view, QEvent::MouseButtonRelease, start + QPoint(50, 25), Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == revision && entity_item->pos() == QPointF(0, 0), "External revision cancels stale gesture");

    std::size_t callbacks = 0;
    view.on_selection = [&](const auto& selection) {
        ++callbacks;
        require(callbacks < 3, "Selection synchronization must not recurse");
        view.select_elements(selection, true);
    };
    view.select_elements({related, related});
    require(callbacks == 1, "Duplicate selection refs publish once");
    view.on_selection = {};

    for (int iteration = 0; iteration < 4; ++iteration) {
        require(editor.duplicate({entity, related, entity}), "Duplicate with duplicate refs");
        view.synchronize();
        require(view.scene()->items().size() == 10, "Duplicate projects owned attributes and fresh connectors once");
        require(editor.undo(), "Undo duplicate");
        view.synchronize();
        require(view.scene()->items().size() == 5, "Undo removes duplicate graphics without leftovers");
        require(find_node(view, "Updated") == entity_item && find_node(view, "Related") == relationship_item,
                "Duplicate undo preserves original graphics");
    }
    view.set_tool(desktop::Tool::Connect);
    click(view, entity_item->sceneBoundingRect().center());
    require(editor.erase({entity}), "Delete pending connection source and owned children");
    view.synchronize();
    require(view.scene()->items().size() == 1, "Delete removes all incident and owned graphics");
    const auto deletion_revision = editor.revision();
    click(view, relationship_item->sceneBoundingRect().center());
    require(editor.revision() == deletion_revision, "Deleted connection source is cancelled");
    key(view, Qt::Key_Escape);
    require(view.tool() == desktop::Tool::Select, "Escape cancels connection tool");
    view.select_elements({entity, entity}, true);
    require(view.selected_elements().empty(), "Deleted selection refs are harmless");

    const auto creation_revision = editor.revision();
    view.set_tool(desktop::Tool::Entity);
    key(view, Qt::Key_Escape);
    click(view, {0, 150});
    require(editor.revision() == creation_revision, "Escape cancels creation before click");
    editor.new_project();
    view.synchronize();
    require(view.scene()->items().empty() && view.selected_elements().empty(), "New project clears scene and selection");
}
} // namespace

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        SequentialIds ids;
        application::Editor editor(ids);
        const auto entity_result = editor.create_entity("Person", {400, 15, 160, 80});
        const auto relationship_result = editor.create_relationship("Supervises", {0, 0, 190, 110});
        const auto unrelated_result = editor.create_entity("Unrelated", {0, 300, 160, 80});
        require(entity_result && relationship_result && unrelated_result, "Create fixture");
        const auto entity = std::get<domain::EntityId>(*entity_result.created);
        const auto relationship = std::get<domain::RelationshipId>(*relationship_result.created);
        const auto participant = editor.connect(relationship, entity);
        require(participant, "Connect fixture");
        desktop::DiagramView view(editor);
        view.resize(1000, 700);
        view.show();
        view.centerOn(280, 160);
        QApplication::processEvents();
        auto* entity_item = find_node(view, "Person");
        auto* relationship_item = find_node(view, "Supervises");
        auto* unrelated_item = find_node(view, "Unrelated");
        auto* edge_item = find_edge(view);
        require(view.scene()->items().size() == 4, "Canvas projects all semantic nodes and participants");
        require(!relationship_item->shape().contains({2, 2}), "Diamond corners do not hit the node");
        require(edge_item->shape().contains({380, 42}), "Cardinality label is close to entity endpoint");
        const auto original_edge = edge_item->shape();
        const auto unrelated_position = unrelated_item->pos();
        view.select_elements({entity});
        view.zoom_in();
        const auto zoom = view.zoom_factor();
        const auto viewport_center = view.mapToScene(view.viewport()->rect().center());
        require(editor.rename(entity, "Employee"), "Rename");
        view.synchronize();
        require(find_node(view, "Employee") == entity_item && find_edge(view) == edge_item, "Synchronization preserves graphics identity");
        require(view.selected_elements() == std::vector<domain::ElementRef>{entity}, "Rename preserves selection");
        require(view.zoom_factor() == zoom && view.mapToScene(view.viewport()->rect().center()) == viewport_center, "Rename preserves viewport");
        view.actual_size();

        // A complete gesture updates presentation live, commits once, and undoes once.
        const auto start_rect = editor.project().layout.at(entity);
        const auto revision = editor.revision();
        const auto start = view.mapFromScene(entity_item->sceneBoundingRect().center());
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, start + QPoint(60, 40), Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, start + QPoint(80, 50), Qt::NoButton, Qt::LeftButton);
        require(editor.revision() == revision, "Pointer movement does not mutate domain");
        require(entity_item->pos() != QPointF(start_rect.x, start_rect.y), "Pointer movement updates node projection");
        require(edge_item->shape() != original_edge && unrelated_item->pos() == unrelated_position, "Drag updates incident connector and leaves unrelated node unchanged");
        mouse(view, QEvent::MouseButtonRelease, start + QPoint(80, 50), Qt::LeftButton, Qt::NoButton);
        require(editor.revision() == revision + 1, "One drag creates one command");
        require(editor.undo(), "Undo drag");
        view.synchronize();
        require(editor.project().layout.at(entity) == start_rect && entity_item->pos() == QPointF(start_rect.x, start_rect.y), "Single undo restores full drag");

        const auto cancel_revision = editor.revision();
        const auto cancel_start = view.mapFromScene(entity_item->sceneBoundingRect().center());
        mouse(view, QEvent::MouseButtonPress, cancel_start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, cancel_start + QPoint(45, 25), Qt::NoButton, Qt::LeftButton);
        key(view, Qt::Key_Escape);
        mouse(view, QEvent::MouseButtonRelease, cancel_start + QPoint(45, 25), Qt::LeftButton, Qt::NoButton);
        require(editor.revision() == cancel_revision && entity_item->pos() == QPointF(start_rect.x, start_rect.y), "Escape cancels drag without history");

        // Recursive participants retain separate identity and distinct hit geometry.
        const auto second_participant = editor.connect(relationship, entity);
        require(second_participant, "Recursive participant");
        view.synchronize();
        QList<QGraphicsItem*> recursive_edges;
        for (auto* item : view.scene()->items()) if (item->zValue() < 0) recursive_edges.push_back(item);
        require(recursive_edges.size() == 2 && recursive_edges[0]->shape() != recursive_edges[1]->shape(), "Recursive participant connectors are distinct");
        view.scene()->clearSelection();
        recursive_edges[0]->setSelected(true);
        const auto before_delete = editor.project();
        view.delete_selection();
        require(editor.project().relationships.at(relationship).participants.size() == 1, "Deleting selected connector disconnects only participant");
        require(editor.undo(), "Undo disconnect");
        view.synchronize();
        require(editor.project() == before_delete, "Undo restores stable participant");

        // Owner-first and child-first connection both work for composite attributes.
        const auto composite_result = editor.create_attribute("Address", {0, -200, 150, 60}, entity);
        const auto child_result = editor.create_attribute("City", {250, -200, 150, 60});
        require(composite_result && child_result, "Attributes fixture");
        const auto composite = std::get<domain::AttributeId>(*composite_result.created);
        const auto child = std::get<domain::AttributeId>(*child_result.created);
        require(editor.set_attribute_kind(composite, domain::AttributeKind::Composite), "Composite kind");
        view.synchronize();
        view.set_tool(desktop::Tool::Connect);
        click(view, find_node(view, "Address")->sceneBoundingRect().center());
        click(view, find_node(view, "City")->sceneBoundingRect().center());
        require(editor.project().attributes.at(child).owner == std::optional<domain::AttributeOwner>{composite}, "Owner-first composite connection");
        view.set_tool(desktop::Tool::Select);

        for (int i = 0; i < 50; ++i) view.zoom_in();
        require(std::abs(view.zoom_factor() - 3.0) < 0.0001, "Maximum zoom bounded");
        for (int i = 0; i < 100; ++i) view.zoom_out();
        require(std::abs(view.zoom_factor() - 0.15) < 0.0001, "Minimum zoom bounded");
        view.fit_diagram();
        require(view.zoom_factor() >= 0.15 && view.zoom_factor() <= 3, "Fit respects zoom bounds");
        group_movement_tests();
        synchronization_lifetime_tests();
        std::cout << "Canvas tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Canvas test failed: " << exception.what() << '\n';
        return 1;
    }
    return 0;
}
