// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "infrastructure/project_store.hpp"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace erdflow::infrastructure {
using namespace domain;
namespace {
// Version 2 added connector shapes; version 3 added associative relationships
// and participants that may target one; version 4 added specializations, and
// version 5 recorded which way each one was read; version 6 lets a placed
// triangle wait for its supertype.
// Earlier versions remain readable; the format specification states the
// compatibility rule for each.
// Version 11 adds pictures and notes, the visual aids placed on the canvas;
// version 12 lets an element's surface be see-through by a percentage;
// version 13 adds weak entities and identifying relationships; version 14 adds
// the paper the diagram is drawn on; version 15 lets a note be a plain one, a
// single character drawn bare on the diagram; version 16 adds review comments,
// pinned to elements, to the lines between them, or into a range of the text
// somebody wrote; version 17 adds what a model says about what it will become
// -- a conceptual mode, an attribute's logical type and length, whether it
// identifies, is required or is unique, and the comment a table or column
// carries into the schema; version 18 drops the mode, because there is one
// model and no modes, and what a panel shows is a user's preference rather than
// anything the document holds; version 19 records whether a relationship side's
// cardinality and participation were chosen or are merely what a new side
// starts as, which conversion needs in order to tell a decided M:M from two
// untouched defaults; version 20 replaces the small portable type set with the
// whole SQL catalogue, gives a decimal its scale, and records the answers to
// the questions a conversion cannot decide for itself; version 21 records
// where the schema has been edited away from the diagram it came from, which
// ADR-010 allows and which therefore has to survive being saved; version 22
// records how the schema has been arranged by hand, for the same reason the
// diagram's own layout is recorded -- it is work; version 24 records what a
// key the conversion invented has been renamed to, which renames every
// foreign key that points at it; version 23 records how tall
// a table has been pulled as well as how wide, since a table answers to all
// four of its edges; version 25 records auto-incrementing columns; version 26
// gives generated relations stable identities of their own; version 27 adds
// the description captured with a project's other creation details; version
// 28 records which bridges were chosen to be keyed by their participants'
// foreign keys; version 29 keeps the size an element's name is drawn for;
// version 30 holds tables and foreign keys made on the schema itself, in a
// project that starts from its schema; version 31 keeps a name typed over a
// foreign key the conversion made; version 32 keeps each schema column's place
// in its table's primary key, so a key of several columns no longer follows
// the order the table lists them in; version 33 keeps the order somebody gave
// the columns of a table worked out from the diagram.
constexpr int current_format_version = 34;
QString text(const std::string& value) { return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size())); }
Uuid bytes_of(const QUuid& value) {
    Uuid result;
    const auto bytes = value.toRfc4122();
    std::memcpy(result.bytes.data(), bytes.constData(), result.bytes.size());
    return result;
}
[[noreturn]] void invalid(const QString& detail) { throw std::runtime_error(detail.toStdString()); }
QJsonObject object(const QJsonValue& value, std::initializer_list<const char*> keys) {
    if (!value.isObject()) invalid("Expected a project object.");
    const auto result = value.toObject();
    if (result.size() != static_cast<qsizetype>(keys.size())) invalid("Missing or unsupported project fields. This file cannot be safely edited.");
    for (const auto* key : keys) if (!result.contains(QLatin1String(key))) invalid("Missing a required project field.");
    return result;
}
std::string string(const QJsonValue& value, std::size_t max = max_name_bytes) {
    if (!value.isString()) invalid("Expected a text field.");
    const auto unicode = value.toString();
    if (!unicode.isValidUtf16()) invalid("A text field contains an unpaired Unicode surrogate.");
    const auto result = unicode.toUtf8();
    if (static_cast<std::size_t>(result.size()) > max) invalid("A text field exceeds the project limit.");
    return result.toStdString();
}
QJsonArray array(const QJsonValue& value) {
    if (!value.isArray()) invalid("Expected an array.");
    auto result = value.toArray();
    if (result.size() > static_cast<qsizetype>(max_elements)) invalid("The project exceeds its element limit.");
    return result;
}
Uuid parse_id(const QJsonValue& value) {
    const auto input = value.isString() ? value.toString() : QString{};
    const QUuid id(input);
    if (input.size() != 36 || id.toString(QUuid::WithoutBraces) != input)
        invalid("Project identifiers must use canonical lowercase UUID text.");
    auto result = bytes_of(id);
    if (!result.valid()) invalid("Project identifiers must be UUIDv7 values.");
    return result;
}
QJsonObject reference(const ElementRef& ref) {
    const char* type = std::holds_alternative<EntityId>(ref) ? "entity"
        : std::holds_alternative<AttributeId>(ref) ? "attribute"
        : std::holds_alternative<SpecializationId>(ref) ? "specialization"
        : std::holds_alternative<PictureId>(ref) ? "picture"
        : std::holds_alternative<RelationId>(ref) ? "relation"
        : std::holds_alternative<NoteId>(ref) ? "note" : "relationship";
    return {{"type", QLatin1String(type)}, {"id", uuid_text(uuid(ref))}};
}
QJsonObject connector_reference(const ConnectorRef& ref) {
    const bool attribute = std::holds_alternative<AttributeId>(ref);
    const auto id = attribute ? std::get<AttributeId>(ref).value : std::get<ParticipantId>(ref).value;
    return {{"type", QLatin1String(attribute ? "attribute" : "participant")}, {"id", uuid_text(id)}};
}
ConnectorRef parse_connector_reference(const QJsonValue& value) {
    auto o = object(value, {"type", "id"});
    auto id = parse_id(o["id"]);
    const auto type = string(o["type"]);
    if (type == "attribute") return AttributeId{id};
    if (type == "participant") return ParticipantId{id};
    invalid("Unsupported connector link type.");
}
// What a comment is pinned to. The three kinds are told apart by a word rather
// than by shape, because an attribute identifier means one thing as an element
// and another as the line that owns it, and a reader must never have to guess
// which was meant.
// The type catalogue, written by its SQL name. The names are what the file
// carries, so they are spelled once here and read back by the same table.
struct TypeName { LogicalType type; const char* name; };
constexpr std::array<TypeName, 38> type_names{{
    {LogicalType::Unset, "unset"},
    {LogicalType::Int, "int"}, {LogicalType::BigInt, "bigint"},
    {LogicalType::SmallInt, "smallint"}, {LogicalType::TinyInt, "tinyint"},
    {LogicalType::Bit, "bit"}, {LogicalType::Decimal, "decimal"},
    {LogicalType::Numeric, "numeric"}, {LogicalType::Money, "money"},
    {LogicalType::SmallMoney, "smallmoney"},
    {LogicalType::Float, "float"}, {LogicalType::Real, "real"},
    {LogicalType::Char, "char"}, {LogicalType::Varchar, "varchar"},
    {LogicalType::VarcharMax, "varchar(max)"}, {LogicalType::Text, "text"},
    {LogicalType::NChar, "nchar"}, {LogicalType::NVarchar, "nvarchar"},
    {LogicalType::NVarcharMax, "nvarchar(max)"}, {LogicalType::NText, "ntext"},
    {LogicalType::Binary, "binary"}, {LogicalType::Varbinary, "varbinary"},
    {LogicalType::VarbinaryMax, "varbinary(max)"}, {LogicalType::Image, "image"},
    {LogicalType::Date, "date"}, {LogicalType::Time, "time"},
    {LogicalType::DateTime, "datetime"}, {LogicalType::DateTime2, "datetime2"},
    {LogicalType::DateTimeOffset, "datetimeoffset"}, {LogicalType::SmallDateTime, "smalldatetime"},
    {LogicalType::UniqueIdentifier, "uniqueidentifier"}, {LogicalType::Xml, "xml"},
    {LogicalType::RowVersion, "rowversion"}, {LogicalType::HierarchyId, "hierarchyid"},
    {LogicalType::SqlVariant, "sql_variant"}, {LogicalType::Cursor, "cursor"},
    {LogicalType::Table, "table"}, {LogicalType::Geometry, "geometry"},
}};

QLatin1String isa_name(IsaStrategy strategy) {
    switch (strategy) {
    case IsaStrategy::SingleTable: return QLatin1String("single_table");
    case IsaStrategy::PerConcrete: return QLatin1String("per_concrete");
    case IsaStrategy::PerSubclass: break;
    }
    return QLatin1String("per_subclass");
}
IsaStrategy parse_isa(const QJsonValue& value) {
    const auto name = string(value);
    if (name == "per_subclass") return IsaStrategy::PerSubclass;
    if (name == "single_table") return IsaStrategy::SingleTable;
    if (name == "per_concrete") return IsaStrategy::PerConcrete;
    invalid("Unsupported mapping strategy.");
}
QLatin1String composite_name(CompositeMode mode) {
    switch (mode) {
    case CompositeMode::Whole: return QLatin1String("whole");
    case CompositeMode::Both: return QLatin1String("both");
    case CompositeMode::Parts: break;
    }
    return QLatin1String("parts");
}
CompositeMode parse_composite(const QJsonValue& value) {
    const auto name = string(value);
    if (name == "parts") return CompositeMode::Parts;
    if (name == "whole") return CompositeMode::Whole;
    if (name == "both") return CompositeMode::Both;
    invalid("Unsupported composite decision.");
}

QLatin1String bridge_key_name(BridgeKey keyed) {
    return QLatin1String(keyed == BridgeKey::Own ? "own" : "pair");
}

BridgeKey parse_bridge_key(const QJsonValue& value) {
    const auto name = string(value);
    if (name == "pair") return BridgeKey::Pair;
    if (name == "own") return BridgeKey::Own;
    invalid("Unsupported bridge-key decision.");
}

// Defined below, beside the other readers.
ElementRef parse_ref(const QJsonValue& value);

// Each decision is written as a list of {id, value} rather than as an object
// keyed by identity, so the file says plainly what it is about and nothing
// depends on how a map orders itself.
template<class Map, class Value>
QJsonArray decision_list(const Map& answers, Value&& value_of) {
    QJsonArray out;
    for (const auto& [key, answer] : answers)
        out.append(QJsonObject{{"id", uuid_text(key.value)}, {"value", value_of(answer)}});
    return out;
}

QJsonObject encode_decisions(const ConversionDecisions& decided) {
    QJsonArray names;
    for (const auto& [relation, chosen] : decided.table_name)
        names.append(QJsonObject{{"relation", uuid_text(relation.value)}, {"value", text(chosen)}});
    return QJsonObject{
        {"naming", QLatin1String(decided.naming == TableNaming::AsDrawn ? "as_drawn" : "plural")},
        {"isa", decision_list(decided.isa, [](IsaStrategy s) { return QJsonValue(isa_name(s)); })},
        {"composite", decision_list(decided.composite, [](CompositeMode m) { return QJsonValue(composite_name(m)); })},
        {"one_to_one_key", decision_list(decided.one_to_one_key,
            [](ParticipantId side) { return QJsonValue(uuid_text(side.value)); })},
        {"junction_name", decision_list(decided.junction_name,
            [](const std::string& chosen) { return QJsonValue(text(chosen)); })},
        {"bridge_key", decision_list(decided.bridge_key,
            [](BridgeKey keyed) { return QJsonValue(bridge_key_name(keyed)); })},
        {"identifier", decision_list(decided.identifier,
            [](AttributeId chosen) { return QJsonValue(uuid_text(chosen.value)); })},
        {"table_name", names}};
}

ConversionDecisions parse_decisions(const QJsonValue& value, bool relational, bool keyed_bridges) {
    // Version 28 records what keys each bridge where somebody said. A file
    // written before it has no such list, which reads correctly as every
    // bridge keyed by the default, a separate fallback key.
    auto o = keyed_bridges
        ? object(value, {"naming", "isa", "composite", "one_to_one_key",
                         "junction_name", "bridge_key", "identifier", "table_name"})
        : object(value, {"naming", "isa", "composite", "one_to_one_key",
                         "junction_name", "identifier", "table_name"});
    ConversionDecisions decided;
    const auto naming = string(o["naming"]);
    if (naming == "as_drawn") decided.naming = TableNaming::AsDrawn;
    else if (naming == "plural") decided.naming = TableNaming::Plural;
    else invalid("Unsupported table naming convention.");

    for (const auto& item : array(o["isa"])) {
        auto entry = object(item, {"id", "value"});
        decided.isa.emplace(SpecializationId{parse_id(entry["id"])}, parse_isa(entry["value"]));
    }
    for (const auto& item : array(o["composite"])) {
        auto entry = object(item, {"id", "value"});
        decided.composite.emplace(AttributeId{parse_id(entry["id"])}, parse_composite(entry["value"]));
    }
    for (const auto& item : array(o["one_to_one_key"])) {
        auto entry = object(item, {"id", "value"});
        decided.one_to_one_key.emplace(RelationshipId{parse_id(entry["id"])},
                                       ParticipantId{parse_id(entry["value"])});
    }
    for (const auto& item : array(o["junction_name"])) {
        auto entry = object(item, {"id", "value"});
        decided.junction_name.emplace(RelationshipId{parse_id(entry["id"])}, string(entry["value"]));
    }
    if (keyed_bridges)
        for (const auto& item : array(o["bridge_key"])) {
            auto entry = object(item, {"id", "value"});
            decided.bridge_key.emplace(RelationshipId{parse_id(entry["id"])}, parse_bridge_key(entry["value"]));
        }
    for (const auto& item : array(o["identifier"])) {
        auto entry = object(item, {"id", "value"});
        decided.identifier.emplace(EntityId{parse_id(entry["id"])}, AttributeId{parse_id(entry["value"])});
    }
    for (const auto& item : array(o["table_name"])) {
        auto entry = relational ? object(item, {"relation", "value"})
                                : object(item, {"element", "value"});
        const auto relation = relational ? RelationId{parse_id(entry["relation"])}
                                         : relation_from(parse_ref(entry["element"]));
        decided.table_name.emplace(relation, string(entry["value"]));
    }
    return decided;
}

QLatin1String logical_type_name(LogicalType type) {
    for (const auto& entry : type_names)
        if (entry.type == type) return QLatin1String(entry.name);
    if (type == LogicalType::Geography) return QLatin1String("geography");
    return QLatin1String("unset");
}

// Before version 20 a file held a small portable set. Each of those names is
// the SQL type it always meant: a portable "text" was a varying string, so it
// reads as varchar, and a "boolean" was a bit.
LogicalType parse_logical_type(const QJsonValue& value, bool catalogued) {
    const auto name = string(value);
    if (!catalogued) {
        if (name == "unset") return LogicalType::Unset;
        if (name == "text") return LogicalType::Varchar;
        if (name == "integer") return LogicalType::Int;
        if (name == "decimal") return LogicalType::Decimal;
        if (name == "boolean") return LogicalType::Bit;
        if (name == "date") return LogicalType::Date;
        if (name == "datetime") return LogicalType::DateTime;
        if (name == "binary") return LogicalType::Varbinary;
        if (name == "uuid") return LogicalType::UniqueIdentifier;
        invalid("Unsupported logical type.");
    }
    for (const auto& entry : type_names)
        if (name == entry.name) return entry.type;
    if (name == "geography") return LogicalType::Geography;
    invalid("Unsupported logical type.");
}

std::uint32_t parse_length(const QJsonValue& value) {
    if (!value.isDouble()) invalid("A logical length must be a number.");
    const auto number = value.toDouble();
    if (!std::isfinite(number) || number < 0 || number != std::floor(number) || number > max_logical_length)
        invalid("A logical length must be a whole, non-negative number within 1,000,000.");
    return static_cast<std::uint32_t>(number);
}
// A column's place in its table's primary key: whole, not negative, and no
// further down than a table could have columns. Validation then checks the
// places against the key they number.
std::uint32_t parse_key_order(const QJsonValue& value) {
    if (!value.isDouble()) invalid("A column's place in its key must be a number.");
    const auto number = value.toDouble();
    if (!std::isfinite(number) || number < 0 || number != std::floor(number) || number > static_cast<double>(max_elements))
        invalid("A column's place in its key must be a whole, non-negative number.");
    return static_cast<std::uint32_t>(number);
}
// When an attribute was created, counted against the project's others:
// whole, not negative, and within the numbers an attribute can hold.
std::uint32_t parse_creation_order(const QJsonValue& value) {
    if (!value.isDouble()) invalid("An attribute's creation order must be a number.");
    const auto number = value.toDouble();
    if (!std::isfinite(number) || number < 0 || number != std::floor(number)
        || number > static_cast<double>(std::numeric_limits<std::uint32_t>::max()))
        invalid("An attribute's creation order must be a whole, non-negative number.");
    return static_cast<std::uint32_t>(number);
}
bool parse_flag(const QJsonValue& value, const char* what) {
    if (!value.isBool()) invalid(QString("The %1 flag must be true or false.").arg(QLatin1String(what)));
    return value.toBool();
}

// A column of a table worked out from the diagram, by what it was made from
// rather than by where it stands, tagged with its kind as a comment's target
// is.
QJsonObject column_identity_object(const ColumnIdentity& column) {
    return std::visit([](const auto& id) -> QJsonObject {
        using T = std::decay_t<decltype(id)>;
        if constexpr (std::is_same_v<T, AttributeId>)
            return {{"kind", QLatin1String("attribute")}, {"id", uuid_text(id.value)}};
        else if constexpr (std::is_same_v<T, SchemaColumnId>)
            return {{"kind", QLatin1String("column")}, {"id", uuid_text(id.value)}};
        else if constexpr (std::is_same_v<T, ForeignKeyColumn>)
            return {{"kind", QLatin1String("foreign_key")}, {"key", uuid_text(id.key.value)},
                    {"part", static_cast<double>(id.part)}};
        else if constexpr (std::is_same_v<T, InventedKeyColumn>)
            return {{"kind", QLatin1String("key")}, {"relation", uuid_text(id.relation.value)}};
        else
            return {{"kind", QLatin1String("discriminator")}, {"specialization", uuid_text(id.specialization.value)}};
    }, column);
}
ColumnIdentity parse_column_identity(const QJsonValue& value) {
    const auto kind = string(value.toObject().value("kind"));
    if (kind == "attribute") return AttributeId{parse_id(object(value, {"kind", "id"})["id"])};
    if (kind == "column") return SchemaColumnId{parse_id(object(value, {"kind", "id"})["id"])};
    if (kind == "foreign_key") {
        const auto o = object(value, {"kind", "key", "part"});
        return ForeignKeyColumn{ForeignKeyId{parse_id(o["key"])}, parse_length(o["part"])};
    }
    if (kind == "key") return InventedKeyColumn{RelationId{parse_id(object(value, {"kind", "relation"})["relation"])}};
    if (kind == "discriminator")
        return DiscriminatorColumn{SpecializationId{parse_id(object(value, {"kind", "specialization"})["specialization"])}};
    invalid("Unsupported column kind in a column order.");
}

// Where the schema has been edited away from the diagram. Written as its own
// section rather than folded into the conversion decisions, because the two
// are different kinds of fact: a decision answers a question the conversion
// cannot settle, and these are somebody editing its result. A file from before
// this existed simply has no section, which reads as no differences at all.
QJsonObject encode_schema(const SchemaOverrides& schema) {
    QJsonArray relations, foreign_keys;
    for (const auto& [id, table] : schema.relations)
        relations.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(table.name)},
            {"description", text(table.description)}, {"comment", text(table.comment)}});
    for (const auto& [id, key] : schema.foreign_keys)
        foreign_keys.append(QJsonObject{{"id", uuid_text(id.value)}, {"from", uuid_text(key.from.value)},
            {"to", uuid_text(key.to.value)}, {"column", uuid_text(key.column.value)}, {"target", uuid_text(key.target.value)}});
    QJsonArray added;
    for (const auto& [relation, columns] : schema.added) {
        QJsonArray of_table;
        for (const auto& column : columns)
            of_table.append(QJsonObject{{"id", uuid_text(column.id.value)}, {"name", text(column.name)},
                {"type", logical_type_name(column.logical_type)},
                {"length", static_cast<double>(column.length)},
                {"scale", static_cast<double>(column.scale)}, {"identifier", column.identifier},
                {"required", column.required}, {"unique", column.unique},
                {"auto_increment", column.auto_increment},
                {"comment", text(column.comment)},
                {"key_order", static_cast<double>(column.key_order)}});
        added.append(QJsonObject{{"relation", uuid_text(relation.value)}, {"columns", of_table}});
    }
    QJsonArray hidden;
    for (const auto& id : schema.hidden) hidden.append(uuid_text(id.value));
    QJsonArray keys;
    for (const auto& [relation, chosen] : schema.key_names)
        keys.append(QJsonObject{{"relation", uuid_text(relation.value)}, {"name", text(chosen)}});
    QJsonArray counting;
    for (const auto& relation : schema.counting_keys) counting.append(uuid_text(relation.value));
    QJsonArray key_names;
    for (const auto& [column, chosen] : schema.foreign_key_names)
        key_names.append(QJsonObject{{"key", uuid_text(column.key.value)},
                                     {"part", static_cast<double>(column.part)}, {"name", text(chosen)}});
    QJsonArray column_order;
    for (const auto& [relation, columns] : schema.column_order) {
        QJsonArray listed;
        for (const auto& column : columns) listed.append(column_identity_object(column));
        column_order.append(QJsonObject{{"relation", uuid_text(relation.value)}, {"columns", listed}});
    }
    return QJsonObject{{"added", added}, {"hidden", hidden}, {"keys", keys},
                       {"counting_keys", counting}, {"standalone", schema.standalone},
                       {"relations", relations}, {"foreign_keys", foreign_keys},
                       {"foreign_key_names", key_names}, {"column_order", column_order}};
}

SchemaOverrides parse_schema(const QJsonValue& value, bool named_keys, bool counted,
                            bool relational, bool native, bool renamed_keys, bool ordered_keys,
                            bool ordered_columns) {
    auto o = ordered_columns ? object(value, {"added", "hidden", "keys", "counting_keys", "standalone", "relations",
                                              "foreign_keys", "foreign_key_names", "column_order"})
           : renamed_keys ? object(value, {"added", "hidden", "keys", "counting_keys", "standalone", "relations",
                                           "foreign_keys", "foreign_key_names"})
           : native ? object(value, {"added", "hidden", "keys", "counting_keys", "standalone", "relations", "foreign_keys"})
           : counted ? object(value, {"added", "hidden", "keys", "counting_keys"})
           : named_keys ? object(value, {"added", "hidden", "keys"})
                        : object(value, {"added", "hidden"});
    SchemaOverrides schema;
    if (native) {
        schema.standalone = parse_flag(o["standalone"], "standalone");
        for (const auto& item : array(o["relations"])) {
            const auto row = object(item, {"id", "name", "description", "comment"});
            Relation table{RelationId{parse_id(row["id"])}, string(row["name"]),
                string(row["description"], max_description_bytes), string(row["comment"], max_comment_bytes)};
            if (!schema.relations.emplace(table.id, table).second) invalid("Duplicate relation identity.");
        }
        for (const auto& item : array(o["foreign_keys"])) {
            const auto row = object(item, {"id", "from", "to", "column", "target"});
            SchemaForeignKey key{ForeignKeyId{parse_id(row["id"])}, RelationId{parse_id(row["from"])},
                RelationId{parse_id(row["to"])}, SchemaColumnId{parse_id(row["column"])}, SchemaColumnId{parse_id(row["target"])} };
            if (!schema.foreign_keys.emplace(key.id, key).second) invalid("Duplicate foreign key identity.");
        }
    }
    for (const auto& item : array(o["added"])) {
        auto entry = relational ? object(item, {"relation", "columns"})
                                : object(item, {"element", "columns"});
        std::vector<SchemaColumn> columns;
        for (const auto& one : array(entry["columns"])) {
            auto c = ordered_keys
                ? object(one, {"id", "name", "type", "length", "scale", "identifier",
                               "required", "unique", "comment", "auto_increment", "key_order"})
                : counted
                ? object(one, {"id", "name", "type", "length", "scale", "identifier",
                               "required", "unique", "comment", "auto_increment"})
                : object(one, {"id", "name", "type", "length", "scale", "identifier",
                               "required", "unique", "comment"});
            SchemaColumn column{.id = SchemaColumnId{parse_id(c["id"])},
                                .name = string(c["name"], max_name_bytes)};
            column.logical_type = parse_logical_type(c["type"], true);
            column.length = parse_length(c["length"]);
            column.scale = parse_length(c["scale"]);
            column.identifier = parse_flag(c["identifier"], "identifier");
            column.required = parse_flag(c["required"], "required");
            column.unique = parse_flag(c["unique"], "unique");
            if (counted) column.auto_increment = parse_flag(c["auto_increment"], "auto_increment");
            column.comment = string(c["comment"], max_comment_bytes);
            if (ordered_keys) column.key_order = parse_key_order(c["key_order"]);
            columns.push_back(std::move(column));
        }
        if (columns.empty()) continue;
        // Before version 32 a key of several columns was read in the order the
        // table listed them, so that is the order it is given now, once: the
        // file means exactly what it meant.
        if (!ordered_keys) number_primary_key(columns);
        const auto owner = relational ? RelationId{parse_id(entry["relation"])}
                                      : relation_from(parse_ref(entry["element"]));
        if (!schema.added.emplace(owner, std::move(columns)).second)
            invalid("Duplicate schema column owner.");
    }
    for (const auto& item : array(o["hidden"])) schema.hidden.insert(AttributeId{parse_id(item)});
    if (named_keys)
        for (const auto& item : array(o["keys"])) {
            auto entry = relational ? object(item, {"relation", "name"})
                                    : object(item, {"element", "name"});
            const auto named = relational ? RelationId{parse_id(entry["relation"])}
                                          : relation_from(parse_ref(entry["element"]));
            if (!schema.key_names.emplace(named, string(entry["name"])).second)
                invalid("Duplicate schema key name.");
        }
    if (counted)
        for (const auto& item : array(o["counting_keys"]))
            if (!schema.counting_keys.insert(relational ? RelationId{parse_id(item)}
                                                        : relation_from(parse_ref(item))).second)
                invalid("Duplicate counting key.");
    // Version 31 keeps the names typed over foreign keys the conversion made.
    if (renamed_keys)
        for (const auto& item : array(o["foreign_key_names"])) {
            const auto entry = object(item, {"key", "part", "name"});
            const ForeignKeyColumn column{ForeignKeyId{parse_id(entry["key"])},
                                          parse_length(entry["part"])};
            if (!schema.foreign_key_names.emplace(column, string(entry["name"])).second)
                invalid("Duplicate foreign key name.");
        }
    // Version 33 keeps the order somebody gave a derived table's columns. A
    // file from before has none, and every table is listed as the conversion
    // makes it, which is all it could say. Whether each column is still there
    // is validation's question, not the reader's.
    if (ordered_columns)
        for (const auto& item : array(o["column_order"])) {
            const auto entry = object(item, {"relation", "columns"});
            const RelationId relation{parse_id(entry["relation"])};
            std::vector<ColumnIdentity> columns;
            for (const auto& one : array(entry["columns"])) columns.push_back(parse_column_identity(one));
            if (columns.empty()) continue;
            if (!schema.column_order.emplace(relation, std::move(columns)).second)
                invalid("Duplicate column order table.");
        }
    return schema;
}
QJsonObject comment_target(const CommentTarget& target) {
    if (const auto* element = std::get_if<ElementRef>(&target))
        return {{"kind", QLatin1String("element")}, {"element", reference(*element)}};
    if (const auto* connector = std::get_if<ConnectorRef>(&target))
        return {{"kind", QLatin1String("connector")}, {"link", connector_reference(*connector)}};
    const auto& anchor = std::get<TextAnchor>(target);
    return {{"kind", QLatin1String("text")}, {"element", reference(anchor.owner)},
            {"field", QLatin1String(anchor.field == TextField::Name ? "name" : "description")},
            {"begin", static_cast<double>(anchor.begin)},
            {"length", static_cast<double>(anchor.length)}};
}
// A character offset into somebody's writing: whole, not negative, and no
// larger than the text could possibly be. Validation then checks it against the
// text it actually points into.
std::uint32_t comment_offset(const QJsonValue& value) {
    if (!value.isDouble()) invalid("A comment's text range must be a number.");
    const auto number = value.toDouble();
    if (!std::isfinite(number) || number < 0 || number != std::floor(number) || number > max_comment_bytes)
        invalid("A comment's text range must be a whole, non-negative offset.");
    return static_cast<std::uint32_t>(number);
}
ElementRef parse_ref(const QJsonValue& value) {
    auto o = object(value, {"type", "id"});
    auto id = parse_id(o["id"]);
    const auto type = string(o["type"]);
    if (type == "entity") return EntityId{id};
    if (type == "attribute") return AttributeId{id};
    if (type == "relationship") return RelationshipId{id};
    if (type == "specialization") return SpecializationId{id};
    if (type == "picture") return PictureId{id};
    if (type == "note") return NoteId{id};
    if (type == "relation") return RelationId{id};
    invalid("Unsupported element type.");
}
// A picture's bytes travel as base64 text. Anything that is not base64, or
// that decodes to more than a picture may hold, is refused rather than
// trimmed; the domain then checks that the bytes could be an image at all.
std::vector<std::uint8_t> image_bytes(const QJsonValue& value) {
    if (!value.isString()) invalid("A picture's image must be base64 text.");
    const auto encoded = value.toString().toLatin1();
    if (encoded.isEmpty()) return {};
    if (static_cast<std::size_t>(encoded.size()) > (max_image_bytes + 2) / 3 * 4)
        invalid("A picture's image exceeds the 2 MiB limit.");
    const auto decoded = QByteArray::fromBase64Encoding(encoded, QByteArray::AbortOnBase64DecodingErrors);
    if (!decoded) invalid("A picture's image must be base64 text.");
    const auto& bytes = *decoded;
    return {bytes.begin(), bytes.end()};
}
QString image_text(const std::vector<std::uint8_t>& image) {
    return QString::fromLatin1(QByteArray::fromRawData(reinterpret_cast<const char*>(image.data()),
                                                       static_cast<qsizetype>(image.size())).toBase64());
}
QString background_name(BackgroundStyle style) {
    switch (style) {
    case BackgroundStyle::Theme: return "theme";
    case BackgroundStyle::Squares: return "squares";
    case BackgroundStyle::Lines: return "lines";
    case BackgroundStyle::Dots: return "dots";
    case BackgroundStyle::Image: return "image";
    }
    invalid("Invalid background style.");
}
BackgroundStyle parse_background_style(const QJsonValue& value) {
    const auto style = string(value);
    if (style == "theme") return BackgroundStyle::Theme;
    if (style == "squares") return BackgroundStyle::Squares;
    if (style == "lines") return BackgroundStyle::Lines;
    if (style == "dots") return BackgroundStyle::Dots;
    if (style == "image") return BackgroundStyle::Image;
    invalid("Unsupported background style.");
}
QString kind_name(AttributeKind kind) {
    switch (kind) {
    case AttributeKind::Normal: return "normal";
    case AttributeKind::Key: return "key";
    case AttributeKind::Composite: return "composite";
    case AttributeKind::Multivalued: return "multivalued";
    case AttributeKind::Derived: return "derived";
    }
    invalid("Invalid attribute kind.");
}
AttributeKind parse_kind(const QJsonValue& value) {
    const auto kind = string(value);
    if (kind == "normal") return AttributeKind::Normal;
    if (kind == "key") return AttributeKind::Key;
    if (kind == "composite") return AttributeKind::Composite;
    if (kind == "multivalued") return AttributeKind::Multivalued;
    if (kind == "derived") return AttributeKind::Derived;
    invalid("Unsupported attribute kind.");
}
double number(const QJsonValue& value) {
    if (!value.isDouble() || !std::isfinite(value.toDouble())) invalid("Invalid layout number.");
    return value.toDouble();
}

// How the schema has been arranged by hand. A link is named by whichever of
// the three things put the foreign key there, so the kind is written beside
// the identifier and read back the same way.
QJsonObject encode_link(const LinkSource& link) {
    if (const auto* id = std::get_if<ForeignKeyId>(&link))
        return {{"kind", QLatin1String("foreign_key")}, {"id", uuid_text(id->value)}};
    if (std::holds_alternative<ParticipantId>(link))
        return {{"kind", QLatin1String("participant")},
                {"id", uuid_text(std::get<ParticipantId>(link).value)}};
    if (std::holds_alternative<AttributeId>(link))
        return {{"kind", QLatin1String("attribute")},
                {"id", uuid_text(std::get<AttributeId>(link).value)}};
    return {{"kind", QLatin1String("subtype")}, {"id", uuid_text(std::get<EntityId>(link).value)}};
}

LinkSource parse_link(const QJsonValue& value) {
    auto o = object(value, {"kind", "id"});
    const auto kind = string(o["kind"]);
    const auto id = parse_id(o["id"]);
    if (kind == "participant") return LinkSource{ParticipantId{id}};
    if (kind == "attribute") return LinkSource{AttributeId{id}};
    if (kind == "subtype") return LinkSource{EntityId{id}};
    if (kind == "foreign_key") return LinkSource{ForeignKeyId{id}};
    invalid("Unsupported schema link kind.");
}

QJsonObject encode_end(const SchemaEnd& end) {
    return {{"on_table", end.on_table}, {"x", end.at.x}, {"y", end.at.y}};
}

SchemaEnd parse_end(const QJsonValue& value) {
    auto o = object(value, {"on_table", "x", "y"});
    return SchemaEnd{parse_flag(o["on_table"], "on_table"), Point{number(o["x"]), number(o["y"])}};
}

QJsonObject encode_layout(const SchemaLayout& layout) {
    // A table's place and the size it was pulled to are written together, so a
    // table that has only been widened or made taller still has a row of its
    // own and is not lost.
    std::map<RelationId, QJsonObject> arranged;
    for (const auto& [relation, at] : layout.tables)
        arranged[relation] = QJsonObject{{"relation", uuid_text(relation.value)}, {"x", at.x}, {"y", at.y}};
    const auto pulled = [&](const std::map<RelationId, double>& sizes, const char* field) {
        for (const auto& [relation, size] : sizes) {
            auto& entry = arranged[relation];
            if (entry.isEmpty())
                entry = QJsonObject{{"relation", uuid_text(relation.value)}, {"x", 0.0}, {"y", 0.0}};
            entry[QLatin1String(field)] = size;
            if (!layout.tables.contains(relation)) entry["placed"] = false;
        }
    };
    pulled(layout.widths, "width");
    pulled(layout.heights, "height");
    QJsonArray tables;
    for (auto& [relation, entry] : arranged) {
        if (!entry.contains("placed")) entry["placed"] = layout.tables.contains(relation);
        // Nothing said about a size is a zero, which reads back as a table
        // that was never pulled that way rather than as one pulled to nothing.
        if (!entry.contains("width")) entry["width"] = 0.0;
        if (!entry.contains("height")) entry["height"] = 0.0;
        tables.append(entry);
    }
    QJsonArray lines;
    for (const auto& [key, line] : layout.lines) {
        QJsonArray route;
        for (const auto& corner : line.route)
            route.append(QJsonObject{{"x", corner.x}, {"y", corner.y}});
        lines.append(QJsonObject{
            {"key", uuid_text(key.value)}, {"route", route},
            {"from", line.from ? QJsonValue(encode_end(*line.from)) : QJsonValue(QJsonValue::Null)},
            {"to", line.to ? QJsonValue(encode_end(*line.to)) : QJsonValue(QJsonValue::Null)}});
    }
    return QJsonObject{{"tables", tables}, {"lines", lines}};
}

// Version 23 added the height a table has been pulled to. A file written
// before it has width alone, which reads correctly as a table whose height is
// still however many rows it has.
SchemaLayout parse_layout(const QJsonValue& value, bool tall, bool relational) {
    auto o = object(value, {"tables", "lines"});
    SchemaLayout layout;
    for (const auto& item : array(o["tables"])) {
        const auto* key_field = relational ? "relation" : "element";
        auto entry = tall ? object(item, {key_field, "x", "y", "width", "height", "placed"})
                          : object(item, {key_field, "x", "y", "width", "placed"});
        const auto table = relational ? RelationId{parse_id(entry[QLatin1String(key_field)])}
                                      : relation_from(parse_ref(entry["element"]));
        if (parse_flag(entry["placed"], "placed")
            && !layout.tables.emplace(table, Point{number(entry["x"]), number(entry["y"])}).second)
            invalid("Duplicate schema table placement.");
        const auto wide = number(entry["width"]);
        if (wide > 0) {
            if (wide < min_table_width || wide > max_table_width)
                invalid("A schema table width is outside what a table may be.");
            if (!layout.widths.emplace(table, wide).second)
                invalid("Duplicate schema table width.");
        }
        if (!tall) continue;
        const auto high = number(entry["height"]);
        if (high <= 0) continue;
        if (high < min_table_height || high > max_table_height)
            invalid("A schema table height is outside what a table may be.");
        if (!layout.heights.emplace(table, high).second)
            invalid("Duplicate schema table height.");
    }
    for (const auto& item : array(o["lines"])) {
        auto entry = relational ? object(item, {"key", "route", "from", "to"})
                                : object(item, {"link", "route", "from", "to"});
        SchemaLine line;
        for (const auto& corner : array(entry["route"])) {
            auto at = object(corner, {"x", "y"});
            line.route.push_back(Point{number(at["x"]), number(at["y"])});
        }
        if (!entry["from"].isNull()) line.from = parse_end(entry["from"]);
        if (!entry["to"].isNull()) line.to = parse_end(entry["to"]);
        if (line.empty()) continue;
        const auto key = relational ? ForeignKeyId{parse_id(entry["key"])}
                                    : foreign_key_from(parse_link(entry["link"]));
        if (!layout.lines.emplace(key, std::move(line)).second)
            invalid("Duplicate schema line shape.");
    }
    return layout;
}
void require_valid(const Project& project) {
    for (const auto& issue : validate(project)) if (issue.blocks_save) invalid(text(issue.message));
}
int hex_digit(char value) {
    if (value >= '0' && value <= '9') return value - '0';
    if (value >= 'a' && value <= 'f') return value - 'a' + 10;
    if (value >= 'A' && value <= 'F') return value - 'A' + 10;
    return -1;
}

// Canonical UTF-8 form of one object key, scanned within the token itself.
// Invoking the JSON parser per key made rejecting a hostile 8 MiB file cost
// about seventeen times a parse-only pass, so keys are unescaped directly.
// The authoritative document parse below still validates JSON syntax, values,
// and text encoding; this pass only has to agree on key identity.
QByteArray key_text(const QByteArray& input, qsizetype open_quote, qsizetype close_quote) {
    const auto* begin = input.constData() + open_quote + 1;
    const auto length = close_quote - open_quote - 1;
    if (!QByteArray::fromRawData(begin, length).contains('\\')) return {begin, length};
    QString decoded;
    decoded.reserve(length);
    auto code_unit = [&](qsizetype at) {
        int value = 0;
        for (qsizetype digit = 0; digit < 4; ++digit) {
            const auto parsed = hex_digit(begin[at + digit]);
            if (parsed < 0) invalid("Invalid project JSON field name.");
            value = value * 16 + parsed;
        }
        return static_cast<char16_t>(value);
    };
    for (qsizetype at = 0; at < length;) {
        if (begin[at] != '\\') {
            const auto run = at;
            while (at < length && begin[at] != '\\') ++at;
            decoded += QString::fromUtf8(begin + run, at - run);
            continue;
        }
        if (++at >= length) invalid("Invalid project JSON field name.");
        switch (const auto escape = begin[at++]; escape) {
        case '"': decoded += QChar(u'"'); break;
        case '\\': decoded += QChar(u'\\'); break;
        case '/': decoded += QChar(u'/'); break;
        case 'b': decoded += QChar(u'\b'); break;
        case 'f': decoded += QChar(u'\f'); break;
        case 'n': decoded += QChar(u'\n'); break;
        case 'r': decoded += QChar(u'\r'); break;
        case 't': decoded += QChar(u'\t'); break;
        case 'u': {
            if (length - at < 4) invalid("Invalid project JSON field name.");
            const auto leading = code_unit(at);
            at += 4;
            if (QChar::isLowSurrogate(leading))
                invalid("A project field name contains an unpaired Unicode surrogate.");
            decoded += QChar(leading);
            if (!QChar::isHighSurrogate(leading)) break;
            if (length - at < 6 || begin[at] != '\\' || begin[at + 1] != 'u')
                invalid("A project field name contains an unpaired Unicode surrogate.");
            const auto trailing = code_unit(at + 2);
            if (!QChar::isLowSurrogate(trailing))
                invalid("A project field name contains an unpaired Unicode surrogate.");
            at += 6;
            decoded += QChar(trailing);
            break;
        }
        default: invalid("Invalid project JSON field name.");
        }
    }
    return decoded.toUtf8();
}

void check_structure(const QByteArray& input) {
    // Objects are capped at sixteen keys, so a linear scan over a reused buffer
    // settles duplicates without building an ordered container per object. A
    // hostile file can contain a million objects; the scopes are therefore
    // pooled by depth and only their contents are cleared.
    struct Scope {
        char opener = 0;
        std::vector<QByteArray> keys;
    };
    std::vector<Scope> scopes(32);
    std::size_t depth = 0;
    auto whitespace = [](char ch) { return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'; };
    for (qsizetype i = 0; i < input.size(); ++i) {
        const char ch = input[i];
        if (ch == '{' || ch == '[') {
            if (depth == 32) invalid("Project nesting exceeds the supported limit.");
            scopes[depth].opener = ch;
            scopes[depth].keys.clear();
            ++depth;
        } else if (ch == '}' || ch == ']') {
            if (depth == 0 || scopes[depth - 1].opener != (ch == '}' ? '{' : '['))
                invalid("Invalid project JSON structure.");
            --depth;
        } else if (ch == '"') {
            const qsizetype start = i++;
            while (i < input.size() && input[i] != '"') {
                if (input[i] == '\\') ++i;
                ++i;
            }
            if (i >= input.size()) invalid("Unterminated project JSON string.");
            auto next = i + 1;
            while (next < input.size() && whitespace(input[next])) ++next;
            if (next == input.size() || input[next] != ':') continue;
            if (depth == 0 || scopes[depth - 1].opener != '{') invalid("Invalid project JSON field.");
            auto& keys = scopes[depth - 1].keys;
            // QJson normalizes duplicate object keys, so they are detected here
            // before the authoritative parse discards the collision.
            auto key = key_text(input, start, i);
            if (std::find(keys.begin(), keys.end(), key) != keys.end())
                invalid("Duplicate project JSON field.");
            keys.push_back(std::move(key));
            // A cap on how many fields one object may carry, to bound the
            // bookkeeping this preflight does for a hostile document before
            // the real parse ever runs. It is not a schema check: it only has
            // to stay above the widest object the format actually writes,
            // which is the project itself at eighteen fields.
            if (keys.size() > 20) invalid("Unsupported project fields.");
        }
    }
}
} // namespace

Uuid QtIdGenerator::next() { return bytes_of(QUuid::createUuidV7()); }
QString uuid_text(Uuid value) {
    return QUuid::fromRfc4122(QByteArrayView(reinterpret_cast<const char*>(value.bytes.data()), 16)).toString(QUuid::WithoutBraces);
}

QByteArray ErdxProjectStore::encode(const Project& project) {
    require_valid(project);
    // Reject clearly oversized models before constructing duplicate Qt/JSON
    // representations. Encoded JSON can only be larger than its UTF-8 text.
    std::size_t text_bytes = project.name.size();
    const auto budget = static_cast<std::size_t>(max_file_bytes);
    auto count = [&](const std::string& value) {
        if (value.size() > budget - text_bytes) invalid("The project exceeds the 8 MiB file limit.");
        text_bytes += value.size();
    };
    count(project.description);
    for (const auto& [id, entity] : project.entities) { (void)id; count(entity.name); count(entity.description); }
    for (const auto& [id, attribute] : project.attributes) { (void)id; count(attribute.name); count(attribute.description); }
    for (const auto& [id, relationship] : project.relationships) {
        (void)id;
        count(relationship.name); count(relationship.description);
        for (const auto& participant : relationship.participants) count(participant.role);
    }
    for (const auto& [id, specialization] : project.specializations) { (void)id; count(specialization.name); count(specialization.description); }
    for (const auto& [id, note] : project.notes) { (void)id; count(note.name); count(note.description); }
    // An image is written as base64, which is a third again as long as the bytes.
    for (const auto& [id, picture] : project.pictures) {
        (void)id;
        count(picture.name); count(picture.description);
        const auto encoded = (picture.image.size() + 2) / 3 * 4;
        if (encoded > budget - text_bytes) invalid("The project exceeds the 8 MiB file limit.");
        text_bytes += encoded;
    }
    QJsonArray entities, attributes, relationships, layout;
    for (const auto& [id, entity] : project.entities)
        entities.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(entity.name)},
                                    {"description", text(entity.description)}, {"weak", entity.weak},
                                    {"comment", text(entity.comment)}});
    for (const auto& [id, attribute] : project.attributes)
        attributes.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(attribute.name)},
            {"description", text(attribute.description)}, {"kind", kind_name(attribute.kind)},
            {"owner", attribute.owner ? QJsonValue(reference(*attribute.owner)) : QJsonValue(QJsonValue::Null)},
            {"comment", text(attribute.comment)}, {"type", logical_type_name(attribute.logical_type)},
            {"length", static_cast<double>(attribute.length)},
            {"scale", static_cast<double>(attribute.scale)}, {"identifier", attribute.identifier},
            {"required", attribute.required}, {"unique", attribute.unique},
            {"auto_increment", attribute.auto_increment},
            {"creation_order", static_cast<double>(attribute.creation_order)}});
    for (const auto& [id, relationship] : project.relationships) {
        QJsonArray participants;
        for (const auto& p : relationship.participants)
            participants.append(QJsonObject{{"id", uuid_text(p.id.value)}, {"target", reference(target_ref(p.target))},
                {"maximum", p.maximum == Cardinality::One ? "one" : "many"},
                {"participation", p.participation == Participation::Total ? "total" : "partial"},
                {"cardinality_confirmed", p.cardinality_confirmed},
                {"participation_confirmed", p.participation_confirmed}, {"role", text(p.role)},
                {"show_constraints", p.show_constraints}});
        relationships.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(relationship.name)},
            {"description", text(relationship.description)}, {"associative", relationship.associative},
            {"identifying", relationship.identifying}, {"participants", participants},
            {"comment", text(relationship.comment)}});
    }
    for (const auto& [ref, rect] : project.layout)
        layout.append(QJsonObject{{"element", reference(ref)}, {"x", rect.x}, {"y", rect.y}, {"width", rect.width}, {"height", rect.height}});
    QJsonArray specializations;
    for (const auto& [id, specialization] : project.specializations) {
        QJsonArray subtypes;
        for (const auto& subtype : specialization.subtypes) subtypes.append(uuid_text(subtype.value));
        specializations.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(specialization.name)},
            {"description", text(specialization.description)},
            {"direction", specialization.direction == Inheritance::Generalization ? "generalization" : "specialization"},
            {"supertype", specialization.supertype ? QJsonValue(uuid_text(specialization.supertype->value))
                                                   : QJsonValue(QJsonValue::Null)},
            {"subtypes", subtypes},
            {"constraint", specialization.constraint == Disjointness::Overlapping ? "overlapping" : "disjoint"},
            {"completeness", specialization.completeness == Completeness::Total ? "total" : "partial"}});
    }
    QJsonArray colours;
    for (const auto& [ref, colour] : project.colours)
        colours.append(QJsonObject{{"element", reference(ref)},
                                   {"red", colour.red}, {"green", colour.green}, {"blue", colour.blue}});
    QJsonArray transparency;
    for (const auto& [ref, percent] : project.transparency)
        transparency.append(QJsonObject{{"element", reference(ref)}, {"percent", percent}});
    QJsonArray lettering;
    for (const auto& [ref, base] : project.lettering)
        lettering.append(QJsonObject{{"element", reference(ref)}, {"width", base.width}, {"height", base.height}});
    QJsonArray pictures, notes;
    for (const auto& [id, picture] : project.pictures)
        pictures.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(picture.name)},
            {"description", text(picture.description)}, {"image", image_text(picture.image)}});
    for (const auto& [id, note] : project.notes)
        notes.append(QJsonObject{{"id", uuid_text(id.value)}, {"name", text(note.name)},
            {"description", text(note.description)}, {"plain", note.plain}});
    QJsonArray connectors;
    for (const auto& [ref, connector] : project.connectors) {
        // Both anchors are always written, null when the join is not pinned, so
        // that every connector in a file carries the same field set. The reader
        // refuses an object whose keys it does not expect, so an optional field
        // has to be a present null rather than an absent key.
        QJsonArray route;
        for (const auto& point : connector.waypoints)
            route.append(QJsonObject{{"x", point.x}, {"y", point.y}});
        connectors.append(QJsonObject{{"link", connector_reference(ref)}, {"offset", connector.offset},
            {"owner_anchor", connector.owner_anchor ? QJsonValue(*connector.owner_anchor) : QJsonValue()},
            {"child_anchor", connector.child_anchor ? QJsonValue(*connector.child_anchor) : QJsonValue()},
            {"waypoints", route}});
    }
    QJsonArray comments;
    for (const auto& [id, comment] : project.comments) {
        QJsonArray targets;
        for (const auto& target : comment.targets) targets.append(comment_target(target));
        comments.append(QJsonObject{{"id", uuid_text(id.value)}, {"text", text(comment.text)},
                                    {"targets", targets}, {"hidden", comment.hidden}});
    }
    auto bytes = QJsonDocument(QJsonObject{{"format", "erdflow"}, {"format_version", current_format_version},
        {"project", QJsonObject{{"id", uuid_text(project.id.value)}, {"name", text(project.name)},
        {"description", text(project.description)},
        {"entities", entities}, {"attributes", attributes}, {"relationships", relationships},
        {"layout", layout}, {"connectors", connectors}, {"colours", colours},
        {"specializations", specializations}, {"pictures", pictures}, {"notes", notes},
        {"comments", comments}, {"transparency", transparency}, {"lettering", lettering},
        {"decisions", encode_decisions(project.decisions)},
        {"schema", encode_schema(project.schema)},
        {"schema_layout", encode_layout(project.schema_layout)},
        {"background", QJsonObject{{"style", background_name(project.background.style)},
                                   {"strength", project.background.strength},
                                   {"image", image_text(project.background.image)}}}}}}).toJson(QJsonDocument::Indented);
    if (bytes.size() > max_file_bytes) invalid("The project exceeds the 8 MiB file limit.");
    return bytes;
}

application::LoadResult ErdxProjectStore::decode(const QByteArray& input) {
    try {
        if (input.size() > max_file_bytes) invalid("The project exceeds the 8 MiB file limit.");
        check_structure(input);
        QJsonParseError error;
        const auto document = QJsonDocument::fromJson(input, &error);
        if (error.error != QJsonParseError::NoError) invalid("Invalid project JSON: " + error.errorString());
        const auto root = object(document.isObject() ? QJsonValue(document.object()) : QJsonValue{}, {"format", "format_version", "project"});
        if (string(root["format"]) != "erdflow") invalid("This is not an ERDFlow project.");
        // Version 1 had no connector shapes. It still opens, and its links start
        // routed automatically; saving then writes the current version.
        const auto version = root["format_version"];
        const auto number_version = version.isDouble() ? version.toDouble() : 0;
        if (number_version < 1 || number_version > current_format_version || number_version != std::floor(number_version))
            invalid("Unsupported project version. Use a compatible ERDFlow release.");
        const bool shaped_connectors = number_version >= 2;
        const bool associative_entities = number_version >= 3;
        const bool inheritance = number_version >= 4;
        const bool inheritance_direction = number_version >= 5;
        const bool detachable_supertype = number_version >= 6;
        // Version 7 lets a connector pin where it meets each shape. Since the
        // reader refuses fields it does not expect, a release that predates
        // pinning declines the whole file rather than opening it and dropping
        // the pins on the next save, which is the better of the two.
        const bool pinned_connectors = number_version >= 7;
        // Version 8 lets a connector carry a route of its own. Earlier files
        // have at most the single bend, which is exactly the shape they had.
        const bool routed_connectors = number_version >= 8;
        // Version 9 lets an element carry a colour of its own. Earlier files
        // have none, and every element follows its theme, as they always did.
        const bool chosen_colours = number_version >= 9;
        // Version 10 lets one side of a relationship be drawn bare. Earlier
        // files draw both, which is what every one of them meant.
        const bool hidable_constraints = number_version >= 10;
        // Version 11 adds the pictures and notes placed on the canvas. Earlier
        // files have none, and could not have had.
        const bool figures = number_version >= 11;
        // Version 12 lets a surface be see-through by a percentage. Earlier
        // files' surfaces are solid, which is all they could be.
        const bool translucent = number_version >= 12;
        // Version 13 adds weak entities and identifying relationships. Earlier
        // files' entities are all regular and their relationships never
        // identifying, which is all they could say.
        const bool weak_entities = number_version >= 13;
        // Version 14 gives the diagram its paper. Earlier files are drawn on
        // the plain colour their theme gives the canvas, which is all they had.
        const bool papered = number_version >= 14;
        // Version 15 lets a note be a plain one. A note written by an earlier
        // version is a card, which is what those files meant.
        const bool plain_notes = number_version >= 15;
        // Version 16 adds review comments. A file written before it carries
        // none, which is all it could carry.
        const bool commented = number_version >= 16;
        // Version 17 lets a model say what it will become, and carries a
        // conceptual mode saying how much of that it was being asked for.
        // Version 18 drops the mode: there is one model and no modes, and what
        // the Properties panel shows is a user's preference rather than
        // anything the document holds. A version 17 file still names a mode and
        // is still read, and the name is discarded. Files written before 17
        // carry no types, which is all they could carry.
        const bool convertible = number_version >= 17;
        const bool moded = number_version == 17;
        // Version 19 records whether each relationship side was answered.
        // Earlier files say only what the sides are, never whether anyone
        // chose them.
        const bool answered_sides = number_version >= 19;
        // Version 20 writes the SQL type catalogue, a decimal's scale, and the
        // conversion decisions. Earlier files name a type from the small
        // portable set and record no decisions at all.
        const bool catalogued = number_version >= 20;
        // Version 25 records whether a column counts itself up. A file written
        // before it simply says nothing about it, which reads correctly as no
        // column doing so.
        const bool generated = number_version >= 25;
        // Version 26 gives the Relational Schema identities of its own, as
        // ADR-008 requires, instead of keying its state by the conceptual
        // element each object came from. A file written before it is migrated
        // on the way in: the identity is worked out from the element that used
        // to be the key, by the same derivation the conversion uses, so the
        // same old file always yields the same relations.
        const bool relational = number_version >= 26;
        // Version 27 keeps the optional prose captured by the project-creation
        // form. Older files had nowhere to store it, and therefore open with
        // an empty description rather than having one invented for them.
        const bool described = number_version >= 27;
        // Version 29 keeps the size an element had when it was first resized
        // by hand, which its name is drawn for. A file written before it has
        // no such section, which reads correctly as nothing having been
        // resized that way, every name drawn at its ordinary size.
        const bool lettered = number_version >= 29;
        // Version 21 records where the schema differs from the diagram. A file
        // written before it simply has no such section, which reads correctly
        // as the two agreeing about everything.
        const bool diverged = number_version >= 21;
        // Version 22 records how the schema has been arranged by hand. A file
        // written before it simply has no such section, which reads correctly
        // as nothing having been arranged.
        const bool arranged = number_version >= 22;
        // Version 23 records how tall a table has been pulled beside how wide.
        // A file written before it says only the width, which reads correctly
        // as a table still as tall as its rows make it.
        const bool pulled_tables = number_version >= 23;
        // Version 24 records what a key the conversion invented has been
        // renamed to. A file written before it simply has no such section,
        // which reads correctly as every generated key still carrying the name
        // the rule gave it.
        const bool named_keys = number_version >= 24;
        // Version 34 records when each attribute was created, which is what an
        // owner's attributes are listed by. A file written before it has no
        // such number, and is numbered as it opens in the order its
        // attributes were listed in then: the order of their identities.
        const bool creation_ordered = number_version >= 34;
        const auto data = lettered
            ? object(root["project"], {"id", "name", "description", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "comments", "transparency", "lettering", "decisions", "schema", "schema_layout", "background"})
            : described
            ? object(root["project"], {"id", "name", "description", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "comments", "transparency", "decisions", "schema", "schema_layout", "background"})
            : arranged
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "comments", "transparency", "decisions", "schema", "schema_layout", "background"})
            : diverged
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "comments", "transparency", "decisions", "schema", "background"})
            : catalogued
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "comments", "transparency", "decisions", "background"})
            : moded
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "comments", "transparency", "mode", "background"})
            : convertible
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "comments", "transparency", "background"})
            : commented
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "comments", "transparency", "background"})
            : papered
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "transparency", "background"})
            : translucent
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes", "transparency"})
            : figures
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours", "pictures", "notes"})
            : chosen_colours
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations", "colours"})
            : inheritance
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors", "specializations"})
            : shaped_connectors
            ? object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout", "connectors"})
            : object(root["project"], {"id", "name", "entities", "attributes", "relationships", "layout"});
        Project project;
        project.id = ProjectId{parse_id(data["id"])};
        project.name = string(data["name"]);
        if (described) project.description = string(data["description"], max_description_bytes);
        for (const auto& value : array(data["entities"])) {
            auto o = convertible ? object(value, {"id", "name", "description", "weak", "comment"})
                   : weak_entities ? object(value, {"id", "name", "description", "weak"})
                                   : object(value, {"id", "name", "description"});
            Entity entity{.id = EntityId{parse_id(o["id"])}, .name = string(o["name"]),
                          .description = string(o["description"], max_description_bytes)};
            if (convertible) entity.comment = string(o["comment"], max_comment_bytes);
            if (weak_entities) {
                if (!o["weak"].isBool()) invalid("An entity's weak flag must be true or false.");
                entity.weak = o["weak"].toBool();
            }
            if (!project.entities.emplace(entity.id, entity).second) invalid("Duplicate entity identifier.");
        }
        for (const auto& value : array(data["attributes"])) {
            auto o = creation_ordered
                ? object(value, {"id", "name", "description", "kind", "owner", "comment", "type", "length",
                                 "scale", "identifier", "required", "unique", "auto_increment", "creation_order"})
                : generated
                ? object(value, {"id", "name", "description", "kind", "owner", "comment", "type", "length",
                                 "scale", "identifier", "required", "unique", "auto_increment"})
                : catalogued
                ? object(value, {"id", "name", "description", "kind", "owner", "comment", "type", "length",
                                 "scale", "identifier", "required", "unique"})
                : convertible
                ? object(value, {"id", "name", "description", "kind", "owner", "comment", "type", "length",
                                 "identifier", "required", "unique"})
                : object(value, {"id", "name", "description", "kind", "owner"});
            Attribute attribute{.id = AttributeId{parse_id(o["id"])}, .name = string(o["name"]),
                                .description = string(o["description"], max_description_bytes),
                                .kind = parse_kind(o["kind"])};
            if (!o["owner"].isNull()) attribute.owner = parse_ref(o["owner"]);
            if (convertible) {
                attribute.comment = string(o["comment"], max_comment_bytes);
                attribute.logical_type = parse_logical_type(o["type"], catalogued);
                attribute.length = parse_length(o["length"]);
                if (catalogued) attribute.scale = parse_length(o["scale"]);
                attribute.identifier = parse_flag(o["identifier"], "identifier");
                attribute.required = parse_flag(o["required"], "required");
                attribute.unique = parse_flag(o["unique"], "unique");
                if (generated) attribute.auto_increment = parse_flag(o["auto_increment"], "auto_increment");
            }
            if (creation_ordered) attribute.creation_order = parse_creation_order(o["creation_order"]);
            if (!project.attributes.emplace(attribute.id, attribute).second) invalid("Duplicate attribute identifier.");
        }
        if (!creation_ordered) {
            std::uint32_t next = 0;
            for (auto& [id, attribute] : project.attributes) {
                (void)id;
                attribute.creation_order = ++next;
            }
        }
        std::size_t participant_count = 0;
        for (const auto& value : array(data["relationships"])) {
            auto o = convertible
                ? object(value, {"id", "name", "description", "associative", "identifying", "participants", "comment"})
                : weak_entities
                ? object(value, {"id", "name", "description", "associative", "identifying", "participants"})
                : associative_entities
                ? object(value, {"id", "name", "description", "associative", "participants"})
                : object(value, {"id", "name", "description", "participants"});
            Relationship relationship{.id = RelationshipId{parse_id(o["id"])}, .name = string(o["name"]),
                                      .description = string(o["description"], max_description_bytes)};
            if (convertible) relationship.comment = string(o["comment"], max_comment_bytes);
            if (associative_entities) {
                if (!o["associative"].isBool()) invalid("A relationship's associative flag must be true or false.");
                relationship.associative = o["associative"].toBool();
            }
            if (weak_entities) {
                if (!o["identifying"].isBool()) invalid("A relationship's identifying flag must be true or false.");
                relationship.identifying = o["identifying"].toBool();
            }
            for (const auto& part : array(o["participants"])) {
                if (++participant_count > max_elements) invalid("The project exceeds its participant limit.");
                auto p = associative_entities
                    ? (answered_sides
                        ? object(part, {"id", "target", "maximum", "participation", "cardinality_confirmed",
                                        "participation_confirmed", "role", "show_constraints"})
                        : hidable_constraints
                        ? object(part, {"id", "target", "maximum", "participation", "role", "show_constraints"})
                        : object(part, {"id", "target", "maximum", "participation", "role"}))
                    : object(part, {"id", "entity", "maximum", "participation", "role"});
                const auto maximum = string(p["maximum"]);
                const auto participation = string(p["participation"]);
                if (maximum != "one" && maximum != "many") invalid("Invalid participant cardinality.");
                if (participation != "partial" && participation != "total") invalid("Invalid participation.");
                // Before version 3 a participant could only be an entity, and
                // was stored as a bare identifier rather than a typed reference.
                ParticipantTarget target = EntityId{};
                if (associative_entities) {
                    const auto element = parse_ref(p["target"]);
                    if (const auto* entity = std::get_if<EntityId>(&element)) target = *entity;
                    else if (const auto* onward = std::get_if<RelationshipId>(&element)) target = *onward;
                    else invalid("A participant must target an entity or an associative relationship.");
                } else {
                    target = EntityId{parse_id(p["entity"])};
                }
                // A file written before a side could be drawn bare draws both,
                // which is what every one of those diagrams meant.
                const auto shown = hidable_constraints ? p["show_constraints"] : QJsonValue(true);
                if (!shown.isBool()) invalid("A participant's show_constraints must be true or false.");
                // A file written before version 19 has no record of which sides
                // were answered. Every side in it is read as answered, because
                // the alternative would greet a diagram somebody has already
                // finished with a readiness question about every line on it.
                const auto chose_maximum = answered_sides ? p["cardinality_confirmed"] : QJsonValue(true);
                const auto chose_participation = answered_sides ? p["participation_confirmed"] : QJsonValue(true);
                if (!chose_maximum.isBool() || !chose_participation.isBool())
                    invalid("A participant's confirmation flags must be true or false.");
                relationship.participants.push_back({ParticipantId{parse_id(p["id"])}, target,
                    maximum == "one" ? Cardinality::One : Cardinality::Many,
                    participation == "total" ? Participation::Total : Participation::Partial,
                    chose_maximum.toBool(), chose_participation.toBool(), string(p["role"]),
                    shown.toBool()});
            }
            if (!project.relationships.emplace(relationship.id, relationship).second) invalid("Duplicate relationship identifier.");
        }
        for (const auto& value : array(data["layout"])) {
            const auto o = object(value, {"element", "x", "y", "width", "height"});
            if (!project.layout.emplace(parse_ref(o["element"]), Rect{number(o["x"]), number(o["y"]), number(o["width"]), number(o["height"])}).second)
                invalid("Duplicate element layout.");
        }
        if (inheritance) {
            for (const auto& value : array(data["specializations"])) {
                const auto o = inheritance_direction
                    ? object(value, {"id", "name", "description", "direction", "supertype", "subtypes", "constraint", "completeness"})
                    : object(value, {"id", "name", "description", "supertype", "subtypes", "constraint", "completeness"});
                Specialization specialization{SpecializationId{parse_id(o["id"])}, string(o["name"]),
                    string(o["description"], max_description_bytes), Inheritance::Specialization,
                    {}, {}, Disjointness::Disjoint, Completeness::Partial};
                if (!o["supertype"].isNull()) specialization.supertype = EntityId{parse_id(o["supertype"])};
                else if (!detachable_supertype) invalid("A specialization before version 6 must name its supertype.");
                if (inheritance_direction) {
                    const auto direction = string(o["direction"]);
                    if (direction != "generalization" && direction != "specialization") invalid("Invalid ISA direction.");
                    specialization.direction = direction == "generalization"
                        ? Inheritance::Generalization : Inheritance::Specialization;
                }
                for (const auto& subtype : array(o["subtypes"])) specialization.subtypes.push_back(EntityId{parse_id(subtype)});
                const auto constraint = string(o["constraint"]);
                const auto completeness = string(o["completeness"]);
                if (constraint != "disjoint" && constraint != "overlapping") invalid("Invalid specialization constraint.");
                if (completeness != "partial" && completeness != "total") invalid("Invalid specialization completeness.");
                specialization.constraint = constraint == "overlapping" ? Disjointness::Overlapping : Disjointness::Disjoint;
                specialization.completeness = completeness == "total" ? Completeness::Total : Completeness::Partial;
                if (!project.specializations.emplace(specialization.id, specialization).second)
                    invalid("Duplicate specialization identifier.");
            }
        }
        if (shaped_connectors) {
            for (const auto& value : array(data["connectors"])) {
                const auto o = routed_connectors
                    ? object(value, {"link", "offset", "owner_anchor", "child_anchor", "waypoints"})
                    : pinned_connectors
                    ? object(value, {"link", "offset", "owner_anchor", "child_anchor"})
                    : object(value, {"link", "offset"});
                domain::Connector shaped;
                shaped.offset = number(o["offset"]);
                // Files written before connectors could be pinned carry no
                // anchors, which is exactly the automatic join they were drawn with.
                if (pinned_connectors && !o["owner_anchor"].isNull()) shaped.owner_anchor = number(o["owner_anchor"]);
                if (pinned_connectors && !o["child_anchor"].isNull()) shaped.child_anchor = number(o["child_anchor"]);
                if (routed_connectors)
                    for (const auto& point : array(o["waypoints"])) {
                        const auto p = object(point, {"x", "y"});
                        shaped.waypoints.push_back(domain::Point{number(p["x"]), number(p["y"])});
                    }
                if (!project.connectors.emplace(parse_connector_reference(o["link"]), shaped).second)
                    invalid("Duplicate connector shape.");
            }
        }
        if (chosen_colours) {
            for (const auto& value : array(data["colours"])) {
                const auto o = object(value, {"element", "red", "green", "blue"});
                domain::Colour colour;
                // Each channel is one byte, so a value outside it is a broken
                // document rather than something to clamp into range quietly.
                for (const auto& [name, channel] : {std::pair{"red", &colour.red}, std::pair{"green", &colour.green},
                                                    std::pair{"blue", &colour.blue}}) {
                    const auto level = number(o[name]);
                    if (level < 0 || level > 255 || level != std::floor(level))
                        invalid("A colour channel must be a whole number between 0 and 255.");
                    *channel = static_cast<std::uint8_t>(level);
                }
                if (!project.colours.emplace(parse_ref(o["element"]), colour).second)
                    invalid("Duplicate element colour.");
            }
        }
        if (translucent) {
            for (const auto& value : array(data["transparency"])) {
                const auto o = object(value, {"element", "percent"});
                const auto percent = number(o["percent"]);
                // A percentage, whole, and never more than all of it.
                if (percent < 0 || percent > max_transparency || percent != std::floor(percent))
                    invalid("Transparency is a whole percentage from 0 to 100.");
                if (!project.transparency.emplace(parse_ref(o["element"]), static_cast<std::uint8_t>(percent)).second)
                    invalid("Duplicate element transparency.");
            }
        }
        if (lettered) {
            for (const auto& value : array(data["lettering"])) {
                const auto o = object(value, {"element", "width", "height"});
                if (!project.lettering.emplace(parse_ref(o["element"]),
                                               LetteringBase{number(o["width"]), number(o["height"])}).second)
                    invalid("Duplicate element lettering.");
            }
        }
        if (figures) {
            for (const auto& value : array(data["pictures"])) {
                const auto o = object(value, {"id", "name", "description", "image"});
                Picture picture{PictureId{parse_id(o["id"])}, string(o["name"]),
                                string(o["description"], max_description_bytes), image_bytes(o["image"])};
                if (!project.pictures.emplace(picture.id, std::move(picture)).second) invalid("Duplicate picture identifier.");
            }
            for (const auto& value : array(data["notes"])) {
                const auto o = plain_notes ? object(value, {"id", "name", "description", "plain"})
                                           : object(value, {"id", "name", "description"});
                Note note{NoteId{parse_id(o["id"])}, string(o["name"]), string(o["description"], max_description_bytes)};
                if (plain_notes) {
                    if (!o["plain"].isBool()) invalid("A note's plain flag must be true or false.");
                    note.plain = o["plain"].toBool();
                }
                if (!project.notes.emplace(note.id, std::move(note)).second) invalid("Duplicate note identifier.");
            }
        }
        if (commented) {
            for (const auto& value : array(data["comments"])) {
                const auto o = object(value, {"id", "text", "targets", "hidden"});
                Comment comment{CommentId{parse_id(o["id"])}, string(o["text"], max_comment_bytes), {}, false};
                if (!o["hidden"].isBool()) invalid("A comment's hidden flag must be true or false.");
                comment.hidden = o["hidden"].toBool();
                for (const auto& entry : array(o["targets"])) {
                    const auto target = entry.toObject();
                    const auto kind = string(target.value("kind"));
                    if (kind == "element") {
                        comment.targets.emplace_back(parse_ref(object(entry, {"kind", "element"})["element"]));
                    } else if (kind == "connector") {
                        comment.targets.emplace_back(
                            parse_connector_reference(object(entry, {"kind", "link"})["link"]));
                    } else if (kind == "text") {
                        const auto anchored = object(entry, {"kind", "element", "field", "begin", "length"});
                        TextAnchor written;
                        written.owner = parse_ref(anchored["element"]);
                        const auto field = string(anchored["field"]);
                        if (field == "name") written.field = TextField::Name;
                        else if (field == "description") written.field = TextField::Description;
                        else invalid("Unsupported comment text field.");
                        written.begin = comment_offset(anchored["begin"]);
                        written.length = comment_offset(anchored["length"]);
                        comment.targets.emplace_back(written);
                    } else {
                        invalid("Unsupported comment target kind.");
                    }
                }
                if (!project.comments.emplace(comment.id, std::move(comment)).second)
                    invalid("Duplicate comment identifier.");
            }
        }
        // A version 17 file names a conceptual mode. The concept is gone, so
        // the name is read only to refuse a file that holds nonsense, exactly
        // as that version always did, and is then discarded.
        if (moded) {
            const auto named = string(data["mode"]);
            if (named != "convertible" && named != "basic") invalid("Unsupported conceptual mode.");
        }
        if (catalogued) project.decisions = parse_decisions(data["decisions"], relational, number_version >= 28);
        if (diverged) project.schema = parse_schema(data["schema"], named_keys, generated, relational, number_version >= 30,
                                                         number_version >= 31, number_version >= 32,
                                                         number_version >= 33);
        if (arranged) project.schema_layout = parse_layout(data["schema_layout"], pulled_tables, relational);
        if (papered) {
            const auto o = object(data["background"], {"style", "strength", "image"});
            project.background.style = parse_background_style(o["style"]);
            const auto strength = number(o["strength"]);
            if (strength < 0 || strength > max_strength || strength != std::floor(strength))
                invalid("A background's strength is a whole percentage from 0 to 100.");
            project.background.strength = static_cast<std::uint8_t>(strength);
            project.background.image = image_bytes(o["image"]);
        }
        require_valid(project);
        return {std::move(project), {}};
    } catch (const std::exception& error) { return {{}, error.what()}; }
}

application::EncodeResult ErdxProjectStore::project_bytes(const Project& project) {
    // Encoding rejects a project past the file limit by throwing, which is the
    // right answer for a save. Here it is an ordinary outcome: a picture whose
    // project will not fit is still written, as a picture, and says so.
    try {
        const auto data = encode(project);
        return {std::string(data.constData(), static_cast<std::size_t>(data.size())), {}};
    } catch (const std::exception& error) { return {{}, error.what()}; }
}

application::LoadResult ErdxProjectStore::project_from_bytes(const std::string& bytes) {
    return decode(QByteArray(bytes.data(), static_cast<qsizetype>(bytes.size())));
}

application::LoadResult ErdxProjectStore::load(const std::string& location) {
    QFile file(text(location));
    if (!file.open(QIODevice::ReadOnly)) return {{}, file.errorString().toStdString()};
    if (file.size() > max_file_bytes) return {{}, "The project exceeds the 8 MiB file limit."};
    const auto data = file.read(max_file_bytes + 1);
    if (file.error() != QFileDevice::NoError) return {{}, file.errorString().toStdString()};
    return decode(data);
}

application::SaveResult ErdxProjectStore::save(const std::string& location, const Project& project) {
    try {
        const auto data = encode(project);
        QSaveFile file(text(location));
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)) return {false, file.errorString().toStdString()};
        if (file.write(data) != data.size()) { file.cancelWriting(); return {false, file.errorString().toStdString()}; }
        if (!file.commit()) return {false, file.errorString().toStdString()};
        return {true, {}};
    } catch (const std::exception& error) { return {false, error.what()}; }
}

} // namespace erdflow::infrastructure
