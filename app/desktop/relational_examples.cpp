// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/relational_examples.hpp"

#include "application/editor.hpp"

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <variant>

namespace erdflow::desktop {
namespace {

using namespace domain;

// A column's type as SQL writes it: the type, and its length or its precision
// and scale where the type takes one.
struct Type {
    LogicalType type = LogicalType::Unset;
    std::uint32_t length = 0;
    std::uint32_t scale = 0;
};
constexpr Type whole{LogicalType::Int};
constexpr Type small{LogicalType::SmallInt};
constexpr Type tiny{LogicalType::TinyInt};
constexpr Type yes_no{LogicalType::Bit};
constexpr Type day{LogicalType::Date};
constexpr Type words(std::uint32_t length) { return {LogicalType::NVarchar, length}; }
constexpr Type code(std::uint32_t length) { return {LogicalType::Varchar, length}; }
constexpr Type amount(std::uint32_t precision, std::uint32_t scale) {
    return {LogicalType::Decimal, precision, scale};
}

// Whether a column may be left empty, and whether two rows may hold the same.
enum class Fill { Nullable, Required };
enum class Rows { Any, Unique };

// A schema drawn through the Editor, a command at a time, as a hand drawing it
// on the schema would: nothing is put into the project except by the commands
// the Table tool, Connect and the Properties panel already use. The examples
// are fixed, so a command refused here is a mistake in them, and is said at
// once rather than left as a schema missing a table.
class Schema {
public:
    explicit Schema(application::Editor& editor) : editor_(editor) {}

    // A table where its top-left corner is asked for, with the key the Table
    // tool gives every table: its name and ID, a whole number, the primary
    // key. A key that stands for nothing but the row counts itself up.
    RelationId table(const char* name, double x, double y, bool counting = true) {
        const auto made = editor_.create_relation(name, Point{x, y});
        must(made);
        const auto id = std::get<RelationId>(*made.created);
        if (counting) must(editor_.set_schema_column_auto_increment(key(id), true));
        return id;
    }

    // The key the table was made with.
    [[nodiscard]] SchemaColumnId key(RelationId table) const {
        return editor_.project().schema.added.at(table).front().id;
    }

    // An ordinary column at the end of the table, typed and ruled.
    SchemaColumnId column(RelationId table, const char* name, Type type, Fill fill = Fill::Nullable,
                          Rows rows = Rows::Any) {
        must(editor_.add_schema_column(ElementRef{table}, name));
        const auto id = editor_.project().schema.added.at(table).back().id;
        must(editor_.set_schema_column_type(id, type.type));
        if (type.length > 0) must(editor_.set_schema_column_size(id, type.length, type.scale));
        rule(id, false, fill, rows);
        return id;
    }

    // A foreign key at the end of the table: a column made to reference
    // another table's key -- or its own table's -- taking the key's type, as
    // Create & Connect makes one.
    SchemaColumnId refer(RelationId table, const char* name, RelationId to, Fill fill, Rows rows = Rows::Any) {
        const auto id = connect(table, std::nullopt, name, to);
        rule(id, false, fill, rows);
        return id;
    }

    // The table's own key renamed and made to reference another table's key
    // as well, as Use as PK + FK makes it: the key of a table that is
    // identified through the one it references.
    void key_refers(RelationId table, const char* name, RelationId to) {
        must(editor_.rename_schema_column(key(table), name));
        connect(table, key(table), "", to);
    }

    // A foreign key that joins the table's primary key, which a junction's
    // second key column is.
    SchemaColumnId key_part_refers(RelationId table, const char* name, RelationId to) {
        const auto id = connect(table, std::nullopt, name, to);
        rule(id, true, Fill::Required, Rows::Any);
        return id;
    }

    // An ordinary column that joins the table's primary key.
    SchemaColumnId key_part(RelationId table, const char* name, Type type) {
        const auto id = column(table, name, type, Fill::Required);
        rule(id, true, Fill::Required, Rows::Any);
        return id;
    }

private:
    SchemaColumnId connect(RelationId table, std::optional<SchemaColumnId> column, const char* name, RelationId to) {
        ForeignKeyId made{};
        must(editor_.connect_foreign_key(table, column, name, to, key(to), &made));
        return editor_.project().schema.foreign_keys.at(made).column;
    }

    void rule(SchemaColumnId id, bool key, Fill fill, Rows rows) {
        must(editor_.set_schema_column_rules(id, key, fill == Fill::Required, rows == Rows::Unique));
    }

    static void must(const application::EditResult& result) {
        if (!result) throw std::logic_error("A Relational example could not be built: " + result.error);
    }

    application::Editor& editor_;
};

constexpr auto required = Fill::Required;
constexpr auto nullable = Fill::Nullable;
constexpr auto unique = Rows::Unique;

} // namespace

void build_company_database_relational(application::Editor& editor) {
    editor.new_schema_project();
    editor.rename_project("Company Database — Relational");
    Schema s(editor);

    // Where each table stands. Employee is at the top in the middle, with its
    // dependents and telephone numbers to its left, its office under it, and
    // what it is joined to on its right: skills, teams and jobs. Under that
    // the work runs down the middle -- the employees assigned to projects,
    // the teams assigned to them, and Project itself -- with tasks and the
    // products a project uses to its right. Departments and the money are on
    // the left, and the supply chain along the foot. A junction stands
    // between the two tables it joins, and every line meets the tables either
    // side of it, as near as the tables allow.
    const auto office = s.table("Office", 675, 497);
    const auto job = s.table("Job", 1237, 428);
    const auto department = s.table("Department", 100, 612);
    const auto department_location = s.table("DepartmentLocation", 100, 451, false);
    const auto employee = s.table("Employee", 675, 60);
    const auto employee_phone = s.table("EmployeePhone", 100, 290, false);
    const auto dependent = s.table("Dependent", 100, 60, false);
    const auto employee_skill = s.table("EmployeeSkill", 1237, 60, false);
    const auto skill = s.table("Skill", 1792, 60);
    const auto project_assignment = s.table("ProjectAssignment", 675, 750, false);
    const auto team_member = s.table("TeamMember", 1237, 267, false);
    const auto project = s.table("Project", 675, 1141);
    const auto team = s.table("Team", 1237, 658);
    const auto team_project = s.table("TeamProject", 675, 980, false);
    const auto task = s.table("Task", 1237, 842);
    const auto client = s.table("Client", 100, 1394);
    const auto invoice = s.table("Invoice", 100, 1141);
    const auto payment = s.table("Payment", 100, 888);
    const auto project_product = s.table("ProjectProduct", 1237, 1141, false);
    const auto product = s.table("Product", 675, 1624);
    const auto supplier_product = s.table("SupplierProduct", 675, 1463, false);
    const auto supplier = s.table("Supplier", 100, 1647);

    s.column(office, "OfficeName", words(100), required);
    s.column(office, "City", words(60), required);
    s.column(office, "Country", words(60), required);
    s.column(office, "Address", words(200));
    s.column(office, "Phone", code(20));

    s.column(job, "JobTitle", words(100), required, unique);
    s.column(job, "Description", words(500));
    s.column(job, "MinimumSalary", amount(12, 2));
    s.column(job, "MaximumSalary", amount(12, 2));

    s.column(department, "DepartmentName", words(100), required, unique);
    s.column(department, "Budget", amount(14, 2));
    s.column(department, "Phone", code(20));
    s.column(department, "CreatedDate", day);

    // A department's locations, any number of them: a table of their own,
    // keyed by the department and the location together.
    s.key_refers(department_location, "DepartmentID", department);
    s.key_part(department_location, "Location", words(100));

    // Age is worked out from BirthDate rather than stored, and the job's
    // title is the Job table's: neither is a column here.
    s.column(employee, "FirstName", words(50), required);
    s.column(employee, "MiddleName", words(50));
    s.column(employee, "LastName", words(50), required);
    s.column(employee, "BirthDate", day);
    s.column(employee, "Gender", words(20));
    s.column(employee, "Email", words(255), required, unique);
    s.column(employee, "Address", words(200));
    s.column(employee, "HireDate", day, required);
    s.column(employee, "Salary", amount(12, 2));
    s.refer(employee, "JobID", job, required);
    s.refer(employee, "OfficeID", office, required);
    // A supervisor is another employee: a foreign key into the same table.
    s.refer(employee, "SupervisorID", employee, nullable);
    s.refer(employee, "DepartmentID", department, required);

    // Whoever manages a department, and since when. One department each, so
    // the key is unique; left empty until somebody is appointed, since a
    // department and its first employees are added before either can point
    // at the other.
    s.refer(department, "ManagerID", employee, nullable, unique);
    s.column(department, "ManagerStartDate", day);

    // An employee's telephone numbers, any number of them.
    s.key_refers(employee_phone, "EmployeeID", employee);
    s.key_part(employee_phone, "PhoneNumber", code(20));

    // A dependent is known by its name within the employee it depends on, so
    // the employee's key is the first part of its own.
    s.key_refers(dependent, "EmployeeID", employee);
    s.key_part(dependent, "DependentName", words(100));
    s.column(dependent, "BirthDate", day);
    s.column(dependent, "Relationship", words(30), required);
    s.column(dependent, "Gender", words(20));

    s.column(skill, "SkillName", words(100), required, unique);
    s.column(skill, "Description", words(500));

    // Which employees have which skills, and how well.
    s.key_refers(employee_skill, "EmployeeID", employee);
    s.key_part_refers(employee_skill, "SkillID", skill);
    s.column(employee_skill, "ProficiencyLevel", tiny, required);
    s.column(employee_skill, "YearsExperience", tiny);

    s.column(client, "ClientName", words(150), required);
    s.column(client, "Email", words(255), nullable, unique);
    s.column(client, "Phone", code(20));
    s.column(client, "Address", words(200));
    s.column(client, "Industry", words(60));

    // Every project is controlled by a department and belongs to a client.
    s.column(project, "ProjectName", words(150), required);
    s.column(project, "Description", words(1000));
    s.column(project, "StartDate", day, required);
    s.column(project, "EndDate", day);
    s.column(project, "Budget", amount(14, 2));
    s.column(project, "Status", words(20), required);
    s.refer(project, "DepartmentID", department, required);
    s.refer(project, "ClientID", client, required);

    // Who works on which project, for how long and as what.
    s.key_refers(project_assignment, "EmployeeID", employee);
    s.key_part_refers(project_assignment, "ProjectID", project);
    s.column(project_assignment, "Hours", amount(6, 2));
    s.column(project_assignment, "AssignedDate", day, required);
    s.column(project_assignment, "Role", words(60));

    s.column(team, "TeamName", words(100), required, unique);
    s.column(team, "CreatedDate", day);

    // A team's members, and the projects a team is assigned to.
    s.key_refers(team_member, "TeamID", team);
    s.key_part_refers(team_member, "EmployeeID", employee);
    s.key_refers(team_project, "TeamID", team);
    s.key_part_refers(team_project, "ProjectID", project);

    s.column(task, "TaskName", words(150), required);
    s.column(task, "Description", words(1000));
    s.column(task, "StartDate", day);
    s.column(task, "DueDate", day);
    s.column(task, "Status", words(20), required);
    s.column(task, "Priority", tiny);
    s.refer(task, "ProjectID", project, required);

    s.column(invoice, "IssueDate", day, required);
    s.column(invoice, "DueDate", day, required);
    s.column(invoice, "TotalAmount", amount(12, 2), required);
    s.column(invoice, "Status", words(20), required);
    s.refer(invoice, "ClientID", client, required);

    s.column(payment, "PaymentDate", day, required);
    s.column(payment, "Amount", amount(12, 2), required);
    s.column(payment, "PaymentMethod", words(30), required);
    s.column(payment, "Status", words(20), required);
    s.refer(payment, "InvoiceID", invoice, required);

    s.column(product, "ProductName", words(150), required);
    s.column(product, "Description", words(1000));
    s.column(product, "UnitPrice", amount(10, 2), required);
    s.column(product, "StockQuantity", whole, required);

    s.column(supplier, "SupplierName", words(150), required);
    s.column(supplier, "Email", words(255));
    s.column(supplier, "Phone", code(20));
    s.column(supplier, "Address", words(200));

    // The products a project uses, and who supplies each product.
    s.key_refers(project_product, "ProjectID", project);
    s.key_part_refers(project_product, "ProductID", product);
    s.key_refers(supplier_product, "SupplierID", supplier);
    s.key_part_refers(supplier_product, "ProductID", product);
}

void build_university_database_relational(application::Editor& editor) {
    editor.new_schema_project();
    editor.rename_project("University Database — Relational");
    Schema s(editor);

    // Where each table stands. Student is on the left, with its telephone
    // numbers and its loans above it and those who run the university under
    // it -- Professor, Department, Program. What joins a student to anything
    // stands in the middle, between Student and what it joins on the right:
    // club members beside the clubs, exam results beside the exams,
    // scholarships awarded beside the scholarships, the library at the top
    // and the courses, faculties and classrooms at the foot. Sections are at
    // the foot on the right, with their prerequisites above and their
    // assignments beside them, and enrollments at the top on the right. Every
    // line joins a table to its neighbour, as near as the tables allow.
    const auto faculty = s.table("Faculty", 662, 1256);
    const auto department = s.table("Department", 100, 1164);
    const auto professor = s.table("Professor", 100, 842);
    const auto program = s.table("Program", 100, 1440);
    const auto course = s.table("Course", 662, 1003);
    const auto course_prerequisite = s.table("CoursePrerequisite", 1230, 957, false);
    const auto classroom = s.table("Classroom", 662, 1463);
    const auto section = s.table("Section", 1230, 1118);
    const auto exam = s.table("Exam", 1230, 497);
    const auto exam_result = s.table("ExamResult", 662, 612, false);
    const auto assignment = s.table("Assignment", 1798, 1250);
    const auto enrollment = s.table("Enrollment", 1230, 60, false);
    const auto student = s.table("Student", 100, 474);
    const auto student_phone = s.table("StudentPhone", 100, 313, false);
    const auto library_book = s.table("LibraryBook", 662, 221);
    const auto book_author = s.table("BookAuthor", 662, 60, false);
    const auto book_loan = s.table("BookLoan", 100, 60);
    const auto club = s.table("Club", 1230, 290);
    const auto club_member = s.table("ClubMember", 662, 451, false);
    const auto scholarship = s.table("Scholarship", 1230, 727);
    const auto student_scholarship = s.table("StudentScholarship", 662, 842, false);

    s.column(faculty, "FacultyName", words(100), required, unique);
    s.column(faculty, "Office", words(60));
    s.column(faculty, "Phone", code(20));

    s.column(department, "DepartmentName", words(100), required, unique);
    s.column(department, "Office", words(60));
    s.column(department, "Phone", code(20));
    s.column(department, "Budget", amount(14, 2));
    s.refer(department, "FacultyID", faculty, required);

    s.column(professor, "FirstName", words(50), required);
    s.column(professor, "LastName", words(50), required);
    s.column(professor, "Email", words(255), required, unique);
    s.column(professor, "Phone", code(20));
    s.column(professor, "Salary", amount(12, 2));
    s.column(professor, "HireDate", day, required);
    s.column(professor, "OfficeNumber", words(20));
    s.refer(professor, "DepartmentID", department, required);

    // The professor who heads a department. One department each, so the key
    // is unique; left empty until a head is appointed, since a department
    // and its professors are added before either can point at the other.
    s.refer(department, "HeadID", professor, nullable, unique);

    s.column(program, "ProgramName", words(100), required);
    s.column(program, "DegreeLevel", words(30), required);
    s.column(program, "DurationYears", tiny, required);
    s.refer(program, "DepartmentID", department, required);

    s.column(course, "CourseName", words(150), required);
    s.column(course, "Description", words(1000));
    s.column(course, "Credits", tiny, required);
    s.column(course, "Level", words(20));
    s.refer(course, "DepartmentID", department, required);

    // A course's prerequisites are other courses: both keys reference Course.
    s.key_refers(course_prerequisite, "CourseID", course);
    s.key_part_refers(course_prerequisite, "PrerequisiteID", course);

    s.column(classroom, "Building", words(60), required);
    s.column(classroom, "RoomNumber", words(10), required);
    s.column(classroom, "Capacity", small, required);

    // A section is one offering of a course, held in a classroom, and taught
    // by a professor once one is assigned.
    s.column(section, "SectionNumber", small, required);
    s.column(section, "Semester", words(20), required);
    s.column(section, "Year", small, required);
    s.column(section, "Capacity", small, required);
    s.refer(section, "CourseID", course, required);
    s.refer(section, "ProfessorID", professor, nullable);
    s.refer(section, "ClassroomID", classroom, required);

    s.column(exam, "ExamType", words(30), required);
    s.column(exam, "ExamDate", day, required);
    s.column(exam, "MaximumScore", amount(5, 2), required);
    s.refer(exam, "SectionID", section, required);

    s.column(assignment, "Title", words(150), required);
    s.column(assignment, "Description", words(1000));
    s.column(assignment, "DueDate", day, required);
    s.column(assignment, "MaximumPoints", amount(5, 2), required);
    s.refer(assignment, "SectionID", section, required);

    // Age is worked out from BirthDate rather than stored.
    s.column(student, "FirstName", words(50), required);
    s.column(student, "MiddleName", words(50));
    s.column(student, "LastName", words(50), required);
    s.column(student, "BirthDate", day);
    s.column(student, "Gender", words(20));
    s.column(student, "Email", words(255), required, unique);
    s.column(student, "Address", words(200));
    s.column(student, "EnrollmentDate", day, required);
    s.refer(student, "ProgramID", program, required);
    s.refer(student, "AdvisorID", professor, nullable);

    // A student's telephone numbers, any number of them.
    s.key_refers(student_phone, "StudentID", student);
    s.key_part(student_phone, "PhoneNumber", code(20));

    // Which students are enrolled in which sections, and how it went.
    s.key_refers(enrollment, "StudentID", student);
    s.key_part_refers(enrollment, "SectionID", section);
    s.column(enrollment, "EnrollmentDate", day, required);
    s.column(enrollment, "Grade", code(2));
    s.column(enrollment, "Status", words(20), required);

    // One result for each student in each exam.
    s.key_refers(exam_result, "ExamID", exam);
    s.key_part_refers(exam_result, "StudentID", student);
    s.column(exam_result, "Score", amount(5, 2), required);
    s.column(exam_result, "Grade", code(2));
    s.column(exam_result, "Passed", yes_no, required);

    s.column(library_book, "ISBN", code(13), required);
    s.column(library_book, "Title", words(200), required);
    s.column(library_book, "Publisher", words(100));
    s.column(library_book, "PublicationYear", small);

    // A book's authors, any number of them.
    s.key_refers(book_author, "LibraryBookID", library_book);
    s.key_part(book_author, "AuthorName", words(100));

    // A loan has a key of its own, since one student may borrow the same
    // book again after returning it.
    s.column(book_loan, "BorrowDate", day, required);
    s.column(book_loan, "DueDate", day, required);
    s.column(book_loan, "ReturnDate", day);
    s.refer(book_loan, "StudentID", student, required);
    s.refer(book_loan, "LibraryBookID", library_book, required);

    s.column(club, "ClubName", words(100), required, unique);
    s.column(club, "Description", words(500));
    s.column(club, "CreatedDate", day);

    s.column(scholarship, "ScholarshipName", words(150), required);
    s.column(scholarship, "Amount", amount(12, 2), required);
    s.column(scholarship, "StartDate", day, required);
    s.column(scholarship, "EndDate", day);

    // The clubs a student has joined, and the scholarships awarded to them.
    s.key_refers(club_member, "StudentID", student);
    s.key_part_refers(club_member, "ClubID", club);
    s.key_refers(student_scholarship, "StudentID", student);
    s.key_part_refers(student_scholarship, "ScholarshipID", scholarship);
}

void build_basic_relational_schema(application::Editor& editor) {
    editor.new_schema_project();
    Schema s(editor);
    // Each key named for its table, as the Table tool names it, so renaming a
    // table renames its key with it.
    // Side by side, within what the schema shows beside its panels, Parent
    // two rows lower so that its key and the foreign key referencing it stand
    // level and the line between them runs straight.
    const auto parent = s.table("Parent", 100, 126, false);
    const auto child = s.table("Child", 500, 80, false);
    s.column(parent, "Name", words(100), required);
    s.column(child, "Name", words(100), required);
    s.refer(child, "ParentID", parent, required);
}

} // namespace erdflow::desktop
