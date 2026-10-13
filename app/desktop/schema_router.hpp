// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include <QPainterPath>
#include <QPoint>
#include <QPointF>
#include <QRectF>

#include <cstdint>
#include <memory>
#include <vector>

// How the Relational Schema's lines find their way between its tables. The
// schema decides where each line leaves and arrives and routes them one by one
// (SchemaView::reroute); this is the searching itself.
//
// It is a file of its own so that it can be compiled optimised even in a Debug
// build (CMakeLists.txt). Routing every line is the costliest thing a drag
// does, and unoptimised the same search, finding exactly the same routes, is
// some fourteen times slower (2026-10-06).
namespace erdflow::desktop::schema_router {

// The router works over a coarse grid: the tables are blocked out, and a line
// finds its way between them. Turning costs more than going straight, so runs
// stay long; a cell another line has already used costs more still, which is
// what keeps two lines going the same way apart without a rule about lanes.
inline constexpr int cell = 9;
inline constexpr int clearance = 6;
inline constexpr int turn_cost = 4;
inline constexpr int share_cost = 7;

class RouteMemory;
struct Searching;

// The grid a pass of routing works over. The tables are blocked out when it
// is made, each with its clearance round it; after that its cells are written
// only by the router -- a line's search opening its two end cells, a trial
// putting them back, a route being marked -- so whatever remembers searches
// for a view (RouteMemory) sees every write, and nothing can change the grid
// behind it. Anything may read it.
class Grid {
public:
    Grid(int columns, int rows, const std::vector<QRectF>& tables, RouteMemory* memory = nullptr);

    [[nodiscard]] int columns() const { return columns_; }
    [[nodiscard]] int rows() const { return rows_; }
    [[nodiscard]] int at(int x, int y) const { return y * columns_ + x; }
    [[nodiscard]] bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < columns_ && y < rows_; }
    [[nodiscard]] bool blocked(int x, int y) const { return blocked_[static_cast<std::size_t>(at(x, y))] != 0; }
    [[nodiscard]] int used(int x, int y) const { return used_[static_cast<std::size_t>(at(x, y))]; }

private:
    friend struct Searching;
    friend class RouteMemory;
    friend std::vector<QPoint> find_way(Grid& grid, QPoint from, QPoint to);
    friend std::vector<QPoint> find_way_trial(Grid& grid, QPoint from, QPoint to);
    friend void open_cell(Grid& grid, QPoint at);
    friend void mark(Grid& grid, const std::vector<QPointF>& corners);

    int columns_ = 0;
    int rows_ = 0;
    std::vector<std::uint8_t> blocked_;
    std::vector<std::uint16_t> used_;
    // The search's own working room, kept with the grid and cleared between
    // lines rather than allocated afresh for each. Every line on the schema is
    // routed again whenever a table is dragged, and two vectors the size of the
    // whole grid per line per frame is a cost paid while the hand is moving.
    std::vector<int> best_;
    std::vector<int> came_;
    RouteMemory* memory_ = nullptr;
};

// The cells walked from one cell to the other, or nothing where there is no way.
std::vector<QPoint> find_way(Grid& grid, QPoint from, QPoint to);
// The same search tried for one pair of sides a line might leave and arrive
// by, which leaves the grid's obstacles exactly as it found them.
std::vector<QPoint> find_way_trial(Grid& grid, QPoint from, QPoint to);
// A line's end cell let through, as its search lets its own ends through.
void open_cell(Grid& grid, QPoint at);
// Say where a route has been, so the next one is nudged off it.
void mark(Grid& grid, const std::vector<QPointF>& corners);
// A route's corners as the path it is drawn along.
QPainterPath path_of(const std::vector<QPointF>& corners);

// The searches a view's last pass of routing made, kept so that the next pass
// can take a search's path again rather than search again, wherever doing so
// gives exactly the path the search would find (2026-10-13).
//
// A search reads only the grid, and within one pass the grid is made by the
// tables and by the lines routed before: each may open its two end cells and
// marks the cells along its route, a cell's count capped at 8 (so the order of
// marking makes no difference). A search from the pass before is taken again
// only for the same line, from the same cells, on a grid of the same size, and
// only if, within the cells it read, no table blocks otherwise, no line opened
// a cell otherwise, and every cell's capped count is what it was -- worked out
// exactly from what every line wrote, then and now. Nothing is compared by
// digest. Searches not asked for in a pass are let go of, so what is kept is
// never more than one pass's searches.
//
// One belongs to each view (SchemaView), for as long as the view.
class RouteMemory {
public:
    RouteMemory();
    ~RouteMemory();
    RouteMemory(const RouteMemory&) = delete;
    RouteMemory& operator=(const RouteMemory&) = delete;

    // One pass of routing over every line, begun once its grid is made and
    // ended when it goes out of scope; each line is said as it begins. A pass
    // in which every table is being moved keeps nothing and takes nothing
    // again: no search could find its grid as it was. A pass with no memory
    // does nothing.
    class Pass {
    public:
        Pass(RouteMemory* memory, const Grid& grid, bool everything_moves);
        ~Pass();
        Pass(const Pass&) = delete;
        Pass& operator=(const Pass&) = delete;
        void line(std::uint32_t id);

    private:
        RouteMemory* memory_;
    };

    // Every search taken again is also made afresh, and a difference counted
    // as a mismatch: for tests, which then require there to be none.
    void set_checking(bool on);
    struct Account {
        std::size_t taken_again = 0;
        std::size_t searched = 0;
        std::size_t outdated = 0;   // of those searched, kept but no longer good
        std::size_t mismatches = 0; // only counted while checking
        std::size_t kept = 0;       // searches kept after the last pass
        std::size_t bytes = 0;      // what is kept, after the last pass
        std::size_t most_bytes = 0;
    };
    [[nodiscard]] const Account& account() const;

private:
    friend struct Searching;
    friend std::vector<QPoint> find_way(Grid& grid, QPoint from, QPoint to);
    friend std::vector<QPoint> find_way_trial(Grid& grid, QPoint from, QPoint to);
    friend void open_cell(Grid& grid, QPoint at);
    friend void mark(Grid& grid, const std::vector<QPointF>& corners);
    struct State;
    std::unique_ptr<State> state_;
};

} // namespace erdflow::desktop::schema_router
