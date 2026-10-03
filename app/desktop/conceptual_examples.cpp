// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/conceptual_examples.hpp"

#include "app/desktop/diagram_view.hpp"
#include "application/editor.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace erdflow::desktop {
namespace {

using namespace domain;

// Wide enough to hold the name whole at the diagram's own lettering, and never
// narrower than an element is made. The figures allow for the widest face the
// lettering falls back on, so no name is cut short on any platform.
double entity_width(std::string_view name) {
    return std::max(entity_body.width, std::round(46 + 11.6 * static_cast<double>(name.size())));
}
double attribute_width(std::string_view name) {
    return std::max(attribute_body.width, std::round(44 + 10.6 * static_cast<double>(name.size())));
}
// A diamond's name is set inside its middle, a little over half its width.
double relationship_width(std::string_view name) {
    return std::max(relationship_body.width, std::round((26 + 10.6 * static_cast<double>(name.size())) / 0.56));
}

Rect centred(double cx, double cy, double width, double height) {
    return {cx - width / 2.0, cy - height / 2.0, width, height};
}

// The four sides of a shape: where a group of attributes stands, and where a
// pinned line meets an entity.
enum class Face { Top, Bottom, Left, Right };
// Which point of a diamond a pinned line leaves from.
enum class Corner { Auto, Top, Bottom, Left, Right };

struct Attr {
    const char* name;
    AttributeKind kind = AttributeKind::Normal;
    std::vector<const char*> parts{};
};

// One side of a relationship. A side left to itself is joined where the
// canvas joins any line, at the middle of the face that looks at the diamond.
// An entity carrying many lines has them pinned along its faces instead, as a
// hand drawing them would, so no two of them meet it at the same point and
// their cardinality marks stay apart.
struct Side {
    ParticipantTarget target;
    Cardinality maximum = Cardinality::Many;
    Participation participation = Participation::Partial;
    const char* role = "";
    std::optional<std::pair<Face, double>> at{};
    Corner corner = Corner::Auto;
};

// How a group of attributes stands off its owner. A column off a side, or a
// row over or under it, far enough out that every line in it meets the owner
// on that face. A split leaves a channel down the middle for a relationship
// line that runs straight out of the same face.
struct Spread {
    double gap = 60;
    double split = 0;
    double shift = 0;
    // How many stand before the channel, above it or to its left; half,
    // rounded up, when not given.
    std::size_t before = 0;
};

class Diagram {
public:
    explicit Diagram(application::Editor& editor) : editor_(editor) {}

    EntityId entity(const char* name, double cx, double cy, double width = 0, double height = 0) {
        const auto wide = width > 0 ? width : entity_width(name);
        const auto tall = height > 0 ? height : entity_body.height;
        return std::get<EntityId>(*editor_.create_entity(name, centred(cx, cy, wide, tall)).created);
    }

    RelationshipId relationship(const char* name, double cx, double cy, bool associative = false) {
        const auto wide = relationship_width(name) + (associative ? 20 : 0);
        const auto tall = associative ? 100.0 : relationship_body.height;
        const auto id = std::get<RelationshipId>(*editor_.create_relationship(name, centred(cx, cy, wide, tall)).created);
        if (associative) editor_.set_associative(id, true, centred(cx, cy, wide, tall));
        return id;
    }

    RelationshipId link(const char* name, double cx, double cy, Side a, Side b, bool associative = false,
                        bool identifying = false) {
        const auto id = relationship(name, cx, cy, associative);
        if (identifying) editor_.set_relationship_kind(id, RelationshipKind::Identifying);
        join(id, a);
        join(id, b);
        return id;
    }

    void join(RelationshipId id, const Side& side) {
        Connector shape;
        if (side.at) shape.child_anchor = anchor(target_ref(side.target), side.at->first, side.at->second);
        if (side.corner != Corner::Auto) shape.owner_anchor = corner(side.corner);
        const auto joined = editor_.connect(id, side.target, shape);
        editor_.update_participant(id, *joined.participant, side.maximum, side.participation, side.role);
    }

    AttributeId attribute(const char* name, double cx, double cy, AttributeOwner owner,
                          AttributeKind kind = AttributeKind::Normal) {
        const auto id = std::get<AttributeId>(
            *editor_.create_attribute(name, centred(cx, cy, attribute_width(name), attribute_body.height), owner)
                 .created);
        if (kind != AttributeKind::Normal) editor_.set_attribute_kind(id, kind);
        return id;
    }

    // A group of attributes on one face of its owner, each line meeting the
    // owner at the middle of that face. A composite's parts stand further out
    // again, beside it.
    void beside(AttributeOwner owner, Face face, const std::vector<Attr>& attrs, Spread spread = {}) {
        const auto box = editor_.project().layout.at(owner);
        const double cx = box.x + box.width / 2;
        const double cy = box.y + box.height / 2;
        const double half_w = box.width / 2;
        const double half_h = box.height / 2;
        const bool column = face == Face::Left || face == Face::Right;
        const double out = (face == Face::Left || face == Face::Top) ? -1.0 : 1.0;
        const auto n = attrs.size();
        // Where each stands along the face, measured from its middle.
        std::vector<double> along(n);
        if (column) {
            constexpr double pitch = 74;
            if (spread.split > 0) {
                const auto before = spread.before > 0 ? spread.before : (n + 1) / 2;
                for (std::size_t i = 0; i < n; ++i) {
                    const bool above = i < before;
                    const auto rank = above ? before - 1 - i : i - before;
                    along[i] = (above ? -1 : 1) * (spread.split / 2 + 30 + static_cast<double>(rank) * pitch);
                }
            } else {
                for (std::size_t i = 0; i < n; ++i)
                    along[i] = (static_cast<double>(i) - static_cast<double>(n - 1) / 2) * pitch;
            }
        } else {
            constexpr double space = 28;
            std::vector<double> widths(n);
            for (std::size_t i = 0; i < n; ++i) widths[i] = attribute_width(attrs[i].name);
            const auto lay = [&](std::size_t from, std::size_t to, double start, double direction) {
                double edge = start;
                for (std::size_t i = from; i < to; ++i) {
                    along[i] = edge + direction * widths[i] / 2;
                    edge += direction * (widths[i] + space);
                }
            };
            if (spread.split > 0) {
                const auto before = spread.before > 0 ? spread.before : (n + 1) / 2;
                // The first half runs outward to the left of the channel, the rest to the right.
                double edge = -spread.split / 2;
                for (std::size_t k = 0; k < before; ++k) {
                    const auto i = before - 1 - k;
                    along[i] = edge - widths[i] / 2;
                    edge -= widths[i] + space;
                }
                lay(before, n, spread.split / 2, 1);
            } else {
                double total = 0;
                for (auto width : widths) total += width;
                total += space * static_cast<double>(n - 1);
                lay(0, n, -total / 2, 1);
            }
        }
        for (auto& each : along) each += spread.shift;
        // Far enough out that every one of them looks at this face rather than
        // at the next one round, which is what decides where its line meets
        // the owner.
        double gap = spread.gap;
        const auto facing = [&](double reach) {
            for (std::size_t i = 0; i < n; ++i) {
                const double width = attribute_width(attrs[i].name);
                if (column) {
                    const double dx = half_w + reach + width / 2;
                    if (std::abs(along[i]) / half_h >= dx / half_w * 0.92) return false;
                } else {
                    const double dy = half_h + reach + attribute_body.height / 2;
                    if (std::abs(along[i]) / half_w >= dy / half_h * 0.92) return false;
                }
            }
            return true;
        };
        while (!facing(gap)) gap += 10;
        for (std::size_t i = 0; i < n; ++i) {
            const double width = attribute_width(attrs[i].name);
            double x = 0;
            double y = 0;
            if (column) {
                x = cx + out * (half_w + gap + width / 2);
                y = cy + along[i];
            } else {
                x = cx + along[i];
                y = cy + out * (half_h + gap + attribute_body.height / 2);
            }
            const auto id = attribute(attrs[i].name, x, y, owner, attrs[i].kind);
            if (attrs[i].parts.empty()) continue;
            // The parts stand further out, in a short line of their own.
            const auto parts = attrs[i].parts.size();
            for (std::size_t p = 0; p < parts; ++p) {
                const double offset = static_cast<double>(p) - static_cast<double>(parts - 1) / 2;
                const double part_width = attribute_width(attrs[i].parts[p]);
                if (column)
                    attribute(attrs[i].parts[p], x + out * (width / 2 + 90 + part_width / 2), y + offset * 72, id);
                else
                    attribute(attrs[i].parts[p], x + offset * (part_width + 28), y + out * 120, id);
            }
        }
    }

private:
    // A pinned join is a direction from the shape's centre; this is the one
    // that meets the given face at the given distance from its middle.
    [[nodiscard]] double anchor(const ElementRef& ref, Face face, double along) const {
        const auto box = editor_.project().layout.at(ref);
        switch (face) {
        case Face::Top: return std::atan2(-box.height / 2, along);
        case Face::Bottom: return std::atan2(box.height / 2, along);
        case Face::Left: return std::atan2(along, -box.width / 2);
        case Face::Right: return std::atan2(along, box.width / 2);
        }
        return 0;
    }
    [[nodiscard]] static double corner(Corner which) {
        switch (which) {
        case Corner::Top: return -std::numbers::pi / 2;
        case Corner::Bottom: return std::numbers::pi / 2;
        case Corner::Left: return std::numbers::pi;
        case Corner::Right: return 0;
        case Corner::Auto: break;
        }
        return 0;
    }
    [[nodiscard]] static ElementRef target_ref(const ParticipantTarget& target) {
        return std::visit([](auto id) { return ElementRef{id}; }, target);
    }

    application::Editor& editor_;
};

} // namespace

void build_company_database(application::Editor& editor) {
    editor.rename_project("Company Database");
    Diagram d(editor);

    // Employee is the middle of the company: its own facts stand in two
    // columns off its sides, and its lines leave from above and below it,
    // each from its own point. Department and Skill sit level with it, the
    // people around it above, and the work below: Project with its team and
    // tasks, the money to the left of it and the supply chain under it.
    const auto employee = d.entity("Employee", 0, 0, 360, 140);
    const auto department = d.entity("Department", -1750, 0);
    const auto skill = d.entity("Skill", 1750, 0);
    const auto office = d.entity("Office", -1150, -800);
    const auto dependent = d.entity("Dependent", -500, -1100);
    const auto job = d.entity("Job", 1150, 800);
    const auto project = d.entity("Project", -500, 1150, 260, 110);
    const auto team = d.entity("Team", 500, 1150);
    const auto task = d.entity("Task", 1100, 1550);
    const auto client = d.entity("Client", -1150, 2000);
    const auto invoice = d.entity("Invoice", -1850, 2000);
    const auto payment = d.entity("Payment", -1850, 2800);
    const auto product = d.entity("Product", -500, 2550);
    const auto supplier = d.entity("Supplier", 500, 2550);

    editor.set_entity_weak(dependent, true);

    d.link("Employs", -1150, 0, {department, Cardinality::One, Participation::Partial},
           {employee, Cardinality::Many, Participation::Total});

    const auto manages = d.link("Manages", -1150, 420,
                                {employee, Cardinality::One, Participation::Partial, "", {{Face::Bottom, -160}}, Corner::Right},
                                {department, Cardinality::One, Participation::Total, "", {{Face::Bottom, 55}}, Corner::Left});
    d.beside(manages, Face::Bottom, {{"Start Date"}}, {.gap = 30});

    // A relationship of Employee with itself: the supervisor's side runs
    // across to the diamond, and the supervisee's loops back over it into
    // the top of Employee, as the canvas draws every second side of a
    // recursive relationship.
    d.link("Supervises", 700, -400,
           {employee, Cardinality::One, Participation::Partial, "Supervisor", {{Face::Top, 160}}, Corner::Left},
           {employee, Cardinality::Many, Participation::Partial, "Supervisee"});

    d.link("Works At", -1150, -300,
           {employee, Cardinality::Many, Participation::Total, "", {{Face::Top, -160}}, Corner::Right},
           {office, Cardinality::One, Participation::Partial});

    d.link("Supports", -500, -600,
           {employee, Cardinality::One, Participation::Partial, "", {{Face::Top, -80}}, Corner::Right},
           {dependent, Cardinality::Many, Participation::Total}, false, true);

    d.link("Holds", 1150, 300,
           {employee, Cardinality::Many, Participation::Total, "", {{Face::Bottom, 160}}, Corner::Left},
           {job, Cardinality::One, Participation::Partial});

    const auto qualified = d.link("Qualified In", 1150, 0, {employee, Cardinality::Many, Participation::Partial},
                                  {skill, Cardinality::Many, Participation::Partial});
    d.beside(qualified, Face::Top, {{"Proficiency Level"}, {"Years Experience"}}, {.gap = 30});

    const auto works_on = d.link("Works On", -500, 600,
                                 {employee, Cardinality::Many, Participation::Partial, "", {{Face::Bottom, -80}}, Corner::Right},
                                 {project, Cardinality::Many, Participation::Partial});
    d.beside(works_on, Face::Top, {{"Hours"}, {"Assigned Date"}, {"Role"}}, {.gap = 30});

    d.link("Staffed By", 500, 600,
           {team, Cardinality::Many, Participation::Partial},
           {employee, Cardinality::Many, Participation::Partial, "", {{Face::Bottom, 80}}, Corner::Left});

    d.link("Assigned To", 0, 1150, {team, Cardinality::Many, Participation::Partial},
           {project, Cardinality::Many, Participation::Partial});

    d.link("Controls", -1750, 1150, {department, Cardinality::One, Participation::Partial},
           {project, Cardinality::Many, Participation::Total});

    d.link("Belongs To", -1150, 1550,
           {project, Cardinality::Many, Participation::Total, "", {{Face::Bottom, -80}}, Corner::Right},
           {client, Cardinality::One, Participation::Partial});

    d.link("Uses", -500, 1950, {project, Cardinality::Many, Participation::Partial},
           {product, Cardinality::Many, Participation::Partial});

    d.link("Contains", 450, 1550,
           {project, Cardinality::One, Participation::Partial, "", {{Face::Bottom, 80}}, Corner::Left},
           {task, Cardinality::Many, Participation::Total});

    d.link("Receives", -1500, 2000, {client, Cardinality::One, Participation::Partial},
           {invoice, Cardinality::Many, Participation::Total});

    d.link("Settles", -1850, 2400, {invoice, Cardinality::One, Participation::Partial},
           {payment, Cardinality::Many, Participation::Total});

    d.link("Supplies", 0, 2550, {supplier, Cardinality::Many, Participation::Partial},
           {product, Cardinality::Many, Participation::Partial});

    // Each entity's facts on the faces its lines leave free. Employee keeps
    // one more below its middle than above, so the supervisee's loop is the
    // one to pass over it.
    d.beside(employee, Face::Left,
             {{"Employee ID", AttributeKind::Key},
              {"Birth Date"},
              {"Gender"},
              {"Age", AttributeKind::Derived},
              {"Name", AttributeKind::Composite, {"First Name", "Middle Name", "Last Name"}}},
             {.split = 90, .before = 2});
    d.beside(employee, Face::Right,
             {{"Email"}, {"Phone", AttributeKind::Multivalued}, {"Address"}, {"Hire Date"}, {"Salary"}, {"Job Title"}},
             {.split = 90});
    d.beside(department, Face::Left,
             {{"Department ID", AttributeKind::Key},
              {"Department Name"},
              {"Location", AttributeKind::Multivalued},
              {"Budget"},
              {"Phone"},
              {"Created Date"}});
    d.beside(office, Face::Top,
             {{"Office ID", AttributeKind::Key}, {"Office Name"}, {"City"}, {"Country"}, {"Address"}, {"Phone"}});
    d.beside(dependent, Face::Top,
             {{"Name", AttributeKind::Key}, {"Birth Date"}, {"Relationship"}, {"Gender"}});
    d.beside(job, Face::Right,
             {{"Job ID", AttributeKind::Key}, {"Job Title"}, {"Description"}, {"Minimum Salary"}, {"Maximum Salary"}});
    d.beside(skill, Face::Right, {{"Skill ID", AttributeKind::Key}, {"Skill Name"}, {"Description"}});
    d.beside(project, Face::Left,
             {{"Project ID", AttributeKind::Key},
              {"Project Name"},
              {"Description"},
              {"Start Date"},
              {"End Date"},
              {"Budget"},
              {"Status"}},
             {.split = 90});
    d.beside(team, Face::Bottom, {{"Team ID", AttributeKind::Key}, {"Team Name"}, {"Created Date"}});
    d.beside(task, Face::Right,
             {{"Task ID", AttributeKind::Key},
              {"Task Name"},
              {"Description"},
              {"Start Date"},
              {"Due Date"},
              {"Status"},
              {"Priority"}});
    d.beside(client, Face::Right,
             {{"Client ID", AttributeKind::Key}, {"Client Name"}, {"Email"}, {"Phone"}, {"Address"}, {"Industry"}});
    d.beside(invoice, Face::Left,
             {{"Invoice ID", AttributeKind::Key}, {"Issue Date"}, {"Due Date"}, {"Total Amount"}, {"Status"}});
    d.beside(payment, Face::Left,
             {{"Payment ID", AttributeKind::Key}, {"Payment Date"}, {"Amount"}, {"Payment Method"}, {"Status"}});
    d.beside(product, Face::Left,
             {{"Product ID", AttributeKind::Key},
              {"Product Name"},
              {"Description"},
              {"Unit Price"},
              {"Stock Quantity"}});
    d.beside(supplier, Face::Right,
             {{"Supplier ID", AttributeKind::Key}, {"Supplier Name"}, {"Email"}, {"Phone"}, {"Address"}});
}

void build_university_database(application::Editor& editor) {
    editor.rename_project("University Database");
    Diagram d(editor);

    // Student and Section hold the middle, joined by Enrollment, with Course
    // beside Section. Teaching sits above them -- Professor, then Department
    // and Faculty -- and assessment below them: Exam under Section, its
    // results between it and Student. The library, clubs and scholarships
    // keep to the outer left.
    const auto student = d.entity("Student", -1000, 0, 300, 140);
    const auto section = d.entity("Section", 1000, 0, 260, 110);
    const auto course = d.entity("Course", 2500, 0);
    const auto professor = d.entity("Professor", 0, -1000, 260, 100);
    const auto department = d.entity("Department", 0, -1800, 260, 100);
    const auto faculty = d.entity("Faculty", 0, -2550);
    const auto program = d.entity("Program", -2300, -500);
    const auto classroom = d.entity("Classroom", 1700, -900);
    const auto exam = d.entity("Exam", 1000, 1150);
    const auto exam_result = d.entity("Exam Result", -100, 1150);
    const auto assignment = d.entity("Assignment", 1900, 1150);
    const auto library_book = d.entity("Library Book", -3150, 0);
    const auto club = d.entity("Club", -2300, 1000);
    const auto scholarship = d.entity("Scholarship", -2300, 500);

    const auto enrollment = d.link("Enrollment", 0, 0, {student, Cardinality::Many, Participation::Partial},
                                   {section, Cardinality::Many, Participation::Partial}, true);
    d.beside(enrollment, Face::Bottom, {{"Enrollment Date"}, {"Grade"}, {"Status"}}, {.gap = 30});

    d.link("Advises", -1000, -1000, {professor, Cardinality::One, Participation::Partial},
           {student, Cardinality::Many, Participation::Partial});

    d.link("Teaches", 1000, -1000, {professor, Cardinality::One, Participation::Partial},
           {section, Cardinality::Many, Participation::Partial});

    d.link("Employs", -80, -1260,
           {department, Cardinality::One, Participation::Partial, "", {{Face::Bottom, -80}}},
           {professor, Cardinality::Many, Participation::Total, "", {{Face::Top, -80}}});

    d.link("Heads", 80, -1550,
           {professor, Cardinality::One, Participation::Partial, "", {{Face::Top, 80}}},
           {department, Cardinality::One, Participation::Total, "", {{Face::Bottom, 80}}});

    d.link("Contains", 0, -2175, {faculty, Cardinality::One, Participation::Partial},
           {department, Cardinality::Many, Participation::Total});

    d.link("Offers", 2500, -1800, {department, Cardinality::One, Participation::Partial},
           {course, Cardinality::Many, Participation::Total});

    d.link("Manages", -2300, -1800, {department, Cardinality::One, Participation::Partial},
           {program, Cardinality::Many, Participation::Total});

    d.link("Enrolled In", -1800, -500,
           {student, Cardinality::Many, Participation::Total, "", {{Face::Top, -100}}, Corner::Right},
           {program, Cardinality::One, Participation::Partial});

    d.link("Offered As", 1800, 0, {course, Cardinality::One, Participation::Total},
           {section, Cardinality::Many, Participation::Total});

    d.link("Held In", 1700, -450,
           {section, Cardinality::Many, Participation::Total, "", {{Face::Top, 90}}, Corner::Left},
           {classroom, Cardinality::One, Participation::Partial});

    // Course's other side loops back under it, into the middle of its foot,
    // as the canvas draws every second side of a recursive relationship.
    d.link("Prerequisite", 3100, 0, {course, Cardinality::Many, Participation::Partial, "Successor"},
           {course, Cardinality::Many, Participation::Partial, "Predecessor"});

    d.link("Assigns", 1900, 350,
           {section, Cardinality::One, Participation::Partial, "", {{Face::Bottom, 90}}, Corner::Left},
           {assignment, Cardinality::Many, Participation::Total});

    d.link("Examines", 1000, 600, {section, Cardinality::One, Participation::Partial},
           {exam, Cardinality::Many, Participation::Total});

    d.link("Produces", 450, 1150, {exam, Cardinality::One, Participation::Total},
           {exam_result, Cardinality::Many, Participation::Total});

    d.link("Receives", -1000, 1150, {student, Cardinality::One, Participation::Partial},
           {exam_result, Cardinality::Many, Participation::Total});

    const auto loan = d.link("Book Loan", -2250, 0, {student, Cardinality::Many, Participation::Partial},
                             {library_book, Cardinality::Many, Participation::Partial}, true);
    d.beside(loan, Face::Bottom, {{"Borrow Date"}, {"Due Date"}, {"Return Date"}}, {.gap = 30});

    d.link("Joins", -1650, 1000,
           {student, Cardinality::Many, Participation::Partial, "", {{Face::Bottom, -40}}, Corner::Right},
           {club, Cardinality::Many, Participation::Partial});

    d.link("Awarded", -1650, 500,
           {student, Cardinality::Many, Participation::Partial, "", {{Face::Bottom, -110}}, Corner::Right},
           {scholarship, Cardinality::Many, Participation::Partial});

    // Each entity's facts on the faces its lines leave free.
    d.beside(student, Face::Left,
             {{"Student ID", AttributeKind::Key},
              {"Birth Date"},
              {"Gender"},
              {"Age", AttributeKind::Derived},
              {"Name", AttributeKind::Composite, {"First Name", "Middle Name", "Last Name"}}},
             {.split = 90, .before = 2});
    d.beside(student, Face::Right,
             {{"Email"}, {"Phone", AttributeKind::Multivalued}, {"Address"}, {"Enrollment Date"}}, {.split = 90});
    d.beside(section, Face::Right,
             {{"Section ID", AttributeKind::Key}, {"Section Number"}, {"Semester"}, {"Year"}, {"Capacity"}},
             {.split = 90});
    d.beside(course, Face::Top,
             {{"Course ID", AttributeKind::Key}, {"Course Name"}, {"Description"}, {"Credits"}, {"Level"}},
             {.split = 90});
    d.beside(professor, Face::Bottom,
             {{"Professor ID", AttributeKind::Key},
              {"First Name"},
              {"Last Name"},
              {"Email"},
              {"Phone"},
              {"Salary"},
              {"Hire Date"},
              {"Office Number"}});
    d.beside(department, Face::Left,
             {{"Department ID", AttributeKind::Key}, {"Department Name"}, {"Office"}, {"Phone"}, {"Budget"}},
             {.split = 90});
    d.beside(faculty, Face::Top,
             {{"Faculty ID", AttributeKind::Key}, {"Faculty Name"}, {"Office"}, {"Phone"}});
    d.beside(program, Face::Left,
             {{"Program ID", AttributeKind::Key}, {"Program Name"}, {"Degree Level"}, {"Duration Years"}});
    d.beside(classroom, Face::Right,
             {{"Classroom ID", AttributeKind::Key}, {"Building"}, {"Room Number"}, {"Capacity"}});
    d.beside(exam, Face::Bottom,
             {{"Exam ID", AttributeKind::Key}, {"Exam Type"}, {"Exam Date"}, {"Maximum Score"}});
    d.beside(exam_result, Face::Bottom,
             {{"Result ID", AttributeKind::Key}, {"Score"}, {"Grade"}, {"Passed"}});
    d.beside(assignment, Face::Right,
             {{"Assignment ID", AttributeKind::Key}, {"Title"}, {"Description"}, {"Due Date"}, {"Maximum Points"}});
    d.beside(library_book, Face::Left,
             {{"Book ID", AttributeKind::Key},
              {"ISBN"},
              {"Title"},
              {"Author", AttributeKind::Multivalued},
              {"Publisher"},
              {"Publication Year"}});
    d.beside(club, Face::Left,
             {{"Club ID", AttributeKind::Key}, {"Club Name"}, {"Description"}, {"Created Date"}});
    d.beside(scholarship, Face::Bottom,
             {{"Scholarship ID", AttributeKind::Key}, {"Scholarship Name"}, {"Amount"}, {"Start Date"}, {"End Date"}});
}

} // namespace erdflow::desktop
