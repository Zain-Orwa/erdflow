// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/diagram_view.hpp"
#include "app/desktop/document_export.hpp"
#include "app/desktop/edge_scroll.hpp"
#include "app/desktop/picture_export.hpp"
#include "domain/schema_preview.hpp"
#include "infrastructure/project_store.hpp"

#include <QApplication>
#include <QBuffer>
#include <QElapsedTimer>
#include <QGraphicsItem>
#include <QGraphicsScene>
#include <QImage>
#include <QKeyEvent>
#include <QLineEdit>
#include <QContextMenuEvent>
#include <QMenu>
#include <QMessageBox>
#include <QAbstractButton>
#include <QMouseEvent>
#include <QNativeGestureEvent>
#include <QScrollBar>
#include <QSlider>
#include <QTimer>
#include <QPainter>
#include <QPainterPathStroker>
#include <QStaticText>
#include <QTemporaryDir>
#include <QPlainTextEdit>
#include <QFocusEvent>
#include <QPointingDevice>
#include <QWheelEvent>
#include <QSvgRenderer>
#include <QXmlStreamReader>

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <iostream>
#include <map>
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
        if (item->zValue() > 0 && item->data(Qt::DisplayRole).toString() == name) return item;
    throw std::runtime_error("Missing node");
}
// A name too long for one line is written in up to three (2026-10-12). What a
// shape paints is the layout it is given, line for line; and the layout keeps
// to the rules: no more than three lines, each within the room its shape
// leaves a name and inside the shape's outline, nothing of the name lost
// unless the last line says so, and the whole name on hover exactly when it is.
void require_written_in_lines(desktop::DiagramView& view, const application::Editor& editor,
                              const domain::ElementRef& ref, desktop::NamedShape shape, bool cut_short) {
    view.synchronize();
    QApplication::processEvents();
    const auto name = QString::fromStdString(domain::name(editor.project(), ref));
    const auto box = editor.project().layout.at(ref);
    const auto base = editor.project().lettering.find(ref);
    const auto factor = base == editor.project().lettering.end() ? 1.0
        : domain::lettering_factor(base->second, box.width, box.height);
    const QRectF bounds(0, 0, box.width, box.height);
    const auto laid = desktop::name_layout(shape, name, view.font(), factor, bounds);
    require(laid.elided == cut_short, cut_short ? "Too long even for three lines, it is cut short on the last"
                                                : "Written whole, in as many lines as it takes");
    require(!laid.lines.empty() && laid.lines.size() <= 3 && laid.lines.size() == laid.rects.size(),
            "In no more than three lines");
    QFont font = view.font();
    font.setFamilies({"Inter", "Source Sans 3", "IBM Plex Sans", "Segoe UI Variable",
                      "Segoe UI", "SF Pro Text", "Helvetica Neue", "Arial"});
    font.setStyleHint(QFont::SansSerif, QFont::PreferAntialias);
    font.setPointSizeF(9.0 * desktop::lettering_scale * factor);
    font.setWeight(QFont::Bold);
    const QFontMetricsF metrics(font);
    const bool hierarchy = shape == desktop::NamedShape::Generalization || shape == desktop::NamedShape::Specialization;
    for (std::size_t i = 0; i < laid.lines.size(); ++i) {
        const auto& rect = laid.rects[i];
        const auto wide = metrics.horizontalAdvance(laid.lines[i]);
        require(wide <= rect.width() + 0.5, "Each line fits the room it is given");
        require(rect.top() >= -0.5 && rect.bottom() <= box.height + 0.5, "Every line is within the shape");
        if (shape == desktop::NamedShape::Entity || shape == desktop::NamedShape::Attribute)
            require(rect.center().x() - wide / 2 >= 15 - 0.5 && rect.center().x() + wide / 2 <= box.width - 15 + 0.5,
                    "15 in from a box's or an oval's sides");
        if (shape == desktop::NamedShape::Generalization)
            require(rect.top() >= box.height / 2 - 0.5, "Under a generalisation's middle, where it is wide");
        if (shape == desktop::NamedShape::Specialization)
            require(rect.bottom() <= box.height / 2 + 0.5, "Above a specialisation's middle, where it is wide");
        if (!hierarchy && laid.lines.size() > 1) {
            // The letters' corners, from the capitals down to the baseline.
            const auto baseline = rect.top() + (rect.height() - metrics.height()) / 2 + metrics.ascent();
            for (const auto y : {baseline, baseline - metrics.capHeight()})
                for (const auto x : {rect.center().x() - wide / 2, rect.center().x() + wide / 2}) {
                    const auto dx = std::abs(x - box.width / 2);
                    const auto dy = std::abs(y - box.height / 2);
                    if (shape == desktop::NamedShape::Relationship)
                        require(dx / (box.width / 2) + dy / (box.height / 2) <= 1.0 + 1e-6,
                                "Every line is inside the diamond");
                    if (shape == desktop::NamedShape::Attribute)
                        require((dx * dx) / ((box.width / 2 - 5) * (box.width / 2 - 5))
                                    + (dy * dy) / ((box.height / 2 - 5) * (box.height / 2 - 5)) <= 1.0 + 1e-6,
                                "Every line is inside the oval's second ring");
                }
        }
    }
    QString joined;
    for (const auto& line : laid.lines) joined += line;
    joined.remove(QLatin1Char(' '));
    auto whole = name;
    whole.remove(QLatin1Char(' '));
    if (!cut_short) require(joined == whole, "Nothing of the name is lost between its lines");
    else {
        require(joined.endsWith(QChar(0x2026)), "The last line says there is more");
        joined.chop(1);
        require(whole.startsWith(joined), "What is written is the name's own beginning");
    }
    // Painted exactly as laid out.
    auto* node = find_node(view, name);
    QImage actual((node->boundingRect().size().toSize() + QSize(8, 8)) * 4, QImage::Format_ARGB32);
    actual.setDevicePixelRatio(4);
    actual.fill(Qt::transparent);
    QPainter painter(&actual);
    painter.setFont(view.font());
    node->paint(&painter, nullptr, nullptr);
    painter.end();
    QImage reference(actual.size(), QImage::Format_ARGB32);
    reference.setDevicePixelRatio(4);
    reference.fill(Qt::transparent);
    QPainter writer(&reference);
    writer.setRenderHint(QPainter::Antialiasing);
    writer.setFont(font);
    const auto ink = desktop::theme(view.theme_id()).node_text;
    writer.setPen(ink);
    for (std::size_t i = 0; i < laid.lines.size(); ++i) writer.drawText(laid.rects[i], Qt::AlignCenter, laid.lines[i]);
    writer.end();
    // A hierarchy also writes its d or o in the same ink, in a corner, so
    // only the name's own lines are compared there.
    const auto ink_of = [&](const QImage& image) {
        std::vector<QPoint> dots;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x) {
                if (image.pixelColor(x, y) != ink) continue;
                const QPointF at((x + 0.5) / 4, (y + 0.5) / 4);
                if (hierarchy && std::none_of(laid.rects.begin(), laid.rects.end(),
                                              [&](const QRectF& rect) { return rect.contains(at); })) continue;
                dots.emplace_back(x, y);
            }
        return dots;
    };
    const auto painted = ink_of(actual);
    require(!painted.empty() && painted == ink_of(reference), "The shape paints its name exactly as it is laid out");
    if (node->toolTip().isEmpty() || node->toolTip() == name)
        require((node->toolTip() == name) == cut_short, "The whole name is offered on hover exactly when it is cut short");
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

    // Both lines start unlocked (Zain, 2026-09-26): neither end is pinned to
    // where its entity was clicked, so each slides round its shapes as they
    // move, until a hand locks it.
    require(editor.project().connectors.empty(), "Neither line is pinned, at either end");

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
        // Centre to centre: the first was widened for its name, about its
        // centre, so its corner is no longer where the second's is measured from.
        const QPointF stepped(second_body.x + second_body.width / 2 - (first_body.x + first_body.width / 2),
                              second_body.y + second_body.height / 2 - (first_body.y + first_body.height / 2));
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
    // The middle of the side facing a point, each axis against its own half.
    const auto middle_toward = [&](const QPointF& target) {
        const auto across = (target.x() - body.center().x()) / (body.width() / 2);
        const auto down = (target.y() - body.center().y()) / (body.height() / 2);
        if (std::abs(down) >= std::abs(across))
            return QPointF(body.center().x(), down < 0 ? body.top() : body.bottom());
        return QPointF(across < 0 ? body.left() : body.right(), body.center().y());
    };

    // Unlocked, the join is the middle of the side facing the attribute
    // (Zain, 2026-09-26), and stays exactly there while the attribute moves
    // about on that side: only the attribute's end of the line moves.
    const auto pinned_at = middle_toward(attribute_centre());
    require(meets(pinned_at), "An unlocked join sits at the middle of the side facing the attribute");
    require(editor.move({{domain::ElementRef{born}, {-90, -200, 130, 54}}}), "Move the attribute along that side");
    view.synchronize();
    QApplication::processEvents();
    require(middle_toward(attribute_centre()) == pinned_at && meets(pinned_at),
            "Moving it along the same side leaves the join where it was");
    require(editor.undo(), "Put it back");
    view.synchronize();
    QApplication::processEvents();

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
    require(meets(middle_toward(attribute_centre())) && middle_toward(attribute_centre()) != pinned_at,
            "Unlocked again, the join is the middle of the side the attribute now faces");

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
    // New lines are not pinned where they are clicked (Zain, 2026-09-26); the
    // window no longer offers it. The canvas can still pin a join at a point,
    // which is what follows here, so it is asked for.
    require(view.join_mode() == desktop::JoinMode::Automatic, "New lines are not pinned where they are clicked");
    view.set_join_mode(desktop::JoinMode::WhereClicked);

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
void free_text_canvas_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    desktop::DiagramView view(editor);
    view.resize(1000, 700); view.show(); view.actual_size(); view.centerOn(0, 0);
    view.set_grid_visible(false);
    QApplication::processEvents();
    view.set_tool(desktop::Tool::Text);
    click(view, {-250, -100});
    require(editor.project().notes.size() == 1 && view.tool() == desktop::Tool::Select, "Text placement is one use");
    auto* field = view.findChild<QPlainTextEdit*>("inlineText");
    require(field && field->isVisible() && field->toPlainText().isEmpty(), "Placement opens empty multiline editor immediately");
    const auto id = editor.project().notes.begin()->first;
    field->setPlainText("Customer data imported from CRM\nChecked weekly");
    view.commit_rename();
    QApplication::processEvents();
    require(editor.project().notes.at(id).description == "Customer data imported from CRM\nChecked weekly", "Inline edit stores prose");
    require(editor.project().notes.at(id).name.empty(), "Text has no title");
    const auto box = editor.project().layout.at(id);
    require(box.x == -250 && box.y == -100 && box.width > 200 && box.height > 30, "Natural multiline bounds preserve origin");
    view.select_elements({id});
    auto* item = view.scene()->selectedItems().front();
    require(item->shape().contains(QPointF(4, 4)), "Padded text hit area is selectable");
    view.select_elements({});
    const auto image = [&](bool selected) {
        item->setSelected(selected);
        QImage shot(int(std::ceil(box.width)) + 8, int(std::ceil(box.height)) + 8, QImage::Format_ARGB32_Premultiplied);
        shot.fill(Qt::transparent);
        QPainter painter(&shot); painter.setRenderHint(QPainter::Antialiasing); painter.translate(4, 4);
        item->paint(&painter, nullptr, nullptr);
        return shot;
    };
    for (const auto theme : {desktop::ThemeId::OfficeLight, desktop::ThemeId::Midnight}) {
        view.set_theme(theme);
        auto expected = image(false); expected.fill(Qt::transparent);
        QPainter painter(&expected); painter.setRenderHint(QPainter::Antialiasing); painter.translate(4, 4);
        QFont font = view.font();
        font.setFamilies({"Inter", "Source Sans 3", "IBM Plex Sans", "Segoe UI Variable", "Segoe UI", "SF Pro Text", "Helvetica Neue", "Arial"});
        font.setStyleHint(QFont::SansSerif, QFont::PreferAntialias); font.setPointSizeF(11 * desktop::lettering_scale);
        font.setWeight(QFont::Normal); font.setUnderline(false);
        painter.setFont(font); painter.setPen(desktop::theme(theme).node_text);
        const QFontMetricsF metrics(font); qreal y = 4 + metrics.ascent();
        for (const auto& line : QString::fromStdString(editor.project().notes.at(id).description).split('\n')) {
            painter.drawText(QPointF(4, y), line); y += metrics.lineSpacing();
        }
        painter.end();
        require(image(false) == expected, "Unselected Text paints only the exact prose, without fill, border, icon or badge");
        require(image(true) != expected, "Selected Text has a temporary indication");
        item->setSelected(false);
    }
    mouse(view, QEvent::MouseButtonDblClick, view.mapFromScene(QPointF(box.x + 20, box.y + 10)), Qt::LeftButton, Qt::LeftButton);
    require(field->isVisible(), "Double-click opens the multiline Text editor");
    field->setPlainText("Discarded"); key_to(field, Qt::Key_Escape);
    require(editor.project().notes.at(id).description.find("Customer") == 0, "Escape restores existing prose");
    view.begin_rename(id); field->setPlainText("Edited\nMultiline"); view.commit_rename();
    require(editor.undo(), "Undo text edit"); view.synchronize();
    require(editor.project().notes.at(id).description.find("Customer") == 0, "Undo restores text and bounds");
    require(editor.redo(), "Redo text edit"); view.synchronize();
    view.begin_rename(id); field->setPlainText("Focus loss\nMultiline");
    QFocusEvent leave(QEvent::FocusOut); QApplication::sendEvent(field, &leave);
    require(editor.project().notes.at(id).description == "Focus loss\nMultiline" && !field->isVisible(), "Focus loss commits multiline prose");
    require(editor.undo(), "Undo focus-loss edit"); view.synchronize();
    view.select_elements({id});
    const auto start = view.mapFromScene(QPointF(box.x + 20, box.y + 10));
    mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, start + QPoint(35, 25), Qt::NoButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, start + QPoint(35, 25), Qt::LeftButton, Qt::NoButton);
    require(editor.project().layout.at(id).x != box.x, "Text drag persists movement");
    require(editor.undo() && editor.redo(), "Movement undo/redo"); view.synchronize();
    desktop::PictureOptions options; options.format = desktop::PictureFormat::Png;
    QByteArray png; require(desktop::draw_picture(view, options, {}, png).ok && !png.isEmpty(), "Annotation-only visual export works");
    options.background = desktop::PictureBackground::Transparent;
    require(desktop::draw_picture(view, options, {}, png).ok, "Transparent Text export");
    const auto pixels = QImage::fromData(png);
    bool ink = false, paper = false;
    for (int y = 0; y < pixels.height(); ++y) for (int x = 0; x < pixels.width(); ++x) {
        ink |= qAlpha(pixels.pixel(x, y)) > 0;
        paper |= qAlpha(pixels.pixel(x, y)) == 0;
    }
    require(ink && paper, "Visual export includes glyphs and transparent surrounding paper");
    for (const auto format : {desktop::PictureFormat::Svg, desktop::PictureFormat::Jpeg,
                             desktop::PictureFormat::Tiff, desktop::PictureFormat::Pdf}) {
        if (!desktop::picture_format_available(format)) continue;
        options.format = format;
        QByteArray bytes;
        require(desktop::draw_picture(view, options, {}, bytes).ok && !bytes.isEmpty(), "Text exports through shared visual formats");
        if (format == desktop::PictureFormat::Svg) require(bytes.contains("Edited") && bytes.contains("Multiline"), "SVG contains Text prose");
    }
    const auto entity = editor.create_entity("Customer", {100, 100, 133, 77});
    const auto entity_ref = *entity.created;
    view.synchronize(); view.select_elements({id, entity_ref}); view.set_align_to_grid(true);
    const auto text_start = editor.project().layout.at(id), entity_start = editor.project().layout.at(entity_ref);
    const auto group_start = view.mapFromScene(QPointF(text_start.x + 20, text_start.y + 10));
    mouse(view, QEvent::MouseButtonPress, group_start, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, group_start + QPoint(47, 32), Qt::NoButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, group_start + QPoint(47, 32), Qt::LeftButton, Qt::NoButton);
    const auto text_moved = editor.project().layout.at(id), entity_moved = editor.project().layout.at(entity_ref);
    require(text_moved.x != text_start.x && text_moved.x - text_start.x == entity_moved.x - entity_start.x
            && text_moved.y - text_start.y == entity_moved.y - entity_start.y, "Text participates in group movement");
    require(std::fmod(text_moved.x, 20) == 0 && std::fmod(text_moved.y, 20) == 0, "Text movement follows grid alignment");
    require(editor.undo() && editor.project().layout.at(id) == text_start
            && editor.project().layout.at(entity_ref) == entity_start, "One undo restores the whole group");
    require(editor.redo(), "Redo Text group move"); view.synchronize(); view.set_align_to_grid(false);
    const auto duplicate = editor.duplicate({id}); require(duplicate && duplicate.created, "Duplicate Text"); view.synchronize();
    const auto copy = std::get<domain::NoteId>(*duplicate.created);
    require(copy != id && editor.project().notes.at(copy).free_text
            && editor.project().notes.at(copy).description == editor.project().notes.at(id).description, "Duplicate preserves subtype and prose with fresh ID");
    view.select_elements({copy}); view.delete_selection();
    require(!editor.project().notes.contains(copy), "Delete selected Text");
    require(editor.undo() && editor.project().notes.contains(copy), "Undo Text deletion");
    require(editor.redo() && !editor.project().notes.contains(copy), "Redo Text deletion"); view.synchronize();
    view.synchronize(); view.set_tool(desktop::Tool::Connect);
    const auto revision = editor.revision();
    click(view, {editor.project().layout.at(id).x + 20, editor.project().layout.at(id).y + 10});
    const auto customer = editor.project().layout.at(entity_ref);
    click(view, {customer.x + customer.width / 2, customer.y + customer.height / 2});
    require(editor.revision() == revision && editor.project().relationships.empty(), "Connect cannot attach Text to entities");
    const auto identifying = editor.create_relationship("Has", {300, -150, 171, 99});
    const auto relationship = std::get<domain::RelationshipId>(*identifying.created);
    require(editor.set_relationship_kind(relationship, domain::RelationshipKind::Identifying), "Identifying relationship fixture");
    view.synchronize(); view.set_tool(desktop::Tool::Connect);
    const auto identifying_revision = editor.revision();
    click(view, {385, -100});
    click(view, {editor.project().layout.at(id).x + 20, editor.project().layout.at(id).y + 10});
    require(editor.revision() == identifying_revision && editor.project().relationships.at(relationship).participants.empty(),
            "Text cannot be an identifying relationship participant");
    const auto attribute = editor.create_attribute("Code", {200, -300, 135, 54});
    const auto attribute_id = std::get<domain::AttributeId>(*attribute.created);
    view.synchronize(); view.set_tool(desktop::Tool::Connect);
    const auto ownership_revision = editor.revision();
    click(view, {267, -273});
    click(view, {editor.project().layout.at(id).x + 20, editor.project().layout.at(id).y + 10});
    require(editor.revision() == ownership_revision && !editor.project().attributes.at(attribute_id).owner,
            "Text cannot own attributes through Connect");
    QByteArray before, after, csv_before, csv_after;
    require(desktop::draw_document(view, editor.project(), desktop::DocumentFormat::Markdown, before).ok, "Model listing with Text");
    require(desktop::draw_document(view, editor.project(), desktop::DocumentFormat::Csv, csv_before).ok, "CSV listing with Text");
    require(editor.erase({id}), "Delete Text"); view.synchronize();
    require(desktop::draw_document(view, editor.project(), desktop::DocumentFormat::Markdown, after).ok && before == after,
            "Structured listing ignores Text");
    require(desktop::draw_document(view, editor.project(), desktop::DocumentFormat::Csv, csv_after).ok && csv_before == csv_after,
            "Structured CSV ignores Text");
    require(editor.undo(), "Restore Text"); view.synchronize();
    view.set_tool(desktop::Tool::Text); click(view, {-350, 180});
    require(editor.project().notes.size() == 2, "Second placement begins");
    key_to(field, Qt::Key_Escape);
    require(editor.project().notes.size() == 1, "Cancelled placement leaves no placeholder object");
    view.set_tool(desktop::Tool::Text, true); click(view, {-350, 180});
    field->setPlainText("Locked placement"); view.commit_rename();
    require(view.tool() == desktop::Tool::Text && view.tool_locked(), "Locked Text placement keeps its existing repeat mode");
    key_to(view.viewport(), Qt::Key_Escape);
    require(view.tool() == desktop::Tool::Select && !view.tool_locked(), "Escape releases locked Text placement");
    (void)entity;
}

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

    // The titled Note is gone (Zain, 2026-10-15): there is no Note tool, and
    // the note put beside the model is free text.
    const auto made = editor.create_free_text("Assumptions\nEach student enrols each term.", {150, 120, 200, 60});
    require(made && made.created, "Free text is placed beside the model");
    const domain::ElementRef note = *made.created;
    view.synchronize();
    QApplication::processEvents();
    const auto note_centre = [&] {
        const auto& at = editor.project().layout.at(note);
        return QPointF(at.x + at.width / 2, at.y + at.height / 2);
    };


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
    click(view, note_centre());
    click(view, find_node(view, "Student")->sceneBoundingRect().center());
    require(editor.revision() == revision, "Text and an entity do not connect");
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
    // What a right-click's Insert offers: Text and Picture, and no Note.
    const auto offered = [&](const QPointF& scene_point) {
        QStringList names;
        QTimer::singleShot(0, [&] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            require(menu != nullptr, "A right-click opens a menu");
            for (auto* action : menu->findChildren<QAction*>()) names << action->objectName();
            menu->close();
        });
        const auto position = view.mapFromScene(scene_point);
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, view.viewport()->mapToGlobal(position));
        QApplication::sendEvent(view.viewport(), &event);
        QApplication::processEvents();
        view.activateWindow();
        return names;
    };
    const auto on_canvas = offered(QPointF(-300, 200));
    require(on_canvas.contains("contextInsertText") && on_canvas.contains("contextInsertPicture"),
            "Insert on the canvas offers Text and Picture");
    require(!on_canvas.contains("contextInsertNote"), "And no Note");
    (void)opened_for_title;
    right_click(QPointF(120, -80), "contextInsertPicture");
    require(asked_for == QPointF(120, -80), "Insert → Picture asks the window for a file, for that point");
    // The same entries sit at the end of an element's own menu.
    const auto on_element = offered(find_node(view, "Student")->sceneBoundingRect().center());
    require(on_element.contains("contextInsertText") && !on_element.contains("contextInsertNote"),
            "An element's menu offers Insert as well, without a Note");
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
        // The titled card is gone (2026-10-15), so nothing on the diagram is
        // drawn on one any longer; the colour is still told apart where it is.
        QImage swatch(8, 8, QImage::Format_RGB32);
        swatch.fill(surface);
        require(carded(swatch) > 0, "The card's colour is recognised where it is drawn");
        require(carded(shot()) == 0, "And nothing on the diagram is drawn on a card");
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

// A total participation is drawn as a double line in every notation (Zain,
// 2026-10-03): two lines of one weight, a narrow gap apart, either side of
// where the single line runs. A partial one stays the single line it was.
void total_participation_drawing_tests(desktop::MinMaxDisplayMode mode = desktop::MinMaxDisplayMode::CardinalityAndOrdinality) {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-520, -43, 148, 86}).created);
    const auto course = std::get<domain::EntityId>(*editor.create_entity("Course", {372, -43, 148, 86}).created);
    const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {-95, -55, 190, 110}).created);
    const auto side = *editor.connect(enrolled, student).participant;
    const auto other = *editor.connect(enrolled, course).participant;
    require(editor.update_participant(enrolled, side, domain::Cardinality::Many, domain::Participation::Partial, ""), "Student's side partial");
    require(editor.update_participant(enrolled, other, domain::Cardinality::Many, domain::Participation::Partial, ""), "Course's side partial");

    desktop::DiagramView view(editor);
    view.resize(1200, 500);
    view.set_minmax_display_mode(mode);
    view.show();
    view.set_grid_visible(false);
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();
    // The dark runs met going straight across Student's line, between the
    // end symbols and the diamond: where each starts and how long it is.
    const auto runs_across = [&] {
        view.synchronize();
        QApplication::processEvents();
        const auto image = view.viewport()->grab().toImage();
        const auto at = device_point(image, view.mapFromScene(QPointF(-150, 0)));
        const auto reach = static_cast<int>(14 * image.devicePixelRatio());
        std::vector<std::pair<int, int>> runs;
        for (int dy = -reach; dy <= reach; ++dy) {
            const bool dark = QColor(image.pixel(at.x(), at.y() + dy)).lightness() < 160;
            if (!dark) continue;
            if (!runs.empty() && runs.back().first + runs.back().second == dy) ++runs.back().second;
            else runs.emplace_back(dy, 1);
        }
        return std::pair{runs, image.devicePixelRatio()};
    };
    const desktop::Notation notations[] = {desktop::Notation::CrowsFoot, desktop::Notation::Chen,
                                           desktop::Notation::MinMax, desktop::Notation::Bachman};
    std::vector<double> single_centres;
    for (const auto notation : notations) {
        view.set_notation(notation);
        const auto [runs, ratio] = runs_across();
        (void)ratio;
        require(runs.size() == 1, "A partial side is one line, in every notation");
        single_centres.push_back(runs.front().first + runs.front().second / 2.0);
    }
    require(editor.update_participant(enrolled, side, domain::Cardinality::Many, domain::Participation::Total, ""), "Student's side total");
    for (std::size_t n = 0; n < std::size(notations); ++n) {
        view.set_notation(notations[n]);
        const auto [runs, ratio] = runs_across();
        require(runs.size() == 2, "A total side is two lines, in every notation");
        const auto [upper, upper_length] = runs[0];
        const auto [lower, lower_length] = runs[1];
        require(std::abs(upper_length - lower_length) <= 1, "Both lines are as heavy as each other");
        const auto gap = lower - (upper + upper_length);
        require(gap >= 1 && gap <= 3 * ratio, "With only a narrow gap between them");
        const auto centre = (upper + lower + lower_length) / 2.0;
        require(std::abs(centre - single_centres[n]) <= 1, "Either side of where the single line ran");
    }
    // The other side, still partial, is the single line it was.
    view.set_notation(desktop::Notation::CrowsFoot);
    view.synchronize();
    QApplication::processEvents();
    const auto image = view.viewport()->grab().toImage();
    const auto at = device_point(image, view.mapFromScene(QPointF(150, 0)));
    int dark_runs = 0;
    bool inside = false;
    for (int dy = -static_cast<int>(14 * image.devicePixelRatio()); dy <= static_cast<int>(14 * image.devicePixelRatio()); ++dy) {
        const bool dark = QColor(image.pixel(at.x(), at.y() + dy)).lightness() < 160;
        if (dark && !inside) ++dark_runs;
        inside = dark;
    }
    require(dark_runs == 1, "Course's partial side is still one line");
}

// Chen's and min-max's number is written at the entity's end (Zain,
// 2026-10-03): just off the entity, above a level line or to the right of a
// standing one, and clear of the line -- of both lines where a total
// participation draws two, so neither is broken under it.
void cardinality_labels_sit_at_the_entity_end_tests() {
    const auto dark_in = [](desktop::DiagramView& view, const QImage& image, const QRectF& scene) {
        const auto top_left = device_point(image, view.mapFromScene(scene.topLeft()));
        const auto bottom_right = device_point(image, view.mapFromScene(scene.bottomRight()));
        int found = 0;
        for (int y = top_left.y(); y <= bottom_right.y(); ++y)
            for (int x = top_left.x(); x <= bottom_right.x(); ++x)
                if (QColor(image.pixel(x, y)).lightness() < 160) ++found;
        return found;
    };
    const auto runs_down = [](desktop::DiagramView& view, const QImage& image, QPointF at) {
        const auto point = device_point(image, view.mapFromScene(at));
        // Wide enough for both lines of a total side, short of the number.
        const auto reach = static_cast<int>(8 * image.devicePixelRatio());
        int runs = 0;
        bool inside = false;
        for (int dy = -reach; dy <= reach; ++dy) {
            const bool dark = QColor(image.pixel(point.x(), point.y() + dy)).lightness() < 160;
            if (dark && !inside) ++runs;
            inside = dark;
        }
        return runs;
    };
    for (const auto notation : {desktop::Notation::Chen, desktop::Notation::MinMax}) {
        const auto wide = notation == desktop::Notation::MinMax ? 72.0 : 42.0;
        // Level: Student, total and many, on the left; Course, partial and one, on the right.
        {
            SequentialIds ids;
            application::Editor editor(ids);
            const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-520, -43, 148, 86}).created);
            const auto course = std::get<domain::EntityId>(*editor.create_entity("Course", {372, -43, 148, 86}).created);
            const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {-95, -55, 190, 110}).created);
            const auto left = *editor.connect(enrolled, student).participant;
            const auto right = *editor.connect(enrolled, course).participant;
            require(editor.update_participant(enrolled, left, domain::Cardinality::Many, domain::Participation::Total, ""), "Student's side total");
            require(editor.update_participant(enrolled, right, domain::Cardinality::One, domain::Participation::Partial, ""), "Course's side partial");
            desktop::DiagramView view(editor);
            view.resize(1300, 600);
            view.show();
            view.set_grid_visible(false);
            view.set_line_style(desktop::LineStyle::Straight);
            view.set_notation(notation);
            view.actual_size();
            view.centerOn(0, 0);
            view.synchronize();
            QApplication::processEvents();
            const auto image = view.viewport()->grab().toImage();
            require(dark_in(view, image, QRectF(-369, -34, wide, 28)) > 0 && dark_in(view, image, QRectF(-369, 6, wide, 28)) == 0,
                    "The total side's number is just above the line, at the entity on the left");
            require(dark_in(view, image, QRectF(369 - wide, -34, wide, 28)) > 0 && dark_in(view, image, QRectF(369 - wide, 6, wide, 28)) == 0,
                    "The partial side's number is just above the line, at the entity on the right");
            for (const auto x : {-366.0, -356.0, -346.0, -336.0})
                require(runs_down(view, image, QPointF(x, 0)) == 2, "Both of the total side's lines run on unbroken beside its number");
            for (const auto x : {366.0, 356.0, 346.0})
                require(runs_down(view, image, QPointF(x, 0)) == 1, "The partial side's line runs on unbroken beside its number");
        }
        // Standing: Student above, total; Course below, partial.
        {
            SequentialIds ids;
            application::Editor editor(ids);
            const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-74, -400, 148, 86}).created);
            const auto course = std::get<domain::EntityId>(*editor.create_entity("Course", {-74, 314, 148, 86}).created);
            const auto enrolled = std::get<domain::RelationshipId>(*editor.create_relationship("Enrolled", {-95, -55, 190, 110}).created);
            const auto above = *editor.connect(enrolled, student).participant;
            const auto below = *editor.connect(enrolled, course).participant;
            require(editor.update_participant(enrolled, above, domain::Cardinality::Many, domain::Participation::Total, ""), "Student's side total");
            require(editor.update_participant(enrolled, below, domain::Cardinality::One, domain::Participation::Partial, ""), "Course's side partial");
            desktop::DiagramView view(editor);
            view.resize(700, 1000);
            view.show();
            view.set_grid_visible(false);
            view.set_line_style(desktop::LineStyle::Straight);
            view.set_notation(notation);
            view.actual_size();
            view.centerOn(0, 0);
            view.synchronize();
            QApplication::processEvents();
            const auto image = view.viewport()->grab().toImage();
            require(dark_in(view, image, QRectF(6, -311, wide, 31)) > 0 && dark_in(view, image, QRectF(-6 - wide, -311, wide, 31)) == 0,
                    "Beside a standing line, the number is to its right, just below the entity above");
            require(dark_in(view, image, QRectF(6, 280, wide, 31)) > 0 && dark_in(view, image, QRectF(-6 - wide, 280, wide, 31)) == 0,
                    "And just above the entity below");
        }
    }
}

// Inspect the existing hit shape's label subpath; no production test API or
// font-dependent expected width is needed. Check whole stroked runs, rather
// than individual antialiased pixels, at all three rendered weights.
void routed_entity_end_label_tests(desktop::MinMaxDisplayMode mode = desktop::MinMaxDisplayMode::CardinalityAndOrdinality) {
    const auto label_rect = [](QGraphicsItem* item, bool role = false) {
        const auto polygons = item->shape().toSubpathPolygons();
        return polygons[polygons.size() - (role ? 2 : 1)].boundingRect();
    };
    const auto clear_runs = [](const QRectF& label, QPointF end, QPointF corner,
                               std::optional<QPointF> preceding, bool total) {
        for (const auto weight : {3.52, 5.44, 6.4}) {
            const auto apart = total ? (weight + 1.6) / 2 : 0.0;
            for (const auto offset : {-apart, apart}) {
                // These fixtures have right-angle turns. Their known parallel
                // paths meet at the intersection of the two offset runs.
                const auto unit_normal = [](QPointF delta) {
                    return QPointF(-delta.y(), delta.x()) / std::hypot(delta.x(), delta.y());
                };
                const auto terminal_normal = unit_normal(end - corner);
                QPainterPath path;
                if (preceding) {
                    const auto before_normal = unit_normal(corner - *preceding);
                    path.moveTo(*preceding + before_normal * offset);
                    path.lineTo(corner + (before_normal + terminal_normal) * offset);
                } else {
                    path.moveTo(corner + terminal_normal * offset);
                }
                path.lineTo(end + terminal_normal * offset);
                QPainterPathStroker stroker;
                stroker.setWidth(weight);
                stroker.setCapStyle(Qt::SquareCap);
                stroker.setJoinStyle(Qt::BevelJoin);
                require(!stroker.createStroke(path).intersects(label),
                        "The filled cardinality box clears every visible terminal/preceding stroke and join");
            }
        }
    };
    const auto near_end = [](const QRectF& label, QPointF end) {
        const auto delta = label.center() - end;
        require(std::hypot(delta.x(), delta.y()) < label.width() + 48,
                "A cleared label stays close to its own entity end");
        require(std::abs(label.height() - 29) < 0.001, "The existing rendered label height is preserved");
    };
    for (const auto notation : {desktop::Notation::Chen, desktop::Notation::MinMax}) {
        // Short automatic elbow: the wide (1,M) box used to erase the
        // preceding vertical run even though it cleared the final horizontal one.
        {
            SequentialIds ids;
            application::Editor editor(ids);
            const auto entity = std::get<domain::EntityId>(*editor.create_entity("Entity", {300, 100, 148, 86}).created);
            const auto rel = std::get<domain::RelationshipId>(*editor.create_relationship("Link", {0, -55, 190, 110}).created);
            const auto participant = *editor.connect(rel, entity).participant;
            require(editor.update_participant(rel, participant, domain::Cardinality::Many, domain::Participation::Total, ""), "Total short elbow");
            desktop::DiagramView view(editor);
            view.set_notation(notation);
            view.set_line_style(desktop::LineStyle::Elbow);
            view.set_minmax_display_mode(mode);
            auto* edge = find_edge(view);
            const auto label = label_rect(edge);
            near_end(label, {300, 143});
            clear_runs(label, {300, 143}, {245, 143}, QPointF(245, 0), true);
            require(label.center().x() < 300 && !label.intersects(QRectF(300, 100, 148, 86)),
                    "Short elbow label stays on the terminal approach side and outside its entity");
            const auto label_index = edge->shape().toSubpathPolygons().size() - 1;
            edge->setSelected(true);
            view.synchronize();
            require(edge->shape().toSubpathPolygons()[label_index].boundingRect() == label,
                    "Selection and refresh do not move the cardinality box");
            require(editor.project().connectors.empty(), "Automatic clearance does not persist a route");
        }
        // The centres point diagonally upward, but the last run is horizontal.
        // Repeat at the origin and away from it, with and without repeated
        // final waypoints accepted by the public Editor API.
        QRectF origin_label;
        for (const bool repeated : {false, true}) {
            for (const auto translation : {QPointF(), QPointF(200, 100)}) {
                SequentialIds ids;
                application::Editor editor(ids);
                const auto entity = std::get<domain::EntityId>(*editor.create_entity("Entity", {100 + translation.x(), -43 + translation.y(), 148, 86}).created);
                const auto rel = std::get<domain::RelationshipId>(*editor.create_relationship("Link", {500 + translation.x(), -1000 + translation.y(), 190, 110}).created);
                const auto participant = *editor.connect(rel, entity).participant;
                require(editor.update_participant(rel, participant, domain::Cardinality::Many, domain::Participation::Total, ""), "Total origin route");
                std::vector<domain::Point> route{{600 + translation.x(), -900 + translation.y()},
                                                {translation.x(), -900 + translation.y()},
                                                {translation.x(), translation.y()}};
                if (repeated) route.push_back(route.back());
                require(editor.route_connector(participant, route), "Public API accepts coincident final waypoints");
                desktop::DiagramView view(editor);
                view.set_notation(notation);
                view.set_minmax_display_mode(mode);
                const auto label = label_rect(find_edge(view)).translated(-translation);
                near_end(label, {100, 0});
                require(std::abs(label.right() - 98) < 0.001 && std::abs(label.bottom() + 8.2) < 0.001,
                        "Origin route follows its real horizontal terminal run, with the existing end and normal gaps");
                clear_runs(label, {100, 0}, {0, 0}, QPointF(0, -900), true);
                if (!repeated && translation == QPointF()) origin_label = label;
                else require(QLineF(label.topLeft(), origin_label.topLeft()).length() < 0.001
                             && QLineF(label.bottomRight(), origin_label.bottomRight()).length() < 0.001,
                             "Whole-diagram translation and coincident corners preserve relative label geometry");
                require(editor.project().connectors.at(participant).waypoints == route,
                        "Label layout never changes the stored route");
            }
        }
        // A last waypoint can also coincide with the pinned endpoint itself.
        // The renderer must skip that zero-length final run, not use its fallback.
        {
            SequentialIds ids;
            application::Editor editor(ids);
            const auto entity = std::get<domain::EntityId>(*editor.create_entity("Entity", {100, -43, 148, 86}).created);
            const auto rel = std::get<domain::RelationshipId>(*editor.create_relationship("Link", {-300, -55, 190, 110}).created);
            domain::Connector route;
            route.child_anchor = std::numbers::pi; // midpoint of the rectangle's left face
            route.waypoints = {{0, 0}, {100, 0}, {100, 0}};
            const auto participant = *editor.connect(rel, entity, route).participant;
            require(editor.update_participant(rel, participant, domain::Cardinality::One, domain::Participation::Total, ""), "Total coincident endpoint");
            desktop::DiagramView view(editor);
            view.set_notation(notation);
            view.set_minmax_display_mode(mode);
            const auto label = label_rect(find_edge(view));
            require(std::abs(label.right() - 98) < 0.001 && std::abs(label.bottom() + 8.2) < 0.001,
                    "Coincident endpoint walks back to the real horizontal segment");
            clear_runs(label, {100, 0}, {0, 0}, std::nullopt, true);
        }
        // Each recursive participant keeps its own end. Roles previously held
        // the short return's label over both strokes of the preceding run.
        for (const bool roles : {false, true}) {
            SequentialIds ids;
            application::Editor editor(ids);
            const auto entity = std::get<domain::EntityId>(*editor.create_entity("Entity", {300, 100, 148, 86}).created);
            const auto rel = std::get<domain::RelationshipId>(*editor.create_relationship("Link", {0, 40, 190, 110}).created);
            const auto first = *editor.connect(rel, entity).participant;
            const auto second = *editor.connect(rel, entity).participant;
            require(editor.update_participant(rel, first, domain::Cardinality::Many, domain::Participation::Total, roles ? "First" : ""), "First recursive side");
            require(editor.update_participant(rel, second, domain::Cardinality::One, domain::Participation::Total, roles ? "Second" : ""), "Second recursive side");
            desktop::DiagramView view(editor);
            view.set_notation(notation);
            view.set_line_style(desktop::LineStyle::Elbow);
            view.set_minmax_display_mode(mode);
            std::vector<QRectF> labels;
            for (auto* edge : view.scene()->items()) {
                if (edge->zValue() >= 0 || !edge->toolTip().startsWith("Participant")) continue;
                const auto label = label_rect(edge, roles);
                labels.push_back(label);
                const bool loop = edge->toolTip().startsWith("Participant: One");
                const auto end = loop ? QPointF(374, 186) : QPointF(300, 143);
                near_end(label, end);
                if (loop) clear_runs(label, end, {374, 212}, QPointF(-26, 212), true);
                else clear_runs(label, end, {245, 143}, QPointF(245, 95), true);
                if (roles) {
                    const auto role = edge->shape().toSubpathPolygons().back().boundingRect();
                    require(!role.intersects(label), "Cleared cardinality does not collide with its existing role");
                    require(loop ? role.left() > 374 : role.top() > 143,
                            "Recursive roles retain their original side of the terminal run");
                }
            }
            require(labels.size() == 2 && labels[0] != labels[1] && !labels[0].intersects(labels[1]),
                    "Recursive ParticipantIds retain independent, distinct entity-end label rectangles");
            require(editor.project().connectors.empty(), "Recursive clearance does not store automatic geometry");
        }
    }
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
// Dragging an element near another's edge or middle lines the two up and draws
// a guide along what they now share. What can be lined up against is gathered
// once when the drag begins rather than asked for again on every pointer move,
// and anything too far away on both axes is passed over without being
// compared -- so this checks both that lining up still happens, and that it
// still happens against a shape far away across, which is the whole point of a
// guide and the case a nearby-only search would lose.
void alignment_guide_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto moving = *editor.create_entity("Moving", {0, 0, 160, 80}).created;
    // Far away across, and far away both ways, so only an alignment can bring
    // the first into line with either and only the first one should.
    editor.create_entity("Across", {1200, 200, 160, 80});
    editor.create_entity("Elsewhere", {1200, 900, 160, 80});
    desktop::DiagramView view(editor);
    view.resize(900, 500);
    view.show();
    view.set_align_to_grid(false);
    view.centerOn(600, 150);
    QApplication::processEvents();
    auto* item = find_node(view, "Moving");
    view.select_elements({moving});

    const auto start = view.mapFromScene(item->sceneBoundingRect().center());
    mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    // Three short of the distant element's top, which is within reach of it.
    mouse(view, QEvent::MouseMove, start + QPoint(0, 197), Qt::NoButton, Qt::LeftButton);
    require(item->pos().y() == 200, "A drag within reach lines up with a distant element's top");
    // Well clear of everything, so nothing is lined up with and the drag is
    // left exactly where the hand put it.
    mouse(view, QEvent::MouseMove, start + QPoint(0, 500), Qt::NoButton, Qt::LeftButton);
    require(item->pos().y() == 500, "A drag out of reach is left where it was put");
    mouse(view, QEvent::MouseButtonRelease, start + QPoint(0, 500), Qt::LeftButton, Qt::NoButton);
    require(editor.project().layout.at(moving).y == 500, "The drag is committed where it was let go");
}

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

    // A highlighted row is painted in the theme's accent, so a sample drawn in
    // that same accent would vanish into it. A sample given an ink is drawn in
    // that ink instead, and none of the accent is left anywhere in it.
    for (const auto id : {desktop::ThemeId::OfficeLight, desktop::ThemeId::Dracula, desktop::ThemeId::Forest}) {
        view.set_theme(id);
        QApplication::processEvents();
        const auto accent = desktop::theme(id).accent;
        const auto lit = desktop::readable_on(accent);
        for (int notation = 0; notation <= static_cast<int>(desktop::Notation::MinMax); ++notation) {
            const auto sample = view.notation_preview(static_cast<desktop::Notation>(notation), QSize(72, 24), lit);
            require(drawn(sample) > 40, "A sample for a highlighted row is still drawn");
            require(carries(sample, lit), "In the ink that reads on the highlight");
            require(!carries(sample, accent), "And nowhere in the colour it would be standing on");
        }
        for (const auto style : {desktop::LineStyle::Curved, desktop::LineStyle::Straight, desktop::LineStyle::Elbow}) {
            const auto sample = view.line_style_preview(style, QSize(48, 24), lit);
            require(carries(sample, lit) && !carries(sample, accent),
                    "A line style sample for a highlighted entry reads on it too");
        }
    }

    // Chen and min-max say the pair in writing, and the line must not run
    // through the very characters the reader is being shown. The written end is
    // found by asking where the ink reaches: with the line stopping short of
    // it, the rightmost ink is the writing rather than the line's own end.
    view.set_theme(desktop::ThemeId::OfficeLight);
    QApplication::processEvents();
    const auto written = view.notation_preview(desktop::Notation::MinMax, QSize(72, 24)).toImage()
                             .convertToFormat(QImage::Format_ARGB32);
    int rows_of_ink_at_middle = 0;
    const auto middle = written.height() / 2;
    for (int x = 0; x < written.width(); ++x)
        if (qAlpha(written.pixel(x, middle)) > 200) ++rows_of_ink_at_middle;
    const auto bare = view.notation_preview(desktop::Notation::CrowsFoot, QSize(72, 24)).toImage()
                          .convertToFormat(QImage::Format_ARGB32);
    int bare_ink_at_middle = 0;
    for (int x = 0; x < bare.width(); ++x)
        if (qAlpha(bare.pixel(x, middle)) > 200) ++bare_ink_at_middle;
    require(rows_of_ink_at_middle < bare_ink_at_middle,
            "A notation that writes its pair draws a shorter line, leaving the writing room of its own");
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
    require(view.notation() == desktop::Notation::CrowsFoot, "Crow's foot is the default notation");
    // An attribute is sized by hand as an entity is: a name that will not fit
    // the default has to be able to be given room.
    require(desktop::entity_body.width == 133 && desktop::entity_body.height == 77,
            "The entity's default is the one that was settled on");

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

void minmax_display_mode_tests() {
    using desktop::MinMaxDisplayMode;
    SequentialIds ids;
    application::Editor editor(ids);
    const auto entity = std::get<domain::EntityId>(*editor.create_entity("Entity", {300, -43, 148, 86}).created);
    const auto rel = std::get<domain::RelationshipId>(*editor.create_relationship("Link", {-95, -55, 190, 110}).created);
    const auto side = *editor.connect(rel, entity).participant;
    desktop::DiagramView view(editor);
    view.set_line_style(desktop::LineStyle::Straight);
    view.set_notation(desktop::Notation::MinMax);
    require(view.minmax_display_mode() == MinMaxDisplayMode::CardinalityAndOrdinality, "Fresh canvas retains combined Min-max default");
    const auto render = [&] {
        QImage image(700, 250, QImage::Format_ARGB32_Premultiplied);
        image.fill(view.canvas_colour());
        QPainter painter(&image);
        view.render_diagram(painter, QRectF(0, 0, 700, 250), QRectF(-150, -125, 700, 250));
        return image;
    };
    // Compare the actual painted label with independently supplied expected
    // text, using the same platform font. This checks characters, not just width.
    const auto expect_label = [&](const QString& text, QGraphicsItem* edge = nullptr) {
        if (!edge) edge = find_edge(view);
        const auto box = edge->shape().toSubpathPolygons().back().boundingRect();
        QFont font;
        font.setFamilies({"Inter", "Source Sans 3", "IBM Plex Sans", "Segoe UI Variable",
                          "Segoe UI", "SF Pro Text", "Helvetica Neue", "Arial"});
        font.setStyleHint(QFont::SansSerif, QFont::PreferAntialias);
        font.setPointSizeF(10 * desktop::lettering_scale);
        require(std::abs(box.width() - QFontMetricsF(font).horizontalAdvance(text) - 10) < 0.001,
                "Label width follows the actual selected text");
        const QSize size(static_cast<int>(std::ceil(box.width())) + 8, 37);
        QImage actual(size, QImage::Format_ARGB32_Premultiplied);
        actual.fill(view.canvas_colour());
        QImage expected = actual;
        const auto paint = [&](QImage& image, bool reference) {
            QPainter painter(&image);
            painter.setRenderHint(QPainter::Antialiasing);
            painter.translate(QPointF(4, 4) - box.topLeft());
            painter.setClipRect(box);
            if (!reference) { edge->paint(&painter, nullptr, nullptr); return; }
            painter.setFont(font);
            painter.setPen(desktop::theme(view.theme_id()).node_text);
            QStaticText written(text);
            written.prepare(QTransform(), font);
            const auto extent = written.size();
            painter.drawStaticText(box.center() - QPointF(extent.width() / 2, extent.height() / 2), written);
        };
        paint(actual, false); paint(expected, true);
        require(actual == expected, "Painted Min-max label matches the exact expected string");
    };
    for (const auto maximum : {domain::Cardinality::One, domain::Cardinality::Many}) {
        for (const auto participation : {domain::Participation::Partial, domain::Participation::Total}) {
            require(editor.update_participant(rel, side, maximum, participation, ""), "Min-max mapping state");
            view.synchronize();
            editor.mark_saved(editor.revision());
            const auto project = editor.project();
            const auto revision = editor.revision();
            const auto history = editor.history_bytes();
            const auto undo = editor.undo_label(), redo = editor.redo_label();
            view.set_minmax_display_mode(MinMaxDisplayMode::CardinalityAndOrdinality);
            const auto pair = QString("(%1,%2)").arg(participation == domain::Participation::Total ? "1" : "0",
                                                     maximum == domain::Cardinality::Many ? "M" : "1");
            expect_label(pair);
            const auto combined = render();
            view.set_minmax_display_mode(MinMaxDisplayMode::CardinalityOnly);
            expect_label(maximum == domain::Cardinality::Many ? "M" : "1");
            require(render() != combined, "Runtime mode switch immediately changes the rendered label");
            view.set_minmax_display_mode(MinMaxDisplayMode::CardinalityAndOrdinality);
            require(render() == combined, "Switching back exactly restores combined rendering");
            require(editor.project() == project && editor.revision() == revision && !editor.dirty()
                    && editor.history_bytes() == history && editor.undo_label() == undo && editor.redo_label() == redo,
                    "Display switching preserves project, IDs, routes, revision, dirty state and history");
        }
    }
    for (const auto notation : {desktop::Notation::Chen, desktop::Notation::CrowsFoot, desktop::Notation::Bachman}) {
        view.set_notation(notation);
        const auto before = render();
        const auto preview = view.notation_preview(notation, {72, 24}).toImage();
        view.set_minmax_display_mode(MinMaxDisplayMode::CardinalityOnly);
        require(render() == before && view.notation_preview(notation, {72, 24}).toImage() == preview,
                "Other notation diagrams and samples are unchanged by Min-max mode");
        view.set_minmax_display_mode(MinMaxDisplayMode::CardinalityAndOrdinality);
    }
    view.set_notation(desktop::Notation::MinMax);
    const auto recursive = *editor.connect(rel, entity).participant;
    require(editor.update_participant(rel, recursive, domain::Cardinality::One, domain::Participation::Total, ""), "Add recursive side after presentation setup");
    view.synchronize();
    view.set_minmax_display_mode(MinMaxDisplayMode::CardinalityOnly);
    for (auto* edge : view.scene()->items())
        if (edge->zValue() < 0 && edge->toolTip().startsWith("Participant"))
            expect_label(edge->toolTip().startsWith("Participant: One") ? "1" : "M", edge);
    view.set_minmax_display_mode(MinMaxDisplayMode::CardinalityAndOrdinality);
    for (auto* edge : view.scene()->items())
        if (edge->zValue() < 0 && edge->toolTip().startsWith("Participant"))
            expect_label(edge->toolTip().startsWith("Participant: One") ? "(1,1)" : "(1,M)", edge);
    const auto sample = view.notation_preview(desktop::Notation::MinMax, {72, 24}).toImage();
    const auto exported = [&] {
        desktop::PictureOptions options;
        options.format = desktop::PictureFormat::Png;
        options.background = desktop::PictureBackground::ThemeColour;
        QByteArray bytes;
        require(desktop::draw_picture(view, options, {}, bytes).ok, "Export Min-max diagram");
        return QImage::fromData(bytes, "PNG");
    };
    const auto combined_export = exported();
    view.set_minmax_display_mode(MinMaxDisplayMode::CardinalityOnly);
    require(view.notation_preview(desktop::Notation::MinMax, {72, 24}).toImage() != sample,
            "Min-max preview reflects the current text mode");
    require(exported() != combined_export, "Export follows the live cardinality-only canvas");
    view.set_notation(desktop::Notation::Chen);
    view.set_notation(desktop::Notation::MinMax);
    require(view.minmax_display_mode() == MinMaxDisplayMode::CardinalityOnly, "Notation switching retains session choice");
    view.set_minmax_display_mode(MinMaxDisplayMode::CardinalityAndOrdinality);
    require(exported() == combined_export, "Export restores the exact combined form");
    desktop::DiagramView fresh(editor);
    require(fresh.minmax_display_mode() == MinMaxDisplayMode::CardinalityAndOrdinality, "An independent canvas has no persisted display choice");
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

// An attribute placed is attached only to an owner locked by hand (Zain,
// 2026-09-26). What happens to be selected is not a choice of owner.
void attribute_owner_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const domain::ElementRef mentor = std::get<domain::RelationshipId>(
        *editor.create_relationship("Mentor", {-60, -20, 180, 100}).created);
    const domain::ElementRef student = std::get<domain::EntityId>(
        *editor.create_entity("Student", {-420, -20, 160, 80}).created);
    const auto address = std::get<domain::AttributeId>(*editor.create_attribute("Address", {260, -220, 150, 60}).created);
    require(editor.set_attribute_kind(address, domain::AttributeKind::Composite), "Address is composite");
    const domain::ElementRef phone = std::get<domain::AttributeId>(
        *editor.create_attribute("Phone", {260, 200, 150, 60}).created);

    desktop::DiagramView view(editor);
    QString announced;
    view.on_status = [&](const QString& message) { announced = message; };
    int told = 0;
    view.on_attribute_owner = [&] { ++told; };
    view.resize(1100, 800);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();
    // A question put while placing is answered by the button named, and what
    // it said is kept. One that was not expected is dismissed, so it is
    // reported rather than left waiting for an answer forever.
    QString asked;
    const auto answering = [&](const char* button) {
        asked.clear();
        QTimer::singleShot(0, [&, button] {
            auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            if (!box) return;
            asked = box->text();
            if (auto* pressed = box->findChild<QAbstractButton*>(QString::fromLatin1(button))) pressed->click();
            else box->reject();
        });
    };
    // The attribute just placed is the one left selected.
    const auto place = [&](const QPointF& at) {
        answering("unexpected");
        click(view, at);
        require(asked.isEmpty(), "Nothing is asked when the attribute is put down beside nothing else");
        const auto chosen = view.selected_elements();
        require(chosen.size() == 1 && std::holds_alternative<domain::AttributeId>(chosen.front()),
                "A click places an attribute");
        return editor.project().attributes.at(std::get<domain::AttributeId>(chosen.front())).owner;
    };

    // Mentor selected, and the attribute still stands on its own.
    view.select_elements({mentor});
    view.set_tool(desktop::Tool::Attribute);
    require(announced.contains("connect it by hand"), "The tool says an attribute is connected by hand");
    require(!place(QPointF(0, -260)), "An attribute placed while Mentor is selected is not attached to it");
    require(!view.attribute_owner(), "Nothing is locked to begin with");

    // What can hold attributes is what can be locked.
    require(view.can_own_attributes(mentor) && view.can_own_attributes(student)
                && view.can_own_attributes(domain::ElementRef{address}),
            "An entity, a relationship and a composite attribute can be locked");
    require(!view.can_own_attributes(phone), "A plain attribute cannot");

    // Locked from Mentor's own menu, every attribute placed goes on Mentor,
    // whatever is selected, as many as a locked tool places.
    const auto right_click = [&](const domain::ElementRef& on, const char* entry) {
        view.select_elements({on});
        QApplication::processEvents();
        QTimer::singleShot(0, [&, entry] {
            auto* menu = qobject_cast<QMenu*>(QApplication::activePopupWidget());
            require(menu != nullptr, "A right-click opens a menu");
            auto* action = menu->findChild<QAction*>(QString::fromLatin1(entry));
            require(action != nullptr, "It offers to lock or release the attribute owner");
            menu->close();
            action->trigger();
        });
        const auto position = view.mapFromScene(find_node(view, QString::fromStdString(
            domain::name(editor.project(), on)))->sceneBoundingRect().center());
        QContextMenuEvent event(QContextMenuEvent::Mouse, position, view.viewport()->mapToGlobal(position));
        QApplication::sendEvent(view.viewport(), &event);
        QApplication::processEvents();
    };
    right_click(mentor, "contextLockOwner");
    require(view.attribute_owner() == mentor, "Mentor is locked as the owner");
    require(told == 1, "And the window is told");
    require(announced.contains("go on Mentor"), "The status line says where attributes will go");
    view.set_tool(desktop::Tool::Attribute, true);
    view.select_elements({student});
    std::vector<domain::AttributeId> fanned;
    for (const auto& at : {QPointF(-120, -260), QPointF(60, -300), QPointF(140, -300)}) {
        require(place(at) == mentor, "Each attribute placed goes on the locked owner, with its line drawn");
        fanned.push_back(std::get<domain::AttributeId>(view.selected_elements().front()));
    }
    require(view.tool() == desktop::Tool::Attribute && view.tool_locked(), "And the tool stays locked");

    // Every one is unlocked, and leaves Mentor from the middle of the side
    // facing it: here Mentor's top point, shared by all three, each by its
    // own straight line (Zain, 2026-09-26).
    const auto& mentor_body = editor.project().layout.at(mentor);
    const QPointF top_of_mentor(mentor_body.x + mentor_body.width / 2, mentor_body.y);
    // A line from the point, running straight towards the attribute.
    const auto leaves_from = [&](const domain::ElementRef& attribute, const QPointF& point) {
        const auto& box = editor.project().layout.at(attribute);
        const QPointF centre(box.x + box.width / 2, box.y + box.height / 2);
        const auto along = point + (centre - point) * 0.3;
        for (auto* item : view.scene()->items())
            if (item->zValue() < 0 && item->toolTip().startsWith("Attribute ownership")
                && item->shape().contains(item->mapFromScene(point))
                && item->shape().contains(item->mapFromScene(along)))
                return true;
        return false;
    };
    for (const auto& id : fanned) {
        require(!editor.project().connectors.contains(domain::ConnectorRef{id}), "Each line is unlocked");
        require(leaves_from(domain::ElementRef{id}, top_of_mentor), "And leaves from Mentor's top point");
    }
    // Placing one is still one step of history.
    const auto placed = editor.project().attributes.size();
    require(editor.undo(), "The last placement undone");
    view.synchronize();
    require(editor.project().attributes.size() == placed - 1 && !editor.project().attributes.contains(fanned[2]),
            "One undo takes back the last attribute and its line together");
    require(editor.redo(), "And redone");
    view.synchronize();
    QApplication::processEvents();

    // Put down beside Student while Mentor is locked, the attribute is asked
    // about before anything is placed (Zain, 2026-09-26).
    const QPointF beside_student(-340, 110);
    answering("ownerContinue");
    click(view, beside_student);
    require(asked.contains("Mentor") && asked.contains("Student"),
            "The question names the locked owner and the element the attribute is beside");
    auto chosen = view.selected_elements();
    require(chosen.size() == 1
                && editor.project().attributes.at(std::get<domain::AttributeId>(chosen.front())).owner == mentor,
            "Continuing attaches it to Mentor all the same");
    require(place(QPointF(-300, 120)) == mentor, "And Student is not asked about again while the lock lasts");
    // Locked afresh, it is asked about again; Cancel places nothing.
    view.set_attribute_owner(std::nullopt);
    view.set_attribute_owner(mentor);
    auto count = editor.project().attributes.size();
    answering("none, so Cancel");
    click(view, beside_student);
    require(!asked.isEmpty() && editor.project().attributes.size() == count, "Cancelled, nothing is placed");
    require(view.attribute_owner() == mentor, "And the lock stays");
    // Unlock lets the lock go and places the attribute on its own, to be
    // connected to Student by hand.
    answering("ownerUnlock");
    click(view, beside_student);
    require(!asked.isEmpty() && !view.attribute_owner(), "Unlocking lets the lock go");
    chosen = view.selected_elements();
    require(editor.project().attributes.size() == count + 1 && chosen.size() == 1
                && !editor.project().attributes.at(std::get<domain::AttributeId>(chosen.front())).owner,
            "And the attribute is placed on its own");
    view.set_attribute_owner(mentor);

    // A plain attribute is not taken as an owner, and the lock held stays.
    view.set_attribute_owner(phone);
    require(view.attribute_owner() == mentor, "Asking for a plain attribute leaves the lock where it was");
    // Another owner moves the lock.
    view.set_attribute_owner(domain::ElementRef{address});
    require(place(QPointF(420, -320)) == domain::ElementRef{address}, "A composite attribute takes its parts");
    const auto part = view.selected_elements().front();
    // Unlocked from its menu, attributes stand on their own again.
    view.set_tool(desktop::Tool::Select);
    right_click(domain::ElementRef{address}, "contextUnlockOwner");
    require(!view.attribute_owner(), "Unlocking lets the lock go");
    view.set_tool(desktop::Tool::Attribute);
    require(!place(QPointF(-300, 260)), "And an attribute is placed on its own again");

    // The padlock on the element does the same without a menu: open on one
    // that is selected, closed on the owner.
    const auto padlock = [&](const QString& named) {
        constexpr auto middle = 7 + 6 * desktop::connector_scale;
        return find_node(view, named)->mapToScene(QPointF(middle, middle));
    };
    view.set_tool(desktop::Tool::Select);
    view.select_elements({});
    click(view, padlock("Student"));
    require(!view.attribute_owner(), "An element not selected has no padlock to press");
    require(view.selected_elements() == std::vector<domain::ElementRef>{student}, "So the press selects it");
    click(view, padlock("Student"));
    require(view.attribute_owner() == student, "Selected, its open padlock locks it");
    click(view, padlock("Student"));
    require(!view.attribute_owner(), "And the closed padlock unlocks it");
    click(view, padlock("Student"));
    view.set_tool(desktop::Tool::Attribute, true);
    const auto before_padlock = editor.project().attributes.size();
    click(view, padlock("Student"));
    require(!view.attribute_owner() && editor.project().attributes.size() == before_padlock,
            "With the Attribute tool in hand, the owner's padlock unlocks it rather than placing one");
    // Locked again later, the owner's attributes leave from the point they
    // already share.
    view.set_attribute_owner(mentor);
    click(view, QPointF(-200, -330));
    QApplication::processEvents();
    require(leaves_from(view.selected_elements().front(), top_of_mentor),
            "Mentor locked again, a new attribute above leaves from the same point");
    view.set_attribute_owner(std::nullopt);

    // The lock goes with its owner, and stays gone when the owner comes back.
    view.set_attribute_owner(student);
    require(editor.erase({student}), "Student deleted");
    view.synchronize();
    require(!view.attribute_owner(), "Deleting the owner lets the lock go");
    require(editor.undo(), "Student restored");
    view.synchronize();
    require(!view.attribute_owner(), "Undoing the deletion does not lock it again");
    // As it does with a composite that stops being one.
    view.set_attribute_owner(domain::ElementRef{address});
    require(editor.erase({part}), "Its part taken off, since a composite with parts cannot be made plain");
    require(editor.set_attribute_kind(address, domain::AttributeKind::Normal), "Address made plain");
    view.synchronize();
    require(!view.attribute_owner(), "A composite made plain lets the lock go");
    // And with the project: another one is not this one.
    view.set_attribute_owner(mentor);
    editor.new_project();
    view.synchronize();
    require(!view.attribute_owner(), "A new project starts with nothing locked");
}

// A composite attribute wears the same padlock an entity does and is locked
// by pressing it on the diagram, as the entity is: selected, it is open;
// pressed, it closes and every part placed goes on the composite; pressed
// again, it lets go. Handing the lock from an entity to a composite leaves
// nothing on the entity, and a rebuilt projection keeps the lock drawn. A
// plain attribute has no padlock at all.
void composite_owner_padlock_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const domain::ElementRef student = *editor.create_entity("Student", {-74, 140, 148, 86}).created;
    const auto name_id = std::get<domain::AttributeId>(
        *editor.create_attribute("Name", {-300, -40, 150, 60}, domain::AttributeOwner{student}).created);
    const domain::ElementRef name{name_id};
    const domain::ElementRef age = *editor.create_attribute("Age", {140, -40, 150, 60},
                                                            domain::AttributeOwner{student}).created;

    desktop::DiagramView view(editor);
    view.resize(1100, 800);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();
    const auto& colors = desktop::theme(view.theme_id());
    // The middle of the padlock, which is also the middle of its body: filled
    // with the selection colour when closed, with the paper when open, and
    // simply the element's own surface when there is no padlock.
    const auto padlock = [&](const domain::ElementRef& on) {
        constexpr auto middle = 7 + 6 * desktop::connector_scale;
        const auto& box = editor.project().layout.at(on);
        return QPointF(box.x + middle, box.y + middle);
    };
    const auto centre = [&](const domain::ElementRef& on) {
        const auto& box = editor.project().layout.at(on);
        return QPointF(box.x + box.width / 2, box.y + box.height / 2);
    };
    const auto drawn = [&](const domain::ElementRef& on) {
        QApplication::processEvents();
        const auto image = view.viewport()->grab().toImage();
        return image.pixelColor(device_point(image, view.mapFromScene(padlock(on)))).rgb();
    };
    const auto open = colors.canvas.rgb();
    const auto closed = colors.accent.rgb();
    // Nothing here is put down near anything that could hold it, so nothing
    // should be asked; a question that does come is dismissed and reported
    // rather than left waiting for an answer.
    QString asked;
    const auto place = [&](const QPointF& at) {
        asked.clear();
        QTimer::singleShot(0, [&] {
            if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) {
                asked = box->text();
                box->reject();
            }
        });
        click(view, at);
        require(asked.isEmpty(), "Nothing is asked when a part is put down beside nothing else");
        const auto chosen = view.selected_elements();
        require(chosen.size() == 1 && std::holds_alternative<domain::AttributeId>(chosen.front()),
                "A click places an attribute");
        return chosen.front();
    };
    view.set_tool(desktop::Tool::Select);

    // The entity, as before: selected, its open padlock locks it.
    click(view, centre(student));
    require(view.selected_elements() == std::vector<domain::ElementRef>{student}, "Student selected");
    require(drawn(student) == open, "Selected, Student wears an open padlock");
    click(view, padlock(student));
    require(view.attribute_owner() == student, "Pressing it locks Student");
    require(drawn(student) == closed, "And the padlock is drawn closed");

    // Name made composite, as Properties makes it, is something that can
    // hold attributes. Not yet selected, it has no padlock showing.
    require(editor.set_attribute_kind(name_id, domain::AttributeKind::Composite), "Name made composite");
    view.synchronize();
    require(view.can_own_attributes(name), "A composite attribute can hold attributes");
    require(drawn(name) != open && drawn(name) != closed, "Not selected, Name shows no padlock");

    // Selected, it wears the open padlock; the owner still wears its closed one.
    click(view, centre(name));
    require(view.selected_elements() == std::vector<domain::ElementRef>{name}, "Name selected");
    require(drawn(name) == open, "Selected, the composite wears an open padlock");
    require(drawn(student) == closed, "While Student, still the owner, keeps its closed one");

    // Pressed, the lock moves to Name and nothing is left on Student.
    click(view, padlock(name));
    require(view.attribute_owner() == name, "Pressing the composite's padlock locks it as the owner");
    require(drawn(name) == closed, "Its padlock is drawn closed");
    require(drawn(student) != closed && drawn(student) != open, "And Student's padlock is gone");
    click(view, padlock(student));
    require(view.attribute_owner() == name && view.selected_elements() == std::vector<domain::ElementRef>{student},
            "Where Student's padlock was, a press only selects Student");

    // Every part placed while it holds goes on Name, the lock held throughout.
    view.set_tool(desktop::Tool::Attribute, true);
    for (const auto& at : {QPointF(-420, -200), QPointF(-225, -220), QPointF(-30, -200)}) {
        const auto part = place(at);
        require(editor.project().attributes.at(std::get<domain::AttributeId>(part)).owner
                    == domain::AttributeOwner{name},
                "Each part placed goes on the composite");
        require(view.attribute_owner() == name, "And the composite stays locked");
    }
    require(view.tool() == desktop::Tool::Attribute && view.tool_locked(), "The locked tool keeps placing");
    require(drawn(name) == closed, "The composite, no longer selected, keeps its closed padlock");

    // Rebuilt after an edit elsewhere, the lock is still held and still drawn.
    require(editor.rename(std::get<domain::EntityId>(student), "Pupil"), "An unrelated edit");
    view.synchronize();
    require(view.attribute_owner() == name && drawn(name) == closed,
            "A rebuilt projection keeps the composite locked and its padlock drawn");

    // Pressed again, with the Attribute tool still in hand, it lets go
    // rather than placing a part.
    const auto parts = editor.project().attributes.size();
    click(view, padlock(name));
    require(!view.attribute_owner() && editor.project().attributes.size() == parts,
            "The closed padlock unlocks the composite rather than placing one");
    require(drawn(name) != closed, "And it is no longer drawn closed");

    // A plain attribute has no padlock and cannot be locked.
    view.set_tool(desktop::Tool::Select);
    click(view, centre(age));
    require(view.selected_elements() == std::vector<domain::ElementRef>{age}, "Age selected");
    require(!view.can_own_attributes(age), "A plain attribute cannot hold attributes");
    require(drawn(age) != open && drawn(age) != closed, "Selected, a plain attribute shows no padlock");
    click(view, padlock(age));
    require(!view.attribute_owner(), "Pressing where a padlock would be locks nothing");
    view.set_attribute_owner(age);
    require(!view.attribute_owner(), "Nor can it be locked any other way");
}

// Placed by hand and named afterwards, attributes are listed in the order they
// are named (Zain, 2026-10-03): four clicked down right to left with the locked
// Attribute tool, then renamed in place 1, 2, 3, 4 from left to right. The
// order they were placed in alone would list 4, 3, 2 after the key.
void attributes_named_on_the_canvas_list_in_naming_order() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto entity = *editor.create_entity("Entity", {-74, 140, 148, 86}).created;
    desktop::DiagramView view(editor);
    view.resize(1100, 800);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();
    view.set_attribute_owner(entity);
    view.set_tool(desktop::Tool::Attribute, true);
    std::vector<domain::AttributeId> placed;
    for (const auto& at : {QPointF(260, -60), QPointF(80, -140), QPointF(-100, -140), QPointF(-280, -60)}) {
        click(view, at);
        const auto chosen = view.selected_elements();
        require(chosen.size() == 1 && std::holds_alternative<domain::AttributeId>(chosen.front()),
                "A click places an attribute");
        placed.push_back(std::get<domain::AttributeId>(chosen.front()));
        require(editor.project().attributes.at(placed.back()).name == "Attribute",
                "It is placed under the name the canvas gives every new attribute");
    }
    view.set_tool(desktop::Tool::Select);
    // Named left to right, so the last placed is named first.
    const std::vector<std::pair<std::size_t, QString>> naming{{3, "1"}, {2, "2"}, {1, "3"}, {0, "4"}};
    for (const auto& [which, name] : naming) {
        view.synchronize();
        QApplication::processEvents();
        const auto& box = editor.project().layout.at(domain::ElementRef{placed[which]});
        const auto centre = view.mapFromScene(QPointF(box.x + box.width / 2, box.y + box.height / 2));
        mouse(view, QEvent::MouseButtonDblClick, centre, Qt::LeftButton, Qt::LeftButton);
        require(view.renaming(), "Double-clicking an attribute opens its name in place");
        view.viewport()->findChild<QLineEdit*>("inlineName")->setText(name);
        view.commit_rename();
        require(editor.project().attributes.at(placed[which]).name == name.toStdString(), "The name is taken");
    }
    require(editor.set_attribute_kind(placed[3], domain::AttributeKind::Key), "1 is made the key");
    std::vector<std::string> listed;
    for (const auto& table : domain::schema_preview(editor.project()).tables)
        for (const auto& column : table.columns) listed.push_back(column.name);
    require((listed == std::vector<std::string>{"1", "2", "3", "4"}),
            "The schema lists the attributes in the order they were named, the key first");
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
    // A link leaves the body from the middle of the side facing its attribute
    // (Zain, 2026-09-26), so every attribute on one side leaves from the same
    // point, each by its own straight line, and moving an attribute about on
    // that side leaves the point where it is. (It used to slide along the
    // outline with the attribute; Zain chose the fixed point, knowing that a
    // line carried past a corner moves to the next side's middle at once.)
    const auto body = find_node(view, "Person")->sceneBoundingRect();
    const QPointF above{body.center().x(), body.top()};
    const QPointF beside{body.left(), body.center().y()};
    for (const auto& attribute : {QStringLiteral("First"), QStringLiteral("Last"), QStringLiteral("Born")}) {
        auto* link = edge_for(attribute);
        require(link->shape().contains(link->mapFromScene(above)),
                "Every attribute above leaves from the middle of the top");
    }
    const auto called = [&](const std::string& name) {
        for (const auto& [id, each] : editor.project().attributes)
            if (each.name == name) return domain::ElementRef{id};
        throw std::runtime_error("Missing attribute");
    };
    require(editor.move({{called("Last"), {290, 20, 130, 54}}}), "Move Last a little along the top");
    view.synchronize();
    QApplication::processEvents();
    auto* moved_last = edge_for(QStringLiteral("Last"));
    require(moved_last->shape().contains(moved_last->mapFromScene(above)),
            "Moved along the same side, its line still leaves from the same point");
    require(editor.undo(), "Put it back");
    view.synchronize();
    QApplication::processEvents();
    auto* sideways = edge_for(QStringLiteral("Ident"));
    require(sideways->shape().contains(sideways->mapFromScene(beside)),
            "An attribute on another side leaves from that side's middle");
    require(!sideways->shape().contains(sideways->mapFromScene(above)),
            "It does not run back across the body to the point above");

    const auto render = [&] {
        QApplication::processEvents();
        return view.viewport()->grab().toImage();
    };
    const auto curved = render();
    view.set_line_style(desktop::LineStyle::Straight);
    require(view.line_style() == desktop::LineStyle::Straight, "The chosen style is kept");
    const auto straight = render();
    require(curved == straight, "An attribute's line is straight in every line style, from its shared point");
    view.set_line_style(desktop::LineStyle::Curved);
    require(render() == curved, "Returning to a style reproduces its drawing");

    // The point goes with the owner when the owner moves.
    require(editor.move({{domain::ElementRef{person}, {620, 330, 160, 80}}}), "Move the owner");
    view.synchronize();
    QApplication::processEvents();
    const auto moved = find_node(view, "Person")->sceneBoundingRect();
    auto* link = edge_for(QStringLiteral("Last"));
    require(link->shape().contains(link->mapFromScene(QPointF(moved.center().x(), moved.top()))),
            "The exit point moves with the owner");
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
// A listing says in words what the diagram says in shapes. It is not the
// project and nothing reopens it, so what matters is that it names what is
// actually there, names it the way the notation means it, and comes out the
// same twice for the same model.
void document_export_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto owner = editor.create_entity("Building", {0, 0, 160, 80});
    const auto weak = editor.create_entity("Room", {400, 0, 160, 80});
    require(owner && weak, "Document fixture");
    const auto building = std::get<domain::EntityId>(*owner.created);
    const auto room = std::get<domain::EntityId>(*weak.created);
    require(editor.set_entity_weak(room, true), "A weak entity to list");
    require(!editor.project().entities.at(building).weak && editor.project().entities.at(room).weak,
            "Strong and Weak listing values retain the existing boolean semantics");
    require(editor.create_relationship("Visits", {200, 400, 190, 110}), "A regular relationship to list");
    const auto contains = editor.create_relationship("Contains", {200, 200, 190, 110});
    const auto relationship = std::get<domain::RelationshipId>(*contains.created);
    require(editor.set_relationship_kind(relationship, domain::RelationshipKind::Identifying),
            "Identified through this one");
    const auto first = editor.connect(relationship, building);
    require(first && editor.connect(relationship, room), "Joined at both ends");
    require(editor.update_participant(relationship, *first.participant, domain::Cardinality::One,
                                      domain::Participation::Total, "owner"),
            "One side mandatory, and with a role");
    const auto number = editor.create_attribute("Number", {400, -150, 150, 60}, room);
    require(number, "A key on the weak entity");
    require(editor.set_attribute_kind(std::get<domain::AttributeId>(*number.created), domain::AttributeKind::Key),
            "Made a key");
    const auto age = editor.create_attribute("Age", {0, -150, 150, 60}, building);
    require(age && editor.set_attribute_kind(std::get<domain::AttributeId>(*age.created),
                                             domain::AttributeKind::Derived),
            "And a derived one on the owner");

    desktop::DiagramView view(editor);
    view.resize(800, 600);
    view.show();
    QApplication::processEvents();

    QByteArray markdown;
    require(desktop::draw_document(view, editor.project(), desktop::DocumentFormat::Markdown, markdown).ok,
            "A Markdown data dictionary is written");
    require(markdown.startsWith("# "), "Opening with the project's own name as a heading");
    require(markdown.contains("| Entity | Kind |"), "With a table of entities");
    require(markdown.contains("Building") && markdown.contains("Room"), "Naming what is on the diagram");
    // A key on a weak entity identifies an instance only once the owner is
    // known, so a dictionary that called it a key would be saying something
    // false about the model.
    require(markdown.contains("Partial key"), "Calling a weak entity's key what it is");
    require(markdown.contains("Derived"), "And naming the attribute nobody stores");
    require(markdown.contains("Weak"), "Saying which entity has no key of its own");
    require(markdown.contains("| Building | Strong |") && markdown.contains("| Room | Weak |"),
            "Markdown entity Kind rows say Strong and Weak");
    require(markdown.contains("| Visits | Regular |"), "Markdown relationship Kind remains Regular");
    require(markdown.contains("Identifying"), "And which relationship it is identified through");
    require(markdown.contains("as owner"), "A role on a side is part of what the side says");
    require(markdown.contains("(one, mandatory)") && markdown.contains("(many, optional)"),
            "Each side read as a person would say it rather than as a pair of numbers");

    // The same model twice is the same file, so a listing kept beside the
    // project in version control shows a change only where one was made.
    QByteArray again;
    require(desktop::draw_document(view, editor.project(), desktop::DocumentFormat::Markdown, again).ok, "Again");
    require(markdown == again, "The same project lists the same way twice");

    QByteArray csv;
    require(desktop::draw_document(view, editor.project(), desktop::DocumentFormat::Csv, csv).ok, "A CSV listing");
    require(csv.startsWith("Element,Name,Belongs to,Kind,Detail,Description\n"), "With a header row");
    require(csv.contains("\"Entity\",\"Building\""), "And one row per element, quoted");
    require(csv.contains("\"Entity\",\"Building\",\"\",\"Strong\"")
            && csv.contains("\"Entity\",\"Room\",\"\",\"Weak\""), "CSV entity Kind rows say Strong and Weak");
    require(csv.contains("\"Relationship\",\"Visits\",\"(not connected)\",\"Regular\""), "CSV relationship Kind remains Regular");
    require(csv.count('\n') >= 5, "Covering entities, attributes and relationships alike");

    QByteArray html;
    require(desktop::draw_document(view, editor.project(), desktop::DocumentFormat::Html, html).ok, "An HTML report");
    require(html.startsWith("<!DOCTYPE html>"), "Which is a web page");
    // One self-contained page: the diagram travels inside it, so there is no
    // sidecar file and no link that can break in transit.
    require(html.contains("<svg") && html.contains("</svg>"), "Carrying the diagram as inline SVG");
    require(!html.contains("<img"), "Rather than pointing at a picture beside it");
    require(html.contains("<table"), "Above the data dictionary");
    require(html.contains("</html>"), "And a complete one");
    {
        // The page is HTML rather than XML, so it is not asked to parse as
        // XML. The picture inside it is XML, and that is worth asking: a
        // report whose diagram will not draw is a report with a hole in it.
        const auto start = html.indexOf("<svg");
        const auto end = html.lastIndexOf("</svg>");
        require(start >= 0 && end > start, "The diagram is in one piece");
        require(QSvgRenderer(html.mid(start, end - start + 6)).isValid(),
                "And is a picture a renderer will draw");
    }

    QByteArray pdf;
    require(desktop::draw_document(view, editor.project(), desktop::DocumentFormat::Pdf, pdf).ok, "A PDF report");
    require(pdf.startsWith("%PDF"), "Which is a PDF");
    require(pdf.size() > 1000, "With the diagram and the dictionary in it");

    // Nothing drawn is nothing to list, and it is refused with a reason rather
    // than written as a file with headings and no rows.
    editor.new_project();
    view.synchronize();
    QByteArray empty;
    const auto refused = desktop::draw_document(view, editor.project(), desktop::DocumentFormat::Markdown, empty);
    require(!refused.ok && !refused.error.isEmpty(), "An empty project is refused, with a reason");
}
// A search narrows what the diagram shows rather than changing what is on it.
// It is how the diagram is being looked at, like the grid, so nothing it does
// reaches the model or the history.
void search_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto made_student = editor.create_entity("Student", {0, 0, 160, 80});
    const auto made_course = editor.create_entity("Course", {400, 0, 160, 80});
    const auto made_staff = editor.create_entity("Professor", {800, 0, 160, 80});
    require(made_student && made_course && made_staff, "Search fixture");
    const auto student = std::get<domain::EntityId>(*made_student.created);
    const auto course = std::get<domain::EntityId>(*made_course.created);
    const auto enrolled = std::get<domain::RelationshipId>(
        *editor.create_relationship("Enrolled", {200, 200, 190, 110}).created);
    require(editor.connect(enrolled, student) && editor.connect(enrolled, course), "Joined");
    require(editor.create_attribute("Gender", {0, -160, 150, 60}, domain::ElementRef{student}), "An attribute");
    require(editor.create_attribute("Credit Hours", {400, -160, 150, 60}, domain::ElementRef{course}), "And another");

    desktop::DiagramView view(editor);
    view.resize(900, 700);
    view.show();
    QApplication::processEvents();

    const auto revision = editor.revision();
    const auto before = editor.project();

    // A name narrows the diagram to what carries it, without regard to case.
    desktop::DiagramSearch asked;
    asked.text = "stud";
    view.set_search(asked);
    require(view.found_elements() == std::vector<domain::ElementRef>{domain::ElementRef{student}},
            "A name finds what carries it, whatever the case");
    require(find_node(view, "Student")->opacity() == 1.0, "What was found keeps its full strength");
    require(find_node(view, "Course")->opacity() < 0.5, "And the rest recede");
    require(find_node(view, "Course")->isVisible(), "But are still there, so the diagram keeps its shape");
    // Nothing about the document moved: a search is a way of looking, not an edit.
    require(editor.revision() == revision && editor.project() == before,
            "Searching changes nothing on the diagram itself");

    // A kind with nothing typed asks for every element of that kind, which is
    // how "show me only the entities" is asked for.
    asked = {};
    asked.kind = desktop::SearchKind::Entities;
    view.set_search(asked);
    require(view.found_elements().size() == 3, "A kind alone finds every element of it");
    require(find_node(view, "Enrolled")->opacity() < 0.5, "And nothing of any other kind");
    asked.kind = desktop::SearchKind::Relationships;
    view.set_search(asked);
    require(view.found_elements() == std::vector<domain::ElementRef>{domain::ElementRef{enrolled}},
            "Asking for relationships finds the relationship");

    // One step out: what belongs to a match, what it is joined to, and the far
    // side of that -- but not the far side's own belongings.
    asked = {};
    asked.text = "Student";
    asked.with_relatives = true;
    view.set_search(asked);
    require(view.found_elements().size() == 1, "What was found is still just the match");
    for (const char* near : {"Student", "Gender", "Enrolled", "Course"})
        require(find_node(view, near)->opacity() == 1.0, near);
    require(find_node(view, "Credit Hours")->opacity() < 0.5,
            "Two steps out is not one step out: the far side's own attributes stay back");
    require(find_node(view, "Professor")->opacity() < 0.5, "And nothing it does not touch comes along");

    // Hiding takes the rest away rather than fading it, and a line goes with
    // whichever of its ends goes, so nothing is left hanging.
    asked.hide_the_rest = true;
    view.set_search(asked);
    require(find_node(view, "Professor")->isVisible() == false, "Hiding takes the rest away");
    require(find_node(view, "Student")->isVisible(), "And leaves what was asked for");
    for (auto* item : view.scene()->items())
        if (item->isVisible())
            require(item->opacity() > 0.5, "Nothing is left both shown and faded");

    // An idle search puts the whole diagram back.
    view.set_search({});
    require(view.found_elements().empty(), "An idle search has found nothing");
    for (const char* every : {"Student", "Course", "Professor", "Enrolled", "Gender", "Credit Hours"})
        require(find_node(view, every)->isVisible() && find_node(view, every)->opacity() == 1.0, every);
}
// Relationship diamonds share the existing handles without changing stored sizes.
// An attribute is pulled by its edges and corners as an entity is. Its
// handles were drawn, but letting go of one sent the new size to the symbols'
// command, which refused it: "Only a symbol can be resized".
void attribute_resize_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const domain::ElementRef ref = *editor.create_attribute("EnrollmentDate", {0, 0, 150, 60}).created;
    desktop::DiagramView view(editor);
    QString refused;
    view.on_edit = [&](const application::EditResult& result) { if (!result) refused = QString::fromStdString(result.error); };
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(75, 30);
    QApplication::processEvents();
    view.select_elements({ref});
    QApplication::processEvents();
    const auto haul = [&](QPointF grip, QPoint by) {
        const auto& at = editor.project().layout.at(ref);
        const auto start = view.mapFromScene(QPointF(at.x, at.y) + grip);
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, start + by, Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseButtonRelease, start + by, Qt::LeftButton, Qt::NoButton);
        view.synchronize();
        return editor.project().layout.at(ref);
    };
    // The right edge, alone, gives the name room.
    auto box = haul({148, 30}, {60, 0});
    require(refused.isEmpty(), "Pulling an attribute is not refused");
    require(box.x == 0 && box.y == 0 && box.width == 210 && box.height == 60,
            "The right edge moves and the other three stay where they were");
    require(editor.undo_label() == "Resize attribute", "It is its own step of history");
    // A corner moves the two sides it lies on.
    box = haul({2, 2}, {-20, -10});
    require(box.x == -20 && box.y == -10 && box.width == 230 && box.height == 70, "The top-left corner moves both");
    // Pulled past the smallest it may be, it stops there.
    box = haul({228, 35}, {-400, 0});
    require(box.width == domain::min_entity_width && box.x == -20, "And an edge pulled too far stops at the minimum");
    require(refused.isEmpty(), "Nothing along the way was refused");
    require(editor.undo() && editor.undo() && editor.undo(), "Each pull undone");
    require(editor.project().layout.at(ref) == domain::Rect{0, 0, 150, 60}, "Back to the size it was placed at");

    // The name is drawn with the box (Zain, 2026-09-26): a corner pulled out
    // to twice the size draws it twice as tall, and pulling only the width
    // out after that gives it room without making it any taller.
    const auto lettering_height = [&] {
        view.synchronize();
        const auto& at = editor.project().layout.at(ref);
        QImage painted(QSize(static_cast<int>(at.width), static_cast<int>(at.height)), QImage::Format_ARGB32);
        painted.fill(Qt::transparent);
        QPainter painter(&painted);
        find_node(view, "EnrollmentDate")->paint(&painter, nullptr, nullptr);
        painter.end();
        // The first line of the name, however many it is written in: the
        // rows of ink from its top down to the first row without any.
        const auto ink = desktop::theme(view.theme_id()).node_text;
        const auto inked = [&](int y) {
            for (int x = 0; x < painted.width(); ++x)
                if (painted.pixelColor(x, y) == ink) return true;
            return false;
        };
        int top = 0;
        while (top < painted.height() && !inked(top)) ++top;
        int bottom = top;
        while (bottom + 1 < painted.height() && inked(bottom + 1)) ++bottom;
        require(top < painted.height(), "The name is painted");
        return bottom - top + 1;
    };
    const auto ordinary = lettering_height();
    haul({148, 58}, {150, 60});
    require(editor.project().layout.at(ref) == domain::Rect{0, 0, 300, 120}, "A corner pulled out doubles the box");
    const auto doubled = lettering_height();
    require(std::abs(doubled - 2 * ordinary) <= 3, "And draws the name twice as tall");
    haul({298, 60}, {120, 0});
    require(std::abs(lettering_height() - doubled) <= 1, "Pulled only wider after that, the name is no taller");
    require(editor.undo() && editor.undo(), "Both pulls undone");
    require(std::abs(lettering_height() - ordinary) <= 1, "And the name is back to its ordinary size");
    require(editor.project().lettering.empty(), "With nothing kept for it");
}

// A picture -- a Dr.icon above all -- is pulled by any of its eight handles
// (Zain, 2026-10-15), as small as a symbol may be and as large. It is never
// stretched, so whichever handle is pulled the whole picture is scaled and
// keeps its proportions: a corner from the corner opposite, an edge from the
// edge opposite, staying centred along the edge. One pull is one step of
// history, called what it is.
void picture_resize_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    QImage art(64, 64, QImage::Format_ARGB32);
    art.fill(Qt::transparent);
    {
        QPainter painter(&art);
        painter.setBrush(QColor(40, 110, 220));
        painter.setPen(Qt::NoPen);
        painter.drawEllipse(QRectF(8, 8, 48, 48));
    }
    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    art.save(&buffer, "PNG");
    const std::vector<std::uint8_t> bytes(png.begin(), png.end());
    const domain::ElementRef icon = *editor.create_picture("Icon", {0, 0, 80, 80}, bytes).created;
    const domain::ElementRef wide = *editor.create_picture("Banner", {400, 0, 160, 80}, bytes).created;
    const domain::ElementRef entity = *editor.create_entity("Student", {0, 300, 133, 77}).created;
    desktop::DiagramView view(editor);
    QString refused;
    view.on_edit = [&](const application::EditResult& result) { if (!result) refused = QString::fromStdString(result.error); };
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(200, 100);
    QApplication::processEvents();
    const auto box_of = [&](const domain::ElementRef& ref) { return editor.project().layout.at(ref); };
    // A handle sits just inside the picture, half a grip in from the edge or
    // corner it stands on; `grip` is that point in the picture's own terms.
    const auto haul = [&](const domain::ElementRef& ref, QPointF grip, QPoint by) {
        view.select_elements({ref});
        QApplication::processEvents();
        const auto& at = box_of(ref);
        const auto start = view.mapFromScene(QPointF(at.x, at.y) + grip);
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, start + by, Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseButtonRelease, start + by, Qt::LeftButton, Qt::NoButton);
        view.synchronize();
        return box_of(ref);
    };
    const auto undo = [&] {
        require(editor.undo(), "The pull undoes");
        view.synchronize();
        QApplication::processEvents();
    };

    // The right edge: the left stays, the picture grows to the right and
    // keeps its proportions, centred where it was down the edge.
    auto box = haul(icon, {75.5, 40}, {40, 0});
    require(refused.isEmpty(), "Pulling a picture is not refused");
    require(box == domain::Rect{0, -20, 120, 120}, "The right edge scales the whole picture, from the left edge");
    require(editor.undo_label() == "Resize picture", "It is its own step of history");
    undo();
    require(box_of(icon) == domain::Rect{0, 0, 80, 80}, "And it undoes to the size it was");

    // The left and top edges move the picture as well as sizing it, in the
    // same one step.
    box = haul(icon, {4.5, 40}, {-40, 0});
    require(box == domain::Rect{-40, -20, 120, 120}, "The left edge scales it from the right edge");
    undo();
    box = haul(icon, {40, 4.5}, {0, -40});
    require(box == domain::Rect{-20, -40, 120, 120}, "The top edge scales it from the bottom edge");
    undo();
    box = haul(icon, {40, 75.5}, {0, 20});
    require(box == domain::Rect{-10, 0, 100, 100}, "The bottom edge scales it from the top edge");
    undo();

    // A corner, from the corner opposite.
    box = haul(icon, {75.5, 75.5}, {60, 60});
    require(box == domain::Rect{0, 0, 140, 140}, "The bottom right corner scales it from the top left");
    undo();
    box = haul(icon, {4.5, 4.5}, {-20, -20});
    require(box == domain::Rect{-20, -20, 100, 100}, "The top left corner scales it from the bottom right");
    undo();

    // Down to a mark beside a name, and no further; up to the largest.
    box = haul(icon, {75.5, 40}, {-400, 0});
    require(box.width == domain::min_picture_size && box.height == domain::min_picture_size && box.x == 0,
            "Pulled past the smallest, it stops there, square still and its left edge where it was");
    // Made that small, its handles shrink with it, so it is still seen and
    // still taken hold of: its middle carries it, its corner pulls it out.
    {
        view.select_elements({icon});
        QApplication::processEvents();
        const auto small = box_of(icon);
        const auto middle = view.mapFromScene(QPointF(small.x + small.width / 2, small.y + small.height / 2));
        mouse(view, QEvent::MouseButtonPress, middle, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, middle + QPoint(30, 0), Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseButtonRelease, middle + QPoint(30, 0), Qt::LeftButton, Qt::NoButton);
        view.synchronize();
        const auto carried = box_of(icon);
        require(carried.width == small.width && carried.height == small.height && carried.x > small.x + 20,
                "A picture made its smallest is carried by its middle, not pulled");
        undo();
        // Its top left handle is four wide, two in from the corner.
        box = haul(icon, {2, 2}, {-24, -24});
        require(box == domain::Rect{small.x - 24, small.y - 24, 40, 40},
                "And its corner, small as its handle is, pulls it out again");
        undo();
    }
    undo();
    box = haul(icon, {75.5, 40}, {9000, 0});
    require(box.width == domain::max_picture_size && box.height == domain::max_picture_size,
            "Pulled past the largest, it stops there");
    undo();
    box = haul(icon, {75.5, 75.5}, {-300, -300});
    require(box.width == domain::min_picture_size && box.height == domain::min_picture_size && box.x == 0 && box.y == 0,
            "A corner pulled past the opposite one stops at the smallest");
    undo();

    // A picture that is not square keeps its own proportions.
    box = haul(wide, {155.5, 40}, {80, 0});
    require(box == domain::Rect{400, -20, 240, 120}, "A wide picture pulled by its edge stays twice as wide as it is tall");
    undo();

    // Pressed and let go where it was, nothing is written; Escape abandons a
    // pull in progress.
    view.select_elements({icon});
    QApplication::processEvents();
    const auto before_click = editor.revision();
    const auto grip = view.mapFromScene(QPointF(75.5, 40));
    mouse(view, QEvent::MouseButtonPress, grip, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, grip, Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == before_click, "A handle clicked and let go writes nothing");
    mouse(view, QEvent::MouseButtonPress, grip, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, grip + QPoint(100, 0), Qt::NoButton, Qt::LeftButton);
    // A picture's item is found where it stands, since nothing is written on it.
    const auto drawn_width = [&] {
        // The picture alone stands there; it lies beneath the diagram's
        // shapes, so it is not looked for among them.
        const auto here = view.scene()->items(QPointF(40, 40));
        if (here.isEmpty()) throw std::runtime_error("Missing picture");
        return here.front()->boundingRect().width();
    };
    require(drawn_width() > 150, "A picture being pulled is drawn at the size asked for");
    require(box_of(icon) == domain::Rect{0, 0, 80, 80}, "Though nothing is written until it is let go");
    view.cancel_interaction();
    QApplication::processEvents();
    require(box_of(icon) == domain::Rect{0, 0, 80, 80} && editor.revision() == before_click, "Escape abandons the pull");
    require(drawn_width() < 100, "And the picture is drawn at its stored size again");

    // The edit is for pictures alone.
    require(!editor.resize_pictures({{entity, domain::Rect{0, 300, 200, 200}}}), "An entity is not resized as a picture");
    require(refused.isEmpty(), "And nothing pulled on the canvas was refused");
}

void relationship_resize_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto first = *editor.create_entity("First", {-340, -140, 148, 86}).created;
    const auto second = *editor.create_entity("Second", {230, 160, 148, 86}).created;
    desktop::DiagramView view(editor);
    view.resize(1000, 700);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();
    view.set_tool(desktop::Tool::Relationship);
    click(view, {0, 0});
    const domain::ElementRef ref{editor.project().relationships.begin()->first};
    const auto original = editor.project().layout.at(ref);
    // Placed at the size its own name is fitted to (2026-10-12), which for
    // "Relationship" is the widest a name makes a diamond on its own.
    const auto fitted = desktop::fitted_body(desktop::NamedShape::Relationship, "Relationship", view.font());
    require(original.width == fitted.width && original.height == fitted.height,
            "Placed at the size its name is fitted to");
    require(editor.rename(ref, "Owns"), "Name the diamond");
    const auto id = std::get<domain::RelationshipId>(ref);
    require(editor.connect(id, std::get<domain::EntityId>(first)), "Connect the first entity");
    const auto pinned = editor.connect(id, std::get<domain::EntityId>(second));
    require(pinned, "Connect the second entity");
    require(editor.pin_connector(*pinned.participant, 0.65, -2.5), "Pin one connector to its outline");
    view.synchronize();
    view.select_elements({ref});
    auto* node = find_node(view, "Owns");
    const auto untouched = editor.project().layout;
    const auto check_drawing = [&] {
        const auto box = node->sceneBoundingRect().adjusted(4, 4, -4, -4);
        require(!node->shape().contains({box.width() * 0.1, box.height() * 0.1}),
                "The resized body retains its diamond outline");
        // Both automatic and pinned joins must meet the current diamond,
        // including during a drag before anything is written to the model.
        const auto target = find_node(view, "First")->sceneBoundingRect().center();
        auto automatic = target - box.center();
        if (view.line_style() == desktop::LineStyle::Elbow)
            automatic = std::abs(automatic.x()) / box.width() >= std::abs(automatic.y()) / box.height()
                ? QPointF(automatic.x(), 0) : QPointF(0, automatic.y());
        for (const auto direction : {automatic, QPointF(std::cos(0.65), std::sin(0.65))}) {
            const auto divisor = std::abs(direction.x()) / (box.width() / 2)
                               + std::abs(direction.y()) / (box.height() / 2);
            const auto join = box.center() + direction / divisor;
            bool attached = false;
            for (auto* item : view.scene()->items())
                if (item->zValue() < 0 && item->shape().contains(item->mapFromScene(join))) attached = true;
            require(attached, "The connector remains attached to the resized diamond boundary");
        }
        // Inspect the actual painted label, independently of the hit shape.
        QImage painted(QSize(static_cast<int>(box.width()), static_cast<int>(box.height())), QImage::Format_ARGB32);
        painted.fill(Qt::transparent);
        QPainter painter(&painted);
        node->paint(&painter, nullptr, nullptr);
        painter.end();
        QRect ink;
        const auto text_colour = desktop::theme(view.theme_id()).node_text;
        for (int y = 0; y < painted.height(); ++y)
            for (int x = 0; x < painted.width(); ++x)
                if (painted.pixelColor(x, y) == text_colour) ink |= QRect(x, y, 1, 1);
        require(!ink.isEmpty(), "The relationship label is painted");
        require(std::abs(ink.center().x() - box.width() / 2) < 5
                    && std::abs(ink.center().y() - box.height() / 2) < 7,
                "The painted label stays centered after resizing");
    };
    check_drawing();
    // Corners first, then edges, matching the existing entity handles.
    const auto w = original.width;
    const auto h = original.height;
    const std::array<QPointF, 8> grips{{{2, 2}, {w - 2, 2}, {w - 2, h - 2}, {2, h - 2},
                                      {w / 2, 2}, {w - 2, h / 2}, {w / 2, h - 2}, {2, h / 2}}};
    const std::array<QPoint, 8> travel{{{-30, -20}, {30, -20}, {30, 20}, {-30, 20},
                                      {0, -20}, {30, 0}, {0, 20}, {-30, 0}}};
    for (std::size_t handle = 0; handle < grips.size(); ++handle) {
        const auto start = view.mapFromScene(QPointF(original.x, original.y) + grips[handle]);
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, start + travel[handle], Qt::NoButton, Qt::LeftButton);
        require(editor.project().layout == untouched, "Dragging previews without changing stored dimensions");
        check_drawing();
        mouse(view, QEvent::MouseButtonRelease, start + travel[handle], Qt::LeftButton, Qt::NoButton);
        view.synchronize();
        const auto resized = editor.project().layout.at(ref);
        require(resized.width == original.width + std::abs(travel[handle].x())
                    && resized.height == original.height + std::abs(travel[handle].y()),
                "Every relationship handle pulls its own sides independently");
        require(resized.x == original.x + std::min(0, travel[handle].x())
                    && resized.y == original.y + std::min(0, travel[handle].y()),
                "The opposite sides stay fixed");
        require(editor.undo_label() == "Resize relationship", "Resize has its own undo step");
        check_drawing();
        require(editor.undo(), "Undo the relationship resize");
        view.synchronize();
        require(editor.project().layout == untouched, "Undo restores every original size exactly");
        require(editor.redo(), "Redo the relationship resize");
        view.synchronize();
        require(editor.project().layout.at(ref) == resized, "Redo restores the resized diamond");
        require(editor.undo(), "Restore for the next handle");
        view.synchronize();
    }
    const auto revision = editor.revision();
    const auto start = view.mapFromScene(QPointF(original.x, original.y) + grips[2]);
    mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseButtonRelease, start, Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == revision, "Clicking a handle preserves existing dimensions and history");
    mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
    mouse(view, QEvent::MouseMove, start + QPoint(40, 30), Qt::NoButton, Qt::LeftButton);
    key(view, Qt::Key_Escape);
    mouse(view, QEvent::MouseButtonRelease, start + QPoint(40, 30), Qt::LeftButton, Qt::NoButton);
    require(editor.revision() == revision && editor.project().layout == untouched,
            "Escape restores the original size without a history entry");
    check_drawing();
}

// Turning the wheel moves the diagram; holding the platform's zoom key and
// turning it makes the diagram larger or smaller, about the pointer.
// An entity is a box holding a name, and how wide and how tall it is are two
// questions, so it answers to each of its four edges and each of its corners:
// the side that is pulled moves, the side opposite it stays exactly where it
// was, and nothing is offered at all until the entity is the thing chosen.
void entity_resize_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto student = std::get<domain::EntityId>(*editor.create_entity("Student", {-80, -40, 160, 80}).created);
    const domain::ElementRef ref{student};
    desktop::DiagramView view(editor);
    view.resize(900, 600);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();

    const auto box = [&] { return editor.project().layout.at(ref); };
    const auto pull = [&](const QPointF& from, const QPoint& by) {
        const auto start = view.mapFromScene(from);
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        for (int step = 1; step <= 6; ++step)
            mouse(view, QEvent::MouseMove, start + by * step / 6, Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseButtonRelease, start + by, Qt::LeftButton, Qt::NoButton);
        view.synchronize();
        QApplication::processEvents();
    };

    // Nothing is pulled while nothing is chosen: the same press moves the
    // entity, which is what a press on a shape means.
    const auto placed = box();
    pull(QPointF(placed.x + placed.width, placed.y + placed.height / 2), QPoint(40, 0));
    require(std::abs(box().width - placed.width) < 0.01, "An unchosen entity is not resized by its edge");
    require(box().x > placed.x + 20, "It is moved, which is what pressing a shape does");
    require(editor.undo(), "Put it back");
    view.synchronize();
    QApplication::processEvents();

    view.select_elements({ref});
    QApplication::processEvents();

    // The right edge: wider, and standing where it always stood.
    const auto before = box();
    pull(QPointF(before.x + before.width - 2, before.y + before.height / 2), QPoint(60, 0));
    require(box().width > before.width + 40, "The right edge makes the entity wider");
    require(std::abs(box().x - before.x) < 0.01 && std::abs(box().y - before.y) < 0.01,
            "Without moving it, because the side opposite the one pulled stays put");
    require(std::abs(box().height - before.height) < 0.01, "And without changing its height");
    require(editor.undo_label() == "Resize entity", "The history says what was done");
    require(editor.undo(), "Undo the pull");
    view.synchronize();
    QApplication::processEvents();
    require(std::abs(box().width - before.width) < 0.01, "Which puts the width back");
    require(editor.redo(), "And redo pulls it out again");
    view.synchronize();
    QApplication::processEvents();
    const auto widened = box();

    // The left edge, which carries the entity's corner with it and leaves the
    // right-hand side alone.
    pull(QPointF(widened.x + 2, widened.y + widened.height / 2), QPoint(-50, 0));
    require(box().x < widened.x - 30, "The left edge follows the pointer");
    require(std::abs(box().x + box().width - (widened.x + widened.width)) < 0.01,
            "And the right-hand side stays where it was");

    // The bottom edge, and then the top one, which is the same thing the other
    // way up.
    const auto sideways = box();
    pull(QPointF(sideways.x + sideways.width / 2, sideways.y + sideways.height - 2), QPoint(0, 45));
    require(box().height > sideways.height + 30, "The bottom edge makes it taller");
    require(std::abs(box().y - sideways.y) < 0.01, "With its top edge where it was");
    const auto taller = box();
    pull(QPointF(taller.x + taller.width / 2, taller.y + 2), QPoint(0, -35));
    require(box().y < taller.y - 20, "The top edge reaches upwards");
    require(std::abs(box().y + box().height - (taller.y + taller.height)) < 0.01,
            "And leaves the bottom where it was");

    // A corner pulls both of the sides that meet at it.
    const auto squared = box();
    pull(QPointF(squared.x + squared.width - 2, squared.y + squared.height - 2), QPoint(40, 40));
    require(box().width > squared.width + 20 && box().height > squared.height + 20,
            "A corner pulls two sides at once");
    require(std::abs(box().x - squared.x) < 0.01 && std::abs(box().y - squared.y) < 0.01,
            "Leaving the corner opposite it alone");

    // Its attributes and relationships are drawn from its outline, so they
    // follow the edge while it is being pulled rather than snapping into place
    // when it is let go. And a pull can be abandoned: Escape puts the box back
    // exactly as it was, with the lines back on it.
    {
        const auto attribute = editor.create_attribute("ID", {200, -20, 150, 60}, domain::AttributeOwner{student});
        require(attribute && attribute.created, "An attribute to hang off it");
        view.synchronize();
        view.select_elements({ref});
        QApplication::processEvents();
        auto* link = find_edge(view, QStringLiteral("Attribute ownership"));
        const auto drawn = link->shape();
        const auto kept = box();
        const auto outline = find_node(view, "Student")->sceneBoundingRect();
        const auto start = view.mapFromScene(QPointF(kept.x + kept.width - 2, kept.y + kept.height / 2));
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, start + QPoint(50, 0), Qt::NoButton, Qt::LeftButton);
        require(find_node(view, "Student")->sceneBoundingRect().width() > outline.width() + 30,
                "The pull is previewed on the diagram before it is let go");
        require(link->shape() != drawn, "And the line it carries follows its edge");
        const auto revision = editor.revision();
        key(view, Qt::Key_Escape);
        QApplication::processEvents();
        require(editor.revision() == revision, "Escape writes nothing");
        require(find_node(view, "Student")->sceneBoundingRect() == outline,
                "And puts the box back as it was");
        require(link->shape() == drawn, "With the line back on it");
        mouse(view, QEvent::MouseButtonRelease, start + QPoint(50, 0), Qt::LeftButton, Qt::NoButton);
        require(editor.revision() == revision, "Letting go after Escape asks for nothing");
        require(editor.erase({*attribute.created}), "Put the diagram back");
        view.synchronize();
        view.select_elements({ref});
        QApplication::processEvents();
    }

    // A pull that runs far past what a box may be asks for the end of the
    // range rather than for the entity to be turned inside out.
    const auto full = box();
    pull(QPointF(full.x + 2, full.y + full.height / 2), QPoint(4000, 0));
    require(box().width == domain::min_entity_width, "However far an edge is pulled past the other one");
    require(box().x + box().width <= full.x + full.width + 0.01, "And the far side is still the far side");
}

void wheel_zoom_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    require(editor.create_entity("Student", {0, 0, 160, 80}), "Wheel fixture");
    desktop::DiagramView view(editor);
    view.resize(800, 600);
    view.show();
    QApplication::processEvents();

    const auto turn = [&](int notches, Qt::KeyboardModifiers held) {
        const QPointF at(400, 300);
        QWheelEvent event(at, view.viewport()->mapToGlobal(at.toPoint()), QPoint(), QPoint(0, notches),
                          Qt::NoButton, held, Qt::NoScrollPhase, false);
        QApplication::sendEvent(view.viewport(), &event);
        QApplication::processEvents();
    };

    // A bare turn is a scroll: the diagram moves and stays the size it was.
    const auto resting = view.zoom_factor();
    const auto down = view.verticalScrollBar()->value();
    turn(-120, Qt::NoModifier);
    require(view.zoom_factor() == resting, "A bare turn does not change the size of anything");
    require(view.verticalScrollBar()->value() != down, "It moves the diagram instead");

    // Held, the same turn zooms. Qt reports Command on a Mac and Control
    // elsewhere as the same modifier, so one test covers both.
    turn(120, Qt::ControlModifier);
    const auto closer = view.zoom_factor();
    require(closer > resting, "Held and turned forwards, the diagram grows");
    turn(-120, Qt::ControlModifier);
    require(view.zoom_factor() < closer, "And turned back, it shrinks again");

    // It stays within the same bounds the buttons keep to, however long the
    // wheel is turned.
    for (int i = 0; i < 60; ++i) turn(120, Qt::ControlModifier);
    const auto nearest = view.zoom_factor();
    turn(120, Qt::ControlModifier);
    require(view.zoom_factor() == nearest, "Zooming in stops where zooming in stops");
    for (int i = 0; i < 120; ++i) turn(-120, Qt::ControlModifier);
    const auto farthest = view.zoom_factor();
    turn(-120, Qt::ControlModifier);
    require(view.zoom_factor() == farthest, "And zooming out stops where zooming out stops");
    require(farthest < nearest, "The two ends are not the same place");
}
} // namespace

// A new entity, attribute or relationship is placed at its default and, named,
// made wide enough up to the Relationship budget (Zain, 2026-10-11). Width is measured
// as the name is painted, from fonts that differ from machine to machine, so
// what is checked is what is painted -- the whole name, inside its shape --
// rather than any width in figures.
void content_aware_size_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    desktop::DiagramView view(editor);
    view.resize(1400, 900);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();
    const auto is_entity = [](const domain::ElementRef& ref) { return std::holds_alternative<domain::EntityId>(ref); };
    const auto is_relationship = [](const domain::ElementRef& ref) {
        return std::holds_alternative<domain::RelationshipId>(ref);
    };
    const auto body_of = [&](const domain::ElementRef& ref) {
        return is_entity(ref) ? desktop::entity_body
             : is_relationship(ref) ? desktop::relationship_body : desktop::attribute_body;
    };

    // Placed with the tools, each is the size its placeholder is fitted to
    // (2026-10-12): never narrower than "Attributes" needs nor wider than
    // "Relationship" needs (2026-10-13), the short Entity and Attribute
    // placeholders exactly as wide as "Attributes" needs, and as tall as that
    // width makes it in proportion to its default. Not measured against the
    // default's width: where the lettering is wide, as on Linux, "Attributes"
    // needs more than the default gives, and the shape is made wider.
    {
        const std::array<std::pair<desktop::Tool, QPointF>, 3> placing{
            {{desktop::Tool::Entity, {-400, 0}},
             {desktop::Tool::Relationship, {0, 0}},
             {desktop::Tool::Attribute, {400, 0}}}};
        for (const auto& [tool, at] : placing) {
            const auto before = editor.project().layout;
            view.set_tool(tool);
            click(view, at);
            require(editor.project().layout.size() == before.size() + 1, "An element placed with its tool");
            for (const auto& [ref, box] : editor.project().layout)
                if (!before.contains(ref)) {
                    const auto shape = is_entity(ref) ? desktop::NamedShape::Entity
                                     : is_relationship(ref) ? desktop::NamedShape::Relationship
                                                            : desktop::NamedShape::Attribute;
                    const auto fitted = desktop::fitted_body(shape, is_entity(ref) ? QStringLiteral("Entity")
                                                                    : is_relationship(ref) ? QStringLiteral("Relationship")
                                                                                           : QStringLiteral("Attribute"),
                                                             view.font());
                    require(box.width == fitted.width && box.height == fitted.height,
                            "Placed at the size its placeholder is fitted to");
                    const auto least = desktop::fitted_body(shape, QStringLiteral("Attributes"), view.font());
                    const auto widest = desktop::fitted_body(shape, QStringLiteral("Relationship"), view.font());
                    require(box.width >= least.width && box.width <= widest.width,
                            "Never narrower than Attributes needs, nor wider than Relationship needs");
                    if (!is_relationship(ref))
                        require(box.width == least.width && box.height == least.height,
                                "A short placeholder is placed as wide as Attributes needs");
                    const auto body = body_of(ref);
                    const auto least_height = std::max(domain::min_entity_height,
                                                       std::ceil(body.height * domain::min_entity_width / body.width));
                    require(box.height == std::clamp(std::round(body.height * box.width / body.width), least_height,
                                                     body.height),
                            "As tall as its width makes it in proportion to its default, and no taller than that");
                }
        }
        require(desktop::entity_body.width == 133 && desktop::entity_body.height == 77
                    && desktop::relationship_body.width == 171 && desktop::relationship_body.height == 99
                    && desktop::attribute_body.width == 135 && desktop::attribute_body.height == 54,
                "The defaults are the ones settled on");
        require(editor.undo() && editor.undo() && editor.undo(), "Taken away again");
        view.synchronize();
    }

    // What is painted of a name: the pixels in the name's own colour, in the
    // shape's own coordinates.
    const auto ink_of = [&](const QString& name) {
        view.synchronize();
        auto* node = find_node(view, name);
        const auto box = node->boundingRect();
        QImage painted(QSize(static_cast<int>(std::ceil(box.right())) + 1,
                             static_cast<int>(std::ceil(box.bottom())) + 1),
                       QImage::Format_ARGB32);
        painted.fill(Qt::transparent);
        QPainter painter(&painted);
        node->paint(&painter, nullptr, nullptr);
        painter.end();
        const auto text = desktop::theme(view.theme_id()).node_text;
        std::vector<QPoint> dots;
        for (int y = 0; y < painted.height(); ++y)
            for (int x = 0; x < painted.width(); ++x)
                if (painted.pixelColor(x, y) == text) dots.emplace_back(x, y);
        return dots;
    };
    const auto span = [](const std::vector<QPoint>& dots) {
        QRect ink;
        for (const auto& dot : dots) ink |= QRect(dot, QSize(1, 1));
        return ink;
    };
    // The same name painted in a box far wider than any name, at the same
    // lettering, so nothing of it can have been cut short there. A shape
    // resized by hand letters its name by the smaller of its two changes from
    // the size it was first resized from, so its height is set to keep that.
    const auto whole_width = [&](const domain::ElementRef& ref, const QString& name) {
        const auto at = editor.project().layout.at(ref);
        const auto base = editor.project().lettering.find(ref);
        const auto tall = base == editor.project().lettering.end()
            ? at.height : domain::lettering_factor(base->second, at.width, at.height) * base->second.height;
        const std::map<domain::ElementRef, domain::Rect> wide{{ref, {at.x, at.y, 1900, tall}}};
        require(is_entity(ref) ? editor.resize_entities(wide)
                : is_relationship(ref) ? editor.resize_relationships(wide) : editor.resize_attributes(wide),
                "Drawn wide enough for any name");
        const auto width = span(ink_of(name)).width();
        require(editor.undo(), "And put back");
        view.synchronize();
        return width;
    };
    // The whole name is written, and every dot of it is inside the room the
    // shape gives a name: 15 in from a box's or an oval's sides, the middle of
    // a diamond and inside its outline, and inside an oval's second ring.
    const auto written_whole_inside = [&](const domain::ElementRef& ref, const QString& name) {
        const auto at = editor.project().layout.at(ref);
        const auto dots = ink_of(name);
        require(!dots.empty(), "The name is painted");
        const auto painted = span(dots).width();
        require(std::abs(painted - whole_width(ref, name)) <= 1, "The whole name is written, not cut short");
        const auto half_w = at.width / 2;
        const auto half_h = at.height / 2;
        for (const auto& dot : dots) {
            const auto x = std::abs(dot.x() + 0.5 - half_w);
            const auto y = std::abs(dot.y() + 0.5 - half_h);
            bool inside = false;
            if (is_entity(ref))
                inside = x <= half_w - 15 + 1 && y <= half_h;
            else if (is_relationship(ref))
                inside = x <= at.width * 0.28 + 1 && x / half_w + y / half_h <= 1;
            else
                inside = x <= half_w - 15 + 1
                      && (x * x) / ((half_w - 5) * (half_w - 5)) + (y * y) / ((half_h - 5) * (half_h - 5)) <= 1;
            require(inside, "Every letter is inside the room its shape gives a name");
        }
    };

    // Named on the diagram, each is made the size its name is fitted to
    // (2026-10-12): never narrower than "Attributes" needs, a short name
    // exactly that wide (2026-10-13) and no taller than the default, a longer
    // one just wide enough, about the same centre. Not measured against the
    // default's width, which wide lettering needs more than for "Attributes".
    struct Named { QString name; bool short_name; };
    const std::array<std::pair<int, std::vector<Named>>, 3> naming{{
        {0, {{"User", true}, {"Student", false}, {"Professor", false}, {"Department", false}}},
        {1, {{"Owns", true}, {"Manages", false}, {"Enrolled", false}, {"Enrolled In", false},
             {"Qualified In", false}, {"Belongs To", false}, {"Works On", false}}},
        {2, {{"ID", true}, {"Age", true}, {"Department Name", false}, {"Employee Number", false},
             {"Date Of Birth", false}}},
    }};
    double y = -600;
    for (const auto& [kind, names] : naming)
        for (const auto& named : names) {
            y += 130;
            const domain::Rect box{-200, y, kind == 0 ? 133.0 : kind == 1 ? 171.0 : 135.0,
                                   kind == 0 ? 77.0 : kind == 1 ? 99.0 : 54.0};
            const auto made = kind == 0 ? editor.create_entity("Entity", box)
                            : kind == 1 ? editor.create_relationship("Relationship", box)
                                        : editor.create_attribute("Attribute", box);
            require(made && made.created, "An element to name");
            const auto ref = *made.created;
            const auto revision = editor.revision();
            require(view.rename(ref, named.name.toStdString()), "Named");
            require(editor.revision() == revision + 1 && editor.undo_label() == "Rename element",
                    "Naming it, widened or not, is one step of history");
            const auto& at = editor.project().layout.at(ref);
            const auto shape = kind == 0 ? desktop::NamedShape::Entity
                             : kind == 1 ? desktop::NamedShape::Relationship : desktop::NamedShape::Attribute;
            const auto fitted = desktop::fitted_body(shape, named.name, view.font());
            require(at.width == fitted.width && at.height == fitted.height, "Made the size its name is fitted to");
            const auto least = desktop::fitted_body(shape, QStringLiteral("Attributes"), view.font());
            require(at.width >= least.width, "Never narrower than Attributes needs");
            if (named.short_name)
                require(at.width == least.width && at.height == least.height && at.height <= box.height,
                        "A short name makes it as wide as Attributes needs, and no taller than its default");
            require(at.width >= domain::min_entity_width && at.height >= domain::min_entity_height,
                    "Never under the domain's least");
            require(std::abs(at.x + at.width / 2 - (box.x + box.width / 2)) < 0.01
                        && std::abs(at.y + at.height / 2 - (box.y + box.height / 2)) < 0.01,
                    "Fitted about its centre, where it was put");
            require(editor.project().lettering.empty(), "Its name is lettered as it was: nothing is kept for it");
            const auto reference = desktop::fitted_body(shape, QStringLiteral("Relationship"), view.font());
            require(at.width <= reference.width, "No wider than Relationship needs");
            if (desktop::name_layout(shape, named.name, view.font(), 1.0, QRectF(0, 0, at.width, at.height)).lines.size() == 1)
                written_whole_inside(ref, named.name);
            else
                require_written_in_lines(view, editor, ref, shape, false);
        }

    // A longer name widens it; a shorter one narrows it again; and Undo
    // takes each name and its size back together. The longer name is longer
    // than "Attributes", the narrowest a name makes a shape, so that it is
    // made wider than the shorter one.
    {
        const auto ref = *editor.create_entity("User", {600, 0, 133, 77}).created;
        require(view.rename(ref, "Department"), "A longer name");
        const auto grown = editor.project().layout.at(ref);
        const auto department = desktop::fitted_body(desktop::NamedShape::Entity, "Department", view.font());
        require(grown.width == department.width && grown.height == department.height,
                "Made the size Department is fitted to");
        written_whole_inside(ref, "Department");
        require(view.rename(ref, "User"), "A shorter name again");
        const auto user = desktop::fitted_body(desktop::NamedShape::Entity, "User", view.font());
        const auto narrowed = editor.project().layout.at(ref);
        require(narrowed.width == user.width && narrowed.height == user.height && narrowed.width < grown.width,
                "Which narrows it again");
        require(editor.undo(), "Undo the shorter name");
        require(editor.project().layout.at(ref) == grown, "The size comes back with the name");
        require(editor.undo(), "Undo the longer name");
        require(editor.project().layout.at(ref) == (domain::Rect{600, 0, 133, 77})
                    && domain::name(editor.project(), ref) == "User",
                "Which takes its widening back with it");

        // Typed into the name box on the diagram, as somebody names it.
        view.synchronize();
        view.begin_rename(ref);
        auto* field = view.findChild<QLineEdit*>("inlineName");
        require(view.renaming() && field != nullptr, "The name box is open");
        field->setText("Student");
        key_to(field, Qt::Key_Return);
        require(domain::name(editor.project(), ref) == "Student", "Named in the box");
        const auto student = desktop::fitted_body(desktop::NamedShape::Entity, "Student", view.font());
        require(editor.project().layout.at(ref).width == student.width
                    && editor.project().layout.at(ref).height == student.height,
                "Made the size its name is fitted to");
        written_whole_inside(ref, "Student");

        // Resized by hand, it keeps the size it was given, however long its
        // name (2026-10-12): a name too long for it is written in more lines
        // inside it rather than the hand's size being changed.
        require(editor.resize_entities({{ref, {600, 0, 90, 77}}}), "Pulled narrower than its name");
        view.synchronize();
        require(editor.project().layout.at(ref).width == 90, "It keeps the width it was given");
        require(view.rename(ref, "Professor"), "Renamed longer after that");
        require(editor.project().layout.at(ref) == (domain::Rect{600, 0, 90, 77}), "Which keeps the size the hand gave it");
        {
            const auto lettering = domain::lettering_factor(editor.project().lettering.at(ref), 90, 77);
            const auto laid = desktop::name_layout(desktop::NamedShape::Entity, "Professor", view.font(), lettering,
                                                   QRectF(0, 0, 90, 77));
            if (laid.lines.size() == 1) written_whole_inside(ref, "Professor");
            else require_written_in_lines(view, editor, ref, desktop::NamedShape::Entity, laid.elided);
        }
        require(editor.resize_entities({{ref, {600, 0, 80, 60}}}), "And it can still be pulled smaller by hand");
        require(editor.project().layout.at(ref) == (domain::Rect{600, 0, 80, 60}), "To exactly what it was given");
    }

    // A widened diamond is met by its lines where its outline now is.
    {
        const auto first = *editor.create_entity("Left", {-1400, 900, 133, 77}).created;
        const auto second = *editor.create_entity("Right", {-800, 1100, 133, 77}).created;
        const auto made = editor.create_relationship("Has", {-1100, 1000, 120, 99});
        const auto id = std::get<domain::RelationshipId>(*made.created);
        require(editor.connect(id, std::get<domain::EntityId>(first)), "Connect the first entity");
        const auto pinned = editor.connect(id, std::get<domain::EntityId>(second));
        require(pinned && editor.pin_connector(*pinned.participant, 0.65, -2.5), "Pin one line to its outline");
        require(view.rename(*made.created, "Is Qualified In"), "A name that needs a wider diamond");
        view.synchronize();
        QApplication::processEvents();
        auto* node = find_node(view, "Is Qualified In");
        const auto box = node->sceneBoundingRect().adjusted(4, 4, -4, -4);
        require(box.width() > 120 - 8 + 1, "The narrow diamond was widened");
        const auto target = find_node(view, "Left")->sceneBoundingRect().center();
        auto automatic = target - box.center();
        if (view.line_style() == desktop::LineStyle::Elbow)
            automatic = std::abs(automatic.x()) / box.width() >= std::abs(automatic.y()) / box.height()
                ? QPointF(automatic.x(), 0) : QPointF(0, automatic.y());
        for (const auto direction : {automatic, QPointF(std::cos(0.65), std::sin(0.65))}) {
            const auto divisor = std::abs(direction.x()) / (box.width() / 2)
                               + std::abs(direction.y()) / (box.height() / 2);
            const auto join = box.center() + direction / divisor;
            bool attached = false;
            for (auto* item : view.scene()->items())
                if (item->zValue() < 0 && item->shape().contains(item->mapFromScene(join))) attached = true;
            require(attached, "Each line, free or pinned, meets the widened diamond's outline");
        }
    }

    // A relationship's name is lettered as an attribute's is: as large, and
    // no smaller for standing in a diamond.
    {
        SequentialIds other_ids;
        application::Editor other(other_ids);
        other.create_relationship("Hold", {0, 0, 171, 99});
        other.create_attribute("Bold", {0, 300, 135, 54});
        desktop::DiagramView beside(other);
        beside.resize(600, 600);
        beside.show();
        beside.synchronize();
        QApplication::processEvents();
        const auto height_of = [&](const QString& name) {
            auto* node = find_node(beside, name);
            QImage painted(node->boundingRect().size().toSize() + QSize(2, 2), QImage::Format_ARGB32);
            painted.fill(Qt::transparent);
            QPainter painter(&painted);
            node->paint(&painter, nullptr, nullptr);
            painter.end();
            const auto text = desktop::theme(beside.theme_id()).node_text;
            int top = painted.height(), bottom = -1;
            for (int y = 0; y < painted.height(); ++y)
                for (int x = 0; x < painted.width(); ++x)
                    if (painted.pixelColor(x, y) == text) { top = std::min(top, y); bottom = std::max(bottom, y); }
            return bottom - top + 1;
        };
        require(std::abs(height_of("Hold") - height_of("Bold")) <= 1,
                "A relationship's name is as tall as an attribute's");
    }
    editor.mark_saved(editor.revision());
}

// Regression: Relationship fits without ellipsis, but an unlimited name does not
// demand an unlimited box. Compare the actual NodeItem paint against the full
// reference text, using the settled typography rather than fixed pixel widths.
void relationship_fits_without_ellipsis_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    desktop::DiagramView view(editor);
    const QString professor = QStringLiteral("Professor");
    const QString reference_name = QStringLiteral("Relationship");
    const QString long_name = QStringLiteral("Department Of International Computer Science And Applied Mathematics");
    const auto font_for = [&](double factor = 1.0) {
        QFont font = view.font();
        font.setFamilies({"Inter", "Source Sans 3", "IBM Plex Sans", "Segoe UI Variable",
                          "Segoe UI", "SF Pro Text", "Helvetica Neue", "Arial"});
        font.setStyleHint(QFont::SansSerif, QFont::PreferAntialias);
        font.setPointSizeF(9.0 * desktop::lettering_scale * factor);
        font.setWeight(QFont::Bold);
        return font;
    };
    const auto text_ink = [&](const QImage& image) {
        std::vector<QPoint> ink;
        const auto colour = desktop::theme(view.theme_id()).node_text;
        for (int y = 0; y < image.height(); ++y)
            for (int x = 0; x < image.width(); ++x)
                if (image.pixelColor(x, y) == colour) ink.emplace_back(x, y);
        return ink;
    };
    const auto available_room = [](desktop::NamedShape shape, double width, double height, const QFont& font) {
        if (shape == desktop::NamedShape::Relationship) return width * (1 - 2 * 0.22);
        auto room = width - 30;
        if (shape == desktop::NamedShape::Attribute) {
            const QFontMetricsF metrics(font);
            const auto baseline = (metrics.ascent() - metrics.descent()) / 2;
            const auto reach = std::max(std::abs(baseline), std::abs(baseline - metrics.capHeight()));
            const auto ring = height / 2 - 5;
            if (ring > reach) room = std::min(room, (width - 10) * std::sqrt(1 - reach * reach / (ring * ring)));
        }
        return std::max(0.0, room);
    };
    const auto painted_as = [&](const domain::ElementRef& ref, desktop::NamedShape shape,
                                const QString& expected) {
        view.synchronize();
        auto* node = find_node(view, QString::fromStdString(domain::name(editor.project(), ref)));
        const auto box = editor.project().layout.at(ref);
        const auto base = editor.project().lettering.find(ref);
        const auto factor = base == editor.project().lettering.end() ? 1.0
            : domain::lettering_factor(base->second, box.width, box.height);
        const auto font = font_for(factor);
        // Supersample so even manually reduced thin strokes have solid ink;
        // this avoids depending on a platform's small-font antialiasing.
        QImage actual((node->boundingRect().size().toSize() + QSize(8, 8)) * 4, QImage::Format_ARGB32);
        actual.setDevicePixelRatio(4);
        actual.fill(Qt::transparent);
        QPainter painter(&actual);
        painter.setFont(view.font());
        node->paint(&painter, nullptr, nullptr);
        painter.end();
        QImage reference(actual.size(), QImage::Format_ARGB32);
        reference.setDevicePixelRatio(4);
        reference.fill(Qt::transparent);
        QPainter writer(&reference);
        writer.setRenderHint(QPainter::Antialiasing);
        writer.setFont(font);
        writer.setPen(desktop::theme(view.theme_id()).node_text);
        const auto room = available_room(shape, box.width, box.height, font);
        const auto inset = (box.width - room) / 2;
        writer.drawText(QRectF(0, 0, box.width, box.height).adjusted(inset, 8, -inset, -8), Qt::AlignCenter, expected);
        writer.end();
        require(expected.isEmpty() || !text_ink(reference).empty(), "Reference text has visible ink");
        if (text_ink(actual) != text_ink(reference))
            throw std::runtime_error("Actual painted text differs from: " + expected.toStdString()
                + "; shape=" + std::to_string(static_cast<int>(shape))
                + "; width=" + std::to_string(box.width) + "; factor=" + std::to_string(factor));
    };

    // Explicit geometry stays authoritative. At the new 9 bold lettering,
    // Professor also fits this compact box without needing automatic growth.
    const auto legacy = *editor.create_entity("Professor", {0, 0, 133, 77}).created;
    const auto old_text = QFontMetricsF(font_for()).elidedText(professor, Qt::ElideRight, 103);
    std::cout << "Professor in an unfitted 133x77 entity: " << old_text.toStdString()
              << "; rendered width=" << QFontMetricsF(font_for()).horizontalAdvance(professor)
              << "; text room=103; lettering=1\n";
    painted_as(legacy, desktop::NamedShape::Entity, old_text);

    for (const auto shape : {desktop::NamedShape::Entity, desktop::NamedShape::Relationship, desktop::NamedShape::Attribute}) {
        const auto body = shape == desktop::NamedShape::Entity ? desktop::entity_body
                        : shape == desktop::NamedShape::Relationship ? desktop::relationship_body : desktop::attribute_body;
        const auto make = [&](const QString& name) {
            const domain::Rect box{0, 200, desktop::width_for_name(shape, name, view.font()), body.height};
            return *(shape == desktop::NamedShape::Entity ? editor.create_entity(name.toStdString(), box)
                   : shape == desktop::NamedShape::Relationship ? editor.create_relationship(name.toStdString(), box)
                                                               : editor.create_attribute(name.toStdString(), box)).created;
        };
        const auto resize = [&](const domain::ElementRef& ref, double width, double height) {
            const std::map<domain::ElementRef, domain::Rect> boxes{{ref, {0, 200, width, height}}};
            return shape == desktop::NamedShape::Entity ? editor.resize_entities(boxes)
                 : shape == desktop::NamedShape::Relationship ? editor.resize_relationships(boxes) : editor.resize_attributes(boxes);
        };
        const auto short_ref = make("ID");
        require(editor.project().layout.at(short_ref).width == body.width, "Short names keep compact defaults");
        const auto ref = make(reference_name);
        painted_as(ref, shape, reference_name);
        require(QFontMetricsF(font_for()).elidedText(reference_name, Qt::ElideRight,
                    available_room(shape, editor.project().layout.at(ref).width, body.height, font_for())) == reference_name,
                shape == desktop::NamedShape::Relationship
                    ? "Relationship fits in the relationship diamond without ellipsis"
                    : "Relationship fits without ellipsis");
        require(find_node(view, reference_name)->toolTip().isEmpty(), "A name that fits has no redundant name hover");
        auto* fitted_node = find_node(view, reference_name);
        require(editor.create_comment("Keep a remark on a fully visible name", {domain::CommentTarget{ref}}),
                "A fitted shape can still have meaningful hover content");
        view.synchronize();
        require(fitted_node->toolTip().contains(reference_name) && fitted_node->toolTip().contains("Keep a remark"),
                "Fitted names keep comment hover with the full-name heading");
        view.set_comments_visible(false);
        require(fitted_node->toolTip().isEmpty(), "Hidden remarks do not restore redundant fitted-name hover");
        view.set_comments_visible(true);
        require(editor.undo(), "Remove the fitted shape comment");
        view.synchronize();
        const auto cap = std::max(body.width, desktop::name_room_width(shape, reference_name, view.font(), 1.0, body.height));
        require(editor.project().layout.at(ref).width == cap, "Reference needs only its shape-specific compact width");
        for (const auto& sample : {QStringLiteral("User"), QStringLiteral("Student"), QStringLiteral("Department"),
                                  QStringLiteral("Enrolled In"), QStringLiteral("Works On"), QStringLiteral("Employee ID"),
                                  QStringLiteral("iiiiiiiiiiiiiiii")}) {
            const QFontMetricsF metrics(font_for());
            if (metrics.horizontalAdvance(sample) <= metrics.horizontalAdvance(reference_name))
                painted_as(make(sample), shape, sample);
        }
        const auto long_ref = make(long_name);
        require(editor.project().layout.at(long_ref).width == cap, "An over-budget name never creates an enormous shape");
        view.synchronize();
        auto* long_node = find_node(view, long_name);
        require(long_node->toolTip() == long_name, "Hover exposes the full original name");
        require(QString::fromStdString(domain::name(editor.project(), long_ref)) == long_name, "Model name remains complete");
        // Made at the cap with its default height, the deliberately long name
        // is written in as many lines as the shape has room for, the last cut
        // short (2026-10-12).
        require(QFontMetricsF(font_for()).horizontalAdvance(long_name) > available_room(shape, cap, body.height, font_for()),
                "The deliberately long name is too long for one line");
        require_written_in_lines(view, editor, long_ref, shape, true);
        require(editor.create_comment("Keep this contextual remark", {domain::CommentTarget{long_ref}}),
                "A meaningful existing tooltip");
        view.synchronize();
        require(long_node->toolTip().contains(long_name) && long_node->toolTip().contains("Keep this contextual remark"),
                "Full-name hover preserves contextual comments");
        view.set_comments_visible(false);
        require(long_node->toolTip() == long_name, "Full-name hover survives hidden comments");
        view.set_comments_visible(true);
        require(editor.undo(), "Remove the test comment");
        view.synchronize();
        require(resize(long_ref, 1900, body.height), "Manual widening beyond the automatic cap is allowed");
        painted_as(long_ref, shape, long_name);
        require(find_node(view, long_name)->toolTip().isEmpty(), "Manual widening removes name hover when the full name fits");
        require(resize(long_ref, domain::min_entity_width, body.height), "Manual narrowing is allowed");
        require(editor.project().layout.at(long_ref).width == domain::min_entity_width, "Exact manual width is retained");
        view.synchronize();
        require(find_node(view, long_name)->toolTip() == long_name, "Narrowed shapes still expose the full name");
        const auto base = editor.project().lettering.at(long_ref);
        const auto factor = domain::lettering_factor(base, domain::min_entity_width, body.height);
        const auto narrow_font = font_for(factor);
        require(QFontMetricsF(narrow_font).horizontalAdvance(long_name)
                    > available_room(shape, domain::min_entity_width, body.height, narrow_font),
                "Too long for a manually narrowed shape's one line");
        require_written_in_lines(view, editor, long_ref, shape, true);

        // After proportional manual resizing, both actual and reference names
        // must use that same font, including its change during automatic growth.
        require(resize(long_ref, body.width * 2, body.height * 2), "Manual lettering scaled with the box");
        const auto lettering_before = editor.project().lettering;
        const auto hand_box = editor.project().layout.at(long_ref);
        require(view.rename(long_ref, "Relationship"), "Renamed after being resized by hand");
        require(editor.project().layout.at(long_ref) == hand_box, "A hand's size is kept by a rename");
        {
            const auto laid = desktop::name_layout(shape, reference_name, view.font(),
                domain::lettering_factor(base, hand_box.width, hand_box.height), QRectF(0, 0, hand_box.width, hand_box.height));
            if (laid.lines.size() == 1) painted_as(long_ref, shape, reference_name);
            else require_written_in_lines(view, editor, long_ref, shape, false);
        }
        const auto measured_box = editor.project().layout.at(long_ref);
        const auto measured_factor = domain::lettering_factor(base, measured_box.width, measured_box.height);
        const auto measured_cap = desktop::name_room_width(shape, reference_name, view.font(), measured_factor, measured_box.height);
        require(measured_box.width <= std::max(body.width * 2, measured_cap) + 1,
                "Manual lettering uses its own reference cap");
        require(editor.project().lettering == lettering_before, "Automatic fitting never rewrites manual lettering metadata");
        require(view.rename(long_ref, long_name.toStdString()), "Long rename at the same effective lettering");
        require(editor.project().layout.at(long_ref).width <= std::max(body.width * 2, measured_cap) + 1,
                "An over-budget name remains capped with manual lettering");
        require(resize(long_ref, 1900, body.height * 2), "Manually enlarged beyond the cap again");
        const auto wide_box = editor.project().layout.at(long_ref);
        require(view.rename(long_ref, "Relationship"), "Rename a manually wide shape");
        require(editor.project().layout.at(long_ref) == wide_box, "A manual width above the cap is never reduced");

        const auto rename_ref = make("ID");
        const auto before = editor.project().layout.at(rename_ref);
        require(view.rename(rename_ref, "Relationship"), "Rename within the reference budget");
        painted_as(rename_ref, shape, reference_name);
        require(editor.project().layout.at(rename_ref).width == cap, "Rename grows only enough for Relationship");
        require(editor.undo(), "Rename and growth undo in one step");
        require(editor.project().layout.at(rename_ref) == before && domain::name(editor.project(), rename_ref) == "ID",
                "One undo restores name and geometry");
        require(view.rename(rename_ref, long_name.toStdString()), "Rename beyond the reference budget");
        require(editor.project().layout.at(rename_ref).width == cap, "Long rename stops at the cap");
        require(editor.project().layout.at(rename_ref).height >= body.height, "And is made as tall as its lines need");
        require_written_in_lines(view, editor, rename_ref, shape, true);
        require(view.rename(rename_ref, "ID"), "Rename shorter");
        const auto id = desktop::fitted_body(shape, "ID", view.font());
        require(editor.project().layout.at(rename_ref).width == id.width
                    && editor.project().layout.at(rename_ref).height == id.height,
                "Which makes it the size ID is fitted to again");
    }
}

// Generalization and specialization use the same smaller bold name font;
// drawing a canvas label must never alter the application's interface font.
void hierarchy_names_use_nine_bold_tests() {
    const auto application_font = QApplication::font();
    SequentialIds ids;
    application::Editor editor(ids);
    desktop::DiagramView view(editor);
    const auto interface_font = view.font();
    QFont expected_font = interface_font;
    expected_font.setFamilies({"Inter", "Source Sans 3", "IBM Plex Sans", "Segoe UI Variable",
                               "Segoe UI", "SF Pro Text", "Helvetica Neue", "Arial"});
    expected_font.setStyleHint(QFont::SansSerif, QFont::PreferAntialias);
    expected_font.setPointSizeF(9.0 * desktop::lettering_scale);
    expected_font.setWeight(QFont::Bold);
    for (const auto direction : {domain::Inheritance::Generalization, domain::Inheritance::Specialization}) {
        require(editor.create_specialization("IS A", {0, 0, 96, 74}, direction), "A hierarchy to label");
        view.synchronize();
        auto* node = find_node(view, "IS A");
        QImage actual(QSize(416, 328), QImage::Format_ARGB32);
        actual.setDevicePixelRatio(4);
        actual.fill(Qt::transparent);
        QPainter painter(&actual);
        painter.setFont(interface_font);
        node->paint(&painter, nullptr, nullptr);
        painter.end();
        QImage expected(actual.size(), QImage::Format_ARGB32);
        expected.setDevicePixelRatio(4);
        expected.fill(Qt::transparent);
        const QRectF room(6, direction == domain::Inheritance::Generalization ? 37 : 4, 84, 33);
        QPainter writer(&expected);
        writer.setRenderHint(QPainter::Antialiasing);
        writer.setFont(expected_font);
        const auto ink = desktop::theme(view.theme_id()).node_text;
        writer.setPen(ink);
        writer.drawText(room, Qt::AlignCenter, "IS A");
        writer.end();
        // Restrict to the name band, excluding the separate d/o constraint.
        int count = 0;
        for (int y = static_cast<int>(room.top() * 4); y < room.bottom() * 4; ++y)
            for (int x = static_cast<int>(room.left() * 4); x < room.right() * 4; ++x) {
                require((actual.pixelColor(x, y) == ink) == (expected.pixelColor(x, y) == ink),
                        "Actual hierarchy name is painted at 9 before scaling, bold");
                if (expected.pixelColor(x, y) == ink) ++count;
            }
        require(count > 0, "The complete hierarchy name has visible ink");
        const auto shape = direction == domain::Inheritance::Generalization
            ? desktop::NamedShape::Generalization : desktop::NamedShape::Specialization;
        const auto ref = *editor.create_specialization("ID", {200, 200, 96, 74}, direction).created;
        const auto before = editor.project().layout.at(ref);
        require(desktop::width_for_name(shape, "ID", interface_font) == 96, "Short hierarchy names keep their default width");
        const auto cap = desktop::width_for_name(shape, "Relationship", interface_font);
        require(view.rename(ref, "Relationship"), "Fit the hierarchy reference name");
        view.synchronize();
        require(editor.project().layout.at(ref).width == cap, "Hierarchy grows only to its Relationship budget");
        require(find_node(view, "Relationship")->toolTip().isEmpty(), "A fitting hierarchy name needs no name hover");
        require(QFontMetricsF(expected_font).elidedText("Relationship", Qt::ElideRight, cap - 12) == "Relationship",
                "Relationship fits without ellipsis");
        require(editor.undo(), "Undo hierarchy name and width together");
        require(editor.project().layout.at(ref) == before && domain::name(editor.project(), ref) == "ID",
                "One undo restores the hierarchy name and geometry");
        const std::string long_name = "Department Of International Computer Science";
        require(view.rename(ref, long_name), "Fit a long hierarchy name");
        view.synchronize();
        require(editor.project().layout.at(ref).width == cap, "A long hierarchy name stops at Relationship");
        require(find_node(view, QString::fromStdString(long_name))->toolTip().contains(QString::fromStdString(long_name)),
                "A shortened hierarchy name reveals its complete name on hover");
        require(domain::name(editor.project(), ref) == long_name, "Hierarchy model name remains complete");
        require(QFontMetricsF(expected_font).elidedText(QString::fromStdString(long_name), Qt::ElideRight, cap - 12)
                    != QString::fromStdString(long_name), "An over-budget hierarchy name is elided");
        require(editor.move({{ref, {200, 200, 70, 74}}}), "Manually narrow a hierarchy below its automatic cap");
        view.synchronize();
        require(editor.project().layout.at(ref).width == 70, "Exact manual hierarchy width is retained");
        require(find_node(view, QString::fromStdString(long_name))->toolTip().contains(QString::fromStdString(long_name)),
                "A narrowed hierarchy exposes the full stored name");
        require(editor.move({{ref, {200, 200, 1200, 74}}}), "Manually widen a hierarchy beyond its automatic cap");
        view.synchronize();
        require(QFontMetricsF(expected_font).elidedText(QString::fromStdString(long_name), Qt::ElideRight, 1200 - 12)
                    == QString::fromStdString(long_name), "A manually wide hierarchy displays its full long name");
        require(find_node(view, QString::fromStdString(long_name))->toolTip().isEmpty(),
                "A manually wide hierarchy needs no name-only hover");
        const auto grown = editor.project().layout.at(ref);
        require(view.rename(ref, "ID"), "Shorten a hierarchy name");
        view.synchronize();
        require(editor.project().layout.at(ref) == grown, "Shortening a hierarchy name never shrinks its shape");
        require(find_node(view, "ID")->toolTip().isEmpty(), "A fitting short hierarchy name removes redundant hover");

    }
    require(QApplication::font() == application_font && view.font() == interface_font,
            "The real application and interface font sizes remain unchanged");
}

// A diagram is drawn at the sizes it was saved with: one whose names are too
// long for their shapes is not fitted to them when it is opened.
void saved_sizes_are_kept_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    const auto entity = *editor.create_entity("A Far Longer Entity Name Than Fits", {0, 0, 90, 50}).created;
    const auto relationship = *editor.create_relationship("Is Responsible For Teaching", {200, 0, 120, 60}).created;
    const auto attribute = *editor.create_attribute("Date Of Registration In Full", {0, 200, 80, 44}).created;
    require(editor.resize_attributes({{attribute, {0, 200, 70, 44}}}), "One of them resized by hand");
    const auto layout = editor.project().layout;
    const auto lettering = editor.project().lettering;
    const auto revision = editor.revision();
    desktop::DiagramView view(editor);
    view.resize(800, 600);
    view.show();
    view.fit_diagram();
    view.synchronize();
    QApplication::processEvents();
    require(editor.revision() == revision, "Drawing the diagram edits nothing");
    require(editor.project().layout == layout && editor.project().lettering == lettering,
            "Every shape is the size it was saved at");
    require(editor.project().layout.at(entity).width == 90 && editor.project().layout.at(relationship).width == 120,
            "However short that is for its name");

    // Shapes saved narrower than "Attributes" needs -- as every short name
    // made them before that was the narrowest (2026-10-13) -- are opened at
    // exactly the sizes they were saved at, and drawn so. Only a rename fits
    // one to its name again, and undoing that gives the saved size back.
    {
        application::Editor saved(ids);
        const auto id = *saved.create_entity("ID", {0, 0, 70, 44}).created;
        const auto no = *saved.create_attribute("No", {0, 200, 70, 40}, domain::ElementRef{std::get<domain::EntityId>(id)}).created;
        const auto has = *saved.create_relationship("Has", {300, 0, 80, 60}).created;
        const auto fitted_owns = desktop::fitted_body(desktop::NamedShape::Entity, "Owns", view.font());
        const auto owns = *saved.create_entity("Owns", {600, 0, fitted_owns.width, fitted_owns.height}).created;
        require(saved.resize_entities({{owns, {600, 0, 75, 50}}}), "One of them resized by hand");
        QTemporaryDir folder;
        infrastructure::ErdxProjectStore store;
        const auto path = folder.filePath(QStringLiteral("small.erdx")).toStdString();
        require(store.save(path, saved.project()).ok, "A diagram of small shapes saves");
        saved.mark_saved(saved.revision());
        auto loaded = store.load(path);
        require(bool(loaded) && loaded.project.has_value(), "And loads");
        application::Editor opened(ids);
        require(opened.replace_project(std::move(*loaded.project)), "And opens");
        const auto opened_layout = opened.project().layout;
        desktop::DiagramView small(opened);
        small.resize(800, 600);
        small.show();
        small.fit_diagram();
        small.synchronize();
        QApplication::processEvents();
        require(opened.project().layout == opened_layout && opened_layout == saved.project().layout,
                "Opened, every shape is exactly the size it was saved at, however short its name");
        require(std::abs(find_node(small, "ID")->sceneBoundingRect().adjusted(4, 4, -4, -4).width() - 70) < 1.0,
                "And is drawn at that size");
        const auto least = desktop::fitted_body(desktop::NamedShape::Entity, "Attributes", small.font());
        require(small.rename(id, "Id"), "Renamed");
        require(opened.project().layout.at(id).width == least.width, "Renamed, it is fitted to its name again");
        require(opened.undo() && opened.project().layout.at(id) == opened_layout.at(id),
                "And undoing the rename gives the saved size back");
        require(small.rename(owns, "Own"), "The one resized by hand renamed");
        require(opened.project().layout.at(owns) == (domain::Rect{600, 0, 75, 50}), "Which keeps the size the hand gave it");
        require(opened.project().layout.at(no) == opened_layout.at(no) && opened.project().layout.at(has) == opened_layout.at(has),
                "And nothing else is resized");
        opened.mark_saved(opened.revision());
    }
}

// Every named shape made for its name (Zain, 2026-10-12): a short name the
// narrowest shape -- as wide as "Attributes" needs (Zain, 2026-10-13) -- in
// proportion to its default; "Relationship" the widest a
// name makes it, written whole on one line; a longer name as wide as that and
// taller, in two or three lines, cut short only after three. Placed with its
// tool, renamed, undone, joined by its lines and saved, it stays so.
void fitted_shapes_tests() {
    SequentialIds ids;
    application::Editor editor(ids);
    desktop::DiagramView view(editor);
    view.resize(1400, 900);
    view.show();
    view.actual_size();
    view.centerOn(0, 0);
    QApplication::processEvents();
    const auto font = view.font();
    const auto laid = [&](desktop::NamedShape shape, const QString& name, const desktop::BodySize& size) {
        return desktop::name_layout(shape, name, font, 1.0, QRectF(0, 0, size.width, size.height));
    };
    const std::array<std::pair<desktop::NamedShape, desktop::BodySize>, 5> kinds{{
        {desktop::NamedShape::Entity, desktop::entity_body},
        {desktop::NamedShape::Attribute, desktop::attribute_body},
        {desktop::NamedShape::Relationship, desktop::relationship_body},
        {desktop::NamedShape::Generalization, desktop::isa_body},
        {desktop::NamedShape::Specialization, desktop::isa_body},
    }};
    const QString two_lines = QStringLiteral("Department Name Registry");
    const QString too_long = QStringLiteral("Supercalifragilistic Expialidocious Enterprise Resource Planning "
                                            "Identifier For Every Department Of The Whole University");
    for (const auto& [shape, body] : kinds) {
        const auto least_height = std::max(domain::min_entity_height,
                                           std::ceil(body.height * domain::min_entity_width / body.width));
        // The narrowest a name makes a shape is as wide as "Attributes" needs
        // on one line (Zain, 2026-10-13), measured from its lettering the
        // same way, and with the same room around the name, as the widest is
        // for "Relationship". A name that needs more than that still grows.
        const auto height_for = [&](double width) {
            return std::clamp(std::round(body.height * width / body.width), least_height, body.height);
        };
        const auto least = desktop::fitted_body(shape, "Attributes", font);
        const auto in_least = laid(shape, "Attributes", least);
        require(in_least.lines.size() == 1 && !in_least.elided && in_least.lines.front() == "Attributes",
                "Attributes is written whole on one line in the narrowest shape a name makes");
        const auto department = desktop::fitted_body(shape, "Department", font);
        require(department.width > least.width && laid(shape, "Department", department).lines.size() == 1,
                "A name needing more room than Attributes grows past it, written whole on one line");
        for (const auto& short_name : {QStringLiteral("ID"), QStringLiteral("Id"), QStringLiteral("User")}) {
            const auto size = desktop::fitted_body(shape, short_name, font);
            require(size.width == least.width && size.height == least.height,
                    "A short name makes it no narrower than Attributes needs");
        }
        const auto small = desktop::fitted_body(shape, "Id", font);
        require(small.width > domain::min_entity_width, "Wider than the least the domain allows a hand to make it");
        require(small.height == height_for(small.width),
                "In proportion to its default, and never squatter than that allows");
        require(small.height <= body.height, "Never taller than the default");
        const auto reference = desktop::fitted_body(shape, "Relationship", font);
        const auto written = laid(shape, "Relationship", reference);
        require(written.lines.size() == 1 && !written.elided && written.lines.front() == "Relationship",
                "Relationship is written whole on one line, never cut short");
        require(least.width <= reference.width, "The narrowest a name makes it is never wider than the widest");
        const QFontMetricsF metrics(font);
        for (const auto& name : {QStringLiteral("Owns"), QStringLiteral("Student"), QStringLiteral("Professor")}) {
            const auto size = desktop::fitted_body(shape, name, font);
            require(size.width <= reference.width, "No name shorter than Relationship makes it wider");
            const auto one = laid(shape, name, size);
            require(one.lines.size() == 1 && !one.elided, "And it is written whole on one line");
        }
        const auto two = desktop::fitted_body(shape, two_lines, font);
        const auto in_two = laid(shape, two_lines, two);
        require(two.width == reference.width && two.height >= reference.height,
                "A longer name makes it as wide as Relationship, and taller");
        require(in_two.lines.size() >= 2 && in_two.lines.size() <= 3 && !in_two.elided,
                "Written whole in two or three lines");
        const auto most = desktop::fitted_body(shape, too_long, font);
        const auto in_three = laid(shape, too_long, most);
        require(most.width == reference.width, "However long the name, no wider than Relationship");
        require(in_three.lines.size() == 3 && in_three.elided, "Too long for three lines, it is cut short on the third");
    }

    // Placed with each tool, each is the size its placeholder is fitted to.
    const std::array<std::tuple<desktop::Tool, desktop::NamedShape, QString>, 5> placing{{
        {desktop::Tool::Entity, desktop::NamedShape::Entity, QStringLiteral("Entity")},
        {desktop::Tool::Attribute, desktop::NamedShape::Attribute, QStringLiteral("Attribute")},
        {desktop::Tool::Relationship, desktop::NamedShape::Relationship, QStringLiteral("Relationship")},
        {desktop::Tool::Generalization, desktop::NamedShape::Generalization, QStringLiteral("IS A")},
        {desktop::Tool::Specialization, desktop::NamedShape::Specialization, QStringLiteral("IS A")},
    }};
    double across = -500;
    std::optional<domain::ElementRef> placed_entity;
    std::optional<domain::ElementRef> placed_relationship;
    for (const auto& [tool, shape, placeholder] : placing) {
        const auto before = editor.project().layout;
        view.set_tool(tool);
        click(view, {across, 0});
        across += 250;
        require(editor.project().layout.size() == before.size() + 1, "Placed with its tool");
        for (const auto& [ref, box] : editor.project().layout)
            if (!before.contains(ref)) {
                const auto fitted = desktop::fitted_body(shape, placeholder, font);
                require(box.width == fitted.width && box.height == fitted.height, "At the size its placeholder is fitted to");
                if (shape == desktop::NamedShape::Entity) placed_entity = ref;
                if (shape == desktop::NamedShape::Relationship) placed_relationship = ref;
            }
    }
    require(placed_entity && placed_relationship, "An entity and a diamond to work with");

    // Renamed longer and shorter, about its centre, each one step undone and
    // redone with its size.
    const auto start = editor.project().layout.at(*placed_entity);
    const auto centre = [](const domain::Rect& box) { return QPointF(box.x + box.width / 2, box.y + box.height / 2); };
    require(view.rename(*placed_entity, two_lines.toStdString()), "Renamed longer");
    const auto longer = editor.project().layout.at(*placed_entity);
    const auto fitted_longer = desktop::fitted_body(desktop::NamedShape::Entity, two_lines, font);
    require(longer.width == fitted_longer.width && longer.height == fitted_longer.height
                && QLineF(centre(longer), centre(start)).length() < 0.01,
            "Made the size its longer name is fitted to, about the same centre");
    require_written_in_lines(view, editor, *placed_entity, desktop::NamedShape::Entity, false);
    require(editor.undo() && editor.project().layout.at(*placed_entity) == start
                && domain::name(editor.project(), *placed_entity) == "Entity",
            "One undo takes the name and the size back");
    require(editor.redo() && editor.project().layout.at(*placed_entity) == longer, "And redo brings both again");

    // Joined to the diamond, then renamed short: the line meets the smaller
    // entity's outline, not where the old one was.
    require(editor.connect(std::get<domain::RelationshipId>(*placed_relationship), std::get<domain::EntityId>(*placed_entity)),
            "Join the entity to the diamond");
    require(view.rename(*placed_entity, "Id"), "Renamed short");
    view.synchronize();
    QApplication::processEvents();
    const auto shrunk = editor.project().layout.at(*placed_entity);
    require(shrunk.width == desktop::fitted_body(desktop::NamedShape::Entity, "Attributes", font).width,
            "Made the narrowest shape a name makes, as wide as Attributes needs");
    {
        const auto box = find_node(view, "Id")->sceneBoundingRect().adjusted(4, 4, -4, -4);
        require(std::abs(box.width() - shrunk.width) < 1.0, "Drawn at its new size");
        const auto target = find_node(view, "Relationship")->sceneBoundingRect().center();
        auto direction = target - box.center();
        if (view.line_style() == desktop::LineStyle::Elbow)
            direction = std::abs(direction.x()) / box.width() >= std::abs(direction.y()) / box.height()
                ? QPointF(direction.x(), 0) : QPointF(0, direction.y());
        const auto scale = std::min(direction.x() != 0 ? (box.width() / 2) / std::abs(direction.x()) : 1e9,
                                    direction.y() != 0 ? (box.height() / 2) / std::abs(direction.y()) : 1e9);
        const auto join = box.center() + direction * scale;
        bool attached = false;
        for (auto* item : view.scene()->items())
            if (item->zValue() < 0 && item->shape().contains(item->mapFromScene(join))) attached = true;
        require(attached, "Its line meets the smaller entity's outline");
    }

    // A fitted size is an ordinary size: nothing else is kept for it, and a
    // diagram opened on the same project again draws every shape at exactly
    // that size without changing anything (the file keeps sizes as it always
    // has: persistence_tests).
    require(editor.project().lettering.empty(), "Fitting keeps nothing of its own in the project");
    const auto layout = editor.project().layout;
    const auto revision = editor.revision();
    {
        desktop::DiagramView again(editor);
        again.resize(800, 600);
        again.show();
        again.synchronize();
        QApplication::processEvents();
    }
    require(editor.revision() == revision && editor.project().layout == layout,
            "Opened again, every fitted shape is the size it was");
}

// A band drawn round shapes, or shapes being carried, held near or past the
// edge of the view scroll it on, so the same gesture goes on into what was
// not showing (C6, 2026-10-12). Nothing else the hand does with the button
// held scrolls the view; the history hears of a carry once, when it is let go.
void hold_for(int ms) {
    QElapsedTimer clock;
    clock.start();
    while (clock.elapsed() < ms) QApplication::processEvents(QEventLoop::AllEvents, 5);
}
int sign_of(int value) { return (value > 0) - (value < 0); }

void edge_scroll_tests() {
    using desktop::EdgeScroll;
    // The pace: nothing away from the edges, one pixel at the inside of the
    // strip along one, more nearer it and more again past it, and never more
    // than the fastest.
    {
        const QSize size(600, 400);
        require(EdgeScroll::step(size, {300, 200}).isNull(), "Away from the edges nothing scrolls");
        require(EdgeScroll::step(size, {300, 400 - EdgeScroll::reach}) == QPoint(0, 1),
                "At the inside of the bottom strip the view goes down a pixel a step");
        require(EdgeScroll::step(size, {300, EdgeScroll::reach - 1}) == QPoint(0, -1),
                "At the inside of the top strip it goes up a pixel a step");
        require(EdgeScroll::step(size, {300, 399}).y() > EdgeScroll::step(size, {300, 390}).y(),
                "Nearer the edge it goes faster");
        require(EdgeScroll::step(size, {300, 460}).y() > EdgeScroll::step(size, {300, 399}).y(),
                "Past the edge it goes faster again");
        require(EdgeScroll::step(size, {300, 5000}).y() == EdgeScroll::fastest
                    && EdgeScroll::step(size, {-5000, 200}).x() == -EdgeScroll::fastest,
                "However far past the edge, a step goes no further than the fastest");
        const auto corner = EdgeScroll::step(size, {599, 0});
        require(corner.x() > 0 && corner.y() < 0, "In a corner it goes both ways at once");
        require(EdgeScroll::step(QSize(40, 40), {20, 20}).isNull(), "A tiny view still has a middle that does not scroll");
        // A step on time goes as far as it says; one that comes late goes as
        // far as the steps it stands for, but never further than the fastest.
        require(EdgeScroll::late_step({0, 7}, EdgeScroll::interval_ms) == QPoint(0, 7), "A step on time is itself");
        require(EdgeScroll::late_step({3, 0}, 0) == QPoint(3, 0), "A step that comes early is itself");
        require(EdgeScroll::late_step({0, -1}, 10 * EdgeScroll::interval_ms) == QPoint(0, -10),
                "A step ten steps late goes ten steps' way");
        require(EdgeScroll::late_step({7, 7}, 600) == QPoint(EdgeScroll::fastest, EdgeScroll::fastest),
                "A step very late goes no further than the fastest");
    }

    SequentialIds ids;
    application::Editor editor(ids);
    std::vector<domain::EntityId> cells;
    for (int row = 0; row < 6; ++row)
        for (int column = 0; column < 6; ++column) {
            const auto made = editor.create_entity("Cell " + std::to_string(row) + std::to_string(column),
                                                   {column * 300.0, row * 300.0, 133, 77});
            require(made, "Create a cell of the grid");
            cells.push_back(std::get<domain::EntityId>(*made.created));
        }
    desktop::DiagramView view(editor);
    view.resize(600, 400);
    view.show();
    view.actual_size();
    QApplication::processEvents();
    auto* port = view.viewport();
    const auto bars = [&] { return QPoint(view.horizontalScrollBar()->value(), view.verticalScrollBar()->value()); };
    const auto box_of = [&](const domain::EntityId& id) {
        const auto& rect = editor.project().layout.at(id);
        return QRectF(rect.x, rect.y, rect.width, rect.height);
    };
    const auto empty_spot = [&] {
        for (int y = port->height() / 3; y < port->height() * 2 / 3; y += 7)
            for (int x = port->width() / 3; x < port->width() * 2 / 3; x += 7)
                if (!view.itemAt(QPoint(x, y)) && !view.itemAt(QPoint(x + 12, y + 12))) return QPoint(x, y);
        throw std::runtime_error("No empty spot in the middle of the view");
    };
    const auto seen = [&] { return view.mapToScene(port->rect()).boundingRect(); };
    const auto look_at = [&](QPointF centre) {
        view.select_elements({});
        view.centerOn(centre);
        QApplication::processEvents();
    };
    const auto near = [](QPointF a, QPointF b, double by) { return std::abs(a.x() - b.x()) <= by && std::abs(a.y() - b.y()) <= by; };

    // A band held at each edge, and at a corner, scrolls the view that way;
    // it goes on while the pointer is held still; it still begins where it
    // was pressed on the diagram and reaches the pointer; let go, it stops,
    // and what the band covered is selected -- shapes that were not in view
    // when it was pressed among them.
    // Each band begins in a gap of the grid, with the view so placed that a
    // row or a column lies just beyond the edge the band is taken to.
    struct Way {
        const char* name;
        QPointF centre;
        QPointF start;
        QPoint at;
        QPoint sign;
    };
    const auto w = port->width();
    const auto h = port->height();
    for (const auto& way : {Way{"bottom", {450, 80}, {220, 150}, {w / 2, h - 3}, {0, 1}},
                            Way{"top", {450, 650}, {220, 750}, {w / 2, 2}, {0, -1}},
                            Way{"right", {580, 40}, {520, 150}, {w - 3, h / 2}, {1, 0}},
                            Way{"left", {1050, 40}, {1120, 150}, {2, h / 2}, {-1, 0}},
                            Way{"bottom right", {450, 80}, {220, 150}, {w - 3, h - 3}, {1, 1}}}) {
        const std::string name = way.name;
        look_at(way.centre);
        const auto was_seen = seen();
        const auto start = view.mapFromScene(way.start);
        require(port->rect().contains(start) && !view.itemAt(start),
                ("The band to the " + name + " begins on empty canvas in view").c_str());
        const auto anchor = view.mapToScene(start);
        const auto from = bars();
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, start + QPoint(12, 12), Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, way.at, Qt::NoButton, Qt::LeftButton);
        hold_for(160);
        const auto scrolled = bars() - from;
        require(sign_of(scrolled.x()) == way.sign.x() && sign_of(scrolled.y()) == way.sign.y(),
                ("A band held at the " + name + " scrolls the view that way").c_str());
        const auto band = view.rubberBandRect();
        const QRectF drawn(view.mapToScene(band.topLeft()), view.mapToScene(band.bottomRight()));
        const auto covered = QRectF(anchor, view.mapToScene(way.at)).normalized();
        require(near(drawn.topLeft(), covered.topLeft(), 2) && near(drawn.bottomRight(), covered.bottomRight(), 2),
                ("The band held at the " + name + " begins where it was pressed and reaches the pointer").c_str());
        const auto still = bars();
        hold_for(80);
        require(bars() != still, ("Held still at the " + name + ", the view goes on scrolling").c_str());
        const auto reached = view.mapToScene(way.at);
        mouse(view, QEvent::MouseButtonRelease, way.at, Qt::LeftButton, Qt::NoButton);
        const auto stopped = bars();
        hold_for(60);
        require(bars() == stopped, ("Let go at the " + name + ", the view stops").c_str());
        const auto caught = QRectF(anchor, reached).normalized();
        const auto selected = view.selected_elements();
        bool every_one_inside = true, one_from_outside = false;
        for (const auto& id : cells) {
            const auto box = box_of(id);
            const bool chosen = std::find(selected.begin(), selected.end(), domain::ElementRef{id}) != selected.end();
            if (caught.contains(box) && !chosen) every_one_inside = false;
            if (chosen && !caught.adjusted(-2, -2, 2, 2).intersects(box)) every_one_inside = false;
            if (chosen && !was_seen.intersects(box)) one_from_outside = true;
        }
        require(every_one_inside, ("The band held at the " + name + " selects exactly what it covered").c_str());
        require(one_from_outside, ("The band held at the " + name + " reaches shapes that were out of view").c_str());
    }

    // Brought back in, cancelled, or with the keyboard taken away, the band
    // stops scrolling the view.
    const auto band_to_bottom = [&] {
        look_at({450, 150});
        const auto start = empty_spot();
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, start + QPoint(12, 12), Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, QPoint(w / 2, h - 3), Qt::NoButton, Qt::LeftButton);
        hold_for(60);
        return start;
    };
    {
        const auto start = band_to_bottom();
        mouse(view, QEvent::MouseMove, start + QPoint(30, 30), Qt::NoButton, Qt::LeftButton);
        const auto in_again = bars();
        hold_for(80);
        require(bars() == in_again, "A band brought back from the edge stops scrolling the view");
        mouse(view, QEvent::MouseButtonRelease, start + QPoint(30, 30), Qt::LeftButton, Qt::NoButton);
    }
    {
        band_to_bottom();
        view.cancel_interaction();
        const auto cancelled = bars();
        hold_for(80);
        require(bars() == cancelled, "A cancelled gesture stops scrolling the view");
        mouse(view, QEvent::MouseButtonRelease, QPoint(w / 2, h - 3), Qt::LeftButton, Qt::NoButton);
    }
    {
        band_to_bottom();
        QFocusEvent away(QEvent::FocusOut, Qt::ActiveWindowFocusReason);
        QApplication::sendEvent(&view, &away);
        const auto unfocused = bars();
        hold_for(80);
        require(bars() == unfocused, "A view that loses the keyboard stops scrolling");
        mouse(view, QEvent::MouseButtonRelease, QPoint(w / 2, h - 3), Qt::LeftButton, Qt::NoButton);
    }

    // Zoomed in, the band still begins where it was pressed on the diagram.
    {
        view.zoom_in();
        view.zoom_in();
        look_at({450, 150});
        const auto start = empty_spot();
        const auto anchor = view.mapToScene(start);
        const auto from = bars();
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, start + QPoint(12, 12), Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, QPoint(w / 2, h - 3), Qt::NoButton, Qt::LeftButton);
        hold_for(120);
        require(bars().y() > from.y(), "Zoomed in, a band held at the bottom scrolls the view down");
        const auto band = view.rubberBandRect();
        const QRectF drawn(view.mapToScene(band.topLeft()), view.mapToScene(band.bottomRight()));
        require(near(drawn.topLeft(), anchor, 2), "Zoomed in, the band still begins where it was pressed");
        mouse(view, QEvent::MouseButtonRelease, QPoint(w / 2, h - 3), Qt::LeftButton, Qt::NoButton);
        view.actual_size();
        QApplication::processEvents();
    }

    // One shape, three, and everything, carried to an edge: the view scrolls,
    // what is carried follows the pointer and keeps its arrangement, and the
    // move is one step of history that undoes and redoes whole.
    const auto cell = [&](int row, int column) { return cells[static_cast<std::size_t>(row * 6 + column)]; };
    struct Carry {
        const char* name;
        QPointF centre;
        std::vector<domain::EntityId> picked;
        QPoint sign;
    };
    // Everything, taken hold of by a shape in view.
    std::vector<domain::EntityId> everything{cell(0, 1)};
    for (const auto& id : cells)
        if (id != cell(0, 1)) everything.push_back(id);
    for (const auto& carry : {Carry{"one shape down", {450, 150}, {cell(0, 1)}, {0, 1}},
                              Carry{"three shapes up", {450, 650}, {cell(2, 1), cell(2, 2), cell(3, 1)}, {0, -1}},
                              Carry{"everything to the right", {450, 150}, everything, {1, 0}},
                              Carry{"one shape to the left", {1050, 150}, {cell(0, 3)}, {-1, 0}}}) {
        const std::string name = carry.name;
        look_at(carry.centre);
        std::vector<domain::ElementRef> refs(carry.picked.begin(), carry.picked.end());
        view.select_elements(refs, false);
        QApplication::processEvents();
        std::map<domain::EntityId, QRectF> before;
        for (const auto& id : cells) before[id] = box_of(id);
        const auto grip = view.mapFromScene(before[carry.picked.front()].center());
        require(port->rect().contains(grip), ("The shape carried " + name + " starts in view").c_str());
        const QPoint edge(carry.sign.x() > 0 ? w - 3 : carry.sign.x() < 0 ? 2 : grip.x(),
                          carry.sign.y() > 0 ? h - 3 : carry.sign.y() < 0 ? 2 : grip.y());
        const auto steps = editor.history_position();
        const auto from = bars();
        mouse(view, QEvent::MouseButtonPress, grip, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, grip + carry.sign * 10, Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, edge, Qt::NoButton, Qt::LeftButton);
        hold_for(160);
        const auto scrolled = bars() - from;
        require(sign_of(scrolled.x()) == carry.sign.x() && sign_of(scrolled.y()) == carry.sign.y(),
                ("Carrying " + name + " to the edge scrolls the view that way").c_str());
        require(editor.history_position() == steps, ("Carrying " + name + " writes nothing until it is let go").c_str());
        mouse(view, QEvent::MouseButtonRelease, edge, Qt::LeftButton, Qt::NoButton);
        require(editor.history_position() == steps + 1, ("Carrying " + name + " is one step of history").c_str());
        const QPointF travelled = QPointF(edge - grip + scrolled);
        const auto moved = box_of(carry.picked.front()).topLeft() - before[carry.picked.front()].topLeft();
        require(near(moved, travelled, 7.5),
                ("What is carried " + name + " stays under the pointer, the view's scrolling included").c_str());
        bool arranged = true, rest_still = true;
        for (const auto& id : cells) {
            const bool carried = std::find(carry.picked.begin(), carry.picked.end(), id) != carry.picked.end();
            const auto now = box_of(id);
            if (carried && !near(now.topLeft() - before[id].topLeft(), moved, 0.01)) arranged = false;
            if (!carried && now != before[id]) rest_still = false;
        }
        require(arranged, ("Everything carried " + name + " moves together and keeps its arrangement").c_str());
        require(rest_still, ("Nothing that was not carried " + name + " moves").c_str());
        std::map<domain::EntityId, QRectF> after;
        for (const auto& id : cells) after[id] = box_of(id);
        require(editor.undo(), ("Undo the carry " + name).c_str());
        view.synchronize();
        bool back = true;
        for (const auto& id : cells) back = back && box_of(id) == before[id];
        require(back, ("Undone, everything carried " + name + " is where it was").c_str());
        require(editor.redo(), ("Redo the carry " + name).c_str());
        view.synchronize();
        bool again = true;
        for (const auto& id : cells) again = again && box_of(id) == after[id];
        require(again, ("Redone, everything carried " + name + " is where it was let go").c_str());
        require(editor.undo(), ("Put the carry " + name + " back").c_str());
        view.synchronize();
        QApplication::processEvents();
    }

    // The diagram's room grows while shapes are carried, by as much as they
    // need and no more, so the view scrolls on with them past where the room
    // used to end -- upward and leftward as well as down and right. Nothing
    // else moves, the view never leaps, and letting go leaves the view where
    // it is with what was carried under the pointer.
    // Zoomed out, so the room's old edge is reached sooner, and so the room is
    // shown to grow at a zoom other than actual size.
    view.zoom_out();
    view.zoom_out();
    view.zoom_out();
    const auto zoom = view.zoom_factor();
    struct Further {
        const char* name;
        std::vector<domain::EntityId> picked;
        QPoint sign;
    };
    for (const auto& further : {Further{"one shape down", {cell(0, 1)}, {0, 1}},
                                Further{"one shape up", {cell(0, 1)}, {0, -1}},
                                Further{"one shape to the left", {cell(0, 1)}, {-1, 0}},
                                Further{"three shapes to the right", {cell(0, 1), cell(0, 2), cell(1, 1)}, {1, 0}}}) {
        const std::string name = further.name;
        look_at({450, 150});
        std::vector<domain::ElementRef> refs(further.picked.begin(), further.picked.end());
        view.select_elements(refs, false);
        QApplication::processEvents();
        std::map<domain::EntityId, QPointF> placed;
        for (const auto& id : cells) placed[id] = find_node(view, QString::fromStdString(editor.project().entities.at(id).name))->pos();
        const auto room = view.sceneRect();
        const auto steps = editor.history_position();
        const auto grip = view.mapFromScene(box_of(further.picked.front()).center());
        std::map<domain::EntityId, QRectF> before;
        for (const auto& id : further.picked) before[id] = box_of(id);
        const auto offset = box_of(further.picked.front()).center() - view.mapToScene(grip);
        const QPoint past(further.sign.x() > 0 ? w + 200 : further.sign.x() < 0 ? -200 : grip.x(),
                          further.sign.y() > 0 ? h + 200 : further.sign.y() < 0 ? -200 : grip.y());
        mouse(view, QEvent::MouseButtonPress, grip, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, grip + further.sign * 10, Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, past, Qt::NoButton, Qt::LeftButton);
        const auto beyond = [&] {
            const auto now = seen();
            return further.sign.y() > 0 ? now.bottom() > room.bottom() + 300
                 : further.sign.y() < 0 ? now.top() < room.top() - 300
                 : further.sign.x() > 0 ? now.right() > room.right() + 300
                                        : now.left() < room.left() - 300;
        };
        auto last_bars = bars();
        auto last_room = view.sceneRect();
        int largest_step = 0;
        bool only_grew = true, kept_in_bounds = true;
        for (int waited = 0; waited < 5000 && !beyond(); waited += 20) {
            hold_for(20);
            const auto moved_by = bars() - last_bars;
            largest_step = std::max({largest_step, std::abs(moved_by.x()), std::abs(moved_by.y())});
            last_bars = bars();
            const auto now = view.sceneRect();
            // Half a pixel either way: an edge left where it was is worked out
            // again from the other edge and the size, and may differ in the
            // last bit.
            if (!now.adjusted(-0.5, -0.5, 0.5, 0.5).contains(last_room)) only_grew = false;
            last_room = now;
            QRectF carried;
            for (const auto& id : further.picked) {
                auto* node = find_node(view, QString::fromStdString(editor.project().entities.at(id).name));
                carried = carried.isNull() ? node->sceneBoundingRect() : carried.united(node->sceneBoundingRect());
            }
            // At least 800 px past what is carried, grown 400 px at a time.
            if (!room.united(carried.adjusted(-1230, -1230, 1230, 1230)).adjusted(-0.5, -0.5, 0.5, 0.5).contains(now))
                kept_in_bounds = false;
        }
        require(beyond(), ("Carrying " + name + ", the view scrolls on past where the room used to end").c_str());
        require(largest_step <= 3 * EdgeScroll::fastest, ("Carrying " + name + ", the view never leaps").c_str());
        require(only_grew, ("Carrying " + name + ", the room only ever grows").c_str());
        require(kept_in_bounds, ("Carrying " + name + ", the room grows no further than a step past what is carried").c_str());
        bool others_still = true;
        for (const auto& id : cells)
            if (std::find(further.picked.begin(), further.picked.end(), id) == further.picked.end()
                && find_node(view, QString::fromStdString(editor.project().entities.at(id).name))->pos() != placed[id])
                others_still = false;
        require(others_still, ("Carrying " + name + ", nothing else moves").c_str());
        require(editor.history_position() == steps, ("Carrying " + name + " writes nothing until it is let go").c_str());
        const auto before_release = bars();
        const auto pointer_on_diagram = view.mapToScene(past);
        mouse(view, QEvent::MouseButtonRelease, past, Qt::LeftButton, Qt::NoButton);
        require(bars() == before_release, ("Let go after carrying " + name + ", the view stays where it is").c_str());
        require(editor.history_position() == steps + 1, ("Carrying " + name + " is one step of history").c_str());
        // What was carried is where the pointer let go of it, held as it was
        // taken hold of, give or take a guide's snap; a group keeps its
        // arrangement.
        require(near(box_of(further.picked.front()).center(), pointer_on_diagram + offset, 7.5 / zoom),
                ("What was carried " + name + " is let go under the pointer").c_str());
        const auto moved = box_of(further.picked.front()).topLeft() - before.at(further.picked.front()).topLeft();
        bool arranged = true;
        for (const auto& id : further.picked)
            if (!near(box_of(id).topLeft() - before.at(id).topLeft(), moved, 0.01)) arranged = false;
        require(arranged, ("Everything carried " + name + " keeps its arrangement").c_str());
        require(editor.undo(), ("Put the carry " + name + " back").c_str());
        view.synchronize();
        QApplication::processEvents();
    }
    view.actual_size();
    QApplication::processEvents();

    // A shape carried past where the room ended keeps its lines joined to it
    // as it goes and once it is let go, and the diagram saved and opened again
    // has it exactly where it was let go.
    {
        SequentialIds joined_ids;
        application::Editor joined(joined_ids);
        const auto made_carried = joined.create_entity("Carried", {0, 0, 133, 77});
        const auto made_stays = joined.create_entity("Stays", {400, 0, 133, 77});
        const auto made_joins = joined.create_relationship("Joins", {180, 160, 171, 99});
        require(made_carried && made_stays && made_joins, "Create the joined diagram");
        const auto carried = std::get<domain::EntityId>(*made_carried.created);
        const auto stays = std::get<domain::EntityId>(*made_stays.created);
        const auto joins = std::get<domain::RelationshipId>(*made_joins.created);
        require(joined.connect(joins, carried) && joined.connect(joins, stays), "Join the diagram");
        desktop::DiagramView small(joined);
        small.resize(600, 400);
        small.show();
        small.actual_size();
        small.zoom_out();
        small.zoom_out();
        small.zoom_out();
        small.centerOn(266, 100);
        QApplication::processEvents();
        auto* small_port = small.viewport();
        const auto room = small.sceneRect();
        const auto lines_meet = [&](const QRectF& body) {
            int meeting = 0;
            for (auto* item : small.scene()->items())
                if (item->zValue() < 0 && item->toolTip().startsWith(QStringLiteral("Participant:"))
                    && item->sceneBoundingRect().intersects(body.adjusted(-4, -4, 4, 4)))
                    ++meeting;
            return meeting;
        };
        small.select_elements({domain::ElementRef{carried}}, false);
        QApplication::processEvents();
        const auto& start = joined.project().layout.at(carried);
        const auto grip = small.mapFromScene(QPointF(start.x + start.width / 2, start.y + start.height / 2));
        const QPoint past(grip.x(), small_port->height() + 200);
        mouse(small, QEvent::MouseButtonPress, grip, Qt::LeftButton, Qt::LeftButton);
        mouse(small, QEvent::MouseMove, grip + QPoint(0, 10), Qt::NoButton, Qt::LeftButton);
        mouse(small, QEvent::MouseMove, past, Qt::NoButton, Qt::LeftButton);
        const auto small_seen = [&] { return small.mapToScene(small_port->rect()).boundingRect(); };
        for (int waited = 0; waited < 5000 && small_seen().bottom() <= room.bottom() + 300; waited += 20) hold_for(20);
        require(small_seen().bottom() > room.bottom() + 300, "The joined shape is carried past where the room ended");
        require(lines_meet(find_node(small, "Carried")->sceneBoundingRect()) == 1,
                "While it is carried there, its line still meets it");
        mouse(small, QEvent::MouseButtonRelease, past, Qt::LeftButton, Qt::NoButton);
        const auto& landed = joined.project().layout.at(carried);
        require(landed.y > room.bottom(), "It is let go past where the room ended");
        require(lines_meet(QRectF(landed.x, landed.y, landed.width, landed.height)) == 1,
                "Let go there, its line still meets it");
        QTemporaryDir folder;
        infrastructure::ErdxProjectStore store;
        const auto path = folder.filePath(QStringLiteral("carried.erdx")).toStdString();
        require(store.save(path, joined.project()).ok, "The diagram carried past its room saves");
        const auto reloaded = store.load(path);
        require(reloaded && reloaded.project->layout == joined.project().layout,
                "And opens again with every shape exactly where it was");
        joined.mark_saved(joined.revision());
    }

    // Pulling a shape by its handle, panning, and drawing a connection do not
    // scroll the view on, however near the edge they are taken.
    {
        look_at({450, 150});
        const auto picked = cell(0, 1);
        view.select_elements({domain::ElementRef{picked}}, false);
        QApplication::processEvents();
        const auto box = box_of(picked);
        const auto handle = view.mapFromScene(box.bottomRight());
        const auto from = bars();
        mouse(view, QEvent::MouseButtonPress, handle, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, handle + QPoint(6, 6), Qt::NoButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, QPoint(handle.x() + 6, h - 3), Qt::NoButton, Qt::LeftButton);
        hold_for(80);
        require(bars() == from, "Pulling a shape's handle to the edge does not scroll the view");
        mouse(view, QEvent::MouseButtonRelease, QPoint(handle.x() + 6, h - 3), Qt::LeftButton, Qt::NoButton);
        require(box_of(picked).size() != box.size(), "The handle pulled the shape larger rather than carrying it");
        require(editor.undo(), "Put the pulled shape back");
        view.synchronize();
        QApplication::processEvents();

        view.set_tool(desktop::Tool::Pan);
        const auto start = empty_spot();
        mouse(view, QEvent::MouseButtonPress, start, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, QPoint(start.x(), h - 3), Qt::NoButton, Qt::LeftButton);
        const auto panned = bars();
        hold_for(80);
        require(bars() == panned, "Panning to the edge moves the view only as far as the hand does");
        mouse(view, QEvent::MouseButtonRelease, QPoint(start.x(), h - 3), Qt::LeftButton, Qt::NoButton);

        view.set_tool(desktop::Tool::Connect);
        look_at({450, 150});
        const auto source = view.mapFromScene(box_of(cell(0, 1)).center());
        const auto still = bars();
        mouse(view, QEvent::MouseButtonPress, source, Qt::LeftButton, Qt::LeftButton);
        mouse(view, QEvent::MouseMove, QPoint(source.x(), h - 3), Qt::NoButton, Qt::LeftButton);
        hold_for(80);
        require(bars() == still, "Drawing a connection to the edge does not scroll the view");
        mouse(view, QEvent::MouseButtonRelease, QPoint(source.x(), h - 3), Qt::LeftButton, Qt::NoButton);
        view.cancel_interaction();
        view.set_tool(desktop::Tool::Select);
    }
    editor.mark_saved(editor.revision());
}

int main(int argc, char** argv) {
    QApplication app(argc, argv);
    try {
        if (argc > 1 && std::string_view(argv[1]) == "--name-fitting") {
            relationship_fits_without_ellipsis_tests();
            hierarchy_names_use_nine_bold_tests();
            content_aware_size_tests();
            saved_sizes_are_kept_tests();
            fitted_shapes_tests();
            std::cout << "Name fitting tests passed\n";
            return 0;
        }
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
        // The label beside an end is how Chen reads a cardinality; crow's foot
        // draws a foot and writes nothing. This case is about where that label
        // falls, so it asks for the notation that has one.
        view.set_notation(desktop::Notation::Chen);
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

            // A mouse wheel has neither pixels nor a phase, and moves the
            // diagram just as two fingers do. It used to zoom instead, which
            // meant the same turn of the same wheel did different things on
            // different machines, depending only on whether the driver chose
            // to report a phase. Zooming has a key of its own now.
            const auto settled = view.verticalScrollBar()->value();
            QWheelEvent notch(at, view.viewport()->mapToGlobal(at.toPoint()), QPoint(), QPoint(0, 120),
                              Qt::NoButton, Qt::NoModifier, Qt::NoScrollPhase, false);
            QApplication::sendEvent(view.viewport(), &notch);
            QApplication::processEvents();
            require(view.zoom_factor() == before, "A wheel notch alone does not zoom");
            require(view.verticalScrollBar()->value() != settled, "It moves the diagram, as two fingers do");
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
        free_text_canvas_tests();
        symbol_resize_tests();
        recursive_loop_tests();
        alignment_tests();
        alignment_guide_tests();
        lock_selection_tests();
        background_tests();
        element_shape_preview_tests();
        weak_and_identifying_drawing_tests();
        total_participation_drawing_tests();
        total_participation_drawing_tests(desktop::MinMaxDisplayMode::CardinalityOnly);
        cardinality_labels_sit_at_the_entity_end_tests();
        routed_entity_end_label_tests();
        routed_entity_end_label_tests(desktop::MinMaxDisplayMode::CardinalityOnly);
        element_colour_tests();
        extend_selection_tests();
        picker_sample_tests();
        participant_menu_tests();
        inline_rename_tests();
        notation_tests();
        minmax_display_mode_tests();
        attribute_trunk_and_line_style_tests();
        selection_highlight_tests();
        tool_locking_tests();
        attribute_owner_tests();
        composite_owner_padlock_tests();
        attributes_named_on_the_canvas_list_in_naming_order();
        inheritance_connection_tests();
        inheritance_deletion_tests();
        inheritance_orientation_tests();
        picture_export_tests();
        document_export_tests();
        search_tests();
        wheel_zoom_tests();
        entity_resize_tests();
        relationship_resize_tests();
        relationship_fits_without_ellipsis_tests();
        hierarchy_names_use_nine_bold_tests();
        content_aware_size_tests();
        saved_sizes_are_kept_tests();
        fitted_shapes_tests();
        attribute_resize_tests();
        picture_resize_tests();
        edge_scroll_tests();
        std::cout << "Canvas tests passed\n";
    } catch (const std::exception& exception) {
        std::cerr << "Canvas test failed: " << exception.what() << '\n';
        return 1;
    }
    return 0;
}
