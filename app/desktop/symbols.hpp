// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include <QString>
#include <vector>

namespace erdflow::desktop {

// One character that can be put into a name, a description or a note, carrying
// the name it goes by so it can be searched for rather than hunted for by eye.
// Nobody remembers where the natural join sign lives in a grid of two hundred
// characters; everybody can type "join".
struct Symbol {
    QString character;
    QString name;
};

struct SymbolGroup {
    QString name;
    std::vector<Symbol> symbols;
};

// Every group, in the order they are offered. Relational algebra leads because
// a conceptual diagram is drawn on the way to a relational schema, and those
// signs are the ones this editor's users reach for and cannot type.
[[nodiscard]] const std::vector<SymbolGroup>& symbol_groups();

// The group a name refers to, for opening the picker where the user asked for
// it. No match leaves the picker on its first group.
[[nodiscard]] int symbol_group_index(const QString& name);

} // namespace erdflow::desktop
