// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "domain/schema_preview.hpp"

#include <QString>
#include <QStringList>

#include <cstddef>
#include <optional>
#include <vector>

namespace erdflow::desktop {

// What can be read off the schema without being stored beside it (Stage 3 of
// the Schema workspace, 2026-10-01): its foreign keys, gathered whole, and its
// tables' primary keys. Read by the Explorer and by Properties alike, so the
// two say the same thing about the same key in the same words.

// One foreign key, however many columns it has: the table whose rows hold it
// and those columns, in the order of the key they point at. Gathered from the
// columns that carry its identity, so a key over two columns is one key here
// as it is on the schema, never two.
struct GatheredKey {
    domain::ForeignKeyId id;
    std::size_t table = 0;
    std::vector<std::size_t> rows;
};

// Every foreign key on the schema, in the order its tables and rows list them.
[[nodiscard]] std::vector<GatheredKey> gather_keys(const domain::SchemaPreview& preview);

// The table a key points at, by its place in the preview, where it points at
// one that is there.
[[nodiscard]] const domain::PreviewTable* pointed_at(const domain::SchemaPreview& preview, const GatheredKey& key);

// The columns of the table it points at, one for each of its own, in order.
[[nodiscard]] QStringList pointed_at_columns(const domain::SchemaPreview& preview, const GatheredKey& key);

// Its own columns, by name, in order.
[[nodiscard]] QStringList own_columns(const domain::SchemaPreview& preview, const GatheredKey& key);

// A foreign key in words: its columns, then the table and columns it points
// at. With its own table named, as it stands under Relationships; without,
// as it stands under the table that holds it.
[[nodiscard]] QString key_words(const domain::SchemaPreview& preview, const GatheredKey& key, bool with_table);

// One end of a key in words: a table and its column, Student.CourseID, or a
// table and its columns, Shipment(InvoiceNo, Year).
[[nodiscard]] QString end_words(const QString& table, const QStringList& columns);

// The rows that make a table's primary key, in the order the key itself is in
// (PreviewTable::primary_key), which need not be the order the table lists
// them in: one for a simple key, several for a composite one, none where the
// table has no key. A row the conversion leaves out is never part of it.
[[nodiscard]] std::vector<std::size_t> primary_key_rows(const domain::PreviewTable& table);

// What letting go of a connection would do (Zain, 2026-10-01, Stage 5 of the
// Schema workspace), worked out from the schema before anything is changed so
// that it can be asked about first. The row Connect started on is the key
// being referenced, and stays exactly what it is; the table it was let go on
// is the one that refers to it, and the row it was let go on, where it was let
// go on one, is the column chosen to hold the foreign key. Read by identity
// and position, never by name: a name only proposes which column to offer.
struct ConnectPlan {
    enum class Kind {
        Refused,       // nothing can be made; why says why
        Exists,        // this foreign key is already there; why says so
        UseColumn,     // an existing ordinary column is to hold it
        UseKeyColumn,  // an existing primary-key column is to hold it as well
        MakeColumn,    // no column is there to hold it; one called name is to be made
        NameColumn,    // the name a new one would have is taken; one is to be made under a name
                       // chosen by hand, name proposing one and why saying which name is taken
    };
    Kind kind = Kind::Refused;
    QString why;
    std::size_t referenced = 0;
    std::size_t key = 0;
    std::size_t referencing = 0;
    std::optional<std::size_t> column;
    QString name;
};
[[nodiscard]] ConnectPlan plan_connection(const domain::SchemaPreview& preview, std::size_t from_table,
                                          std::size_t from_row, std::size_t to_table,
                                          std::optional<std::size_t> to_row);

// The other way a connection let go on a primary-key row can be made (Zain,
// 2026-10-01): the key let go on is left exactly as it is, and its table
// refers to the key started on through a column of its own -- the column a
// connection let go on the table itself would use or make, found by the same
// rules in the same order: a foreign key there already to this key, then a
// column already called what a new one would be, then a new one. A name
// already taken is never taken over: where the column that has it cannot hold
// the key -- a key, one of another type, one referring elsewhere -- a new
// column is still made, under a name chosen by hand (Zain, 2026-10-01), the
// referenced table's name and its key's proposed.
[[nodiscard]] ConnectPlan plan_new_column(const domain::SchemaPreview& preview, std::size_t from_table,
                                          std::size_t from_row, std::size_t to_table);

} // namespace erdflow::desktop
