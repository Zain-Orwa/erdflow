// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "schema_colours.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <set>

namespace erdflow::desktop::schema_colours {

namespace {

// Sixteen colours as far apart from one another as colours can be while every
// one stands out from its canvas at least three to one, and is neither so dark
// on a light canvas that it reads as black nor so pale on a dark one that it
// reads as white (lightness 26-61 on light, 49-85 on dark). Chosen by search
// (2026-10-13): no two of the light ones are nearer than 22.1 apart, and no
// two of the dark ones nearer than 22.3.
const std::vector<QColor>& light() {
    static const std::vector<QColor> colours{
        QColor("#a60073"), QColor("#4ca600"), QColor("#e67300"), QColor("#0d99bf"), QColor("#404000"),
        QColor("#800d1a"), QColor("#cc738c"), QColor("#005999"), QColor("#006640"), QColor("#a68c00"),
        QColor("#8c8ccc"), QColor("#00a68c"), QColor("#f200ff"), QColor("#f20d26"), QColor("#8c4c00"),
        QColor("#660dff")};
    return colours;
}
const std::vector<QColor>& dark() {
    static const std::vector<QColor> colours{
        QColor("#8cd900"), QColor("#8059e6"), QColor("#ffa666"), QColor("#33d9ff"), QColor("#ffa6b2"),
        QColor("#0d80b2"), QColor("#f2cc33"), QColor("#ff1a40"), QColor("#99800d"), QColor("#00998c"),
        QColor("#99b2ff"), QColor("#8ce6bf"), QColor("#d91a8c"), QColor("#0d8c00"), QColor("#f280ff"),
        QColor("#bf5900")};
    return colours;
}
// A theme with no colour draws its lines in greys only (Zain chose a grey for
// each line over one grey for all or dash patterns, 2026-09-24). Greys differ
// in lightness alone, and between near-black and the palest grey that still
// stands out from white three to one there is room for only so many that can
// be told apart: these five are 8.4 apart, as far as five can be. So the greys
// are shared out like the colours -- every relationship at a table a grey of
// its own while there are greys enough, evenly where there are not -- rather
// than each line taking one of many greys too near one another to tell apart.
const std::vector<QColor>& greys() {
    static const std::vector<QColor> colours{QColor("#1f1f1f"), QColor("#3a3a3a"), QColor("#565656"),
                                             QColor("#757575"), QColor("#949494")};
    return colours;
}

std::uint64_t fnv(const std::string& text) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto c : text) {
        hash ^= static_cast<unsigned char>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

struct Lab {
    double l, a, b;
};
Lab lab(const QColor& colour) {
    const auto linear = [](double v) { return v <= 0.04045 ? v / 12.92 : std::pow((v + 0.055) / 1.055, 2.4); };
    const double r = linear(colour.redF()), g = linear(colour.greenF()), b = linear(colour.blueF());
    const double x = (0.4124 * r + 0.3576 * g + 0.1805 * b) / 0.95047;
    const double y = 0.2126 * r + 0.7152 * g + 0.0722 * b;
    const double z = (0.0193 * r + 0.1192 * g + 0.9505 * b) / 1.08883;
    const auto f = [](double t) { return t > 0.008856 ? std::cbrt(t) : 7.787 * t + 16.0 / 116; };
    return {116 * f(y) - 16, 500 * (f(x) - f(y)), 200 * (f(y) - f(z))};
}

// The distances between every two colours of a palette, worked out once.
const std::vector<std::vector<double>>& distances(Palette palette) {
    static std::array<std::vector<std::vector<double>>, 3> worked;
    auto& table = worked[static_cast<std::size_t>(palette)];
    if (table.empty()) {
        const auto& colours = colours_of(palette);
        table.assign(colours.size(), std::vector<double>(colours.size(), 0));
        for (std::size_t i = 0; i < colours.size(); ++i)
            for (std::size_t j = 0; j < colours.size(); ++j) table[i][j] = distance(colours[i], colours[j]);
    }
    return table;
}

// Which colours of a palette are of one family, worked out once.
const std::vector<std::vector<int>>& families(Palette palette) {
    static std::array<std::vector<std::vector<int>>, 3> worked;
    auto& table = worked[static_cast<std::size_t>(palette)];
    if (table.empty()) {
        const auto& colours = colours_of(palette);
        table.assign(colours.size(), std::vector<int>(colours.size(), 0));
        for (std::size_t i = 0; i < colours.size(); ++i)
            for (std::size_t j = 0; j < colours.size(); ++j) table[i][j] = kindred(colours[i], colours[j]) ? 1 : 0;
    }
    return table;
}

// Far enough apart that any two such colours are plainly different.
constexpr double plainly_apart = 20.0;

// Lines meeting at one row meet at one point, a few pixels apart at most, so
// plainly different is not enough there: two colours 22 apart, a dark and a
// light orange, are told apart side by side but not where their ends crowd
// together. 30 is as far apart as the sixteen colours can keep the six or so
// relationships of a busy key (2026-10-13: 32 for seven in light, 28 in
// dark); asking for more would set every relationship at a row chasing the
// farthest colour, and the relationships at its tables would no longer be
// kept plainly apart.
constexpr double apart_at_a_row = 30.0;

// How far a colour has to be from those a relationship meets to be as good as
// one farther: plainly apart, or, in a palette whose colours are not all that
// far apart -- the greys -- any colour of the palette other than theirs. Taking
// the farthest grey instead made every choice hang on every neighbour's, and
// one relationship added recoloured up to 33 others across a schema; taking the
// nearest free one to its own, up to 12 (2026-10-13, 400 schemas measured).
double good_enough(Palette palette) {
    const auto& table = distances(palette);
    double closest = plainly_apart;
    for (std::size_t i = 0; i < table.size(); ++i)
        for (std::size_t j = i + 1; j < table.size(); ++j) closest = std::min(closest, table[i][j]);
    return closest;
}

// And at a row: as far apart as a row asks, or, in the greys, as at a table
// -- any grey but theirs -- for the reason above.
double good_enough_at_a_row(Palette palette) {
    const auto at_a_table = good_enough(palette);
    return at_a_table < plainly_apart ? at_a_table : apart_at_a_row;
}

} // namespace

const std::vector<QColor>& colours_of(Palette palette) {
    switch (palette) {
    case Palette::Light: return light();
    case Palette::Dark: return dark();
    case Palette::Grey: return greys();
    }
    return light();
}

double distance(const QColor& one, const QColor& other) {
    const auto p = lab(one), q = lab(other);
    constexpr double rad = 3.14159265358979323846 / 180;
    const double c1 = std::hypot(p.a, p.b), c2 = std::hypot(q.a, q.b), cb = (c1 + c2) / 2;
    const double g = 0.5 * (1 - std::sqrt(std::pow(cb, 7) / (std::pow(cb, 7) + std::pow(25.0, 7))));
    const double a1 = (1 + g) * p.a, a2 = (1 + g) * q.a;
    const double c1p = std::hypot(a1, p.b), c2p = std::hypot(a2, q.b);
    double h1 = std::atan2(p.b, a1) / rad, h2 = std::atan2(q.b, a2) / rad;
    if (h1 < 0) h1 += 360;
    if (h2 < 0) h2 += 360;
    double dh = h2 - h1;
    if (c1p * c2p == 0) dh = 0;
    else if (dh > 180) dh -= 360;
    else if (dh < -180) dh += 360;
    const double dl = q.l - p.l, dc = c2p - c1p, dhh = 2 * std::sqrt(c1p * c2p) * std::sin(dh / 2 * rad);
    const double lb = (p.l + q.l) / 2, cbp = (c1p + c2p) / 2;
    double hb = h1 + h2;
    if (c1p * c2p != 0) {
        if (std::abs(h1 - h2) > 180) hb += hb < 360 ? 360 : -360;
        hb /= 2;
    }
    const double t = 1 - 0.17 * std::cos((hb - 30) * rad) + 0.24 * std::cos(2 * hb * rad)
                   + 0.32 * std::cos((3 * hb + 6) * rad) - 0.20 * std::cos((4 * hb - 63) * rad);
    const double dth = 30 * std::exp(-std::pow((hb - 275) / 25, 2));
    const double rc = 2 * std::sqrt(std::pow(cbp, 7) / (std::pow(cbp, 7) + std::pow(25.0, 7)));
    const double sl = 1 + 0.015 * std::pow(lb - 50, 2) / std::sqrt(20 + std::pow(lb - 50, 2));
    const double sc = 1 + 0.045 * cbp, sh = 1 + 0.015 * cbp * t, rt = -std::sin(2 * dth * rad) * rc;
    return std::sqrt(std::pow(dl / sl, 2) + std::pow(dc / sc, 2) + std::pow(dhh / sh, 2) + rt * (dc / sc) * (dhh / sh));
}

bool kindred(const QColor& one, const QColor& other) {
    const auto p = lab(one), q = lab(other);
    // Below this a colour reads as a grey, whatever its hue.
    constexpr double coloured = 20;
    if (std::hypot(p.a, p.b) < coloured || std::hypot(q.a, q.b) < coloured) return false;
    constexpr double degrees = 180 / 3.14159265358979323846;
    auto hues = std::abs(std::atan2(p.b, p.a) - std::atan2(q.b, q.a)) * degrees;
    if (hues > 180) hues = 360 - hues;
    return hues < 25;
}

std::map<std::string, QColor> allocate(const std::vector<Line>& lines, Palette palette) {
    const auto& colours = colours_of(palette);
    const auto& apart = distances(palette);
    const auto& kin = families(palette);
    const auto enough = good_enough(palette);
    const auto enough_at_a_row = good_enough_at_a_row(palette);
    const auto count = static_cast<int>(colours.size());

    // Every relationship, the tables it meets, all its lines together, and
    // what it says (its name where it has one, otherwise its identity); and
    // the rows its lines say they meet.
    using Row = std::pair<std::size_t, std::size_t>;
    std::map<std::string, std::set<std::size_t>> tables_of;
    std::map<std::string, std::set<Row>> rows_of;
    std::map<std::string, std::string> said;
    for (const auto& line : lines) {
        tables_of[line.identity].insert(line.from_table);
        tables_of[line.identity].insert(line.to_table);
        auto& rows = rows_of[line.identity];
        if (line.from_column != no_column) rows.insert({line.from_table, line.from_column});
        if (line.to_column != no_column) rows.insert({line.to_table, line.to_column});
        said.emplace(line.identity, line.name.empty() ? line.identity : line.name);
    }
    // Taken in an order that depends on what they say alone.
    struct Taken {
        std::uint64_t hash;
        const std::string* says;
        const std::string* identity;
    };
    std::vector<Taken> order;
    order.reserve(tables_of.size());
    for (const auto& [identity, tables] : tables_of) {
        (void)tables;
        const auto& says = said.at(identity);
        order.push_back({fnv(says), &says, &identity});
    }
    std::sort(order.begin(), order.end(), [](const Taken& a, const Taken& b) {
        return std::tie(a.hash, *a.says, *a.identity) < std::tie(b.hash, *b.says, *b.identity);
    });

    // How many relationships meet at each table and at each row, so each
    // one's even share of a colour is known before any is given.
    std::map<std::size_t, int> meeting;
    std::map<Row, int> meeting_row;
    for (const auto& [identity, tables] : tables_of) {
        for (const auto table : tables) ++meeting[table];
        for (const auto& row : rows_of.at(identity)) ++meeting_row[row];
    }
    std::map<std::size_t, std::vector<int>> given_at;
    std::map<Row, std::vector<int>> given_row;
    std::map<std::string, int> chosen_of;
    std::vector<int> met, met_row;
    for (const auto& [hash, says, identity] : order) {
        (void)says;
        const auto own = static_cast<int>(hash % static_cast<std::uint64_t>(count));
        // The colours of the relationships already taken that meet it, and
        // of those among them that meet it at one of its rows.
        met.clear();
        for (const auto table : tables_of.at(*identity)) {
            const auto found = given_at.find(table);
            if (found != given_at.end()) met.insert(met.end(), found->second.begin(), found->second.end());
        }
        met_row.clear();
        const auto& rows = rows_of.at(*identity);
        for (const auto& row : rows) {
            const auto found = given_row.find(row);
            if (found != given_row.end()) met_row.insert(met_row.end(), found->second.begin(), found->second.end());
        }
        int chosen = own;
        if (!met.empty()) {
            // Of every colour, how near it comes to those met (far enough
            // counting as well apart as any); how far past its even share
            // taking it would put the more crowded of its tables -- a table
            // met by n relationships has an even share of n over the size of
            // the palette, rounded up, of each colour; how often the table
            // that has it most already has it; how many of those met have it;
            // and how far it is from its own in the palette. The best is the
            // farthest from those met, then the least past an even share, then
            // the least at any one table, then the least used, then the
            // nearest its own, then the first. Weighing each table against its
            // own share, rather than counting uses across both ends together,
            // is what shares a colour out evenly where a table has more
            // relationships than the palette has colours: counted together, a
            // relationship would take a third use at a crowded table to spare
            // its quiet other table a first (2026-10-13: Employee in Plain had
            // one grey three times and another once, now each twice).
            //
            // Ahead of all of it come the relationships meeting it at one of
            // its rows (C3c-2, 2026-10-14), where the ends of their lines
            // crowd into one point: how many of them have a colour of its
            // family, how near it comes to theirs (as far apart as a row asks
            // counting as well apart as any), and how far past its even share
            // it would put the more crowded of its rows. Families are weighed
            // first because two of one family can be farther apart than 30 --
            // a dark olive and a mustard -- and still read as one colour where
            // their ends meet.
            auto best = std::make_tuple(std::numeric_limits<int>::min(), -1.0, std::numeric_limits<int>::min(), -1.0,
                                        std::numeric_limits<int>::min(), std::numeric_limits<int>::min(),
                                        std::numeric_limits<int>::min(), std::numeric_limits<int>::min(), 0);
            for (int c = 0; c < count; ++c) {
                const auto at = static_cast<std::size_t>(c);
                int kindred_there = 0;
                double nearest_there = enough_at_a_row;
                for (const auto m : met_row) {
                    kindred_there += kin[at][static_cast<std::size_t>(m)];
                    nearest_there = std::min(nearest_there, apart[at][static_cast<std::size_t>(m)]);
                }
                int past_there = rows.empty() ? 0 : std::numeric_limits<int>::min();
                for (const auto& row : rows) {
                    const auto found = given_row.find(row);
                    const auto has = found == given_row.end()
                        ? 0 : static_cast<int>(std::count(found->second.begin(), found->second.end(), c));
                    past_there = std::max(past_there, has + 1 - (meeting_row.at(row) + count - 1) / count);
                }
                double nearest = enough;
                int uses = 0;
                for (const auto m : met) {
                    nearest = std::min(nearest, apart[static_cast<std::size_t>(c)][static_cast<std::size_t>(m)]);
                    uses += m == c ? 1 : 0;
                }
                int past = std::numeric_limits<int>::min();
                int most = 0;
                for (const auto table : tables_of.at(*identity)) {
                    const auto found = given_at.find(table);
                    const auto has = found == given_at.end()
                        ? 0 : static_cast<int>(std::count(found->second.begin(), found->second.end(), c));
                    past = std::max(past, has + 1 - (meeting.at(table) + count - 1) / count);
                    most = std::max(most, has);
                }
                const auto step = std::abs(c - own);
                const auto score = std::make_tuple(-kindred_there, nearest_there, -past_there, nearest, -past, -most,
                                                   -uses, -std::min(step, count - step), -c);
                if (score > best) {
                    best = score;
                    chosen = c;
                }
            }
        }
        chosen_of.emplace(*identity, chosen);
        for (const auto table : tables_of.at(*identity)) given_at[table].push_back(chosen);
        for (const auto& row : rows) given_row[row].push_back(chosen);
    }

    // Taken one at a time, a relationship knows only those taken before it,
    // and a choice that was right then can leave one taken later with nothing
    // good: two relationships at a quiet table sharing a grey because the
    // busy table at the other end had none left (2026-10-13: Programs in the
    // University example). So each is looked at again, in the same order,
    // knowing them all, and moves to another colour only where that is
    // plainly better -- at its rows first, then at its tables: farther from
    // those it meets, or less past an even share, or less used -- never
    // merely to be nearer its own. Where every
    // table is already plainly apart nothing is better, and nothing moves. Of
    // 400 made-up schemas, 271 had a table given a grey more often than its
    // even share when uses were counted across both ends together, 210 when
    // each table was weighed against its own share, and 120 with this second
    // look.
    std::map<std::size_t, std::vector<int>> uses_at;
    for (const auto& [table, chosen] : given_at) {
        auto& counted = uses_at[table];
        counted.assign(static_cast<std::size_t>(count), 0);
        for (const auto c : chosen) ++counted[static_cast<std::size_t>(c)];
    }
    std::map<Row, std::vector<int>> uses_row;
    for (const auto& [row, chosen] : given_row) {
        auto& counted = uses_row[row];
        counted.assign(static_cast<std::size_t>(count), 0);
        for (const auto c : chosen) ++counted[static_cast<std::size_t>(c)];
    }
    // Weighed as the first time round: its rows first, then its tables.
    const auto weigh = [&](const std::set<std::size_t>& tables, const std::set<Row>& rows, int c) {
        const auto at = static_cast<std::size_t>(c);
        int kindred_there = 0;
        double nearest_there = enough_at_a_row;
        int past_there = rows.empty() ? 0 : std::numeric_limits<int>::min();
        for (const auto& row : rows) {
            const auto& counted = uses_row.at(row);
            for (int k = 0; k < count; ++k)
                if (const auto has = counted[static_cast<std::size_t>(k)]; has > 0) {
                    kindred_there += has * kin[at][static_cast<std::size_t>(k)];
                    nearest_there = std::min(nearest_there, apart[at][static_cast<std::size_t>(k)]);
                }
            past_there = std::max(past_there, counted[at] + 1 - (meeting_row.at(row) + count - 1) / count);
        }
        double nearest = enough;
        int past = std::numeric_limits<int>::min(), most = 0, uses = 0;
        for (const auto table : tables) {
            const auto& counted = uses_at.at(table);
            for (int k = 0; k < count; ++k)
                if (counted[static_cast<std::size_t>(k)] > 0)
                    nearest = std::min(nearest, apart[static_cast<std::size_t>(c)][static_cast<std::size_t>(k)]);
            const auto has = counted[static_cast<std::size_t>(c)];
            past = std::max(past, has + 1 - (meeting.at(table) + count - 1) / count);
            most = std::max(most, has);
            uses += has;
        }
        return std::make_tuple(-kindred_there, nearest_there, -past_there, nearest, -past, -most, -uses);
    };
    // Each move makes one relationship's lot strictly better and can make
    // another's worse, so the passes are bounded rather than run until
    // nothing moves. Of 800 made-up schemas, nothing moved after the second
    // pass in any of them; in colour, anything moved in only four.
    for (int pass = 0; pass < 4; ++pass) {
        bool moved = false;
        for (const auto& [hash, says, identity] : order) {
            (void)hash;
            (void)says;
            const auto& tables = tables_of.at(*identity);
            const auto& rows = rows_of.at(*identity);
            auto& chosen = chosen_of.at(*identity);
            for (const auto table : tables) --uses_at.at(table)[static_cast<std::size_t>(chosen)];
            for (const auto& row : rows) --uses_row.at(row)[static_cast<std::size_t>(chosen)];
            auto best = weigh(tables, rows, chosen);
            auto better = chosen;
            for (int c = 0; c < count; ++c) {
                const auto weighed = weigh(tables, rows, c);
                if (weighed > best) {
                    best = weighed;
                    better = c;
                }
            }
            moved = moved || better != chosen;
            chosen = better;
            for (const auto table : tables) ++uses_at.at(table)[static_cast<std::size_t>(chosen)];
            for (const auto& row : rows) ++uses_row.at(row)[static_cast<std::size_t>(chosen)];
        }
        if (!moved) break;
    }

    std::map<std::string, QColor> given;
    for (const auto& [identity, chosen] : chosen_of) given.emplace(identity, colours[static_cast<std::size_t>(chosen)]);
    return given;
}

} // namespace erdflow::desktop::schema_colours
