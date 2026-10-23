// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#include "domain/practice_data.hpp"

#include <algorithm>
#include <cctype>
#include <charconv>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <set>

namespace erdflow::domain {
namespace {

bool whole_number_type(LogicalType type) {
    switch (type) {
    case LogicalType::Int: case LogicalType::BigInt: case LogicalType::SmallInt: case LogicalType::TinyInt:
    case LogicalType::Bit:
        return true;
    default:
        return false;
    }
}

bool number_type(LogicalType type) {
    switch (type) {
    case LogicalType::Decimal: case LogicalType::Numeric: case LogicalType::Money: case LogicalType::SmallMoney:
    case LogicalType::Float: case LogicalType::Real:
        return true;
    default:
        return whole_number_type(type);
    }
}

// Whether a column of no type yet is one a reader would take for a number: an
// identifier, by its name.
bool looks_counted(const PreviewColumn& column) {
    if (column.type != LogicalType::Unset || column.name.size() < 2) return false;
    const auto tail = column.name.substr(column.name.size() - 2);
    return (tail[0] == 'I' || tail[0] == 'i') && (tail[1] == 'D' || tail[1] == 'd');
}

std::string lower(std::string text) {
    std::transform(text.begin(), text.end(), text.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

std::string trimmed(const std::string& text) {
    const auto first = text.find_first_not_of(" \t");
    if (first == std::string::npos) return {};
    const auto last = text.find_last_not_of(" \t");
    return text.substr(first, last - first + 1);
}

std::size_t characters(const std::string& text) {
    std::size_t count = 0;
    for (const auto c : text)
        if ((static_cast<unsigned char>(c) & 0xC0) != 0x80) ++count;
    return count;
}

// Cut to so many characters, never through the middle of one.
std::string first_characters(const std::string& text, std::size_t count) {
    std::size_t seen = 0;
    for (std::size_t at = 0; at < text.size(); ++at) {
        if ((static_cast<unsigned char>(text[at]) & 0xC0) != 0x80) {
            if (seen == count) return text.substr(0, at);
            ++seen;
        }
    }
    return text;
}

std::optional<long long> whole(const std::string& typed) {
    auto text = trimmed(typed);
    if (!text.empty() && text[0] == '+') text.erase(0, 1);
    if (text.empty()) return std::nullopt;
    long long value = 0;
    const auto [end, error] = std::from_chars(text.data(), text.data() + text.size(), value);
    if (error != std::errc{} || end != text.data() + text.size()) return std::nullopt;
    return value;
}

bool digits_only(const std::string& text) {
    return !text.empty() && std::all_of(text.begin(), text.end(), [](unsigned char c) { return std::isdigit(c); });
}

// A decimal number taken apart: its sign, the digits before the point without
// the noughts that lead them, and those after it without the noughts that
// trail them. Nothing where it is not a number written that way.
struct Decimal {
    bool negative = false;
    std::string whole;
    std::string fraction;
};
std::optional<Decimal> decimal(const std::string& typed) {
    auto text = trimmed(typed);
    Decimal result;
    if (!text.empty() && (text[0] == '+' || text[0] == '-')) {
        result.negative = text[0] == '-';
        text.erase(0, 1);
    }
    const auto point = text.find('.');
    auto before = text.substr(0, point);
    auto after = point == std::string::npos ? std::string{} : text.substr(point + 1);
    if (before.empty() && after.empty()) return std::nullopt;
    if ((!before.empty() && !digits_only(before)) || (!after.empty() && !digits_only(after))) return std::nullopt;
    before.erase(0, std::min(before.find_first_not_of('0'), before.size()));
    while (!after.empty() && after.back() == '0') after.pop_back();
    result.whole = before;
    result.fraction = after;
    if (result.whole.empty() && result.fraction.empty()) result.negative = false;
    return result;
}

bool leap(int year) { return (year % 4 == 0 && year % 100 != 0) || year % 400 == 0; }

bool date_text(const std::string& text) {
    if (text.size() != 10 || text[4] != '-' || text[7] != '-') return false;
    if (!digits_only(text.substr(0, 4)) || !digits_only(text.substr(5, 2)) || !digits_only(text.substr(8, 2)))
        return false;
    const int year = std::stoi(text.substr(0, 4));
    const int month = std::stoi(text.substr(5, 2));
    const int day = std::stoi(text.substr(8, 2));
    static constexpr int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (year < 1 || month < 1 || month > 12 || day < 1) return false;
    return day <= days[month - 1] + (month == 2 && leap(year) ? 1 : 0);
}

bool time_text(const std::string& text) {
    // HH:MM, HH:MM:SS, or HH:MM:SS.fraction.
    if (text.size() < 5 || text[2] != ':') return false;
    if (!digits_only(text.substr(0, 2)) || !digits_only(text.substr(3, 2))) return false;
    if (std::stoi(text.substr(0, 2)) > 23 || std::stoi(text.substr(3, 2)) > 59) return false;
    if (text.size() == 5) return true;
    if (text.size() < 8 || text[5] != ':' || !digits_only(text.substr(6, 2)) || std::stoi(text.substr(6, 2)) > 59)
        return false;
    if (text.size() == 8) return true;
    return text[8] == '.' && text.size() <= 16 && digits_only(text.substr(9));
}

bool date_time_text(const std::string& text, bool offset) {
    if (text.size() == 10) return date_text(text);
    if (text.size() < 16 || (text[10] != ' ' && text[10] != 'T') || !date_text(text.substr(0, 10))) return false;
    auto rest = text.substr(11);
    if (offset) {
        if (!rest.empty() && rest.back() == 'Z') rest.pop_back();
        else {
            const auto sign = rest.find_last_of("+-");
            if (sign != std::string::npos && sign + 6 == rest.size() && rest[sign + 3] == ':'
                && digits_only(rest.substr(sign + 1, 2)) && digits_only(rest.substr(sign + 4, 2)))
                rest = trimmed(rest.substr(0, sign));
        }
    }
    return time_text(rest);
}

bool guid_text(const std::string& text) {
    if (text.size() != 36) return false;
    for (std::size_t at = 0; at < text.size(); ++at) {
        const bool dash = at == 8 || at == 13 || at == 18 || at == 23;
        if (dash ? text[at] != '-' : !std::isxdigit(static_cast<unsigned char>(text[at]))) return false;
    }
    return true;
}

std::string with_digits(long long value) { return std::to_string(value); }

std::string whole_problem(const PreviewColumn& column, const std::string& value, long long least, long long most,
                          const char* type) {
    const auto parsed = whole(value);
    const auto range = with_digits(least) + " to " + with_digits(most);
    if (!parsed)
        return column.name + " holds whole numbers (" + type + "), from " + range + ". \"" + value
             + "\" is not one.";
    if (*parsed < least || *parsed > most)
        return column.name + " holds whole numbers from " + range + " (" + type + "). " + value
             + " is outside that range.";
    return {};
}

// A column's values compared with another's: the same kind of thing either side
// of a foreign key, since a foreign key has its key's type.
using Tuple = std::vector<std::string>;

const PracticeTable* rows_of(const PracticeData& data, const PreviewTable& table) {
    const auto found = data.tables.find(table.id);
    return found == data.tables.end() ? nullptr : &found->second;
}

// The values of a row at some columns, compared as their type compares them,
// or nothing where any of them is NULL: a key that is partly NULL identifies
// nothing and points at nothing.
std::optional<Tuple> tuple_of(const PreviewTable& table, const PracticeRow& row, const std::vector<std::size_t>& at) {
    Tuple result;
    for (const auto column : at) {
        const auto value = practice_value(row, table.columns[column]);
        if (!value) return std::nullopt;
        result.push_back(comparable(table.columns[column], *value));
    }
    return result;
}

// One foreign key of a table: the table it points at, the columns that make it
// up, and the column of the referenced table each one points at, in the key's
// own order.
struct KeyGroup {
    std::size_t target = 0;
    std::vector<std::size_t> columns;
    std::vector<std::size_t> targets;
};

std::vector<KeyGroup> key_groups(const SchemaPreview& preview, std::size_t t) {
    const auto& table = preview.tables[t];
    std::vector<KeyGroup> groups;
    std::map<ForeignKeyId, std::size_t> by_key;
    std::vector<std::pair<std::uint32_t, std::size_t>> order;
    for (std::size_t c = 0; c < table.columns.size(); ++c) {
        const auto& column = table.columns[c];
        if (!column.foreign_key || column.ignored || !column.references
            || *column.references >= preview.tables.size()
            || column.references_column >= preview.tables[*column.references].columns.size())
            continue;
        std::size_t index = groups.size();
        if (column.key_id) {
            const auto [found, made] = by_key.emplace(*column.key_id, groups.size());
            index = found->second;
            if (made) groups.push_back({*column.references, {}, {}});
        } else {
            groups.push_back({*column.references, {}, {}});
        }
        auto& group = groups[index];
        // Kept in the order of the key the group points at.
        const auto place = std::find_if(group.columns.begin(), group.columns.end(), [&](std::size_t other) {
            return table.columns[other].reference_part > column.reference_part;
        });
        const auto at = static_cast<std::size_t>(place - group.columns.begin());
        group.columns.insert(group.columns.begin() + static_cast<std::ptrdiff_t>(at), c);
        group.targets.insert(group.targets.begin() + static_cast<std::ptrdiff_t>(at), column.references_column);
    }
    return groups;
}

std::set<Tuple> target_tuples(const SchemaPreview& preview, const PracticeData& data, const KeyGroup& group) {
    std::set<Tuple> result;
    const auto& target = preview.tables[group.target];
    if (const auto* rows = rows_of(data, target))
        for (const auto& row : rows->rows)
            if (auto values = tuple_of(target, row, group.targets)) result.insert(std::move(*values));
    return result;
}

// "DepartmentID 99", or "StudentID 1, SectionID 2" for a key of several.
std::string named_values(const PreviewTable& table, const PracticeRow& row, const std::vector<std::size_t>& at) {
    std::string result;
    for (const auto column : at) {
        if (!result.empty()) result += ", ";
        result += table.columns[column].name + " " + spoken(table.columns[column], practice_value(row, table.columns[column]));
    }
    return result;
}

PracticeProblem problem(PracticeRule rule, std::size_t table, std::size_t row, std::size_t column,
                        std::string title, std::string message) {
    return {rule, table, row, column, std::move(title), std::move(message)};
}

std::string duplicate_key_message(const PreviewTable& table, const PracticeRow& row) {
    if (table.primary_key.size() == 1)
        return named_values(table, row, table.primary_key)
             + " already exists. A primary key must uniquely identify each row.";
    return "A row with " + named_values(table, row, table.primary_key)
         + " already exists. A primary key must uniquely identify each row, so no two rows may share all of "
           "its values.";
}

std::string missing_target_message(const SchemaPreview& preview, const PreviewTable& table, const PracticeRow& row,
                                   const KeyGroup& group) {
    const auto& target = preview.tables[group.target].name;
    if (group.columns.size() == 1)
        return named_values(table, row, group.columns) + " does not exist in " + target
             + ". A foreign key can only point at a row that is there.";
    return "No row in " + target + " has " + named_values(table, row, group.columns)
         + ". A foreign key can only point at a row that is there.";
}

std::string null_message(const PreviewColumn& column) {
    if (column.primary_key)
        return column.name + " cannot be NULL. A primary key identifies its row, so every row needs a value.";
    return column.name + " is required (NOT NULL). It cannot be left empty.";
}

bool single_key(const PreviewTable& table, std::size_t column) {
    return table.primary_key.size() == 1 && table.primary_key.front() == column;
}

// What a column starts with, in the row counted from nought. Made from its type
// and, where its name says what it holds, from its name -- never from a list of
// tables somebody once drew.
PracticeValue made_value(const PreviewTable& table, const PreviewColumn& column, std::size_t i) {
    const auto n = static_cast<long long>(i) + 1;
    const auto name = lower(column.name);
    const auto has = [&](const char* word) { return name.find(word) != std::string::npos; };
    const auto pick = [&](std::initializer_list<const char*> words) {
        return std::string(*(words.begin() + static_cast<std::ptrdiff_t>(i % words.size())));
    };
    const bool keyed = column.primary_key || column.unique;
    const auto type = column.type;
    std::string value;
    if (whole_number_type(type) || looks_counted(column)) {
        if (type == LogicalType::Bit) value = i % 2 == 0 ? "1" : "0";
        else if (keyed || column.auto_increment) value = with_digits(n);
        else if (has("year")) value = "2024";
        else if (has("credit")) value = pick({"3", "3", "4"});
        else if (has("capacity") || has("seats")) value = pick({"40", "60", "30"});
        else if (has("age")) value = with_digits(18 + n);
        else if (has("level")) value = with_digits(100 * n);
        else if (has("score") || has("points") || has("mark")) value = pick({"100", "50", "75"});
        else value = with_digits(n);
    } else if (type == LogicalType::Decimal || type == LogicalType::Numeric) {
        value = with_digits(n);
        if (column.scale > 0) value += "." + std::string(column.scale, '0');
    } else if (type == LogicalType::Money || type == LogicalType::SmallMoney) {
        value = with_digits(100 * n) + ".00";
    } else if (type == LogicalType::Float || type == LogicalType::Real) {
        value = with_digits(n) + ".5";
    } else if (type == LogicalType::Date) {
        value = "2024-08-" + with_digits(27 + n);
    } else if (type == LogicalType::Time) {
        value = (n < 2 ? "0" : "") + with_digits(8 + n) + ":00:00";
    } else if (type == LogicalType::DateTime || type == LogicalType::DateTime2 || type == LogicalType::SmallDateTime) {
        value = "2024-08-" + with_digits(27 + n) + " 09:00:00";
    } else if (type == LogicalType::DateTimeOffset) {
        value = "2024-08-" + with_digits(27 + n) + " 09:00:00 +00:00";
    } else if (type == LogicalType::UniqueIdentifier) {
        const auto tail = with_digits(n);
        value = "00000000-0000-0000-0000-" + std::string(12 - tail.size(), '0') + tail;
    } else if (type == LogicalType::Binary || type == LogicalType::Varbinary || type == LogicalType::VarbinaryMax
               || type == LogicalType::Image) {
        value = "0x0" + with_digits(n);
    } else if (type == LogicalType::Xml) {
        value = "<item>" + with_digits(n) + "</item>";
    } else if (type == LogicalType::RowVersion || type == LogicalType::HierarchyId || type == LogicalType::SqlVariant
               || type == LogicalType::Cursor || type == LogicalType::Table || type == LogicalType::Geometry
               || type == LogicalType::Geography) {
        if (!column.required && !column.primary_key) return std::nullopt;
        value = column.name + " " + with_digits(n);
    } else if (has("email")) {
        std::string who;
        for (const auto c : lower(table.name))
            if (std::isalnum(static_cast<unsigned char>(c))) who += c;
        value = (who.empty() ? std::string("user") : who) + with_digits(n) + "@example.com";
    } else if (has("phone") || has("mobile")) {
        value = "555-010" + with_digits(n);
    } else if (has("first")) {
        value = pick({"Alex", "Taylor", "Jordan", "Casey", "Morgan"});
    } else if (has("middle")) {
        value = pick({"J", "M", "R"});
    } else if (has("last") || has("surname")) {
        value = pick({"Wilson", "Kim", "Patel", "Nguyen", "Garcia"});
    } else if (has("city")) {
        value = pick({"Springfield", "Riverside", "Fairview"});
    } else if (has("country")) {
        value = pick({"Canada", "Ireland", "Japan"});
    } else if (has("address") || has("street")) {
        value = with_digits(2 + 10 * n) + " Main Street";
    } else if (has("semester") || has("term")) {
        value = pick({"Fall", "Spring", "Summer"});
    } else if (has("grade")) {
        value = pick({"A", "B", "C"});
    } else if (has("status")) {
        value = pick({"Active", "Pending", "Closed"});
    } else if (has("code")) {
        std::string letters;
        for (const auto c : table.name)
            if (std::isalpha(static_cast<unsigned char>(c)) && letters.size() < 3)
                letters += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        value = (letters.empty() ? std::string("C") : letters) + "-" + with_digits(100 + n);
    } else if (has("description") || has("note") || has("comment") || has("remark")) {
        value = "Practice " + table.name + " " + with_digits(n);
    } else if (has("name") || has("title")) {
        value = table.name + " " + with_digits(n);
    } else {
        value = column.name + " " + with_digits(n);
    }
    // Kept to the column's length. Where that leaves too little to tell the
    // rows apart, a key or a unique column is given the row's number instead.
    if (size_of(type) == TypeSize::Length && column.length > 0 && characters(value) > column.length) {
        const auto cut = first_characters(value, column.length);
        value = keyed && characters(with_digits(n)) <= column.length ? with_digits(n) : cut;
    }
    if (type_problem(column, value)) {
        if (number_type(type)) value = whole_number_type(type) ? with_digits(n) : "0";
        if (type_problem(column, value)) return column.required || column.primary_key ? PracticeValue(value) : std::nullopt;
    }
    return value;
}

bool collides(const PreviewTable& table, const std::vector<PracticeRow>& kept, const PracticeRow& row) {
    for (const auto& other : kept) {
        if (!table.primary_key.empty()) {
            const auto mine = tuple_of(table, row, table.primary_key);
            const auto theirs = tuple_of(table, other, table.primary_key);
            if (mine && theirs && *mine == *theirs) return true;
        }
        for (std::size_t c = 0; c < table.columns.size(); ++c) {
            const auto& column = table.columns[c];
            if (!column.unique || column.ignored || single_key(table, c)) continue;
            const auto mine = practice_value(row, column);
            const auto theirs = practice_value(other, column);
            if (mine && theirs && comparable(column, *mine) == comparable(column, *theirs)) return true;
        }
    }
    return false;
}

} // namespace

std::vector<std::size_t> practice_columns(const PreviewTable& table) {
    std::vector<std::size_t> result;
    for (std::size_t c = 0; c < table.columns.size(); ++c)
        if (!table.columns[c].ignored && column_identity(table.columns[c])) result.push_back(c);
    return result;
}

PracticeValue practice_value(const PracticeRow& row, const PreviewColumn& column) {
    const auto identity = column_identity(column);
    if (!identity) return std::nullopt;
    const auto found = row.values.find(*identity);
    return found == row.values.end() ? std::nullopt : found->second;
}

std::string comparable(const PreviewColumn& column, const std::string& value) {
    if (whole_number_type(column.type) || looks_counted(column)) {
        if (const auto parsed = whole(value)) return with_digits(*parsed);
        return value;
    }
    if (number_type(column.type)) {
        if (const auto parsed = decimal(value))
            return (parsed->negative ? "-" : "") + (parsed->whole.empty() ? std::string("0") : parsed->whole)
                 + (parsed->fraction.empty() ? std::string{} : "." + parsed->fraction);
        return value;
    }
    if (column.type == LogicalType::UniqueIdentifier) return lower(trimmed(value));
    return value;
}

std::string spoken(const PreviewColumn& column, const PracticeValue& value) {
    if (!value) return "NULL";
    if ((number_type(column.type) || looks_counted(column)) && decimal(*value)) return trimmed(*value);
    return "\"" + *value + "\"";
}

std::optional<std::string> type_problem(const PreviewColumn& column, const PracticeValue& value) {
    if (!value) return std::nullopt;
    const auto& text = *value;
    const auto said = [&](std::string why) -> std::optional<std::string> {
        if (why.empty()) return std::nullopt;
        return why;
    };
    switch (column.type) {
    case LogicalType::TinyInt: return said(whole_problem(column, text, 0, 255, "tinyint"));
    case LogicalType::SmallInt: return said(whole_problem(column, text, -32768, 32767, "smallint"));
    case LogicalType::Int: return said(whole_problem(column, text, -2147483647LL - 1, 2147483647LL, "int"));
    case LogicalType::BigInt:
        return said(whole_problem(column, text, std::numeric_limits<long long>::min(),
                                  std::numeric_limits<long long>::max(), "bigint"));
    case LogicalType::Bit: {
        const auto parsed = whole(text);
        if (parsed && (*parsed == 0 || *parsed == 1)) return std::nullopt;
        return column.name + " holds 0 or 1 (bit). \"" + text + "\" is neither.";
    }
    case LogicalType::Decimal: case LogicalType::Numeric: {
        const std::uint32_t precision = column.length > 0 ? column.length : 18;
        const std::uint32_t scale = std::min(column.scale, precision);
        const auto parsed = decimal(text);
        const auto shape = std::to_string(precision) + "," + std::to_string(scale);
        if (!parsed) return column.name + " holds a number (decimal(" + shape + ")). \"" + text + "\" is not one.";
        if (parsed->fraction.size() > scale || parsed->whole.size() > precision - scale)
            return column.name + " holds at most " + std::to_string(precision - scale) + " digits before the point and "
                 + std::to_string(scale) + " after it (decimal(" + shape + ")). " + trimmed(text) + " does not fit.";
        return std::nullopt;
    }
    case LogicalType::Money: case LogicalType::SmallMoney: {
        const bool small = column.type == LogicalType::SmallMoney;
        const auto parsed = decimal(text);
        const char* type = small ? "smallmoney" : "money";
        if (!parsed) return column.name + " holds an amount of money (" + type + "). \"" + text + "\" is not one.";
        if (parsed->fraction.size() > 4 || parsed->whole.size() > (small ? 6U : 15U))
            return column.name + " holds amounts with at most four digits after the point (" + type + "). "
                 + trimmed(text) + " does not fit.";
        return std::nullopt;
    }
    case LogicalType::Float: case LogicalType::Real: {
        const auto clean = trimmed(text);
        char* end = nullptr;
        (void)std::strtod(clean.c_str(), &end);
        if (!clean.empty() && end == clean.c_str() + clean.size()) return std::nullopt;
        return column.name + " holds a number (" + (column.type == LogicalType::Float ? "float" : "real") + "). \""
             + text + "\" is not one.";
    }
    case LogicalType::Char: case LogicalType::Varchar: case LogicalType::NChar: case LogicalType::NVarchar: {
        if (column.length == 0 || characters(text) <= column.length) return std::nullopt;
        const auto type = std::string(column.type == LogicalType::Char ? "char" : column.type == LogicalType::Varchar
                                      ? "varchar" : column.type == LogicalType::NChar ? "nchar" : "nvarchar");
        return column.name + " holds at most " + std::to_string(column.length) + " characters (" + type + "("
             + std::to_string(column.length) + ")). This has " + std::to_string(characters(text)) + ".";
    }
    case LogicalType::Binary: case LogicalType::Varbinary: case LogicalType::VarbinaryMax: case LogicalType::Image: {
        const auto clean = trimmed(text);
        const bool hex = clean.size() >= 2 && clean[0] == '0' && (clean[1] == 'x' || clean[1] == 'X')
                      && std::all_of(clean.begin() + 2, clean.end(), [](unsigned char c) { return std::isxdigit(c); });
        if (!hex) return column.name + " holds bytes, written in hexadecimal after 0x, such as 0x1F.";
        const auto bytes = (clean.size() - 1) / 2;
        if (size_of(column.type) == TypeSize::Length && column.length > 0 && bytes > column.length)
            return column.name + " holds at most " + std::to_string(column.length) + " bytes. This has "
                 + std::to_string(bytes) + ".";
        return std::nullopt;
    }
    case LogicalType::Date:
        if (date_text(trimmed(text))) return std::nullopt;
        return column.name + " holds a date, written year-month-day, such as 2024-08-28. \"" + text
             + "\" is not one.";
    case LogicalType::Time:
        if (time_text(trimmed(text))) return std::nullopt;
        return column.name + " holds a time of day, such as 09:30 or 09:30:00. \"" + text + "\" is not one.";
    case LogicalType::DateTime: case LogicalType::DateTime2: case LogicalType::SmallDateTime:
        if (date_time_text(trimmed(text), false)) return std::nullopt;
        return column.name + " holds a date and time, such as 2024-08-28 09:30:00. \"" + text + "\" is not one.";
    case LogicalType::DateTimeOffset:
        if (date_time_text(trimmed(text), true)) return std::nullopt;
        return column.name + " holds a date, time and offset, such as 2024-08-28 09:30:00 +02:00. \"" + text
             + "\" is not one.";
    case LogicalType::UniqueIdentifier:
        if (guid_text(trimmed(text))) return std::nullopt;
        return column.name + " holds a GUID, such as 6F9619FF-8B86-D011-B42D-00C04FC964FF. \"" + text
             + "\" is not one.";
    default:
        return std::nullopt;
    }
}

std::vector<PracticeProblem> practice_problems(const SchemaPreview& preview, const PracticeData& data,
                                               std::size_t t) {
    std::vector<PracticeProblem> result;
    if (t >= preview.tables.size()) return result;
    const auto& table = preview.tables[t];
    const auto* held = rows_of(data, table);
    if (!held) return result;
    const auto& rows = held->rows;
    const auto columns = practice_columns(table);
    for (std::size_t r = 0; r < rows.size(); ++r) {
        for (const auto c : columns) {
            const auto& column = table.columns[c];
            const auto value = practice_value(rows[r], column);
            if (auto why = type_problem(column, value))
                result.push_back(problem(PracticeRule::Type, t, r, c, "Data type violation", std::move(*why)));
            else if (!value && column.primary_key)
                result.push_back(problem(PracticeRule::PrimaryKey, t, r, c, "Primary Key violation", null_message(column)));
            else if (!value && column.required)
                result.push_back(problem(PracticeRule::Required, t, r, c, "NOT NULL violation", null_message(column)));
        }
    }
    if (!table.primary_key.empty()) {
        std::set<Tuple> seen;
        for (std::size_t r = 0; r < rows.size(); ++r) {
            const auto values = tuple_of(table, rows[r], table.primary_key);
            if (!values || seen.insert(*values).second) continue;
            for (const auto c : table.primary_key)
                result.push_back(problem(PracticeRule::PrimaryKey, t, r, c, "Primary Key violation",
                                         duplicate_key_message(table, rows[r])));
        }
    }
    for (const auto c : columns) {
        const auto& column = table.columns[c];
        if (!column.unique || single_key(table, c)) continue;
        std::set<std::string> seen;
        for (std::size_t r = 0; r < rows.size(); ++r) {
            const auto value = practice_value(rows[r], column);
            if (!value || seen.insert(comparable(column, *value)).second) continue;
            result.push_back(problem(PracticeRule::Unique, t, r, c, "Unique constraint violation",
                                     column.name + " " + spoken(column, value)
                                         + " already exists. A UNIQUE column cannot hold the same value in two rows."));
        }
    }
    for (const auto& group : key_groups(preview, t)) {
        const auto there = target_tuples(preview, data, group);
        for (std::size_t r = 0; r < rows.size(); ++r) {
            const auto values = tuple_of(table, rows[r], group.columns);
            if (!values || there.contains(*values)) continue;
            for (const auto c : group.columns)
                result.push_back(problem(PracticeRule::ForeignKey, t, r, c, "Foreign Key violation",
                                         missing_target_message(preview, table, rows[r], group)));
        }
    }
    return result;
}

std::optional<PracticeProblem> practice_refusal(const SchemaPreview& preview, const PracticeData& before,
                                                const PracticeData& after, std::size_t t,
                                                std::optional<std::size_t> row,
                                                const std::vector<std::size_t>& columns) {
    if (t >= preview.tables.size()) return std::nullopt;
    const auto& table = preview.tables[t];
    const auto* held = rows_of(after, table);
    if (row && held && *row < held->rows.size()) {
        const auto& rows = held->rows;
        const auto& mine = rows[*row];
        const auto others = [&](auto&& same) {
            for (std::size_t r = 0; r < rows.size(); ++r)
                if (r != *row && same(rows[r])) return true;
            return false;
        };
        for (const auto c : columns) {
            const auto& column = table.columns[c];
            const auto value = practice_value(mine, column);
            if (auto why = type_problem(column, value))
                return problem(PracticeRule::Type, t, *row, c, "Data type violation", std::move(*why));
            if (!value && column.primary_key)
                return problem(PracticeRule::PrimaryKey, t, *row, c, "Primary Key violation", null_message(column));
            if (!value && column.required)
                return problem(PracticeRule::Required, t, *row, c, "NOT NULL violation", null_message(column));
        }
        const auto touches = [&](const std::vector<std::size_t>& at) {
            return std::any_of(at.begin(), at.end(), [&](std::size_t c) {
                return std::find(columns.begin(), columns.end(), c) != columns.end();
            });
        };
        if (!table.primary_key.empty() && touches(table.primary_key)) {
            if (const auto key = tuple_of(table, mine, table.primary_key))
                if (others([&](const PracticeRow& other) { return tuple_of(table, other, table.primary_key) == key; }))
                    return problem(PracticeRule::PrimaryKey, t, *row, table.primary_key.front(),
                                   "Primary Key violation", duplicate_key_message(table, mine));
        }
        for (const auto c : columns) {
            const auto& column = table.columns[c];
            if (!column.unique || single_key(table, c)) continue;
            const auto value = practice_value(mine, column);
            if (!value) continue;
            const auto wanted = comparable(column, *value);
            if (others([&](const PracticeRow& other) {
                    const auto theirs = practice_value(other, column);
                    return theirs && comparable(column, *theirs) == wanted;
                }))
                return problem(PracticeRule::Unique, t, *row, c, "Unique constraint violation",
                               column.name + " " + spoken(column, value)
                                   + " already exists. A UNIQUE column cannot hold the same value in two rows.");
        }
        for (const auto& group : key_groups(preview, t)) {
            if (!touches(group.columns)) continue;
            const auto values = tuple_of(table, mine, group.columns);
            if (values && !target_tuples(preview, after, group).contains(*values))
                return problem(PracticeRule::ForeignKey, t, *row, group.columns.front(), "Foreign Key violation",
                               missing_target_message(preview, table, mine, group));
        }
    }
    // A row elsewhere left pointing at nothing by this change: one that found
    // its row before and does not now.
    for (std::size_t ct = 0; ct < preview.tables.size(); ++ct) {
        const auto& child = preview.tables[ct];
        const auto* children = rows_of(after, child);
        if (!children) continue;
        for (const auto& group : key_groups(preview, ct)) {
            if (group.target != t) continue;
            const auto was = target_tuples(preview, before, group);
            const auto now = target_tuples(preview, after, group);
            std::optional<Tuple> lost;
            std::size_t count = 0;
            for (const auto& one : children->rows) {
                const auto values = tuple_of(child, one, group.columns);
                if (!values || !was.contains(*values) || now.contains(*values)) continue;
                if (!lost) lost = values;
                if (*values == *lost) ++count;
            }
            if (!lost) continue;
            std::string which;
            for (std::size_t i = 0; i < group.targets.size(); ++i) {
                const auto& column = table.columns[group.targets[i]];
                if (!which.empty()) which += ", ";
                which += column.name + " " + spoken(column, (*lost)[i]);
            }
            return problem(PracticeRule::Referenced, t, row.value_or(0), group.targets.front(),
                           "Foreign Key violation",
                           "The " + table.name + " row with " + which + " is still referenced by "
                               + std::to_string(count) + (count == 1 ? " row" : " rows") + " in " + child.name
                               + ". Change or delete " + (count == 1 ? "that row" : "those rows") + " first.");
        }
    }
    return std::nullopt;
}

std::string next_counted_value(const PracticeTable& table, const PreviewColumn& column) {
    long long most = 0;
    for (const auto& row : table.rows)
        if (const auto value = practice_value(row, column))
            if (const auto parsed = whole(*value)) most = std::max(most, *parsed);
    return with_digits(most + 1);
}

std::vector<PracticeRow> starter_rows(const SchemaPreview& preview, const PracticeData& data, std::size_t t) {
    std::vector<PracticeRow> rows;
    if (t >= preview.tables.size()) return rows;
    constexpr std::size_t wanted = 3;
    const auto& table = preview.tables[t];
    const auto groups = key_groups(preview, t);
    std::set<std::size_t> referring;
    for (const auto& group : groups) referring.insert(group.columns.begin(), group.columns.end());
    // What each foreign key can point at: the rows already in the table it
    // points at. A table that points at itself is answered row by row below.
    std::vector<std::vector<PracticeRow>> candidates(groups.size());
    for (std::size_t g = 0; g < groups.size(); ++g) {
        if (groups[g].target == t) continue;
        const auto& target = preview.tables[groups[g].target];
        if (const auto* there = rows_of(data, target))
            for (const auto& one : there->rows)
                if (tuple_of(target, one, groups[g].targets)) candidates[g].push_back(one);
    }
    const auto point = [&](PracticeRow& row, std::size_t g, const PracticeRow* at) {
        const auto& group = groups[g];
        const auto& target = preview.tables[group.target];
        for (std::size_t i = 0; i < group.columns.size(); ++i) {
            const auto identity = column_identity(table.columns[group.columns[i]]);
            if (!identity) continue;
            row.values[*identity] = at ? practice_value(*at, target.columns[group.targets[i]]) : std::nullopt;
        }
    };
    for (std::size_t i = 0; i < wanted; ++i) {
        PracticeRow row;
        for (const auto c : practice_columns(table)) {
            if (referring.contains(c)) continue;
            row.values[*column_identity(table.columns[c])] = made_value(table, table.columns[c], i);
        }
        // A table that points at itself: the first row at nothing where it
        // may, or at itself, and each after it at the row before.
        for (std::size_t g = 0; g < groups.size(); ++g) {
            if (groups[g].target != t) continue;
            const bool optional = std::none_of(groups[g].columns.begin(), groups[g].columns.end(), [&](std::size_t c) {
                return table.columns[c].required || table.columns[c].primary_key;
            });
            if (!rows.empty()) {
                point(row, g, &rows.back());
            } else if (optional) {
                point(row, g, nullptr);
            } else {
                const auto itself = row;
                point(row, g, &itself);
            }
        }
        // The other foreign keys: each row along the rows of what it points
        // at, a foreign key further along starting one further on, so a table
        // that points at the same table twice does not point both at one row.
        // Where that would give a key or a unique value a row already has,
        // the next way that does not is taken; where none does, there are
        // fewer rows.
        std::vector<std::size_t> sizes;
        std::size_t ways = 1;
        for (std::size_t g = 0; g < groups.size(); ++g) {
            const auto size = groups[g].target == t ? std::size_t{1} : std::max<std::size_t>(candidates[g].size(), 1);
            sizes.push_back(size);
            ways = std::min<std::size_t>(ways * size, 4096);
        }
        const auto attempt = [&](const std::vector<std::size_t>& choice) {
            auto made = row;
            for (std::size_t g = 0; g < groups.size(); ++g) {
                if (groups[g].target == t) continue;
                point(made, g, candidates[g].empty() ? nullptr : &candidates[g][choice[g]]);
            }
            return made;
        };
        std::vector<std::size_t> diagonal;
        for (std::size_t g = 0; g < groups.size(); ++g) diagonal.push_back((i + g) % sizes[g]);
        auto made = attempt(diagonal);
        bool found = !collides(table, rows, made);
        for (std::size_t way = 0; !found && way < ways; ++way) {
            std::vector<std::size_t> choice;
            auto rest = way;
            for (const auto size : sizes) {
                choice.push_back(rest % size);
                rest /= size;
            }
            made = attempt(choice);
            found = !collides(table, rows, made);
        }
        if (!found) break;
        // NULL is kept as no entry at all, as everywhere else rows are kept.
        std::erase_if(made.values, [](const auto& held) { return !held.second; });
        rows.push_back(std::move(made));
    }
    return rows;
}

PracticeData converted_to_practice(const SchemaPreview& preview, PracticeData data) {
    data.converted = true;
    std::vector<std::size_t> waiting;
    for (std::size_t t = 0; t < preview.tables.size(); ++t)
        if (!data.tables.contains(preview.tables[t].id)) waiting.push_back(t);
    while (!waiting.empty()) {
        // The first table whose references are all to tables that have rows
        // already, or to itself; failing that, where references go round in
        // a circle, simply the first.
        auto next = std::find_if(waiting.begin(), waiting.end(), [&](std::size_t t) {
            const auto groups = key_groups(preview, t);
            return std::all_of(groups.begin(), groups.end(), [&](const KeyGroup& group) {
                return group.target == t
                    || std::find(waiting.begin(), waiting.end(), group.target) == waiting.end();
            });
        });
        if (next == waiting.end()) next = waiting.begin();
        const auto t = *next;
        waiting.erase(next);
        data.tables[preview.tables[t].id].rows = starter_rows(preview, data, t);
    }
    return data;
}

} // namespace erdflow::domain
