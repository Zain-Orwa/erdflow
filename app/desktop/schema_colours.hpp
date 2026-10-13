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

// One line to be given a colour: what is the same for every line of one
// relationship and different for every other relationship; the two tables it
// joins (the same table twice for a key that refers to its own table); and
// what the relationship says -- the tables and columns it joins, by name --
// which is what its colour is worked out from. A name is the same every time
// a schema is made, where an identity may be new each time an example is
// opened, and the colour has to be the same every time too. Without a name the
// identity stands in for it.
struct Line {
    std::string identity;
    std::size_t from_table = 0;
    std::size_t to_table = 0;
    std::string name;
    bool operator==(const Line& other) const = default;
    bool operator<(const Line& other) const {
        return std::tie(identity, from_table, to_table, name)
             < std::tie(other.identity, other.from_table, other.to_table, other.name);
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
[[nodiscard]] std::map<std::string, QColor> allocate(const std::vector<Line>& lines, Palette palette);

} // namespace erdflow::desktop::schema_colours
