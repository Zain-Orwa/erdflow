#include "app/desktop/diagram_view.hpp"
#include "app/desktop/picture_export.hpp"

#include <QApplication>
#include <QBuffer>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QScrollBar>
#include <QSlider>
#include <QTimer>
#include <QPainter>
#include <QPointingDevice>
#include <QWheelEvent>
#include <QSvgRenderer>
#include <QXmlStreamReader>

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
    view.set_align_to_grid(true);
    const auto spacing = second_item->pos() - first_item->pos();
    const auto revision = editor.revision();
    const auto start = view.mapFromScene(first_item->sceneBoundingRect().center());
    mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, start + QPoint(47, 32), Qt::NoButton, Qt::LeftButton);
    require(second_item->pos() - first_item->pos() == spacing, "Aligning preserves relative group spacing");
    require(std::fmod(first_item->pos().x(), 20) == 0 && std::fmod(first_item->pos().y(), 20) == 0, "Drag anchor aligns to the grid");
    mouse(view, QEvent::MouseButtonRelease, start + QPoint(47, 32), Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == revision + 1, "Group drag commits once");
    require(editor.undo(), "Undo group drag");
    view.synchronize();
    require(first_item->pos() == QPointF(13, 17) && second_item->pos() == QPointF(246, 61), "One undo restores both group members");

    view.set_align_to_grid(false);
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
    // The single bend is the shape a straight line carries; a right-angle one
    // is shaped by its corners instead, which the route tests cover.
    view.set_line_style(desktop::LineStyle::Straight);
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

// Two entities have no line of their own, so connecting them creates the
// relationship they mean, midway between them, in one edit, and says so.
void entity_to_entity_creates_a_relationship_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-400, -40, 160, 80}).created);
    const auto course = std::get<domain::EntityId>(*editor.create_entity("Course", {240, 160, 160, 80}).created);

    desktop::DiagramView view(editor);
    QString announced;
    view.on_status = [&](const QString& message) { announced = message; };
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, 60);
    QApplication::processEvents();

    const auto body_of = [&](const QString& name) {
        auto* item = find_node(view, name);
        return item->shape().boundingRect().translated(item->scenePos());
    };
    const auto first = body_of("Student");
    const auto second = body_of("Course");
    const auto revision = editor.revision();
    view.set_tool(desktop::Tool::Connect);
    // Clicked high on one and low on the other, so the joins can be told apart.
    const QPointF on_student = first.center() + QPointF(50, -25);
    const QPointF on_course = second.center() + QPointF(-50, 25);
    click(view, on_student);
    click(view, on_course);

    require(editor.project().relationships.size() == 1, "A relationship was created between them");
    const auto& made = *editor.project().relationships.begin();
    require(made.second.participants.size() == 2, "With both entities as its sides");
    require(made.second.participants.front().target == domain::ParticipantTarget{student}
                && made.second.participants.back().target == domain::ParticipantTarget{course},
            "In the order they were clicked");
    require(editor.revision() == revision + 1, "The whole thing is one edit");
    require(editor.undo_label() == "Relate entities", "Named for what it did");
    require(announced.contains("created between them"), "And the user is told a relationship was made");

    // It lies between the two entities, is selected, and is open for its name.
    const auto body = editor.project().layout.at(domain::ElementRef{made.first});
    const auto midway = (first.center() + second.center()) / 2;
    require(std::abs(body.x + body.width / 2 - midway.x()) < 1 && std::abs(body.y + body.height / 2 - midway.y()) < 1,
            "Placed midway between the pair");
    require(view.selected_elements() == std::vector<domain::ElementRef>{made.first}, "The new relationship is selected");
    require(view.renaming(), "And opened for its name, since an unnamed relationship says nothing");
    auto* field = view.findChild<QLineEdit*>("inlineName");
    require(field != nullptr, "With the name field on screen");
    field->setText("Enrolled");
    key_to(field, Qt::Key_Return);
    require(editor.project().relationships.begin()->second.name == "Enrolled", "Typing names it");

    // Each line is pinned to the point its entity was clicked, as any other
    // connection made by clicking is.
    const auto& shapes = editor.project().connectors;
    require(shapes.size() == 2, "Both sides were pinned where they were clicked");
    const auto direction_to = [](const QRectF& box, const QPointF& point) {
        return std::atan2(point.y() - box.center().y(), point.x() - box.center().x());
    };
    const auto& student_side = shapes.at(domain::ConnectorRef{made.second.participants.front().id});
    require(student_side.child_anchor && std::abs(*student_side.child_anchor - direction_to(first, on_student)) < 0.05,
            "The student end where the student was clicked");
    require(!student_side.owner_anchor, "And the relationship end left to route itself");

    // A second relationship between the same pair reads it another way, so it
    // steps aside rather than landing on the first, and is then an element
    // like any other to put where it belongs.
    {
        const auto first_body = editor.project().layout.at(made.first);
        view.set_tool(desktop::Tool::Connect);
        click(view, on_student);
        click(view, on_course);
        require(editor.project().relationships.size() == 2, "The pair can be read a second way");
        domain::ElementRef second_ref = made.first;
        for (const auto& [id, relationship] : editor.project().relationships) {
            (void)relationship;
            if (id != made.first) second_ref = id;
        }
        const auto second_body = editor.project().layout.at(second_ref);
        const auto clear = std::abs(second_body.y - first_body.y) > first_body.height
            || std::abs(second_body.x - first_body.x) > first_body.width;
        require(clear, "The second diamond does not land on the first");
        // It steps across the line joining the entities rather than along it,
        // which is what keeps two readings of one pair side by side.
        const auto between = QPointF(second.center().x() - first.center().x(),
                                     second.center().y() - first.center().y());
        const QPointF stepped(second_body.x - first_body.x, second_body.y - first_body.y);
        const auto along = QPointF::dotProduct(stepped, between) / std::hypot(between.x(), between.y());
        require(std::abs(along) < 1.0, "It steps across the line joining the entities, not along it");
        require(std::hypot(stepped.x(), stepped.y()) > first_body.height, "And far enough to stand clear");
        // And it is an ordinary element afterwards.
        require(editor.move({{second_ref, {900, 900, second_body.width, second_body.height}}}), "Put it elsewhere");
        require(editor.project().layout.at(second_ref).x == 900, "Where it stays");
        require(editor.undo(), "Undo the move");
        require(editor.undo(), "Undo the second relationship");
        require(editor.project().relationships.size() == 1, "Which leaves the first alone");
        view.synchronize();
    }

    // The tool is spent by the use, and one undo takes the whole thing back.
    require(view.tool() == desktop::Tool::Select, "The pair spent the tool");
    require(editor.undo(), "Undo the naming");
    require(editor.undo(), "Undo the relationship");
    require(editor.project().relationships.empty() && editor.project().connectors.empty(),
            "One undo removes the relationship and both of its lines");
    require(editor.project().entities.size() == 2, "The entities are untouched");
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

    // A click on empty canvas hands back even before anything is armed, as
    // every other tool does on such a click.
    view.set_tool(desktop::Tool::Connect);
    click(view, view.mapToScene(QPoint(8, 8)));
    require(view.tool() == desktop::Tool::Select, "Clicking empty canvas hands the tool back");

    // Locked, it stays through the same click.
    view.set_tool(desktop::Tool::Connect, true);
    click(view, view.mapToScene(QPoint(8, 8)));
    require(view.tool() == desktop::Tool::Connect, "A locked Connect survives a click on empty canvas");
    view.set_tool(desktop::Tool::Select);

    // An illegal pair is still an attempt. The tool goes back, and the reason
    // the pair was refused has to survive the handover rather than being
    // overwritten by the message the incoming tool announces itself with.
    // Two plain relationships are such a pair: only an associative one can
    // take part in another relationship.
    require(editor.create_relationship("Teaches", {-60, 320, 180, 100}), "A second relationship");
    view.synchronize();
    QApplication::processEvents();
    const auto revision = editor.revision();
    view.set_tool(desktop::Tool::Connect);
    click(view, centre("Enrolled"));
    click(view, centre("Teaches"));
    require(editor.revision() == revision, "Two plain relationships do not connect");
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


// A connection made by clicking two points joins each shape at those two
// points, and either end can then be carried to another point on its shape.
// Two lines may leave one point. The automatic joins are still there for the
// asking, and a connection made under them stores nothing.
void join_where_clicked_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-320, 0, 160, 80}).created);
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {220, 0, 190, 110}).created);
    editor.create_entity("Course", {-320, 260, 160, 80});

    desktop::DiagramView view(editor);
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, 80);
    QApplication::processEvents();
    require(view.join_mode() == desktop::JoinMode::WhereClicked, "New lines join where they are clicked unless told otherwise");

    // A node's bounding rect is padded a little for its selection outline; the
    // body itself is the shape, and the grips sit on the body's outline.
    const auto body_of = [&](const QString& name) {
        auto* item = find_node(view, name);
        return item->shape().boundingRect().translated(item->scenePos());
    };
    const auto entity = body_of("Student");
    const auto diamond = body_of("Enrolled");
    // The point on a box or a diamond in a given direction from its centre.
    const auto outline_at = [](const QRectF& box, double radians, bool is_diamond) {
        const QPointF direction{std::cos(radians), std::sin(radians)};
        const auto rx = box.width() / 2;
        const auto ry = box.height() / 2;
        const auto divisor = is_diamond ? std::abs(direction.x()) / rx + std::abs(direction.y()) / ry
                                        : std::max(std::abs(direction.x()) / rx, std::abs(direction.y()) / ry);
        return box.center() + direction / divisor;
    };
    const auto direction_to = [](const QRectF& box, const QPointF& point) {
        return std::atan2(point.y() - box.center().y(), point.x() - box.center().x());
    };
    const auto participants = [&] { return editor.project().relationships.at(enrolled).participants; };
    const auto lines_through = [&](const QPointF& point) {
        int count = 0;
        for (auto* item : view.scene()->items())
            if (item->zValue() < 0 && item->shape().contains(item->mapFromScene(point))) ++count;
        return count;
    };

    // Click the entity high on its right, and the diamond low on its left.
    const QPointF on_entity = entity.center() + QPointF(50, -30);
    const QPointF on_diamond = diamond.center() + QPointF(-50, 15);
    view.set_tool(desktop::Tool::Connect, true);
    click(view, on_entity);
    click(view, on_diamond);
    require(participants().size() == 1, "The pair connected");
    const auto side = participants().front().id;
    require(editor.project().connectors.size() == 1, "The line was given its shape as it was made");
    const auto shape = editor.project().connectors.at(domain::ConnectorRef{side});
    require(shape.owner_anchor && shape.child_anchor, "Both ends are pinned");
    require(std::abs(*shape.child_anchor - direction_to(entity, on_entity)) < 0.05,
            "The entity end is pinned in the direction it was clicked");
    require(std::abs(*shape.owner_anchor - direction_to(diamond, on_diamond)) < 0.05,
            "And the relationship end in the direction the diamond was clicked");
    require(editor.undo_label() == "Connect participant", "The joins came with the connection, as one edit");
    const auto entity_join = outline_at(entity, *shape.child_anchor, false);
    require(lines_through(entity_join) == 1, "The line meets the entity where it was clicked");
    require(lines_through(outline_at(diamond, *shape.owner_anchor, true)) == 1, "And the diamond where it was clicked");

    // Moving the other end leaves a join made by clicking exactly where it was.
    require(editor.move({{domain::ElementRef{enrolled}, {220, -300, 190, 110}}}), "Move the relationship away");
    view.synchronize();
    QApplication::processEvents();
    require(lines_through(entity_join) == 1, "A join made by clicking stays put as the other end moves");
    require(editor.undo(), "Put the relationship back");
    view.synchronize();
    QApplication::processEvents();

    // Taking hold of the entity end and carrying it across the entity moves
    // the join round the outline to meet it. It is previewed while carried
    // and written once, on release.
    view.set_tool(desktop::Tool::Select);
    find_edge(view)->setSelected(true);
    QApplication::processEvents();
    const QPointF top{entity.center().x(), entity.top()};
    const auto from = view.mapFromScene(entity_join);
    const auto to = view.mapFromScene(top + QPointF(0, 12));
    const auto revision = editor.revision();
    mouse(view, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 10; ++step)
        mouse(view, QEvent::MouseMove, from + (to - from) * step / 10, Qt::NoButton, Qt::LeftButton);
    require(editor.revision() == revision, "Carrying an end does not commit until release");
    require(lines_through(top) == 1, "The carried end is previewed where the pointer is");
    mouse(view, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == revision + 1, "Putting the end down commits once");
    const auto carried = editor.project().connectors.at(domain::ConnectorRef{side});
    require(std::abs(*carried.child_anchor + M_PI / 2) < 0.05, "The entity end now joins at the top");
    require(carried.owner_anchor == shape.owner_anchor, "The other end was left alone");
    require(carried.waypoints.empty(), "And the line still runs straight to it");
    require(lines_through(top) == 1, "And the line is drawn to the top");
    require(editor.undo_label() == "Shape connector", "Putting an end down is one named history entry");
    require(editor.undo(), "Undo it");
    view.synchronize();
    QApplication::processEvents();
    require(lines_through(entity_join) == 1, "Undo puts the end back where it was");
    require(editor.redo(), "Redo it");
    view.synchronize();
    QApplication::processEvents();

    // Carried past the shape and let go in open canvas, the end stops there:
    // the line keeps a corner at the point rather than the gesture coming to
    // nothing, and the join it was dragged from is left as it was.
    {
        auto* link = find_edge(view);
        link->setSelected(true);
        QApplication::processEvents();
        const auto grip = view.mapFromScene(outline_at(entity, -M_PI / 2, false));
        const QPointF stop{entity.center().x() - 210, entity.top() - 150};
        const auto away = view.mapFromScene(stop);
        const auto before = editor.revision();
        mouse(view, QEvent::MouseButtonPress, grip, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 8; ++step)
            mouse(view, QEvent::MouseMove, grip + (away - grip) * step / 8, Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseButtonRelease, away, Qt::LeftButton, Qt::NoButton);
        require(editor.revision() == before + 1, "Letting go in open canvas is one edit");
        const auto stopped = editor.project().connectors.at(domain::ConnectorRef{side});
        require(!stopped.waypoints.empty(), "Which leaves a corner where the end was let go");
        require(std::hypot(stopped.waypoints.back().x - stop.x(), stopped.waypoints.back().y - stop.y()) < 2.0,
                "At the point itself");
        require(stopped.child_anchor == carried.child_anchor, "The join it was dragged from is left where it was");
        require(editor.undo(), "Undo the stopping point");
        view.synchronize();
        QApplication::processEvents();
    }

    // A second line may leave the very same point: connect the entity to the
    // relationship again from its top.
    view.set_tool(desktop::Tool::Connect, true);
    click(view, top + QPointF(0, 6));
    click(view, on_diamond);
    require(participants().size() == 2, "A second connection was made");
    const auto second = editor.project().connectors.at(domain::ConnectorRef{participants().back().id});
    require(second.child_anchor && std::abs(*second.child_anchor + M_PI / 2) < 0.1, "It is pinned to the same point");
    require(lines_through(top) == 2, "Two lines leave one point");

    // Automatic joins are still there for the asking, and store nothing.
    view.set_join_mode(desktop::JoinMode::Automatic);
    click(view, find_node(view, "Course")->sceneBoundingRect().center() + QPointF(40, 20));
    click(view, on_diamond);
    require(participants().size() == 3, "The pair connected under automatic joins");
    require(editor.project().connectors.size() == 2, "An automatic connection stores no shape");

    // An attribute's link made by clicking is pinned the same way, with the
    // owner end on the element that owns the attribute.
    view.set_join_mode(desktop::JoinMode::WhereClicked);
    const auto born = std::get<domain::AttributeId>(*editor.create_attribute("Born", {-320, -220, 130, 54}).created);
    view.synchronize();
    QApplication::processEvents();
    const auto attribute = body_of("Born");
    const QPointF on_attribute = attribute.center() + QPointF(20, 18);
    const QPointF on_owner = entity.center() + QPointF(-45, -30);
    click(view, on_attribute);
    click(view, on_owner);
    require(editor.project().attributes.at(born).owner == std::optional<domain::AttributeOwner>{domain::ElementRef{student}},
            "The attribute now belongs to the entity");
    const auto link = editor.project().connectors.at(domain::ConnectorRef{born});
    require(link.owner_anchor && std::abs(*link.owner_anchor - direction_to(entity, on_owner)) < 0.05,
            "The owner end of an attribute link is on the entity, where it was clicked");
    require(link.child_anchor && std::abs(*link.child_anchor - direction_to(attribute, on_attribute)) < 0.05,
            "And the child end on the attribute, where it was clicked");
    require(editor.undo_label() == "Change attribute owner", "One edit made the link and its joins");
    view.set_tool(desktop::Tool::Select);
}

// Pictures and notes are on the canvas without being in the model: a note is
// put down by its tool and opens for its title, a picture draws its own
// pixels, and nothing connects to either.
// A symbol is drawn as its character grown to fill its box, so the box is how
// big the character is. That makes it the one element on the diagram with a
// size of its own to choose: by its corners, or by the two commands that do
// the same thing without the hand.
void symbol_resize_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto symbol = *editor.create_symbol("\U0001F393", {0, 0, 56, 56}).created;
    const auto entity = *editor.create_entity("Student", {300, 0, 160, 80}).created;
    desktop::DiagramView view(editor);
    view.resize(900, 600);
    view.show();
    view.actual_size();
    view.centerOn(100, 40);
    QApplication::processEvents();
    const auto box_of = [&](const domain::ElementRef& ref) { return editor.project().layout.at(ref); };

    // An entity's box is sized by the name it has to hold, so it has no size
    // to choose and the commands leave it alone rather than reshaping it.
    view.select_elements({entity});
    QApplication::processEvents();
    require(view.selected_symbols().empty(), "An entity is not a symbol");
    const auto entity_box = box_of(entity);
    const auto before_entity = editor.revision();
    view.resize_symbols(2.0);
    require(box_of(entity) == entity_box, "So enlarging does nothing to it");
    require(editor.revision() == before_entity, "And writes no edit at all");

    view.select_elements({symbol});
    QApplication::processEvents();
    require(view.selected_symbols() == std::vector<domain::ElementRef>{symbol}, "A symbol is one");

    // Hauling the bottom right corner. The corner opposite it stays where it
    // is, which is what makes the symbol grow away from the hand.
    const auto start = box_of(symbol);
    const QPointF corner(start.x + start.width, start.y + start.height);
    mouse(view, QEvent::MouseButtonPress, view.mapFromScene(corner), Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, view.mapFromScene(corner + QPointF(60, 60)), Qt::NoButton, Qt::LeftButton);
    require(box_of(symbol) == start, "A drag in progress has written nothing yet");
    require(find_node(view, "\U0001F393")->boundingRect().width() > start.width + 50,
            "Though the symbol is already drawn at the size being asked for");
    mouse(view, QEvent::MouseButtonRelease, view.mapFromScene(corner + QPointF(60, 60)), Qt::LeftButton, Qt::NoButton);
    const auto hauled = box_of(symbol);
    require(std::abs(hauled.width - 116) < 2 && std::abs(hauled.height - 116) < 2, "Letting go writes the size");
    require(std::abs(hauled.x - start.x) < 0.01 && std::abs(hauled.y - start.y) < 0.01,
            "And the opposite corner has not moved");
    require(editor.undo_label() == "Resize symbol", "The history says what was done");
    require(editor.undo() && box_of(symbol) == start, "And it undoes like any other edit");
    view.synchronize();
    QApplication::processEvents();

    // A grip pressed and let go without travelling asks for nothing, so the
    // history is not filled with steps that changed nothing.
    const auto before_click = editor.revision();
    mouse(view, QEvent::MouseButtonPress, view.mapFromScene(corner), Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, view.mapFromScene(corner), Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == before_click, "A grip clicked and let go writes nothing");

    // Escape during the drag puts the symbol back at the size it was.
    mouse(view, QEvent::MouseButtonPress, view.mapFromScene(corner), Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, view.mapFromScene(corner + QPointF(120, 120)), Qt::NoButton, Qt::LeftButton);
    view.cancel_interaction();
    QApplication::processEvents();
    require(box_of(symbol) == start, "Escape abandons the size");
    require(std::abs(find_node(view, "\U0001F393")->boundingRect().width() - start.width) < 20,
            "And the symbol is drawn at the stored size again");

    // The two commands grow and shrink about the symbol's own centre, so it
    // stays where it was put instead of walking across the diagram.
    const QPointF centre(start.x + start.width / 2, start.y + start.height / 2);
    view.resize_symbols(1.25);
    const auto grown = box_of(symbol);
    require(std::abs(grown.width - start.width * 1.25) < 0.01, "Enlarge grows it by a quarter");
    require(std::abs(grown.x + grown.width / 2 - centre.x()) < 0.01
            && std::abs(grown.y + grown.height / 2 - centre.y()) < 0.01, "About its own centre");
    view.resize_symbols(1 / 1.25);
    require(std::abs(box_of(symbol).width - start.width) < 0.01, "And shrink puts it back");

    // Neither runs away: a symbol cannot be shrunk to nothing or enlarged
    // until it is the paper rather than a mark on it.
    for (int step = 0; step < 40; ++step) view.resize_symbols(1 / 1.25);
    require(box_of(symbol).width == domain::min_symbol_size, "Shrinking stops at the smallest");
    for (int step = 0; step < 80; ++step) view.resize_symbols(1.25);
    require(box_of(symbol).width == domain::max_symbol_size, "And enlarging at the largest");

    // A mixed selection is not a selection of symbols, so the commands that
    // act on symbols do not act on half of it.
    view.select_elements({symbol, entity});
    QApplication::processEvents();
    require(view.selected_symbols().size() == 1, "A mixed selection holds one symbol");
    require(view.selected_symbols() != view.selected_elements(), "But is not all symbols");
}

void figure_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    editor.create_entity("Student", {-320, 0, 160, 80});
    desktop::DiagramView view(editor);
    view.resize(900, 600);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    // The note tool puts a note where the canvas is clicked and opens it for
    // its title, since a note is for writing on.
    view.set_tool(desktop::Tool::Note);
    click(view, QPointF(200, 150));
    require(editor.project().notes.size() == 1, "A click places a note");
    const domain::ElementRef note = editor.project().notes.begin()->first;
    require(view.tool() == desktop::Tool::Select, "The note tool is one use, like the others");
    require(view.selected_elements() == std::vector<domain::ElementRef>{note}, "The new note is selected");
    require(view.renaming(), "And opens for its title at once");
    auto* field = view.findChild<QLineEdit*>("inlineName");
    require(field && field->isVisible(), "With the title field on screen");
    field->setText("Assumptions");
    key_to(field, Qt::Key_Return);
    require(editor.project().notes.begin()->second.name == "Assumptions", "Typing gives the note its title");
    require(editor.describe(note, "Each student enrols each term."), "Its text is its description");
    view.synchronize();
    QApplication::processEvents();
    require(find_node(view, "Assumptions") != nullptr, "The note is a node like any other");


    // A picture draws its own pixels: a solid red image comes out red.
    QImage red(40, 30, QImage::Format_RGB32);
    red.fill(QColor(220, 30, 30));
    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    require(red.save(&buffer, "PNG"), "Encode a test picture");
    const auto placed = editor.create_picture("Red", {-100, -200, 120, 90},
                                              std::vector<std::uint8_t>(encoded.begin(), encoded.end()));
    require(placed && placed.created, "A picture is placed from PNG bytes");
    view.set_grid_visible(false);
    view.synchronize();
    QApplication::processEvents();
    const auto image = view.viewport()->grab().toImage();
    const auto pixel = image.pixelColor(device_point(image, view.mapFromScene(QPointF(-40, -155))));
    require(pixel.red() > 180 && pixel.green() < 90 && pixel.blue() < 90, "The picture's pixels are drawn on the canvas");
    // It lies beneath the lines and the elements, so it can be a background.
    for (auto* item : view.scene()->items())
        if (item->toolTip() == "Red") require(item->zValue() < -1, "A picture is drawn beneath the lines");

    // Nothing connects to a figure: Connect refuses the pair.
    view.set_tool(desktop::Tool::Connect);
    const auto revision = editor.revision();
    click(view, find_node(view, "Assumptions")->sceneBoundingRect().center());
    click(view, find_node(view, "Student")->sceneBoundingRect().center());
    require(editor.revision() == revision, "A note and an entity do not connect");
    require(view.tool() == desktop::Tool::Select, "The refused pair spends the tool");

    // The right-click menu offers to insert either, at the point clicked.
    QPointF asked_for;
    view.on_insert_picture = [&](QPointF at) { asked_for = at; };
    bool opened_for_title = false;
    const auto right_click = [&](const QPointF& scene_point, const char* entry) {
        opened_for_title = false;
        QTimer::singleShot(0, [&, entry] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            require(menu != nullptr, "A right-click opens a menu");
            auto* action = menu->findChild<QAction*>(QString::fromLatin1(entry));
            require(action != nullptr, "It offers the entry asked for");
            // A click closes the menu before the entry acts, and so does this.
            menu->close();
            action->trigger();
            // Looked at here, the moment the entry has acted, which is what a
            // user sees. The offscreen platform then deactivates the window as
            // the popup goes, which closes any editor and which no desktop does.
            opened_for_title = view.renaming();
        });
        const auto position = view.mapFromScene(scene_point);
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, view.viewport()->mapToGlobal(position));
        QApplication::sendEvent(view.viewport(), &event);
        QApplication::processEvents();
        view.activateWindow();
    };
    right_click(QPointF(-300, 200), "contextInsertNote");
    require(editor.project().notes.size() == 2, "Insert → Note from the menu places a note");
    const auto latest = editor.project().layout.at(editor.project().notes.rbegin()->first);
    require(std::abs(latest.x + latest.width / 2 + 300) < 1 && std::abs(latest.y + latest.height / 2 - 200) < 1,
            "Centred where the canvas was clicked");
    require(opened_for_title, "And opened for its title");
    view.cancel_interaction();
    right_click(QPointF(120, -80), "contextInsertPicture");
    require(asked_for == QPointF(120, -80), "Insert → Picture asks the window for a file, for that point");
    // The same entries sit at the end of an element's own menu.
    right_click(find_node(view, "Student")->sceneBoundingRect().center(), "contextInsertNote");
    require(editor.project().notes.size() == 3, "An element's menu offers Insert as well");
    view.cancel_interaction();
    require(editor.undo() && editor.undo(), "Take the two menu notes away again");
    view.synchronize();

    // Both go with the rest of a selection, and come back with one undo.
    view.select_elements({note, *placed.created});
    view.delete_selection();
    require(editor.project().notes.empty() && editor.project().pictures.empty(), "Delete removes both");
    require(editor.undo(), "Undo the deletion");
    view.synchronize();
    require(editor.project().notes.size() == 1 && editor.project().pictures.size() == 1, "One undo brings both back");

    // A symbol is a note that is one character standing on its own. It is
    // drawn as the character and nothing else: no card, no border, no title.
    // The card is what has to be gone, so the test looks for the card rather
    // than for the character.
    {
        const auto shot = [&] {
            view.synchronize();
            QApplication::processEvents();
            return view.viewport()->grab().toImage();
        };
        const auto surface = desktop::note_surface(desktop::theme(view.theme_id()));
        // Compared as plain pixel values: two QColors of the same colour are
        // unequal when they were built in different colour spaces.
        const auto wanted = surface.rgb();
        const auto carded = [wanted](const QImage& image) {
            int found = 0;
            for (int y = 0; y < image.height(); ++y)
                for (int x = 0; x < image.width(); ++x)
                    if ((image.pixel(x, y) | 0xff000000u) == wanted) ++found;
            return found;
        };
        require(carded(shot()) > 0, "An ordinary note is drawn on a card");
        const domain::Rect where{120, -220, 56, 56};
        const auto symbol = editor.create_symbol("⋈", where);
        require(symbol && symbol.created, "A symbol is placed");
        require(editor.project().notes.at(std::get<domain::NoteId>(*symbol.created)).plain,
                "And it is a note that knows it is drawn bare");
        // Only the symbol's own patch of canvas is read, so the written note
        // elsewhere on the diagram cannot answer for it.
        const auto image = shot();
        require(find_node(view, "⋈") != nullptr, "The symbol is a node like any other");
        const auto ratio = image.devicePixelRatio();
        const auto room = view.mapFromScene(QRectF(where.x, where.y, where.width, where.height)).boundingRect();
        const QRect patch(QPoint(static_cast<int>(room.left() * ratio) - 4, static_cast<int>(room.top() * ratio) - 4),
                          QPoint(static_cast<int>(room.right() * ratio) + 4, static_cast<int>(room.bottom() * ratio) + 4));
        const auto paper = desktop::theme(view.theme_id()).canvas.rgb();
        int card = 0, marked = 0;
        for (int y = std::max(0, patch.top()); y <= std::min(image.height() - 1, patch.bottom()); ++y)
            for (int x = std::max(0, patch.left()); x <= std::min(image.width() - 1, patch.right()); ++x) {
                const auto pixel = image.pixel(x, y) | 0xff000000u;
                if (pixel == wanted) ++card;
                if (pixel != paper) ++marked;
            }
        require(card == 0, "A symbol has no card behind it, no border and no title");
        require(marked > 0, "The character itself is drawn");
    }
}

// The notation for the weak side of a model: a weak entity wears a double
// border, its key a dashed underline, an identifying relationship a double
// diamond, and a total specialization a double line to its supertype, with the
// constraint's letter on the triangle.
void weak_and_identifying_drawing_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto employee = std::get<domain::EntityId>(*editor.create_entity("Employee", {-340, -40, 160, 80}).created);
    const auto dependant = std::get<domain::EntityId>(*editor.create_entity("Dependant", {180, -40, 160, 80}).created);
    const auto has = std::get<domain::RelationshipId>(*editor.create_relationship("Has", {-100, -60, 190, 110}).created);
    const auto owner = domain::AttributeOwner{domain::ElementRef{dependant}};
    const auto name = std::get<domain::AttributeId>(*editor.create_attribute("Name", {200, 120, 150, 60}, owner).created);
    require(editor.set_attribute_kind(name, domain::AttributeKind::Key), "A key on the dependant");
    require(editor.connect(has, employee) && editor.connect(has, dependant), "Both take part");
    const auto isa = std::get<domain::SpecializationId>(*editor.create_specialization("IS A", {-380, 160, 96, 74}, domain::Inheritance::Specialization).created);
    require(editor.set_supertype(isa, employee), "The triangle hangs off the employee");

    desktop::DiagramView view(editor);
    view.resize(1000, 700);
    view.show();
    view.set_grid_visible(false);
    view.actual_size();
    view.centerOn(-60, 60);
    QApplication::processEvents();
    const auto render = [&](const QString& node) {
        view.synchronize();
        QApplication::processEvents();
        const auto image = view.viewport()->grab().toImage();
        const auto box = find_node(view, node)->sceneBoundingRect();
        const QRect area(view.mapFromScene(box.topLeft()), view.mapFromScene(box.bottomRight()));
        return image.copy(QRect(device_point(image, area.topLeft()), device_point(image, area.bottomRight())));
    };
    const auto plain_entity = render("Dependant");
    const auto plain_key = render("Name");
    require(editor.set_entity_weak(dependant, true), "Make it weak");
    require(render("Dependant") != plain_entity, "A weak entity is drawn differently: its double border");
    require(render("Name") != plain_key, "And its key differently: the dashed underline of a partial key");
    const auto plain_diamond = render("Has");
    require(editor.set_relationship_kind(has, domain::RelationshipKind::Identifying), "Make the relationship identifying");
    require(render("Has") != plain_diamond, "An identifying relationship is drawn with its second diamond");
    require(editor.set_relationship_kind(has, domain::RelationshipKind::Regular), "And back");
    require(render("Has") == plain_diamond, "Regular again draws as before");

    // The triangle wears the constraint's letter, and a total specialization's
    // line to its supertype is doubled.
    const auto disjoint = render("IS A");
    require(editor.set_specialization_rules(isa, domain::Disjointness::Overlapping, domain::Completeness::Partial), "Overlapping");
    require(render("IS A") != disjoint, "The letter changes from d to o");
    const auto whole = [&] {
        view.synchronize();
        QApplication::processEvents();
        return view.viewport()->grab().toImage();
    };
    const auto partial = whole();
    require(editor.set_specialization_rules(isa, domain::Disjointness::Overlapping, domain::Completeness::Total), "Total");
    require(whole() != partial, "A total specialization draws its link doubled");
}

// A relationship that meets one entity twice is recursive. Its first side runs
// straight to the diamond; its second leaves the far corner and comes back
// around the entity at right angles, and follows the shapes as they move until
// the user takes hold of it.
void recursive_loop_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto employee = std::get<domain::EntityId>(*editor.create_entity("Employee", {-260, -40, 180, 90}).created);
    const auto manages = editor.relate(employee, employee, {160, -55, 190, 110}, "Manages");
    require(manages && manages.created, "A relationship from the entity back to itself");
    const auto id = std::get<domain::RelationshipId>(*manages.created);
    require(editor.project().relationships.at(id).participants.size() == 2, "With two sides, both on the entity");

    desktop::DiagramView view(editor);
    view.resize(1100, 800);
    view.show();
    view.set_grid_visible(false);
    view.actual_size();
    view.centerOn(40, 60);
    QApplication::processEvents();

    const auto sides = [&] {
        std::vector<QGraphicsItem*> found;
        for (auto* item : view.scene()->items())
            if (item->zValue() < 0 && item->toolTip().startsWith("Participant")) found.push_back(item);
        require(found.size() == 2, "Both sides are drawn");
        return found;
    };
    const auto entity_body = [&] {
        auto* node = find_node(view, "Employee");
        return node->shape().boundingRect().translated(node->scenePos());
    };
    const auto diamond_body = [&] {
        auto* node = find_node(view, "Manages");
        return node->shape().boundingRect().translated(node->scenePos());
    };

    // One of the two reaches well past both shapes: that is the loop. The
    // other stays in the space between them.
    const auto below_both = std::max(entity_body().bottom(), diamond_body().bottom());
    auto* first = sides()[0];
    auto* second = sides()[1];
    const auto reaches = [&](QGraphicsItem* item) { return item->sceneBoundingRect().bottom() > below_both + 8; };
    require(reaches(first) != reaches(second), "Exactly one of the two sides loops around");
    auto* loop = reaches(first) ? first : second;
    auto* straight = reaches(first) ? second : first;
    require(straight->sceneBoundingRect().left() > entity_body().left(),
            "The straight side keeps to the space between the two shapes");

    // The loop comes back underneath the entity, which is what makes it read
    // as a loop rather than as a line wandering off.
    const auto returns_under = [&] {
        for (qreal y = entity_body().bottom() + 4; y < entity_body().bottom() + 90; y += 2)
            if (loop->shape().contains(loop->mapFromScene(QPointF(entity_body().center().x(), y)))) return true;
        return false;
    };
    require(returns_under(), "The loop returns under the entity and into its face");

    // It follows the shapes: move the entity and the loop moves with it.
    require(editor.move({{domain::ElementRef{employee}, {-260, 260, 180, 90}}}), "Move the entity down");
    view.synchronize();
    QApplication::processEvents();
    loop = reaches(sides()[0]) ? sides()[0] : sides()[1];
    require(returns_under(), "And the loop is redrawn around where the entity now is");

    // Nothing is stored for it until the user takes hold of it; dragging the
    // line then keeps the shape they can see and adds a corner to it.
    require(editor.project().connectors.empty(), "A loop the canvas worked out stores nothing");
    loop->setSelected(true);
    QApplication::processEvents();
    const auto on_loop = [&] {
        const auto bounds = loop->sceneBoundingRect();
        for (qreal x = bounds.left(); x < bounds.right(); x += 2)
            for (qreal y = bounds.bottom() - 30; y < bounds.bottom(); y += 2)
                if (loop->shape().contains(loop->mapFromScene(QPointF(x, y)))) return QPointF(x, y);
        throw std::runtime_error("No point found on the loop");
    }();
    const auto from = view.mapFromScene(on_loop);
    mouse(view, QEvent::MouseButtonPress, from, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 6; ++step)
        mouse(view, QEvent::MouseMove, from + QPoint(0, step * 5), Qt::NoButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, from + QPoint(0, 30), Qt::LeftButton, Qt::NoButton);
    require(editor.project().connectors.size() == 1, "Taking hold of it stores the route");
    const auto& kept = editor.project().connectors.begin()->second;
    require(kept.waypoints.size() >= 4, "Which keeps the loop's own corners and the new one");
    require(editor.undo(), "Undo the shaping");
    view.synchronize();
    QApplication::processEvents();
    require(editor.project().connectors.empty(), "And the loop is the canvas's again");
}

// Elements line up with their neighbours as they are dragged, the way a page
// layout does, and a selection can be lined up on one edge or middle at once.
void alignment_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto anchor = std::get<domain::EntityId>(*editor.create_entity("Anchor", {0, 0, 160, 80}).created);
    const auto mover = std::get<domain::EntityId>(*editor.create_entity("Mover", {400, 260, 160, 80}).created);
    const auto third = std::get<domain::EntityId>(*editor.create_entity("Third", {-300, 500, 120, 60}).created);

    desktop::DiagramView view(editor);
    view.resize(1100, 900);
    view.show();
    view.set_grid_visible(false);
    view.actual_size();
    view.centerOn(100, 250);
    QApplication::processEvents();

    // Dragged so its middle comes within a few pixels of the anchor's, it
    // meets it exactly rather than stopping just short.
    const auto centre_of = [&](domain::EntityId id) {
        const auto rect = editor.project().layout.at(domain::ElementRef{id});
        return QPointF(rect.x + rect.width / 2, rect.y + rect.height / 2);
    };
    const auto anchor_centre = centre_of(anchor);
    view.select_elements({mover});
    auto* item = find_node(view, "Mover");
    const auto grab = view.mapFromScene(item->sceneBoundingRect().center());
    // Four pixels short of standing in line with the anchor's middle.
    const auto wanted = QPointF(anchor_centre.x() + 4, centre_of(mover).y());
    const auto to = view.mapFromScene(wanted);
    mouse(view, QEvent::MouseButtonPress, grab, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 8; ++step)
        mouse(view, QEvent::MouseMove, grab + (to - grab) * step / 8, Qt::NoButton, Qt::LeftButton);
    require(std::abs(item->sceneBoundingRect().center().x() - anchor_centre.x()) < 0.51,
            "A middle that comes near another's meets it exactly");
    mouse(view, QEvent::MouseButtonRelease, to, Qt::LeftButton, Qt::NoButton);
    require(std::abs(centre_of(mover).x() - anchor_centre.x()) < 0.51, "And it is written where it was drawn");

    // Dropped well away from anything, nothing pulls it out of place.
    const auto revision = editor.revision();
    const auto free_grab = view.mapFromScene(find_node(view, "Mover")->sceneBoundingRect().center());
    const auto free_to = free_grab + QPoint(133, 47);
    mouse(view, QEvent::MouseButtonPress, free_grab, Qt::LeftButton, Qt::LeftButton);
    for (int step = 1; step <= 8; ++step)
        mouse(view, QEvent::MouseMove, free_grab + (free_to - free_grab) * step / 8, Qt::NoButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, free_to, Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == revision + 1, "The free drag is one edit");
    require(std::abs(centre_of(mover).x() - anchor_centre.x()) > 40, "Nothing holds an element that is going elsewhere");

    // The whole selection can be lined up at once, on one edge or one middle.
    const auto lefts = [&] {
        std::vector<double> found;
        for (const auto id : {anchor, mover, third}) found.push_back(editor.project().layout.at(domain::ElementRef{id}).x);
        return found;
    };
    require(lefts()[0] != lefts()[1] || lefts()[1] != lefts()[2], "They do not start on one line");
    const auto before = editor.revision();
    const std::vector<domain::ElementRef> all{anchor, mover, third};
    const auto leftmost = std::min({lefts()[0], lefts()[1], lefts()[2]});
    view.align_selection_for_test(all, true, 0);
    require(editor.revision() == before + 1, "Lining a selection up is one edit");
    for (const auto edge : lefts()) require(std::abs(edge - leftmost) < 0.51, "Every one of them sits on the left edge");
    require(editor.undo(), "Which undoes in one step");
    require(lefts()[0] != lefts()[1] || lefts()[1] != lefts()[2], "Putting them back where they were");
}

// Anything that names an element can draw it: the canvas renders one element's
// own shape in its own colour, so a relationship shows as a diamond rather
// than as a coloured box, and the panel and the diagram cannot disagree.
void element_shape_preview_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto entity = *editor.create_entity("Person", {0, 0, 160, 80}).created;
    const auto relationship = *editor.create_relationship("Knows", {300, 0, 190, 110}).created;
    desktop::DiagramView view(editor);
    view.resize(700, 500);
    view.show();
    QApplication::processEvents();

    const auto drawn = [&](const domain::ElementRef& ref) {
        return view.element_preview(ref, QSize(40, 28), true).toImage().convertToFormat(QImage::Format_ARGB32);
    };
    const auto box = drawn(entity);
    const auto diamond = drawn(relationship);
    require(box.pixelColor(box.width() / 2, box.height() / 2).alpha() > 200
                && diamond.pixelColor(diamond.width() / 2, diamond.height() / 2).alpha() > 200,
            "Both are drawn where their middles are");
    require(box.pixelColor(3, 3).alpha() > 200, "A rectangle reaches its corners");
    require(diamond.pixelColor(2, 2).alpha() < 60, "A diamond leaves them bare, which a box would not");

    // It carries the element's own colour, including one the user chose.
    const auto& colors = desktop::theme(view.theme_id());
    const auto middle = [](const QImage& image) { return image.pixelColor(image.width() / 2, image.height() / 2); };
    const auto near_enough = [](const QColor& first, const QColor& second) {
        return std::abs(first.red() - second.red()) < 14 && std::abs(first.green() - second.green()) < 14
            && std::abs(first.blue() - second.blue()) < 14;
    };
    require(near_enough(middle(box), colors.entity_fill), "An entity wears the theme's entity colour");
    require(editor.recolour({entity}, domain::Colour{0x9E, 0xE8, 0xC4}), "Colour it by hand");
    view.synchronize();
    QApplication::processEvents();
    require(near_enough(middle(drawn(entity)), QColor(0x9E, 0xE8, 0xC4)), "And then the one it was given");
}

// A selection's lines can be locked or released together from the right-click
// menu, in one edit, and the whole diagram's from the menu on empty canvas.
void lock_selection_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-360, 0, 160, 80}).created);
    const auto course = std::get<domain::EntityId>(*editor.create_entity("Course", {320, 0, 160, 80}).created);
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {-40, -20, 180, 100}).created);
    const auto first = editor.connect(enrolled, student);
    const auto second = editor.connect(enrolled, course);
    require(first && second, "Both sides connected");
    const auto owner = domain::AttributeOwner{domain::ElementRef{student}};
    const auto born = std::get<domain::AttributeId>(*editor.create_attribute("Born", {-380, -230, 140, 60}, owner).created);

    desktop::DiagramView view(editor);
    view.resize(1100, 800);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    const auto pinned = [&](const domain::ConnectorRef& ref) {
        const auto found = editor.project().connectors.find(ref);
        return found != editor.project().connectors.end() && found->second.pinned();
    };
    const auto right_click = [&](const QPointF& scene_point, const char* entry, bool expect_enabled) {
        QTimer::singleShot(0, [&, entry, expect_enabled] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            require(menu != nullptr, "A right-click opens a menu");
            auto* action = menu->findChild<QAction*>(QString::fromLatin1(entry));
            require(action != nullptr, "It offers locking and releasing");
            require(action->isEnabled() == expect_enabled, "Each is offered only while it has something to do");
            // A click closes the menu before the entry acts, and so does this.
            menu->close();
            if (expect_enabled) action->trigger();
        });
        const auto position = view.mapFromScene(scene_point);
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, view.viewport()->mapToGlobal(position));
        QApplication::sendEvent(view.viewport(), &event);
        QApplication::processEvents();
        view.activateWindow();
    };

    // The lines one entity touches: its side of the relationship and the link
    // to its attribute, but not the other entity's side.
    view.select_elements({domain::ElementRef{student}});
    QApplication::processEvents();
    const auto revision = editor.revision();
    right_click(find_node(view, "Student")->sceneBoundingRect().center(), "contextLockConnectors", true);
    require(editor.revision() == revision + 1, "Locking a selection's lines is one edit");
    require(editor.undo_label() == "Lock connectors", "Named for what it did");
    require(pinned(domain::ConnectorRef{*first.participant}), "The side it takes part in is pinned");
    require(pinned(domain::ConnectorRef{born}), "And the link to its attribute");
    require(!pinned(domain::ConnectorRef{*second.participant}), "The line it does not touch is left alone");
    require(editor.undo(), "Which undoes in one step");
    require(editor.project().connectors.empty(), "Putting every one of them back");
    require(editor.redo(), "And redoes");

    // Offered only while it has something to do: these are locked already.
    right_click(find_node(view, "Student")->sceneBoundingRect().center(), "contextLockConnectors", false);
    right_click(find_node(view, "Student")->sceneBoundingRect().center(), "contextUnlockConnectors", true);
    require(!pinned(domain::ConnectorRef{*first.participant}) && !pinned(domain::ConnectorRef{born}),
            "Releasing hands the joins back");

    // With nothing chosen, the menu on empty canvas covers the whole diagram.
    view.select_elements({});
    QApplication::processEvents();
    const auto before = editor.revision();
    right_click(QPointF(0, 420), "contextLockConnectors", true);
    require(editor.revision() == before + 1, "The whole diagram is one edit too");
    require(pinned(domain::ConnectorRef{*first.participant}) && pinned(domain::ConnectorRef{*second.participant})
                && pinned(domain::ConnectorRef{born}),
            "Every line is pinned, whichever elements it touches");
    right_click(QPointF(0, 420), "contextUnlockConnectors", true);
    require(editor.project().connectors.empty(), "And every one of them released again");
}

// The paper a diagram is drawn on: each ruling draws something the plain
// canvas does not, a picture of one's own tiles behind everything, and the
// strength decides how much of any of it shows.
void background_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    desktop::DiagramView view(editor);
    view.resize(600, 420);
    view.show();
    view.set_grid_visible(false);
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    const auto drawn = [&] {
        view.synchronize();
        QApplication::processEvents();
        return view.viewport()->grab().toImage();
    };
    const auto canvas_colour = desktop::theme(view.theme_id()).canvas;
    const auto ink = [canvas_colour](const QImage& image) {
        // How much of the view is not the plain canvas colour. The colour is
        // taken from the theme rather than from a corner, since a corner may
        // itself be sitting on a rule.
        const auto plain = canvas_colour;
        int marked = 0;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (image.pixelColor(x, y) != plain) ++marked;
        return marked;
    };
    const auto plain = drawn();
    require(ink(plain) == 0, "The plain canvas is plain");

    for (const auto style : {domain::BackgroundStyle::Squares, domain::BackgroundStyle::Lines,
                             domain::BackgroundStyle::Dots}) {
        require(editor.set_background(domain::Background{style, 100, {}}), "Lay a ruling on the canvas");
        const auto ruled = drawn();
        require(ruled != plain && ink(ruled) > 0, "Which draws something the plain canvas does not");
        // A ruling is drawn as the ruling it is: asking for less of it is not
        // offered, and asking anyway leaves it at full strength.
        require(editor.set_background(domain::Background{style, 20, {}}), "Ask for a fainter ruling");
        require(editor.project().background.strength == 100, "A ruling stays at full strength");
    }
    // Squares and lines are not the same ruling.
    require(editor.set_background(domain::Background{domain::BackgroundStyle::Squares, 100, {}}), "Squares");
    const auto squares = drawn();
    require(editor.set_background(domain::Background{domain::BackgroundStyle::Lines, 100, {}}), "Lines");
    require(drawn() != squares, "Each ruling is its own");

    // A picture of one's own, tiled behind the diagram.
    QImage own(16, 16, QImage::Format_RGB32);
    own.fill(QColor(0x20, 0x80, 0x40));
    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    require(own.save(&buffer, "PNG"), "Encode a background picture");
    require(editor.set_background(domain::Background{domain::BackgroundStyle::Image, 100,
                                                     std::vector<std::uint8_t>(encoded.begin(), encoded.end())}),
            "Lay it on the canvas");
    const auto papered = drawn();
    const auto middle = papered.pixelColor(papered.width() / 2, papered.height() / 2);
    require(middle.green() > 100 && middle.red() < 80, "The picture is what the canvas now shows");
    require(papered.pixelColor(4, 4) == middle && papered.pixelColor(papered.width() - 4, 4) == middle,
            "One picture covering the view, corner to corner, rather than repeated across it");

    // It is fixed to the view rather than to the canvas, so zooming in does
    // not magnify it: it stays the picture at its own resolution.
    view.zoom_in();
    view.zoom_in();
    require(drawn() == papered, "Zooming in does not magnify the picture");
    view.actual_size();

    // A picture is the one paper that fades, so it sits behind the diagram
    // rather than in front of it.
    auto faded = editor.project().background;
    faded.strength = 30;
    require(editor.set_background(faded), "Ask for less of the picture");
    require(editor.project().background.strength == 30, "Which a picture allows");
    const auto softened = drawn();
    require(softened != papered, "And shows through less");
    faded.strength = 0;
    require(editor.set_background(faded), "Down to nothing");
    require(ink(drawn()) == 0, "At no strength the picture is not drawn at all");

    // Back to the theme, and the dots that mark the grid return with it.
    require(editor.set_background(domain::Background{}), "Back to the plain canvas");
    require(ink(drawn()) == 0, "Which is plain again");
    view.set_grid_visible(true);
    require(ink(drawn()) > 0, "And the grid's own dots are drawn once more");
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

    // Transparency is a level over whatever colour the surface has, the
    // theme's own included. All the way leaves only the outline with the
    // canvas showing through; half way is a blend of the two.
    view.set_grid_visible(false);
    require(editor.recolour({domain::ElementRef{student}}, {}), "Back to the theme's colour");
    const auto below_label = [&] {
        return view.mapFromScene(find_node(view, "Student")->sceneBoundingRect().center() + QPointF(0, 26));
    };
    const auto seen = [&] {
        view.synchronize();
        QApplication::processEvents();
        const auto image = view.viewport()->grab().toImage();
        return image.pixelColor(device_point(image, below_label()));
    };
    const auto solid = seen();
    const auto canvas = desktop::theme(view.theme_id()).canvas;
    require(solid != canvas, "A solid surface differs from the canvas");
    require(editor.set_transparency({domain::ElementRef{student}}, 100), "Fade it all the way");
    require(seen() == canvas, "The canvas shows through a fully transparent element");
    require(editor.set_transparency({domain::ElementRef{student}}, 50), "Half way");
    const auto half = seen();
    require(half != canvas && half != solid, "Half way is a blend of the surface and the canvas");
    require(editor.set_transparency({domain::ElementRef{student}}, 0), "And back");
    require(editor.project().transparency.empty(), "None stores nothing");

    // The right-click menu carries a bar for it below the colours, which
    // previews as it moves and writes once when it is let go.
    view.select_elements({domain::ElementRef{student}});
    QTimer::singleShot(0, [&] {
        auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
        require(menu != nullptr, "A right-click opens a menu");
        auto* slider = menu->findChild<QSlider*>("contextTransparency");
        require(slider != nullptr, "It carries a transparency bar");
        const auto revision = editor.revision();
        slider->setSliderDown(true);
        slider->setValue(60);
        require(editor.revision() == revision, "Moving the bar previews without writing");
        slider->setSliderDown(false);
        emit slider->sliderReleased();
        require(editor.revision() == revision + 1, "Letting go writes once");
        menu->close();
    });
    const auto position = view.mapFromScene(find_node(view, "Student")->sceneBoundingRect().center());
    QContextMenuEvent event(QContextMenuEvent::Mouse, position, view.viewport()->mapToGlobal(position));
    QApplication::sendEvent(view.viewport(), &event);
    QApplication::processEvents();
    view.activateWindow();
    require(editor.project().transparency.at(domain::ElementRef{student}) == 60, "The bar's value is what was stored");
    require(editor.undo_label() == "Set transparency", "As one named edit");
    require(editor.undo(), "Undo it");
    view.synchronize();
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
        for (const auto style : {desktop::LineStyle::Curved, desktop::LineStyle::Straight, desktop::LineStyle::Elbow}) {
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

    // One side can be drawn bare while the other keeps its symbols, which is
    // how an ERD is often drawn when only one side is being made a point of.
    view.synchronize();
    QApplication::processEvents();
    const auto annotated = view.viewport()->grab().toImage();
    require(editor.show_participant_constraints(enrolled, side, false), "Draw this side bare");
    view.synchronize();
    QApplication::processEvents();
    require(view.viewport()->grab().toImage() != annotated, "The symbols come off the line");
    // The constraints are still there: this changed the diagram, not the model.
    require(constraints() == std::pair{domain::Cardinality::Many, domain::Participation::Partial},
            "A bare side keeps the constraints it holds");
    require(editor.undo_label() == "Hide constraints", "Named for what it did");
    require(editor.show_participant_constraints(enrolled, side, true), "Show them again");
    view.synchronize();
    QApplication::processEvents();
    require(view.viewport()->grab().toImage() == annotated, "And the line is drawn as it was");

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

void inheritance_deletion_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto person = std::get<domain::EntityId>(*editor.create_entity("Person", {0, 0, 160, 80}).created);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-260, 420, 160, 80}).created);
    const auto employee = std::get<domain::EntityId>(*editor.create_entity("Employee", {260, 420, 160, 80}).created);
    const auto isa = std::get<domain::SpecializationId>(
        *editor.create_specialization("IS A", {32, 200, 96, 74}, domain::Inheritance::Specialization).created);
    require(editor.set_supertype(isa, person), "Connect supertype for deletion");
    require(editor.attach_subtype(isa, student), "Connect first subtype for deletion");
    require(editor.attach_subtype(isa, employee), "Connect second subtype for deletion");
    const auto attribute = std::get<domain::AttributeId>(
        *editor.create_attribute("Name", {-260, 0, 130, 60}, person).created);
    const auto relationship = std::get<domain::RelationshipId>(
        *editor.create_relationship("Works", {440, 0, 180, 100}).created);
    require(editor.connect(relationship, person), "Connect participant for mixed deletion");
    const auto unrelated = *editor.create_entity("Unrelated", {600, 400, 160, 80}).created;
    require(editor.recolour({isa}, domain::Colour{255, 180, 100}), "Colour the ISA triangle");
    const auto original = editor.project();
    desktop::DiagramView view(editor);
    view.resize(1000, 700);
    view.show();
    view.fit_diagram();
    QApplication::processEvents();

    const auto restore = [&] {
        const auto deleted = editor.project();
        require(editor.undo(), "Undo selected links in one step");
        view.synchronize();
        require(editor.project() == original, "Undo restores exact graph, layout and colours");
        require(editor.redo(), "Redo selected links in one step");
        view.synchronize();
        require(editor.project() == deleted, "Redo restores the complete deletion");
        require(editor.undo(), "Restore deletion fixture");
        view.synchronize();
        view.scene()->clearSelection();
    };

    find_edge(view, QStringLiteral("Inheritance — supertype"))->setSelected(true);
    auto revision = editor.revision();
    key(view, Qt::Key_Delete);
    require(editor.revision() == revision + 1, "Deleting a supertype link is one command");
    require(!editor.project().specializations.at(isa).supertype, "Deleting a supertype link detaches it");
    require(editor.project().specializations.at(isa).subtypes.size() == 2, "Supertype detach preserves subtypes");
    require(editor.project().entities.size() == 4 && !blocks(editor.project()), "Link deletion preserves entities and validity");
    restore();

    find_edge(view, QStringLiteral("Inheritance — subtype"))->setSelected(true);
    key(view, Qt::Key_Backspace);
    require(editor.project().specializations.at(isa).subtypes.size() == 1, "Backspace detaches only the selected subtype link");
    require(editor.project().specializations.at(isa).supertype == person, "Subtype detach preserves supertype");
    require(editor.project().entities.size() == 4 && !blocks(editor.project()), "Subtype link deletion preserves entities");
    restore();

    // All edge kinds and a node share the same atomic delete operation.
    view.select_elements({unrelated});
    for (auto* item : view.scene()->items())
        if (item->zValue() < 0) item->setSelected(true);
    revision = editor.revision();
    key(view, Qt::Key_Delete);
    require(editor.revision() == revision + 1, "Mixed deletion is one history entry");
    require(!editor.project().specializations.at(isa).supertype
            && editor.project().specializations.at(isa).subtypes.empty(), "Mixed deletion detaches all ISA links");
    require(!editor.project().attributes.at(attribute).owner, "Mixed deletion detaches attribute ownership");
    require(editor.project().relationships.at(relationship).participants.empty(), "Mixed deletion detaches participant");
    require(!domain::exists(editor.project(), unrelated) && !blocks(editor.project()), "Mixed deletion removes only the selected node");
    restore();

    // Deleting a coloured supertype cascades to its triangle even when those
    // same links are also selected; it must not leave a colour behind.
    view.select_elements({person});
    for (auto* item : view.scene()->items())
        if (item->toolTip().startsWith(QStringLiteral("Inheritance —"))) item->setSelected(true);
    revision = editor.revision();
    key(view, Qt::Key_Delete);
    require(editor.revision() == revision + 1, "Cascade and selected links delete together");
    require(!editor.project().specializations.contains(isa), "Deleting the supertype removes its triangle");
    require(!editor.project().colours.contains(isa), "Cascade removes the triangle's colour");
    require(editor.project().entities.contains(student) && editor.project().entities.contains(employee)
            && !blocks(editor.project()), "Cascade preserves subtype entities and validity");
    restore();

    view.select_elements({isa});
    for (auto* item : view.scene()->items())
        if (item->toolTip().startsWith(QStringLiteral("Inheritance —"))) item->setSelected(true);
    key(view, Qt::Key_Delete);
    require(!editor.project().specializations.contains(isa), "Triangle and its selected links delete together");
    require(editor.project().entities.size() == 4 && !blocks(editor.project()), "Deleting a triangle preserves its entities");
    restore();
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
    require(view.line_style() == desktop::LineStyle::Elbow, "Lines break at right angles unless told otherwise");
    view.set_line_style(desktop::LineStyle::Curved);

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
// A picture of the diagram is of the diagram: it holds what was drawn, at the
// size and on the background that were asked for, and it carries the project
// inside it in the two formats that have somewhere to put one.
void picture_export_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto first = editor.create_entity("Student", {0, 0, 160, 80});
    const auto second = editor.create_entity("Course", {400, 300, 160, 80});
    require(first && second, "Picture fixture");
    const auto student = std::get<domain::EntityId>(*first.created);
    desktop::DiagramView view(editor);
    view.resize(800, 600);
    view.show();
    QApplication::processEvents();

    // Each extent covers what it says. The whole diagram holds both entities,
    // a selection holds only what is chosen, and neither is empty.
    const auto whole = desktop::picture_extent(view, desktop::PictureExtent::WholeDiagram);
    view.select_elements({domain::ElementRef{student}});
    const auto chosen = desktop::picture_extent(view, desktop::PictureExtent::Selection);
    require(!whole.isEmpty() && !chosen.isEmpty(), "A drawn diagram has something to take a picture of");
    require(whole.width() > chosen.width() && whole.contains(chosen),
            "A selection covers less of the diagram than the whole of it does");

    desktop::PictureOptions options;
    options.format = desktop::PictureFormat::Png;
    options.background = desktop::PictureBackground::White;
    options.margin = 10;
    QByteArray png;
    auto result = desktop::draw_picture(view, options, {}, png);
    require(result && !png.isEmpty(), "A PNG of the diagram is written");
    QImage drawn;
    require(drawn.loadFromData(png, "png"), "And is a PNG that loads");
    require(drawn.size() == result.pixels, "Whose size is the size that was reported");
    require(drawn.size() == QSize(qRound(whole.width() + 20), qRound(whole.height() + 20)),
            "The margin is added to the extent rather than taken out of it");

    // What is selected belongs to the editor, not to the picture. Exporting
    // with something chosen must draw exactly what exporting with nothing
    // chosen draws, and must leave the selection where it found it.
    QByteArray with_selection;
    require(desktop::draw_picture(view, options, {}, with_selection).ok, "A picture is drawn while something is selected");
    require(view.selected_elements() == std::vector<domain::ElementRef>{domain::ElementRef{student}},
            "Exporting gives the selection back afterwards");
    view.select_elements({});
    QByteArray without_selection;
    require(desktop::draw_picture(view, options, {}, without_selection).ok, "And again with nothing selected");
    require(with_selection == without_selection, "A picture never shows the selection rings of the editor that drew it");

    // The background is the one that was asked for, read off a corner the
    // diagram does not reach.
    options.background = desktop::PictureBackground::Transparent;
    require(desktop::draw_picture(view, options, {}, png) && drawn.loadFromData(png, "png"), "A transparent PNG");
    require(qAlpha(drawn.pixel(0, 0)) == 0, "Transparent leaves the corner empty");
    options.background = desktop::PictureBackground::ThemeColour;
    require(desktop::draw_picture(view, options, {}, png) && drawn.loadFromData(png, "png"), "A PNG on the canvas colour");
    require(QColor(drawn.pixel(0, 0)) == view.canvas_colour(), "The theme's own canvas colour is what it stands on");

    // Scale multiplies the picture without changing what is in it, and a size
    // beyond what ERDFlow will draw is refused rather than attempted.
    options.scale = 2;
    require(desktop::draw_picture(view, options, {}, png) && drawn.loadFromData(png, "png"), "A PNG at twice the size");
    require(drawn.width() == qRound((whole.width() + 20) * 2), "Scale multiplies the picture");
    options.scale = 40;
    require(!desktop::draw_picture(view, options, {}, png), "A picture too large to draw is refused");
    options.scale = 1;

    // The project rides inside the two formats that have somewhere to put it,
    // and comes back out as exactly the bytes that went in.
    const QByteArray payload = "{\"format\":\"erdflow\",\"format_version\":15}";
    result = desktop::draw_picture(view, options, payload, png);
    require(result && result.carried_project, "A PNG carries the project");
    require(desktop::payload_of_picture(png) == payload, "And gives back exactly the bytes it was given");
    require(drawn.loadFromData(png, "png"), "A PNG carrying a project is still a PNG");

    options.format = desktop::PictureFormat::Svg;
    QByteArray svg;
    result = desktop::draw_picture(view, options, payload, svg);
    require(result && result.carried_project, "An SVG carries the project");
    require(desktop::payload_of_picture(svg) == payload, "And gives back exactly the bytes it was given");
    require(svg.contains("<metadata>") && svg.contains("</svg>"), "In the metadata element, inside a complete SVG");
    // An SVG has to name a font the machine reading it can resolve. Qt's own
    // "Sans Serif" is not a family any CSS engine knows, so it must not survive
    // into a file meant to be opened somewhere else.
    require(!svg.contains("font-family=\"Sans Serif\""), "No Qt font alias survives into an exported SVG");
    require(svg.contains("sans-serif"), "The text names a chain ending in a generic family instead");
    QXmlStreamReader reader(svg);
    while (!reader.atEnd()) reader.readNext();
    require(!reader.hasError(), "An SVG carrying a project is still well-formed XML");
    // Well-formed is not the same as readable. The project riding inside must
    // leave a picture that still draws, or the file is no use to the recipient
    // it was sent to, which is the whole reason for carrying it there.
    {
        QSvgRenderer renderer(svg);
        require(renderer.isValid(), "And still an SVG a renderer will draw");
        QImage sheet(240, 200, QImage::Format_ARGB32_Premultiplied);
        sheet.fill(Qt::white);
        QPainter onto(&sheet);
        renderer.render(&onto);
        onto.end();
        bool inked = false;
        for (int y = 0; y < sheet.height() && !inked; ++y)
            for (int x = 0; x < sheet.width() && !inked; ++x)
                if (QColor(sheet.pixel(x, y)) != QColor(Qt::white)) inked = true;
        require(inked, "That draws the diagram rather than an empty sheet");
    }

    // Asked to carry nothing, it carries nothing, and says so rather than
    // leaving the caller to guess.
    options.carry_project = false;
    require(desktop::draw_picture(view, options, payload, svg).ok, "An SVG written without the project");
    require(desktop::payload_of_picture(svg).isEmpty(), "Carries nothing");
    require(QSvgRenderer(svg).isValid(), "And is the same valid picture without it");
    options.carry_project = true;
    result = desktop::draw_picture(view, options, {}, svg);
    require(result && !result.carried_project && !result.carried_note.isEmpty(),
            "A picture written without a project to carry says so plainly");

    // A lossy or niche format carries nothing even when asked, because a file
    // that looks like it holds the project and does not is worse than one that
    // never claimed to.
    if (desktop::picture_format_available(desktop::PictureFormat::Jpeg)) {
        options.format = desktop::PictureFormat::Jpeg;
        QByteArray jpeg;
        result = desktop::draw_picture(view, options, payload, jpeg);
        require(result && !result.carried_project, "JPEG carries no project");
        require(desktop::payload_of_picture(jpeg).isEmpty(), "And nothing is found inside one");
        QImage photograph;
        require(photograph.loadFromData(jpeg, "jpeg"), "But it is a JPEG");
        require(qAlpha(photograph.pixel(0, 0)) == 255,
                "Asked for transparency it cannot keep, it is given paper rather than whatever the encoder leaves");
    }

    // A page is a page: measured in dots per inch rather than pixels.
    options.format = desktop::PictureFormat::Pdf;
    QByteArray pdf;
    require(desktop::draw_picture(view, options, payload, pdf).ok, "A PDF page of the diagram");
    require(pdf.startsWith("%PDF"), "Which is a PDF");
    require(desktop::payload_of_picture(pdf).isEmpty(), "Carrying no project, as a page cannot");

    // An ordinary picture that ERDFlow did not write is an ordinary picture,
    // not an error.
    require(desktop::payload_of_picture("not a picture at all").isEmpty(), "Bytes that are no picture carry nothing");
    require(desktop::payload_of_picture("<svg xmlns=\"http://www.w3.org/2000/svg\"/>").isEmpty(),
            "An SVG from somewhere else carries nothing");

    // Nothing selected is nothing to export, and it is refused with a reason
    // rather than written as an empty file.
    view.select_elements({});
    options.format = desktop::PictureFormat::Png;
    options.extent = desktop::PictureExtent::Selection;
    result = desktop::draw_picture(view, options, {}, png);
    require(!result && !result.error.isEmpty(), "A picture of nothing is refused, with a reason");
    editor.new_project();
    view.synchronize();
    options.extent = desktop::PictureExtent::WholeDiagram;
    require(!desktop::draw_picture(view, options, {}, png), "And so is a picture of an empty diagram");
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

        // Two fingers travelling together move the diagram rather than zoom
        // it. A trackpad reports the pixels they covered and gives its scroll
        // a phase, and it fills in a wheel's angle as well, so the angle alone
        // must not be taken for a wheel notch.
        view.actual_size();
        view.centerOn(0, 0);
        QApplication::processEvents();
        {
            const auto before = view.zoom_factor();
            const auto down = view.verticalScrollBar()->value();
            const auto across = view.horizontalScrollBar()->value();
            const QPointF at(300, 200);
            QWheelEvent together(at, view.viewport()->mapToGlobal(at.toPoint()), QPoint(-30, -45), QPoint(-120, -180),
                                 Qt::NoButton, Qt::NoModifier, Qt::ScrollUpdate, false);
            QApplication::sendEvent(view.viewport(), &together);
            QApplication::processEvents();
            require(view.zoom_factor() == before, "Two fingers together do not zoom");
            require(view.verticalScrollBar()->value() == down + 45
                        && view.horizontalScrollBar()->value() == across + 30,
                    "They move the diagram under them, both ways");

            // A mouse wheel has neither pixels nor a phase, and still zooms.
            QWheelEvent notch(at, view.viewport()->mapToGlobal(at.toPoint()), QPoint(), QPoint(0, 120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(view.viewport(), &notch);
            QApplication::processEvents();
            require(view.zoom_factor() > before, "A wheel notch still zooms in");
        }

        // A trackpad pinch zooms as the wheel does, in and out, within the same bounds.
        view.actual_size();
        const auto pinch = [&](qreal amount) {
            QNativeGestureEvent gesture(Qt::ZoomNativeGesture, QPointingDevice::primaryPointingDevice(), 2,
                                        QPointF(300, 200), QPointF(300, 200), QPointF(300, 200), amount, QPointF());
            QApplication::sendEvent(view.viewport(), &gesture);
            QApplication::processEvents();
        };
        pinch(0.5);
        require(std::abs(view.zoom_factor() - 1.5) < 0.0001, "Spreading two fingers zooms in");
        pinch(-0.5);
        require(std::abs(view.zoom_factor() - 0.75) < 0.0001, "Pinching them zooms out");
        for (int i = 0; i < 50; ++i) pinch(1.0);
        require(std::abs(view.zoom_factor() - 3.0) < 0.0001, "A pinch is bounded like the wheel");
        view.actual_size();
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
        entity_to_entity_creates_a_relationship_tests();
        lock_connector_tests();
        connector_route_tests();
        lock_participant_tests();
        join_where_clicked_tests();
        figure_tests();
        symbol_resize_tests();
        recursive_loop_tests();
        alignment_tests();
        lock_selection_tests();
        background_tests();
        element_shape_preview_tests();
        weak_and_identifying_drawing_tests();
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
        inheritance_deletion_tests();
        inheritance_orientation_tests();
        picture_export_tests();
        std::cout << "Canvas tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Canvas test failed: " << exception.what() << '\n';
        return 1;
    }
    return 0;
}
