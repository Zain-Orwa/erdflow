// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "domain/model.hpp"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace erdflow::domain {

// A schema drawn by hand, turned into the Conceptual ERD it would have come
// from (Zain, 2026-09-27). The course rules run backwards, so that converting
// the diagram forwards again gives back the same tables:
//
//   table                   -> entity; its columns other than foreign keys
//                              become its attributes, and its primary key
//                              columns become its key attributes (ADR-021 §5a)
//   foreign key             -> a one-to-many relationship, the key's own table
//                              on the many side; one-to-one where the key is
//                              UNIQUE; the key being NOT NULL is the one side
//                              taking part totally
//   self reference          -> the same, between an entity and itself
//   join table              -> a many-to-many relationship: a table whose two
//                              foreign keys, both NOT NULL and neither UNIQUE,
//                              point at two tables, which nothing points at,
//                              and which is keyed by the two keys together, by
//                              a whole-number key of its own, or by nothing;
//                              its other columns become the relationship's
//                              attributes
//
// Afterwards the diagram is the model and the schema is worked out from it,
// as in any project begun as a diagram (Zain chose this over keeping the two
// apart). What the tables were called, what their columns were called and
// typed, what they enforce, and where the tables sat are all carried across,
// so the schema reads the same afterwards; where the rules cannot say exactly
// what the schema said, the conversion says so in words rather than hiding it.

// How big each kind of element is drawn when it is made. The desktop owns
// these figures; the conversion is only told them.
struct DiagramSizes {
    double entity_width = 148;
    double entity_height = 86;
    double attribute_width = 150;
    double attribute_height = 60;
    double relationship_width = 190;
    double relationship_height = 110;
    // How wide each kind of element has to be for a name to be written whole,
    // told by whatever draws the names. An element is made at the size above
    // unless its name needs more, as the template's diamond is drawn wider
    // than a new one so it does not open on its own word cut short. Absent,
    // every element is made at the size above.
    std::function<double(const std::string& name)> entity_fit;
    std::function<double(const std::string& name)> attribute_fit;
    std::function<double(const std::string& name)> relationship_fit;
};

struct DiagramFromSchema {
    // The whole project as it is afterwards: the same project, drawn as a
    // diagram, no longer starting from its schema.
    Project project;
    // What each table became: an entity, or the relationship a join table
    // became.
    std::map<RelationId, ElementRef> became;
    // What could not be carried across exactly, one sentence each. Empty when
    // the schema reads exactly the same afterwards.
    std::vector<std::string> notes;
};

// Convert a project that starts from its schema. `tables` is where each table
// is on the schema now, which is where it stays; it also decides where each
// entity is put on the diagram, so the diagram keeps the schema's shape.
// `next_id` issues every identity the diagram needs.
[[nodiscard]] DiagramFromSchema diagram_from_schema(const Project& project,
                                                    const std::map<RelationId, Point>& tables,
                                                    const DiagramSizes& sizes,
                                                    const std::function<Uuid()>& next_id);

} // namespace erdflow::domain
