#include "app/desktop/diagram_view.hpp"

#include <QApplication>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPainter>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>

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
bool blocks(const domain::Project& project) {
    const auto issues = validate(project);
    return std::any_of(issues.begin(), issues.end(), [](const auto& issue) { return issue.blocks_save; });
}
// A grab is in device pixels, which on a high-density display is not the same
// as the widget coordinates a scene point maps to. Indexing one with the other
// reads the wrong pixel, and only on such a display, so every pixel probe goes
// through here rather than depending on the machine running the test.
QPoint device_point(const QImage& image, const QPoint& widget_point) {
    const auto ratio = image.devicePixelRatio();
    return QPoint(static_cast<int>(widget_point.x() * ratio), static_cast<int>(widget_point.y() * ratio));
}
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
void key_to(QWidget* widget, int code) {
    QKeyEvent event(QEvent::KeyPress, code, Qt::NoModifier);
    QApplication::sendEvent(widget, &event);
    QApplication::processEvents();
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

// Dragging a connector's handle must behave like dragging a node: previewed
// live, committed once on release, and reversible in a single undo.
void connector_shaping_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-300, 0, 160, 80}).created);
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {-40, -20, 180, 100}).created);
    require(editor.connect(enrolled, student), "Connect student");

    desktop::DiagramView view(editor);
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    // With no bend stored the handle sits midway between the two body centres.
    const auto bend = (find_node(view, "Student")->sceneBoundingRect().center()
                       + find_node(view, "Enrolled")->sceneBoundingRect().center()) / 2;
    const auto from = view.mapFromScene(bend);

    // An unselected connector shows no handle, so the same drag must not shape it.
    mouse(view, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 10; ++step)
        mouse(view, QEvent::MouseMove, from + QPoint(0, step * 6), Qt::NoButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, from + QPoint(0, 60), Qt::LeftButton, Qt::NoButton);
    require(editor.project().connectors.empty(), "An unselected connector has no grab handle");

    auto* edge = find_edge(view);
    edge->setSelected(true);
    QApplication::processEvents();

    const auto revision = editor.revision();
    mouse(view, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 10; ++step)
        mouse(view, QEvent::MouseMove, from + QPoint(0, step * 6), Qt::NoButton, Qt::LeftButton);
    // The bend is only a preview until the button is released.
    require(editor.revision() == revision, "Dragging a handle does not commit until release");
    mouse(view, QEvent::MouseButtonRelease, from + QPoint(0, 60), Qt::LeftButton, Qt::NoButton);

    require(editor.project().connectors.size() == 1, "Release stores exactly one connector shape");
    const auto shaped = editor.project().connectors.begin()->first;
    require(editor.project().connectors.at(shaped).offset != 0, "The stored bend is non-zero");
    require(editor.undo_label() == "Shape connector", "The drag is one named history entry");
    require(editor.undo(), "Undo the bend");
    require(editor.project().connectors.empty(), "One undo restores automatic routing");
    require(editor.redo(), "Redo the bend");
    require(editor.project().connectors.size() == 1, "Redo restores the shape");

    // Double-clicking the connector restores automatic routing. The bend now
    // lies where the pointer left it, which is the only point still on the path.
    view.synchronize();
    QApplication::processEvents();
    const auto bent = from + QPoint(0, 60);
    mouse(view, QEvent::MouseButtonDblClick, bent, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, bent, Qt::LeftButton, Qt::NoButton);
    require(editor.project().connectors.empty(), "Double-click straightens the connector");
}

// Connecting must work as one press-drag-release gesture as well as the older
// click-then-click, and a drag that lands nowhere must not connect anything.
void drag_to_connect_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-320, 0, 160, 80}).created);
    const auto course = std::get<domain::EntityId>(*editor.create_entity("Course", {320, 0, 160, 80}).created);
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {-60, -20, 180, 100}).created);

    desktop::DiagramView view(editor);
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    // This case makes several connections in a row, so it locks the tool.
    view.set_tool(desktop::Tool::Connect, true);
    QApplication::processEvents();

    const auto at = [&](const QString& name) {
        return view.mapFromScene(find_node(view, name)->sceneBoundingRect().center());
    };

    // One gesture: press the relationship, carry the line, release on an entity.
    mouse(view, QEvent::MouseButtonPress, at("Enrolled"), Qt::LeftButton, Qt::LeftButton);
    const auto midway = (at("Enrolled") + at("Student")) / 2;
    mouse(view, QEvent::MouseMove, midway, Qt::NoButton, Qt::LeftButton);
    require(editor.project().relationships.at(enrolled).participants.empty(),
            "Carrying the line does not connect until release");
    mouse(view, QEvent::MouseButtonRelease, at("Student"), Qt::LeftButton, Qt::NoButton);
    require(editor.project().relationships.at(enrolled).participants.size() == 1,
            "Release over a valid target connects");
    require(editor.project().relationships.at(enrolled).participants.front().target == domain::ParticipantTarget{student},
            "The drag connected the entity under the release");

    // Click-then-click must still work for the same pair.
    click(view, find_node(view, "Enrolled")->sceneBoundingRect().center());
    require(editor.project().relationships.at(enrolled).participants.size() == 1,
            "A plain click only arms the source");
    click(view, find_node(view, "Course")->sceneBoundingRect().center());
    require(editor.project().relationships.at(enrolled).participants.size() == 2,
            "A second click completes the connection");
    require(editor.project().relationships.at(enrolled).participants.back().target == domain::ParticipantTarget{course},
            "Click-then-click connected the second entity");

    // A drag released over empty canvas connects nothing and disarms cleanly.
    const auto revision = editor.revision();
    mouse(view, QEvent::MouseButtonPress, at("Enrolled"), Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, QPoint(20, 20), Qt::NoButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, QPoint(20, 20), Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == revision, "Releasing over empty canvas connects nothing");
    view.cancel_interaction();
    require(editor.revision() == revision, "Cancelling a carried connection changes nothing");
}

// An unlocked tool is spent by a single use. Connect is where that matters
// most: it is the only tool needing two clicks, so it is the only one that can
// be abandoned part way, and a Connect left armed turns the next click on an
// element into a connection when the user meant to select it.
void connect_returns_to_select_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    editor.create_entity("Student", {-320, 0, 160, 80});
    editor.create_entity("Course", {320, 0, 160, 80});
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {-60, -20, 180, 100}).created);

    desktop::DiagramView view(editor);
    QString announced;
    view.on_status = [&](const QString& message) { announced = message; };
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();
    const auto centre = [&](const QString& name) { return find_node(view, name)->sceneBoundingRect().center(); };

    view.set_tool(desktop::Tool::Connect);
    require(!view.tool_locked(), "Choosing a tool plainly does not lock it");
    click(view, centre("Enrolled"));
    require(view.tool() == desktop::Tool::Connect, "The tool stays while a connection is half made");
    click(view, centre("Course"));
    require(editor.project().relationships.at(enrolled).participants.size() == 1, "The pair connected");
    require(view.tool() == desktop::Tool::Select, "Completing a connection hands the tool back");

    // Giving up on one spends the use just the same, so the click after it
    // selects rather than starting another connection.
    view.set_tool(desktop::Tool::Connect);
    click(view, centre("Enrolled"));
    click(view, view.mapToScene(QPoint(8, 8)));
    require(view.tool() == desktop::Tool::Select, "Abandoning a half-made connection hands the tool back");

    // A missed first click is only a miss, and must not cost the tool.
    view.set_tool(desktop::Tool::Connect);
    click(view, view.mapToScene(QPoint(8, 8)));
    require(view.tool() == desktop::Tool::Connect, "Clicking past an element with nothing armed keeps the tool");

    // An illegal pair is still an attempt. The tool goes back, and the reason
    // the pair was refused has to survive the handover rather than being
    // overwritten by the message the incoming tool announces itself with.
    const auto revision = editor.revision();
    view.set_tool(desktop::Tool::Connect);
    click(view, centre("Student"));
    click(view, centre("Course"));
    require(editor.revision() == revision, "Two entities do not connect");
    require(view.tool() == desktop::Tool::Select, "A refused pair spends the tool too");
    require(announced.contains("Connect an entity to a relationship"),
            "The refusal is still on screen after the tool hands back");

    // Locking is what holds a tool open, and must still do so.
    view.set_tool(desktop::Tool::Connect, true);
    click(view, centre("Enrolled"));
    click(view, centre("Student"));
    require(editor.project().relationships.at(enrolled).participants.size() == 2, "The locked tool still connects");
    require(view.tool() == desktop::Tool::Connect, "A locked tool keeps going");
}

// Locking a connector pins where it meets each shape, so the joins stop sliding
// as the attribute is moved.
void lock_connector_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto person = std::get<domain::EntityId>(*editor.create_entity("Person", {0, 0, 160, 80}).created);
    const auto owner = domain::AttributeOwner{domain::ElementRef{person}};
    const auto born = std::get<domain::AttributeId>(*editor.create_attribute("Born", {0, -220, 130, 54}, owner).created);

    desktop::DiagramView view(editor);
    view.resize(900, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, -60);
    QApplication::processEvents();

    auto* link = [&] {
        auto* node = find_node(view, "Born");
        for (auto* item : view.scene()->items())
            if (item->zValue() < 0 && item->shape().translated(item->scenePos())
                    .intersects(node->sceneBoundingRect()))
                return item;
        throw std::runtime_error("Missing attribute link");
    }();
    const auto body = find_node(view, "Person")->sceneBoundingRect();
    const auto outline_toward = [&](const QPointF& target) {
        const auto delta = target - body.center();
        const auto divisor = std::max(std::abs(delta.x()) / (body.width() / 2),
                                      std::abs(delta.y()) / (body.height() / 2));
        return body.center() + delta / divisor;
    };
    const auto meets = [&](const QPointF& point) { return link->shape().contains(link->mapFromScene(point)); };
    const auto attribute_centre = [&] { return find_node(view, "Born")->sceneBoundingRect().center(); };

    // Unlocked, the join tracks the attribute wherever it goes.
    const auto pinned_at = outline_toward(attribute_centre());
    require(meets(pinned_at), "An unlocked join sits in the attribute's direction");

    // Lock it exactly where it is drawn, so locking itself moves nothing.
    const auto angle = std::atan2(pinned_at.y() - body.center().y(), pinned_at.x() - body.center().x());
    require(editor.pin_connector(domain::ConnectorRef{born}, angle, std::nullopt), "Lock the connector");
    view.synchronize();
    QApplication::processEvents();
    require(meets(pinned_at), "Locking leaves the line where it was");

    // Now the attribute can go anywhere and the join stays behind.
    require(editor.move({{domain::ElementRef{born}, {430, 260, 130, 54}}}), "Move the attribute far away");
    view.synchronize();
    QApplication::processEvents();
    const auto would_have_slid = outline_toward(attribute_centre());
    require(std::hypot(would_have_slid.x() - pinned_at.x(), would_have_slid.y() - pinned_at.y()) > 40.0,
            "The move is far enough that an unlocked join would have travelled");
    require(meets(pinned_at), "A locked join stays where it was pinned");
    require(!meets(would_have_slid), "It does not follow the attribute");

    // Unlocking hands the connector back to automatic routing and stores nothing.
    require(editor.pin_connector(domain::ConnectorRef{born}, std::nullopt, std::nullopt), "Unlock it");
    require(editor.project().connectors.empty(), "An unlocked, unbent connector stores nothing");
    view.synchronize();
    QApplication::processEvents();
    require(meets(outline_toward(attribute_centre())), "Unlocking lets the join follow the attribute again");

    // A lock is an edit like any other.
    require(editor.undo(), "Undo the unlock");
    require(editor.project().connectors.size() == 1, "Undo restores the lock");
    require(editor.project().connectors.begin()->second.pinned(), "And it is restored as a pin");
    require(editor.redo(), "Redo the unlock");
    require(editor.project().connectors.empty(), "Redo clears it again");
    view.synchronize();
    QApplication::processEvents();

    // The padlock itself: a selected link carries one, and clicking it pins the
    // joins. Where exactly it sits is the drawing's business, so it is found the
    // way it is defined -- as hit area that a selected link has and an
    // unselected one does not -- rather than by assuming a position.
    view.set_tool(desktop::Tool::Select);
    const auto covers = [&](const QPointF& point) {
        return link->shape().contains(link->mapFromScene(point));
    };
    const auto search = link->shape().boundingRect().translated(link->scenePos()).adjusted(-20, -20, 20, 20);
    std::vector<QPointF> only_when_selected;
    for (qreal y = search.top(); y <= search.bottom(); y += 3)
        for (qreal x = search.left(); x <= search.right(); x += 3) {
            const QPointF point{x, y};
            link->setSelected(false);
            if (covers(point)) continue;
            link->setSelected(true);
            if (covers(point)) only_when_selected.push_back(point);
        }
    require(!only_when_selected.empty(), "A selected link grows hit areas an unselected one lacks");
    bool pinned_by_click = false;
    for (const auto& point : only_when_selected) {
        link->setSelected(true);
        QApplication::processEvents();
        click(view, point);
        if (!editor.project().connectors.empty() && editor.project().connectors.begin()->second.pinned()) {
            pinned_by_click = true;
            break;
        }
    }
    require(pinned_by_click, "One of them is a padlock, and clicking it pins the joins");
    require(editor.undo_label() == "Lock connector", "Clicking the padlock is one named history entry");
}


// A connector can be shaped by more than one bend: pressing on a selected line
// and dragging puts a corner there, and the line is routed through it.
void connector_route_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-320, 0, 160, 80}).created);
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {220, 0, 190, 110}).created);
    require(editor.connect(enrolled, student), "Connect student");

    desktop::DiagramView view(editor);
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    view.set_line_style(desktop::LineStyle::Straight);
    QApplication::processEvents();

    const auto student_centre = find_node(view, "Student")->sceneBoundingRect().center();
    const auto midpoint = (student_centre + find_node(view, "Enrolled")->sceneBoundingRect().center()) / 2;
    // Where the line actually runs is the drawing's business, so a point on it
    // is found by asking the item rather than by reproducing its geometry. Any
    // grip is avoided: a drag starting on one would move that corner instead of
    // making a new one.
    const auto point_on_line = [&](const QPointF& away_from) {
        auto* item = find_edge(view);
        const auto search = item->shape().boundingRect().translated(item->scenePos());
        const auto inside = [&](const QPointF& point) {
            return item->shape().contains(item->mapFromScene(point));
        };
        std::vector<QPointF> candidates;
        for (qreal y = search.top(); y <= search.bottom(); y += 2)
            for (qreal x = search.left(); x <= search.right(); x += 2) {
                const QPointF point{x, y};
                // Well inside the line's own hit area, not merely touching its
                // edge, or the press can miss by a rounding error.
                if (!inside(point) || !inside(point + QPointF(3, 0)) || !inside(point - QPointF(3, 0))
                    || !inside(point + QPointF(0, 3)) || !inside(point - QPointF(0, 3))) continue;
                if (find_node(view, "Student")->sceneBoundingRect().adjusted(-10, -10, 10, 10).contains(point)) continue;
                if (find_node(view, "Enrolled")->sceneBoundingRect().adjusted(-10, -10, 10, 10).contains(point)) continue;
                if (std::hypot(point.x() - away_from.x(), point.y() - away_from.y()) < 30.0) continue;
                candidates.push_back(point);
            }
        if (candidates.empty()) throw std::runtime_error("No point found on the connector");
        // The middle of what is left, so the point sits along the line rather
        // than at whichever extreme the scan happened to reach first.
        std::sort(candidates.begin(), candidates.end(), [&](const QPointF& a, const QPointF& b) {
            return std::hypot(a.x() - away_from.x(), a.y() - away_from.y())
                 < std::hypot(b.x() - away_from.x(), b.y() - away_from.y());
        });
        return candidates[candidates.size() / 2];
    };
    const auto on_line = point_on_line(midpoint);
    const auto drag = [&](const QPointF& from, const QPointF& to) {
        const auto start = view.mapFromScene(from);
        const auto finish = view.mapFromScene(to);
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 8; ++step)
            mouse(view, QEvent::MouseMove, start + (finish - start) * step / 8, Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseButtonRelease, finish, Qt::LeftButton, Qt::NoButton);
    };

    // The item is re-found before each gesture: reshaping a connector can
    // replace its item, and a kept pointer would be stale.
    const auto select = [&] { find_edge(view)->setSelected(true); QApplication::processEvents(); };

    // A line that is not selected is not shaped by anything: shaping is a
    // second gesture on something already picked out.
    click(view, on_line);
    require(editor.project().connectors.empty(), "Clicking an unselected connector only selects it");
    require(find_edge(view)->isSelected(), "Though it does select it");
    find_edge(view)->setSelected(false);
    QApplication::processEvents();
    drag(on_line, on_line + QPointF(0, 90));
    require(editor.project().connectors.empty(), "Dragging an unselected connector shapes nothing");

    // Clicking a selected line fixes it at that point. No grip has to be found
    // and no drag is needed: the click is the whole gesture.
    select();
    click(view, on_line);
    require(editor.project().connectors.size() == 1, "Clicking a selected line places a corner there");
    require(editor.project().connectors.begin()->second.waypoints.size() == 1, "Exactly one corner");
    const auto placed = editor.project().connectors.begin()->second.waypoints.front();
    require(std::hypot(placed.x - on_line.x(), placed.y - on_line.y()) < 8.0,
            "The corner lands where it was clicked");
    require(editor.undo(), "Undo the placed corner");
    require(editor.project().connectors.empty(), "Undo takes it away again");
    view.synchronize();
    QApplication::processEvents();

    select();
    drag(on_line, on_line + QPointF(0, 120));
    require(editor.project().connectors.size() == 1, "Dragging a selected line also stores a route");
    const auto ref = editor.project().connectors.begin()->first;
    require(editor.project().connectors.at(ref).waypoints.size() == 1, "It has one corner");
    require(editor.undo_label() == "Route connector", "The drag is one named history entry");
    const auto first = editor.project().connectors.at(ref).waypoints.front();
    require(std::abs(first.y - (on_line.y() + 120)) < 12.0, "The corner lands where it was dropped");
    require(std::abs(first.x - on_line.x()) < 12.0, "And keeps the across position it was dragged from");

    // A second drag, on a different stretch, adds a second corner rather than
    // replacing the first, and the two are kept in the order they are met.
    select();
    // Somewhere on the routed line, as far as possible from the corner it now
    // has, so the drag makes a second one rather than moving the first.
    const QPointF corner{first.x, first.y};
    const auto first_leg = point_on_line(corner);
    drag(first_leg, first_leg + QPointF(-60, -40));
    const auto corners = editor.project().connectors.at(ref).waypoints;
    require(corners.size() == 2, "A second drag adds a second corner");

    // Dragging a corner moves that corner rather than making another.
    select();
    const QPointF grip{corners.front().x, corners.front().y};
    drag(grip, grip + QPointF(40, 40));
    const auto moved = editor.project().connectors.at(ref).waypoints;
    require(moved.size() == 2, "Dragging a corner does not add one");
    require(moved.front() != corners.front(), "It moves the corner it grabbed");

    // Double-clicking one corner removes just that corner.
    select();
    const auto remaining = editor.project().connectors.at(ref).waypoints;
    const QPointF second{remaining.back().x, remaining.back().y};
    const auto position = view.mapFromScene(second);
    mouse(view, QEvent::MouseButtonPress, position, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton);
    mouse(view, QEvent::MouseButtonDblClick, position, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, position, Qt::LeftButton, Qt::NoButton);
    require(editor.project().connectors.at(ref).waypoints.size() == 1, "Double-clicking a corner removes it");

    // And undo walks back through each of those shaping steps.
    require(editor.undo(), "Undo the removal");
    require(editor.project().connectors.at(ref).waypoints.size() == 2, "Undo restores the corner");

    // The point of placing a corner is that it is a fixed point: the line is
    // frozen there, and moving either end of it swings the rest of the line
    // about the corner rather than carrying the corner along.
    const auto fixed = editor.project().connectors.at(ref).waypoints;
    require(editor.move({{domain::ElementRef{student}, {-320, 360, 160, 80}}}), "Move the entity");
    view.synchronize();
    QApplication::processEvents();
    require(editor.project().connectors.at(ref).waypoints == fixed,
            "Moving an element does not move the corners of a line that meets it");
    auto* held = find_edge(view);
    for (const auto& corner : fixed)
        require(held->shape().contains(held->mapFromScene(QPointF(corner.x, corner.y))),
                "And the line still runs through every one of them");
    require(editor.move({{domain::ElementRef{enrolled}, {220, -380, 190, 110}}}), "Move the relationship too");
    view.synchronize();
    QApplication::processEvents();
    require(editor.project().connectors.at(ref).waypoints == fixed, "Still fixed from the other end");
}


// The same locking applies to the line between an entity and a relationship,
// not only to an attribute's link.
void lock_participant_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-320, 0, 160, 80}).created);
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {220, 0, 190, 110}).created);
    require(editor.connect(enrolled, student), "Connect student");
    const auto side = editor.project().relationships.at(enrolled).participants.front().id;

    desktop::DiagramView view(editor);
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    const auto entity = find_node(view, "Student")->sceneBoundingRect();
    const auto outline_at = [](const QRectF& box, double radians) {
        const QPointF direction{std::cos(radians), std::sin(radians)};
        const auto divisor = std::max(std::abs(direction.x()) / (box.width() / 2),
                                      std::abs(direction.y()) / (box.height() / 2));
        return box.center() + direction / divisor;
    };
    const auto meets = [&](const QPointF& point) {
        auto* item = find_edge(view);
        return item->shape().contains(item->mapFromScene(point));
    };

    // Pin the entity end to the top of its box, which is not where the line
    // would otherwise meet it: the relationship is off to the right.
    const double upwards = -M_PI / 2;
    const auto pinned = outline_at(entity, upwards);
    require(!meets(pinned), "The line does not start out meeting the top of the entity");
    require(editor.pin_connector(domain::ConnectorRef{side}, std::nullopt, upwards), "Lock the entity end");
    view.synchronize();
    QApplication::processEvents();
    require(meets(pinned), "Locking moves the join to where it was pinned");

    // Moving the relationship right around must not drag the join with it.
    require(editor.move({{domain::ElementRef{enrolled}, {-320, 420, 190, 110}}}), "Move the relationship");
    view.synchronize();
    QApplication::processEvents();
    require(meets(pinned), "A locked join on a participant stays put");

    // Released, it follows again.
    require(editor.pin_connector(domain::ConnectorRef{side}, std::nullopt, std::nullopt), "Unlock it");
    require(editor.project().connectors.empty(), "An unlocked, unshaped connector stores nothing");
    view.synchronize();
    QApplication::processEvents();
    require(!meets(pinned), "Unlocking lets the join follow the relationship again");

    // And the relationship end can be pinned on its own.
    require(editor.pin_connector(domain::ConnectorRef{side}, 0.0, std::nullopt), "Lock the relationship end");
    const auto shaped = editor.project().connectors.at(domain::ConnectorRef{side});
    require(shaped.owner_anchor && !shaped.child_anchor, "Each end is pinned independently");
}


// An element can be given a surface colour of its own, one colour can be given
// to several at once, and a coloured element keeps a readable label.
void element_colour_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-320, 0, 160, 80}).created);
    const auto course = std::get<domain::EntityId>(*editor.create_entity("Course", {120, 0, 160, 80}).created);

    desktop::DiagramView view(editor);
    view.resize(900, 600);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    // The surface of an element, read from what is actually drawn.
    const auto surface = [&](const QString& name) {
        auto* node = find_node(view, name);
        const auto box = node->sceneBoundingRect();
        const auto image = view.viewport()->grab().toImage();
        return image.pixelColor(device_point(image, view.mapFromScene(box.center())
                                                 + QPoint(0, static_cast<int>(box.height() / 4))));
    };
    const auto before = surface("Student");

    const domain::Colour coral{0xFF, 0xA8, 0xA8};
    require(editor.recolour({domain::ElementRef{student}}, coral), "Colour one entity");
    view.synchronize();
    QApplication::processEvents();
    require(editor.project().colours.at(domain::ElementRef{student}) == coral, "The colour is stored");
    require(!editor.project().colours.contains(domain::ElementRef{course}), "Its neighbour is untouched");
    const auto after = surface("Student");
    require(after != before, "The element is drawn in its new colour");
    require(std::abs(after.red() - 0xFF) < 40 && std::abs(after.green() - 0xA8) < 40,
            "And that colour is the one chosen");
    require(surface("Course") == before, "The neighbour still follows the theme");

    // One colour for a whole selection, as one step of history.
    const auto revision = editor.revision();
    require(editor.recolour({domain::ElementRef{student}, domain::ElementRef{course}}, coral),
            "Colour both entities");
    require(editor.revision() == revision + 1, "Recolouring a selection is a single edit");
    require(editor.undo_label() == "Set colour", "Named for what it did");
    view.synchronize();
    QApplication::processEvents();
    require(editor.project().colours.size() == 2, "Both are stored");
    require(editor.undo(), "Undo the group recolour");
    require(editor.project().colours.size() == 1, "Undo restores exactly what was there");

    // Clearing hands the element back to its theme.
    require(editor.recolour({domain::ElementRef{student}}, {}), "Clear the colour");
    require(editor.project().colours.empty(), "Nothing is stored for an element following its theme");
    view.synchronize();
    QApplication::processEvents();
    require(surface("Student") == before, "And it is drawn the way the theme says again");

    // A dark surface must not be written on in dark ink.
    require(editor.recolour({domain::ElementRef{student}}, domain::Colour{0x20, 0x20, 0x30}), "Colour it dark");
    view.synchronize();
    QApplication::processEvents();
    const auto image = view.viewport()->grab().toImage();
    const auto box = find_node(view, "Student")->sceneBoundingRect();
    bool light_ink = false;
    const auto centre = view.mapFromScene(box.center());
    for (int dx = -50; dx <= 50 && !light_ink; ++dx)
        for (int dy = -8; dy <= 8 && !light_ink; ++dy) {
            const auto pixel = image.pixelColor(device_point(image, centre + QPoint(dx, dy)));
            if (pixel.red() > 200 && pixel.green() > 200 && pixel.blue() > 200) light_ink = true;
        }
    require(light_ink, "A label on a dark surface is written in light ink");
}


// Several elements can be picked out and then acted on together: the modifier
// that extends a selection is the same one the colour and delete paths read.
void extend_selection_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-360, 0, 160, 80}).created);
    const auto course = std::get<domain::EntityId>(*editor.create_entity("Course", {-60, 0, 160, 80}).created);
    const auto tutor = std::get<domain::EntityId>(*editor.create_entity("Tutor", {240, 0, 160, 80}).created);

    desktop::DiagramView view(editor);
    view.resize(1000, 600);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    const auto at = [&](const QString& name) {
        return view.mapFromScene(find_node(view, name)->sceneBoundingRect().center());
    };
    const auto click_with = [&](const QString& name, Qt::KeyboardModifiers modifiers) {
        mouse(view, QEvent::MouseButtonPress, at(name), Qt::LeftButton, Qt::LeftButton, modifiers);
        mouse(view, QEvent::MouseButtonRelease, at(name), Qt::LeftButton, Qt::NoButton, modifiers);
    };

    click_with("Student", Qt::NoModifier);
    require(view.selected_elements().size() == 1, "A plain click selects one element");

    // Command on a Mac and Control elsewhere both arrive as ControlModifier.
    click_with("Course", Qt::ControlModifier);
    require(view.selected_elements().size() == 2, "Command or Control click adds to the selection");
    click_with("Tutor", Qt::ShiftModifier);
    require(view.selected_elements().size() == 3, "Shift click adds to it as well");

    // Whatever has been picked out is what the colour is given to.
    const domain::Colour lilac{0xC9, 0xB0, 0xFF};
    require(editor.recolour(view.selected_elements(), lilac), "Colour the selection");
    require(editor.project().colours.size() == 3, "Every selected element takes the colour");

    // A copy looks like what it was copied from.
    require(editor.duplicate({domain::ElementRef{student}}), "Duplicate a coloured entity");
    require(editor.project().colours.size() == 4, "The copy carries the colour of its original");
    require(editor.undo(), "Undo the duplicate");
    require(editor.project().colours.size() == 3, "And undoing takes both away");
    view.synchronize();
    QApplication::processEvents();

    // The same modifier takes one back out again.
    click_with("Course", Qt::ControlModifier);
    require(view.selected_elements().size() == 2, "Clicking a selected element again removes it");

    // And a plain click starts over rather than adding.
    click_with("Student", Qt::NoModifier);
    require(view.selected_elements().size() == 1, "A plain click replaces the selection");

    // Deleting acts on the whole selection too.
    click_with("Course", Qt::ControlModifier);
    view.delete_selection();
    require(editor.project().entities.size() == 1, "Delete removes everything selected");
    require(editor.project().entities.begin()->first == tutor, "And leaves what was not");
    (void)student;
}


// The two pickers show a sample of a line. Drawn at the canvas weight in the
// connector's muted grey they came out as hairlines that could not be told
// apart in a menu, so they are checked for being visible at all.
void picker_sample_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    desktop::DiagramView view(editor);
    view.resize(800, 600);
    view.show();
    QApplication::processEvents();

    // How much of a sample is actually drawn, and in what colour.
    const auto drawn = [](const QPixmap& sample) {
        const auto image = sample.toImage().convertToFormat(QImage::Format_ARGB32);
        int opaque = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (qAlpha(image.pixel(x, y)) > 200) ++opaque;
        return opaque;
    };
    const auto carries = [](const QPixmap& sample, const QColor& ink) {
        const auto image = sample.toImage().convertToFormat(QImage::Format_ARGB32);
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                const auto pixel = image.pixel(x, y);
                if (qAlpha(pixel) < 200) continue;
                if (std::abs(qRed(pixel) - ink.red()) < 24 && std::abs(qGreen(pixel) - ink.green()) < 24
                    && std::abs(qBlue(pixel) - ink.blue()) < 24) return true;
            }
        return false;
    };

    for (const auto id : {desktop::ThemeId::OfficeLight, desktop::ThemeId::Dracula, desktop::ThemeId::Forest}) {
        view.set_theme(id);
        QApplication::processEvents();
        const auto accent = desktop::theme(id).accent;
        for (const auto style : {desktop::LineStyle::Curved, desktop::LineStyle::Straight}) {
            const auto sample = view.line_style_preview(style, QSize(48, 24));
            require(drawn(sample) > 60, "A line style sample is drawn heavily enough to see");
            require(carries(sample, accent), "And in the theme's own accent, not a muted grey");
        }
        for (int notation = 0; notation <= static_cast<int>(desktop::Notation::MinMax); ++notation) {
            const auto sample = view.notation_preview(static_cast<desktop::Notation>(notation), QSize(72, 24));
            require(drawn(sample) > 60, "A notation sample is drawn heavily enough to see");
            require(carries(sample, accent), "And in the theme's own accent");
        }
    }

    // The two styles have to be distinguishable from one another, or the picker
    // shows two samples that say the same thing.
    const auto curved = view.line_style_preview(desktop::LineStyle::Curved, QSize(48, 24)).toImage();
    const auto straight = view.line_style_preview(desktop::LineStyle::Straight, QSize(48, 24)).toImage();
    require(curved != straight, "The curved and straight samples are told apart");
}


// A side's constraints can be read off the line, so they can be changed there
// too. The canvas menu drives the same commands the properties panel does.
void participant_menu_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-320, 0, 160, 80}).created);
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {220, 0, 190, 110}).created);
    require(editor.connect(enrolled, student), "Connect student");
    const auto side = editor.project().relationships.at(enrolled).participants.front().id;

    desktop::DiagramView view(editor);
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    const auto constraints = [&] {
        const auto& participants = editor.project().relationships.at(enrolled).participants;
        const auto found = std::find_if(participants.begin(), participants.end(),
                                        [&](const auto& item) { return item.id == side; });
        require(found != participants.end(), "The side is still there");
        return std::pair{found->maximum, found->participation};
    };
    // The menu carries the role through rather than editing it, so a role set
    // beforehand must survive a change of constraint.
    require(editor.update_participant(enrolled, side, domain::Cardinality::Many,
                                      domain::Participation::Partial, "student"), "Give the side a role");
    require(constraints() == std::pair{domain::Cardinality::Many, domain::Participation::Partial},
            "It starts optional and many");

    // The menu opens on a line and its entries stand for these edits.
    const auto apply = [&](domain::Cardinality maximum, domain::Participation participation) {
        const auto& participants = editor.project().relationships.at(enrolled).participants;
        const auto found = std::find_if(participants.begin(), participants.end(),
                                        [&](const auto& item) { return item.id == side; });
        return editor.update_participant(enrolled, side, maximum, participation, found->role);
    };
    require(apply(domain::Cardinality::One, domain::Participation::Partial), "Set the maximum to one");
    require(constraints().first == domain::Cardinality::One, "The maximum changed");
    require(constraints().second == domain::Participation::Partial, "And the minimum was carried through");
    require(apply(domain::Cardinality::One, domain::Participation::Total), "Set the minimum to total");
    require(constraints() == std::pair{domain::Cardinality::One, domain::Participation::Total},
            "Both now hold what was chosen");
    const auto& participants = editor.project().relationships.at(enrolled).participants;
    require(std::find_if(participants.begin(), participants.end(),
                         [&](const auto& item) { return item.id == side; })->role == "student",
            "And the role was never touched");

    // What the line draws has to follow, or the menu and the diagram disagree.
    view.synchronize();
    QApplication::processEvents();
    const auto total = view.viewport()->grab().toImage();
    require(apply(domain::Cardinality::Many, domain::Participation::Partial), "Set it back to optional many");
    view.synchronize();
    QApplication::processEvents();
    require(view.viewport()->grab().toImage() != total, "The line is redrawn for the new constraints");

    // Reversing and disconnecting are offered on the same menu.
    require(editor.connect(enrolled, std::get<domain::EntityId>(*editor.create_entity("Course", {220, 300, 160, 80}).created)),
            "Connect a second side");
    require(editor.reverse_participants(enrolled), "Reverse the two sides");
    require(editor.disconnect(enrolled, side), "Disconnect one side");
    require(editor.project().relationships.at(enrolled).participants.size() == 1, "Only the other side is left");
    (void)student;
}

// A name must be editable on the element itself, not only in the properties
// panel, and the in-place editor must go through the same command path.
void inline_rename_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-120, 0, 170, 84}).created);

    desktop::DiagramView view(editor);
    view.resize(900, 600);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    const auto centre = view.mapFromScene(find_node(view, "Student")->sceneBoundingRect().center());
    require(!view.renaming(), "No editor is open to begin with");
    mouse(view, QEvent::MouseButtonDblClick, centre, Qt::LeftButton, Qt::LeftButton);
    require(view.renaming(), "Double-clicking an element opens an in-place editor");

    auto* field = view.viewport()->findChild<QLineEdit*>("inlineName");
    require(field != nullptr, "The in-place editor exists");
    require(field->isVisible(), "The in-place editor is shown over the element");
    require(field->text() == "Student", "The editor starts from the current name");
    require(field->geometry().contains(centre), "The editor sits over the element it renames");

    // Committing goes through the editor, so it is one undoable edit.
    const auto revision = editor.revision();
    field->setText("UniversityStudent");
    require(editor.revision() == revision, "Typing alone does not change the model");
    view.commit_rename();
    require(editor.project().entities.at(student).name == "UniversityStudent", "Commit renames the element");
    require(!view.renaming(), "Committing closes the editor");
    require(editor.undo_label() == "Rename element", "The in-place rename is one named history entry");
    require(editor.undo() && editor.project().entities.at(student).name == "Student", "One undo restores the name");

    // Escape abandons the pending text and leaves the model untouched.
    view.synchronize();
    QApplication::processEvents();
    mouse(view, QEvent::MouseButtonDblClick, centre, Qt::LeftButton, Qt::LeftButton);
    require(view.renaming(), "The editor reopens");
    field = view.viewport()->findChild<QLineEdit*>("inlineName");
    field->setText("Discarded");
    const auto before_escape = editor.revision();
    key_to(field, Qt::Key_Escape);
    require(!view.renaming(), "Escape closes the editor");
    require(editor.revision() == before_escape, "Escape makes no edit");
    require(editor.project().entities.at(student).name == "Student", "Escape keeps the previous name");

    // Renaming to the identical text is not an edit at all.
    mouse(view, QEvent::MouseButtonDblClick, centre, Qt::LeftButton, Qt::LeftButton);
    const auto unchanged = editor.revision();
    view.commit_rename();
    require(editor.revision() == unchanged, "Committing an unchanged name records nothing");

    // Escape must cancel only the rename. Left to bubble, it would reach the
    // view's own Escape handling and reset the active tool as a side effect.
    view.set_tool(desktop::Tool::Connect);
    view.begin_rename(domain::ElementRef{student});
    require(view.renaming(), "Renaming can be started without a double-click");
    field = view.viewport()->findChild<QLineEdit*>("inlineName");
    field->setText("Abandoned");
    key_to(field, Qt::Key_Escape);
    require(!view.renaming(), "Escape cancels the rename");
    require(view.tool() == desktop::Tool::Connect, "Escape while renaming leaves the active tool alone");
    require(editor.project().entities.at(student).name == "Student", "Escape still keeps the previous name");

    // Cancelling any interaction must not leave an editor floating on the canvas.
    view.set_tool(desktop::Tool::Select);
    mouse(view, QEvent::MouseButtonDblClick, centre, Qt::LeftButton, Qt::LeftButton);
    view.cancel_interaction();
    require(!view.renaming() && !field->isVisible(), "Cancelling closes the in-place editor");
}

// The same participant is readable in several notations. Each must reach the
// painter, and within a notation both the minimum and the maximum must show.
void notation_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto course = std::get<domain::EntityId>(*editor.create_entity("Course", {300, 0, 160, 80}).created);
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {-60, 0, 160, 80}).created);
    const auto side = *editor.connect(enrolled, domain::ParticipantTarget{course}).participant;

    desktop::DiagramView view(editor);
    view.resize(640, 260);
    view.show();
    view.set_grid_visible(false);
    view.fit_diagram();
    QApplication::processEvents();
    require(view.notation() == desktop::Notation::Chen, "Chen is the default notation");

    const auto render = [&] {
        QApplication::processEvents();
        QImage image(640, 260, QImage::Format_ARGB32_Premultiplied);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        view.render(&painter);
        return image;
    };

    // Every notation must draw the same participant differently.
    const std::array<desktop::Notation, 4> styles{desktop::Notation::Chen, desktop::Notation::MinMax,
                                                  desktop::Notation::CrowsFoot, desktop::Notation::Bachman};
    std::vector<QImage> drawn;
    for (const auto style : styles) {
        view.set_notation(style);
        require(view.notation() == style, "The chosen notation is kept");
        drawn.push_back(render());
    }
    for (std::size_t a = 0; a < drawn.size(); ++a)
        for (std::size_t b = a + 1; b < drawn.size(); ++b)
            require(drawn[a] != drawn[b], "Each notation draws the participant differently");

    // Re-selecting a notation reproduces its drawing exactly.
    view.set_notation(desktop::Notation::Chen);
    require(render() == drawn.front(), "Returning to a notation reproduces its drawing");

    // Within one notation, all four minimum/maximum combinations must differ,
    // which is only true if the minimum is drawn as well as the maximum.
    for (const auto style : styles) {
        view.set_notation(style);
        std::vector<QImage> combinations;
        for (const auto maximum : {domain::Cardinality::One, domain::Cardinality::Many})
            for (const auto minimum : {domain::Participation::Partial, domain::Participation::Total}) {
                require(editor.update_participant(enrolled, side, maximum, minimum, ""), "Set participant bounds");
                view.synchronize();
                combinations.push_back(render());
            }
        for (std::size_t a = 0; a < combinations.size(); ++a)
            for (std::size_t b = a + 1; b < combinations.size(); ++b)
                require(combinations[a] != combinations[b], "Every minimum/maximum pair is drawn distinctly");
    }
}

// A tool used once returns to Select; a locked tool stays, so several elements
// can be placed without going back to the toolbar between each one.
void tool_locking_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    desktop::DiagramView view(editor);
    view.resize(800, 600);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    require(!view.tool_locked(), "Tools start unlocked");
    view.set_tool(desktop::Tool::Entity);
    click(view, QPointF(-200, -120));
    require(editor.project().entities.size() == 1, "One click places one entity");
    require(view.tool() == desktop::Tool::Select, "An unlocked tool returns to Select");
    click(view, QPointF(0, -120));
    require(editor.project().entities.size() == 1, "The next click no longer places anything");

    view.set_tool(desktop::Tool::Entity, true);
    require(view.tool_locked(), "The tool reports being locked");
    for (const auto& at : {QPointF(-200, 40), QPointF(0, 40), QPointF(200, 40)}) click(view, at);
    require(editor.project().entities.size() == 4, "A locked tool keeps placing");
    require(view.tool() == desktop::Tool::Entity, "A locked tool stays selected");

    // Choosing any tool afresh clears the lock unless it is asked for again.
    view.set_tool(desktop::Tool::Relationship);
    require(!view.tool_locked(), "Choosing a tool normally clears the lock");
    click(view, QPointF(-200, 200));
    require(editor.project().relationships.size() == 1, "One click places one relationship");
    require(view.tool() == desktop::Tool::Select, "The relationship tool is one-shot too");

    // Escape leaves a locked tool, which is how the status line says to stop.
    view.set_tool(desktop::Tool::Entity, true);
    key(view, Qt::Key_Escape);
    require(view.tool() == desktop::Tool::Select && !view.tool_locked(), "Escape leaves a locked tool");

    // The ISA tools lock like any other, in either direction.
    for (const auto direction : {desktop::Tool::Specialization, desktop::Tool::Generalization}) {
        const auto before = editor.project().specializations.size();
        view.set_tool(direction, true);
        for (const auto& at : {QPointF(-200, 360), QPointF(0, 360), QPointF(200, 360)}) click(view, at);
        require(editor.project().specializations.size() == before + 3, "A locked ISA tool keeps placing");
        require(view.tool() == direction && view.tool_locked(), "It stays on the chosen direction");
    }
    // Each was placed with the direction its tool was set to.
    std::size_t generalisations = 0;
    for (const auto& [id, specialization] : editor.project().specializations) {
        (void)id;
        if (specialization.direction == domain::Inheritance::Generalization) ++generalisations;
    }
    require(generalisations == 3, "A locked tool keeps its direction for every element it places");

    // Select cannot be locked: it has nothing to repeat.
    view.set_tool(desktop::Tool::Select, true);
    require(!view.tool_locked(), "Select is never locked");
}

// The triangle is placed like any other element and wired up by hand. The first
// entity connected is what it generalises; every one after that is a subtype.
void inheritance_connection_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto person = std::get<domain::EntityId>(*editor.create_entity("Person", {0, 0, 160, 80}).created);
    editor.create_entity("Student", {-260, 420, 160, 80});
    editor.create_entity("Employee", {260, 420, 160, 80});

    desktop::DiagramView view(editor);
    view.resize(900, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, 200);
    QApplication::processEvents();
    const auto at = [&](const QString& name) {
        return find_node(view, name)->sceneBoundingRect().center();
    };

    // Placing needs no entity under the pointer, and connects nothing.
    view.set_tool(desktop::Tool::Specialization);
    click(view, QPointF(0, 210));
    require(editor.project().specializations.size() == 1, "A triangle is placed on empty canvas");
    const auto isa = editor.project().specializations.begin()->first;
    require(!editor.project().specializations.at(isa).supertype, "It starts with no supertype");
    require(editor.project().specializations.at(isa).subtypes.empty(), "It starts with no subtypes");
    require(!blocks(editor.project()), "An unconnected triangle is work in progress, not an error");

    // The first entity connected becomes the supertype.
    view.set_tool(desktop::Tool::Connect, true);
    click(view, at("IS A"));
    click(view, at("Person"));
    require(editor.project().specializations.at(isa).supertype == person, "The first connection is the supertype");
    require(editor.project().specializations.at(isa).subtypes.empty(), "It is not also a subtype");

    // Every one after that is a subtype.
    click(view, at("IS A"));
    click(view, at("Student"));
    click(view, at("IS A"));
    click(view, at("Employee"));
    const auto& wired = editor.project().specializations.at(isa);
    require(wired.subtypes.size() == 2, "Later connections are subtypes");
    require(wired.supertype == person, "The supertype is unchanged by them");
    require(!blocks(editor.project()), "The wired hierarchy is valid");

    // Detaching the supertype leaves the triangle and its subtypes in place.
    require(editor.set_supertype(isa, {}), "Detach the supertype");
    require(!editor.project().specializations.at(isa).supertype, "The supertype is cleared");
    require(editor.project().specializations.at(isa).subtypes.size() == 2, "The subtypes are untouched");
    require(!blocks(editor.project()), "A detached triangle is still valid");
    require(editor.undo(), "Undo the detach");
    require(editor.project().specializations.at(isa).supertype == person, "Undo restores the supertype");
}

// The ISA triangle points the way the hierarchy was read, so the two directions
// must not draw the same shape.
void inheritance_orientation_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto person = std::get<domain::EntityId>(*editor.create_entity("Person", {0, 0, 160, 80}).created);
    const auto isa = std::get<domain::SpecializationId>(
        *editor.create_specialization("IS A", {32, 200, 96, 74}, domain::Inheritance::Specialization).created);
    require(editor.set_supertype(isa, person), "Connect the supertype");

    desktop::DiagramView view(editor);
    view.resize(520, 420);
    view.show();
    view.set_grid_visible(false);
    view.actual_size();
    view.centerOn(80, 140);
    QApplication::processEvents();

    // Grab the viewport rather than rendering the widget: viewport pixels share
    // the coordinate space mapFromScene reports, so a point can be sampled.
    const auto render = [&] {
        QApplication::processEvents();
        return view.viewport()->grab().toImage();
    };
    // Sample just inside the triangle's top-left corner. That corner is solid
    // when the apex points down and empty when it points up, which pins the
    // shape itself rather than merely proving that something changed.
    const auto corner = [&] {
        const auto box = find_node(view, "IS A")->sceneBoundingRect();
        return view.mapFromScene(QPointF(box.left() + box.width() * 0.12, box.top() + box.height() * 0.12));
    };
    const auto background = render().pixel(2, 2);

    const auto pointing_down = render();
    require(pointing_down.pixel(device_point(pointing_down, corner())) != background,
            "Specialising fills the top corner: the apex is at the bottom");

    require(editor.set_inheritance_direction(isa, domain::Inheritance::Generalization), "Flip the direction");
    view.synchronize();
    const auto pointing_up = render();
    require(pointing_up.pixel(device_point(pointing_up, corner())) == background,
            "Generalising leaves the top corner empty: the apex is at the top");
    require(pointing_down != pointing_up, "The two ISA directions are drawn differently");

    require(editor.set_inheritance_direction(isa, domain::Inheritance::Specialization), "Flip it back");
    view.synchronize();
    require(render() == pointing_down, "Returning to a direction reproduces its drawing");

    // The link holds the triangle's point. Moving the entity it reaches must
    // bend the line rather than slide the attachment around the triangle.
    const auto subtype = std::get<domain::EntityId>(*editor.create_entity("Student", {0, 420, 160, 80}).created);
    require(editor.attach_subtype(isa, subtype), "Attach a subtype");
    view.synchronize();
    QApplication::processEvents();
    auto* link = find_edge(view, QStringLiteral("Inheritance — subtype"));
    const auto apex = [&] {
        const auto box = find_node(view, "IS A")->sceneBoundingRect();
        return QPointF(box.center().x(), box.bottom() - 1);
    };
    require(link->shape().contains(link->mapFromScene(apex())), "The link starts at the apex");
    // Far enough sideways that a boundary-following attachment would have left
    // the point entirely and moved onto the triangle's edge.
    require(editor.move({{domain::ElementRef{subtype}, {900, 420, 160, 80}}}), "Move the subtype aside");
    view.synchronize();
    QApplication::processEvents();
    link = find_edge(view, QStringLiteral("Inheritance — subtype"));
    require(link->shape().contains(link->mapFromScene(apex())), "The link still starts at the apex after the move");
}

// Attribute links leave their owner from one point per side and branch from
// there, and the same diagram can be drawn curved or straight.
void attribute_trunk_and_line_style_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto person = std::get<domain::EntityId>(*editor.create_entity("Person", {330, 330, 160, 80}).created);
    const auto owner = domain::AttributeOwner{domain::ElementRef{person}};
    // Three above, one to the left: two different sides of the same owner.
    editor.create_attribute("First", {120, 60, 130, 54}, owner);
    editor.create_attribute("Last", {330, 30, 130, 54}, owner);
    editor.create_attribute("Born", {540, 60, 130, 54}, owner);
    editor.create_attribute("Ident", {40, 330, 130, 54}, owner);

    desktop::DiagramView view(editor);
    view.resize(900, 620);
    view.show();
    view.set_grid_visible(false);
    view.fit_diagram();
    QApplication::processEvents();
    require(view.line_style() == desktop::LineStyle::Curved, "Connectors are curved by default");

    const auto edge_for = [&](const QString& attribute) {
        auto* node = find_node(view, attribute);
        for (auto* item : view.scene()->items())
            if (item->zValue() < 0 && item->shape().translated(item->scenePos())
                    .intersects(node->sceneBoundingRect()))
                return item;
        throw std::runtime_error("Missing attribute link");
    };
    // A link meets the body where its own attribute lies, rather than at the
    // middle of whichever face is nearest. Anchoring to a face's midpoint is
    // what made a dragged attribute's line jump: the join held still, then
    // leapt the width of the body the moment the nearest face changed.
    const auto body = find_node(view, "Person")->sceneBoundingRect();
    const QPointF above{body.center().x(), body.top()};
    const QPointF beside{body.left(), body.center().y()};
    const auto exit_toward = [](const QRectF& owner, const QPointF& target) {
        const auto centre = owner.center();
        const auto delta = target - centre;
        const auto divisor = std::max(std::abs(delta.x()) / (owner.width() / 2),
                                      std::abs(delta.y()) / (owner.height() / 2));
        return centre + delta / divisor;
    };
    std::vector<QPointF> exits;
    for (const auto& attribute : {QStringLiteral("First"), QStringLiteral("Last"), QStringLiteral("Born")}) {
        auto* link = edge_for(attribute);
        const auto exit = exit_toward(body, find_node(view, attribute)->sceneBoundingRect().center());
        require(link->shape().contains(link->mapFromScene(exit)),
                "A link leaves the body in its own attribute's direction");
        require(std::abs(exit.y() - body.top()) < 1.0, "An attribute above still leaves by the top");
        exits.push_back(exit);
    }
    // The outer two sit far apart along that same top edge. Were the anchor
    // still snapping to the face's midpoint, all three would coincide.
    require(std::abs(exits.front().x() - exits.back().x()) > 20.0,
            "Attributes spread along a side do not collapse onto one exit point");
    // Moving an attribute a little must move its join a little. This is the
    // property the old midpoint anchor lacked, and the reason the line jumped.
    const auto before = exit_toward(body, find_node(view, "Last")->sceneBoundingRect().center());
    const auto nudged = exit_toward(body, find_node(view, "Last")->sceneBoundingRect().center() + QPointF(6, 0));
    const auto shift = std::hypot(nudged.x() - before.x(), nudged.y() - before.y());
    require(shift > 0.0 && shift < 12.0, "A small move of an attribute slides its join a small amount");
    auto* sideways = edge_for(QStringLiteral("Ident"));
    require(sideways->shape().contains(sideways->mapFromScene(beside)),
            "A link on another side uses that side's exit point");
    require(!sideways->shape().contains(sideways->mapFromScene(above)),
            "It does not run back across the body to the shared point above");

    const auto render = [&] {
        QApplication::processEvents();
        return view.viewport()->grab().toImage();
    };
    const auto curved = render();
    view.set_line_style(desktop::LineStyle::Straight);
    require(view.line_style() == desktop::LineStyle::Straight, "The chosen style is kept");
    const auto straight = render();
    require(curved != straight, "The two line styles draw differently");
    view.set_line_style(desktop::LineStyle::Curved);
    require(render() == curved, "Returning to a style reproduces its drawing");

    // The exit point follows the owner as it moves, and is recomputed against
    // where the attribute now lies rather than staying on the face it left by.
    require(editor.move({{domain::ElementRef{person}, {620, 330, 160, 80}}}), "Move the owner");
    view.synchronize();
    QApplication::processEvents();
    const auto moved = find_node(view, "Person")->sceneBoundingRect();
    auto* link = edge_for(QStringLiteral("Last"));
    const auto moved_exit = exit_toward(moved, find_node(view, "Last")->sceneBoundingRect().center());
    require(link->shape().contains(link->mapFromScene(moved_exit)), "The exit point moves with the owner");
}

// Selecting an element must show what it connects to, so its links are drawn
// heavier and lifted above the rest rather than left to be traced by eye.
void selection_highlight_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-330, 240, 170, 84}).created);
    const auto course = std::get<domain::EntityId>(*editor.create_entity("Course", {330, 240, 170, 84}).created);
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {-40, 235, 180, 100}).created);
    require(editor.connect(enrolled, domain::ParticipantTarget{student}), "Connect student");
    require(editor.connect(enrolled, domain::ParticipantTarget{course}), "Connect course");
    const auto owner = domain::AttributeOwner{domain::ElementRef{student}};
    editor.create_attribute("StudentID", {-440, 60, 140, 58}, owner);
    editor.create_attribute("Name", {-230, 30, 140, 58}, owner);
    editor.create_attribute("Title", {470, 30, 140, 58}, domain::AttributeOwner{domain::ElementRef{course}});

    desktop::DiagramView view(editor);
    view.resize(1000, 600);
    view.show();
    view.fit_diagram();
    QApplication::processEvents();

    // Links sit below the elements; a highlighted one is lifted but stays below.
    const auto raised = [&] {
        std::size_t count = 0;
        for (auto* item : view.scene()->items())
            if (item->zValue() < 0 && item->zValue() > -1) ++count;
        return count;
    };
    require(raised() == 0, "Nothing is highlighted with no selection");

    view.select_elements({domain::ElementRef{student}});
    QApplication::processEvents();
    require(raised() == 3, "Selecting an entity raises its two attributes and its participant link");

    // A relationship raises both its participant links and its own attribute.
    view.select_elements({domain::ElementRef{enrolled}});
    QApplication::processEvents();
    require(raised() == 2, "Selecting a relationship raises both of its participant links");

    // Selecting both ends raises every link between and around them.
    view.select_elements({domain::ElementRef{student}, domain::ElementRef{course}});
    QApplication::processEvents();
    require(raised() == 5, "A multiple selection raises every link it touches");

    view.select_elements({});
    QApplication::processEvents();
    require(raised() == 0, "Clearing the selection clears the highlight");

    // A rebuilt link must be highlighted too. Detaching an attribute destroys
    // its link and undoing creates a fresh one, so this exercises a new item
    // rather than an existing one being updated in place.
    view.select_elements({domain::ElementRef{student}});
    QApplication::processEvents();
    domain::AttributeId detached{};
    for (const auto& [id, attribute] : editor.project().attributes)
        if (attribute.name == "Name") detached = id;
    require(editor.set_attribute_owner(detached, {}), "Detach an attribute");
    view.synchronize();
    QApplication::processEvents();
    require(raised() == 2, "The destroyed link is no longer highlighted");
    require(editor.undo(), "Undo the detach");
    view.synchronize();
    QApplication::processEvents();
    require(raised() == 3, "The rebuilt link is highlighted again");
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
        connector_shaping_tests();
        drag_to_connect_tests();
        connect_returns_to_select_tests();
        lock_connector_tests();
        connector_route_tests();
        lock_participant_tests();
        element_colour_tests();
        extend_selection_tests();
        picker_sample_tests();
        participant_menu_tests();
        inline_rename_tests();
        notation_tests();
        attribute_trunk_and_line_style_tests();
        selection_highlight_tests();
        tool_locking_tests();
        inheritance_connection_tests();
        inheritance_orientation_tests();
        std::cout << "Canvas tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Canvas test failed: " << exception.what() << '\n';
        return 1;
    }
    return 0;
}
