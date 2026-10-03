// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "app/desktop/schema_explorer.hpp"

#include "app/desktop/schema_facts.hpp"
#include "app/desktop/schema_view.hpp"

#include <QByteArray>
#include <QFont>
#include <QStandardItem>
#include <QStringList>

#include <algorithm>
#include <type_traits>
#include <vector>

namespace erdflow::desktop {
namespace {

QString text(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }

QString hex(const domain::Uuid& id) {
    return QString::fromLatin1(QByteArray(reinterpret_cast<const char*>(id.bytes.data()), 16).toHex());
}

QString column_key(const SchemaColumnRef& column) {
    const auto source = std::visit([](const auto& what) -> QString {
        using What = std::decay_t<decltype(what)>;
        if constexpr (std::is_same_v<What, domain::SchemaColumnId>) return "own:" + hex(what.value);
        else if constexpr (std::is_same_v<What, domain::ForeignKeyColumn>)
            return "key:" + hex(what.key.value) + ":" + QString::number(what.part);
        else if constexpr (std::is_same_v<What, GeneratedKey>) return QStringLiteral("generated");
        else if constexpr (std::is_same_v<What, Discriminator>) return QStringLiteral("discriminator");
        else return "from:" + hex(what.attribute.value);
    }, column.source);
    return "column:" + hex(column.table.value) + "/" + source;
}

} // namespace

QString schema_key(const SchemaSelection& chosen) {
    if (const auto* one = std::get_if<ChosenTable>(&chosen)) return "table:" + hex(one->table.value);
    if (const auto* several = std::get_if<ChosenTables>(&chosen)) {
        QStringList tables;
        for (const auto id : several->tables) tables << hex(id.value);
        return "tables:" + tables.join(',');
    }
    if (const auto* column = std::get_if<ChosenColumn>(&chosen)) return column_key(column->column);
    if (const auto* key = std::get_if<ChosenForeignKey>(&chosen)) return "key:" + hex(key->key.value);
    return {};
}

void fill_schema_explorer(QStandardItemModel& model, const SchemaView& view, const SchemaExplorerLook& look) {
    model.clear();
    const auto& preview = view.preview();
    const auto keys = gather_keys(preview);

    // A group is a heading over what it holds, and stands for nothing to be
    // chosen, as the diagram's groups do.
    const auto group = [](const QIcon& icon, const QString& words, const QString& key) {
        auto* item = new QStandardItem(icon, words);
        item->setFlags(Qt::ItemIsEnabled);
        item->setData(key, Qt::UserRole);
        return item;
    };
    const auto counted = [](QStandardItem* item, std::size_t count) {
        item->setData(static_cast<int>(count), explorer_count_role);
    };
    // A row that stands for something on the schema, chosen by it.
    const auto chosen = [](QStandardItem* item, const SchemaSelection& what, const QString& tip) {
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        item->setData(schema_key(what), Qt::UserRole);
        item->setData(QVariant::fromValue(what), schema_choice_role);
        item->setToolTip(tip);
    };
    // A column, wherever it is listed. Its roles are said together at the end
    // of the row, since a column can be a primary key and a foreign key at
    // once; its mark is the key where it is one, as on the schema, and the
    // key and the link side by side where it is both (Zain, 2026-10-01), so
    // neither role hides the other.
    const auto column_row = [&](std::size_t t, std::size_t row) {
        const auto& column = preview.tables[t].columns[row];
        const auto name = text(column.name);
        const auto& mark = column.primary_key && column.foreign_key ? look.both
                         : column.primary_key                       ? look.key
                         : column.foreign_key                       ? look.link
                                                                    : look.none;
        auto* item = new QStandardItem(mark, name);
        QStringList roles;
        QStringList said;
        if (column.primary_key) { roles << "PK"; said << "primary key"; }
        if (column.foreign_key) { roles << "FK"; said << "foreign key"; }
        if (!roles.isEmpty()) item->setData(roles.join(' '), explorer_note_role);
        // A row the conversion leaves out is shown, as the schema shows it, but
        // in italics and never counted: it is not a column of the table.
        auto tip = name + (said.isEmpty() ? QString() : " — " + said.join(" and "));
        if (column.ignored) {
            auto slanted = item->font();
            slanted.setItalic(true);
            item->setFont(slanted);
            tip = name + " — not a column: the conversion leaves it out";
        }
        if (const auto handle = view.column_ref(t, row)) {
            chosen(item, ChosenColumn{*handle}, tip);
        } else {
            item->setFlags(Qt::ItemIsEnabled);
            item->setToolTip(tip);
        }
        return item;
    };
    const auto key_row = [&](const GatheredKey& key, bool with_table) {
        auto* item = new QStandardItem(look.link, key_words(preview, key, with_table));
        chosen(item, ChosenForeignKey{key.id}, key_words(preview, key, true));
        return item;
    };

    auto* root = group(QIcon(), QStringLiteral("Schema"), QStringLiteral("schema"));
    model.appendRow(root);

    auto* tables = group(look.table, QStringLiteral("Tables"), QStringLiteral("tables"));
    counted(tables, preview.tables.size());
    root->appendRow(tables);
    for (std::size_t t = 0; t < preview.tables.size(); ++t) {
        const auto& table = preview.tables[t];
        const auto name = text(table.name);
        const auto own = schema_key(ChosenTable{table.id});
        auto* item = new QStandardItem(look.table, name);
        chosen(item, ChosenTable{table.id}, name);
        tables->appendRow(item);

        // Columns, in the order the table lists them.
        if (!table.columns.empty()) {
            auto* columns = group(look.none, QStringLiteral("Columns"), own + "/columns");
            counted(columns, static_cast<std::size_t>(std::count_if(
                table.columns.begin(), table.columns.end(), [](const auto& column) { return !column.ignored; })));
            item->appendRow(columns);
            for (std::size_t row = 0; row < table.columns.size(); ++row) columns->appendRow(column_row(t, row));
        }
        // The primary key, whole: every column of it, which is more than one
        // where the key is composite, and counted then so that it shows.
        const auto key_columns = primary_key_rows(table);
        if (!key_columns.empty()) {
            auto* primary = group(look.key, QStringLiteral("Primary Key"), own + "/primary-key");
            if (key_columns.size() > 1) counted(primary, key_columns.size());
            item->appendRow(primary);
            for (const auto row : key_columns) primary->appendRow(column_row(t, row));
        }
        // The foreign keys its rows hold, each one key whatever its width.
        std::vector<const GatheredKey*> held;
        for (const auto& key : keys)
            if (key.table == t) held.push_back(&key);
        if (!held.empty()) {
            auto* foreign = group(look.link, QStringLiteral("Foreign Keys"), own + "/foreign-keys");
            counted(foreign, held.size());
            item->appendRow(foreign);
            for (const auto* key : held) foreign->appendRow(key_row(*key, false));
        }
    }

    // Every foreign key again, each named with the table that holds it: the
    // same keys as under the tables, gathered in one place.
    auto* relationships = group(look.relationships, QStringLiteral("Relationships"), QStringLiteral("relationships"));
    counted(relationships, keys.size());
    root->appendRow(relationships);
    for (const auto& key : keys) relationships->appendRow(key_row(key, true));
}

} // namespace erdflow::desktop
