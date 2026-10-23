// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "domain/model.hpp"
#include "domain/schema_preview.hpp"

#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace erdflow::domain {

// What Practice Data does with its rows (PracticeData, in model.hpp): which
// columns hold values, what a value or a row breaks, and the rows a table
// starts with. Worked out from the schema each time, as the schema itself is.

// The columns of a table that hold values: everything but a column the
// conversion lists only to show it left it out, and anything with no identity
// to keep a value against. Positions in the table's columns.
[[nodiscard]] std::vector<std::size_t> practice_columns(const PreviewTable& table);

// What a row holds in one column.
[[nodiscard]] PracticeValue practice_value(const PracticeRow& row, const PreviewColumn& column);

// Which rule a value or a row breaks.
enum class PracticeRule { PrimaryKey, ForeignKey, Unique, Required, Type, Counted, Referenced };

// What is wrong, said for somebody learning what the rule is for.
struct PracticeProblem {
    PracticeRule rule = PracticeRule::Type;
    // The table it is in, by its place in the preview; the row, by its place
    // in that table's rows; and the column, by its place in the table's
    // columns.
    std::size_t table = 0;
    std::size_t row = 0;
    std::size_t column = 0;
    // "Primary Key violation", and then what happened and why it matters.
    std::string title;
    std::string message;
};

// Whether a value can be held by a column of its type at all, and if not, why.
// NULL is a question for the column's rules, not its type, and passes here.
[[nodiscard]] std::optional<std::string> type_problem(const PreviewColumn& column, const PracticeValue& value);

// Everything a table's rows break, for marking them. Each row and column is
// named once for each rule it breaks. A duplicate is marked on the later of
// the rows that share a value, so the row that had it first reads as fine.
[[nodiscard]] std::vector<PracticeProblem> practice_problems(const SchemaPreview& preview, const PracticeData& data,
                                                             std::size_t table);

// What a change would break, worked out from the data before it and after it,
// or nothing where it breaks nothing new. `table` is where the change was made;
// `row` and `columns` say which values it gave, where it gave any (a deleted
// row gives none). A row that points at a row which the change took away is
// always reported, wherever it is.
[[nodiscard]] std::optional<PracticeProblem> practice_refusal(const SchemaPreview& preview,
                                                              const PracticeData& before, const PracticeData& after,
                                                              std::size_t table, std::optional<std::size_t> row,
                                                              const std::vector<std::size_t>& columns);

// The value a column that counts itself up gives the next row: one more than
// the largest it holds, or 1.
[[nodiscard]] std::string next_counted_value(const PracticeTable& table, const PreviewColumn& column);

// Rows to start a table with: a few, of the column's own type, keyed one,
// two, three, and pointing at rows that exist in the tables they refer to.
// They are made from what the schema says and nothing else, so the same schema
// always starts with the same rows. Reset puts them back.
[[nodiscard]] std::vector<PracticeRow> starter_rows(const SchemaPreview& preview, const PracticeData& data,
                                                    std::size_t table);

// The tables a schema converts into, given the rows they start with. A table
// that already has rows keeps them; tables are filled in an order that puts a
// table before those that refer to it, so there is something to refer to.
[[nodiscard]] PracticeData converted_to_practice(const SchemaPreview& preview, PracticeData data);

// Values compared as the column's type compares them: 7, 07 and +7 are the same
// whole number. Text is compared as it was typed.
[[nodiscard]] std::string comparable(const PreviewColumn& column, const std::string& value);

// How a value is written in a sentence: in quotes for text, as it is for a
// number, and NULL for nothing.
[[nodiscard]] std::string spoken(const PreviewColumn& column, const PracticeValue& value);

} // namespace erdflow::domain
