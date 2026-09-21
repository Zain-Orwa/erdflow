#include "domain/schema_preview.hpp"

#include <algorithm>
#include <map>
#include <set>

namespace erdflow::domain {
namespace {

// Words whose plural is not made by adding a letter, and words that have no
// plural at all. Neither list can ever be complete, which is exactly why a
// name the user typed always wins over the one worked out here.
const std::map<std::string, std::string>& irregular_plurals() {
    static const std::map<std::string, std::string> table{
        {"child", "children"}, {"man", "men"}, {"woman", "women"},
        {"tooth", "teeth"}, {"foot", "feet"}, {"goose", "geese"},
        {"mouse", "mice"}, {"louse", "lice"}, {"ox", "oxen"},
        {"datum", "data"}, {"medium", "media"}, {"criterion", "criteria"},
        {"phenomenon", "phenomena"}, {"index", "indices"}, {"matrix", "matrices"},
        {"vertex", "vertices"}, {"appendix", "appendices"}, {"alumnus", "alumni"},
        {"stimulus", "stimuli"}, {"cactus", "cacti"}, {"fungus", "fungi"},
        {"nucleus", "nuclei"}, {"radius", "radii"}, {"curriculum", "curricula"},
        {"bacterium", "bacteria"}, {"stratum", "strata"}};
    return table;
}

// Mass nouns, words already plural, and the plurals of the irregulars above:
// there is no such thing as two JSONs, and Media is already the plural of
// Medium.
const std::set<std::string>& unchanged_plurals() {
    static const std::set<std::string> table = [] {
        std::set<std::string> words{
            "people", "series", "species", "staff", "aircraft", "sheep", "fish",
            "data", "metadata", "equipment", "information", "software", "hardware",
            "json", "xml", "html", "css", "sql", "text", "content", "audio", "video",
            "news", "money", "feedback", "traffic", "research", "evidence", "luggage",
            "furniture", "advice", "weather", "music"};
        for (const auto& [singular, plural] : irregular_plurals()) {
            (void)singular;
            words.insert(plural);
        }
        return words;
    }();
    return table;
}

std::string lowered(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool vowel(char c) {
    const auto lower = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return lower == 'a' || lower == 'e' || lower == 'i' || lower == 'o' || lower == 'u';
}

bool ends_with(const std::string& value, const std::string& tail) {
    return value.size() >= tail.size()
        && lowered(value.substr(value.size() - tail.size())) == tail;
}

// The plural keeps the capitalisation of the word it came from, so Student
// becomes Students and STUDENT becomes STUDENTS.
std::string match_case(const std::string& sample, std::string word) {
    if (sample.empty() || word.empty()) return word;
    const auto upper = std::all_of(sample.begin(), sample.end(), [](unsigned char c) {
        return !std::isalpha(c) || std::isupper(c);
    });
    if (upper) {
        std::transform(word.begin(), word.end(), word.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return word;
    }
    if (std::isupper(static_cast<unsigned char>(sample.front())))
        word.front() = static_cast<char>(std::toupper(static_cast<unsigned char>(word.front())));
    return word;
}

std::string plural_word(const std::string& word) {
    if (word.empty()) return word;
    const auto lower = lowered(word);
    if (unchanged_plurals().contains(lower)) return word;
    const auto irregular = irregular_plurals().find(lower);
    if (irregular != irregular_plurals().end()) return match_case(word, irregular->second);
    // Already plural: ends in s, but is not one of the singular -s endings.
    if (ends_with(word, "s") && !ends_with(word, "ss") && !ends_with(word, "us")
        && !ends_with(word, "is") && !ends_with(word, "as") && !ends_with(word, "os"))
        return word;
    // The one Latin ending regular enough to be a rule: Analysis, Crisis, Axis.
    if (ends_with(word, "sis") || ends_with(word, "xis")) return word.substr(0, word.size() - 2) + "es";
    if (ends_with(word, "s") || ends_with(word, "x") || ends_with(word, "z")
        || ends_with(word, "ch") || ends_with(word, "sh"))
        return word + "es";
    // Consonant + y becomes -ies; a vowel before the y keeps it.
    if (ends_with(word, "y") && word.size() >= 2 && !vowel(word[word.size() - 2]))
        return word.substr(0, word.size() - 1) + "ies";
    if (ends_with(word, "fe")) return word.substr(0, word.size() - 2) + "ves";
    if (ends_with(word, "f")) return word.substr(0, word.size() - 1) + "ves";
    // Consonant + o usually takes -es: Hero, Potato.
    if (ends_with(word, "o") && word.size() >= 2 && !vowel(word[word.size() - 2]))
        return word + "es";
    return word + "s";
}

// Only the last word is pluralised, so AccessCard becomes AccessCards rather
// than AccessesCards. A camel-cased run is split on its capitals.
std::size_t last_word_start(const std::string& name) {
    std::size_t start = 0;
    for (std::size_t i = 0; i < name.size(); ++i) {
        const auto c = static_cast<unsigned char>(name[i]);
        if (c == ' ' || c == '_' || c == '-') start = i + 1;
        else if (i > 0 && std::isupper(c) && !std::isupper(static_cast<unsigned char>(name[i - 1])))
            start = i;
    }
    return start;
}

// Everything owned by one element, in the project's own order so a schema
// drawn twice comes out the same.
std::vector<std::pair<AttributeId, Attribute>> owned_by(const Project& project, const ElementRef& owner) {
    std::vector<std::pair<AttributeId, Attribute>> found;
    for (const auto& [id, attribute] : project.attributes)
        if (attribute.owner && *attribute.owner == owner) found.emplace_back(id, attribute);
    return found;
}

PreviewColumn column_of(AttributeId id, const Attribute& attribute) {
    PreviewColumn column;
    column.name = attribute.name;
    column.type = attribute.logical_type;
    column.length = attribute.length;
    column.scale = attribute.scale;
    column.primary_key = attribute.identifier;
    column.required = attribute.required || attribute.identifier;
    column.unique = attribute.unique;
    column.auto_increment = attribute.auto_increment;
    column.origin = id;
    return column;
}

// A generated key: the bridge's own, or a surrogate where nothing identifies a
// table. It has no attribute behind it, so it carries a type of its own.
PreviewColumn generated_key(std::string name) {
    PreviewColumn column;
    column.name = std::move(name);
    column.type = LogicalType::Int;
    column.primary_key = true;
    column.required = true;
    column.origin_kind = ColumnOrigin::Generated;
    return column;
}

// The same, but letting a name the user typed stand in for the generated one.
// The table is named by the element it came from, which is the only stable
// thing an invented column has to be remembered against.
PreviewColumn generated_key_for(const Project& project, const ElementRef& table, std::string suggested) {
    const auto chosen = project.schema.key_names.find(table);
    auto column = generated_key(chosen != project.schema.key_names.end() ? chosen->second
                                                                         : std::move(suggested));
    column.auto_increment = project.schema.counting_keys.contains(table);
    return column;
}
} // namespace

std::vector<SharedName> shared_names(const SchemaPreview& preview) {
    // Gathered in the order the names are first met, so the list does not
    // reshuffle itself as a schema is worked through.
    std::vector<SharedName> groups;
    std::map<std::string, std::size_t> seen;
    for (const auto& table : preview.tables)
        for (const auto& column : table.columns) {
            if (column.ignored || column.type != LogicalType::Unset) continue;
            const auto found = seen.find(column.name);
            const auto at = found == seen.end()
                ? (seen.emplace(column.name, groups.size()),
                   groups.push_back(SharedName{column.name, {}, 0}), groups.size() - 1)
                : found->second;
            ++groups[at].columns;
            if (column.origin) groups[at].attributes.push_back(*column.origin);
        }
    // One column is not a group, and a group nothing can answer is no help:
    // a generated key or a foreign key takes its type from elsewhere.
    std::erase_if(groups, [](const SharedName& group) {
        return group.columns < 2 || group.attributes.empty();
    });
    return groups;
}

std::string plural_of(const std::string& word) {
    if (word.empty()) return word;
    const auto start = last_word_start(word);
    return word.substr(0, start) + plural_word(word.substr(start));
}

namespace {
// What a table is called: the name typed over it, else the entity's own name
// under the project's naming convention.
std::string table_name(const Project& project, const ElementRef& origin, const std::string& base) {
    const auto typed = project.decisions.table_name.find(origin);
    if (typed != project.decisions.table_name.end()) return typed->second;
    return project.decisions.naming == TableNaming::AsDrawn ? base : plural_of(base);
}

// The columns an attribute contributes. A composite gives its roots, or its
// whole, or both; a derived attribute gives nothing; a multivalued attribute
// gives nothing here because it becomes a table of its own.
void add_attribute(const Project& project, const ElementRef& owner,
                   AttributeId id, const Attribute& attribute, std::vector<PreviewColumn>& into) {
    if (attribute.kind == AttributeKind::Derived) {
        // Ignored by the rule, and shown to be. It becomes no column, so it
        // carries nothing a column carries.
        auto nothing = column_of(id, attribute);
        nothing.ignored = true;
        nothing.type = LogicalType::Unset;
        nothing.primary_key = false;
        nothing.required = false;
        nothing.unique = false;
        into.push_back(nothing);
        return;
    }
    if (attribute.kind == AttributeKind::Multivalued) return;  // becomes its own table
    // Taken off the schema by hand, and still on the diagram: ADR-010 allows
    // the two to differ, so the diagram is left alone and the column is not
    // drawn. A composite's parts are hidden with it, because hiding the whole
    // and keeping the pieces would say something nobody asked for.
    if (project.schema.hidden.contains(id)) return;
    if (attribute.kind != AttributeKind::Composite) {
        into.push_back(column_of(id, attribute));
        return;
    }
    const auto found = project.decisions.composite.find(id);
    const auto mode = found == project.decisions.composite.end() ? CompositeMode::Parts : found->second;
    if (mode != CompositeMode::Whole) {
        for (const auto& [part_id, part] : owned_by(project, ElementRef{id}))
            add_attribute(project, owner, part_id, part, into);
    }
    if (mode != CompositeMode::Parts) {
        auto whole = column_of(id, attribute);
        // Kept both ways, the whole is computed from its parts and stored
        // nowhere -- which is what a derived attribute already is.
        if (mode == CompositeMode::Both) whole.origin_kind = ColumnOrigin::Generated;
        into.push_back(whole);
    }
}

// The columns added to one table on the schema and nowhere else. They come
// after whatever the diagram contributed and before the foreign keys, which is
// where a reader expects a column they added themselves to be.
void add_schema_only(const Project& project, const ElementRef& table,
                     std::vector<PreviewColumn>& into) {
    const auto found = project.schema.added.find(table);
    if (found == project.schema.added.end()) return;
    for (const auto& one : found->second) {
        PreviewColumn column;
        column.name = one.name;
        column.type = one.logical_type;
        column.length = one.length;
        column.scale = one.scale;
        column.primary_key = one.identifier;
        column.required = one.required || one.identifier;
        column.unique = one.unique;
        column.auto_increment = one.auto_increment;
        column.origin_kind = ColumnOrigin::SchemaOnly;
        column.added = one.id;
        into.push_back(column);
    }
}

// The primary key columns of a table, by position, so a foreign key can name
// what it points at.
std::vector<std::size_t> key_columns(const PreviewTable& table) {
    std::vector<std::size_t> keys;
    for (std::size_t i = 0; i < table.columns.size(); ++i)
        if (table.columns[i].primary_key && !table.columns[i].ignored) keys.push_back(i);
    return keys;
}
} // namespace

SchemaPreview schema_preview(const Project& project) {
    SchemaPreview preview;
    std::map<ElementRef, std::size_t> table_of;   // which table an element became

    // 1. A table for each entity, carrying its attributes.
    for (const auto& [id, entity] : project.entities) {
        PreviewTable table;
        table.origin = ElementRef{id};
        table.name = table_name(project, ElementRef{id}, entity.name);
        table.origin_kind = TableOrigin::Entity;
        for (const auto& [attribute_id, attribute] : owned_by(project, ElementRef{id}))
            add_attribute(project, ElementRef{id}, attribute_id, attribute, table.columns);
        add_schema_only(project, ElementRef{id}, table.columns);
        // Nothing identifies it: the chosen identifier, if one was chosen, and
        // otherwise a key of its own so the table is still drawable.
        const auto chosen = project.decisions.identifier.find(id);
        if (chosen != project.decisions.identifier.end()) {
            for (auto& column : table.columns)
                if (column.origin == chosen->second && !column.ignored) {
                    column.primary_key = true;
                    column.required = true;
                }
        }
        if (key_columns(table).empty() && !entity.weak)
            table.columns.insert(table.columns.begin(), generated_key_for(project, ElementRef{id}, entity.name + "ID"));
        table_of.emplace(ElementRef{id}, preview.tables.size());
        preview.tables.push_back(std::move(table));
    }

    // 2. A table for each relationship that carries its own identity, and for
    //    each many-to-many: a bridge holding both keys, with a key of its own.
    for (const auto& [id, relationship] : project.relationships) {
        const auto many = std::count_if(relationship.participants.begin(), relationship.participants.end(),
                                        [](const Participant& side) { return side.maximum == Cardinality::Many; });
        const bool bridge = relationship.associative || many >= 2;
        if (!bridge) continue;
        PreviewTable table;
        table.origin = ElementRef{id};
        const auto named = project.decisions.junction_name.find(id);
        table.name = named != project.decisions.junction_name.end()
            ? named->second
            : table_name(project, ElementRef{id}, relationship.name);
        table.origin_kind = relationship.associative ? TableOrigin::Associative : TableOrigin::Bridge;
        // Its own primary key first, which is the rule: the bridge is given one
        // rather than keyed by the pair of foreign keys it holds.
        table.columns.push_back(generated_key_for(project, ElementRef{id}, relationship.name + "ID"));
        for (const auto& [attribute_id, attribute] : owned_by(project, ElementRef{id}))
            add_attribute(project, ElementRef{id}, attribute_id, attribute, table.columns);
        add_schema_only(project, ElementRef{id}, table.columns);
        table_of.emplace(ElementRef{id}, preview.tables.size());
        preview.tables.push_back(std::move(table));
    }

    // 3. A table for each multivalued attribute, with a key of its own and a
    //    foreign key back to whatever owns it.
    for (const auto& [id, attribute] : project.attributes) {
        if (attribute.kind != AttributeKind::Multivalued || !attribute.owner) continue;
        PreviewTable table;
        table.origin = ElementRef{id};
        table.name = table_name(project, ElementRef{id}, attribute.name);
        table.origin_kind = TableOrigin::Multivalued;
        table.columns.push_back(generated_key_for(project, ElementRef{id}, attribute.name + "ID"));
        auto value = column_of(id, attribute);
        value.primary_key = false;
        table.columns.push_back(value);
        add_schema_only(project, ElementRef{id}, table.columns);
        table_of.emplace(ElementRef{id}, preview.tables.size());
        preview.tables.push_back(std::move(table));
    }

    // A foreign key referring to a table's primary key, named for it.
    const auto add_foreign_key = [&](std::size_t into, std::size_t target,
                                     bool optional, bool one_to_one, ColumnOrigin kind,
                                     LinkSource link, const std::string& role = {}) {
        const auto keys = key_columns(preview.tables[target]);
        if (keys.empty()) return;
        PreviewColumn column;
        column.name = preview.tables[target].columns[keys.front()].name;
        // The role the side was given names the key, which is the only thing
        // that can tell two links to the same table apart. A flight's departure
        // and arrival airports are both AirportID without it, and naming one of
        // them after the table it points at says nothing about which is which.
        if (!role.empty()) column.name = role + column.name;
        // A key pointing back into its own table cannot share the name it
        // points at, or a table would hold the same column twice.
        else if (into == target) column.name = "Parent" + column.name;
        // Qualify until the name is the table's own. Comparing once while
        // changing the name mid-comparison leaves whether it collides
        // depending on the order the columns happen to be in.
        const auto taken_already = [&](const std::string& wanted) {
            return std::any_of(preview.tables[into].columns.begin(), preview.tables[into].columns.end(),
                               [&](const PreviewColumn& existing) { return existing.name == wanted; });
        };
        if (taken_already(column.name)) {
            const auto qualified = preview.tables[target].name + column.name;
            column.name = qualified;
            for (int attempt = 2; taken_already(column.name); ++attempt)
                column.name = qualified + std::to_string(attempt);
        }
        column.type = preview.tables[target].columns[keys.front()].type;
        column.length = preview.tables[target].columns[keys.front()].length;
        column.foreign_key = true;
        column.required = !optional;
        // A key on the near side of a one-to-one holds one row and no more,
        // which is exactly what unique says. It is the relationship's shape
        // rather than a rule anybody typed, so it is read from it.
        column.unique = one_to_one;
        column.origin_kind = kind;
        column.references = target;
        column.references_column = keys.front();
        column.optional_link = optional;
        column.one_to_one = one_to_one;
        column.link = link;
        preview.tables[into].columns.push_back(column);
    };

    // 4. The multivalued tables take their owner's key.
    for (const auto& [id, attribute] : project.attributes) {
        if (attribute.kind != AttributeKind::Multivalued || !attribute.owner) continue;
        const auto here = table_of.find(ElementRef{id});
        const auto owner = table_of.find(*attribute.owner);
        if (here == table_of.end() || owner == table_of.end()) continue;
        add_foreign_key(here->second, owner->second, false, false, ColumnOrigin::ForeignKey,
                        LinkSource{id});
    }

    // 5. Relationships. A bridge takes every side's key; a one-to-many puts the
    //    one side's key on the many side; a one-to-one puts it wherever it was
    //    decided, and on the second side by default.
    for (const auto& [id, relationship] : project.relationships) {
        const auto& sides = relationship.participants;
        if (sides.size() < 2) continue;
        const auto bridge = table_of.find(ElementRef{id});
        if (bridge != table_of.end()) {
            for (const auto& side : sides) {
                const auto target = table_of.find(target_ref(side.target));
                if (target == table_of.end()) continue;
                add_foreign_key(bridge->second, target->second,
                                side.participation == Participation::Partial, false,
                                ColumnOrigin::ForeignKey, LinkSource{side.id}, side.role);
            }
            continue;
        }
        // Not a bridge: exactly one side carries the key.
        const auto many = std::find_if(sides.begin(), sides.end(), [](const Participant& side) {
            return side.maximum == Cardinality::Many;
        });
        const Participant* carries = nullptr;
        const Participant* points_at = nullptr;
        const bool one_to_one = many == sides.end();
        if (!one_to_one) {
            carries = &*many;
            points_at = &sides[many == sides.begin() ? 1 : 0];
        } else {
            // Either side is correct, so the decision says which; failing that
            // the second side takes it, and readiness reports the question.
            const auto chosen = project.decisions.one_to_one_key.find(id);
            const auto* picked = &sides[1];
            if (chosen != project.decisions.one_to_one_key.end())
                for (const auto& side : sides)
                    if (side.id == chosen->second) picked = &side;
            carries = picked;
            points_at = &sides[picked == &sides.front() ? 1 : 0];
        }
        const auto into = table_of.find(target_ref(carries->target));
        const auto target = table_of.find(target_ref(points_at->target));
        if (into == table_of.end() || target == table_of.end()) continue;
        add_foreign_key(into->second, target->second,
                        points_at->participation == Participation::Partial, one_to_one,
                        ColumnOrigin::ForeignKey, LinkSource{points_at->id}, points_at->role);
    }

    // Before anything is folded away: the questions each table is where to
    // answer. They are attached now so that they travel with the table when
    // the tables are renumbered below, and one of them is moved if the table
    // it was attached to turns out not to survive.
    const auto attach = [&](const ElementRef& table, OpenDecision decision) {
        const auto found = table_of.find(table);
        if (found != table_of.end()) preview.tables[found->second].decisions.push_back(std::move(decision));
    };
    for (const auto& [id, specialization] : project.specializations) {
        if (!specialization.supertype || specialization.subtypes.empty()) continue;
        const auto found = project.decisions.isa.find(id);
        OpenDecision decision;
        decision.kind = DecisionKind::IsaStrategy;
        decision.about = ElementRef{id};
        decision.answered = found != project.decisions.isa.end();
        const auto strategy = decision.answered ? found->second : IsaStrategy::PerSubclass;
        decision.chosen = strategy == IsaStrategy::PerSubclass ? 0
                        : strategy == IsaStrategy::SingleTable ? 1 : 2;
        attach(ElementRef{*specialization.supertype}, std::move(decision));
    }
    for (const auto& [id, attribute] : project.attributes) {
        if (attribute.kind != AttributeKind::Composite || !attribute.owner) continue;
        const auto found = project.decisions.composite.find(id);
        OpenDecision decision;
        decision.kind = DecisionKind::CompositeMode;
        decision.about = ElementRef{id};
        decision.answered = found != project.decisions.composite.end();
        const auto mode = decision.answered ? found->second : CompositeMode::Parts;
        decision.chosen = mode == CompositeMode::Parts ? 0 : mode == CompositeMode::Whole ? 1 : 2;
        // Asked on the table the attribute belongs to, which is where its
        // columns are about to appear or not appear.
        auto owner = *attribute.owner;
        while (std::holds_alternative<AttributeId>(owner)) {
            const auto parent = project.attributes.find(std::get<AttributeId>(owner));
            if (parent == project.attributes.end() || !parent->second.owner) break;
            owner = *parent->second.owner;
        }
        attach(owner, std::move(decision));
    }
    for (const auto& [id, relationship] : project.relationships) {
        const auto& sides = relationship.participants;
        if (sides.size() != 2 || relationship.associative) continue;
        if (std::any_of(sides.begin(), sides.end(),
                        [](const Participant& side) { return side.maximum == Cardinality::Many; }))
            continue;
        const auto found = project.decisions.one_to_one_key.find(id);
        OpenDecision decision;
        decision.kind = DecisionKind::OneToOneKey;
        decision.about = ElementRef{id};
        decision.answered = found != project.decisions.one_to_one_key.end();
        decision.chosen = 1;   // the second side by default, as the mapping does
        for (std::size_t i = 0; i < sides.size(); ++i) {
            decision.sides.push_back(sides[i].id);
            decision.side_targets.push_back(target_ref(sides[i].target));
            if (decision.answered && sides[i].id == found->second) decision.chosen = i;
        }
        attach(target_ref(sides[decision.chosen].target), std::move(decision));
    }

    // 6. Hierarchies. The relationship is always one to one, and how it is
    //    mapped is the one conversion question with three genuinely different
    //    answers, so all three are built rather than only the default.
    //
    //    Tables are marked for removal here and taken out in one pass at the
    //    end. Removing one now would renumber every table after it, and every
    //    foreign key already added points at a table by its number.
    std::set<std::size_t> dropped;
    std::map<std::size_t, std::size_t> folded_into;   // a dropped table's replacement
    for (const auto& [id, specialization] : project.specializations) {
        if (!specialization.supertype) continue;
        const auto parent = table_of.find(ElementRef{*specialization.supertype});
        if (parent == table_of.end()) continue;
        const auto found = project.decisions.isa.find(id);
        const auto strategy = found == project.decisions.isa.end() ? IsaStrategy::PerSubclass : found->second;
        for (const auto& subtype : specialization.subtypes) {
            const auto child = table_of.find(ElementRef{subtype});
            if (child == table_of.end()) continue;
            preview.tables[child->second].origin_kind = TableOrigin::Subtype;
            preview.tables[child->second].derives_from = ElementRef{*specialization.supertype};
            if (strategy == IsaStrategy::PerSubclass) {
                add_foreign_key(child->second, parent->second, false, true,
                                ColumnOrigin::ForeignKey, LinkSource{subtype});
            } else if (strategy == IsaStrategy::PerConcrete) {
                // No parent table: each child carries the parent's columns.
                auto columns = preview.tables[parent->second].columns;
                auto& into = preview.tables[child->second].columns;
                into.insert(into.begin(), columns.begin(), columns.end());
            } else {
                // One table for the lot: the parent takes every child's own
                // columns, and the children stop being tables. A column that
                // belongs to only some of the rows cannot be required, whatever
                // the child said, because the other rows have nothing to put
                // there.
                for (const auto& column : preview.tables[child->second].columns) {
                    if (column.primary_key) continue;   // the parent's key is the key
                    auto moved = column;
                    moved.required = false;
                    moved.primary_key = false;
                    preview.tables[parent->second].columns.push_back(moved);
                }
                dropped.insert(child->second);
                folded_into.emplace(child->second, parent->second);
            }
        }
        if (strategy == IsaStrategy::SingleTable) {
            auto& into = preview.tables[parent->second];
            into.columns.push_back([] {
                PreviewColumn discriminator;
                discriminator.name = "Type";
                discriminator.type = LogicalType::Varchar;
                discriminator.length = 40;
                discriminator.required = true;
                discriminator.origin_kind = ColumnOrigin::Discriminator;
                return discriminator;
            }());
        } else if (strategy == IsaStrategy::PerConcrete) {
            // The parent is not a table under this strategy. Anything that
            // pointed at it has nothing single left to point at, which is the
            // strategy's own well-known cost rather than a fault here: the
            // column stays, because the value is still needed, and the line
            // goes, because there is no longer one table it leads to.
            dropped.insert(parent->second);
        }
    }

    if (!dropped.empty()) {
        // New numbers for the tables that remain, so every foreign key can be
        // pointed at the same table it was pointed at before.
        std::vector<std::optional<std::size_t>> moved_to(preview.tables.size());
        std::size_t next = 0;
        for (std::size_t i = 0; i < preview.tables.size(); ++i)
            if (!dropped.contains(i)) moved_to[i] = next++;
        for (auto& table : preview.tables) {
            for (auto& column : table.columns) {
                if (!column.references) continue;
                auto target = *column.references;
                // A child folded into its parent is still reachable: the rows
                // are in the parent now, so the key points there instead.
                if (const auto instead = folded_into.find(target); instead != folded_into.end())
                    target = instead->second;
                column.references = moved_to[target];
                if (!column.references) column.references_column = 0;
            }
        }
        // A question asked on a table that is about to go has to be asked
        // somewhere, or the answer that removed the table could not be undone
        // from the schema. It moves to whatever took that table's rows, and
        // failing that to the first table left.
        for (const auto& going : dropped) {
            if (preview.tables[going].decisions.empty()) continue;
            auto instead = folded_into.find(going) != folded_into.end()
                ? folded_into.at(going)
                : preview.tables.size();
            if (instead == preview.tables.size() || dropped.contains(instead))
                for (std::size_t i = 0; i < preview.tables.size(); ++i)
                    if (!dropped.contains(i)) { instead = i; break; }
            if (instead >= preview.tables.size()) continue;
            auto& moving = preview.tables[going].decisions;
            auto& into = preview.tables[instead].decisions;
            into.insert(into.end(), moving.begin(), moving.end());
            moving.clear();
        }
        std::vector<PreviewTable> kept;
        kept.reserve(next);
        for (std::size_t i = 0; i < preview.tables.size(); ++i)
            if (!dropped.contains(i)) kept.push_back(std::move(preview.tables[i]));
        preview.tables = std::move(kept);
    }

    return preview;
}

} // namespace erdflow::domain
