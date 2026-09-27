// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "domain/diagram_from_schema.hpp"

#include "domain/schema_preview.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <optional>
#include <set>

namespace erdflow::domain {
namespace {

// A table of the schema being converted, read once: its columns in order and
// which of them point somewhere.
struct Table {
    RelationId id;
    const Relation* relation = nullptr;
    std::vector<const SchemaColumn*> columns;
    // The foreign key each column carries, where it carries one.
    std::map<SchemaColumnId, const SchemaForeignKey*> keys;
};

// How a join table is keyed, which decides what the relationship it becomes
// is told: its two foreign keys together, a whole-number key of its own, or
// nothing yet.
enum class JoinKey { Pair, Own, None };

bool whole_number_key(const SchemaColumn& column) {
    return column.logical_type == LogicalType::Int && column.length == 0 && column.scale == 0;
}

std::string lowered(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool starts_with_word(const std::string& name, const std::string& word) {
    return !word.empty() && lowered(name).rfind(lowered(word), 0) == 0;
}

// What the side a foreign key points at is called by it, where the key says
// more than the table's own name does: ManagerID pointing into Employee says
// Manager, while DeptID pointing into Department says only Department again.
// Empty where it says nothing more.
std::string role_word(const std::string& key, const std::string& target_key, const std::string& target_table) {
    // Named exactly as the key it points at, it says nothing about the side
    // beyond which key that is -- DeptID into Department's DeptID.
    if (lowered(key) == lowered(target_key)) return {};
    auto word = key;
    if (word.size() > target_key.size() && lowered(word).ends_with(lowered(target_key)))
        word.resize(word.size() - target_key.size());
    else if (lowered(word).ends_with("_id"))
        word.resize(word.size() - 3);
    else if (lowered(word).ends_with("id") && word.size() > 2)
        word.resize(word.size() - 2);
    while (!word.empty() && (word.back() == '_' || word.back() == ' ')) word.pop_back();
    if (word.empty() || word == key) return {};
    if (starts_with_word(target_table, word) || starts_with_word(word, target_table)) return {};
    return word;
}

// Two sides of one relationship given roles that tell them apart, however
// alike the words they were given.
void name_sides(Participant& first, Participant& second, std::string first_role, std::string second_role) {
    if (lowered(first_role) == lowered(second_role)) second_role += "2";
    first.role = std::move(first_role);
    second.role = std::move(second_role);
}

bool overlaps(const Rect& a, const Rect& b, double margin) {
    return a.x < b.x + b.width + margin && b.x < a.x + a.width + margin
        && a.y < b.y + b.height + margin && b.y < a.y + a.height + margin;
}

Rect centred(double cx, double cy, double width, double height) {
    return Rect{cx - width / 2, cy - height / 2, width, height};
}

// Rows of attributes laid over and under the thing they belong to, at most
// `across` to a row, each as wide as it was made. They alternate, over then
// under, so the first two sit either side of the name; each row is centred
// on the thing they belong to.
std::vector<Rect> ring(double cx, double cy, double body_height, const std::vector<double>& widths,
                       std::size_t across, const DiagramSizes& sizes) {
    constexpr double gap_x = 20;
    constexpr double gap_y = 24;
    std::vector<Rect> placed(widths.size());
    for (const bool over : {true, false}) {
        std::vector<std::size_t> side;
        for (std::size_t i = over ? 0 : 1; i < widths.size(); i += 2) side.push_back(i);
        for (std::size_t first = 0; first < side.size(); first += across) {
            const auto in_row = std::min(across, side.size() - first);
            const auto row = first / across;
            const auto offset = body_height / 2 + 30 + sizes.attribute_height / 2
                              + static_cast<double>(row) * (sizes.attribute_height + gap_y);
            const auto y = over ? cy - offset : cy + offset;
            double wide = gap_x * static_cast<double>(in_row - 1);
            for (std::size_t k = 0; k < in_row; ++k) wide += widths[side[first + k]];
            auto x = cx - wide / 2;
            for (std::size_t k = 0; k < in_row; ++k) {
                const auto width = widths[side[first + k]];
                placed[side[first + k]] = Rect{x, y - sizes.attribute_height / 2, width, sizes.attribute_height};
                x += width + gap_x;
            }
        }
    }
    return placed;
}

// How much room a thing and its attributes take, from the rows laid out
// round it.
Rect reach(const Rect& body, const std::vector<Rect>& attributes) {
    auto left = body.x;
    auto top = body.y;
    auto right = body.x + body.width;
    auto bottom = body.y + body.height;
    for (const auto& one : attributes) {
        left = std::min(left, one.x);
        top = std::min(top, one.y);
        right = std::max(right, one.x + one.width);
        bottom = std::max(bottom, one.y + one.height);
    }
    return Rect{left, top, right - left, bottom - top};
}

} // namespace

DiagramFromSchema diagram_from_schema(const Project& project, const std::map<RelationId, Point>& tables,
                                      const DiagramSizes& sizes, const std::function<Uuid()>& next_id) {
    DiagramFromSchema result;
    result.project = project;
    if (!project.schema.standalone) return result;
    auto& made = result.project;
    const auto& schema = project.schema;

    // Every table, read once.
    std::vector<Table> read;
    std::map<RelationId, std::size_t> index_of;
    std::map<SchemaColumnId, std::pair<RelationId, const SchemaColumn*>> column_at;
    for (const auto& [id, relation] : schema.relations) {
        Table table;
        table.id = id;
        table.relation = &relation;
        if (const auto found = schema.added.find(id); found != schema.added.end())
            for (const auto& column : found->second) {
                table.columns.push_back(&column);
                column_at.emplace(column.id, std::pair{id, &column});
            }
        index_of.emplace(id, read.size());
        read.push_back(std::move(table));
    }
    std::set<RelationId> pointed_at;
    for (const auto& [id, key] : schema.foreign_keys) {
        const auto from = index_of.find(key.from);
        if (from == index_of.end() || !index_of.contains(key.to) || !column_at.contains(key.target)) continue;
        read[from->second].keys.emplace(key.column, &key);
        pointed_at.insert(key.to);
    }
    const auto primary_key_of = [&](const Table& table) {
        std::vector<const SchemaColumn*> keys;
        for (const auto* column : table.columns)
            if (column->identifier) keys.push_back(column);
        return keys;
    };
    // Whether a foreign key points at the one column its table is keyed by,
    // which is the only thing a relationship on the diagram can point at.
    const auto at_the_key = [&](const SchemaForeignKey& key) {
        const auto keys = primary_key_of(read[index_of.at(key.to)]);
        return keys.size() == 1 && keys.front()->id == key.target;
    };

    // Which tables are join tables, and how each is keyed.
    std::map<RelationId, JoinKey> joins;
    for (const auto& table : read) {
        if (table.keys.size() != 2 || pointed_at.contains(table.id)) continue;
        bool plain_keys = true;
        for (const auto& [column, key] : table.keys) {
            const auto* own = column_at.at(column).second;
            if (!own->required || own->unique || !at_the_key(*key) || key->to == table.id) plain_keys = false;
        }
        if (!plain_keys) continue;
        const auto keys = primary_key_of(table);
        const auto is_foreign = [&](const SchemaColumn* column) { return table.keys.contains(column->id); };
        if (keys.empty()) joins.emplace(table.id, JoinKey::None);
        else if (keys.size() == 2 && std::all_of(keys.begin(), keys.end(), is_foreign))
            joins.emplace(table.id, JoinKey::Pair);
        else if (keys.size() == 1 && !is_foreign(keys.front()) && whole_number_key(*keys.front()))
            joins.emplace(table.id, JoinKey::Own);
    }

    // Identities are issued up front and handed out in the order things are
    // listed on the schema, because the conversion lists an element's
    // attributes, and a table's foreign keys, in the order of their
    // identities. Issued in a burst they need not come out in order, so they
    // are sorted first.
    std::size_t wanted = 0;
    for (const auto& table : read) {
        wanted += 1 + table.columns.size();
        wanted += 3 * table.keys.size();
    }
    std::vector<Uuid> pool;
    pool.reserve(wanted);
    for (std::size_t i = 0; i < wanted; ++i) pool.push_back(next_id());
    std::sort(pool.begin(), pool.end());
    std::size_t issued = 0;
    const auto issue = [&] { return pool.at(issued++); };

    // What every table became, before anything is joined up.
    std::map<RelationId, EntityId> entity_of;
    std::map<RelationId, RelationshipId> join_of;
    std::map<EntityId, std::vector<AttributeId>> entity_attributes;
    std::map<RelationshipId, std::vector<AttributeId>> join_attributes;
    const auto attribute_from = [&](const SchemaColumn& column, const ElementRef& owner, bool key_allowed) {
        Attribute attribute;
        attribute.id = AttributeId{issue()};
        attribute.name = column.name;
        attribute.comment = column.comment;
        attribute.logical_type = column.logical_type;
        attribute.length = column.length;
        attribute.scale = column.scale;
        attribute.identifier = key_allowed && column.identifier;
        attribute.kind = attribute.identifier ? AttributeKind::Key : AttributeKind::Normal;
        attribute.required = column.required || attribute.identifier;
        attribute.unique = column.unique;
        attribute.auto_increment = column.auto_increment;
        attribute.owner = owner;
        made.attributes.emplace(attribute.id, attribute);
        return attribute.id;
    };
    for (const auto& table : read) {
        if (joins.contains(table.id)) continue;
        Entity entity;
        entity.id = EntityId{issue()};
        entity.name = table.relation->name;
        entity.description = table.relation->description;
        entity.comment = table.relation->comment;
        made.entities.emplace(entity.id, entity);
        entity_of.emplace(table.id, entity.id);
        result.became.emplace(table.id, ElementRef{entity.id});
        for (const auto* column : table.columns) {
            if (table.keys.contains(column->id)) continue;
            entity_attributes[entity.id].push_back(attribute_from(*column, ElementRef{entity.id}, true));
        }
    }

    // The relationships: each join table, and each other foreign key, in the
    // order their tables and columns come, which is the order the conversion
    // will list them back in.
    std::map<ForeignKeyId, ParticipantId> link_of;   // the side each key becomes
    std::map<ForeignKeyId, std::string> key_named;   // what each key was called
    struct Placing { RelationshipId id; EntityId first; EntityId second; };
    std::vector<Placing> placing;
    for (const auto& table : read) {
        const auto join = joins.find(table.id);
        if (join != joins.end()) {
            Relationship relationship;
            relationship.id = RelationshipId{issue()};
            relationship.name = table.relation->name;
            relationship.description = table.relation->description;
            relationship.comment = table.relation->comment;
            for (const auto* column : table.columns) {
                const auto key = table.keys.find(column->id);
                if (key == table.keys.end()) continue;
                Participant side;
                side.id = ParticipantId{issue()};
                side.target = entity_of.at(key->second->to);
                side.maximum = Cardinality::Many;
                side.cardinality_confirmed = true;
                relationship.participants.push_back(side);
                link_of.emplace(key->second->id, side.id);
                key_named.emplace(key->second->id, column->name);
            }
            // A join table between a table and itself -- a friendship between
            // two people -- names its sides for the keys that hold them.
            if (relationship.participants.size() == 2
                && relationship.participants[0].target == relationship.participants[1].target) {
                std::vector<std::string> said;
                for (const auto* column : table.columns) {
                    const auto key = table.keys.find(column->id);
                    if (key == table.keys.end()) continue;
                    const auto& target = read[index_of.at(key->second->to)];
                    const auto word = role_word(column->name, column_at.at(key->second->target).second->name,
                                                target.relation->name);
                    said.push_back(word.empty() ? column->name : word);
                }
                if (said.size() == 2)
                    name_sides(relationship.participants[0], relationship.participants[1], said[0], said[1]);
            }
            join_of.emplace(table.id, relationship.id);
            result.became.emplace(table.id, ElementRef{relationship.id});
            placing.push_back({relationship.id, std::get<EntityId>(relationship.participants[0].target),
                               std::get<EntityId>(relationship.participants[1].target)});
            made.relationships.emplace(relationship.id, relationship);
            for (const auto* column : table.columns) {
                if (table.keys.contains(column->id)) continue;
                if (join->second == JoinKey::Own && column->identifier) continue;
                join_attributes[relationship.id].push_back(
                    attribute_from(*column, ElementRef{relationship.id}, false));
            }
            if (join->second == JoinKey::Pair) made.decisions.bridge_key[relationship.id] = BridgeKey::Pair;
            continue;
        }
        const auto here = entity_of.at(table.id);
        for (const auto* column : table.columns) {
            const auto key = table.keys.find(column->id);
            if (key == table.keys.end()) continue;
            const auto& target = read[index_of.at(key->second->to)];
            const auto there = entity_of.at(target.id);
            const auto* target_column = column_at.at(key->second->target).second;
            const bool one_to_one = column->unique
                || (column->identifier && primary_key_of(table).size() == 1);
            const auto word = role_word(column->name, target_column->name, target.relation->name);

            Relationship relationship;
            relationship.id = RelationshipId{issue()};
            relationship.name = word.empty() ? std::string{"Has"} : word;
            Participant pointed;   // the side the key points at
            pointed.id = ParticipantId{issue()};
            pointed.target = there;
            pointed.maximum = Cardinality::One;
            pointed.participation = column->required ? Participation::Total : Participation::Partial;
            pointed.cardinality_confirmed = true;
            pointed.participation_confirmed = true;
            Participant carrying;  // the side the key is kept on
            carrying.id = ParticipantId{issue()};
            carrying.target = here;
            carrying.maximum = one_to_one ? Cardinality::One : Cardinality::Many;
            carrying.cardinality_confirmed = true;
            // An entity joined to itself is told apart by the roles its two
            // sides play, or the diagram cannot say which is which: the side
            // pointed at plays what the key calls it -- Manager -- and the
            // side holding the key plays the table itself.
            if (there == here) name_sides(pointed, carrying, word.empty() ? column->name : word,
                                          table.relation->name);
            relationship.participants = {pointed, carrying};
            if (one_to_one) made.decisions.one_to_one_key[relationship.id] = carrying.id;
            link_of.emplace(key->second->id, pointed.id);
            key_named.emplace(key->second->id, column->name);
            placing.push_back({relationship.id, there, here});
            made.relationships.emplace(relationship.id, relationship);

            if (column->identifier && !one_to_one)
                result.notes.push_back(table.relation->name + "." + column->name
                    + " was part of the primary key. A relationship's key is not part of the "
                      "entity's own key on the diagram, so it is now an ordinary foreign key.");
            if (!at_the_key(*key->second))
                result.notes.push_back(table.relation->name + "." + column->name + " pointed at "
                    + target.relation->name + "." + target_column->name + ", which is not "
                    + target.relation->name + "'s primary key. It now points at the primary key.");
        }
    }

    // The table naming the schema was drawn with is the one it keeps: every
    // table is named exactly as the entity is, so none is renamed on the way.
    made.decisions.naming = TableNaming::AsDrawn;
    // Nothing the schema kept against a table it no longer has survives.
    std::erase_if(made.decisions.table_name, [&](const auto& entry) { return index_of.contains(entry.first); });

    SchemaOverrides overrides;
    for (const auto& table : read) {
        const auto join = joins.find(table.id);
        if (join == joins.end() || join->second != JoinKey::Own) continue;
        const auto relationship = join_of.at(table.id);
        const auto* own = primary_key_of(table).front();
        const auto relation = relation_from(ElementRef{relationship});
        if (own->name != made.relationships.at(relationship).name + "ID") overrides.key_names[relation] = own->name;
        if (own->auto_increment) overrides.counting_keys.insert(relation);
    }
    made.schema = overrides;

    // Where the tables sit on the schema, and how they were sized and their
    // lines shaped, kept against what each became.
    SchemaLayout arranged;
    for (const auto& [old_id, element] : result.became) {
        const auto relation = relation_from(element);
        if (const auto at = tables.find(old_id); at != tables.end()) arranged.tables[relation] = at->second;
        else if (const auto kept = project.schema_layout.tables.find(old_id); kept != project.schema_layout.tables.end())
            arranged.tables[relation] = kept->second;
        if (const auto wide = project.schema_layout.widths.find(old_id); wide != project.schema_layout.widths.end())
            arranged.widths[relation] = wide->second;
        if (const auto tall = project.schema_layout.heights.find(old_id); tall != project.schema_layout.heights.end())
            arranged.heights[relation] = tall->second;
    }
    for (const auto& [old_key, line] : project.schema_layout.lines)
        if (const auto side = link_of.find(old_key); side != link_of.end())
            arranged.lines[foreign_key_from(LinkSource{side->second})] = line;
    made.schema_layout = arranged;

    // A colour or a see-through surface given to a table is its element's now.
    const auto rekey = [&](auto& by_element) {
        auto copy = by_element;
        by_element.clear();
        for (auto& [ref, value] : copy) {
            const auto* relation = std::get_if<RelationId>(&ref);
            if (!relation) { by_element.emplace(ref, value); continue; }
            if (const auto now = result.became.find(*relation); now != result.became.end())
                by_element.emplace(now->second, value);
        }
    };
    rekey(made.colours);
    rekey(made.transparency);
    for (auto& [id, comment] : made.comments) {
        (void)id;
        for (auto& target : comment.targets) {
            auto* ref = std::get_if<ElementRef>(&target);
            if (auto* anchor = std::get_if<TextAnchor>(&target)) ref = &anchor->owner;
            if (!ref) continue;
            if (const auto* relation = std::get_if<RelationId>(ref))
                if (const auto now = result.became.find(*relation); now != result.became.end()) *ref = now->second;
        }
    }

    // The schema this is no longer.
    made.schema.standalone = false;

    // Foreign keys keep the names they were drawn with. What the rules would
    // call each one is read off the converted schema, and only a name that
    // differs is kept as typed, so a key that already reads right still
    // follows the key it points at. Worked out again until nothing changes,
    // since a kept name can decide whether a later one had to be qualified.
    for (int pass = 0; pass < 4; ++pass) {
        bool changed = false;
        const auto preview = schema_preview(made);
        for (const auto& table : preview.tables)
            for (const auto& column : table.columns) {
                if (!column.foreign_key || !column.link) continue;
                const auto* side = std::get_if<ParticipantId>(&*column.link);
                if (!side) continue;
                for (const auto& [old_key, participant] : link_of) {
                    if (participant != *side) continue;
                    const auto& wanted_name = key_named.at(old_key);
                    if (column.name == wanted_name) continue;
                    made.schema.foreign_key_names[ForeignKeyColumn{foreign_key_from(*column.link),
                                                                   column.reference_part}] = wanted_name;
                    changed = true;
                }
            }
        if (!changed) break;
    }

    // What reads differently afterwards, said once for each table it touches.
    const auto after = schema_preview(made);
    bool reordered = false;
    for (const auto& table : read) {
        const auto element = result.became.at(table.id);
        const auto found = std::find_if(after.tables.begin(), after.tables.end(),
                                        [&](const PreviewTable& one) { return one.origin == element; });
        if (found == after.tables.end()) continue;
        std::vector<std::string> was;
        for (const auto* column : table.columns) was.push_back(column->name);
        std::vector<std::string> now;
        for (const auto& column : found->columns)
            if (!column.ignored) now.push_back(column.name);
        for (const auto& column : found->columns)
            if (column.origin_kind == ColumnOrigin::Generated
                && std::find(was.begin(), was.end(), column.name) == was.end())
                result.notes.push_back(primary_key_of(table).empty()
                    ? table.relation->name + " had no primary key, so the diagram gives it one: "
                          + column.name + "."
                    : table.relation->name + " was keyed only by its foreign keys, which a diagram does not "
                          "draw as an entity's own key, so it is given a key of its own: " + column.name + ".");
        auto sorted_was = was;
        auto sorted_now = now;
        std::sort(sorted_was.begin(), sorted_was.end());
        std::sort(sorted_now.begin(), sorted_now.end());
        if (sorted_was == sorted_now && was != now) reordered = true;
    }
    if (reordered)
        result.notes.push_back("Foreign keys are listed after a table's other columns, as a schema worked out "
                               "from a diagram always lists them.");

    // How big each element is made: the size a new one has, or wider where
    // its name needs more room to be written whole.
    const auto fit = [](const std::function<double(const std::string&)>& measure, const std::string& name,
                        double ordinary) {
        return measure ? std::max(ordinary, measure(name)) : ordinary;
    };
    const auto width_of_attribute = [&](AttributeId id) {
        return fit(sizes.attribute_fit, made.attributes.at(id).name, sizes.attribute_width);
    };
    const auto widths_of = [&](const std::vector<AttributeId>& owned) {
        std::vector<double> widths;
        for (const auto& id : owned) widths.push_back(width_of_attribute(id));
        return widths;
    };
    const auto relationship_box = [&](RelationshipId id, double cx, double cy) {
        const auto width = fit(sizes.relationship_fit, made.relationships.at(id).name, sizes.relationship_width);
        // A diamond drawn wider is drawn a little taller with it, as the
        // template's is, so it does not flatten into a line.
        const auto height = sizes.relationship_height + (width - sizes.relationship_width) * 0.1;
        return centred(cx, cy, width, height);
    };

    // Where everything is drawn. Entities keep the schema's arrangement --
    // left to right and top to bottom as the tables were -- on a grid roomy
    // enough for their attributes above and below them, which leaves the
    // space between them for the relationships.
    std::vector<std::pair<RelationId, Point>> entity_places;
    for (const auto& table : read) {
        if (!entity_of.contains(table.id)) continue;
        const auto at = tables.find(table.id);
        const auto kept = project.schema_layout.tables.find(table.id);
        entity_places.emplace_back(table.id, at != tables.end() ? at->second
                                             : kept != project.schema_layout.tables.end()
                                                 ? kept->second
                                                 : Point{0, static_cast<double>(entity_places.size()) * 200});
    }
    constexpr std::size_t across = 4;
    // Each entity's box and its attributes', laid out round a centre of
    // nothing first, so the grid can be made as roomy as the roomiest.
    std::map<EntityId, Rect> body_of;
    std::map<EntityId, std::vector<Rect>> spots_of;
    double widest = sizes.entity_width;
    double tallest = sizes.entity_height;
    for (const auto& [old_id, at] : entity_places) {
        (void)at;
        const auto entity = entity_of.at(old_id);
        const auto body = centred(0, 0, fit(sizes.entity_fit, made.entities.at(entity).name, sizes.entity_width),
                                  sizes.entity_height);
        body_of[entity] = body;
        spots_of[entity] = ring(0, 0, sizes.entity_height, widths_of(entity_attributes[entity]), across, sizes);
        const auto room = reach(body, spots_of[entity]);
        widest = std::max(widest, room.width);
        tallest = std::max(tallest, room.height);
    }
    const auto cell_width = widest + sizes.relationship_width + 160;
    const auto cell_height = tallest + sizes.relationship_height + 80;
    // Columns are tables standing roughly one above another; rows are their
    // order within the column.
    auto by_x = entity_places;
    std::sort(by_x.begin(), by_x.end(), [](const auto& a, const auto& b) {
        return a.second.x != b.second.x ? a.second.x < b.second.x : a.second.y < b.second.y;
    });
    std::vector<std::vector<std::pair<RelationId, Point>>> columns;
    for (const auto& one : by_x) {
        if (columns.empty() || one.second.x - columns.back().front().second.x > 80) columns.emplace_back();
        columns.back().push_back(one);
    }
    std::vector<Rect> occupied;
    std::map<EntityId, Point> centre_of;
    std::map<EntityId, double> reach_of;
    const auto moved = [](Rect box, double dx, double dy) {
        box.x += dx;
        box.y += dy;
        return box;
    };
    for (std::size_t c = 0; c < columns.size(); ++c) {
        auto& column = columns[c];
        std::sort(column.begin(), column.end(), [](const auto& a, const auto& b) { return a.second.y < b.second.y; });
        for (std::size_t r = 0; r < column.size(); ++r) {
            const auto entity = entity_of.at(column[r].first);
            const auto cx = static_cast<double>(c) * cell_width;
            const auto cy = static_cast<double>(r) * cell_height;
            centre_of[entity] = Point{cx, cy};
            const auto body = moved(body_of.at(entity), cx, cy);
            made.layout[ElementRef{entity}] = body;
            occupied.push_back(body);
            const auto& owned = entity_attributes[entity];
            const auto& spots = spots_of.at(entity);
            reach_of[entity] = reach(body_of.at(entity), spots).width / 2;
            for (std::size_t a = 0; a < owned.size() && a < spots.size(); ++a) {
                const auto spot = moved(spots[a], cx, cy);
                made.layout[ElementRef{owned[a]}] = spot;
                occupied.push_back(spot);
            }
        }
    }
    // A relationship goes between the two things it joins, pushed aside along
    // the line across them until it lands clear of everything already put
    // down; a relationship of an entity with itself goes beside that entity.
    for (const auto& [id, first, second] : placing) {
        const auto& owned = join_attributes[id];
        const auto widths = widths_of(owned);
        const auto a = centre_of.at(first);
        const auto b = centre_of.at(second);
        const auto own_width = relationship_box(id, 0, 0).width;
        std::vector<Point> tries;
        if (first == second) {
            const auto out = reach_of.at(first) + own_width / 2 + 60;
            for (int k = 0; k < 8; ++k) {
                const auto step = static_cast<double>(k / 2) * (sizes.relationship_height + 30);
                tries.push_back(Point{a.x + (k % 2 == 0 ? out : -out), a.y + step});
            }
        } else {
            const auto dx = b.x - a.x;
            const auto dy = b.y - a.y;
            const auto length = std::max(1.0, std::hypot(dx, dy));
            const Point across_line{-dy / length, dx / length};
            const Point middle{(a.x + b.x) / 2, (a.y + b.y) / 2};
            for (int k = 0; k < 13; ++k) {
                const auto step = static_cast<double>((k + 1) / 2) * (sizes.relationship_height + 40)
                                * (k % 2 == 0 ? 1.0 : -1.0);
                tries.push_back(Point{middle.x + across_line.x * step, middle.y + across_line.y * step});
            }
        }
        const auto fits = [&](const Point& at) {
            const auto body = relationship_box(id, at.x, at.y);
            std::vector<Rect> wants{body};
            for (const auto& spot : ring(at.x, at.y, body.height, widths, 3, sizes)) wants.push_back(spot);
            for (const auto& want : wants)
                for (const auto& taken : occupied)
                    if (overlaps(want, taken, 16)) return false;
            return true;
        };
        auto chosen = tries.front();
        for (const auto& at : tries)
            if (fits(at)) { chosen = at; break; }
        const auto body = relationship_box(id, chosen.x, chosen.y);
        made.layout[ElementRef{id}] = body;
        occupied.push_back(body);
        const auto spots = ring(chosen.x, chosen.y, body.height, widths, 3, sizes);
        for (std::size_t n = 0; n < owned.size() && n < spots.size(); ++n) {
            made.layout[ElementRef{owned[n]}] = spots[n];
            occupied.push_back(spots[n]);
        }
    }
    // Everything drawn here is moved together so the diagram starts near the
    // top left of the canvas, as a diagram drawn by hand does.
    std::vector<ElementRef> drawn;
    for (const auto& [id, entity] : made.entities) { (void)entity; drawn.emplace_back(id); }
    for (const auto& [id, attribute] : made.attributes) { (void)attribute; drawn.emplace_back(id); }
    for (const auto& [id, relationship] : made.relationships) { (void)relationship; drawn.emplace_back(id); }
    double left = 0;
    double top = 0;
    for (std::size_t i = 0; i < drawn.size(); ++i) {
        const auto& box = made.layout.at(drawn[i]);
        left = i == 0 ? box.x : std::min(left, box.x);
        top = i == 0 ? box.y : std::min(top, box.y);
    }
    for (const auto& ref : drawn) {
        auto& box = made.layout.at(ref);
        box.x += 60 - left;
        box.y += 60 - top;
    }
    return result;
}

} // namespace erdflow::domain
