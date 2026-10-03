// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/schema_facts.hpp"

#include "app/desktop/schema_view.hpp"

#include <algorithm>

namespace erdflow::desktop {
namespace {

QString text(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }

} // namespace

std::vector<GatheredKey> gather_keys(const domain::SchemaPreview& preview) {
    std::vector<GatheredKey> keys;
    for (std::size_t t = 0; t < preview.tables.size(); ++t)
        for (std::size_t row = 0; row < preview.tables[t].columns.size(); ++row) {
            const auto& column = preview.tables[t].columns[row];
            if (!column.key_id || column.ignored) continue;
            const auto found = std::find_if(keys.begin(), keys.end(), [&](const GatheredKey& one) {
                return one.id == *column.key_id && one.table == t;
            });
            if (found != keys.end()) found->rows.push_back(row);
            else keys.push_back(GatheredKey{*column.key_id, t, {row}});
        }
    for (auto& key : keys)
        std::stable_sort(key.rows.begin(), key.rows.end(), [&](std::size_t a, std::size_t b) {
            const auto& columns = preview.tables[key.table].columns;
            return columns[a].reference_part < columns[b].reference_part;
        });
    return keys;
}

const domain::PreviewTable* pointed_at(const domain::SchemaPreview& preview, const GatheredKey& key) {
    const auto& first = preview.tables[key.table].columns[key.rows.front()];
    return first.references && *first.references < preview.tables.size() ? &preview.tables[*first.references]
                                                                          : nullptr;
}

QStringList pointed_at_columns(const domain::SchemaPreview& preview, const GatheredKey& key) {
    const auto* to = pointed_at(preview, key);
    QStringList theirs;
    for (const auto row : key.rows) {
        const auto& column = preview.tables[key.table].columns[row];
        theirs << (to && column.references_column < to->columns.size()
                       ? text(to->columns[column.references_column].name) : QStringLiteral("?"));
    }
    return theirs;
}

QStringList own_columns(const domain::SchemaPreview& preview, const GatheredKey& key) {
    QStringList own;
    for (const auto row : key.rows) own << text(preview.tables[key.table].columns[row].name);
    return own;
}

QString key_words(const domain::SchemaPreview& preview, const GatheredKey& key, bool with_table) {
    const auto* to = pointed_at(preview, key);
    const auto own = own_columns(preview, key);
    const auto theirs = pointed_at_columns(preview, key);
    const auto target = to ? text(to->name) : QStringLiteral("?");
    const auto holder = with_table ? text(preview.tables[key.table].name) : QString();
    if (key.rows.size() == 1)
        return (with_table ? holder + "." : QString()) + own.front() + " → " + target + "." + theirs.front();
    return holder + "(" + own.join(", ") + ") → " + target + "(" + theirs.join(", ") + ")";
}

QString end_words(const QString& table, const QStringList& columns) {
    if (columns.size() == 1) return table + "." + columns.front();
    return table + "(" + columns.join(", ") + ")";
}

std::vector<std::size_t> primary_key_rows(const domain::PreviewTable& table) {
    return table.primary_key;
}

namespace {

// Both plans, which differ only in what is said where the name a new column
// would have is a key's: let go on the table, the way to make that key a
// foreign key too is to let go on its row; let go on a key's row already and
// asked for a new column, it is that the name is taken.
ConnectPlan plan_for(const domain::SchemaPreview& preview, std::size_t from_table, std::size_t from_row,
                     std::size_t to_table, std::optional<std::size_t> to_row, bool new_column) {
    ConnectPlan plan;
    plan.referenced = from_table;
    plan.key = from_row;
    plan.referencing = to_table;
    const auto refused = [&](const QString& why) {
        plan.kind = ConnectPlan::Kind::Refused;
        plan.why = why;
        return plan;
    };
    if (from_table >= preview.tables.size() || to_table >= preview.tables.size()
        || from_row >= preview.tables[from_table].columns.size()
        || (to_row && *to_row >= preview.tables[to_table].columns.size()))
        return refused(QStringLiteral("The schema has changed under the line. Draw it again."));
    const auto& home = preview.tables[from_table];
    const auto& start = home.columns[from_row];
    const auto& holder = preview.tables[to_table];
    const auto key = text(home.name) + "." + text(start.name);
    // The row a connection starts on is the key it refers to. Anything else
    // is turned away rather than made a key, or made the foreign key itself.
    if (!start.primary_key || start.ignored)
        return refused(QString("Cannot create relationship. %1 is not %2's primary key, and a connection starts "
                               "on the key being referenced: start on a primary key and let go on the table "
                               "that refers to it.").arg(key, text(home.name)));
    if (primary_key_rows(home).size() > 1)
        return refused(QString("%1 has a primary key of several columns. Making a foreign key to a composite "
                               "key by hand is not supported yet, and a key to part of it would be wrong.")
                           .arg(text(home.name)));
    const auto typed = [](const domain::PreviewColumn& column) {
        return column.type == domain::LogicalType::Unset ? QStringLiteral("not given a type yet") : written_type(column);
    };
    // One column of the table let go on, offered to hold the foreign key.
    const auto offered = [&](std::size_t row) {
        const auto& column = holder.columns[row];
        const auto called = text(holder.name) + "." + text(column.name);
        if (to_table == from_table && row == from_row)
            return refused(QString("A column cannot reference itself. Let go on the table that refers to %1, "
                                   "or on the column that is to hold it.").arg(key));
        if (column.ignored) return refused(QString("%1 is not a column of %2.").arg(text(column.name), text(holder.name)));
        if (column.foreign_key && column.references) {
            if (*column.references == from_table && column.references_column == from_row) {
                plan.kind = ConnectPlan::Kind::Exists;
                plan.why = QString("Relationship already exists: %1 already references %2.").arg(called, key);
                return plan;
            }
            const auto& elsewhere = preview.tables[*column.references];
            const auto there = text(elsewhere.name) + "."
                             + (column.references_column < elsewhere.columns.size()
                                    ? text(elsewhere.columns[column.references_column].name) : QStringLiteral("?"));
            return refused(QString("%1 already references %2. A column holds one foreign key; remove that one "
                                   "first to make it reference %3.").arg(called, there, key));
        }
        if (!domain::takes_key_type(column.type, column.length, column.scale, start.type, start.length, start.scale))
            return refused(QString("Cannot use %1: it is %2, and %3 is %4. A foreign key has its key's type, so "
                                   "change one of them first.").arg(called, typed(column), key, typed(start)));
        plan.kind = column.primary_key ? ConnectPlan::Kind::UseKeyColumn : ConnectPlan::Kind::UseColumn;
        plan.column = row;
        return plan;
    };
    // A new column whose name is taken by one that cannot hold the key is
    // still made, under a name chosen by hand (Zain, 2026-10-01): the
    // referenced table's name and its key's is proposed -- the key's alone
    // where it starts with the table's already -- numbered on where that is
    // taken too.
    const auto renamed = [&](const QString& name) {
        const auto taken = [&](const QString& called) {
            return std::any_of(holder.columns.begin(), holder.columns.end(),
                               [&](const domain::PreviewColumn& one) { return text(one.name) == called; });
        };
        const auto key_name = text(start.name);
        const auto base = key_name.startsWith(text(home.name), Qt::CaseInsensitive) ? key_name
                                                                                      : text(home.name) + key_name;
        auto proposed = base;
        for (int n = 2; taken(proposed); ++n) proposed = base + QString::number(n);
        plan.kind = ConnectPlan::Kind::NameColumn;
        plan.column.reset();
        plan.name = proposed;
        plan.why = QString("The default foreign key name \"%1\" is already used in %2.").arg(name, text(holder.name));
        return plan;
    };
    if (to_row) return offered(*to_row);
    // Let go on the table itself: a foreign key already there to this key,
    // then a column already called what a new one would be, then a new one.
    for (std::size_t row = 0; row < holder.columns.size(); ++row) {
        const auto& column = holder.columns[row];
        if (column.foreign_key && column.references == from_table && column.references_column == from_row) {
            plan.kind = ConnectPlan::Kind::Exists;
            plan.why = QString("Relationship already exists: %1.%2 already references %3.")
                           .arg(text(holder.name), text(column.name), key);
            return plan;
        }
    }
    // Named for the key it holds, as the conversion names one; a key into its
    // own table cannot share its key's name, and is its Parent's, as the
    // conversion names that too.
    const auto name = to_table == from_table ? "Parent" + text(start.name) : text(start.name);
    for (std::size_t row = 0; row < holder.columns.size(); ++row) {
        const auto& column = holder.columns[row];
        if (text(column.name) != name || column.ignored) continue;
        // A key is never taken to hold a foreign key unless it is the row let
        // go on, chosen by hand.
        if (column.primary_key && !column.foreign_key) {
            if (new_column) return renamed(name);
            return refused(QString("%1 already has %2, in its primary key. To make it a foreign key as well, "
                                   "let go on that row.").arg(text(holder.name), name));
        }
        if (new_column) {
            const auto found = offered(row);
            return found.kind == ConnectPlan::Kind::Refused ? renamed(name) : found;
        }
        return offered(row);
    }
    plan.kind = ConnectPlan::Kind::MakeColumn;
    plan.name = name;
    return plan;
}

} // namespace

ConnectPlan plan_connection(const domain::SchemaPreview& preview, std::size_t from_table, std::size_t from_row,
                            std::size_t to_table, std::optional<std::size_t> to_row) {
    return plan_for(preview, from_table, from_row, to_table, to_row, false);
}

ConnectPlan plan_new_column(const domain::SchemaPreview& preview, std::size_t from_table, std::size_t from_row,
                            std::size_t to_table) {
    return plan_for(preview, from_table, from_row, to_table, std::nullopt, true);
}

} // namespace erdflow::desktop
