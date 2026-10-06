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

#include <cstdint>
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

// The cells walked from one cell to the other, or nothing where there is no way.
std::vector<QPoint> find_way(Grid& grid, QPoint from, QPoint to);
// The same search tried for one pair of sides a line might leave and arrive
// by, which leaves the grid's obstacles exactly as it found them.
std::vector<QPoint> find_way_trial(Grid& grid, QPoint from, QPoint to);
// Say where a route has been, so the next one is nudged off it.
void mark(Grid& grid, const std::vector<QPointF>& corners);
// A route's corners as the path it is drawn along.
QPainterPath path_of(const std::vector<QPointF>& corners);

} // namespace erdflow::desktop::schema_router
