// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "schema_router.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <queue>

namespace erdflow::desktop::schema_router {

namespace {
struct Step { int x; int y; int direction; int cost; };
} // namespace

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

std::vector<QPoint> find_way_trial(Grid& grid, QPoint from, QPoint to) {
    // find_way opens its terminal cells; probes must not leave
    // holes in the obstacle map for subsequent alternatives.
    const auto blocked = grid.blocked;
    auto path = find_way(grid, from, to);
    grid.blocked = blocked;
    return path;
}

QPainterPath path_of(const std::vector<QPointF>& corners) {
    if (corners.empty()) return {};
    QPainterPath path(corners.front());
    for (std::size_t i = 1; i < corners.size(); ++i) path.lineTo(corners[i]);
    return path;
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

} // namespace erdflow::desktop::schema_router
