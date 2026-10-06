// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "schema_view.hpp"
#include "schema_facts.hpp"

#include "theme.hpp"
#include "application/editor.hpp"

#include <QKeyEvent>
#include <QLineEdit>
#include <QStringList>
#include <QToolTip>

#include <QContextMenuEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QFontMetricsF>
#include <QPainterPath>
#include <QResizeEvent>
#include <QScopedValueRollback>

#include <algorithm>
#include <set>
#include <vector>
#include <queue>
#include <limits>
#include <array>
#include <cmath>

namespace erdflow::desktop {
namespace {
// The constraints a column carries, written at the right of its row: whether
// it may be empty, whether no two rows may share it, and whether the database
// fills it in. They are always drawn, never hidden when they are off, because
// what makes them readable is reading down the column to find the rows that
// have one -- and a mark that is missing when it is off cannot be read that
// way. An unset one is written faintly instead.
//
// Each has a column of its own, ruled off from its neighbours: three words
// side by side with nothing between them cannot be told apart, and which one
// a mark belongs to is the whole of what makes it readable.
//
// They are written out in full -- PK, NOT NULL, UNIQUE, IDENTITY -- because
// these are what the generated SQL will say, and an abbreviation is one more
// thing to learn before the table can be read. Writing them the way the DDL
// writes them is also what lets the column become that DDL later rather than
// having to be translated into it.
//
// They share one column and are chosen from a list. Three switches side by
// side could only ever hold three; a list of words holds whatever the column
// turns out to enforce, and reads as the one answer it is.
constexpr const char* longest_null = "NOT NULL";
constexpr const char* longest_unique = "UNIQUE";
constexpr const char* longest_identity = "IDENTITY";
// The air either side of a word inside its column, and how wide the
// constraints column is allowed to become before it stops growing.
constexpr double column_pad = 14;
constexpr double rules_min = 84;
constexpr double rules_max = 250;
// The least room a name is given. A table narrowed past what its columns need
// folds them away rather than squeezing its names to nothing: the names are
// what a table is, and no other column can stand in for them.
constexpr double name_floor = 60;
// What the type and its length take together. One column, not two: a type is
// written the way SQL writes it, varchar(255), so the length belongs inside
// the type rather than beside it. The width is the widest of them in the
// table, held between these, so the rules run straight down whatever each row
// happens to say.
constexpr double type_min = 74;
constexpr double type_max = 168;

// What a table falls back to before its own contents have been measured.
// Every real table works its width out from what it holds -- see
// natural_width -- so this is only the floor a table with nothing in it
// stands on.
constexpr double table_width = 320;
// How far to either side of a table's edge the strip reaches that pulls it,
// and how far along two edges their shared corner claims. A corner that was
// only as big as the strip is wide would be six pixels square, which is not
// something a hand can be asked to hit.
constexpr double resize_grip = 6;
constexpr double resize_corner = 16;
constexpr double header_height = 26;
// The row under the table's name that says what each column holds: Column,
// Type, Constraints. A table read as a table needs its columns named, or a
// reader has to work out from the contents what each one is for.
constexpr double heading_height = 19;
constexpr double row_height = 23;
// Wide enough for the letters and a key drawn solid beside them. A hairline
// key fits in less and reads as nothing, so the room is given rather than
// the mark shrunk.
constexpr double gutter_width = 46;
constexpr double column_gap = 64;
constexpr double row_gap = 30;
constexpr double margin = 84;    // room on the left for the lines to run in
constexpr double grid_step = 20;
// The footer a table grows when it carries a question: a line for the words,
// a row for the answers, and air around them.
constexpr double question_height = 19;
constexpr double chip_height = 22;
constexpr double chip_padding = 9;
constexpr double chip_gap = 5;
constexpr double footer_pad = 7;
// The blank a column waits in while nobody has said what type it is.
constexpr double ask_height = 16;
constexpr double ask_radius = 5;
// The size beside a measured type. Wide enough for four figures, and wider
// again where a precision carries its scale as well.

// How far apart two lines leaving the same side of the same table turn, so
// their long runs do not lie on top of one another. Three lanes is as many as
// the gap between two columns of tables will take before a line would have to
// turn inside its neighbour.
constexpr double lane_step = 10;
constexpr int lanes_across = 3;
// How near the pointer has to be to take hold of a line, and how big the
// corners drawn on the line it is over are.
constexpr double grab_reach = 7;
constexpr double handle_size = 7;
// Travel that separates shaping a line from merely clicking it, so a corner
// cannot appear under a hand that only twitched.
constexpr double shaping_travel = 4;
// The end symbols: the foot reaches this far out of the table, opens this wide,
// and the minimum sits out here beyond it.
constexpr double foot_reach = 13;
constexpr double foot_spread = 6;
constexpr double minimum_at = 21;
// The straight length of line the symbols at an end need to sit on. They are
// drawn along the line, so the line has to go straight for at least as far as
// they reach, or it turns out from under them.
constexpr double end_reach = minimum_at + 7;

// The type's own name, with nothing about its size. Where the size can be
// changed it is drawn beside the name rather than inside it, so the two are
// wanted apart.
QString type_name(const domain::PreviewColumn& column) {
    if (column.type == domain::LogicalType::Unset) return "?";
    return QString::fromStdString([&] {
        switch (column.type) {
        case domain::LogicalType::Int: return "int";
        case domain::LogicalType::BigInt: return "bigint";
        case domain::LogicalType::SmallInt: return "smallint";
        case domain::LogicalType::TinyInt: return "tinyint";
        case domain::LogicalType::Bit: return "bit";
        case domain::LogicalType::Decimal: return "decimal";
        case domain::LogicalType::Numeric: return "numeric";
        case domain::LogicalType::Money: return "money";
        case domain::LogicalType::SmallMoney: return "smallmoney";
        case domain::LogicalType::Float: return "float";
        case domain::LogicalType::Real: return "real";
        case domain::LogicalType::Char: return "char";
        case domain::LogicalType::Varchar: return "varchar";
        case domain::LogicalType::VarcharMax: return "varchar(max)";
        case domain::LogicalType::Text: return "text";
        case domain::LogicalType::NChar: return "nchar";
        case domain::LogicalType::NVarchar: return "nvarchar";
        case domain::LogicalType::NVarcharMax: return "nvarchar(max)";
        case domain::LogicalType::NText: return "ntext";
        case domain::LogicalType::Binary: return "binary";
        case domain::LogicalType::Varbinary: return "varbinary";
        case domain::LogicalType::VarbinaryMax: return "varbinary(max)";
        case domain::LogicalType::Image: return "image";
        case domain::LogicalType::Date: return "date";
        case domain::LogicalType::Time: return "time";
        case domain::LogicalType::DateTime: return "datetime";
        case domain::LogicalType::DateTime2: return "datetime2";
        case domain::LogicalType::DateTimeOffset: return "datetimeoffset";
        case domain::LogicalType::SmallDateTime: return "smalldatetime";
        case domain::LogicalType::UniqueIdentifier: return "uniqueidentifier";
        case domain::LogicalType::Xml: return "xml";
        case domain::LogicalType::RowVersion: return "rowversion";
        case domain::LogicalType::HierarchyId: return "hierarchyid";
        case domain::LogicalType::SqlVariant: return "sql_variant";
        case domain::LogicalType::Cursor: return "cursor";
        case domain::LogicalType::Table: return "table";
        case domain::LogicalType::Geometry: return "geometry";
        case domain::LogicalType::Geography: return "geography";
        case domain::LogicalType::Unset: break;
        }
        return "?";
    }());
}

// The type as it reads where nothing about it can be changed: the name with
// its size written into it, the way a schema is normally written down.
// The type and its length as one word, the way SQL writes it. A length nobody
// has given yet is shown as what it wants rather than as a number, so a column
// still waiting still says so: varchar(n), decimal(p,s).
QString typed_label(const domain::PreviewColumn& column) {
    if (column.type == domain::LogicalType::Unset) return "?";
    const auto takes = domain::size_of(column.type);
    if (takes == domain::TypeSize::None) return type_name(column);
    if (!column.length) return type_name(column) + (takes == domain::TypeSize::Precision ? "(p,s)" : "(n)");
    return type_name(column)
         + (takes == domain::TypeSize::Precision
                ? QString("(%1,%2)").arg(column.length).arg(column.scale)
                : QString("(%1)").arg(column.length));
}

// Everything a column enforces, in the order SQL would state it. A key is
// not said to be unique as well: it is unique by being the key, and saying so
// twice would be a constraint the database already keeps.
QString rules_text(const domain::PreviewColumn& column) {
    if (column.ignored) return {};
    QStringList said;
    if (column.primary_key) said << "PK";
    said << (column.required ? longest_null : "NULL");
    if (column.unique && !column.primary_key) said << longest_unique;
    if (column.auto_increment) said << longest_identity;
    return said.join(", ");
}

QString type_text(const domain::PreviewColumn& column) {
    auto written = type_name(column);
    if (column.type == domain::LogicalType::Unset) return written;
    if (domain::size_of(column.type) == domain::TypeSize::Precision && column.length)
        written += QString("(%1,%2)").arg(column.length).arg(column.scale);
    else if (domain::size_of(column.type) == domain::TypeSize::Length && column.length)
        written += QString("(%1)").arg(column.length);
    return written;
}

// A line of its own colour, so two crossing lines can still be told apart.
// Hues step by the golden angle, which spaces each new one as far from all the
// previous as it can, and once the hues come round the lightness steps to a new
// band so a repeated hue is still a different colour.
QColor link_colour(std::size_t index, bool dark) {
    constexpr double golden = 137.508;
    const auto hue = std::fmod(static_cast<double>(index) * golden, 360.0);
    const auto band = (index / 12) % 3;
    const double base = dark ? 68 : 42;
    const double step = band == 1 ? (dark ? -15 : 15) : band == 2 ? (dark ? 13 : -11) : 0;
    // Yellow reads far brighter than blue at one lightness, so it is nudged by
    // hue to keep every line equally readable.
    const auto evened = std::cos((hue - 70) * M_PI / 180.0) * (dark ? -7 : 7);
    return QColor::fromHslF(hue / 360.0, dark ? 0.60 : 0.66,
                            std::clamp((base + step - evened) / 100.0, 0.12, 0.88));
}

// The same for a theme with no colour: a grey of its own for each line, so two
// crossing lines can still be told apart (Zain chose this over one grey or dash
// patterns, 2026-09-24). The shade steps by the golden ratio through a band
// dark enough to read on the paper, so each new line is as far as it can be
// from those before it.
QColor link_grey(std::size_t index, bool dark) {
    const auto spread = std::fmod(static_cast<double>(index) * 0.6180339887, 1.0);
    const auto lightness = dark ? 0.52 + spread * 0.36 : 0.12 + spread * 0.44;
    return QColor::fromHslF(0.0f, 0.0f, static_cast<float>(lightness));
}
} // namespace

QString written_type(const domain::PreviewColumn& column) { return type_text(column); }
QString written_type_name(const domain::PreviewColumn& column) { return type_name(column); }

namespace {
// The router works over a coarse grid: the tables are blocked out, and a line
// finds its way between them. Turning costs more than going straight, so runs
// stay long; a cell another line has already used costs more still, which is
// what keeps two lines going the same way apart without a rule about lanes.
constexpr int cell = 9;
constexpr int clearance = 6;
constexpr int turn_cost = 4;
constexpr int share_cost = 7;

struct Grid {
    int columns = 0;
    int rows = 0;
    std::vector<std::uint8_t> blocked;
    std::vector<std::uint16_t> used;
    // The search's own working room, kept with the grid and cleared between
    // lines rather than allocated afresh for each. Every line on the schema is
    // routed again whenever a table is dragged, and two vectors the size of the
    // whole grid per line per frame is a cost paid while the hand is moving.
    std::vector<int> best;
    std::vector<int> came;
    [[nodiscard]] int at(int x, int y) const { return y * columns + x; }
    [[nodiscard]] bool inside(int x, int y) const {
        return x >= 0 && y >= 0 && x < columns && y < rows;
    }
};

struct Step { int x; int y; int direction; int cost; };

// A* over the grid. Returns the cells walked, or nothing where no way could be
// found -- which the caller answers with a plain elbow rather than no line.
std::vector<QPoint> find_way(Grid& grid, QPoint from, QPoint to) {
    if (!grid.inside(from.x(), from.y()) || !grid.inside(to.x(), to.y())) return {};
    // Both ends sit against their own tables, so their cells are let through.
    grid.blocked[grid.at(from.x(), from.y())] = 0;
    grid.blocked[grid.at(to.x(), to.y())] = 0;

    const auto cells = static_cast<std::size_t>(grid.columns) * static_cast<std::size_t>(grid.rows);
    auto& best = grid.best;
    auto& came = grid.came;
    best.assign(cells, std::numeric_limits<int>::max());
    came.assign(cells, -1);
    const auto compare = [](const Step& a, const Step& b) { return a.cost > b.cost; };
    std::priority_queue<Step, std::vector<Step>, decltype(compare)> open(compare);

    const auto start = grid.at(from.x(), from.y());
    best[static_cast<std::size_t>(start)] = 0;
    open.push({from.x(), from.y(), -1, 0});

    static constexpr std::array<QPoint, 4> moves{QPoint{1, 0}, QPoint{-1, 0}, QPoint{0, 1}, QPoint{0, -1}};
    std::size_t examined = 0;
    while (!open.empty() && examined < 60000) {
        const auto here = open.top();
        open.pop();
        ++examined;
        if (here.x == to.x() && here.y == to.y()) {
            std::vector<QPoint> path;
            auto at = grid.at(here.x, here.y);
            while (at >= 0) {
                path.push_back(QPoint(at % grid.columns, at / grid.columns));
                if (at == start) break;
                at = came[static_cast<std::size_t>(at)];
            }
            std::reverse(path.begin(), path.end());
            return path;
        }
        for (int m = 0; m < 4; ++m) {
            const auto next_x = here.x + moves[static_cast<std::size_t>(m)].x();
            const auto next_y = here.y + moves[static_cast<std::size_t>(m)].y();
            if (!grid.inside(next_x, next_y)) continue;
            const auto index = static_cast<std::size_t>(grid.at(next_x, next_y));
            if (grid.blocked[index]) continue;
            const auto step = 1 + (here.direction != -1 && here.direction != m ? turn_cost : 0)
                            + grid.used[index] * share_cost;
            const auto walked = here.cost + step;
            if (walked >= best[index]) continue;
            best[index] = walked;
            came[index] = grid.at(here.x, here.y);
            open.push({next_x, next_y, m,
                       walked + std::abs(to.x() - next_x) + std::abs(to.y() - next_y)});
        }
    }
    return {};
}

// Drop the points that repeat, which squaring a route off readily produces and
// which would otherwise be corners the pointer could grab at no length.
void tighten(std::vector<QPointF>& corners) {
    corners.erase(std::unique(corners.begin(), corners.end(), [](QPointF a, QPointF b) {
        return std::abs(a.x() - b.x()) < 0.01 && std::abs(a.y() - b.y()) < 0.01;
    }), corners.end());
}

// Drop a corner that is not a corner. Where the runs either side of a point lie
// along the same axis, the point is in the middle of a straight length and says
// nothing -- or worse, it is the tip of a little spur where the line goes out
// and comes back, which is what the router leaves behind when it rounds the
// start of a route to its own coarse grid. Either way the line reads better
// without it, and a run that is really one run can then be moved as one.
void straighten(std::vector<QPointF>& line) {
    for (std::size_t i = 1; i + 1 < line.size();) {
        const bool before = std::abs(line[i].x() - line[i - 1].x()) < 0.01;
        const bool after = std::abs(line[i + 1].x() - line[i].x()) < 0.01;
        if (before == after) line.erase(line.begin() + static_cast<std::ptrdiff_t>(i));
        else ++i;
    }
}

// Cells to corners, with the straight runs collapsed: a route of one point per
// cell would be a staircase of hundreds of segments.
std::vector<QPointF> corners_of(const std::vector<QPoint>& cells, QPointF from, QPointF to) {
    std::vector<QPointF> corners{from};
    QPointF previous = from;
    for (std::size_t i = 1; i + 1 < cells.size(); ++i) {
        const auto before = cells[i - 1];
        const auto here = cells[i];
        const auto after = cells[i + 1];
        const bool straight = (before.x() == here.x() && here.x() == after.x())
                           || (before.y() == here.y() && here.y() == after.y());
        if (straight) continue;
        const QPointF corner(here.x() * cell, here.y() * cell);
        // Only the turns are kept, and a turn is squared off so the line stays
        // orthogonal all the way.
        corners.push_back(QPointF(corner.x(), previous.y()));
        corners.push_back(corner);
        previous = corner;
    }
    corners.push_back(QPointF(to.x(), previous.y()));
    corners.push_back(to);
    tighten(corners);
    return corners;
}

QPainterPath path_of(const std::vector<QPointF>& corners) {
    if (corners.empty()) return {};
    QPainterPath path(corners.front());
    for (std::size_t i = 1; i < corners.size(); ++i) path.lineTo(corners[i]);
    return path;
}

// Walk to the next point in right angles, carrying on along whichever axis the
// line is already travelling so that it turns once rather than doubling back on
// itself. This is what routes a line through the corners somebody has put in by
// hand, and it keeps the same orthogonal shape the router produces.
void step_to(std::vector<QPointF>& corners, bool& horizontal, QPointF next) {
    const auto previous = corners.back();
    const bool across = std::abs(next.x() - previous.x()) > 0.01;
    const bool down = std::abs(next.y() - previous.y()) > 0.01;
    if (!across && !down) return;
    if (across && down) {
        corners.push_back(horizontal ? QPointF(next.x(), previous.y())
                                     : QPointF(previous.x(), next.y()));
        horizontal = !horizontal;
    } else {
        horizontal = across;
    }
    corners.push_back(next);
}

// Say where a route has been, so the next one is nudged off it rather than laid
// along it. The band is three cells wide because two lines a single cell apart
// still read as one thick line.
void mark(Grid& grid, const std::vector<QPointF>& corners) {
    const auto touch = [&](int x, int y) {
        for (int away = -1; away <= 1; ++away) {
            if (!grid.inside(x, y + away)) continue;
            auto& count = grid.used[static_cast<std::size_t>(grid.at(x, y + away))];
            count = static_cast<std::uint16_t>(std::min(8, count + (away == 0 ? 3 : 1)));
        }
    };
    for (std::size_t i = 1; i < corners.size(); ++i) {
        const auto from = corners[i - 1];
        const auto to = corners[i];
        const auto steps = static_cast<int>(std::max(std::abs(to.x() - from.x()),
                                                     std::abs(to.y() - from.y())) / cell) + 1;
        for (int step = 0; step <= steps; ++step) {
            const auto t = static_cast<double>(step) / steps;
            touch(static_cast<int>((from.x() + (to.x() - from.x()) * t) / cell),
                  static_cast<int>((from.y() + (to.y() - from.y()) * t) / cell));
        }
    }
}

// The nearest place on a table's outline to a point, which is where an end
// being dragged over its own table sits. Whichever edge is nearest takes it,
// and it slides along that edge, so an end can be put anywhere around a table
// rather than only where the router would have put it.
QPointF on_outline(const QRectF& box, QPointF here) {
    const auto left = std::abs(here.x() - box.left());
    const auto right = std::abs(here.x() - box.right());
    const auto top = std::abs(here.y() - box.top());
    const auto bottom = std::abs(here.y() - box.bottom());
    const auto nearest = std::min({left, right, top, bottom});
    if (nearest == left) return {box.left(), std::clamp(here.y(), box.top(), box.bottom())};
    if (nearest == right) return {box.right(), std::clamp(here.y(), box.top(), box.bottom())};
    if (nearest == top) return {std::clamp(here.x(), box.left(), box.right()), box.top()};
    return {std::clamp(here.x(), box.left(), box.right()), box.bottom()};
}

// A point on a table as a fraction of its box, and back again. Kept this way so
// that a join stays where it was put when the table is moved, resized by a new
// row, or renamed into a wider one.
QPointF fraction_in(const QRectF& box, QPointF at) {
    return {box.width() > 0.01 ? (at.x() - box.left()) / box.width() : 0.0,
            box.height() > 0.01 ? (at.y() - box.top()) / box.height() : 0.0};
}

QPointF point_in(const QRectF& box, QPointF fraction) {
    return {box.left() + fraction.x() * box.width(), box.top() + fraction.y() * box.height()};
}

// Which way a line leaves the place it is joined. On a table it is the square
// out of whichever edge the join sits on; off a table there is no edge, so it
// is the way the end lies from the table it came from.
QPointF step_away(const QRectF& box, QPointF at, bool on_table) {
    if (on_table) {
        if (std::abs(at.x() - box.left()) < 0.5) return {-1, 0};
        if (std::abs(at.x() - box.right()) < 0.5) return {1, 0};
        if (std::abs(at.y() - box.top()) < 0.5) return {0, -1};
        return {0, 1};
    }
    const auto away = at - box.center();
    if (std::abs(away.x()) >= std::abs(away.y())) return {away.x() < 0 ? -1.0 : 1.0, 0};
    return {0, away.y() < 0 ? -1.0 : 1.0};
}

// How near a point is to a run of segments, which is the whole of hit-testing a
// line: a line is grabbed wherever it is drawn, not only at its corners.
double distance_to(const std::vector<QPointF>& corners, QPointF point) {
    auto nearest = std::numeric_limits<double>::max();
    for (std::size_t i = 1; i < corners.size(); ++i) {
        const auto from = corners[i - 1];
        const auto along = corners[i] - from;
        const auto length = along.x() * along.x() + along.y() * along.y();
        const auto t = length < 0.01
            ? 0.0
            : std::clamp(QPointF::dotProduct(point - from, along) / length, 0.0, 1.0);
        const auto foot = from + along * t;
        nearest = std::min(nearest, std::hypot(point.x() - foot.x(), point.y() - foot.y()));
    }
    return nearest;
}

// Move one straight run of a line sideways, carrying the corners at both of its
// ends with it, so the runs either side of it stretch to keep up. This is what
// pushing a line aside means. Dropping a single corner into the middle of a
// straight length instead would send the line out to that corner and back
// again, which draws a spur rather than a line that has been moved.
//
// The two ends are pinned to their tables, so a run touching an end cannot
// carry that end along with it: it grows a corner there instead, and the line
// stays joined where it always was.
void slide_run(std::vector<QPointF>& line, std::size_t run, QPointF here) {
    if (run + 1 >= line.size()) return;
    if (run == 0) {
        line.insert(line.begin() + 1, line.front());
        ++run;
    }
    if (run + 1 == line.size() - 1) line.insert(line.end() - 1, line.back());
    auto& near_end = line[run];
    auto& far_end = line[run + 1];
    if (std::abs(near_end.x() - far_end.x()) < 0.01) {
        near_end.setX(here.x());
        far_end.setX(here.x());
    } else {
        near_end.setY(here.y());
        far_end.setY(here.y());
    }
}

// Hold the straight stretch at each end of a line at least as long as the
// symbols drawn there reach.
//
// The foot, the bar and the minimum are drawn along the line from its end. If
// the line turns before they finish, they are left standing in open space
// beside it -- the line appears to have walked away from its own constraints
// and abandoned them at the table, which is exactly what it has done. So the
// run just past the stub is pushed outwards until the stub is long enough,
// and whatever follows stretches to keep up. The join itself never moves: the
// line is held off the turn, not pulled off the table.
void reserve_ends(std::vector<QPointF>& line, QPointF from_step, QPointF to_step) {
    const auto hold = [&](bool at_front, QPointF step) {
        if (line.size() < 3) return;   // too close together to bend; draw what there is
        const auto end = at_front ? line.front() : line.back();
        const auto next = at_front ? line[1] : line[line.size() - 2];
        if (QPointF::dotProduct(next - end, step) >= end_reach) return;
        // The run past the stub is perpendicular to it, so sliding that run
        // is what lengthens the stub. It is the same move a hand makes.
        slide_run(line, at_front ? 1 : line.size() - 3, end + step * end_reach);
    };
    hold(true, from_step);
    hold(false, to_step);
}

// Which run of a line a point is nearest to, and how far off it is.
std::pair<std::size_t, double> run_nearest(const std::vector<QPointF>& line, QPointF point) {
    std::size_t nearest = 0;
    auto closest = std::numeric_limits<double>::max();
    for (std::size_t i = 1; i < line.size(); ++i) {
        const auto gap = distance_to({line[i - 1], line[i]}, point);
        if (gap >= closest) continue;
        closest = gap;
        nearest = i - 1;
    }
    return {nearest, closest};
}
} // namespace

SchemaView::SchemaView(application::Editor& editor, QWidget* parent)
    : QWidget(parent), editor_(editor) {
    setObjectName("schemaView");
    setMouseTracking(true);
    setMinimumHeight(180);
    // A press on the schema gives it the keyboard, as a press on the diagram
    // gives the diagram the keyboard, so what is typed next -- Select All --
    // is about the schema rather than about the diagram behind it.
    setFocusPolicy(Qt::ClickFocus);
}

void SchemaView::set_icon_mode(IconMode mode) {
    if (icon_mode_ == mode) return;
    icon_mode_ = mode;
    key_mark_ = {};
    update();
}

// The key drawn beside PK, kept until the size, the ink or the set it is taken
// from changes. Asking for it again on every row of every repaint is the kind
// of work a drag across a large schema pays for a hundred times a second.
const QPixmap& SchemaView::key_mark(int side) {
    const auto ratio = devicePixelRatioF();
    if (key_mark_.isNull() || key_mark_side_ != side || key_mark_mode_ != icon_mode_
        || key_mark_ink_ != theme_->warning || std::abs(key_mark_ratio_ - ratio) > 0.01) {
        key_mark_side_ = side;
        key_mark_mode_ = icon_mode_;
        key_mark_ink_ = theme_->warning;
        key_mark_ratio_ = ratio;
        // The golden key Zain chose (2026-09-26): held bow up and pointing
        // down, its own polished drawing rather than a glyph from the icon
        // set, and the same key a table will wear. Grey where the theme has
        // no colour.
        key_mark_ = primary_key_mark(side, ratio, colourless(theme_->id));
    }
    return key_mark_;
}

void SchemaView::set_theme(const Theme& theme) {
    theme_ = &theme;
    key_mark_ = {};
    reroute();
    update();
}

void SchemaView::set_notation(Notation notation) {
    if (notation_ == notation) return;
    notation_ = notation;
    update();
}

void SchemaView::refresh() {
    // A name being typed belongs to a row of the schema as it was. Worked out
    // again, the rows may not be the same rows, so the box is given up rather
    // than left standing over whatever has taken that place.
    cancel_rename();
    preview_ = domain::schema_preview(editor_.project());
    // Nothing of the arrangement is kept here to prune: it lives in the
    // project, where an element that goes takes its own entries with it.
    std::set<domain::LinkSource> present;
    for (const auto& table : preview_.tables)
        for (const auto& column : table.columns)
            if (column.link) present.insert(*column.link);
    if (hovered_ && !present.contains(*hovered_)) hovered_.reset();
    // Anything marked that the model no longer has is forgotten, rather than
    // left marked and unfindable.
    std::erase_if(selected_, [this](const domain::ElementRef& ref) {
        return std::none_of(preview_.tables.begin(), preview_.tables.end(),
                            [&](const domain::PreviewTable& table) {
                                return table.origin && *table.origin == ref;
                            });
    });
    // And a column or line picked that is no longer there, likewise.
    if (picked_) {
        const auto still = std::visit([this](const auto& what) {
            if constexpr (std::is_same_v<std::decay_t<decltype(what)>, SchemaColumnRef>)
                return locate(what).has_value();
            else
                return link_of(what).has_value();
        }, *picked_);
        if (!still) picked_.reset();
    }
    shaping_.reset();
    arrange();
    reroute();
    update();
    announce_invented_keys();
}

void SchemaView::announce_invented_keys() {
    // An entity with no key attribute is given a key by the rule, and that is
    // said out loud, once for each table, rather than left for somebody to
    // find by hovering over it (Zain, 2026-09-24). A table that has since been
    // given a key of its own is forgotten, so losing it again is said again.
    // Bridges and multivalued tables are given their own key by the course's
    // rules whatever is drawn, so there is nothing to tell anybody about them.
    std::vector<std::size_t> fresh;
    std::set<domain::ElementRef> invented;
    for (std::size_t t = 0; t < preview_.tables.size(); ++t) {
        const auto& table = preview_.tables[t];
        if (!table.origin || table.origin_kind != domain::TableOrigin::Entity) continue;
        const bool made_up = std::any_of(table.columns.begin(), table.columns.end(),
                                         [](const domain::PreviewColumn& column) {
                                             return column.primary_key && !column.origin
                                                 && column.origin_kind == domain::ColumnOrigin::Generated;
                                         });
        if (!made_up) continue;
        invented.insert(*table.origin);
        if (!announced_keys_.contains(*table.origin)) fresh.push_back(t);
    }
    announced_keys_ = std::move(invented);
    if (fresh.empty() || !warned) return;
    const auto& project = editor_.project();
    QStringList entities;
    QStringList keys;
    for (const auto t : fresh) {
        const auto& table = preview_.tables[t];
        const auto* entity = std::get_if<domain::EntityId>(&*table.origin);
        const auto named = entity ? project.entities.find(*entity) : project.entities.end();
        entities << QString::fromStdString(named != project.entities.end() ? named->second.name : table.name);
        for (const auto& column : table.columns)
            if (column.primary_key && !column.origin && column.origin_kind == domain::ColumnOrigin::Generated)
                keys << QString::fromStdString(column.name);
    }
    const auto listed = [](const QStringList& names) {
        if (names.size() < 2) return names.join(QString());
        return names.mid(0, names.size() - 1).join(QStringLiteral(", ")) + QStringLiteral(" and ") + names.back();
    };
    const auto said = fresh.size() == 1
        ? QStringLiteral("%1 has no key attribute, so %2 was made its primary key. "
                         "Mark one of its attributes as a key to use that instead.")
              .arg(entities.front(), listed(keys))
        : QStringLiteral("%1 have no key attribute, so a primary key was made for each: %2. "
                         "Mark an attribute of each as a key to use that instead.")
              .arg(listed(entities), listed(keys));
    const auto at = fresh.front() < placed_.size() ? placed_[fresh.front()].box.topLeft().toPoint() : QPoint();
    warned(said, mapToGlobal(at));
}

void SchemaView::release_lines() {
    shaping_.reset();
    shaping_shape_.reset();
    const auto result = editor_.release_schema_lines();
    if (arranged) arranged(result);
}

void SchemaView::set_routing(SchemaRouting routing) {
    if (routing_ == routing) return;
    routing_ = routing;
    reroute();
    update();
}

void SchemaView::set_lines_give_way(bool give_way) {
    lines_give_way_ = give_way;
}

// Which shaped lines are lying across a table. A line the router drew is not
// asked about: it already goes around, and if it could not, no shuffling of
// it would help.
std::vector<domain::LinkSource> SchemaView::lines_over_tables() const {
    std::vector<domain::LinkSource> crossing;
    for (const auto& routed : routes_) {
        if (!routed.link || !line_is_shaped(*routed.link)) continue;
        for (std::size_t i = 1; i < routed.corners.size(); ++i) {
            const QRectF run(routed.corners[i - 1], routed.corners[i]);
            const auto over = std::any_of(placed_.begin(), placed_.end(), [&](const Placed& one) {
                // Pulled in at the edges, because a line is supposed to meet
                // the outline of the table it joins; only a run that is
                // properly inside one counts as lying across it.
                return one.box.adjusted(3, 3, -3, -3).intersects(run.normalized());
            });
            if (!over) continue;
            crossing.push_back(*routed.link);
            break;
        }
    }
    return crossing;
}

void SchemaView::set_names_only(bool on) {
    if (names_only_ == on) return;
    names_only_ = on;
    arrange();
    reroute();
    update();
}

double SchemaView::heading_room() const { return names_only_ ? 0.0 : heading_height; }

void SchemaView::set_tables_resizable(bool resizable) {
    if (tables_resizable_ == resizable) return;
    tables_resizable_ = resizable;
    update();
}

QFont SchemaView::row_font() const {
    auto measuring = font();
    measuring.setFamily("Menlo");
    measuring.setPointSizeF(font().pointSizeF() - 0.5);
    return measuring;
}

SchemaView::Columns SchemaView::columns_of(const domain::PreviewTable& table) const {
    const QFontMetricsF typed(row_font());
    Columns room;
    // The constraints column takes the longest list any of these rows makes,
    // never less than room for the plainest of them, so nothing it says is
    // cut off by the rule beside it.
    double stated = typed.horizontalAdvance(longest_null);
    for (const auto& column : table.columns)
        stated = std::max(stated, typed.horizontalAdvance(rules_text(column)));
    room.rules = std::clamp(stated + column_pad, rules_min, rules_max);
    // The type column takes the widest type the table carries, so that every
    // one of them is written out whole and the rules still run straight.
    double widest = 0;
    for (const auto& column : table.columns) {
        if (column.ignored || !(column.origin || column.added)) continue;
        widest = std::max(widest, typed.horizontalAdvance(typed_label(column)));
    }
    room.type = std::clamp(widest + column_pad, type_min, type_max);
    // And the names take the longest of them, so the last letter of a name is
    // never cut off by the rule beside it.
    double named = 0;
    for (const auto& column : table.columns)
        named = std::max(named, typed.horizontalAdvance(QString::fromStdString(column.name)));
    // Never less than the floor, so that a table left at its natural width has
    // room for every column it holds and folds none of them away.
    room.name = std::max(name_floor, named + column_pad);
    return room;
}

// What a table is as wide as before a hand has said otherwise: everything it
// holds, written out whole. A table that had to elide its own names to fit a
// width nobody chose would be hiding work rather than showing it.
double SchemaView::natural_width(const domain::PreviewTable& table) const {
    const auto room = columns_of(table);
    // With only the names shown, a table is as wide as its keys and names.
    if (names_only_) {
        auto title_font = font();
        title_font.setBold(true);
        const auto title_width = QFontMetricsF(title_font).horizontalAdvance(
            QString::fromStdString(table.name)) + 18;
        return std::clamp(std::max(gutter_width + room.name, title_width),
                          domain::min_table_width, domain::max_table_width);
    }
    return std::clamp(gutter_width + room.name + room.type + room.rules,
                      domain::min_table_width, domain::max_table_width);
}

// As natural_width measures a table with every column shown, whichever way
// the schema is presented at the moment.
double SchemaView::full_width(const domain::PreviewTable& table) const {
    const auto room = columns_of(table);
    return std::clamp(gutter_width + room.name + room.type + room.rules,
                      domain::min_table_width, domain::max_table_width);
}

double SchemaView::width_of(const domain::PreviewTable& table) const {
    if (!table.origin || names_only_) return natural_width(table);
    if (const auto held = resizing_to_.find(*table.origin); held != resizing_to_.end())
        return held->second.box.width();
    const auto& widths = editor_.project().schema_layout.widths;
    const auto found = widths.find(table.id);
    return found == widths.end() ? natural_width(table) : found->second;
}

// A height given by hand is never less than the rows themselves need: rows are
// what a table is for, and a table is not allowed to be pulled down over its
// own columns. A table that gains a column afterwards therefore grows past the
// height it was given rather than hiding the new one.
double SchemaView::height_of(const domain::PreviewTable& table, double natural) const {
    if (!table.origin || names_only_) return natural;
    if (const auto held = resizing_to_.find(*table.origin); held != resizing_to_.end())
        return std::max(natural, held->second.box.height());
    const auto& heights = editor_.project().schema_layout.heights;
    const auto found = heights.find(table.id);
    return found == heights.end() ? natural : std::max(natural, found->second);
}

double SchemaView::natural_height(const domain::PreviewTable& table, double wide) const {
    return header_height + heading_room() + row_height * static_cast<double>(table.columns.size())
         + footer_height(table, wide);
}

bool SchemaView::moves_corner(Pull pull) { return pulls_left(pull) || pulls_top(pull); }

bool SchemaView::pulls_left(Pull pull) {
    return pull == Pull::Left || pull == Pull::TopLeft || pull == Pull::BottomLeft;
}

bool SchemaView::pulls_right(Pull pull) {
    return pull == Pull::Right || pull == Pull::TopRight || pull == Pull::BottomRight;
}

bool SchemaView::pulls_top(Pull pull) {
    return pull == Pull::Top || pull == Pull::TopLeft || pull == Pull::TopRight;
}

bool SchemaView::pulls_bottom(Pull pull) {
    return pull == Pull::Bottom || pull == Pull::BottomLeft || pull == Pull::BottomRight;
}

Qt::CursorShape SchemaView::cursor_for(Pull pull) {
    switch (pull) {
    case Pull::Left:
    case Pull::Right: return Qt::SizeHorCursor;
    case Pull::Top:
    case Pull::Bottom: return Qt::SizeVerCursor;
    // The two diagonals, each named for the way it leans: a top-left corner
    // and a bottom-right one are pulled along the same line.
    case Pull::TopLeft:
    case Pull::BottomRight: return Qt::SizeFDiagCursor;
    case Pull::TopRight:
    case Pull::BottomLeft: break;
    }
    return Qt::SizeBDiagCursor;
}

std::optional<SchemaView::Resizing> SchemaView::edge_at(QPointF point) const {
    if (!tables_resizable_ || names_only_) return std::nullopt;
    for (std::size_t i = placed_.size(); i-- > 0;) {
        if (!preview_.tables[i].origin) continue;
        const auto& box = placed_[i].box;
        if (!box.adjusted(-resize_grip, -resize_grip, resize_grip, resize_grip).contains(point)) continue;
        const auto left = std::abs(point.x() - box.left()) <= resize_grip;
        const auto right = std::abs(point.x() - box.right()) <= resize_grip;
        const auto top = std::abs(point.y() - box.top()) <= resize_grip;
        const auto bottom = std::abs(point.y() - box.bottom()) <= resize_grip;
        // Well inside the table, which is where it is moved and read rather
        // than pulled about.
        if (!left && !right && !top && !bottom) continue;
        const auto near_left = point.x() <= box.left() + resize_corner;
        const auto near_right = point.x() >= box.right() - resize_corner;
        const auto near_top = point.y() <= box.top() + resize_corner;
        const auto near_bottom = point.y() >= box.bottom() - resize_corner;
        // A corner answers before the two edges that meet at it, from either
        // of them: the hand going for a corner is asking for both sides.
        std::optional<Pull> pull;
        if ((left && near_top) || (top && near_left)) pull = Pull::TopLeft;
        else if ((right && near_top) || (top && near_right)) pull = Pull::TopRight;
        else if ((left && near_bottom) || (bottom && near_left)) pull = Pull::BottomLeft;
        else if ((right && near_bottom) || (bottom && near_right)) pull = Pull::BottomRight;
        else if (left) pull = Pull::Left;
        else if (right) pull = Pull::Right;
        else if (top) pull = Pull::Top;
        else if (bottom) pull = Pull::Bottom;
        if (!pull) continue;
        return Resizing{i, *pull, box, point};
    }
    return std::nullopt;
}

QRectF SchemaView::pulled_box(const Resizing& drag, QPointF pointer, double floor) const {
    const auto travelled = pointer - drag.grab;
    auto box = drag.start;
    // The side being pulled follows the pointer and the side opposite it stays
    // exactly where it was: a pull is one edge moving, not a box growing about
    // its middle. Nothing is snapped while it is held, for the same reason a
    // table being moved is not -- a box that jumps under the hand cannot be
    // aimed. A left or top edge stops at the edge of the schema, which is as
    // far as it can go and still be reached again.
    if (pulls_left(drag.pull)) {
        const auto widest = std::max(domain::min_table_width,
                                     std::min(domain::max_table_width, drag.start.right()));
        box.setLeft(drag.start.right()
                    - std::clamp(drag.start.width() - travelled.x(), domain::min_table_width, widest));
    } else if (pulls_right(drag.pull)) {
        box.setWidth(std::clamp(drag.start.width() + travelled.x(),
                                domain::min_table_width, domain::max_table_width));
    }
    const auto shortest = std::max(floor, domain::min_table_height);
    if (pulls_top(drag.pull)) {
        const auto tallest = std::max(shortest, std::min(domain::max_table_height, drag.start.bottom()));
        box.setTop(drag.start.bottom()
                   - std::clamp(drag.start.height() - travelled.y(), shortest, tallest));
    } else if (pulls_bottom(drag.pull)) {
        box.setHeight(std::clamp(drag.start.height() + travelled.y(), shortest,
                                 std::max(shortest, domain::max_table_height)));
    }
    return box;
}

void SchemaView::set_showing(SchemaShowing showing) {
    if (showing_ == showing) return;
    showing_ = showing;
    update();
}

void SchemaView::set_looking_for(const QString& looking_for) {
    const auto wanted = looking_for.trimmed();
    if (looking_for_ == wanted) return;
    looking_for_ = wanted;
    // Looking for something is a broader question than asking about one
    // table, so it puts down whatever was being asked about.
    if (!looking_for_.isEmpty()) {
        selected_.clear();
        picked_.reset();
    }
    update();
    if (chose) chose();
}

void SchemaView::set_answering(std::optional<Answering> which, bool size) {
    if (answering_ == which && answering_size_ == size) return;
    answering_ = which;
    answering_size_ = size;
    update();
}

void SchemaView::select(std::optional<domain::ElementRef> table) {
    std::vector<domain::ElementRef> wanted;
    if (table) wanted.push_back(*table);
    if (selected_ == wanted && !picked_) return;
    selected_ = std::move(wanted);
    // Marking tables is choosing tables: whatever column or line had been
    // picked is put down with what was marked before.
    picked_.reset();
    update();
    if (chose) chose();
}

void SchemaView::toggle_mark(const domain::ElementRef& table) {
    const auto found = std::find(selected_.begin(), selected_.end(), table);
    if (found != selected_.end()) selected_.erase(found);
    else selected_.push_back(table);
    picked_.reset();
    update();
    if (chose) chose();
}

void SchemaView::select_all() {
    std::vector<domain::ElementRef> every;
    for (const auto& table : preview_.tables)
        if (table.origin && std::find(every.begin(), every.end(), *table.origin) == every.end())
            every.push_back(*table.origin);
    if (selected_ == every && !picked_) return;
    selected_ = std::move(every);
    picked_.reset();
    update();
    if (chose) chose();
}

void SchemaView::keyPressEvent(QKeyEvent* event) {
    // Escape puts Table down, as it puts a placing tool down on the diagram.
    if (event->key() == Qt::Key_Escape && placing_) {
        set_placing(false);
        if (placing_changed) placing_changed(false);
        event->accept();
        return;
    }
    // A line being drawn from a key's gutter, without Connect in hand, is let
    // go of and nothing is asked for it.
    if (event->key() == Qt::Key_Escape && linking_ && !connecting_) {
        linking_.reset();
        update();
        event->accept();
        return;
    }
    // Escape puts Connect down, as it puts a tool down on the diagram.
    if (event->key() == Qt::Key_Escape && connecting_) {
        set_connecting(false);
        if (connecting_changed) connecting_changed(false);
        event->accept();
        return;
    }
    // Select All gathers every table, as it gathers everything on the diagram,
    // so the whole schema can be taken hold of and moved as one.
    if (event->matches(QKeySequence::SelectAll)) {
        select_all();
        event->accept();
        return;
    }
    // On a schema drawn by hand, Delete takes away what is marked, as it
    // does on the diagram.
    if ((event->key() == Qt::Key_Delete || event->key() == Qt::Key_Backspace) && drawn_by_hand()
        && delete_asked && !selected_.empty()) {
        delete_asked();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

// Whether one table is among those marked. Asked often enough, and in enough
// places, to be worth asking one way.
bool SchemaView::is_marked(const domain::PreviewTable& table) const {
    return table.origin
        && std::find(selected_.begin(), selected_.end(), *table.origin) != selected_.end();
}

// Which tables are being pointed out. A selection wins over the narrowing,
// because asking about one table is a more particular question than asking
// about a kind of table, and answering the general one over the top of it
// would throw away what was just asked.
std::optional<std::size_t> SchemaView::selected_table() const {
    const auto one = selected();
    if (!one) return std::nullopt;
    for (std::size_t i = 0; i < preview_.tables.size(); ++i)
        if (preview_.tables[i].origin && *preview_.tables[i].origin == *one) return i;
    return std::nullopt;
}

std::vector<bool> SchemaView::lit_tables() const {
    std::vector<bool> lit(preview_.tables.size(), true);
    // A search is the most particular question of the three, so it answers
    // first: somebody who has typed a name is looking for that name.
    if (!looking_for_.isEmpty()) {
        for (std::size_t t = 0; t < preview_.tables.size(); ++t) {
            const auto& table = preview_.tables[t];
            const auto named = [&](const std::string& what) {
                return QString::fromStdString(what).contains(looking_for_, Qt::CaseInsensitive);
            };
            lit[t] = named(table.name)
                  || std::any_of(table.columns.begin(), table.columns.end(),
                                 [&](const domain::PreviewColumn& column) { return named(column.name); });
        }
        return lit;
    }
    // Several marked together means exactly those: a band was drawn round them
    // or they were pressed one by one, and neither says anything about what
    // they are joined to. One marked is the older question -- what does this
    // one touch -- and still answers it.
    if (selected_.size() > 1) {
        lit.assign(preview_.tables.size(), false);
        for (std::size_t t = 0; t < preview_.tables.size(); ++t)
            if (is_marked(preview_.tables[t])) lit[t] = true;
        return lit;
    }
    if (const auto one = selected()) {
        const auto here = std::find_if(preview_.tables.begin(), preview_.tables.end(),
                                       [&](const domain::PreviewTable& table) {
                                           return table.origin && *table.origin == *one;
                                       });
        if (here != preview_.tables.end()) {
            const auto which = static_cast<std::size_t>(std::distance(preview_.tables.begin(), here));
            lit.assign(preview_.tables.size(), false);
            lit[which] = true;
            // One step out, both ways: what this table points at, and what
            // points at it. A key is a two-ended fact and a reader asking what
            // a table touches means both ends of it.
            for (const auto& column : preview_.tables[which].columns)
                if (column.references && *column.references < lit.size()) lit[*column.references] = true;
            for (std::size_t t = 0; t < preview_.tables.size(); ++t)
                for (const auto& column : preview_.tables[t].columns)
                    if (column.references && *column.references == which) lit[t] = true;
            return lit;
        }
    }
    if (showing_ == SchemaShowing::Everything) return lit;
    for (std::size_t t = 0; t < preview_.tables.size(); ++t) {
        switch (preview_.tables[t].origin_kind) {
        case domain::TableOrigin::Entity:
        case domain::TableOrigin::Subtype:
            lit[t] = showing_ == SchemaShowing::FromEntities;
            break;
        case domain::TableOrigin::Bridge:
        case domain::TableOrigin::Associative:
            lit[t] = showing_ == SchemaShowing::FromRelationships;
            break;
        case domain::TableOrigin::Multivalued:
            lit[t] = showing_ == SchemaShowing::FromAttributes;
            break;
        }
    }
    return lit;
}

void SchemaView::tidy() {
    // Tidying puts the lines back too: a line bent by hand around a table
    // that is about to be moved somewhere else is not a shape worth keeping.
    shaping_.reset();
    shaping_shape_.reset();
    dragging_at_.clear();
    carried_lines_.clear();
    resizing_to_.clear();
    const auto result = editor_.tidy_schema();
    if (arranged) arranged(result);
}

void SchemaView::align() {
    std::map<domain::ElementRef, domain::Point> squared;
    for (std::size_t i = 0; i < preview_.tables.size(); ++i) {
        if (!preview_.tables[i].origin) continue;
        squared.emplace(*preview_.tables[i].origin,
                        domain::Point{std::round(placed_[i].box.left() / grid_step) * grid_step,
                                      std::round(placed_[i].box.top() / grid_step) * grid_step});
    }
    const auto result = editor_.move_schema_tables(squared);
    if (arranged) arranged(result);
}

// The grid the router works over is the size of the widget, so a line's way
// round the tables has to be found again when that changes.
void SchemaView::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    // The window, the panel or the scroll area changing the view's size is
    // routed here. The arrangement growing the canvas is not: whatever asked
    // for the arrangement routes every line straight after it.
    if (!sizing_canvas_) reroute();
    place_naming_box();
}

// How tall a table's questions make it, and how wide each answer is. Worked
// out apart from the painting, because it decides where everything below the
// table goes, the pointer has to find the answers too, and an edge being
// pulled has to know how short the table may be made.
double SchemaView::footer_height(const domain::PreviewTable& table, double wide) const {
    if (names_only_ || table.decisions.empty()) return 0.0;
    auto asking_font = font();
    asking_font.setPointSizeF(std::max(7.0, font().pointSizeF() - 1.0));
    // Measured bold, because the answer in force is drawn bold and every chip
    // has to be wide enough for it. Sized for the widest it could be, the row
    // does not reshuffle itself as answers are given.
    asking_font.setBold(true);
    const QFontMetricsF asking(asking_font);
    auto tall = footer_pad;
    for (const auto& decision : table.decisions) {
        const auto answers = answers_to(decision);
        auto line = 1;
        auto used = 0.0;
        for (const auto& answer : answers) {
            const auto across = asking.horizontalAdvance(answer) + chip_padding * 2;
            if (used > 0 && used + across > wide - 18) { ++line; used = 0; }
            used += across + chip_gap;
        }
        tall += question_height + line * (chip_height + chip_gap) + footer_pad;
    }
    return tall;
}

// Packed into columns, shortest column first, so the arrangement stays even
// without anybody having to lay it out.
void SchemaView::arrange() {
    placed_.assign(preview_.tables.size(), {});
    std::array<double, 3> columns{16, 16, 16};
    auto asking_font = font();
    asking_font.setPointSizeF(std::max(7.0, font().pointSizeF() - 1.0));
    asking_font.setBold(true);
    const QFontMetricsF asking(asking_font);
    // How far apart the columns of the packing stand. Tables are no longer all
    // one width -- each takes what its own contents need -- so the step is the
    // widest of them, and the arrangement stays a regular grid rather than a
    // pile in which a wide table lands on its neighbour.
    //
    // Measured from the width each table would have had anyway, never from one
    // a hand gave it. This is the same rule the packing already follows for
    // heights and for exactly the same reason: a table pulled wider must not
    // push its neighbours across, or pulling one table about would move the
    // others, which is not what pulling one table means.
    double step = table_width;
    for (const auto& table : preview_.tables) step = std::max(step, natural_width(table));
    step += column_gap;
    for (std::size_t i = 0; i < preview_.tables.size(); ++i) {
        const auto& table = preview_.tables[i];
        const auto wide = width_of(table);
        const auto footer = footer_height(table, wide);
        const auto natural = header_height + heading_room()
                           + row_height * static_cast<double>(table.columns.size()) + footer;
        const auto height = height_of(table, natural);
        // The automatic arrangement is worked out for every table, whether or
        // not it has been moved, and a table placed by hand then simply sits
        // somewhere else instead. Leaving a moved table out of the packing
        // would change which column each later table fell into and how far
        // down it sat, so moving one table would move the others -- which is
        // not what moving one table means.
        const auto shortest = static_cast<std::size_t>(
            std::distance(columns.begin(), std::min_element(columns.begin(), columns.end())));
        auto at = QPointF(margin + static_cast<double>(shortest) * step, columns[shortest]);
        // The packing counts the height a table would have had anyway, not the
        // one a hand gave it, for that same reason: a table pulled taller
        // would otherwise push every table under it down the column, and
        // pulling one table about would move the others. A table pulled over
        // its neighbour is answered by moving either of them apart, exactly as
        // a table pulled wider is.
        columns[shortest] += natural + row_gap;
        if (const auto found = table.origin ? placed_by_hand(*table.origin) : std::nullopt) at = *found;
        placed_[i].box = QRectF(at.x(), at.y(), wide, height);
        // Room given to a table's height is shared out between its rows rather
        // than left lying under the last of them: a table with a gap at the
        // bottom reads as one that has lost something, and a taller row is
        // simply a roomier table.
        const auto deep = table.columns.empty()
            ? row_height
            : std::max(row_height, (height - header_height - heading_room() - footer)
                                       / static_cast<double>(table.columns.size()));
        placed_[i].rows.clear();
        placed_[i].cells.clear();
        placed_[i].asked.clear();
        placed_[i].chips.clear();
        auto measuring = font();
        measuring.setFamily("Menlo");
        measuring.setPointSizeF(font().pointSizeF() - 0.5);
        const QFontMetricsF typed(measuring);
        // How much room each column needs to write what it holds whole.
        // Worked out once for the table, because a column whose edge moved
        // from row to row could not be ruled off, and a rule that wandered
        // would be worse than no rule at all.
        const auto room = columns_of(table);
        const auto type_room = room.type;
        // How much of the row a table pulled narrow can still hold.
        //
        // The columns fold away from the right, one at a time: the
        // constraints go first, then the type. What is left at the narrowest
        // is the gutter with its keys and the names beside it, which is the
        // least a table can be and still be a table -- a name and whether it
        // is the key are the two things nothing else can stand in for.
        //
        // Nothing springs back. The width is wherever a hand left it, and
        // every column returns, in the reverse order, as it is widened again.
        const auto beside = wide - gutter_width;
        const bool shows_rules = !names_only_ && beside - type_room - room.rules >= name_floor;
        const bool shows_type = !names_only_ && (shows_rules || beside - type_room >= name_floor);
        // Where the rules between the columns fall: one for each column that
        // is still being shown beyond the names.
        placed_[i].dividers.clear();
        const auto right = at.x() + wide;
        if (shows_rules)
            placed_[i].dividers = {right - room.rules - type_room, right - room.rules};
        else if (shows_type)
            placed_[i].dividers = {right - type_room};
        for (std::size_t row = 0; row < table.columns.size(); ++row) {
            const QRectF where(at.x(), at.y() + header_height + heading_room()
                                   + deep * static_cast<double>(row),
                               wide, deep);
            placed_[i].rows.push_back(where);
            const auto& column = table.columns[row];
            Cell cell;
            const auto top = where.center().y() - ask_height / 2;
            // A row squeezed narrower than the columns fit keeps only its name
            // and gutter. Nothing springs back: the width is where a hand left
            // it, and the columns return when it is widened.
            if (!column.ignored && shows_type) {
                if (shows_rules)
                    cell.rules = QRectF(where.right() - room.rules, top, room.rules, ask_height);
                // The type and its length share one column, split where the
                // type's name ends so that pressing the name asks what type it
                // is and pressing the brackets asks how long. One column to
                // read, two things to press.
                if (column.origin || column.added) {
                    const auto type_right = shows_rules ? cell.rules.left() : where.right();
                    const QRectF whole(type_right - type_room, top, type_room, ask_height);
                    const auto label = typed_label(column);
                    const auto full = typed.horizontalAdvance(label);
                    const auto from = whole.left() + std::max(6.0, (type_room - full) / 2);
                    const auto measured = column.type != domain::LogicalType::Unset
                                       && domain::size_of(column.type) != domain::TypeSize::None;
                    const auto split = measured
                        ? std::min(whole.right(), from + typed.horizontalAdvance(type_name(column)))
                        : whole.right();
                    cell.type = QRectF(whole.left(), top, split - whole.left(), ask_height);
                    if (measured)
                        cell.size = QRectF(split, top, whole.right() - split, ask_height);
                }
            }
            placed_[i].cells.push_back(cell);
        }
        auto below = at.y() + header_height + heading_room()
                   + deep * static_cast<double>(table.columns.size()) + footer_pad;
        for (std::size_t d = 0; !names_only_ && d < table.decisions.size(); ++d) {
            placed_[i].asked.push_back(QRectF(at.x() + 9, below, wide - 18, question_height));
            below += question_height;
            auto left = at.x() + 9;
            const auto answers = answers_to(table.decisions[d]);
            for (std::size_t choice = 0; choice < static_cast<std::size_t>(answers.size()); ++choice) {
                const auto across = asking.horizontalAdvance(answers[static_cast<int>(choice)]) + chip_padding * 2;
                if (left > at.x() + 9 && left + across > at.x() + wide - 9) {
                    left = at.x() + 9;
                    below += chip_height + chip_gap;
                }
                placed_[i].chips.push_back(Chip{d, choice, QRectF(left, below, across, chip_height)});
                left += across + chip_gap;
            }
            below += chip_height + chip_gap + footer_pad;
        }
    }
    double tallest = 0;
    double widest = 0;
    for (const auto& one : placed_) {
        tallest = std::max(tallest, one.box.bottom());
        widest = std::max(widest, one.box.right());
    }
    // A corner or a loose end dragged past the tables counts towards the size
    // too, or the thing just put there would be off the edge of the schema and
    // unreachable. Only the places a hand chose are measured, never the route
    // worked out between them, because that route is worked out from the size
    // and measuring it here would have the two chasing each other.
    const auto reach = [&](QPointF at) {
        widest = std::max(widest, at.x());
        tallest = std::max(tallest, at.y());
    };
    const auto measure = [&](const Shape& shape) {
        for (const auto& corner : shape.route) reach(corner);
        if (shape.from && !shape.from->on_table) reach(shape.from->at);
        if (shape.to && !shape.to->on_table) reach(shape.to->at);
    };
    for (const auto& [key, line] : editor_.project().schema_layout.lines) {
        (void)key;
        measure(as_shape(line));
    }
    if (shaping_shape_) measure(shaping_shape_->second);
    // Every caller routes the lines as soon as this returns, so the resize
    // this may cause does not route them as well (see sizing_canvas_).
    const QScopedValueRollback<bool> sizing(sizing_canvas_, true);
    setMinimumSize(static_cast<int>(widest + 40), static_cast<int>(tallest + 30));
}

SchemaView::Shape SchemaView::as_shape(const domain::SchemaLine& line) {
    Shape shape;
    shape.route.reserve(line.route.size());
    for (const auto& at : line.route) shape.route.emplace_back(at.x, at.y);
    // The model keeps plain points; the painter wants Qt's. Converted here,
    // at the one boundary, rather than everywhere either is used.
    const auto brought = [](const std::optional<domain::SchemaEnd>& end) {
        return end ? std::optional<EndAnchor>{EndAnchor{end->on_table, QPointF(end->at.x, end->at.y)}}
                   : std::nullopt;
    };
    shape.from = brought(line.from);
    shape.to = brought(line.to);
    return shape;
}

SchemaView::Shape SchemaView::shape_of(const domain::LinkSource& link) const {
    // Being dragged wins over what the project holds: the hand is mid-sentence
    // and the model has not been told yet.
    if (shaping_shape_ && shaping_shape_->first == link) return shaping_shape_->second;
    const auto& lines = editor_.project().schema_layout.lines;
    const auto found = lines.find(domain::foreign_key_from(link));
    if (found == lines.end()) return {};
    // Carried whole by a drag, it is where the drag has taken it.
    if (!dragging_at_.empty()
        && std::find(carried_lines_.begin(), carried_lines_.end(), link) != carried_lines_.end())
        return moved_by(as_shape(found->second), carried_by_);
    return as_shape(found->second);
}

SchemaView::Shape SchemaView::moved_by(Shape shape, QPointF by) {
    for (auto& corner : shape.route) corner += by;
    if (shape.from && !shape.from->on_table) shape.from->at += by;
    if (shape.to && !shape.to->on_table) shape.to->at += by;
    return shape;
}

domain::SchemaLine SchemaView::as_line(const Shape& shape) {
    domain::SchemaLine line;
    line.route.reserve(shape.route.size());
    for (const auto& corner : shape.route) line.route.push_back(domain::Point{corner.x(), corner.y()});
    const auto taken = [](const std::optional<EndAnchor>& end) {
        return end ? std::optional<domain::SchemaEnd>{
                         domain::SchemaEnd{end->on_table, domain::Point{end->at.x(), end->at.y()}}}
                   : std::nullopt;
    };
    line.from = taken(shape.from);
    line.to = taken(shape.to);
    return line;
}

bool SchemaView::line_is_shaped(const domain::LinkSource& link) const {
    if (shaping_shape_ && shaping_shape_->first == link) return true;
    return editor_.project().schema_layout.lines.contains(domain::foreign_key_from(link));
}

std::optional<QPointF> SchemaView::placed_by_hand(const domain::ElementRef& table) const {
    if (const auto held = dragging_at_.find(table); held != dragging_at_.end()) return held->second;
    // An edge on the left or the top carries the table's corner with it while
    // it is pulled, so where the table stands is part of what is being held.
    if (const auto pulled = resizing_to_.find(table);
        pulled != resizing_to_.end() && moves_corner(pulled->second.pull))
        return pulled->second.box.topLeft();
    const auto& tables = editor_.project().schema_layout.tables;
    const auto found = tables.find(domain::relation_from(table));
    if (found == tables.end()) return std::nullopt;
    return QPointF(found->second.x, found->second.y);
}

std::vector<std::vector<QPointF>> SchemaView::line_shapes() const {
    std::vector<std::vector<QPointF>> shapes;
    shapes.reserve(routes_.size());
    for (const auto& routed : routes_) shapes.push_back(routed.corners);
    return shapes;
}

std::vector<std::vector<QRectF>> SchemaView::row_boxes() const {
    std::vector<std::vector<QRectF>> rows;
    rows.reserve(placed_.size());
    for (const auto& one : placed_) rows.push_back(one.rows);
    return rows;
}

std::vector<std::vector<SchemaView::Cell>> SchemaView::cell_boxes() const {
    std::vector<std::vector<Cell>> cells;
    cells.reserve(placed_.size());
    for (const auto& one : placed_) cells.push_back(one.cells);
    return cells;
}

std::vector<QRectF> SchemaView::table_boxes() const {
    std::vector<QRectF> boxes;
    boxes.reserve(placed_.size());
    for (const auto& one : placed_) boxes.push_back(one.box);
    return boxes;
}

std::optional<SchemaView::Answerable> SchemaView::type_cell_at(QPointF point) const {
    // The cells were measured where the rows were, so the pointer is tested
    // against what was actually drawn rather than against a guess at it.
    for (std::size_t table = 0; table < placed_.size(); ++table) {
        if (!placed_[table].box.contains(point)) continue;
        // The outermost pixels belong to the edge that pulls the table. Once
        // the constraints have folded away the type column reaches the border
        // itself, and a cell answers before the table it sits in, so without
        // this a narrowed table could never be widened again.
        if (!placed_[table].box.adjusted(resize_grip, resize_grip, -resize_grip, -resize_grip)
                 .contains(point))
            return std::nullopt;
        for (std::size_t row = 0; row < placed_[table].cells.size(); ++row) {
            const auto& cell = placed_[table].cells[row];
            if (!cell.size.isEmpty() && cell.size.contains(point))
                return Answerable{table, row, true};
            if (!cell.type.isEmpty() && cell.type.contains(point))
                return Answerable{table, row, false};
        }
        return std::nullopt;
    }
    return std::nullopt;
}

std::optional<SchemaView::Constrained> SchemaView::rules_at(QPointF point) const {
    for (std::size_t table = 0; table < placed_.size(); ++table) {
        if (!placed_[table].box.contains(point)) continue;
        // The outermost pixels belong to the edge that pulls the table. The
        // last mark's column runs right up to the border, which is what makes
        // the row read as a table; but a mark answers before the table it
        // sits in, so without this the right edge could never be taken hold
        // of at all.
        if (!placed_[table].box.adjusted(resize_grip, resize_grip, -resize_grip, -resize_grip)
                 .contains(point))
            return std::nullopt;
        for (std::size_t row = 0; row < placed_[table].cells.size(); ++row) {
            const auto& cell = placed_[table].cells[row];
            if (cell.rules.isEmpty()) continue;
            if (cell.rules.contains(point)) return Constrained{table, row};
        }
        return std::nullopt;
    }
    return std::nullopt;
}

QString SchemaView::question_of(const domain::OpenDecision& decision) const {
    const auto called = [&](const domain::ElementRef& ref) {
        return QString::fromStdString(domain::name(editor_.project(), ref));
    };
    switch (decision.kind) {
    case domain::DecisionKind::IsaStrategy:
        return "ISA mapping strategy:";
    case domain::DecisionKind::CompositeMode:
        return QString("%1 is composite. What it becomes:").arg(called(decision.about));
    case domain::DecisionKind::OneToOneKey:
        return QString("%1 is 1:1, so this key could sit on either table:").arg(called(decision.about));
    case domain::DecisionKind::BridgeKey:
        return QString("%1 has no defined identifier. Bridge primary key:").arg(called(decision.about));
    }
    return {};
}

QStringList SchemaView::answers_to(const domain::OpenDecision& decision) const {
    switch (decision.kind) {
    case domain::DecisionKind::IsaStrategy:
        return {"Table per subclass", "Single table", "Table per concrete"};
    case domain::DecisionKind::CompositeMode:
        return {"Parts", "Whole", "Both"};
    case domain::DecisionKind::OneToOneKey: {
        QStringList sides;
        for (const auto& target : decision.side_targets)
            sides << "On " + QString::fromStdString(domain::name(editor_.project(), target));
        return sides;
    }
    case domain::DecisionKind::BridgeKey:
        return {"Separate key (default)", "Use participant keys (composite PK)"};
    }
    return {};
}

std::optional<std::pair<std::size_t, SchemaView::Chip>> SchemaView::chip_at(QPointF point) const {
    for (std::size_t t = 0; t < placed_.size(); ++t)
        for (const auto& chip : placed_[t].chips)
            if (chip.box.contains(point)) return std::pair{t, chip};
    return std::nullopt;
}

QString SchemaView::size_text(const domain::PreviewColumn& column) {
    const auto takes = domain::size_of(column.type);
    if (takes == domain::TypeSize::None) return {};
    if (!column.length) return takes == domain::TypeSize::Precision ? "p,s" : "n";
    return takes == domain::TypeSize::Precision
        ? QString("%1,%2").arg(column.length).arg(column.scale)
        : QString::number(column.length);
}

QString SchemaView::provenance_of(const domain::PreviewTable& table) const {
    if (!table.origin || std::holds_alternative<domain::RelationId>(*table.origin)) return {};
    const auto called = [&](const domain::ElementRef& ref) {
        return QString::fromStdString(domain::name(editor_.project(), ref));
    };
    switch (table.origin_kind) {
    case domain::TableOrigin::Entity:
        return "entity " + called(*table.origin);
    case domain::TableOrigin::Subtype:
        return table.derives_from ? "ISA subtype of " + called(*table.derives_from)
                                  : QString("ISA subtype");
    case domain::TableOrigin::Bridge:
        return "bridge for M:M " + called(*table.origin);
    case domain::TableOrigin::Associative:
        return "associative " + called(*table.origin);
    case domain::TableOrigin::Multivalued:
        return "multivalued " + called(*table.origin);
    }
    return {};
}

QColor SchemaView::surface_for(const domain::PreviewTable& table) const {
    if (!theme_) return Qt::white;
    // A table wears the colour of the thing it came from, so the work of
    // colouring a diagram is not lost when the same project is read as tables.
    if (table.origin) {
        const auto chosen = editor_.project().colours.find(*table.origin);
        if (chosen != editor_.project().colours.end())
            return QColor(chosen->second.red, chosen->second.green, chosen->second.blue);
    }
    switch (table.origin_kind) {
    case domain::TableOrigin::Bridge: case domain::TableOrigin::Associative:
        return theme_->relationship_fill;
    case domain::TableOrigin::Multivalued: return theme_->attribute_fill;
    default: return theme_->entity_fill;
    }
}

QColor SchemaView::edge_for(const domain::PreviewTable& table) const {
    if (!theme_) return Qt::black;
    switch (table.origin_kind) {
    case domain::TableOrigin::Bridge: case domain::TableOrigin::Associative:
        return theme_->relationship_border;
    case domain::TableOrigin::Multivalued: return theme_->attribute_border;
    default: return theme_->entity_border;
    }
}

std::optional<std::size_t> SchemaView::table_at(QPointF point) const {
    for (std::size_t i = placed_.size(); i-- > 0;)
        if (placed_[i].box.contains(point)) return i;
    return std::nullopt;
}

std::optional<std::size_t> SchemaView::row_at(std::size_t table, QPointF point) const {
    if (table >= placed_.size()) return std::nullopt;
    for (std::size_t row = 0; row < placed_[table].rows.size(); ++row)
        if (placed_[table].rows[row].contains(point)) return row;
    return std::nullopt;
}

// Asking what can be done here is not shaping anything, so it goes nowhere
// near the drag handling: it finds the place under the pointer and hands it
// on. A line is not asked about, because a line draws a foreign key and a
// foreign key is not a thing to be added or taken away by itself -- it is
// there because the relationship that put it there is.
void SchemaView::contextMenuEvent(QContextMenuEvent* event) {
    const auto found = table_at(event->pos());
    // The empty schema, where it is drawn by hand, is where a table is added.
    if (!found && drawn_by_hand() && asked_nowhere) {
        asked_nowhere(QPointF(event->pos()), event->globalPos());
        event->accept();
        return;
    }
    if (!found || !asked) { QWidget::contextMenuEvent(event); return; }
    asked(Spot{*found, row_at(*found, event->pos()), event->globalPos()});
    event->accept();
}

void SchemaView::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) { QWidget::mousePressEvent(event); return; }
    // Taken here as well as by the focus policy, so a press always hands the
    // schema the keyboard, whatever the press came from.
    setFocus(Qt::MouseFocusReason);
    // With Table in hand, a press places a table where it lands, before
    // anything there answers, as the diagram's placing tools place wherever
    // they are pressed.
    placed_on_press_ = placing_;
    if (placing_) {
        if (drawn_by_hand() && add_table) add_table(event->position());
        event->accept();
        return;
    }
    // The plus answers before anything else on the table it belongs to, or it
    // could not be pressed: a press on a header takes hold of the table.
    if (hovered_table_ && add_slot(*hovered_table_).contains(event->position())) {
        if (add_column) add_column(*hovered_table_);
        event->accept();
        return;
    }
    // With Connect in hand, a row draws a foreign key wherever it is pressed,
    // before anything else on it answers: the tool says what the press is for.
    if (const auto from = connect_row_at(event->position())) {
        linking_ = Linking{from->first, from->second, event->position(), false};
        linking_to_ = event->position();
        select(preview_.tables[from->first].origin);
        setCursor(Qt::CrossCursor);
        return;
    }
    // A line answers before the table it crosses. An end lies on a table's
    // outline, and a run pushed behind a table is drawn over it while the
    // pointer is on it -- so if the table answered first, what is plainly on
    // top could not be taken hold of. A line is a few pixels wide and a table
    // is not, so little is lost the other way.
    if (auto line = line_at(event->position())) {
        shaping_ = *line;
        hovered_ = shaping_->link;
        // Pressing a line chooses the foreign key it stands for, and nothing
        // else (Stage 1): the tables put down, as a press on the diagram's
        // line puts down its shapes.
        if (const auto key = key_of(shaping_->link)) {
            select(std::nullopt);
            pick(*key);
        }
        // The cursor keeps saying which way the run goes while it is held,
        // rather than becoming a closed hand that says nothing about it.
        setCursor(run_cursor(event->position()).value_or(Qt::ClosedHandCursor));
        update();
        return;
    }
    // An answer offered inside a table answers before the table is picked up,
    // or it could be clicked only by dragging the table a little first.
    // The type cell answers before the table it sits in, or it could only be
    // reached by dragging the table a little first.
    // A constraint mark answers before the table it sits in, for the same
    // reason the type cell does: otherwise it could only be reached by
    // dragging the table a little first.
    if (const auto hit = rules_at(event->position())) {
        select(preview_.tables[hit->table].origin);
        if (const auto column = column_ref(hit->table, hit->row)) pick(*column);
        // Opened under the cell rather than under the pointer, so the list
        // stands below what it is about and does not cover it.
        if (rules_asked)
            rules_asked(*hit, mapToGlobal(placed_[hit->table].cells[hit->row]
                                              .rules.bottomLeft().toPoint()) + QPoint(0, 4));
        return;
    }
    if (const auto cell = type_cell_at(event->position())) {
        const auto& table = preview_.tables[cell->table];
        select(table.origin);
        if (const auto column = column_ref(cell->table, cell->row)) pick(*column);
        const auto& drawn = placed_[cell->table].cells[cell->row];
        const auto below = mapToGlobal((cell->size ? drawn.size : drawn.type).bottomLeft().toPoint())
                         + QPoint(0, 4);
        if (cell->size) {
            if (asked_size) asked_size(table.columns[cell->row], below);
        } else if (asked_type) {
            asked_type(table.columns[cell->row], below);
        }
        return;
    }
    if (const auto chip = chip_at(event->position())) {
        const auto& table = preview_.tables[chip->first];
        if (chip->second.decision < table.decisions.size()) {
            select(table.origin);
            if (decided) decided(table.decisions[chip->second.decision], chip->second.choice);
        }
        return;
    }
    // A table's edges and corners pull it about, and they answer before the
    // table itself does: they lie along the inside of its own box, so if the
    // table answered first an edge could never be taken hold of.
    if (const auto edge = edge_at(event->position())) {
        resizing_ = *edge;
        select(preview_.tables[edge->table].origin);
        setCursor(cursor_for(edge->pull));
        return;
    }
    // On a schema drawn by hand, a row's key gutter draws a foreign key from
    // that row rather than picking the table up (Zain, 2026-09-27). The rest
    // of the row, and the header, still move the table, and a press held
    // down still gathers it.
    if (const auto from = gutter_at(event->position());
        from && !(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
        linking_ = Linking{from->first, from->second, event->position(), false};
        linking_to_ = event->position();
        select(preview_.tables[from->first].origin);
        setCursor(Qt::CrossCursor);
        return;
    }
    if (const auto found = table_at(event->position())) {
        const auto& table = preview_.tables[*found];
        // Held down, a press adds the table to what is marked or takes it out
        // again, so several can be gathered one at a time. It does not pick
        // the table up: gathering and moving are different acts, and a hand
        // adding a fourth table to three should not drag it by mistake.
        if (table.origin && (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) {
            toggle_mark(*table.origin);
            return;
        }
        // Pressing inside a group that has already been gathered keeps the
        // group: it is about to be acted on, and taking it apart because it
        // was pressed would make a selection impossible to use.
        dragging_ = *found;
        setCursor(Qt::ClosedHandCursor);
        if (!is_marked(table)) select(table.origin);
        // A press on one of its rows chooses that column too, and on its
        // heading the table alone (Stage 1). Either way the table is taken
        // hold of as before.
        if (const auto row = row_at(*found, event->position())) {
            if (const auto column = column_ref(*found, *row)) pick(*column);
        } else {
            pick(std::nullopt);
        }
        // What the hand carries: the table pressed and, where that table is
        // one of several marked, every one of them, each from where it stands
        // now. A gathered group moves together and keeps its arrangement.
        carried_from_ = event->position();
        carried_.clear();
        for (std::size_t t = 0; t < preview_.tables.size() && t < placed_.size(); ++t) {
            const auto& one = preview_.tables[t];
            if (one.origin && (t == *found || is_marked(one)))
                carried_.emplace_back(*one.origin, placed_[t].box.topLeft());
        }
        carried_by_ = {};
        carried_lines_.clear();
        const auto is_carried = [&](std::size_t t) {
            return t < preview_.tables.size() && preview_.tables[t].origin
                && std::any_of(carried_.begin(), carried_.end(),
                               [&](const auto& one) { return one.first == *preview_.tables[t].origin; });
        };
        for (const auto& routed : routes_)
            if (routed.link && line_is_shaped(*routed.link) && is_carried(routed.from_table)
                && is_carried(routed.to_table))
                carried_lines_.push_back(*routed.link);
        return;
    }
    // Pressing the empty canvas asks about nothing, which puts the whole
    // schema back -- and holding it draws a band round whatever it is taken
    // across, which is the other way of gathering several.
    band_from_ = event->position();
    band_.reset();
    if (!(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier))) select(std::nullopt);
    QWidget::mousePressEvent(event);
}

void SchemaView::mouseMoveEvent(QMouseEvent* event) {
    // Which table is being looked at, so it can wear its plus. Not while
    // something is being dragged: a table under the pointer in the middle of a
    // gesture is not a table being considered.
    pointer_ = event->position();
    // A foreign key being drawn follows the pointer until it is let go. Below
    // a few pixels of travel it is still a click, and draws nothing.
    if (linking_) {
        linking_to_ = event->position();
        if (std::hypot(linking_to_.x() - linking_->press.x(), linking_to_.y() - linking_->press.y())
            > shaping_travel)
            linking_->travelled = true;
        update();
        return;
    }
    // A band drawn from the empty canvas, growing under the hand. Nothing is
    // marked while it grows: what it has caught is settled when it is let go,
    // so a band dragged across the schema and back again marks what it ends
    // on rather than everything it passed over.
    if ((event->buttons() & Qt::LeftButton) && !shaping_ && !dragging_ && !resizing_) {
        const auto travelled = std::hypot(event->position().x() - band_from_.x(),
                                          event->position().y() - band_from_.y());
        // Below a few pixels nothing is drawn, so a click on the canvas that
        // shifts a little is still a click rather than a band of two pixels.
        if (band_ || travelled > shaping_travel) {
            band_ = QRectF(band_from_, event->position()).normalized();
            update();
            return;
        }
    }
    if (!shaping_ && !(event->buttons() & Qt::LeftButton)) {
        auto over = table_at(event->position());
        // The slot is under the table and outside it, so being on the slot is
        // being on the table it belongs to. Asked of every table rather than
        // only the one last hovered, or a pointer arriving on the slot from
        // below would find nothing there.
        if (!over)
            for (std::size_t t = 0; t < placed_.size(); ++t)
                if (preview_.tables[t].origin && add_slot(t).contains(event->position())) { over = t; break; }
        if (over != hovered_table_) { hovered_table_ = over; update(); }
        if (over) update();   // the slot lights as the pointer crosses it
        // Which constraint mark is under the pointer, so it can wear its box
        // and say it is pressable. Compared field by field because the mark
        // is identified by where it is rather than by an identity of its own.
        const auto mark = rules_at(event->position());
        const auto same = (!mark && !hovered_constraint_)
            || (mark && hovered_constraint_ && mark->table == hovered_constraint_->table
                && mark->row == hovered_constraint_->row);
        if (!same) { hovered_constraint_ = mark; update(); }
        const auto half = type_cell_at(event->position());
        const auto unchanged = (!half && !hovered_type_)
            || (half && hovered_type_ && half->table == hovered_type_->table
                && half->row == hovered_type_->row && half->size == hovered_type_->size);
        if (!unchanged) { hovered_type_ = half; update(); }
    }
    if (shaping_) {
        const auto here = QPointF(std::max(0.0, event->position().x()),
                                  std::max(0.0, event->position().y()));
        if (shaping_->grip != Grip::Run) { drag_end(here); return; }
        const auto travelled = std::hypot(here.x() - shaping_->press.x(), here.y() - shaping_->press.y());
        // Below a few pixels of travel nothing moves, so a line can be clicked
        // without shifting under a hand that merely twitched.
        if (!shaping_->grabbed && travelled <= shaping_travel) return;
        drag_run(here);
        return;
    }
    if (resizing_) {
        const auto& table = preview_.tables[resizing_->table];
        if (!table.origin) return;
        // How short this table may be made is worked out from the width the
        // pull has reached, not the one it started at: a narrower table wraps
        // its answers onto more lines, and so needs more room underneath.
        const auto across = pulled_box(*resizing_, event->position(), 0).width();
        const auto box = pulled_box(*resizing_, event->position(), natural_height(table, across));
        resizing_to_[*table.origin] = Pulled{box, resizing_->pull};
        arrange();
        reroute();
        update();
        return;
    }
    if (!dragging_) {
        // Table in hand places wherever it is pressed, and the pointer says so
        // everywhere.
        if (placing_) {
            setCursor(Qt::CrossCursor);
            return;
        }
        // What the pointer says is what a press there would do, so this asks
        // in the same order the press does: the line first, then whatever a
        // table offers inside itself, then its edges, then the table.
        const auto line = line_at(event->position());
        const auto over = line ? std::optional<domain::LinkSource>{line->link} : std::nullopt;
        if (over != hovered_) { hovered_ = over; update(); }
        if (line) {
            setCursor(run_cursor(event->position()).value_or(Qt::OpenHandCursor));
            return;
        }
        if (chip_at(event->position()) || type_cell_at(event->position())
            || rules_at(event->position())) {
            setCursor(Qt::PointingHandCursor);
            return;
        }
        if (const auto edge = edge_at(event->position())) { setCursor(cursor_for(edge->pull)); return; }
        // A key gutter that draws a foreign key says so by the pointer.
        if (gutter_at(event->position()) || connect_row_at(event->position())) {
            setCursor(Qt::CrossCursor);
            return;
        }
        setCursor(table_at(event->position()) ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    const auto& table = preview_.tables[*dragging_];
    if (!table.origin) return;
    // The table follows the pointer exactly: nothing is snapped while it is
    // being moved, because a shape that jumps under the hand cannot be aimed.
    // Every table carried with it moves by the same amount. None is taken
    // past the top or the left of the schema, and the group stops there as a
    // whole when the first of them reaches it, rather than the rest piling
    // up against the edge and losing the arrangement they were moved in.
    auto moved = event->position() - carried_from_;
    for (const auto& [carried, from] : carried_) {
        moved.setX(std::max(moved.x(), -from.x()));
        moved.setY(std::max(moved.y(), -from.y()));
    }
    for (const auto& [carried, from] : carried_) dragging_at_[carried] = from + moved;
    carried_by_ = moved;
    arrange();
    reroute();
    update();
}

void SchemaView::mouseReleaseEvent(QMouseEvent* event) {
    // Table still in hand after placing one: the press did all there was to
    // do, and the pointer goes on saying where the next one goes.
    if (placing_) {
        setCursor(Qt::CrossCursor);
        event->accept();
        return;
    }
    // A foreign key drawn by hand, let go on a row: which row was taken to
    // which is handed on, and whether that makes a foreign key is the
    // Editor's to say. Let go anywhere else, it says where it should have
    // been let go, where the hand is.
    if (linking_) {
        const auto from = *linking_;
        linking_.reset();
        update();
        const auto here = event->position();
        const auto at = mapToGlobal(here.toPoint());
        if (from.travelled) {
            // The row it started on is the key referred to, and the table let
            // go on is the one referring to it -- the row there too, where the
            // hand let go on one, being the column chosen to hold the key.
            // Nothing is found for the hand: where it let go is what it meant,
            // found as it was found while the line was drawn (link_spot), the
            // strip under a table being the table itself.
            if (const auto spot = link_spot(here)) {
                if (linked) linked(Linked{from.table, from.row, spot->table, spot->row, at});
            } else if (warned && from.table < preview_.tables.size()
                       && from.row < preview_.tables[from.table].columns.size()) {
                const auto& start = preview_.tables[from.table];
                warned(start.columns[from.row].primary_key
                           ? tr("Let go on the table that refers to %1.%2, or on the column there that is to hold it.")
                                 .arg(QString::fromStdString(start.name),
                                      QString::fromStdString(start.columns[from.row].name))
                           : tr("A connection starts on a primary key: start on the key being referenced and let "
                                "go on the table that refers to it."),
                       at);
            }
        }
        setCursor(gutter_at(here) || connect_row_at(here) ? Qt::CrossCursor
                  : table_at(here) ? Qt::OpenHandCursor : Qt::ArrowCursor);
        return;
    }
    // What the band caught. A table counts as caught when the band touches it
    // at all rather than when it swallows it whole: a band drawn across a row
    // of tables is meant to take them, and asking for every edge to be inside
    // would make a wide table almost impossible to catch.
    if (band_) {
        const auto caught = *band_;
        band_.reset();
        const bool adding = (event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier)) != 0;
        if (!adding) selected_.clear();
        picked_.reset();
        for (std::size_t t = 0; t < placed_.size() && t < preview_.tables.size(); ++t) {
            const auto& table = preview_.tables[t];
            if (!table.origin || !placed_[t].box.intersects(caught)) continue;
            if (std::find(selected_.begin(), selected_.end(), *table.origin) == selected_.end())
                selected_.push_back(*table.origin);
        }
        update();
        if (chose) chose();
        return;
    }
    dragging_.reset();
    carried_.clear();
    resizing_.reset();
    // The route a push leaves behind may hold a corner that is no longer one --
    // a run slid back level with its neighbour, say. The drawn line has already
    // had those taken out of it, so the stored route is brought back into step
    // with it here, and the next grab counts runs the way the drawing does.
    if (shaping_ && shaping_->grabbed && shaping_->grip == Grip::Run && shaping_shape_) {
        const auto found = std::find_if(routes_.begin(), routes_.end(), [&](const Routed& routed) {
            return routed.link && *routed.link == shaping_->link;
        });
        if (found != routes_.end() && found->corners.size() >= 2)
            shaping_shape_->second.route = found->corners;
    }
    // Which end was being pulled, remembered before the edit is written: writing
    // it refreshes the view, and a refresh gives up whatever was being dragged.
    const auto pulled = shaping_ && (shaping_->grip == Grip::FromEnd || shaping_->grip == Grip::ToEnd)
        ? shaping_ : std::nullopt;
    // One edit for the whole drag, written when the hand lets go. Editing on
    // every frame would put a pixel of movement into the history each time.
    commit_arrangement();
    // And, once it is written, whatever is wrong with where the end was put.
    // Said on release rather than during the drag: a warning that flickered as
    // the pointer crossed each row would be noise rather than news.
    if (warned && pulled) {
        const auto found = std::find_if(routes_.begin(), routes_.end(), [&](const Routed& routed) {
            return routed.link && *routed.link == pulled->link;
        });
        if (found != routes_.end())
            warned(end_complaint(*found, pulled->grip),
                   mapToGlobal(event->position().toPoint()));
    }
    shaping_.reset();
    if (line_at(event->position())) {
        setCursor(run_cursor(event->position()).value_or(Qt::OpenHandCursor));
        return;
    }
    // Still on the edge it was just pulled by, so the pointer still says it
    // would pull it again rather than flickering back to a hand for a moment.
    if (const auto edge = edge_at(event->position())) { setCursor(cursor_for(edge->pull)); return; }
    setCursor(table_at(event->position()) ? Qt::OpenHandCursor : Qt::ArrowCursor);
}

// Push one run of a line sideways. The first push is also what settles the
// line's route: until then the router has been deciding it afresh every time,
// and a run cannot be moved out of a shape that is about to be worked out
// again. So the route the line has at that moment becomes the route it keeps,
// and everything after that is a change to it.
void SchemaView::drag_run(QPointF here) {
    if (!shaping_) return;
    if (!shaping_shape_ || shaping_shape_->first != shaping_->link)
        shaping_shape_ = {shaping_->link, shape_of(shaping_->link)};
    auto& shape = shaping_shape_->second;
    if (!shaping_->grabbed) {
        const auto found = std::find_if(routes_.begin(), routes_.end(), [&](const Routed& routed) {
            return routed.link && *routed.link == shaping_->link;
        });
        if (found == routes_.end() || found->corners.size() < 2) return;
        if (shape.route.size() < 2) shape.route = found->corners;
        shaping_->grabbed = true;
    }
    if (shaping_->index + 1 >= shape.route.size()) return;
    // The run follows the pointer exactly. Nothing is snapped while it is being
    // moved, for the same reason a table is not: a shape that jumps under the
    // hand cannot be aimed.
    slide_run(shape.route, shaping_->index, here);
    // Sliding a run against an end grows a corner there, which shifts every
    // run after it along by one; the hand is still holding the same run.
    if (shaping_->index == 0) shaping_->index = 1;
    arrange();
    reroute();
    update();
    if (shaped) shaped();
}

// What the hand has been moving, written into the project as one edit.
//
// Nothing is written while a drag is in progress: a table or a line follows
// the pointer through the view's own copy, and the model hears about it once,
// when it is let go. That is what makes one drag one step of undo instead of
// several hundred.
void SchemaView::commit_arrangement() {
    application::EditResult result;
    if (!resizing_to_.empty()) {
        const auto& layout = editor_.project().schema_layout;
        // A size the table would have had anyway is the same as never having
        // been pulled that way, so it is written as nothing at all rather than
        // as a figure that happens to match.
        const auto given = [](double reached, double standard) {
            return std::abs(reached - standard) < 0.5 ? 0.0 : reached;
        };
        // Only the axis that was actually pulled is written. Pulling the right
        // edge of a table that had been made taller must leave its height
        // alone, so the untouched axis carries forward whatever the project
        // already holds.
        const auto held = [](const std::map<domain::RelationId, double>& sizes,
                             const domain::ElementRef& table) {
            const auto found = sizes.find(domain::relation_from(table));
            return found == sizes.end() ? 0.0 : found->second;
        };
        std::map<domain::ElementRef, domain::SchemaTableBox> boxes;
        for (const auto& [table, pulled] : resizing_to_) {
            const auto across = pulls_left(pulled.pull) || pulls_right(pulled.pull);
            const auto down = pulls_top(pulled.pull) || pulls_bottom(pulled.pull);
            const auto found = std::find_if(preview_.tables.begin(), preview_.tables.end(),
                                            [&](const domain::PreviewTable& one) {
                                                return one.origin && *one.origin == table;
                                            });
            const auto natural = found == preview_.tables.end()
                ? pulled.box.height() : natural_height(*found, pulled.box.width());
            std::optional<domain::Point> at;
            if (moves_corner(pulled.pull))
                at = domain::Point{pulled.box.left(), pulled.box.top()};
            boxes.emplace(table, domain::SchemaTableBox{
                across ? given(pulled.box.width(), table_width) : held(layout.widths, table),
                down ? given(pulled.box.height(), natural) : held(layout.heights, table), at});
        }
        resizing_to_.clear();
        result = editor_.resize_schema_tables(boxes);
    } else if (!dragging_at_.empty()) {
        std::map<domain::ElementRef, domain::Point> places;
        for (const auto& [table, at] : dragging_at_) places.emplace(table, domain::Point{at.x(), at.y()});
        // The lines the drag carried whole are written where it took them.
        std::vector<std::pair<domain::LinkSource, domain::SchemaLine>> carried;
        for (const auto& link : carried_lines_) carried.emplace_back(link, as_line(shape_of(link)));
        // A line told to give way is given back in the same edit as the move
        // that displaced it, so one undo takes both back together rather than
        // leaving the line released and the table where it was.
        const auto give_way = lines_give_way_ ? lines_over_tables() : std::vector<domain::LinkSource>{};
        dragging_at_.clear();
        carried_lines_.clear();
        carried_by_ = {};
        result = editor_.move_schema_tables(places, give_way, carried);
    } else if (shaping_shape_) {
        domain::SchemaLine line;
        line.route.reserve(shaping_shape_->second.route.size());
        for (const auto& corner : shaping_shape_->second.route)
            line.route.push_back(domain::Point{corner.x(), corner.y()});
        const auto taken = [](const std::optional<EndAnchor>& end) {
            return end ? std::optional<domain::SchemaEnd>{
                             domain::SchemaEnd{end->on_table, domain::Point{end->at.x(), end->at.y()}}}
                       : std::nullopt;
        };
        line.from = taken(shaping_shape_->second.from);
        line.to = taken(shaping_shape_->second.to);
        const auto link = shaping_shape_->first;
        shaping_shape_.reset();
        result = editor_.shape_schema_line(link, std::move(line));
    } else {
        return;
    }
    if (arranged) arranged(result);
}

// Where an end goes as it is dragged. Over its own table it sits on the
// outline and slides along whichever edge is nearest, so a join can be put
// anywhere around a table. Off the table it stays exactly where the hand left
// it: an end that sprang back to the edge could not be aimed, and a connection
// left hanging is something the schema should say out loud rather than quietly
// undo. The foreign key it draws is untouched either way -- this is where the
// line is drawn, not what the model holds.
// What is wrong with where an end has been put.
//
// A line on the schema is not a line between two tables; it is a line between
// two rows -- the foreign key it draws and the key that key points at. That is
// why the ends meet the rows themselves and not the table's edge. An end put
// somewhere else is still put there, because a hand's placement is never
// undone, but the line is then saying something the schema does not: that these
// two rows are joined, drawn as though some other pair were.
//
// So the answer is a warning that names both the row the end belongs to and the
// row it was left on, and says why that matters. Silence would leave a drawing
// that reads as a different schema from the one it is of.
QString SchemaView::end_complaint(const Routed& routed, Grip which) const {
    if (!routed.link) return {};
    const bool from_end = which == Grip::FromEnd;
    const auto table = from_end ? routed.from_table : routed.to_table;
    const auto belongs = from_end ? routed.from_column : routed.to_column;
    if (table >= placed_.size() || table >= preview_.tables.size()) return {};
    const auto& rows = placed_[table].rows;
    const auto& columns = preview_.tables[table].columns;
    if (belongs >= rows.size() || belongs >= columns.size()) return {};
    const auto shape = shape_of(*routed.link);
    const auto& anchor = from_end ? shape.from : shape.to;
    if (!anchor) return {};   // the router still has this end: nothing was put anywhere

    const auto in_table = QString::fromStdString(preview_.tables[table].name);
    const auto wanted = QString::fromStdString(columns[belongs].name);
    // The key at the other end, which is what makes the pair worth naming.
    const auto other = from_end ? routed.to_table : routed.from_table;
    const auto other_column = from_end ? routed.to_column : routed.from_column;
    QString joined;
    if (other < preview_.tables.size() && other_column < preview_.tables[other].columns.size())
        joined = QString::fromStdString(preview_.tables[other].columns[other_column].name)
               + " in " + QString::fromStdString(preview_.tables[other].name);

    if (!anchor->on_table) {
        // Said from this end's own side: the foreign key points, the key it
        // points at is pointed at, and saying it the other way round reads as
        // though the schema ran backwards.
        QString pair;
        if (!joined.isEmpty())
            pair = from_end ? tr(", which points at %1").arg(joined)
                            : tr(", which %1 points at").arg(joined);
        return tr("This end has been left off %1. It belongs on %2%3, so that is the row it "
                  "joins; where it is now it joins nothing, and the line no longer shows which "
                  "rows the key runs between.").arg(in_table, wanted, pair);
    }

    const auto at = point_in(placed_[table].box, anchor->at);
    for (std::size_t row = 0; row < rows.size(); ++row) {
        if (!rows[row].contains(QPointF(rows[row].center().x(), at.y()))) continue;
        if (row == belongs) return {};
        const auto& landed = columns[row];
        const auto on = QString::fromStdString(landed.name);
        // Why this particular row is the wrong one to have landed on, said in
        // the terms the schema itself uses rather than as a rule number.
        //
        // The two ends are wrong in different ways and are told apart here. A
        // line runs from a foreign key to the key it points at, so the near
        // end has to be on a column that does the pointing and the far end on
        // the column pointed at. A primary key is a perfectly good column to
        // land on and a very bad one to start from, which is why the same row
        // earns a different answer at each end.
        QString because;
        if (landed.ignored)
            because = tr("%1 is not a column at all: the conversion left it out, so nothing can "
                         "point at it and nothing can run from it.").arg(on);
        else if (from_end) {
            if (landed.foreign_key)
                because = tr("%1 is a foreign key of its own, running somewhere else.").arg(on);
            else if (landed.primary_key)
                because = tr("%1 is a key this table is identified by, not a key that points "
                             "anywhere. A line starts at the foreign key that does the "
                             "pointing, and here that is %2.").arg(on, wanted);
            else
                because = tr("%1 holds no key, so no line runs from it.").arg(on);
        } else {
            if (landed.primary_key)
                because = tr("%1 is a key of this table, but not the one %2 points at.").arg(on, wanted);
            else if (landed.foreign_key)
                because = tr("%1 is a foreign key of its own, running somewhere else. A foreign "
                             "key points at what identifies a row, not at another pointer.").arg(on);
            else
                because = tr("%1 is an ordinary column. A key can only run to a key.").arg(on);
        }
        return tr("This end has been left on %1, and it belongs on %2. %3")
            .arg(on, wanted, because);
    }
    return tr("This end has been left beside no row of %1. It belongs on %2.").arg(in_table, wanted);
}

void SchemaView::drag_end(QPointF here) {
    if (!shaping_) return;
    const auto found = std::find_if(routes_.begin(), routes_.end(), [&](const Routed& routed) {
        return routed.link && *routed.link == shaping_->link;
    });
    if (found == routes_.end()) return;
    const auto table = shaping_->grip == Grip::FromEnd ? found->from_table : found->to_table;
    if (table >= placed_.size()) return;
    const auto& box = placed_[table].box;

    EndAnchor anchor;
    anchor.on_table = box.contains(here);
    anchor.at = anchor.on_table ? fraction_in(box, on_outline(box, here)) : here;
    if (!shaping_shape_ || shaping_shape_->first != shaping_->link)
        shaping_shape_ = {shaping_->link, shape_of(shaping_->link)};
    auto& shape = shaping_shape_->second;
    (shaping_->grip == Grip::FromEnd ? shape.from : shape.to) = anchor;
    // Moving an end moves the whole line, the way a rope follows the end that
    // is pulled rather than holding its shape and stretching at one corner. So
    // whatever route the line had been keeping is let go, and the router finds
    // it again from wherever the end now is, for every step of the drag.
    shape.route.clear();
    arrange();
    reroute();
    update();
    if (shaped) shaped();
}

// A line stops being the one under the pointer when the pointer is no longer
// over the schema at all, or it would be left lit with nothing pointing at it.
void SchemaView::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
    if (hovered_table_) { hovered_table_.reset(); update(); }
    if (hovered_constraint_) { hovered_constraint_.reset(); update(); }
    if (hovered_type_) { hovered_type_.reset(); update(); }
    if (shaping_ || !hovered_) return;
    hovered_.reset();
    update();
}

// A line half drawn is let go of when the schema is put away under it, as
// Escape lets go of it, so neither it nor where it was pointing is waiting
// when the schema comes back.
void SchemaView::hideEvent(QHideEvent* event) {
    QWidget::hideEvent(event);
    if (linking_) {
        linking_.reset();
        update();
    }
}

// A line is given back to the router by double-clicking it, which is the way
// back from a shape that turned out worse than the one it replaced. One line at
// a time, where Tidy gives back every line and every table at once.
void SchemaView::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    // The second half of a press that placed a table places nothing more and
    // opens nothing.
    if (placed_on_press_) {
        placed_on_press_ = false;
        event->accept();
        return;
    }
    // A line behind a table is still the line, for the same reason it can be
    // taken hold of there: it has to be givable back, or it is stuck.
    const auto line = line_at(event->position());
    if (!line || !line_is_shaped(line->link)) {
        // Nothing shaped under the pointer, so this is a name being opened for
        // typing: the table's when the header was struck, the column's when a
        // row was.
        if (const auto table = table_at(event->position())) {
            begin_rename(*table, row_at(*table, event->position()));
            event->accept();
            return;
        }
        // The empty schema, where it is drawn by hand, makes a table there.
        if (drawn_by_hand() && add_table) {
            add_table(event->position());
            event->accept();
            return;
        }
        QWidget::mouseDoubleClickEvent(event);
        return;
    }
    shaping_.reset();
    shaping_shape_.reset();
    const auto result = editor_.shape_schema_line(line->link, {});
    if (arranged) arranged(result);
}

// Where a name is written. A table's is in its header, left of whatever badge
// says where the table came from; a column's runs from the key gutter to
// wherever its type cell begins, which is the room the name is drawn in.
// The empty slot under a table, where another row would go.
//
// It sits under the table rather than in its header because that is where the
// row it makes will appear, and a thing is added where it will be. It shows
// only on the table being pointed at: a schema is read far more often than it
// is added to, and a slot standing under every table all the time would be
// furniture rather than an offer. Drawn outside the table's own box, so no
// table changes height for it and nothing on the schema moves.
QRectF SchemaView::add_slot(std::size_t table) const {
    if (table >= placed_.size()) return {};
    const auto& box = placed_[table].box;
    return QRectF(box.left(), box.bottom() + 2, box.width(), 19);
}

QRectF SchemaView::name_cell(std::size_t table, std::optional<std::size_t> column) const {
    if (table >= placed_.size()) return {};
    const auto& box = placed_[table].box;
    if (!column) return QRectF(box.left() + 6, box.top() + 3, std::max(24.0, box.width() - 70), header_height - 6);
    if (*column >= placed_[table].rows.size()) return {};
    const auto& row = placed_[table].rows[*column];
    const auto& cell = placed_[table].cells[*column];
    const auto right = cell.type.isEmpty() ? row.right() - 7 : cell.type.left() - 4;
    return QRectF(row.left() + gutter_width + 4, row.top() + 2,
                  std::max(24.0, right - row.left() - gutter_width - 4), row.height() - 4);
}

void SchemaView::place_naming_box() {
    if (!naming_what_ || !naming_) return;
    const auto area = name_cell(naming_what_->table, naming_what_->column);
    if (area.isNull()) { cancel_rename(); return; }
    naming_->setGeometry(area.toRect());
}

void SchemaView::begin_rename(std::size_t table, std::optional<std::size_t> column) {
    if (table >= preview_.tables.size()) return;
    commit_rename();
    const auto& subject = preview_.tables[table];
    if (column && *column >= subject.columns.size()) return;
    if (!naming_) {
        naming_ = new QLineEdit(this);
        naming_->setObjectName("schemaName");
        naming_->setFrame(false);
        naming_->installEventFilter(this);
        // editingFinished covers both Return and losing focus; Escape is
        // handled by the filter and clears the target before it fires.
        connect(naming_, &QLineEdit::editingFinished, this, [this] { commit_rename(); });
    }
    naming_what_ = Spot{table, column, {}};
    naming_->setText(QString::fromStdString(column ? subject.columns[*column].name : subject.name));
    place_naming_box();
    naming_->show();
    naming_->selectAll();
    naming_->setFocus(Qt::MouseFocusReason);
}

void SchemaView::open_column_for(domain::AttributeId attribute) {
    for (std::size_t t = 0; t < preview_.tables.size(); ++t) {
        const auto& columns = preview_.tables[t].columns;
        for (std::size_t c = 0; c < columns.size(); ++c)
            if (columns[c].origin == attribute) { begin_rename(t, c); return; }
    }
}

void SchemaView::open_column_for(domain::SchemaColumnId column) {
    for (std::size_t t = 0; t < preview_.tables.size(); ++t) {
        const auto& columns = preview_.tables[t].columns;
        for (std::size_t c = 0; c < columns.size(); ++c)
            if (columns[c].added == column) { begin_rename(t, c); return; }
    }
}

void SchemaView::open_table_for(const domain::ElementRef& table) {
    for (std::size_t t = 0; t < preview_.tables.size(); ++t)
        if (preview_.tables[t].origin == table) { begin_rename(t, std::nullopt); return; }
}

void SchemaView::commit_rename() {
    if (!naming_what_ || !naming_) return;
    const auto what = *naming_what_;
    const auto typed = naming_->text();
    naming_what_.reset();
    const auto* keyboard = window() ? window()->focusWidget() : nullptr;
    const bool holding = keyboard == naming_;
    naming_->hide();
    if (holding) setFocus(Qt::OtherFocusReason);
    if (what.table >= preview_.tables.size()) return;
    const auto& subject = preview_.tables[what.table];
    if (what.column && *what.column >= subject.columns.size()) return;
    const auto was = QString::fromStdString(what.column ? subject.columns[*what.column].name : subject.name);
    if (typed == was) return;
    if (renamed) renamed(what, typed);
}

void SchemaView::cancel_rename() {
    if (!naming_) return;
    naming_what_.reset();
    naming_->hide();
}

// What is worth saying about the thing under the pointer.
//
// The case this exists for is a key the conversion invented. Generating one is
// right -- a table with nothing to identify it still needs a key -- but doing
// it silently leaves the user with a name they did not choose and no sign that
// choosing another is allowed. The others say which way an edit travels, since
// that is the thing about the schema a reader cannot see.
QString SchemaView::hint_for(std::size_t table, std::optional<std::size_t> column) const {
    if (table >= preview_.tables.size()) return {};
    const auto& subject = preview_.tables[table];
    if (!column) {
        if (subject.origin && std::holds_alternative<domain::RelationId>(*subject.origin))
            return tr("Double-click to rename this table.");
        return subject.origin ? tr("Double-click to rename this table. It renames what it came from "
                                   "on the diagram, so both say the same thing.")
                              : QString{};
    }
    if (*column >= subject.columns.size()) return {};
    const auto& one = subject.columns[*column];
    // A schema drawn by hand has no diagram for a column to be on or off.
    if (drawn_by_hand()) {
        if (one.foreign_key && one.references && *one.references < preview_.tables.size()) {
            const auto& target = preview_.tables[*one.references];
            const auto key = one.references_column < target.columns.size()
                ? QString::fromStdString(target.columns[one.references_column].name) : QString{};
            return tr("A foreign key to %1.%2. Double-click to rename it.")
                .arg(QString::fromStdString(target.name), key);
        }
        if (one.primary_key)
            return tr("Double-click to rename this column. Drag from its key gutter onto the table that refers "
                      "to it to give that table a foreign key.");
        return tr("Double-click to rename this column. To make it a foreign key, drag from the key it is to "
                  "reference and let go on this row.");
    }
    switch (one.origin_kind) {
    case domain::ColumnOrigin::Generated:
        return one.primary_key
            ? tr("This is the primary key. Nothing on the diagram identifies this table, so one was "
                 "made for it. Double-click to give it another name; every foreign key pointing here "
                 "follows.")
            : tr("The conversion made this column.");
    case domain::ColumnOrigin::Attribute:
        return tr("Double-click to rename this column. It renames the attribute on the diagram.");
    case domain::ColumnOrigin::SchemaOnly:
        return tr("This column is in Relational Design and not on the diagram. Double-click to rename it.");
    case domain::ColumnOrigin::ForeignKey:
        return tr("A foreign key. It is named for the key it points at, and follows when that is renamed, "
                  "until it is given a name of its own: double-click to rename it.");
    case domain::ColumnOrigin::Discriminator:
        return tr("The conversion made this column, to say which kind of row this is.");
    }
    return {};
}

bool SchemaView::event(QEvent* happening) {
    if (happening->type() == QEvent::ToolTip) {
        const auto* asking = static_cast<QHelpEvent*>(happening);
        const auto where = QPointF(asking->pos());
        QString words;
        if (const auto table = table_at(where)) words = hint_for(*table, row_at(*table, where));
        if (const auto gutter = gutter_at(where))
            words = preview_.tables[gutter->first].columns[gutter->second].primary_key
                ? tr("Drag from here onto the table that refers to this key, or onto the column there that is "
                     "to hold it, to give that table a foreign key to it.")
                : tr("A connection starts on a primary key: drag from a key's gutter onto the table that refers "
                     "to it.");
        if (words.isEmpty()) QToolTip::hideText();
        else QToolTip::showText(asking->globalPos(), words, this);
        happening->accept();
        return true;
    }
    return QWidget::event(happening);
}

bool SchemaView::eventFilter(QObject* watched, QEvent* happening) {
    if (naming_ && watched == static_cast<QObject*>(naming_)
        && happening->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(happening)->key() == Qt::Key_Escape) {
        // Discard the pending text before the field can report it as finished.
        cancel_rename();
        setFocus();
        return true;
    }
    return QWidget::eventFilter(watched, happening);
}

// Where every line goes. Each one leaves whichever side of its table faces the
// table it is going to, which removes most crossings before any routing is
// done; the rest is found around the tables rather than through them.
//
// A line somebody has bent by hand is not routed at all: its corners say
// everything about its shape, and the router is not entitled to argue.
void SchemaView::reroute() {
    routes_.clear();
    if (!theme_ || placed_.empty()) return;
    ++routings_;
    const auto dark = theme_->canvas.lightnessF() < 0.5;

    Grid grid;
    grid.columns = std::max(1, width() / cell + 2);
    grid.rows = std::max(1, height() / cell + 2);
    grid.blocked.assign(static_cast<std::size_t>(grid.columns) * static_cast<std::size_t>(grid.rows), 0);
    grid.used.assign(grid.blocked.size(), 0);
    for (const auto& one : placed_) {
        const auto box = one.box.adjusted(-clearance, -clearance, clearance, clearance);
        for (int y = std::max(0, static_cast<int>(box.top()) / cell);
             y < std::min(grid.rows, static_cast<int>(box.bottom()) / cell + 1); ++y)
            for (int x = std::max(0, static_cast<int>(box.left()) / cell);
                 x < std::min(grid.columns, static_cast<int>(box.right()) / cell + 1); ++x)
                grid.blocked[static_cast<std::size_t>(grid.at(x, y))] = 1;
    }

    // Short links first, so the near ones get the tidy paths and the long ones
    // bend around them rather than the other way about.
    struct Pending { std::size_t table; std::size_t column; double reach; };
    std::vector<Pending> pending;
    for (std::size_t t = 0; t < preview_.tables.size(); ++t)
        for (std::size_t c = 0; c < preview_.tables[t].columns.size(); ++c) {
            const auto& column = preview_.tables[t].columns[c];
            if (!column.references || *column.references >= placed_.size()) continue;
            if (column.references_column >= placed_[*column.references].rows.size()) continue;
            const auto here = placed_[t].box.center();
            const auto there = placed_[*column.references].box.center();
            pending.push_back({t, c, std::abs(here.x() - there.x()) + std::abs(here.y() - there.y())});
        }
    std::sort(pending.begin(), pending.end(),
              [](const Pending& a, const Pending& b) { return a.reach < b.reach; });

    // How many lines already leave each side of each table. Two lines leaving
    // the same side turn at different distances out, so their long runs lie
    // beside one another instead of on top of one another. It comes round after
    // a few, because past that the turn would be inside the next table along
    // and the router would have nowhere to start from.
    std::map<std::pair<std::size_t, int>, int> taken;
    const auto side_of = [](QPointF step) {
        return step.x() > 0 ? 0 : step.x() < 0 ? 1 : step.y() > 0 ? 2 : 3;
    };
    // How many lines already land on each row. Several foreign keys pointing at
    // one primary key is the ordinary case, and left alone they all arrive at
    // the very same point with their end symbols one on top of another. They
    // are fanned across the row instead, which is as much room as there is.
    std::map<std::pair<std::size_t, std::size_t>, int> arriving;
    // Measured against the row they land on rather than the standard one: a
    // table pulled taller has deeper rows, and the fan is as wide as the row
    // it is spread across.
    const auto fanned = [](int slot, double deep) {
        const auto away = ((slot + 1) / 2) * 5.0;
        return std::clamp(slot % 2 ? -away : away, -deep / 2 + 4, deep / 2 - 4);
    };

    std::size_t index = 0;
    for (const auto& one : pending) {
        const auto& column = preview_.tables[one.table].columns[one.column];
        const auto target = *column.references;
        const auto& from_box = placed_[one.table].box;
        const auto& to_box = placed_[target].box;
        const auto from_row = placed_[one.table].rows[one.column];
        const auto to_row = placed_[target].rows[column.references_column];

        // Which side each end leaves by. Always the left or the right, so both
        // ends meet their own rows and the line joins the exact columns it is
        // about (Zain, 2026-09-19 and 2026-09-24). Tables side by side face
        // each other. Tables stacked one above the other have no facing sides,
        // so both ends leave by the same side -- whichever the two tables'
        // edges come nearer to lining up on, so the run between them is short
        // -- rather than landing on the top of one and the foot of the other,
        // where the reader could not tell which column was meant.
        QPointF from;
        QPointF to;
        QPointF from_step;
        QPointF to_step;
        if (to_box.left() > from_box.right() + 8) {
            from = QPointF(from_box.right(), from_row.center().y());
            from_step = QPointF(1, 0);
            to = QPointF(to_box.left(), to_row.center().y());
            to_step = QPointF(-1, 0);
        } else if (to_box.right() + 8 < from_box.left()) {
            from = QPointF(from_box.left(), from_row.center().y());
            from_step = QPointF(-1, 0);
            to = QPointF(to_box.right(), to_row.center().y());
            to_step = QPointF(1, 0);
        } else if (std::abs(from_box.left() - to_box.left()) <= std::abs(from_box.right() - to_box.right())) {
            from = QPointF(from_box.left(), from_row.center().y());
            from_step = QPointF(-1, 0);
            to = QPointF(to_box.left(), to_row.center().y());
            to_step = QPointF(-1, 0);
        } else {
            from = QPointF(from_box.right(), from_row.center().y());
            from_step = QPointF(1, 0);
            to = QPointF(to_box.right(), to_row.center().y());
            to_step = QPointF(1, 0);
        }
        // A key pointing back into its own table leaves and returns on the same
        // side, so it is given a small loop rather than a line of no length.
        const bool itself = one.table == target;
        if (itself) {
            from = QPointF(from_box.right(), from_row.center().y());
            from_step = QPointF(1, 0);
            to = QPointF(to_box.right(), to_row.center().y());
            to_step = QPointF(1, 0);
        }

        // Compare row-exact side pairs using the same obstacles and occupied
        // lanes as the router. Internally "from" is the FK end. A small right
        // penalty makes its left side win when the routes are equally clean.
        // Manual routes and endpoints bypass this choice entirely.
        const auto saved_shape = column.link ? shape_of(*column.link) : Shape{};
        if (!itself && !saved_shape.from && !saved_shape.to && saved_shape.route.empty()) {
            double best_score = std::numeric_limits<double>::max();
            for (const int fk_side : {-1, 1}) {
                for (const int pk_side : {-1, 1}) {
                    const QPointF candidate_from(fk_side < 0 ? from_box.left() : from_box.right(),
                                                 from_row.center().y());
                    const QPointF candidate_to(pk_side < 0 ? to_box.left() : to_box.right(),
                                               to_row.center().y());
                    const QPointF fk_step(fk_side, 0), pk_step(pk_side, 0);
                    const auto out_lane = taken[{one.table, side_of(fk_step)}] % lanes_across;
                    const auto in_lane = taken[{target, side_of(pk_step)}] % lanes_across;
                    const auto start_at = candidate_from + fk_step * (minimum_at + 6 + out_lane * lane_step);
                    const auto finish_at = candidate_to + pk_step * (minimum_at + 6 + in_lane * lane_step);
                    // The short end runs must not pass through another table.
                    const auto clear_end = [&](QPointF end, QPointF outside, std::size_t owner) {
                        const QRectF run = QRectF(end, outside).normalized().adjusted(-1, -1, 1, 1);
                        for (std::size_t t = 0; t < placed_.size(); ++t)
                            if (t != owner && run.intersects(placed_[t].box)) return false;
                        return true;
                    };
                    if (!clear_end(candidate_from, start_at, one.table)
                        || !clear_end(candidate_to, finish_at, target)) continue;
                    // find_way opens its terminal cells; probes must not leave
                    // holes in the obstacle map for subsequent alternatives.
                    const auto blocked = grid.blocked;
                    const auto path = find_way(grid,
                        QPoint(static_cast<int>(start_at.x()) / cell, static_cast<int>(start_at.y()) / cell),
                        QPoint(static_cast<int>(finish_at.x()) / cell, static_cast<int>(finish_at.y()) / cell));
                    grid.blocked = blocked;
                    if (path.empty()) continue;
                    double score = static_cast<double>(path.size()) + (fk_side > 0 ? 2.0 : 0.0);
                    for (std::size_t i = 0; i < path.size(); ++i) {
                        score += grid.used[static_cast<std::size_t>(grid.at(path[i].x(), path[i].y()))] * share_cost;
                        if (i > 1 && path[i] - path[i - 1] != path[i - 1] - path[i - 2])
                            score += turn_cost;
                    }
                    if (score >= best_score) continue;
                    best_score = score;
                    from = candidate_from;
                    to = candidate_to;
                    from_step = fk_step;
                    to_step = pk_step;
                }
            }
        }

        // An end put by hand overrules all of that. Where it was left on its
        // table it sits on the outline and the line leaves square out of that
        // edge; where it was taken off the table it stays exactly where it was
        // let go, and the line runs out to meet it.
        const auto placed_end = [](const std::optional<EndAnchor>& anchor, const QRectF& box,
                                   QPointF& at, QPointF& step) {
            if (!anchor) return false;
            at = anchor->on_table ? point_in(box, anchor->at) : anchor->at;
            step = step_away(box, at, anchor->on_table);
            return true;
        };
        const auto said = column.link ? shape_of(*column.link) : Shape{};
        const auto* shape = column.link && line_is_shaped(*column.link) ? &said : nullptr;
        const bool from_by_hand = shape && placed_end(shape->from, from_box, from, from_step);
        const bool to_by_hand = shape && placed_end(shape->to, to_box, to, to_step);

        // The ends stand off before the routing begins, so a line never starts
        // inside the symbol drawn at its end, and each one stands off in its
        // own lane so two lines leaving together do not run as one.
        if (!to_by_hand)
            to += QPointF(-to_step.y(), to_step.x())
                * fanned(arriving[{target, column.references_column}]++, to_row.height());
        const auto lane_out = from_by_hand ? 0 : taken[{one.table, side_of(from_step)}]++ % lanes_across;
        const auto lane_in = to_by_hand ? 0 : taken[{target, side_of(to_step)}]++ % lanes_across;
        // An end off its table stands off from nothing, and must not: standing
        // off would put the router's target beyond the place the end was left,
        // so the line would run past it and come back -- a spur again, this
        // time hanging in open space.
        const bool from_adrift = shape && shape->from && !shape->from->on_table;
        const bool to_adrift = shape && shape->to && !shape->to->on_table;
        const QPointF start = from
            + from_step * (from_adrift ? 0.0 : minimum_at + 6 + lane_out * lane_step);
        const QPointF finish = to
            + to_step * (to_adrift ? 0.0 : minimum_at + 6 + lane_in * lane_step);

        Routed routed;
        routed.link = column.link;
        routed.colour = colourless(theme_->id) ? link_grey(index++, dark) : link_colour(index++, dark);
        routed.from = from;
        routed.to = to;
        routed.from_loose = from_adrift;
        routed.to_loose = to_adrift;
        routed.from_step = from_step;
        routed.to_step = to_step;
        routed.from_column = one.column;
        routed.to_column = column.references_column;
        routed.from_table = one.table;
        routed.to_table = target;
        routed.many = !column.one_to_one;
        routed.optional = column.optional_link;

        // Corners put in by hand win outright: the line goes where it was put,
        // and the router is not consulted about a shape somebody has decided.
        if (shape && shape->route.size() >= 2) {
            // The route is taken as it stands. Only its two ends are put where
            // the tables now are, and the run beside each end stretches to keep
            // up, so moving a table drags the line along by its ends instead of
            // tearing it off them.
            routed.corners = shape->route;
            const auto follow = [&](std::size_t end, std::size_t beside, QPointF at) {
                if (std::abs(routed.corners[end].x() - routed.corners[beside].x()) < 0.01)
                    routed.corners[beside].setX(at.x());
                else
                    routed.corners[beside].setY(at.y());
                routed.corners[end] = at;
            };
            follow(0, 1, from);
            follow(routed.corners.size() - 1, routed.corners.size() - 2, to);
            tighten(routed.corners);
        } else {
            const auto cells = itself || routing_ == SchemaRouting::Straight
                ? std::vector<QPoint>{}
                : find_way(grid, QPoint(static_cast<int>(start.x()) / cell, static_cast<int>(start.y()) / cell),
                                 QPoint(static_cast<int>(finish.x()) / cell, static_cast<int>(finish.y()) / cell));
            if (cells.size() > 2) {
                routed.corners = {from};
                const auto middle = corners_of(cells, start, finish);
                routed.corners.insert(routed.corners.end(), middle.begin(), middle.end());
                routed.corners.push_back(to);
                tighten(routed.corners);
            } else if (itself || routing_ == SchemaRouting::AroundTables) {
                // No way round, or a line back into its own table: a plain elbow
                // out and back, which is still orthogonal and still readable.
                const auto out = from_step.x() < 0 && to_step.x() < 0
                    ? std::min(start.x(), finish.x())
                    : std::max(start.x(), finish.x()) + (itself ? 26 : 0);
                routed.corners = {from, QPointF(out, from.y()), QPointF(out, to.y()), to};
                tighten(routed.corners);
            } else {
                // Straight there: out of one table, one turn, into the other.
                // It crosses whatever lies between, which is the point -- what
                // is joined to what is plain, and no detour is hiding it.
                routed.corners = {from, start};
                bool horizontal = from_step.x() != 0;
                step_to(routed.corners, horizontal, finish);
                routed.corners.push_back(to);
                tighten(routed.corners);
            }
        }
        tighten(routed.corners);
        straighten(routed.corners);
        // The end symbols are squared on the line as it was actually drawn,
        // not on the side of the table the router set out from. A line that has
        // been pushed about, or one whose end has been taken off its table, may
        // arrive from anywhere, and a foot lying across a line rather than
        // along it is the plainest sign that something has gone wrong.
        // The step runs from the end outwards along the line, which is the way
        // the symbols are built: the foot reaches out along it and opens back
        // onto the end, and the minimum sits beyond that.
        const auto facing = [](QPointF at, QPointF along) {
            const auto away = along - at;
            if (std::abs(away.x()) < 0.01 && std::abs(away.y()) < 0.01) return QPointF();
            return std::abs(away.x()) >= std::abs(away.y())
                ? QPointF(away.x() < 0 ? -1 : 1, 0)
                : QPointF(0, away.y() < 0 ? -1 : 1);
        };
        if (routed.corners.size() >= 2) {
            const auto leaving = facing(routed.corners.front(), routed.corners[1]);
            const auto arriving_at = facing(routed.corners.back(),
                                            routed.corners[routed.corners.size() - 2]);
            if (!leaving.isNull()) routed.from_step = leaving;
            if (!arriving_at.isNull()) routed.to_step = arriving_at;
            // Only now, with the line's real directions known, can the room
            // its symbols need be held open at each end.
            reserve_ends(routed.corners, routed.from_step, routed.to_step);
            tighten(routed.corners);
        }
        // Whatever shape it ended up with, the next line is told where it went.
        // The fallback elbows used to say nothing, which is how two of them came
        // to be drawn along exactly the same line.
        mark(grid, routed.corners);
        routed.path = path_of(routed.corners);
        routes_.push_back(std::move(routed));
    }
}

// Which line the pointer is over, and which of its corners a drag would take
// hold of. A corner already there is picked up; anywhere else along the line
// gives the place a new one belongs, counted among the corners the line has
// rather than among the ones the router worked out.
std::size_t SchemaView::shaped_lines() const {
    auto shaped_count = editor_.project().schema_layout.lines.size();
    if (shaping_shape_ && !editor_.project().schema_layout.lines.contains(
            domain::foreign_key_from(shaping_shape_->first)))
        ++shaped_count;
    return shaped_count;
}

std::size_t SchemaView::loose_ends() const {
    std::size_t loose = 0;
    const auto count = [&](const Shape& shape) {
        if (shape.from && !shape.from->on_table) ++loose;
        if (shape.to && !shape.to->on_table) ++loose;
    };
    for (const auto& [key, line] : editor_.project().schema_layout.lines) {
        (void)key;
        count(as_shape(line));
    }
    if (shaping_shape_ && !editor_.project().schema_layout.lines.contains(
            domain::foreign_key_from(shaping_shape_->first)))
        count(shaping_shape_->second);
    return loose;
}

std::optional<SchemaView::Shaping> SchemaView::grip_at(QPointF point) const {
    // The nearest end, not the first one near enough. Ends landing on one row
    // are fanned a few pixels apart, which is closer than the reach of a grab,
    // so taking the first would hand back whichever line happened to be routed
    // earlier rather than the one being pointed at.
    std::optional<Shaping> nearest;
    auto closest = static_cast<double>(handle_size);
    for (const auto& routed : routes_) {
        if (!routed.link) continue;
        const std::array<std::pair<QPointF, Grip>, 2> ends{
            std::pair{routed.from, Grip::FromEnd}, std::pair{routed.to, Grip::ToEnd}};
        for (const auto& [at, which] : ends) {
            const auto away = point - at;
            const auto gap = std::hypot(away.x(), away.y());
            if (gap > closest) continue;
            closest = gap;
            nearest = Shaping{*routed.link, which, 0, point, true};
        }
    }
    return nearest;
}

std::optional<SchemaView::Shaping> SchemaView::line_at(QPointF point) const {
    // An end is grabbed before the run it is attached to, so the join can still
    // be moved where a run reaches right up to it.
    if (auto grip = grip_at(point)) return grip;
    std::optional<Shaping> nearest;
    auto closest = grab_reach;
    for (const auto& routed : routes_) {
        if (!routed.link || routed.corners.size() < 2) continue;
        const auto [run, gap] = run_nearest(routed.corners, point);
        if (gap > closest) continue;
        closest = gap;
        nearest = Shaping{*routed.link, Grip::Run, run, point, false};
    }
    return nearest;
}

std::optional<Qt::CursorShape> SchemaView::run_cursor(QPointF point) const {
    const auto line = line_at(point);
    if (!line) return std::nullopt;
    if (line->grip != Grip::Run) return Qt::OpenHandCursor;
    const auto found = std::find_if(routes_.begin(), routes_.end(), [&](const Routed& routed) {
        return routed.link && *routed.link == line->link;
    });
    if (found == routes_.end() || line->index + 1 >= found->corners.size())
        return Qt::OpenHandCursor;
    // The cursor says which way the run will go before it is taken hold of: an
    // upright run moves across, a level one moves up and down, and neither can
    // be pushed along its own length.
    const auto a = found->corners[line->index];
    const auto b = found->corners[line->index + 1];
    return std::abs(a.x() - b.x()) < 0.01 ? Qt::SizeHorCursor : Qt::SizeVerCursor;
}

void SchemaView::paintEvent(QPaintEvent*) {
    if (!theme_) return;
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.fillRect(rect(), theme_->canvas);
    painter.setBrush(Qt::NoBrush);

    if (preview_.tables.empty()) {
        painter.setPen(theme_->muted);
        // A schema drawn by hand has no entity to wait for: it starts from a
        // table (Zain, 2026-09-27).
        painter.drawText(rect(), Qt::AlignCenter,
                         drawn_by_hand() ? "Create a table to start designing your schema."
                                         : "Draw an entity and its Relational Design appears here.");
        return;
    }
    const auto chosen_row = [this]() -> std::optional<std::pair<std::size_t, std::size_t>> {
        const auto now = selection_now();
        if (const auto* column = std::get_if<ChosenColumn>(&now)) return locate(column->column);
        return std::nullopt;
    }();

    // Every line is routed before any is drawn, then all the knockouts, then
    // all the lines. The order matters: a knockout laid down after a line would
    // erase the line it crossed, which is how a line comes to vanish halfway
    // along while another appears to run straight through it.
    for (const auto& routed : routes_) {
        painter.setPen(QPen(theme_->canvas, 6, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.drawPath(routed.path);
    }
    // The line chosen is picked out as the one under the pointer is.
    const auto chosen_line = [this]() -> std::optional<domain::LinkSource> {
        const auto now = selection_now();
        if (const auto* key = std::get_if<ChosenForeignKey>(&now)) return link_of(key->key);
        return std::nullopt;
    }();
    const auto lit = [&](const Routed& routed) {
        return routed.link && ((hovered_ && *routed.link == *hovered_) || (chosen_line && *routed.link == *chosen_line));
    };
    // A line is only as prominent as the tables it joins: faded at either end,
    // it fades too, or the schema would be crossed by lines leading to nothing
    // the reader is being shown.
    const auto shown = lit_tables();
    const auto faded = std::find(shown.begin(), shown.end(), false) != shown.end();
    const auto line_shown = [&](const Routed& routed) {
        return !faded || (routed.from_table < shown.size() && shown[routed.from_table]
                          && routed.to_table < shown.size() && shown[routed.to_table]);
    };
    const auto dim = [&](QColor colour, bool keep) {
        if (keep) return colour;
        colour.setAlphaF(0.16);
        return colour;
    };
    // The ring round the table being asked about is carried along everything
    // it is joined to. A table picked out while its connections look like
    // every other line says only where the table is; the ring running out
    // along the lines says what it reaches, which is the question that was
    // asked by pressing it. Laid before the lines, so the colours still sit
    // on top and crossings still read.
    if (const auto asking = selected_table()) {
        painter.setPen(QPen(theme_->text, 6.0, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        for (const auto& routed : routes_) {
            if (routed.from_table != *asking && routed.to_table != *asking) continue;
            if (!line_shown(routed)) continue;
            painter.drawPath(routed.path);
        }
    }
    // Drawn bold, so a connection reads at a glance across a full schema
    // (Zain, 2026-09-25), and bolder still under the pointer.
    for (const auto& routed : routes_) {
        painter.setPen(QPen(dim(routed.colour, line_shown(routed)), lit(routed) ? 3.4 : 2.6,
                            Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.drawPath(routed.path);
    }

    // The ends. The foot opens onto the table's edge and the minimum sits
    // outside it, both drawn wholly clear of the row they belong to, and turned
    // to face whichever side the line arrives on.
    const auto draw_ends = [&](const Routed& routed) {
        const auto draw_end = [&](QPointF at, QPointF step, bool many, bool optional) {
            const QPointF across(-step.y(), step.x());
            const auto point = [&](double along, double side) {
                return QPointF(at.x() + step.x() * along + across.x() * side,
                               at.y() + step.y() * along + across.y() * side);
            };
            painter.setPen(QPen(dim(routed.colour, line_shown(routed)), 2.2));
            painter.setBrush(Qt::NoBrush);
            if (notation_ == Notation::Chen || notation_ == Notation::MinMax) {
                // The label stands beside the line rather than on it, on
                // whichever side of it is clear of the tables. It is given the
                // canvas to sit on so the line does not run through the middle
                // of it -- but only where that canvas is its own, because a
                // patch laid over a table would rub out the table's own words.
                const auto beside = [&](double side) {
                    const auto at_side = point(minimum_at + 12, side);
                    return QRectF(at_side.x() - 16, at_side.y() - 8, 32, 16);
                };
                auto box = beside(-12);
                const auto clear = [&](const QRectF& candidate) {
                    return std::none_of(placed_.begin(), placed_.end(), [&](const Placed& one) {
                        return one.box.intersects(candidate);
                    });
                };
                const bool room = clear(box) || (box = beside(12), clear(box));
                if (room) painter.fillRect(box, theme_->canvas);
                painter.drawText(box, Qt::AlignCenter,
                                 notation_ == Notation::Chen
                                     ? (many ? "M" : "1")
                                     : QString("(%1,%2)").arg(optional ? 0 : 1).arg(many ? "N" : "1"));
                return;
            }
            if (notation_ == Notation::Bachman) {
                if (!many) {
                    painter.drawLine(point(10, -5), point(1, 0));
                    painter.drawLine(point(10, 5), point(1, 0));
                }
            } else if (many) {
                painter.drawLine(point(foot_reach, 0), point(1, -foot_spread));
                painter.drawLine(point(foot_reach, 0), point(1, 0));
                painter.drawLine(point(foot_reach, 0), point(1, foot_spread));
            } else {
                painter.drawLine(point(7, -foot_spread), point(7, foot_spread));
            }
            if (optional) {
                painter.setBrush(theme_->canvas);
                painter.drawEllipse(point(minimum_at, 0), 4.2, 4.2);
                painter.setBrush(Qt::NoBrush);
            } else if (notation_ != Notation::Bachman) {
                painter.drawLine(point(minimum_at, -foot_spread), point(minimum_at, foot_spread));
            }
        };
        draw_end(routed.from, routed.from_step, routed.many, routed.optional);
        draw_end(routed.to, routed.to_step, false, false);
    };

    // Then the tables.
    auto bold = font();
    bold.setBold(true);
    auto mono = font();
    mono.setFamily("Menlo");
    mono.setPointSizeF(font().pointSizeF() - 0.5);

    auto small = font();
    small.setPointSizeF(std::max(6.5, font().pointSizeF() - 2.0));
    small.setCapitalization(QFont::AllUppercase);
    auto small_question = font();
    small_question.setPointSizeF(std::max(7.0, font().pointSizeF() - 1.0));
    for (std::size_t t = 0; t < preview_.tables.size(); ++t) {
        const auto& table = preview_.tables[t];
        const auto& box = placed_[t].box;
        const auto here = !faded || shown[t];
        const auto surface = dim(surface_for(table), here);
        const auto edge = dim(edge_for(table), here);

        painter.setPen(QPen(edge, 1.5));
        painter.setBrush(theme_->base);
        painter.drawRoundedRect(box, 3, 3);
        painter.setBrush(surface);
        painter.drawRoundedRect(QRectF(box.left(), box.top(), box.width(), header_height), 3, 3);
        painter.fillRect(QRectF(box.left() + 1, box.top() + header_height - 4, box.width() - 2, 4), surface);
        // The table being asked about is outlined, so which one the schema is
        // answering for is never in doubt.
        if (is_marked(table)) {
            painter.setPen(QPen(theme_->text, 2.0));
            painter.setBrush(Qt::NoBrush);
            painter.drawRoundedRect(box.adjusted(-2, -2, 2, 2), 4, 4);
        }

        // Where the table came from, said in the header. A reader learning the
        // rules needs to know that Enrolleds is a bridge and Phones is a
        // multivalued attribute, and the schema is the only place that says so.
        // Not in the compact schema, which gives each table its name alone:
        // cut down to a table only as wide as its names, the badge was never
        // more than a few letters and an ellipsis.
        const auto came_from = names_only_ ? QString() : provenance_of(table);
        // A hovered table wears a plus at the right of its header, so the room
        // the header has for words is that much less while it is worn. Taken
        // off here rather than drawn over, or the badge and the plus would sit
        // on top of one another.
        const bool offering = hovered_table_ && *hovered_table_ == t && table.origin.has_value();
        auto room = box.width() - 18;
        if (!came_from.isEmpty()) {
            painter.setFont(small);
            // The name comes first: it is what the table is called, and a
            // reader looking for one knows its name and not its provenance.
            // Whatever is left over goes to the provenance, shortened from the
            // end where there is not enough, so it is never cut off its front.
            const QFontMetricsF measured(small);
            const auto named = QFontMetricsF(bold).horizontalAdvance(QString::fromStdString(table.name));
            const auto spare = std::max(0.0, room - named - 10);
            const auto shortened = measured.elidedText(came_from, Qt::ElideRight, spare);
            const auto wide = std::min(measured.horizontalAdvance(shortened) + 2, spare);
            if (wide > 16) {
                auto ink = readable_on(surface);
                ink.setAlphaF(here ? 0.62 : 0.16);
                painter.setPen(ink);
                painter.drawText(QRectF(box.right() - 9 - wide, box.top(), wide, header_height),
                                 Qt::AlignRight | Qt::AlignVCenter, shortened);
                room -= wide + 10;
            }
        }
        painter.setFont(bold);
        painter.setPen(dim(readable_on(surface), here));
        painter.drawText(QRectF(box.left() + 9, box.top(), std::max(20.0, room), header_height),
                         Qt::AlignLeft | Qt::AlignVCenter,
                         QFontMetricsF(bold).elidedText(QString::fromStdString(table.name),
                                                        Qt::ElideRight, std::max(20.0, room)));

        // The empty slot, under the table being pointed at. Brighter while the
        // pointer is actually on it, which is what says it can be pressed.
        // While a line is being drawn the strip a line can be let go on stands
        // there instead (draw_linking), as nothing can be pressed then.
        if (offering && !(linking_ && linking_->travelled)) {
            const auto slot = add_slot(t);
            const auto under = slot.contains(pointer_);
            auto ink = theme_->muted;
            QPen frame(ink, 1.0, Qt::DashLine);
            auto wash = ink;
            wash.setAlphaF(under ? 0.16 : 0.07);
            painter.setPen(frame);
            painter.setBrush(wash);
            painter.drawRoundedRect(slot, 3, 3);
            auto words = theme_->text;
            words.setAlphaF(under ? 0.9 : 0.55);
            painter.setPen(words);
            painter.setFont(small);
            painter.drawText(slot, Qt::AlignCenter, tr("+  Add column"));
            painter.setFont(mono);
        }

        // Where the columns end and the table's questions begin. Everything
        // below this line is about the table rather than a row of it, so the
        // gutter stops here and the footer is shaded apart. Read off the rows
        // themselves, since a table pulled taller shares that room out between
        // them and its rows are deeper than the standard one.
        const auto rows_end = placed_[t].rows.empty() ? box.top() + header_height + heading_room()
                                                      : placed_[t].rows.back().bottom();

        // The gutter holding the keys, closed by a rule running the height of
        // the columns: it is reserved for keys and nothing else, and there are
        // no keys among the questions underneath.
        painter.setPen(QPen(dim(edge, here), 1.0));
        painter.drawLine(QPointF(box.left() + gutter_width, box.top() + header_height),
                         QPointF(box.left() + gutter_width, rows_end));

        // The rules between the columns: name, type, and each constraint in a
        // column of its own. They run only as far as the rows do, stopping
        // where the gutter stops, so the empty slot under the last row stays
        // the clear place to press that it is and the footer's questions are
        // not ruled into columns they have nothing to do with.
        // Both name a column of their own, so both run from the top of the
        // headings down to where the rows end.
        for (const auto x : placed_[t].dividers)
            painter.drawLine(QPointF(x, box.top() + header_height), QPointF(x, rows_end));

        // What each column holds, said once at the top of the table. The
        // three constraints share one heading, because between them they are
        // the one question of what the table enforces; they keep their own
        // columns underneath, where telling them apart is what matters.
        const auto heading_top = box.top() + header_height;
        const auto heading_end = heading_top + heading_height;
        // With only the names shown there is no row naming the columns.
        if (!names_only_) {
        painter.setPen(QPen(dim(edge, here), 1.0));
        painter.drawLine(QPointF(box.left() + 1, heading_end), QPointF(box.right() - 1, heading_end));
        {
            auto ink = readable_on(surface);
            ink.setAlphaF(here ? 0.62 : 0.22);
            painter.setPen(ink);
            // Named, not stamped: these are words to read, unlike the
            // provenance in the header above, which is a label on the table.
            auto naming = small;
            naming.setCapitalization(QFont::MixedCase);
            painter.setFont(naming);
            const QRectF band(box.left(), heading_top, box.width(), heading_height);
            // Over the names, not over the gutter. The gutter is the keys'
            // and is ruled off from the names beside it; a heading standing
            // on the wrong side of that rule names the wrong column.
            const auto names_from = box.left() + gutter_width + 7;
            const auto names_to = placed_[t].dividers.empty() ? box.right() - 9
                                                              : placed_[t].dividers.front() - 6;
            painter.drawText(QRectF(names_from, heading_top,
                                    std::max(20.0, names_to - names_from), heading_height),
                             Qt::AlignLeft | Qt::AlignVCenter, tr("Column"));
            // A heading for each column that survived the table's width. A
            // folded column has no heading, or the row would be named after
            // something that is not there.
            const QFontMetricsF heading(naming);
            const auto& rules = placed_[t].dividers;
            if (!rules.empty()) {
                const auto type_to = rules.size() > 1 ? rules[1] : band.right();
                painter.drawText(QRectF(rules[0], heading_top, type_to - rules[0], heading_height),
                                 Qt::AlignCenter,
                                 heading.elidedText(tr("Type"), Qt::ElideRight,
                                                    std::max(8.0, type_to - rules[0] - 4)));
            }
            if (rules.size() > 1) {
                const QRectF over(rules[1] + 6, heading_top, band.right() - rules[1] - 12,
                                  heading_height);
                painter.drawText(over, Qt::AlignLeft | Qt::AlignVCenter,
                                 heading.elidedText(tr("Constraints"), Qt::ElideRight, over.width()));
            }
            painter.setFont(mono);
        }
        }

        painter.setFont(mono);
        for (std::size_t row = 0; row < table.columns.size(); ++row) {
            const auto& column = table.columns[row];
            const auto& where = placed_[t].rows[row];
            // The column chosen on its own wears a quiet wash of the accent,
            // so what Properties is showing is plain here too (Stage 1).
            if (chosen_row && chosen_row->first == t && chosen_row->second == row) {
                auto wash = theme_->accent;
                wash.setAlpha(38);
                painter.fillRect(where.adjusted(1, 0.5, -1, -0.5), wash);
            }
            if (row) {
                painter.setPen(QPen(dim(edge, here), 0.6));
                painter.drawLine(QPointF(where.left() + 1, where.top()), QPointF(where.right() - 1, where.top()));
            }
            // References always use the FK green, even if another constraint
            // also applies. Never merge the two meanings into a PK FK badge.
            // A column that is both keys wears both marks, side by side and
            // each in its own colour (Zain, 2026-10-01): the golden key, the
            // orange PK, then the green FK, so neither role hides the other.
            const QString marks = column.foreign_key ? "FK" : column.primary_key ? "PK" : "";
            if (!marks.isEmpty()) {
                const bool both = column.primary_key && column.foreign_key;
                // The key is the size it is in every key row, worked out from
                // the letters at their own size.
                const auto lettered = QFontMetricsF(painter.font()).horizontalAdvance(marks);
                const auto side = static_cast<int>(std::min({gutter_width - 6 - lettered,
                                                             where.height() - 2, 19.0}));
                // Both pairs of letters stand in the gutter every table has,
                // beside a key of that size: drawn only as much smaller as
                // the room left beside it asks, and PK a hair before FK.
                double letters = lettered;
                if (both) {
                    constexpr double between = 2;
                    painter.save();
                    const auto room = gutter_width - 8 - (side >= 9 ? side * 0.47 : 0.0);
                    if (const auto wanted = 2 * lettered + between; wanted > room) {
                        auto smaller = painter.font();
                        smaller.setPointSizeF(smaller.pointSizeF() * room / wanted);
                        painter.setFont(smaller);
                    }
                    const QFontMetricsF fitted(painter.font());
                    const auto after = fitted.horizontalAdvance(marks) + between;
                    painter.setPen(dim(theme_->warning, here));
                    painter.drawText(QRectF(where.left(), where.top(), gutter_width - 4 - after, where.height()),
                                     Qt::AlignRight | Qt::AlignVCenter, QStringLiteral("PK"));
                    letters = after + fitted.horizontalAdvance(QStringLiteral("PK"));
                }
                painter.setPen(dim(column.foreign_key ? theme_->valid : theme_->warning, here));
                painter.drawText(QRectF(where.left(), where.top(), gutter_width - 4, where.height()),
                                 Qt::AlignRight | Qt::AlignVCenter, marks);
                if (both) painter.restore();
                if (column.primary_key) {
                    if (side >= 9) {
                        const auto& mark = key_mark(side);
                        const auto was = painter.opacity();
                        if (!here) painter.setOpacity(was * 0.45);
                        // Stood right before PK, so the key and its letters
                        // read as one mark beside the name (Zain,
                        // 2026-09-26): the drawing's key reaches 72% across
                        // its square, and its teeth end a hair short of P.
                        // Beside both pairs of letters it may stand with the
                        // empty margin of its square past the gutter's edge,
                        // the key itself still inside it.
                        const auto letters_at = where.left() + gutter_width - 4 - letters;
                        const auto edge = both ? where.left() + 1 - side * 0.24 : where.left() + 1;
                        const auto left = std::max(edge, letters_at - 3 - side * 0.72);
                        painter.drawPixmap(QPointF(left, where.top() + (where.height() - side) / 2), mark);
                        painter.setOpacity(was);
                    }
                }
            }
            // A column that exists here and not on the diagram is named in
            // italic. It is a real column and reads as one; the slant is only
            // to say that the diagram does not know about it, which is a thing
            // the reader chose and should be able to see at a glance.
            // A schema drawn by hand has no diagram to be missing from, so
            // its columns stand upright.
            const bool only_here = column.origin_kind == domain::ColumnOrigin::SchemaOnly && !drawn_by_hand();
            if (only_here) {
                auto slanted = mono;
                slanted.setItalic(true);
                painter.setFont(slanted);
            }
            // A column the conversion left out is named in italic and greyed,
            // and says what became of it instead of carrying a type.
            if (column.ignored) {
                auto slanted = mono;
                slanted.setItalic(true);
                painter.setFont(slanted);
            }
            // What the right-hand side says: what a dropped column became,
            // what a key points at while its table is the one being asked
            // about, and otherwise the type.
            const bool showing_target =
                selected() && is_marked(table) && column.references
                      && *column.references < preview_.tables.size()
                      && column.references_column < preview_.tables[*column.references].columns.size();
            const auto written = column.ignored
                ? QString("ignored")
                : selected() && is_marked(table) && column.references
                      && *column.references < preview_.tables.size()
                      && column.references_column < preview_.tables[*column.references].columns.size()
                ? QString("→ %1.%2")
                      .arg(QString::fromStdString(preview_.tables[*column.references].name))
                      .arg(QString::fromStdString(
                          preview_.tables[*column.references].columns[column.references_column].name))
                : type_text(column);
            // The two share the row rather than being given fixed halves of
            // it: a type is three letters and a key's target is a sentence,
            // and whichever is there, the name must not be written over.
            const QFontMetricsF measuring(painter.font());
            const auto& cell = placed_[t].cells[row];
            // Where the name's column ends. Before the type where the row is
            // ruled into columns, at the row's own edge where it is too narrow
            // to be.
            const auto name_end = placed_[t].dividers.empty()
                ? where.right() - 7
                : placed_[t].dividers.front() - 6;
            const bool unanswered = written == "?";
            painter.setPen(dim(column.ignored ? theme_->muted : theme_->text, here));
            const auto name_room = std::max(0.0, name_end - where.left() - gutter_width - 7);
            painter.drawText(QRectF(where.left() + gutter_width + 7, where.top(),
                                    name_room, where.height()),
                             Qt::AlignLeft | Qt::AlignVCenter,
                             measuring.elidedText(QString::fromStdString(column.name),
                                                  Qt::ElideRight, name_room));
            if (only_here || column.ignored) painter.setFont(mono);
            // Whether this row is the one with something open over it, and
            // which of its two halves that something is filling.
            const auto being_answered = [&](bool size) {
                if (!answering_ || answering_size_ != size) return false;
                if (column.origin && std::holds_alternative<domain::AttributeId>(*answering_))
                    return std::get<domain::AttributeId>(*answering_) == *column.origin;
                if (column.added && std::holds_alternative<domain::SchemaColumnId>(*answering_))
                    return std::get<domain::SchemaColumnId>(*answering_) == *column.added;
                return false;
            };
            // The whole of the type's column, which is what the type is drawn
            // in whether or not it can be pressed.
            const QRectF typed_box = cell.type.isEmpty()
                ? QRectF(name_end + 6, where.center().y() - ask_height / 2,
                         // With the constraints folded away there is only the
                         // one rule, and the type runs to the table's edge,
                         // as its heading does.
                         placed_[t].dividers.empty()
                             ? 0.0
                             : (placed_[t].dividers.size() > 1 ? placed_[t].dividers[1] : where.right())
                                   - placed_[t].dividers.front(),
                         ask_height)
                : QRectF(cell.type.left(), cell.type.top(),
                         cell.size.isEmpty() ? cell.type.width()
                                             : cell.size.right() - cell.type.left(),
                         ask_height);
            if (typed_box.width() > 1) {
                if (!cell.type.isEmpty() && (being_answered(false) || being_answered(true))) {
                    // Something is on its way into this row. No dashes,
                    // because it is no longer asking; no question mark,
                    // because the question has been put and is being answered
                    // somewhere above.
                    auto hollow = theme_->muted;
                    hollow.setAlphaF(0.22);
                    painter.setPen(QPen(dim(theme_->muted, here), 1.0));
                    painter.setBrush(QBrush(hollow));
                    painter.drawRoundedRect(being_answered(true) && !cell.size.isEmpty()
                                                ? cell.size : cell.type,
                                            ask_radius, ask_radius);
                    painter.setBrush(QBrush(Qt::NoBrush));
                    if (being_answered(true) && !cell.size.isEmpty()) {
                        painter.setPen(dim(theme_->muted, here));
                        painter.drawText(cell.type, Qt::AlignRight | Qt::AlignVCenter,
                                         type_name(column));
                    }
                } else if (!cell.type.isEmpty() && unanswered) {
                    // A type nobody has given yet is a blank to be filled in,
                    // and it is drawn as one: a dashed outline round a pane of
                    // its own colour, with the question in the middle of it. A
                    // bare "?" reads as a remark about the column; a box
                    // waiting to be filled reads as something to do.
                    QPen dashes(dim(theme_->warning, here), 1.1, Qt::DashLine);
                    dashes.setDashPattern({2.6, 2.2});
                    painter.setPen(dashes);
                    auto glass = theme_->warning;
                    glass.setAlphaF(here ? 0.11 : 0.03);
                    painter.setBrush(QBrush(glass));
                    painter.drawRoundedRect(typed_box.adjusted(5, 0, -5, 0), ask_radius, ask_radius);
                    painter.setBrush(QBrush(Qt::NoBrush));
                    painter.setPen(dim(theme_->warning, here));
                    painter.drawText(typed_box, Qt::AlignCenter, "?");
                } else {
                    // The type and its length, written as one word the way SQL
                    // writes it. The column it sits in is ruled off on both
                    // sides, which is what says where it ends; it wears a box
                    // only while it is pointed at, and then only round the half
                    // under the pointer, because the two halves ask different
                    // questions.
                    if (hovered_constraint_ && hovered_constraint_->table == t
                        && hovered_constraint_->row == row) { /* a mark has it */ }
                    else if (!cell.type.isEmpty() && hovered_type_
                             && hovered_type_->table == t && hovered_type_->row == row) {
                        painter.setPen(QPen(dim(theme_->border, here), 1.0));
                        painter.setBrush(QBrush(Qt::NoBrush));
                        painter.drawRoundedRect(hovered_type_->size && !cell.size.isEmpty()
                                                    ? cell.size : cell.type,
                                                ask_radius, ask_radius);
                    }
                    painter.setPen(dim(theme_->muted, here));
                    const auto say = showing_target || column.ignored || cell.type.isEmpty()
                        ? written
                        : typed_label(column);
                    // A key's target is cut from its front, because the end of
                    // "-> Students.StudentID" is the part worth keeping. Any
                    // other word is cut from its end, the way words are read.
                    painter.drawText(typed_box, Qt::AlignCenter,
                                     QFontMetricsF(painter.font()).elidedText(
                                         say, showing_target ? Qt::ElideLeft : Qt::ElideRight,
                                         typed_box.width()));
                }
            }
            // What the column enforces, in one Constraints cell at the right
            // of every real column, written the way the generated SQL will
            // write it (rules_text): PK where the column is in the key; then
            // NULL or NOT NULL, always written out either way, because a
            // column that may be empty must never look like one nobody has
            // decided about; then UNIQUE and IDENTITY, each only where it is
            // set. A rule that is off is not written at all.
            //
            // The cell wears its box only while it is pointed at. A box on
            // every row would drown the rows it describes, and the box is
            // there to say the cell can be pressed, which matters at the
            // moment the hand is on it and not before. Pressing it opens the
            // list of what can be said about this column rather than toggling
            // one thing: several of them apply at once, and a few of them
            // cannot apply together, which a list can say and a switch cannot.
            if (!cell.rules.isEmpty()) {
                painter.setFont(mono);
                const bool under = hovered_constraint_ && hovered_constraint_->table == t
                                && hovered_constraint_->row == row;
                if (under) {
                    painter.setPen(QPen(dim(theme_->border, here), 1.0));
                    painter.setBrush(QBrush(Qt::NoBrush));
                    painter.drawRoundedRect(cell.rules.adjusted(3, 0, -3, 0), ask_radius, ask_radius);
                    painter.setBrush(QBrush(Qt::NoBrush));
                }
                const auto said = rules_text(column);
                painter.setPen(dim(theme_->text, here));
                painter.drawText(cell.rules.adjusted(6, 0, -6, 0),
                                 Qt::AlignLeft | Qt::AlignVCenter,
                                 QFontMetricsF(mono).elidedText(said, Qt::ElideRight,
                                                                cell.rules.width() - 12));
            }
        }

        // The questions this table is where to answer, under its columns. They
        // are put here rather than collected into a dialog because the answer
        // changes this table, and a reader deciding should be looking at what
        // the decision is about.
        //
        // They are shaded apart and ruled off first. A question about the
        // whole table reading as though it were another row of it is the one
        // way this could mislead, so the footer is plainly not a row: it has
        // its own ground, and the last column keeps a line under it.
        if (!table.decisions.empty() && rows_end < box.bottom() - 1) {
            const QRectF footer(box.left() + 1, rows_end, box.width() - 2, box.bottom() - rows_end);
            QPainterPath under;
            under.addRoundedRect(box, 3, 3);
            painter.save();
            painter.setClipPath(under);
            auto ground = theme_->canvas;
            ground.setAlphaF(here ? 0.55 : 0.18);
            painter.fillRect(footer, ground);
            painter.restore();
            painter.setPen(QPen(dim(edge, here), 1.0));
            painter.drawLine(QPointF(box.left(), rows_end), QPointF(box.right(), rows_end));
        }
        painter.setFont(small_question);
        for (std::size_t d = 0; d < table.decisions.size() && d < placed_[t].asked.size(); ++d) {
            painter.setPen(dim(theme_->muted, here));
            painter.drawText(placed_[t].asked[d], Qt::AlignLeft | Qt::AlignVCenter,
                             QFontMetricsF(small_question).elidedText(
                                 question_of(table.decisions[d]), Qt::ElideRight,
                                 placed_[t].asked[d].width()));
        }
        const auto answers_for = [&](std::size_t d) {
            return d < table.decisions.size() ? answers_to(table.decisions[d]) : QStringList{};
        };
        for (const auto& chip : placed_[t].chips) {
            if (chip.decision >= table.decisions.size()) continue;
            const auto& decision = table.decisions[chip.decision];
            const auto answers = answers_for(chip.decision);
            if (chip.choice >= static_cast<std::size_t>(answers.size())) continue;
            const bool taken = chip.choice == decision.chosen;
            // The answer in force is filled in. Where it is only what the
            // conversion does because nobody has said, it is outlined instead:
            // a default is not a decision, and the two must not look alike.
            const bool settled = taken && decision.answered;
            QColor ink;
            if (settled) {
                painter.setPen(QPen(dim(theme_->accent, here), 1.0));
                painter.setBrush(QBrush(dim(theme_->accent, here)));
                ink = readable_on(theme_->accent);
            } else if (taken) {
                // In force, but only because nobody has said. It is filled, so
                // that which answer is in force is never in doubt, and filled
                // more faintly, so that a default and a decision are not the
                // same picture.
                auto tint = theme_->accent;
                tint.setAlphaF(here ? 0.30 : 0.10);
                painter.setPen(QPen(dim(theme_->accent, here), 1.0));
                painter.setBrush(QBrush(tint));
                ink = theme_->accent;
            } else {
                painter.setPen(QPen(dim(theme_->border, here), 1.0));
                painter.setBrush(QBrush(Qt::NoBrush));
                ink = theme_->muted;
            }
            painter.drawRoundedRect(chip.box, chip_height / 2, chip_height / 2);
            auto chip_font = small_question;
            chip_font.setBold(taken);
            painter.setFont(chip_font);
            painter.setPen(dim(ink, here));
            painter.drawText(chip.box, Qt::AlignCenter, answers[static_cast<int>(chip.choice)]);
        }
        painter.setBrush(Qt::NoBrush);
        painter.setFont(mono);
    }

    // The line under the pointer is then laid over the tables. A line is
    // normally behind them, which is what keeps a schema from being crossed
    // out by its own connections -- but a line being worked on has to be
    // followable for its whole length, including wherever it has been dragged.
    // A path is filled as well as stroked unless it is told not to be, and the
    // brush still standing here is the last table's own surface.
    painter.setBrush(Qt::NoBrush);
    for (const auto& routed : routes_) {
        // A line that is being faded is not being shown, so it is not lifted
        // over the tables either, however near the pointer happens to be.
        if (!lit(routed) || !line_shown(routed)) continue;
        painter.setPen(QPen(theme_->canvas, 7, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.drawPath(routed.path);
        painter.setPen(QPen(routed.colour, 3.4, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        painter.drawPath(routed.path);
    }

    // The ends go on last. A symbol stands outside its own table, but it can
    // still fall under the table beside it, and a foot or a minimum half
    // covered by a neighbour is worse than one that is simply not there.
    painter.setFont(mono);
    for (const auto& routed : routes_) draw_ends(routed);

    // The band goes over everything: it is the thing being done, not part of
    // what is being looked at. Before the hovered line's grips, which give up
    // early, so a band is drawn whether or not a line happens to be hovered.
    draw_band(painter);
    draw_linking(painter);

    // The line under the pointer shows a round grip at each end, which is what
    // moves where the line meets its table. Its runs carry no marks: a run is
    // taken hold of anywhere along it, and the cursor says which way it goes.
    if (!hovered_) return;
    const auto found = std::find_if(routes_.begin(), routes_.end(), [&](const Routed& routed) {
        return routed.link && *routed.link == *hovered_;
    });
    if (found == routes_.end()) return;
    painter.setPen(QPen(found->colour, 1.4));
    const auto end_grip = [&](QPointF at, bool loose) {
        // A loose end is filled rather than hollow, because an end resting on
        // its table and an end left hanging beside it are otherwise the same
        // small circle, and which of the two it is matters.
        painter.setBrush(loose ? found->colour : theme_->base);
        painter.drawEllipse(at, handle_size / 2.0, handle_size / 2.0);
    };
    end_grip(found->from, found->from_loose);
    end_grip(found->to, found->to_loose);
}

// The band drawn round tables while the hand is gathering them. Laid over
// everything, because it is the thing being done rather than part of what is
// being looked at. Drawn in the theme's accent -- the colour the rest of the
// application uses for what is chosen -- filled faintly so the tables under it
// can still be seen being caught.
bool SchemaView::drawn_by_hand() const { return editor_.project().schema.standalone; }

std::optional<std::pair<std::size_t, std::size_t>> SchemaView::gutter_at(QPointF point) const {
    if (!drawn_by_hand()) return std::nullopt;
    const auto table = table_at(point);
    if (!table) return std::nullopt;
    const auto row = row_at(*table, point);
    if (!row) return std::nullopt;
    if (point.x() > placed_[*table].rows[*row].left() + gutter_width) return std::nullopt;
    return std::pair{*table, *row};
}

void SchemaView::set_connecting(bool on) {
    if (connecting_ == on) return;
    connecting_ = on;
    if (!on) linking_.reset();
    setCursor(Qt::ArrowCursor);
    update();
}

void SchemaView::set_placing(bool on) {
    if (placing_ == on) return;
    placing_ = on;
    setCursor(on ? Qt::CrossCursor : Qt::ArrowCursor);
    update();
}

std::optional<std::pair<std::size_t, std::size_t>> SchemaView::connect_row_at(QPointF point) const {
    if (!connecting_ || !drawn_by_hand()) return std::nullopt;
    const auto table = table_at(point);
    if (!table) return std::nullopt;
    const auto row = row_at(*table, point);
    if (!row) return std::nullopt;
    return std::pair{*table, *row};
}

void SchemaView::pick(std::optional<std::variant<SchemaColumnRef, domain::ForeignKeyId>> what) {
    if (picked_ == what) return;
    picked_ = std::move(what);
    update();
    if (chose) chose();
}

std::optional<domain::ForeignKeyId> SchemaView::key_of(const domain::LinkSource& link) const {
    for (const auto& table : preview_.tables)
        for (const auto& column : table.columns)
            if (column.link && *column.link == link && column.key_id) return column.key_id;
    return std::nullopt;
}

std::optional<domain::LinkSource> SchemaView::link_of(domain::ForeignKeyId key) const {
    for (const auto& table : preview_.tables)
        for (const auto& column : table.columns)
            if (column.key_id && *column.key_id == key && column.link) return column.link;
    return std::nullopt;
}

std::optional<SchemaColumnRef> SchemaView::column_ref(std::size_t table, std::size_t row) const {
    if (table >= preview_.tables.size() || row >= preview_.tables[table].columns.size()) return std::nullopt;
    const auto& owner = preview_.tables[table];
    const auto& column = owner.columns[row];
    // Its own identity first, where it has one; then what the schema keeps
    // it by; and last where it was worked out from.
    if (column.added) return SchemaColumnRef{owner.id, *column.added};
    if (column.origin_kind == domain::ColumnOrigin::ForeignKey && column.key_id)
        return SchemaColumnRef{owner.id, domain::ForeignKeyColumn{*column.key_id, column.reference_part}};
    if (column.origin_kind == domain::ColumnOrigin::Generated) return SchemaColumnRef{owner.id, GeneratedKey{}};
    if (column.origin_kind == domain::ColumnOrigin::Discriminator) return SchemaColumnRef{owner.id, Discriminator{}};
    if (column.origin) return SchemaColumnRef{owner.id, WorkedOutFrom{*column.origin}};
    return std::nullopt;
}

std::optional<std::pair<std::size_t, std::size_t>> SchemaView::locate(const SchemaColumnRef& column) const {
    for (std::size_t t = 0; t < preview_.tables.size(); ++t) {
        if (preview_.tables[t].id != column.table) continue;
        for (std::size_t row = 0; row < preview_.tables[t].columns.size(); ++row)
            if (const auto here = column_ref(t, row); here && *here == column) return std::pair{t, row};
    }
    return std::nullopt;
}

SchemaSelection SchemaView::selection_now() const {
    const auto marked = [this](const domain::PreviewTable& table) {
        return table.origin && std::find(selected_.begin(), selected_.end(), *table.origin) != selected_.end();
    };
    if (picked_) {
        if (const auto* column = std::get_if<SchemaColumnRef>(&*picked_)) {
            if (const auto at = locate(*column); at && marked(preview_.tables[at->first]))
                return ChosenColumn{*column};
        } else if (selected_.empty()) {
            return ChosenForeignKey{std::get<domain::ForeignKeyId>(*picked_)};
        }
    }
    // The tables marked, in the order they were marked, each by its own
    // identity rather than by the element it came from.
    std::vector<domain::RelationId> tables;
    for (const auto& ref : selected_)
        for (const auto& table : preview_.tables)
            if (table.origin && *table.origin == ref
                && std::find(tables.begin(), tables.end(), table.id) == tables.end())
                tables.push_back(table.id);
    if (tables.empty()) return NothingChosen{};
    if (tables.size() == 1) return ChosenTable{tables.front()};
    return ChosenTables{std::move(tables)};
}

void SchemaView::choose(const SchemaSelection& wanted) {
    const auto origin_of = [this](domain::RelationId id) -> std::optional<domain::ElementRef> {
        for (const auto& table : preview_.tables)
            if (table.id == id) return table.origin;
        return std::nullopt;
    };
    if (const auto* one = std::get_if<ChosenTable>(&wanted)) {
        select(origin_of(one->table));
    } else if (const auto* several = std::get_if<ChosenTables>(&wanted)) {
        std::vector<domain::ElementRef> marks;
        for (const auto id : several->tables)
            if (const auto origin = origin_of(id)) marks.push_back(*origin);
        selected_ = std::move(marks);
        picked_.reset();
        update();
        if (chose) chose();
    } else if (const auto* column = std::get_if<ChosenColumn>(&wanted)) {
        select(origin_of(column->column.table));
        pick(column->column);
    } else if (const auto* key = std::get_if<ChosenForeignKey>(&wanted)) {
        select(std::nullopt);
        pick(key->key);
    } else {
        select(std::nullopt);
    }
}

// Asked of the same plan, with the same table and row, that letting go asks
// (MainWindow::link_schema_rows): the table and the row are found where the
// pointer is exactly as the release finds them (link_spot), so what is lit is
// what letting go would do. Lines are drawn only on a schema drawn by hand,
// where every table and column is one made by hand, so the plan is the whole
// of the answer.
std::optional<SchemaView::LinkTarget> SchemaView::link_target() const {
    if (!linking_ || !linking_->travelled) return std::nullopt;
    auto spot = link_spot(linking_to_);
    if (!spot) return std::nullopt;
    const auto plan = plan_connection(preview_, linking_->table, linking_->row, spot->table, spot->row);
    spot->takes = plan.kind != ConnectPlan::Kind::Refused;
    return spot;
}

// A row's height, just under the table, where the slot for adding a column
// stands: somewhere a line can be let go on the table itself without finding
// one of its rows, however many it has, or none. Taken from where the table
// is drawn now, so it is wherever the table's foot is.
QRectF SchemaView::drop_strip(std::size_t table) const {
    if (table >= placed_.size()) return {};
    const auto& box = placed_[table].box;
    return QRectF(box.left(), box.bottom() + 2, box.width(), row_height);
}

// A row first, being the most particular thing a line can land on; then the
// strip under a table, which stands for the table; then the rest of a table.
std::optional<SchemaView::LinkTarget> SchemaView::link_spot(QPointF point) const {
    const auto table = table_at(point);
    if (table && *table < preview_.tables.size() && *table < placed_.size())
        if (const auto row = row_at(*table, point);
            row && *row < preview_.tables[*table].columns.size() && *row < placed_[*table].rows.size())
            return LinkTarget{*table, row};
    for (std::size_t t = 0; t < placed_.size() && t < preview_.tables.size(); ++t)
        if (preview_.tables[t].origin && drop_strip(t).contains(point)) return LinkTarget{t, std::nullopt, false, true};
    if (table && *table < preview_.tables.size() && *table < placed_.size()) return LinkTarget{*table};
    return std::nullopt;
}

// The foreign key being drawn: a line from the row's gutter to the pointer,
// in the accent the rest of the application marks what is being done with.
// Where it would land is lit by what letting go there would do (link_target):
// in the theme's colour for what is valid where it would go on -- straight
// away, or after asking -- and in its colour for an error, dashed, where it
// would be turned away; the row it would land on where it is over one, and
// a ring round the table where it is over the rest of it, or over the strip
// under it (drop_strip), which is lit as a row is and ringed with the table.
// Each table's strip is offered faintly, empty and unworded, in the dashes
// of the slot for adding a column, which it stands in for while the line is
// drawn. Over the empty schema nothing is lit. Drawn only while the line is,
// so it cannot outlast it, and drawn apart from what marks a choice: an
// outline of its own on the row, and a ring standing clear of the table's.
void SchemaView::draw_linking(QPainter& painter) const {
    if (!linking_ || !linking_->travelled || !theme_) return;
    if (linking_->table >= placed_.size() || linking_->row >= placed_[linking_->table].rows.size()) return;
    const auto& from_row = placed_[linking_->table].rows[linking_->row];
    const QPointF from(from_row.left() + gutter_width / 2, from_row.center().y());
    {
        auto frame = theme_->muted;
        frame.setAlphaF(0.6f);
        auto wash = theme_->muted;
        wash.setAlphaF(0.07f);
        painter.setPen(QPen(frame, 1.0, Qt::DashLine));
        painter.setBrush(wash);
        for (std::size_t t = 0; t < placed_.size() && t < preview_.tables.size(); ++t)
            if (preview_.tables[t].origin) painter.drawRoundedRect(drop_strip(t), 3, 3);
    }
    if (const auto target = link_target()) {
        const auto ink = target->takes ? theme_->valid : theme_->error;
        const auto stroke = target->takes ? Qt::SolidLine : Qt::DashLine;
        painter.setBrush(Qt::NoBrush);
        const auto light = [&](const QRectF& where) {
            auto wash = ink;
            wash.setAlphaF(0.16f);
            painter.fillRect(where.adjusted(1, 0.5, -1, -0.5), wash);
            auto edge = ink;
            edge.setAlphaF(0.85f);
            painter.setPen(QPen(edge, 1.5, stroke));
            painter.drawRoundedRect(where.adjusted(1.5, 1, -1.5, -1), 3, 3);
        };
        if (target->row) {
            light(placed_[target->table].rows[*target->row]);
        } else {
            auto around = placed_[target->table].box;
            if (target->below) {
                light(drop_strip(target->table));
                around = around.united(drop_strip(target->table));
            }
            const auto ring = around.adjusted(-6, -6, 6, 6);
            auto glow = ink;
            glow.setAlphaF(0.18f);
            painter.setPen(QPen(glow, 8.0));
            painter.drawRoundedRect(ring, 7, 7);
            auto edge = ink;
            edge.setAlphaF(0.9f);
            painter.setPen(QPen(edge, 2.0, stroke));
            painter.drawRoundedRect(ring, 7, 7);
        }
    }
    painter.setPen(QPen(theme_->accent, 2.0, Qt::DashLine, Qt::RoundCap));
    painter.drawLine(from, linking_to_);
    painter.setPen(Qt::NoPen);
    painter.setBrush(theme_->accent);
    painter.drawEllipse(from, 3.5, 3.5);
    painter.setBrush(Qt::NoBrush);
}

void SchemaView::draw_band(QPainter& painter) const {
    if (!band_) return;
    auto wash = theme_->accent;
    wash.setAlphaF(0.14f);
    painter.setBrush(QBrush(wash));
    painter.setPen(QPen(theme_->accent, 1.0, Qt::DashLine));
    painter.drawRect(*band_);
    painter.setBrush(Qt::NoBrush);
}

} // namespace erdflow::desktop
