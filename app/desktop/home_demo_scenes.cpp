// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/home_demo_scenes.hpp"

#include "app/desktop/home_sidebar.hpp"

#include <QFontMetricsF>
#include <QPainter>
#include <QPainterPath>

#include <algorithm>
#include <cmath>
#include <numeric>
#include <optional>

namespace erdflow::desktop {
namespace {
// ---- The Conceptual card: decorative data only ----------------------------
enum ConceptualStep : std::size_t {
    Empty, Student, Course, StudentId, StudentName, CourseId, CourseName,
    Enrolled, StudentLine, CourseLine, Cardinalities, EnrolledAttribute, Hold, Reset,
};

const QRectF student_box(12, 87, 68, 26);
const QRectF course_box(200, 87, 68, 26);
const QRectF enrolled_box(104, 84, 72, 32);

QRectF oval(double x, double y, double wide) {
    return {x - wide / 2, y - 10, wide, 20};
}

void add_attribute(std::vector<DemoElement>& into, std::size_t step, const QString& name,
                   const QRectF& box, const QRectF& owner, bool key = false) {
    const bool over = box.center().y() < owner.center().y();
    const QPointF foot(owner.center().x(), over ? owner.top() : owner.bottom());
    const QPointF end(box.center().x(), over ? box.bottom() : box.top());
    // Both branches share the first three units directly at their owner.
    const QPointF trunk = foot + QPointF(0, over ? -3 : 3);
    const QPointF bend(end.x(), trunk.y());
    into.push_back({DemoShape::Connector, name, {}, foot, end, {trunk, bend},
                    false, false, step, 0.0, 0.6});
    into.push_back({DemoShape::Attribute, name, box, {}, {}, {}, false, false, step, 0.25, 1.0});
    if (key) into.push_back({DemoShape::KeyMark, name, box, {}, {}, {}, false, false, step, 0.25, 1.0});
}

const std::vector<DemoElement>& conceptual_elements() {
    static const std::vector<DemoElement> pieces = [] {
        std::vector<DemoElement> all;
        all.push_back({DemoShape::Entity, "Student", student_box, {}, {}, {}, false, false, Student});
        all.push_back({DemoShape::Entity, "Course", course_box, {}, {}, {}, false, false, Course});
        add_attribute(all, StudentId, "ID", oval(22, 55, 34), student_box, true);
        add_attribute(all, StudentName, "Name", oval(70, 55, 40), student_box);
        add_attribute(all, CourseId, "ID", oval(210, 55, 34), course_box, true);
        add_attribute(all, CourseName, "Name", oval(258, 55, 40), course_box);
        all.push_back({DemoShape::Relationship, "Enrolled", enrolled_box, {}, {}, {}, false, false, Enrolled});
        const QPointF student(student_box.right(), 100), course(course_box.left(), 100);
        all.push_back({DemoShape::Connector, "Student–Enrolled", {}, student,
                       QPointF(enrolled_box.left(), 100), {}, false, false, StudentLine});
        all.push_back({DemoShape::Connector, "Course–Enrolled", {}, course,
                       QPointF(enrolled_box.right(), 100), {}, false, false, CourseLine});
        // Default conceptual participation is partial: zero-to-many on both
        // entity ends, just as DiagramView draws Crow's Foot participants.
        all.push_back({DemoShape::OptionalMany, "Student", {}, student, QPointF(1, 0), {},
                       false, false, Cardinalities});
        all.push_back({DemoShape::OptionalMany, "Course", {}, course, QPointF(-1, 0), {},
                       false, false, Cardinalities});
        const auto date = oval(140, 145, 104);
        all.push_back({DemoShape::Connector, "Enrollment Date", {}, QPointF(140, enrolled_box.bottom()),
                       QPointF(140, date.top()), {}, false, false, EnrolledAttribute, 0.0, 0.6});
        all.push_back({DemoShape::Attribute, "Enrollment Date", date, {}, {}, {},
                       false, false, EnrolledAttribute, 0.25, 1.0});
        return all;
    }();
    return pieces;
}

const std::vector<DemoStep>& conceptual_steps() {
    static const std::vector<DemoStep> steps{
        {"Empty", 0.4}, {"Student", 0.6}, {"Course", 0.6},
        {"Student ID", 0.6}, {"Student Name", 0.6}, {"Course ID", 0.6}, {"Course Name", 0.6},
        {"Enrolled", 0.6}, {"Student line", 0.6}, {"Course line", 0.6},
        {"Cardinalities", 0.6}, {"Enrollment Date", 0.8}, {"Hold", 2.8}, {"Reset", 0.8},
    };
    return steps;
}

// ---- The Relational scene -------------------------------------------------
//
// The same idea as tables: Students and Courses, and the Enrollments bridge
// between them, each table and then each of its rows arriving in turn, a key
// marked as its row arrives -- the primary keys in orange, the foreign keys
// in green -- and then each foreign key's line drawn from the exact row of
// the key it points at to the exact row that points at it. No diamond: in a
// schema a relationship is its foreign keys.
enum RelationalStep : std::size_t {
    Nothing, StudentsTable, StudentsKeyRow, StudentsNameRow, CoursesTable, CoursesKeyRow,
    CoursesNameRow, EnrollmentsTable, EnrollmentsStudentRow, EnrollmentsCourseRow, EnrollmentsDateRow,
    StudentsLine, CoursesLine, Held, Faded,
};

// A table's parts, in the authored box: a header, then its rows, with a
// narrow gutter on the left where a key is marked.
constexpr double table_header = 18;
constexpr double table_row = 17;
constexpr double key_gutter = 18;

QRectF table_box(double left, double top, double wide, std::size_t rows) {
    return {left, top, wide, table_header + table_row * static_cast<double>(rows)};
}
QRectF row_box(const QRectF& table, std::size_t index) {
    return {table.left(), table.top() + table_header + table_row * static_cast<double>(index),
            table.width(), table_row};
}

// The approved arrangement: Students upper left, Courses lower left, the
// Enrollments bridge on the right between them. Stood in the middle of the
// box, as the conceptual model is, since a demo held inside its card has no
// floor under it to leave room for.
const QRectF students_box = table_box(10, 32, 98, 2);
const QRectF courses_box = table_box(10, 118, 98, 2);
const QRectF enrollments_box = table_box(158, 67, 114, 3);

// A foreign key's line: out of the referenced row's right side, across to a
// lane of its own, and into the referencing row's left side, meeting both
// rows exactly as the Relational Design view does.
std::vector<QPointF> reference_route(const QRectF& key_row, const QRectF& foreign_row, double lane) {
    const auto from = QPointF(key_row.right(), key_row.center().y());
    const auto to = QPointF(foreign_row.left(), foreign_row.center().y());
    return {from, QPointF(lane, from.y()), QPointF(lane, to.y()), to};
}

// A row arriving in its own step, and the mark in its key gutter, where it
// has one, following it in the same step.
void add_row(std::vector<DemoElement>& into, const QString& column, const QRectF& table, std::size_t index,
             bool bridge, std::size_t step, std::optional<DemoShape> mark = {}) {
    const auto row = row_box(table, index);
    into.push_back({DemoShape::Column, column, row, {}, {}, {}, bridge, false, step, 0.0, 0.7});
    if (mark) into.push_back({*mark, column, row, {}, {}, {}, bridge, false, step, 0.35, 1.0});
}

const std::vector<DemoElement>& relational_elements() {
    static const std::vector<DemoElement> pieces = [] {
        std::vector<DemoElement> all;
        all.push_back({DemoShape::Table, "Students", students_box, {}, {}, {}, false, false, StudentsTable});
        add_row(all, "StudentID", students_box, 0, false, StudentsKeyRow, DemoShape::PrimaryKey);
        add_row(all, "Name", students_box, 1, false, StudentsNameRow);
        all.push_back({DemoShape::Table, "Courses", courses_box, {}, {}, {}, false, false, CoursesTable});
        add_row(all, "CourseID", courses_box, 0, false, CoursesKeyRow, DemoShape::PrimaryKey);
        add_row(all, "Name", courses_box, 1, false, CoursesNameRow);
        all.push_back({DemoShape::Table, "Enrollments", enrollments_box, {}, {}, {}, true, false,
                       EnrollmentsTable});
        add_row(all, "StudentID", enrollments_box, 0, true, EnrollmentsStudentRow, DemoShape::ForeignKey);
        add_row(all, "CourseID", enrollments_box, 1, true, EnrollmentsCourseRow, DemoShape::ForeignKey);
        add_row(all, "EnrollmentDate", enrollments_box, 2, true, EnrollmentsDateRow);
        all.push_back({DemoShape::Reference, "StudentID", {}, {}, {},
                       reference_route(row_box(students_box, 0), row_box(enrollments_box, 0), 127), false, false,
                       StudentsLine, 0.0, 1.0});
        all.push_back({DemoShape::Reference, "CourseID", {}, {}, {},
                       reference_route(row_box(courses_box, 0), row_box(enrollments_box, 1), 138), false, false,
                       CoursesLine, 0.0, 1.0});
        return all;
    }();
    return pieces;
}

const std::vector<DemoStep>& relational_steps() {
    // Eleven seconds a pass, like the conceptual one beside it.
    static const std::vector<DemoStep> steps{
        {"Empty", 0.5}, {"Students", 0.6}, {"StudentID", 0.6}, {"Students Name", 0.5},
        {"Courses", 0.6}, {"CourseID", 0.6}, {"Courses Name", 0.5},
        {"Enrollments", 0.6}, {"Enrollments StudentID", 0.6}, {"Enrollments CourseID", 0.6},
        {"EnrollmentDate", 0.5}, {"Students line", 0.8}, {"Courses line", 0.8},
        {"Hold", 2.4}, {"Reset", 0.8},
    };
    return steps;
}

// ---- The SQL scene --------------------------------------------------------
//
// The same three tables written as SQL: an editor comes up, the script is
// typed into it a character at a time at one steady speed, and a line at its
// foot says what running it made. Nothing is run; it is a picture of typing.
enum SqlStep : std::size_t {
    Blank, EditorOpens, StudentsHead, StudentsColumns, CoursesHead, CoursesColumns,
    EnrollmentsHead, EnrollmentsColumns, Outcome, Kept, Gone,
};

// How fast it is typed: steady, and slow enough to read as it goes.
constexpr double characters_a_second = 40;

// The script Zain gave (2026-09-25), in the runs typed in each step: each
// table's head, then its columns. Set compactly -- two spaces in, and each
// table closed on its last column's line -- so all ten lines stand whole in
// the editor at a size that can be read inside the card. A new line is a
// character like any other, which gives the typing its small pause at the
// end of a line.
const std::vector<std::pair<SqlStep, QString>>& sql_runs() {
    static const std::vector<std::pair<SqlStep, QString>> runs{
        {StudentsHead, "CREATE TABLE Students ("},
        {StudentsColumns, "\n  StudentID INT PRIMARY KEY,\n  Name VARCHAR(100));"},
        {CoursesHead, "\nCREATE TABLE Courses ("},
        {CoursesColumns, "\n  CourseID INT PRIMARY KEY,\n  Name VARCHAR(100));"},
        {EnrollmentsHead, "\nCREATE TABLE Enrollments ("},
        {EnrollmentsColumns, "\n  StudentID INT,\n  CourseID INT,\n  EnrollmentDate DATE);"},
    };
    return runs;
}

// The editor, in the authored box: a slim header naming the file, a gutter of
// line numbers, the script, and a strip at the foot for what running it said.
// Stood in the middle of the box, as the other two demos are, since inside
// its card it has no floor to leave room for.
constexpr double editor_header = 15;
constexpr double code_line = 13;
constexpr double code_lettering = 10;
constexpr double result_height = 19;
constexpr double editor_height = editor_header + 6 + code_line * 10 + 4 + result_height;
const QRectF editor_box(26, (200 - editor_height) / 2.0, 228, editor_height);
constexpr double code_left = 26 + 24;
const double code_top = editor_box.top() + editor_header + 6;

const std::vector<DemoElement>& sql_elements() {
    static const std::vector<DemoElement> pieces = [] {
        std::vector<DemoElement> all;
        all.push_back({DemoShape::Editor, "university.sql", editor_box, {}, {}, {}, false, false,
                       EditorOpens, 0.0, 1.0});
        for (const auto& [step, run] : sql_runs())
            all.push_back({DemoShape::Code, run, {}, {}, {}, {}, false, true, step, 0.0, 1.0});
        all.push_back({DemoShape::Result, "3 tables created",
                       QRectF(editor_box.left(), editor_box.bottom() - result_height, editor_box.width(),
                              result_height),
                       {}, {}, {}, false, false, Outcome, 0.0, 1.0});
        return all;
    }();
    return pieces;
}

const std::vector<DemoStep>& sql_steps() {
    // A typing step lasts as long as its run takes at the steady speed, so
    // the speed is the same whichever part is being written. About ten
    // seconds a pass in all.
    static const std::vector<DemoStep> steps = [] {
        const auto typing = [](SqlStep step) {
            for (const auto& [at, run] : sql_runs())
                if (at == step) return static_cast<double>(run.size()) / characters_a_second;
            return 0.0;
        };
        return std::vector<DemoStep>{
            {"Empty", 0.4}, {"Editor", 0.6}, {"Students", typing(StudentsHead)},
            {"Students columns", typing(StudentsColumns)}, {"Courses", typing(CoursesHead)},
            {"Courses columns", typing(CoursesColumns)}, {"Enrollments", typing(EnrollmentsHead)},
            {"Enrollments columns", typing(EnrollmentsColumns)},
            {"Result", 0.6}, {"Hold", 1.8}, {"Reset", 0.7},
        };
    }();
    return steps;
}

// ---- Drawing --------------------------------------------------------------

// How large the lettering is, in the authored box. Small, because the demo
// is; no smaller than stays readable at the size it is shown under a card.
constexpr double entity_lettering = 11;
constexpr double small_lettering = 9;

// Calm, not springy: eased in and out, and nothing overshoots.
double ease(double t) {
    t = std::clamp(t, 0.0, 1.0);
    return t * t * (3.0 - 2.0 * t);
}

struct Ink {
    QColor entity_fill, entity_border, attribute_fill, attribute_border;
    QColor relationship_fill, relationship_border, text, line, shadow;
    // A table's body, and the key marks: a primary key in the warning
    // colour the Relational Design view marks one in, a foreign key in its
    // valid green.
    QColor body, primary_key, foreign_key;
    // The editor: its page and header, and the script's colours.
    QColor page, page_header, page_edge, muted, keyword, type, number, done;
    QStringList family;
};

// The Conceptual workspace's own colours for its shapes, in whatever theme is
// worn, shown in grey where the theme has no colour.
Ink ink_of(ThemeId id) {
    const auto& shapes = theme(id);
    const auto& t = tokens(id);
    const auto own = [id](const QColor& colour) { return colourless(id) ? greyed(colour) : colour; };
    auto shadow = own(t.shadow_card_hover.ink);
    shadow.setAlphaF(0.16f);
    // The script's colours are the theme's, deepened where they would not
    // read on the editor's page.
    const auto on_page = [&](const QColor& colour) { return own(legible_on(colour, t.surface)); };
    return {own(shapes.entity_fill), own(shapes.entity_border), own(shapes.attribute_fill),
            own(shapes.attribute_border), own(shapes.relationship_fill), own(shapes.relationship_border),
            own(shapes.node_text), own(shapes.connector), shadow,
            own(shapes.base), own(shapes.warning), own(shapes.valid),
            t.surface, own(t.window_background), own(t.border_medium), own(t.text_muted),
            on_page(t.primary), on_page(t.lavender), on_page(t.amber), on_page(t.green), t.family};
}

QFont lettering(const Ink& ink, double pixels, bool bold) {
    QFont font;
    font.setFamilies(ink.family);
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(pixels * 4))));
    font.setWeight(bold ? QFont::DemiBold : QFont::Normal);
    return font;
}

QPainterPath outline_of(const DemoElement& element) {
    QPainterPath path;
    const auto& box = element.box;
    switch (element.shape) {
    case DemoShape::Entity: path.addRect(box); break;
    case DemoShape::Attribute: path.addEllipse(box); break;
    case DemoShape::Relationship:
        path.moveTo(box.center().x(), box.top());
        path.lineTo(box.right(), box.center().y());
        path.lineTo(box.center().x(), box.bottom());
        path.lineTo(box.left(), box.center().y());
        path.closeSubpath();
        break;
    default: break;
    }
    return path;
}

// Text is laid out four times larger and drawn a quarter the size, so small
// lettering keeps its shape when the whole demo is scaled rather than
// snapping to whole pixels.
void write(QPainter& painter, const QRectF& box, const QString& words, const QFont& font,
           Qt::Alignment align = Qt::AlignCenter) {
    painter.save();
    painter.translate(box.center());
    painter.scale(0.25, 0.25);
    painter.setFont(font);
    const QRectF big(-box.width() * 2, -box.height() * 2, box.width() * 4, box.height() * 4);
    painter.drawText(big, static_cast<int>(align.toInt()), words);
    painter.restore();
}

// The columns are set in a fixed-width face, as the Relational Design view
// sets them.
QFont column_lettering(double pixels) {
    QFont font;
    font.setFamilies({"Menlo", "SF Mono", "Consolas", "DejaVu Sans Mono", "monospace"});
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(std::max(1, static_cast<int>(std::lround(pixels * 4))));
    return font;
}

// Faded in and grown the last few per cent, about the middle of a box.
void arrive(QPainter& painter, const QRectF& box, double arrived) {
    const auto grow = 0.95 + 0.05 * arrived;
    painter.setOpacity(painter.opacity() * arrived);
    painter.translate(box.center());
    painter.scale(grow, grow);
    painter.translate(-box.center());
}

void paint_table(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived) {
    const auto header_fill = element.bridge ? ink.relationship_fill : ink.entity_fill;
    const auto edge = element.bridge ? ink.relationship_border : ink.entity_border;
    const auto& box = element.box;
    painter.save();
    arrive(painter, box, arrived);
    QPainterPath outline;
    outline.addRoundedRect(box, 2.5, 2.5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink.shadow);
    painter.drawPath(outline.translated(0, 1.8));
    painter.setBrush(ink.body);
    painter.drawPath(outline);
    // The header in the colour of what the table came from, as the view
    // colours it: an entity's for a table, a relationship's for a bridge.
    QPainterPath header;
    header.addRect(QRectF(box.left(), box.top(), box.width(), table_header));
    painter.setBrush(header_fill);
    painter.drawPath(outline.intersected(header));
    painter.setPen(QPen(edge, 1.1));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(outline);
    painter.drawLine(QPointF(box.left(), box.top() + table_header),
                     QPointF(box.right(), box.top() + table_header));
    // The gutter's rule, which the key marks stand to the left of.
    auto faint = edge;
    faint.setAlphaF(0.45f);
    painter.setPen(QPen(faint, 0.6));
    painter.drawLine(QPointF(box.left() + key_gutter, box.top() + table_header),
                     QPointF(box.left() + key_gutter, box.bottom()));
    painter.setPen(ink.text);
    write(painter, QRectF(box.left() + 6, box.top(), box.width() - 12, table_header), element.label,
          lettering(ink, 10.5, true), Qt::AlignLeft | Qt::AlignVCenter);
    painter.restore();
}

void paint_column(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived,
                  bool first_row) {
    const auto& cell = element.box;
    painter.save();
    painter.setOpacity(painter.opacity() * arrived);
    // Rising into its row as it fills in.
    painter.translate(0, (1.0 - arrived) * 3.0);
    if (!first_row) {
        auto faint = element.bridge ? ink.relationship_border : ink.entity_border;
        faint.setAlphaF(0.3f);
        painter.setPen(QPen(faint, 0.5));
        painter.drawLine(QPointF(cell.left() + 1, cell.top()), QPointF(cell.right() - 1, cell.top()));
    }
    painter.setPen(ink.text);
    write(painter, QRectF(cell.left() + key_gutter + 5, cell.top(), cell.width() - key_gutter - 8, cell.height()),
          element.label, column_lettering(9.5), Qt::AlignLeft | Qt::AlignVCenter);
    painter.restore();
}

// A primary key's mark: a small key, upright with its bow at the top and its
// teeth at the foot, filled in the primary key's colour.
void paint_primary_key(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived) {
    const QPointF middle(element.box.left() + key_gutter / 2.0, element.box.center().y());
    painter.save();
    painter.setOpacity(painter.opacity() * arrived);
    painter.translate(middle);
    const auto grow = (0.6 + 0.4 * arrived) * 1.15;
    painter.scale(grow, grow);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink.primary_key);
    QPainterPath key;
    key.addEllipse(QPointF(0, -3.2), 2.8, 2.8);
    key.addRect(QRectF(-0.8, -1.0, 1.6, 6.2));
    key.addRect(QRectF(0.8, 2.4, 1.8, 1.1));
    key.addRect(QRectF(0.8, 4.2, 1.4, 1.0));
    painter.drawPath(key.simplified());
    painter.setBrush(ink.body);
    painter.drawEllipse(QPointF(0, -3.2), 1.0, 1.0);
    painter.restore();
}

// A foreign key's mark: the letters FK, in the foreign key's green.
void paint_foreign_key(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived) {
    painter.save();
    painter.setOpacity(painter.opacity() * arrived);
    painter.setPen(ink.foreign_key);
    write(painter, QRectF(element.box.left(), element.box.top(), key_gutter, element.box.height()),
          QStringLiteral("FK"), lettering(ink, 7.5, true));
    painter.restore();
}

// The editor the script is written in: a page standing off the floor, with
// a slim header naming the file and a gutter for the line numbers.
void paint_editor(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived) {
    const auto& box = element.box;
    painter.save();
    arrive(painter, box, arrived);
    QPainterPath page;
    page.addRoundedRect(box, 5, 5);
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink.shadow);
    painter.drawPath(page.translated(0, 2.2));
    painter.setBrush(ink.page);
    painter.drawPath(page);
    QPainterPath top;
    top.addRect(QRectF(box.left(), box.top(), box.width(), editor_header));
    painter.setBrush(ink.page_header);
    painter.drawPath(page.intersected(top));
    painter.setPen(QPen(ink.page_edge, 0.8));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(page);
    painter.drawLine(QPointF(box.left(), box.top() + editor_header),
                     QPointF(box.right(), box.top() + editor_header));
    painter.setPen(ink.muted);
    write(painter, QRectF(box.left() + 8, box.top(), box.width() - 16, editor_header), element.label,
          lettering(ink, 8, false), Qt::AlignLeft | Qt::AlignVCenter);
    painter.restore();
}

// What a word of SQL is, for colouring it: its keywords, its types, its
// numbers, and everything else as written.
QColor colour_of(const QString& word, const Ink& ink) {
    static const QStringList keywords{"CREATE", "TABLE", "PRIMARY", "KEY", "REFERENCES", "NOT", "NULL"};
    static const QStringList types{"INT", "VARCHAR", "DATE"};
    if (keywords.contains(word)) return ink.keyword;
    if (types.contains(word)) return ink.type;
    if (!word.isEmpty() && word.front().isDigit()) return ink.number;
    if (!word.isEmpty() && !word.front().isLetterOrNumber() && word.front() != '_') return ink.muted;
    return ink.text;
}

// The script typed so far, a line at a time, each word in its colour, with
// the line numbers beside it and the caret where the typing is.
void paint_code(QPainter& painter, const QString& typed, bool typing, const Ink& ink) {
    const auto font = column_lettering(code_lettering);
    const QFontMetricsF measured(font);
    const auto advance = measured.horizontalAdvance(QLatin1Char('M')) / 4.0;
    const auto lines = typed.split('\n');
    painter.save();
    painter.setFont(font);
    for (qsizetype n = 0; n < lines.size(); ++n) {
        const auto baseline = code_top + code_line * static_cast<double>(n) + code_line * 0.72;
        // The line's number, faint, in the gutter.
        painter.setPen(ink.muted);
        painter.save();
        painter.translate(code_left - 6, baseline);
        painter.scale(0.25, 0.25);
        const auto number = QString::number(n + 1);
        painter.drawText(QPointF(-measured.horizontalAdvance(number), 0), number);
        painter.restore();
        // The words, each in its colour.
        const auto& line = lines[n];
        qsizetype at = 0;
        while (at < line.size()) {
            auto end = at + 1;
            const auto word_like = [](QChar c) { return c.isLetterOrNumber() || c == '_'; };
            if (word_like(line[at])) while (end < line.size() && word_like(line[end])) ++end;
            else if (line[at] == ' ') while (end < line.size() && line[end] == ' ') ++end;
            const auto word = line.mid(at, end - at);
            painter.setPen(colour_of(word, ink));
            painter.save();
            painter.translate(code_left + advance * static_cast<double>(at), baseline);
            painter.scale(0.25, 0.25);
            painter.drawText(QPointF(0, 0), word);
            painter.restore();
            at = end;
        }
    }
    // The caret, at the end of what has been typed, while typing goes on.
    if (typing) {
        const auto last = static_cast<double>(lines.size() - 1);
        const auto x = code_left + advance * static_cast<double>(lines.back().size()) + 0.5;
        const auto top = code_top + code_line * last + code_line * 0.12;
        painter.setPen(QPen(ink.keyword, 0.9));
        painter.drawLine(QPointF(x, top), QPointF(x, top + code_line * 0.78));
    }
    painter.restore();
}

// What running it said, in a strip at the editor's foot: a tick in the
// valid green, and the words.
void paint_result(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived) {
    const auto& strip = element.box;
    painter.save();
    painter.setOpacity(painter.opacity() * arrived);
    painter.translate(0, (1.0 - arrived) * 3.0);
    painter.setPen(QPen(ink.page_edge, 0.6));
    painter.drawLine(QPointF(strip.left() + 1, strip.top()), QPointF(strip.right() - 1, strip.top()));
    const QPointF tick(strip.left() + 14, strip.center().y());
    painter.setPen(QPen(ink.done, 1.4, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    QPainterPath mark;
    mark.moveTo(tick + QPointF(-3.5, 0.2));
    mark.lineTo(tick + QPointF(-1.0, 2.8));
    mark.lineTo(tick + QPointF(3.8, -2.8));
    painter.drawPath(mark);
    painter.setPen(ink.text);
    write(painter, QRectF(strip.left() + 24, strip.top(), strip.width() - 30, strip.height()),
          element.label, lettering(ink, 8.5, true), Qt::AlignLeft | Qt::AlignVCenter);
    painter.restore();
}

// A foreign key's line, drawn out along its way from the key it references,
// with Crow's Foot ends once it arrives: two bars at the key, where exactly
// one row is meant, and a foot with a bar at the column that references it,
// where there may be many and there must be one.
void paint_reference(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived) {
    const auto& route = element.route;
    if (route.size() < 2 || arrived <= 0) return;
    double length = 0;
    for (std::size_t i = 1; i < route.size(); ++i)
        length += std::hypot(route[i].x() - route[i - 1].x(), route[i].y() - route[i - 1].y());
    auto left = length * arrived;
    QPainterPath drawn(route.front());
    for (std::size_t i = 1; i < route.size() && left > 0; ++i) {
        const auto piece = std::hypot(route[i].x() - route[i - 1].x(), route[i].y() - route[i - 1].y());
        const auto share = std::min(1.0, left / std::max(piece, 1e-6));
        drawn.lineTo(route[i - 1] + (route[i] - route[i - 1]) * share);
        left -= piece;
    }
    painter.save();
    painter.setPen(QPen(ink.line, 1.2, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(drawn);
    // The ends come once the line has reached them.
    const auto ends = std::clamp((arrived - 0.8) / 0.2, 0.0, 1.0);
    if (ends > 0) {
        painter.setOpacity(painter.opacity() * ends);
        painter.setPen(QPen(ink.line, 1.0));
        const auto& key_end = route.front();
        painter.drawLine(key_end + QPointF(4, -3), key_end + QPointF(4, 3));
        painter.drawLine(key_end + QPointF(7, -3), key_end + QPointF(7, 3));
        const auto& foot_end = route.back();
        painter.drawLine(foot_end + QPointF(-7, 0), foot_end + QPointF(0, -3.5));
        painter.drawLine(foot_end + QPointF(-7, 0), foot_end + QPointF(0, 3.5));
        painter.drawLine(foot_end + QPointF(-10, -3), foot_end + QPointF(-10, 3));
    }
    painter.restore();
}

void paint_shape(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived) {
    painter.save();
    arrive(painter, element.box, arrived);
    const auto outline = outline_of(element);
    // Stood a little off the page, over a soft shadow a step lower.
    painter.setPen(Qt::NoPen);
    painter.setBrush(ink.shadow);
    painter.drawPath(outline.translated(0, 1.8));
    const bool entity = element.shape == DemoShape::Entity;
    const bool attribute = element.shape == DemoShape::Attribute;
    painter.setBrush(entity ? ink.entity_fill : attribute ? ink.attribute_fill : ink.relationship_fill);
    painter.setPen(QPen(entity ? ink.entity_border : attribute ? ink.attribute_border : ink.relationship_border,
                        1.1));
    painter.drawPath(outline);
    painter.setPen(ink.text);
    write(painter, element.box, element.label,
          lettering(ink, entity ? entity_lettering : small_lettering, entity));
    painter.restore();
}

void paint_connector(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived) {
    if (arrived <= 0) return;
    const auto path = demo_connector_path(element);
    // Trace the same curve used in the finished diagram.
    QPainterPath drawn(element.from);
    const int samples = 48;
    for (int i = 1; i <= samples; ++i)
        drawn.lineTo(path.pointAtPercent(arrived * i / samples));
    painter.save();
    painter.setPen(QPen(ink.line, 1.3, Qt::SolidLine, Qt::RoundCap));
    painter.setBrush(Qt::NoBrush);
    painter.drawPath(drawn);
    painter.restore();
}

// Miniature of DiagramView::draw_participant_end's optional-many symbols:
// reach 15, spread 7, minimum at 25, ring radius 4.5, scaled by 0.4.
void paint_optional_many(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived) {
    painter.save();
    painter.setOpacity(painter.opacity() * arrived);
    const auto end = element.from;
    const auto u = element.to;
    const QPointF n(-u.y(), u.x());
    const auto apex = end + u * 6;
    painter.setPen(QPen(ink.line, 1.0));
    painter.drawLine(apex, end + n * 2.8);
    painter.drawLine(apex, end - n * 2.8);
    painter.drawLine(apex, end);
    painter.setBrush(ink.body);
    painter.drawEllipse(end + u * 10, 1.8, 1.8);
    painter.restore();
}

void paint_key(QPainter& painter, const DemoElement& element, const Ink& ink, double arrived) {
    if (arrived <= 0) return;
    // A key is its name underlined, as the canvas marks one, drawn along it.
    const auto font = lettering(ink, small_lettering, false);
    const QFontMetricsF measured(font);
    const auto wide = measured.horizontalAdvance(element.label) / 4.0;
    const auto baseline = element.box.center().y() + (measured.ascent() - measured.descent()) / 8.0 + 1.2;
    const auto left = element.box.center().x() - wide / 2.0;
    painter.save();
    painter.setPen(QPen(ink.text, 0.8));
    painter.drawLine(QPointF(left, baseline), QPointF(left + wide * arrived, baseline));
    painter.restore();
}
} // namespace

QPainterPath demo_connector_path(const DemoElement& element) {
    QPainterPath path(element.from);
    if (element.route.size() == 2) {
        path.lineTo(element.route[0]);
        path.cubicTo(element.route[0], element.route[1], element.to);
    } else path.lineTo(element.to);
    return path;
}

const std::vector<DemoStep>& demo_steps(HomeDemoKind kind) {
    static const std::vector<DemoStep> none;
    switch (kind) {
    case HomeDemoKind::Conceptual: return conceptual_steps();
    case HomeDemoKind::Relational: return relational_steps();
    case HomeDemoKind::Sql: return sql_steps();
    }
    return none;
}

const std::vector<DemoElement>& demo_elements(HomeDemoKind kind) {
    static const std::vector<DemoElement> none;
    switch (kind) {
    case HomeDemoKind::Conceptual: return conceptual_elements();
    case HomeDemoKind::Relational: return relational_elements();
    case HomeDemoKind::Sql: return sql_elements();
    }
    return none;
}

double demo_loop_seconds(HomeDemoKind kind) {
    const auto& steps = demo_steps(kind);
    return std::accumulate(steps.begin(), steps.end(), 0.0,
                           [](double sum, const DemoStep& step) { return sum + step.seconds; });
}

std::size_t demo_finished_step(HomeDemoKind kind) {
    const auto& steps = demo_steps(kind);
    for (std::size_t i = 0; i < steps.size(); ++i)
        if (QString::fromLatin1(steps[i].name) == QStringLiteral("Hold")) return i;
    return steps.empty() ? 0 : steps.size() - 1;
}

DemoMoment demo_moment_at(HomeDemoKind kind, double seconds) {
    const auto& steps = demo_steps(kind);
    const auto loop = demo_loop_seconds(kind);
    if (steps.empty() || loop <= 0) return {};
    auto into = std::fmod(seconds, loop);
    if (into < 0) into += loop;
    for (std::size_t i = 0; i < steps.size(); ++i) {
        if (into < steps[i].seconds) return {i, into / steps[i].seconds};
        into -= steps[i].seconds;
    }
    return {steps.size() - 1, 1.0};
}

double demo_arrival(const DemoElement& element, DemoMoment moment) {
    if (moment.step > element.step) return 1.0;
    if (moment.step < element.step) return 0.0;
    const auto span = std::max(1e-6, element.ends - element.starts);
    const auto through = std::clamp((moment.progress - element.starts) / span, 0.0, 1.0);
    return element.steady ? through : ease(through);
}

QString demo_code_at(HomeDemoKind kind, DemoMoment moment) {
    QString typed;
    for (const auto& element : demo_elements(kind)) {
        if (element.shape != DemoShape::Code) continue;
        const auto shown = demo_arrival(element, moment);
        typed += element.label.left(static_cast<qsizetype>(
            std::floor(shown * static_cast<double>(element.label.size()) + 1e-9)));
    }
    return typed;
}

bool demo_step_is_still(HomeDemoKind kind, std::size_t step) {
    const auto& steps = demo_steps(kind);
    if (step + 1 >= steps.size()) return false;   // the fade changes throughout
    for (const auto& element : demo_elements(kind))
        if (element.step == step) return false;
    return true;
}

double demo_scene_opacity(HomeDemoKind kind, DemoMoment moment) {
    const auto& steps = demo_steps(kind);
    // The last step of every pass is its fade.
    if (steps.empty() || moment.step + 1 != steps.size()) return 1.0;
    return 1.0 - ease(moment.progress);
}

QRectF demo_scene_bounds(HomeDemoKind kind) {
    if (kind == HomeDemoKind::Sql)
        return {editor_box.left(), editor_box.top() + editor_header, editor_box.width(),
                editor_box.height() - editor_header};
    QRectF covered;
    const auto take = [&](const QRectF& box) { covered = covered.isNull() ? box : covered.united(box); };
    for (const auto& element : demo_elements(kind)) {
        if (!element.box.isNull()) take(element.box);
        if (element.shape == DemoShape::Connector) take(QRectF(element.from, element.to).normalized());
        for (const auto& point : element.route) take(QRectF(point, QSizeF(0.1, 0.1)));
    }
    return covered.adjusted(-6, -6, 6, 6);
}

void paint_demo_scene(QPainter& painter, HomeDemoKind kind, ThemeId theme_id, DemoMoment moment,
                      bool framed) {
    const auto shown = demo_scene_opacity(kind, moment);
    if (shown <= 0) return;
    const auto ink = ink_of(theme_id);
    painter.save();
    painter.setOpacity(painter.opacity() * shown);
    // Lines first, so every shape stands over the ends of the lines it meets.
    for (const auto& element : demo_elements(kind))
        if (element.shape == DemoShape::Connector)
            paint_connector(painter, element, ink, demo_arrival(element, moment));
    const QRectF* last_table = nullptr;
    for (const auto& element : demo_elements(kind)) {
        if (element.shape == DemoShape::Table) last_table = &element.box;
        const auto arrived = demo_arrival(element, moment);
        if (arrived <= 0) continue;
        switch (element.shape) {
        case DemoShape::Entity:
        case DemoShape::Attribute:
        case DemoShape::Relationship: paint_shape(painter, element, ink, arrived); break;
        case DemoShape::OptionalMany: paint_optional_many(painter, element, ink, arrived); break;
        case DemoShape::KeyMark: paint_key(painter, element, ink, arrived); break;
        case DemoShape::Table: paint_table(painter, element, ink, arrived); break;
        case DemoShape::Column:
            paint_column(painter, element, ink, arrived,
                         last_table && std::abs(element.box.top() - last_table->top() - table_header) < 0.5);
            break;
        case DemoShape::PrimaryKey: paint_primary_key(painter, element, ink, arrived); break;
        case DemoShape::ForeignKey: paint_foreign_key(painter, element, ink, arrived); break;
        case DemoShape::Editor:
            if (!framed) paint_editor(painter, element, ink, arrived);
            break;
        case DemoShape::Result: paint_result(painter, element, ink, arrived); break;
        case DemoShape::Connector:
        case DemoShape::Reference:
        case DemoShape::Code: break;
        }
    }
    // The script, typed so far, into the editor it is written in.
    const auto typed = demo_code_at(kind, moment);
    if (!typed.isEmpty()) {
        bool typing = false;
        for (const auto& element : demo_elements(kind))
            if (element.shape == DemoShape::Code) {
                const auto part = demo_arrival(element, moment);
                typing = typing || (part > 0.0 && part < 1.0);
            }
        paint_code(painter, typed, typing, ink);
    }
    // A foreign key's line over the tables it joins, since it ends on them.
    for (const auto& element : demo_elements(kind))
        if (element.shape == DemoShape::Reference)
            paint_reference(painter, element, ink, demo_arrival(element, moment));
    painter.restore();
}

} // namespace erdflow::desktop
