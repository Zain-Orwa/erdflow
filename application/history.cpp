// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "application/history.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <utility>

namespace erdflow::application {
namespace {

using Kind = HistoryChange::Kind;

std::string quoted(const std::string& name) { return "\"" + name + "\""; }

// A command is named as something to do ("Change entity kind"); a step that
// has been done is said in the past ("Changed entity kind"). Only the first
// word changes, and only the words the commands begin with are known here;
// anything else is left as the command said it.
std::string past(const std::string& label) {
    static constexpr std::array<std::pair<const char*, const char*>, 38> verbs{{
        {"Add", "Added"}, {"Attach", "Attached"}, {"Change", "Changed"}, {"Choose", "Chose"},
        {"Clear", "Cleared"}, {"Connect", "Connected"}, {"Count", "Counted"}, {"Create", "Created"},
        {"Delete", "Deleted"}, {"Describe", "Described"}, {"Detach", "Detached"},
        {"Disconnect", "Disconnected"}, {"Duplicate", "Duplicated"}, {"Edit", "Edited"},
        {"Hide", "Hid"}, {"Import", "Imported"}, {"Insert", "Inserted"}, {"Lock", "Locked"},
        {"Make", "Made"}, {"Move", "Moved"}, {"Name", "Named"}, {"Pin", "Pinned"},
        {"Relate", "Related"}, {"Release", "Released"}, {"Remove", "Removed"}, {"Rename", "Renamed"},
        {"Resize", "Resized"}, {"Reverse", "Reversed"}, {"Route", "Routed"}, {"Set", "Set"},
        {"Shape", "Shaped"}, {"Show", "Showed"}, {"Stop", "Stopped"}, {"Straighten", "Straightened"},
        {"Take", "Took"}, {"Tidy", "Tidied"}, {"Unlock", "Unlocked"}, {"Unpin", "Unpinned"}}};
    const auto space = label.find(' ');
    const auto first = label.substr(0, space);
    for (const auto& [now, then] : verbs)
        if (first == now) return then + (space == std::string::npos ? std::string{} : label.substr(space));
    return label;
}

std::string plural(const std::string& noun, std::size_t count) {
    std::string word = noun;
    std::transform(word.begin(), word.end(), word.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    if (count == 1) return word;
    if (word.size() > 1 && word.back() == 'y') return word.substr(0, word.size() - 1) + "ies";
    return word + "s";
}

// How several things of one step are counted: by their own noun when they
// are all the same kind of thing, as elements otherwise.
std::string counted(const std::vector<const HistoryChange*>& group) {
    const auto& noun = group.front()->noun;
    const bool alike = std::all_of(group.begin(), group.end(), [&](const auto* c) { return c->noun == noun; });
    return std::to_string(group.size()) + " " + plural(alike ? noun : std::string{"element"}, group.size());
}

// What a created thing is said to have been done to it, which the command
// says better than the change does: a copy was duplicated, an import imported.
std::string creation_verb(const std::string& label) {
    for (const auto* verb : {"Duplicate", "Import", "Insert"})
        if (label.rfind(verb, 0) == 0) return past(verb);
    return "Created";
}

std::string one(const std::string& label, const HistoryChange& c, std::size_t carried,
                const std::string& carried_noun) {
    const auto subject = c.noun + " " + quoted(c.name);
    // An attribute or a column is attached to its owner; an entity is taken
    // into a relationship. The sentence names them in that order.
    const bool attaches = c.noun == "Attribute" || c.noun == "Column";
    switch (c.kind) {
    case Kind::Created:
        if (!c.other.empty() && creation_verb(label) == "Created") return "Added " + subject + " to " + quoted(c.other);
        return creation_verb(label) + " " + subject;
    case Kind::Deleted: {
        auto sentence = "Deleted " + subject;
        if (carried > 0) sentence += " with its " + std::to_string(carried) + " " + plural(carried_noun, carried);
        return sentence;
    }
    case Kind::Renamed:
        if (c.previous.empty()) return "Renamed " + c.noun + " to " + quoted(c.name);
        return "Renamed " + c.noun + " " + quoted(c.previous) + " to " + quoted(c.name);
    case Kind::Connected:
        if (attaches) return "Connected " + subject + " to " + quoted(c.other);
        return "Connected " + quoted(c.other) + " to " + subject;
    case Kind::Disconnected:
        if (attaches) return "Disconnected " + subject + " from " + quoted(c.other);
        return "Disconnected " + quoted(c.other) + " from " + subject;
    case Kind::Moved:
        return "Moved " + subject;
    case Kind::Resized:
        return "Resized " + subject;
    case Kind::Changed:
        break;
    }
    return past(label) + ": " + subject;
}

std::string several(const std::string& label, const std::vector<const HistoryChange*>& group) {
    const auto& c = *group.front();
    switch (c.kind) {
    case Kind::Created: {
        const bool one_owner = !c.other.empty() && std::all_of(group.begin(), group.end(), [&](const auto* each) {
            return each->other == c.other;
        });
        if (one_owner && creation_verb(label) == "Created") return "Added " + counted(group) + " to " + quoted(c.other);
        return creation_verb(label) + " " + counted(group);
    }
    case Kind::Deleted: return "Deleted " + counted(group);
    case Kind::Renamed: return "Renamed " + counted(group);
    case Kind::Connected: return "Connected " + counted(group);
    case Kind::Disconnected: return "Disconnected " + counted(group);
    case Kind::Moved: return "Moved " + counted(group);
    case Kind::Resized: return "Resized " + counted(group);
    case Kind::Changed: break;
    }
    return past(label) + ": " + counted(group);
}

} // namespace

std::string describe(const std::string& label, const std::vector<HistoryChange>& changes) {
    // What leads the sentence is what was done on purpose; what only went
    // along with it is counted beside it.
    std::vector<const HistoryChange*> chosen;
    for (const auto& c : changes)
        if (!c.carried) chosen.push_back(&c);
    if (chosen.empty())
        for (const auto& c : changes) chosen.push_back(&c);
    if (chosen.empty()) return past(label);
    // The most telling kind of change the step made speaks for it.
    for (const auto kind : {Kind::Created, Kind::Deleted, Kind::Renamed, Kind::Connected, Kind::Disconnected,
                            Kind::Resized, Kind::Moved, Kind::Changed}) {
        std::vector<const HistoryChange*> group;
        for (const auto* c : chosen)
            if (c->kind == kind) group.push_back(c);
        if (group.empty()) continue;
        if (group.size() > 1) return several(label, group);
        const auto carried = static_cast<std::size_t>(std::count_if(changes.begin(), changes.end(), [&](const auto& c) {
            return c.carried && c.kind == kind;
        }));
        // What went along with it is named for what it was: an entity's
        // attributes, a table's columns.
        const auto along = std::find_if(changes.begin(), changes.end(), [&](const auto& c) {
            return c.carried && c.kind == kind;
        });
        return one(label, *group.front(), carried, along != changes.end() ? along->noun : std::string{"Attribute"});
    }
    return past(label);
}

} // namespace erdflow::application
