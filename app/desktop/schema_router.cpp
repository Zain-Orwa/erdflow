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
#include <unordered_map>

namespace erdflow::desktop::schema_router {

namespace {
struct Step { int x; int y; int direction; int cost; };

// For every row, the leftmost and rightmost cell a search took from its queue
// there. What it read is those cells' four neighbours, so the span it read in
// a row is the span taken there widened by one, joined with the spans taken in
// the rows above and below: one note for each cell taken rather than one for
// each cell read.
struct Taken {
    std::vector<int> lo;
    std::vector<int> hi;
    int first = 0;
    int last = -1;
    void reset(int rows) {
        lo.assign(static_cast<std::size_t>(rows), std::numeric_limits<int>::max());
        hi.assign(static_cast<std::size_t>(rows), -1);
        first = rows;
        last = -1;
    }
    void note(int x, int y) {
        auto& l = lo[static_cast<std::size_t>(y)];
        auto& h = hi[static_cast<std::size_t>(y)];
        if (x < l) l = x;
        if (x > h) h = x;
        if (y < first) first = y;
        if (y > last) last = y;
    }
    [[nodiscard]] int lo_at(int y) const {
        return y < first || y > last ? std::numeric_limits<int>::max() : lo[static_cast<std::size_t>(y)];
    }
    [[nodiscard]] int hi_at(int y) const { return y < first || y > last ? -1 : hi[static_cast<std::size_t>(y)]; }
};

// What one line wrote into the grid, in the order it wrote it: each cell it
// marked with how much (1 or 3), and each cell it opened (0); and the box
// round them.
struct Writes {
    std::vector<int> cells; // index * 4 + amount
    int min_x = std::numeric_limits<int>::max();
    int min_y = std::numeric_limits<int>::max();
    int max_x = -1;
    int max_y = -1;
    void add(int index, int amount, int columns) {
        cells.push_back(index * 4 + amount);
        const auto x = index % columns;
        const auto y = index / columns;
        min_x = std::min(min_x, x);
        max_x = std::max(max_x, x);
        min_y = std::min(min_y, y);
        max_y = std::max(max_y, y);
    }
};

// One search kept: the line it was for, the grid's size, the rows it read with
// the span read in each, and the path it found; the pass it was last good in
// and its line's place in that pass's order.
struct Kept {
    std::uint32_t line = 0;
    int columns = 0;
    int rows = 0;
    int first = 0;
    int last = -1;
    int min_x = std::numeric_limits<int>::max();
    int max_x = -1;
    std::vector<std::uint32_t> spans; // lo << 16 | hi a row from `first`; lo > hi reads nothing
    std::vector<QPoint> path;
    std::uint64_t pass = 0;
    std::size_t at = 0;
};

// The order a pass routed its lines in, and what each line wrote.
struct Order {
    std::vector<std::uint32_t> lines;
    std::unordered_map<std::uint32_t, std::size_t> place;
    std::unordered_map<std::uint32_t, Writes> writes;
};

const Writes& writes_of(const Order& order, std::uint32_t line) {
    static const Writes nothing;
    const auto found = order.writes.find(line);
    return found == order.writes.end() ? nothing : found->second;
}
bool reads(const Kept& kept, int index, int columns) {
    const auto x = index % columns;
    const auto y = index / columns;
    if (y < kept.first || y > kept.last || x < kept.min_x || x > kept.max_x) return false;
    const auto span = kept.spans[static_cast<std::size_t>(y - kept.first)];
    return static_cast<int>(span >> 16) <= x && x <= static_cast<int>(span & 0xffff);
}
bool near(const Writes& writes, const Kept& kept) {
    return !(writes.max_x < kept.min_x || writes.min_x > kept.max_x || writes.max_y < kept.first
             || writes.min_y > kept.last);
}
std::uint64_t key_of(QPoint from, QPoint to) {
    return (static_cast<std::uint64_t>(static_cast<std::uint16_t>(from.x())) << 48)
         | (static_cast<std::uint64_t>(static_cast<std::uint16_t>(from.y())) << 32)
         | (static_cast<std::uint64_t>(static_cast<std::uint16_t>(to.x())) << 16)
         | static_cast<std::uint64_t>(static_cast<std::uint16_t>(to.y()));
}
std::size_t size_of(const Kept& kept) {
    return sizeof(Kept) + 16 + kept.spans.capacity() * sizeof(std::uint32_t) + kept.path.capacity() * sizeof(QPoint);
}
} // namespace

struct RouteMemory::State {
    bool checking = false;
    // Whether this pass keeps and takes searches again at all.
    bool keeping = false;
    std::uint64_t pass = 0;
    int columns = 0;
    int rows = 0;
    // The cells the tables blocked at the start of the last pass, and those
    // they block otherwise at the start of this one, with the box round them.
    std::vector<std::uint8_t> tables;
    std::vector<int> moved_tables;
    int moved_min_x = 0, moved_max_x = -1, moved_min_y = 0, moved_max_y = -1;
    Order before;
    Order now;
    std::uint32_t line = 0;
    Writes* writing = nullptr;
    // Over this pass so far: every cell's count without its cap; how far it
    // stands from what the same lines added in the pass before (each line
    // written so far, taken as it was written then); the cells where that is
    // not nothing; the cells some line opened otherwise than before; and the
    // first place at which this pass's order parts from the one before.
    std::vector<int> raw;
    std::vector<std::int16_t> apart;
    std::vector<int> apart_cells;
    std::vector<int> opened_otherwise;
    std::size_t parted = std::numeric_limits<std::size_t>::max();
    std::unordered_map<std::uint64_t, Kept> kept;
    // Working room for put_right in still_good, kept between searches.
    std::vector<std::int16_t> put_right;
    std::vector<int> put_right_cells;
    Taken taken;
    Account account;

    void finish_line();
    [[nodiscard]] bool still_good(const Kept& kept, std::size_t here);
};

void RouteMemory::State::finish_line() {
    if (!writing) return;
    const auto& then = writes_of(before, line);
    if (then.cells != writing->cells) {
        const auto take = [&](const Writes& writes, int sign) {
            for (const auto written : writes.cells) {
                const auto index = written >> 2;
                if ((written & 3) == 0) {
                    opened_otherwise.push_back(index);
                    continue;
                }
                const auto i = static_cast<std::size_t>(index);
                if (apart[i] == 0) apart_cells.push_back(index);
                apart[i] = static_cast<std::int16_t>(apart[i] + sign * (written & 3));
            }
        };
        take(*writing, 1);
        take(then, -1);
    }
    writing = nullptr;
}

// Whether the grid, within what `kept` read, is as it was when it was last
// good -- the pass before, at its line's place then (`kept.at`) -- this line
// being at `here` now. A cell's count then and now differ by what the lines
// written before it now added, less what the lines written before it then
// added: `apart` holds that for the lines written so far, each taken as it was
// written then, and lines written before it now but not then (or then but not
// now) are put right here. The count now is known without its cap, so the
// count then follows, and only a cell whose capped count differs makes the
// search one to make again. A table blocking otherwise, or a line opening a
// cell otherwise, within what was read, is taken as a change.
bool RouteMemory::State::still_good(const Kept& one, std::size_t here) {
    if (moved_max_x >= one.min_x && moved_min_x <= one.max_x && moved_max_y >= one.first && moved_min_y <= one.last)
        for (const auto moved : moved_tables)
            if (reads(one, moved, columns)) return false;
    for (const auto opened : opened_otherwise)
        if (reads(one, opened, columns)) return false;
    if (put_right.size() < apart.size()) put_right.assign(apart.size(), 0);
    put_right_cells.clear();
    bool good = true;
    // Only where the order has parted from the pass before's, or this line
    // stands elsewhere in it, are the two sets of lines written before unlike.
    if (!(one.at == here && here <= parted)) {
        const auto add = [&](const Writes& writes, int sign) {
            if (!near(writes, one)) return;
            for (const auto written : writes.cells) {
                const auto index = written >> 2;
                if (!reads(one, index, columns)) continue;
                if ((written & 3) == 0) {
                    good = false;
                    return;
                }
                const auto i = static_cast<std::size_t>(index);
                if (put_right[i] == 0) put_right_cells.push_back(index);
                put_right[i] = static_cast<std::int16_t>(put_right[i] + sign * (written & 3));
            }
        };
        for (std::size_t p = 0; p < here && p < now.lines.size() && good; ++p) {
            const auto line_now = now.lines[p];
            const auto then_at = before.place.find(line_now);
            if (then_at == before.place.end() || then_at->second >= one.at) add(writes_of(before, line_now), 1);
        }
        for (std::size_t p = 0; p < one.at && p < before.lines.size() && good; ++p) {
            const auto line_then = before.lines[p];
            const auto now_at = now.place.find(line_then);
            if (now_at == now.place.end() || now_at->second >= here) add(writes_of(before, line_then), -1);
        }
    }
    const auto same_count = [&](int index) {
        const auto i = static_cast<std::size_t>(index);
        const int difference = apart[i] + put_right[i];
        if (difference == 0) return true;
        const int count = raw[i];
        return std::min(8, count) == std::min(8, count - difference);
    };
    for (std::size_t k = 0; k < apart_cells.size() && good; ++k) {
        const auto index = apart_cells[k];
        if (reads(one, index, columns) && !same_count(index)) good = false;
    }
    for (const auto index : put_right_cells) {
        if (good && reads(one, index, columns) && !same_count(index)) good = false;
        put_right[static_cast<std::size_t>(index)] = 0;
    }
    return good;
}

RouteMemory::RouteMemory() : state_(std::make_unique<State>()) {}
RouteMemory::~RouteMemory() = default;

void RouteMemory::set_checking(bool on) { state_->checking = on; }
const RouteMemory::Account& RouteMemory::account() const { return state_->account; }

RouteMemory::Pass::Pass(RouteMemory* memory, const Grid& grid, bool everything_moves) : memory_(memory) {
    if (!memory_) return;
    auto& s = *memory_->state_;
    ++s.pass;
    s.now = Order{};
    s.writing = nullptr;
    s.moved_tables.clear();
    s.moved_min_x = s.moved_min_y = std::numeric_limits<int>::max();
    s.moved_max_x = s.moved_max_y = -1;
    s.keeping = !everything_moves;
    if (!s.keeping || grid.columns_ != s.columns || grid.rows_ != s.rows) {
        s.kept.clear();
        s.before = Order{};
    } else {
        for (std::size_t i = 0; i < grid.blocked_.size(); ++i)
            if (grid.blocked_[i] != s.tables[i]) {
                const auto index = static_cast<int>(i);
                s.moved_tables.push_back(index);
                s.moved_min_x = std::min(s.moved_min_x, index % grid.columns_);
                s.moved_max_x = std::max(s.moved_max_x, index % grid.columns_);
                s.moved_min_y = std::min(s.moved_min_y, index / grid.columns_);
                s.moved_max_y = std::max(s.moved_max_y, index / grid.columns_);
            }
    }
    s.columns = grid.columns_;
    s.rows = grid.rows_;
    s.tables = grid.blocked_;
    s.raw.assign(grid.used_.size(), 0);
    s.apart.assign(grid.used_.size(), 0);
    s.apart_cells.clear();
    s.opened_otherwise.clear();
    s.parted = std::numeric_limits<std::size_t>::max();
}

void RouteMemory::Pass::line(std::uint32_t id) {
    if (!memory_) return;
    auto& s = *memory_->state_;
    if (!s.keeping) return;
    s.finish_line();
    s.line = id;
    const auto at = s.now.lines.size();
    if (s.parted == std::numeric_limits<std::size_t>::max() && (at >= s.before.lines.size() || s.before.lines[at] != id))
        s.parted = at;
    s.now.place[id] = at;
    s.now.lines.push_back(id);
    s.writing = &s.now.writes[id];
}

RouteMemory::Pass::~Pass() {
    if (!memory_) return;
    auto& s = *memory_->state_;
    if (!s.keeping) {
        s.account.kept = 0;
        s.account.bytes = 0;
        return;
    }
    s.finish_line();
    // What this pass did not ask for is let go of: a table moved has new ends
    // for its lines, and their old searches will not be asked for again.
    std::size_t bytes = 0;
    for (auto it = s.kept.begin(); it != s.kept.end();) {
        if (it->second.pass != s.pass) {
            it = s.kept.erase(it);
        } else {
            bytes += size_of(it->second);
            ++it;
        }
    }
    for (const auto& [line, writes] : s.now.writes) {
        (void)line;
        bytes += 64 + writes.cells.capacity() * sizeof(int);
    }
    bytes += s.tables.capacity() + s.moved_tables.capacity() * sizeof(int) + s.raw.capacity() * sizeof(int)
           + s.apart.capacity() * sizeof(std::int16_t) + s.apart_cells.capacity() * sizeof(int)
           + s.opened_otherwise.capacity() * sizeof(int) + s.put_right.capacity() * sizeof(std::int16_t);
    s.account.kept = s.kept.size();
    s.account.bytes = bytes;
    s.account.most_bytes = std::max(s.account.most_bytes, bytes);
    s.before = std::move(s.now);
    s.now = Order{};
}

Grid::Grid(int columns, int rows, const std::vector<QRectF>& tables, RouteMemory* memory)
    : columns_(columns), rows_(rows), memory_(memory) {
    blocked_.assign(static_cast<std::size_t>(columns_) * static_cast<std::size_t>(rows_), 0);
    used_.assign(blocked_.size(), 0);
    for (const auto& table : tables) {
        const auto box = table.adjusted(-clearance, -clearance, clearance, clearance);
        for (int y = std::max(0, static_cast<int>(box.top()) / cell);
             y < std::min(rows_, static_cast<int>(box.bottom()) / cell + 1); ++y)
            for (int x = std::max(0, static_cast<int>(box.left()) / cell);
                 x < std::min(columns_, static_cast<int>(box.right()) / cell + 1); ++x)
                blocked_[static_cast<std::size_t>(at(x, y))] = 1;
    }
}

// A* over the grid. Returns the cells walked, or nothing where no way could be
// found -- which the caller answers with a plain elbow rather than no line.
// Made twice: as the router has always made it, and noting the cells it takes
// for a RouteMemory to keep.
struct Searching {
    template <bool noting>
    static std::vector<QPoint> run(Grid& grid, QPoint from, QPoint to, Taken* taken) {
        const auto cells = static_cast<std::size_t>(grid.columns_) * static_cast<std::size_t>(grid.rows_);
        auto& best = grid.best_;
        auto& came = grid.came_;
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
            if constexpr (noting) taken->note(here.x, here.y);
            if (here.x == to.x() && here.y == to.y()) {
                std::vector<QPoint> path;
                auto at = grid.at(here.x, here.y);
                while (at >= 0) {
                    path.push_back(QPoint(at % grid.columns_, at / grid.columns_));
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
                if (grid.blocked_[index]) continue;
                const auto step = 1 + (here.direction != -1 && here.direction != m ? turn_cost : 0)
                                + grid.used_[index] * share_cost;
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
};

void open_cell(Grid& grid, QPoint at) {
    const auto index = grid.at(at.x(), at.y());
    auto& blocked = grid.blocked_[static_cast<std::size_t>(index)];
    if (!blocked) return;
    blocked = 0;
    if (!grid.memory_) return;
    auto& s = *grid.memory_->state_;
    if (s.keeping && s.writing) s.writing->add(index, 0, grid.columns_);
}

std::vector<QPoint> find_way(Grid& grid, QPoint from, QPoint to) {
    if (!grid.inside(from.x(), from.y()) || !grid.inside(to.x(), to.y())) return {};
    // What the line had written before this search, its own two ends aside: a
    // search kept is checked against what the lines before it wrote, so one
    // made after its own line has written is neither kept nor taken again.
    // The router never does that -- a trial puts back what it opened, and a
    // line's ends are opened for good only after its last search -- but the
    // memory does not rely on it.
    const auto* writing = grid.memory_ ? grid.memory_->state_->writing : nullptr;
    const bool written_before = writing && !writing->cells.empty();
    // Both ends sit against their own tables, so their cells are let through.
    open_cell(grid, from);
    open_cell(grid, to);
    if (!grid.memory_ || !grid.memory_->state_->keeping || written_before)
        return Searching::run<false>(grid, from, to, nullptr);

    auto& s = *grid.memory_->state_;
    const auto key = key_of(from, to);
    const auto here = s.now.lines.empty() ? 0 : s.now.lines.size() - 1;
    const auto found = s.kept.find(key);
    if (found != s.kept.end()) {
        auto& one = found->second;
        const bool same_grid = one.columns == grid.columns_ && one.rows == grid.rows_ && one.line == s.line;
        // Asked again for the same line in the same pass, nothing has been
        // written since: a trial puts back what it opened.
        const bool good = same_grid
            && ((one.pass == s.pass && one.at == here) || (one.pass + 1 == s.pass && s.still_good(one, here)));
        if (good) {
            ++s.account.taken_again;
            one.pass = s.pass;
            one.at = here;
            if (s.checking && Searching::run<false>(grid, from, to, nullptr) != one.path) ++s.account.mismatches;
            return one.path;
        }
        ++s.account.outdated;
    }
    ++s.account.searched;
    s.taken.reset(grid.rows_);
    s.taken.note(from.x(), from.y());
    s.taken.note(to.x(), to.y());
    auto path = Searching::run<true>(grid, from, to, &s.taken);

    Kept one;
    one.line = s.line;
    one.columns = grid.columns_;
    one.rows = grid.rows_;
    // Rows read: one above the first taken to one below the last, within the grid.
    one.first = std::max(0, s.taken.first - 1);
    one.last = std::min(grid.rows_ - 1, s.taken.last + 1);
    one.spans.reserve(static_cast<std::size_t>(std::max(0, one.last - one.first + 1)));
    for (int y = one.first; y <= one.last; ++y) {
        const auto lo_here = s.taken.lo_at(y);
        const auto hi_here = s.taken.hi_at(y);
        auto lo = std::min({lo_here == std::numeric_limits<int>::max() ? lo_here : lo_here - 1, s.taken.lo_at(y - 1),
                            s.taken.lo_at(y + 1)});
        auto hi = std::max({hi_here < 0 ? -1 : hi_here + 1, s.taken.hi_at(y - 1), s.taken.hi_at(y + 1)});
        lo = std::max(lo, 0);
        hi = std::min(hi, grid.columns_ - 1);
        if (hi < lo) {
            one.spans.push_back(1u << 16); // lo 1, hi 0: nothing read in this row
            continue;
        }
        one.min_x = std::min(one.min_x, lo);
        one.max_x = std::max(one.max_x, hi);
        one.spans.push_back((static_cast<std::uint32_t>(lo) << 16) | static_cast<std::uint32_t>(hi));
    }
    one.path = path;
    one.pass = s.pass;
    one.at = here;
    s.kept[key] = std::move(one);
    return path;
}

std::vector<QPoint> find_way_trial(Grid& grid, QPoint from, QPoint to) {
    // find_way opens its terminal cells; probes must not leave
    // holes in the obstacle map for subsequent alternatives. Those two cells
    // are all it opens, so they are all that is put back (2026-10-12): a copy
    // of the whole map for every probe was most of what a probe cost beyond
    // its search.
    if (!grid.inside(from.x(), from.y()) || !grid.inside(to.x(), to.y())) return {};
    const auto start = static_cast<std::size_t>(grid.at(from.x(), from.y()));
    const auto finish = static_cast<std::size_t>(grid.at(to.x(), to.y()));
    const auto start_was = grid.blocked_[start];
    const auto finish_was = grid.blocked_[finish];
    auto* writing = grid.memory_ ? grid.memory_->state_->writing : nullptr;
    const auto written = writing ? writing->cells.size() : 0;
    auto path = find_way(grid, from, to);
    grid.blocked_[finish] = finish_was;
    grid.blocked_[start] = start_was;
    // What a trial opened it has put back, so for the line it was never written.
    if (writing) writing->cells.resize(written);
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
    auto* s = grid.memory_ && grid.memory_->state_->keeping ? grid.memory_->state_.get() : nullptr;
    const auto touch = [&](int x, int y) {
        for (int away = -1; away <= 1; ++away) {
            if (!grid.inside(x, y + away)) continue;
            const auto index = grid.at(x, y + away);
            const auto amount = away == 0 ? 3 : 1;
            auto& count = grid.used_[static_cast<std::size_t>(index)];
            count = static_cast<std::uint16_t>(std::min(8, count + amount));
            if (s && s->writing) {
                s->writing->add(index, amount, grid.columns_);
                s->raw[static_cast<std::size_t>(index)] += amount;
            }
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
