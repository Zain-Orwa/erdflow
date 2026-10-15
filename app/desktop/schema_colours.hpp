// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include <QColor>

#include <cstddef>
#include <map>
#include <string>
#include <tuple>
#include <vector>

// The colour each relationship on the Relational Schema is drawn in (C3a,
// 2026-10-13). A relationship's colour is given by what the relationship is,
// never by where its tables stand or the order its lines are routed in, so it
// stays the same while tables move, through saving and opening, and through
// undo and redo. The relationships that meet at one table are given colours
// plainly different from one another wherever the palette has enough.
namespace erdflow::desktop::schema_colours {

// The colours lines are drawn in on a light canvas, on a dark one, and in a
// theme with no colour.
enum class Palette { Light, Dark, Grey };
[[nodiscard]] const std::vector<QColor>& colours_of(Palette palette);
// How far apart two colours look (CIEDE2000): under about 10 two thin lines
// are hard to tell apart, from 20 they are plainly different.
[[nodiscard]] double distance(const QColor& one, const QColor& other);
// Whether two colours are of one family -- two greens, two oranges -- however
// far apart their lightness puts them: both plainly coloured, and their hues
// within 25 degrees of each other. Greys are of no family.
[[nodiscard]] bool kindred(const QColor& one, const QColor& other);

// A line that does not say which row of a table it leaves or lands on.
inline constexpr std::size_t no_column = static_cast<std::size_t>(-1);

// One line to be given a colour: what is the same for every line of one
// relationship and different for every other relationship; the two tables it
// joins (the same table twice for a key that refers to its own table); and
// what the relationship says -- the tables and columns it joins, by name --
// which is what its colour is worked out from. A name is the same every time
// a schema is made, where an identity may be new each time an example is
// opened, and the colour has to be the same every time too. Without a name the
// identity stands in for it. Last, the rows it leaves and lands on, by their
// places among their tables' columns: where several lines meet at one row --
// foreign keys referring to one key -- they meet at one point, and are kept
// apart there first. A line that does not say its rows is kept apart at its
// tables alone.
struct Line {
    std::string identity;
    std::size_t from_table = 0;
    std::size_t to_table = 0;
    std::string name;
    std::size_t from_column = no_column;
    std::size_t to_column = no_column;
    bool operator==(const Line& other) const = default;
    bool operator<(const Line& other) const {
        return std::tie(identity, from_table, to_table, name, from_column, to_column)
             < std::tie(other.identity, other.from_table, other.to_table, other.name, other.from_column,
                        other.to_column);
    }
};

// The colour of each relationship, by identity.
//
// Each relationship has a colour of its own to begin with, worked out from what
// it says alone. The relationships are then taken in an order that also
// depends only on what they say, and each keeps its own colour unless a
// relationship already taken that meets it at one of its tables has that
// colour. Then it takes the colour farthest from those it meets (any far
// enough apart counting alike -- in the greys, any grey but theirs), of those
// the one that takes neither of its tables past an even share of it, then the
// least used among those it meets, of those the nearest its own in the
// palette. What a relationship is given therefore depends on the
// relationships it meets at its tables, never on where tables stand or the
// order the lines are listed in.
//
// Before any of that, the relationships meeting it at one of its rows are
// weighed (C3c-2, 2026-10-14): a colour of a family none of them has, then
// one as far from theirs as a row's few colours can be (30, where the
// palette has room; in the greys any grey but theirs), then one that takes
// none of its rows past an even share. At a busy key the relationships
// arriving at its row are told apart before the rest of the table's.
[[nodiscard]] std::map<std::string, QColor> allocate(const std::vector<Line>& lines, Palette palette);

} // namespace erdflow::desktop::schema_colours
