// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/home_demo_canvas.hpp"

#include "app/desktop/diagram_view.hpp"
#include "app/desktop/picture_export.hpp"
#include "application/editor.hpp"

#include <QPainter>
#include <QUuid>

#include <map>

namespace erdflow::desktop {
namespace {
// Identities for an example that lives only long enough to be drawn.
class ExampleIds final : public application::IdGenerator {
public:
    domain::Uuid next() override {
        const auto made = QUuid::createUuidV7().toRfc4122();
        domain::Uuid id;
        for (std::size_t i = 0; i < id.bytes.size(); ++i)
            id.bytes[i] = static_cast<std::uint8_t>(made[static_cast<qsizetype>(i)]);
        return id;
    }
};
} // namespace

void build_conceptual_example(application::Editor& editor) {
    using namespace domain;
    // At the sizes the canvas makes each element at (diagram_view.hpp):
    // entity 148 x 86, relationship 190 x 110, attribute 150 x 60. Laid out
    // on one line, Student, Enrolled, Course, as the card has read since
    // 2026-09-24, with each side's attributes over it and the relationship's
    // own under it.
    const auto student = std::get<EntityId>(*editor.create_entity("Student", {0, 100, 148, 86}).created);
    const auto course = std::get<EntityId>(*editor.create_entity("Course", {452, 100, 148, 86}).created);
    const auto enrolled = std::get<RelationshipId>(
        *editor.relate(student, course, {205, 88, 190, 110}, "Enrolled").created);
    // Many to many: a student takes many courses and a course has many
    // students.
    const auto sides = editor.project().relationships.at(enrolled).participants;
    for (const auto& side : sides)
        editor.update_participant(enrolled, side.id, Cardinality::Many, Participation::Partial, "");
    const auto attribute = [&](const char* name, Rect at, ElementRef owner, bool key) {
        const auto id = std::get<AttributeId>(*editor.create_attribute(name, at, AttributeOwner{owner}).created);
        if (key) editor.set_attribute_kind(id, AttributeKind::Key);
    };
    attribute("ID", {-40, 0, 150, 60}, ElementRef{student}, true);
    attribute("Name", {115, 0, 150, 60}, ElementRef{student}, false);
    attribute("ID", {335, 0, 150, 60}, ElementRef{course}, true);
    attribute("Name", {490, 0, 150, 60}, ElementRef{course}, false);
    // Widened to hold its name, as somebody would widen it on the canvas:
    // at the size an attribute is made at, its name is shortened.
    attribute("Enrollment Date", {195, 232, 210, 60}, ElementRef{enrolled}, false);
}

const CanvasPicture& conceptual_canvas_picture(ThemeId theme) {
    // Built once for each theme: a theme is tried by pointing at it, and a
    // canvas made again for every name the pointer passed would be felt.
    static std::map<ThemeId, CanvasPicture> made;
    const auto found = made.find(theme);
    if (found != made.end()) return found->second;
    ExampleIds ids;
    application::Editor editor(ids);
    build_conceptual_example(editor);
    DiagramView view(editor);
    view.set_theme(theme);
    view.synchronize();
    CanvasPicture drawn;
    drawn.source = picture_extent(view, PictureExtent::WholeDiagram).adjusted(-16, -16, 16, 16);
    QPainter painter(&drawn.picture);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);
    painter.fillRect(QRectF(QPointF(0, 0), drawn.source.size()), view.canvas_colour());
    view.render_diagram(painter, QRectF(QPointF(0, 0), drawn.source.size()), drawn.source);
    painter.end();
    return made.emplace(theme, std::move(drawn)).first->second;
}

} // namespace erdflow::desktop
