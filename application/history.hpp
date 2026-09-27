// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "domain/model.hpp"

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace erdflow::application {

// The History is the undo history read as a record of what was done (Zain,
// 2026-09-26). Every step Undo can take back is one entry, described in
// words; nothing is kept apart from it, so the History and Undo can never
// disagree about what happened or in what order.

// What one step did to one thing. A step usually does one thing, but not
// always: deleting an entity takes its attributes with it, and moving it may
// carry several. The entry's sentence is made from these, and they are kept
// so that a fuller account can be given of each step later.
struct HistoryChange {
    enum class Kind { Created, Deleted, Renamed, Moved, Resized, Connected, Disconnected, Changed };
    Kind kind = Kind::Changed;
    // The element, where the thing is one. A table or a column on the schema
    // is named but carries none.
    std::optional<domain::ElementRef> element;
    // What kind of thing it is, as it would be written in a sentence:
    // "Entity", "Attribute", "Relationship", "Table", "Column" and so on.
    std::string noun;
    // Its name after the step, or before it for a thing the step deleted.
    std::string name;
    // Its name before the step, for a thing the step renamed.
    std::string previous;
    // What it was attached to or detached from: an attribute's owner, the
    // table a column is on, the entity a relationship took in or let go.
    std::string other;
    // Whether this thing only went because something else the step touched
    // did: an attribute deleted or moved along with its entity.
    bool carried = false;
};

// One step of the history.
struct HistoryEntry {
    // The command, as the Edit menu's Undo names it.
    std::string label;
    // What happened, in words: Created Entity "Student".
    std::string description;
    std::vector<HistoryChange> changes;
    std::chrono::system_clock::time_point when;
    // Whether the step is in effect. One that has been undone stays in the
    // history, waiting to be redone, until a new edit takes its place.
    bool in_effect = true;
    // Who made the step is not recorded yet. ERDFlow will have accounts,
    // several people and several agents working at once, and an entry is
    // where each will be named; for now every step is the one person at the
    // keyboard.
};

// The sentence for a step, from its command and what it did.
[[nodiscard]] std::string describe(const std::string& label, const std::vector<HistoryChange>& changes);

} // namespace erdflow::application
