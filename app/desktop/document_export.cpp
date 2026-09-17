#include "document_export.hpp"

#include <QBuffer>
#include <QFont>
#include <QImage>
#include <QMarginsF>
#include <QPageSize>
#include <QPdfWriter>
#include <QSaveFile>
#include <QTextDocument>
#include <QUrl>

#include <algorithm>
#include <cmath>

namespace erdflow::desktop {
namespace {

using namespace erdflow::domain;

[[nodiscard]] QString text(const std::string& value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

// A row of the data dictionary, in the one shape all four formats render from,
// so the page, the web page, the Markdown and the CSV cannot disagree about
// what the project contains.
struct Row {
    QString element;   // Entity, Attribute, Relationship, Specialization
    QString name;
    QString belongs;   // what owns it, or the sides it joins
    QString kind;
    QString detail;
    QString description;
};

struct Listing {
    QString title;
    QString summary;
    std::vector<Row> entities;
    std::vector<Row> attributes;
    std::vector<Row> relationships;
    std::vector<Row> hierarchies;
};

[[nodiscard]] QString name_of(const Project& project, const ElementRef& ref) {
    if (const auto* id = std::get_if<EntityId>(&ref)) {
        const auto found = project.entities.find(*id);
        return found == project.entities.end() ? QString() : text(found->second.name);
    }
    if (const auto* id = std::get_if<AttributeId>(&ref)) {
        const auto found = project.attributes.find(*id);
        return found == project.attributes.end() ? QString() : text(found->second.name);
    }
    if (const auto* id = std::get_if<RelationshipId>(&ref)) {
        const auto found = project.relationships.find(*id);
        return found == project.relationships.end() ? QString() : text(found->second.name);
    }
    if (const auto* id = std::get_if<SpecializationId>(&ref)) {
        const auto found = project.specializations.find(*id);
        return found == project.specializations.end() ? QString() : text(found->second.name);
    }
    return {};
}

// An unnamed element is work in progress rather than an error, and a listing
// has to say something about it, so it says exactly that.
[[nodiscard]] QString shown(const QString& name) { return name.isEmpty() ? QStringLiteral("(unnamed)") : name; }

// A key on a weak entity is a partial key: it identifies an instance only once
// the owner is known. The kind stored is the same; what it means is not, and a
// dictionary that called both "Key" would be telling the reader something
// false about the model.
[[nodiscard]] QString attribute_kind_name(const Project& project, const Attribute& attribute) {
    const bool weak_owner = attribute.owner
        && std::holds_alternative<EntityId>(*attribute.owner)
        && [&] {
               const auto found = project.entities.find(std::get<EntityId>(*attribute.owner));
               return found != project.entities.end() && found->second.weak;
           }();
    switch (attribute.kind) {
        case AttributeKind::Key: return weak_owner ? QStringLiteral("Partial key") : QStringLiteral("Key");
        case AttributeKind::Composite: return QStringLiteral("Composite");
        case AttributeKind::Multivalued: return QStringLiteral("Multivalued");
        case AttributeKind::Derived: return QStringLiteral("Derived");
        case AttributeKind::Normal: break;
    }
    return QStringLiteral("Simple");
}

[[nodiscard]] QString relationship_kind_name(const Relationship& relationship) {
    switch (relationship_kind(relationship)) {
        case RelationshipKind::Identifying: return QStringLiteral("Identifying");
        case RelationshipKind::Associative: return QStringLiteral("Associative");
        case RelationshipKind::Regular: break;
    }
    return QStringLiteral("Regular");
}

// One side of a relationship in words: the pair the diagram draws beside the
// line, said as a reader would say it rather than as (0,M).
[[nodiscard]] QString side_text(const Project& project, const Participant& participant) {
    const auto target = std::holds_alternative<EntityId>(participant.target)
        ? ElementRef{std::get<EntityId>(participant.target)}
        : ElementRef{std::get<RelationshipId>(participant.target)};
    auto said = shown(name_of(project, target));
    if (!participant.role.empty()) said += " as " + text(participant.role);
    said += participant.maximum == Cardinality::Many ? " (many, " : " (one, ";
    said += participant.participation == Participation::Total ? "mandatory)" : "optional)";
    return said;
}

// Sorted by what a reader looks for, which is the name, and by identity after
// it so two elements sharing a name always come out in the same order and the
// same project always produces the same file.
template<class Map, class Fn>
void in_reading_order(const Map& map, Fn&& append) {
    std::vector<typename Map::const_iterator> order;
    order.reserve(map.size());
    for (auto it = map.begin(); it != map.end(); ++it) order.push_back(it);
    std::sort(order.begin(), order.end(), [](const auto& left, const auto& right) {
        if (left->second.name != right->second.name) return left->second.name < right->second.name;
        return left->first.value.bytes < right->first.value.bytes;
    });
    for (const auto& it : order) append(it->first, it->second);
}

[[nodiscard]] Listing listing_of(const Project& project) {
    Listing listing;
    listing.title = shown(text(project.name));

    in_reading_order(project.entities, [&](const EntityId& id, const Entity& entity) {
        // What hangs off it, named in the order it is listed below, so the
        // summary line and the attribute table tell the same story.
        std::vector<QString> owned;
        in_reading_order(project.attributes, [&](const AttributeId&, const Attribute& attribute) {
            if (attribute.owner && std::holds_alternative<EntityId>(*attribute.owner)
                && std::get<EntityId>(*attribute.owner) == id)
                owned.push_back(shown(text(attribute.name)));
        });
        listing.entities.push_back({QStringLiteral("Entity"), shown(text(entity.name)), {},
                                    entity.weak ? QStringLiteral("Weak") : QStringLiteral("Regular"),
                                    owned.empty() ? QStringLiteral("—")
                                                  : QStringList(QList<QString>(owned.begin(), owned.end())).join(", "),
                                    text(entity.description)});
    });

    in_reading_order(project.attributes, [&](const AttributeId&, const Attribute& attribute) {
        listing.attributes.push_back({QStringLiteral("Attribute"), shown(text(attribute.name)),
                                      attribute.owner ? shown(name_of(project, *attribute.owner))
                                                      : QStringLiteral("(not attached)"),
                                      attribute_kind_name(project, attribute), {},
                                      text(attribute.description)});
    });

    in_reading_order(project.relationships, [&](const RelationshipId& id, const Relationship& relationship) {
        std::vector<QString> sides;
        for (const auto& participant : relationship.participants) sides.push_back(side_text(project, participant));
        std::vector<QString> carried;
        in_reading_order(project.attributes, [&](const AttributeId&, const Attribute& attribute) {
            if (attribute.owner && std::holds_alternative<RelationshipId>(*attribute.owner)
                && std::get<RelationshipId>(*attribute.owner) == id)
                carried.push_back(shown(text(attribute.name)));
        });
        listing.relationships.push_back({QStringLiteral("Relationship"), shown(text(relationship.name)),
            sides.empty() ? QStringLiteral("(not connected)")
                          : QStringList(QList<QString>(sides.begin(), sides.end())).join(" — "),
            relationship_kind_name(relationship),
            carried.empty() ? QStringLiteral("—")
                            : QStringList(QList<QString>(carried.begin(), carried.end())).join(", "),
            text(relationship.description)});
    });

    in_reading_order(project.specializations, [&](const SpecializationId&, const Specialization& hierarchy) {
        std::vector<QString> subtypes;
        for (const auto& subtype : hierarchy.subtypes) subtypes.push_back(shown(name_of(project, ElementRef{subtype})));
        auto constraint = hierarchy.constraint == Disjointness::Disjoint ? QStringLiteral("Disjoint")
                                                                        : QStringLiteral("Overlapping");
        constraint += hierarchy.completeness == Completeness::Total ? QStringLiteral(", total")
                                                                   : QStringLiteral(", partial");
        listing.hierarchies.push_back({QStringLiteral("Hierarchy"), shown(text(hierarchy.name)),
            hierarchy.supertype ? shown(name_of(project, ElementRef{*hierarchy.supertype}))
                                : QStringLiteral("(no supertype yet)"),
            hierarchy.direction == Inheritance::Generalization ? QStringLiteral("Generalization")
                                                              : QStringLiteral("Specialization"),
            subtypes.empty() ? QStringLiteral("—")
                             : QStringList(QList<QString>(subtypes.begin(), subtypes.end())).join(", "),
            constraint});
    });

    listing.summary = QString("%1 %2, %3 %4, %5 %6")
        .arg(project.entities.size()).arg(project.entities.size() == 1 ? "entity" : "entities")
        .arg(project.attributes.size()).arg(project.attributes.size() == 1 ? "attribute" : "attributes")
        .arg(project.relationships.size())
        .arg(project.relationships.size() == 1 ? "relationship" : "relationships");
    if (!project.specializations.empty())
        listing.summary += QString(", %1 %2").arg(project.specializations.size())
                               .arg(project.specializations.size() == 1 ? "hierarchy" : "hierarchies");
    return listing;
}

[[nodiscard]] QString escaped_html(const QString& value) {
    return value.toHtmlEscaped();
}

// A pipe or a line break inside a cell would end the cell early and break the
// table around it, so both are written as something that survives the row.
[[nodiscard]] QString escaped_markdown(const QString& value) {
    auto cell = value;
    cell.replace('\\', "\\\\");
    cell.replace('|', "\\|");
    cell.replace('\n', "<br>");
    return cell.isEmpty() ? QStringLiteral("—") : cell;
}

[[nodiscard]] QString escaped_csv(const QString& value) {
    auto cell = value;
    cell.replace('"', "\"\"");
    return '"' + cell + '"';
}

struct Column { const char* heading; QString Row::*field; };

const std::vector<Column>& entity_columns() {
    static const std::vector<Column> columns{{"Entity", &Row::name}, {"Kind", &Row::kind},
                                             {"Attributes", &Row::detail}, {"Description", &Row::description}};
    return columns;
}
const std::vector<Column>& attribute_columns() {
    static const std::vector<Column> columns{{"Attribute", &Row::name}, {"Belongs to", &Row::belongs},
                                             {"Kind", &Row::kind}, {"Description", &Row::description}};
    return columns;
}
const std::vector<Column>& relationship_columns() {
    static const std::vector<Column> columns{{"Relationship", &Row::name}, {"Kind", &Row::kind},
                                             {"Between", &Row::belongs}, {"Attributes", &Row::detail},
                                             {"Description", &Row::description}};
    return columns;
}
const std::vector<Column>& hierarchy_columns() {
    static const std::vector<Column> columns{{"Hierarchy", &Row::name}, {"Read as", &Row::kind},
                                             {"Supertype", &Row::belongs}, {"Subtypes", &Row::detail},
                                             {"Constraint", &Row::description}};
    return columns;
}

struct Section { const char* heading; const std::vector<Row> Listing::*rows; const std::vector<Column>& (*columns)(); };

const std::vector<Section>& sections() {
    static const std::vector<Section> list{
        {"Entities", &Listing::entities, entity_columns},
        {"Relationships", &Listing::relationships, relationship_columns},
        {"Attributes", &Listing::attributes, attribute_columns},
        {"Hierarchies", &Listing::hierarchies, hierarchy_columns},
    };
    return list;
}

[[nodiscard]] QByteArray markdown_of(const Listing& listing) {
    QString out;
    out += "# " + listing.title + "\n\n";
    out += listing.summary + ".\n";
    for (const auto& section : sections()) {
        const auto& rows = listing.*section.rows;
        if (rows.empty()) continue;
        const auto& columns = section.columns();
        out += QString("\n## %1\n\n").arg(QString::fromLatin1(section.heading));
        QStringList heads, rules;
        for (const auto& column : columns) {
            heads << QString::fromLatin1(column.heading);
            rules << "---";
        }
        out += "| " + heads.join(" | ") + " |\n";
        out += "| " + rules.join(" | ") + " |\n";
        for (const auto& row : rows) {
            QStringList cells;
            for (const auto& column : columns) cells << escaped_markdown(row.*column.field);
            out += "| " + cells.join(" | ") + " |\n";
        }
    }
    out += "\nWritten by ERDFlow. This is a listing of the model, not the model:"
           " reopen the project, or a picture carrying it, to edit the diagram.\n";
    // LF endings and UTF-8 with no byte-order mark, so the file reads the same
    // on every machine and usefully in version control.
    return out.toUtf8();
}

[[nodiscard]] QByteArray csv_of(const Listing& listing) {
    QString out = "Element,Name,Belongs to,Kind,Detail,Description\n";
    for (const auto& section : sections()) {
        for (const auto& row : listing.*section.rows) {
            const QStringList cells{escaped_csv(row.element), escaped_csv(row.name), escaped_csv(row.belongs),
                                    escaped_csv(row.kind), escaped_csv(row.detail), escaped_csv(row.description)};
            out += cells.join(',') + "\n";
        }
    }
    return out.toUtf8();
}

// The tables both page-shaped formats share. Kept as one string so the PDF and
// the web page cannot drift into listing different things.
[[nodiscard]] QString tables_html(const Listing& listing) {
    QString out;
    for (const auto& section : sections()) {
        const auto& rows = listing.*section.rows;
        if (rows.empty()) continue;
        const auto& columns = section.columns();
        out += QString("<h2>%1</h2>\n<table width=\"100%\" cellspacing=\"0\" cellpadding=\"5\" border=\"1\">\n<tr>")
                   .arg(QString::fromLatin1(section.heading));
        for (const auto& column : columns)
            out += QString("<th align=\"left\">%1</th>").arg(QString::fromLatin1(column.heading));
        out += "</tr>\n";
        for (const auto& row : rows) {
            out += "<tr>";
            for (const auto& column : columns) {
                const auto value = row.*column.field;
                out += "<td>" + (value.isEmpty() ? QStringLiteral("—") : escaped_html(value)) + "</td>";
            }
            out += "</tr>\n";
        }
        out += "</table>\n";
    }
    return out;
}

// The diagram, drawn the way the picture half draws it, so a report and an
// exported picture of the same project are the same picture.
[[nodiscard]] PictureResult diagram_picture(DiagramView& view, PictureFormat format, double target_width,
                                            QByteArray& out) {
    PictureOptions options;
    options.format = format;
    options.extent = PictureExtent::WholeDiagram;
    options.background = PictureBackground::White;
    options.margin = 24;
    options.carry_project = false;
    const auto bounds = picture_extent(view, PictureExtent::WholeDiagram);
    const auto wide = bounds.width() + options.margin * 2;
    options.scale = wide > 0 ? std::clamp(target_width / wide, 0.5, 4.0) : 1.0;
    return draw_picture(view, options, {}, out);
}

[[nodiscard]] QByteArray html_of(DiagramView& view, const Listing& listing) {
    QByteArray drawing;
    QString picture;
    // One self-contained page: the diagram travels inside it as inline SVG, so
    // there is no sidecar file and no link that can break in transit.
    if (diagram_picture(view, PictureFormat::Svg, 1200, drawing)) {
        const auto start = drawing.indexOf("<svg");
        if (start >= 0) picture = QString::fromUtf8(drawing.mid(start));
    }
    QString out;
    out += "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n";
    out += "<title>" + escaped_html(listing.title) + "</title>\n";
    out += "<style>\n"
           "  :root { color-scheme: light dark; }\n"
           "  body { font-family: 'Helvetica Neue', Helvetica, Arial, 'Liberation Sans', sans-serif;\n"
           "         margin: 0 auto; max-width: 60rem; padding: 2rem 1rem; line-height: 1.5; }\n"
           "  h1 { margin-bottom: 0.25rem; }\n"
           "  .summary { color: #666; margin-top: 0; }\n"
           "  figure { margin: 2rem 0; }\n"
           "  figure svg { max-width: 100%; height: auto; }\n"
           "  table { border-collapse: collapse; width: 100%; margin-bottom: 2rem; }\n"
           "  th, td { border: 1px solid #ccc; padding: 0.4rem 0.6rem; text-align: left;\n"
           "           vertical-align: top; }\n"
           "  th { background: #f4f4f4; }\n"
           "  footer { color: #666; font-size: 0.9rem; border-top: 1px solid #ccc; padding-top: 1rem; }\n"
           "  @media (prefers-color-scheme: dark) {\n"
           "    body { background: #14161a; color: #e6e6e6; }\n"
           "    .summary, footer { color: #9aa0a6; }\n"
           "    th { background: #23262b; }\n"
           "    th, td, footer { border-color: #3a3f46; }\n"
           "    figure { background: #ffffff; border-radius: 6px; padding: 1rem; }\n"
           "  }\n"
           "</style>\n</head>\n<body>\n";
    out += "<h1>" + escaped_html(listing.title) + "</h1>\n";
    out += "<p class=\"summary\">" + escaped_html(listing.summary) + ".</p>\n";
    if (!picture.isEmpty()) out += "<figure>\n" + picture + "\n</figure>\n";
    out += tables_html(listing);
    out += "<footer>Written by ERDFlow. This is a listing of the model, not the model: "
           "reopen the project, or a picture carrying it, to edit the diagram.</footer>\n";
    out += "</body>\n</html>\n";
    return out.toUtf8();
}

[[nodiscard]] DocumentResult pdf_of(DiagramView& view, const Listing& listing, QByteArray& out) {
    QBuffer buffer(&out);
    if (!buffer.open(QIODevice::WriteOnly)) return {false, "The document could not be written."};
    {
        QPdfWriter writer(&buffer);
        writer.setCreator("ERDFlow");
        writer.setTitle(listing.title);
        writer.setResolution(300);
        writer.setPageSize(QPageSize(QPageSize::A4));
        writer.setPageMargins(QMarginsF(15, 15, 15, 15), QPageLayout::Millimeter);

        QTextDocument document;
        // Named rather than left to the toolkit's default, so a report reads
        // the same wherever it is opened, and set at a size meant for paper.
        QFont face("Helvetica");
        face.setStyleHint(QFont::SansSerif);
        face.setPointSizeF(10);
        document.setDefaultFont(face);
        document.setPageSize(QSizeF(writer.width(), writer.height()));
        // A page cannot hold an SVG: Qt's rich text understands raster images
        // only. The diagram is therefore drawn once, large enough for print,
        // and handed to the document as a resource so the file stays one file.
        QByteArray drawing;
        QString picture;
        if (diagram_picture(view, PictureFormat::Png, 2000, drawing)) {
            QImage image;
            if (image.loadFromData(drawing, "png")) {
                document.addResource(QTextDocument::ImageResource, QUrl("erdflow:diagram"), QVariant(image));
                // Sized to the text width rather than to its own pixels, so the
                // picture fills the page it is on whatever scale it was drawn
                // at, and then held to the height of a page: a tall diagram
                // drawn at full width would run off the bottom and be cut in
                // half by the page break, which is worse than being smaller.
                const auto page_width = writer.width();
                const auto page_height = writer.height() * 0.8;
                const auto ratio = static_cast<double>(image.height()) / std::max(1, image.width());
                auto width = static_cast<double>(page_width);
                if (width * ratio > page_height) width = page_height / std::max(ratio, 0.0001);
                picture = QString("<p><img src=\"erdflow:diagram\" width=\"%1\" height=\"%2\"></p>")
                              .arg(static_cast<int>(std::llround(width)))
                              .arg(static_cast<int>(std::llround(width * ratio)));
            }
        }
        QString html = "<h1>" + escaped_html(listing.title) + "</h1>";
        html += "<p>" + escaped_html(listing.summary) + ".</p>";
        html += picture;
        html += tables_html(listing);
        html += "<p>Written by ERDFlow. This is a listing of the model, not the model: "
                "reopen the project, or a picture carrying it, to edit the diagram.</p>";
        document.setHtml(html);
        document.print(&writer);
    }
    buffer.close();
    return {true, {}};
}

} // namespace

const std::vector<DocumentFormatInfo>& document_formats() {
    static const std::vector<DocumentFormatInfo> formats{
        {DocumentFormat::Pdf, "PDF document", "pdf", true,
         "A paginated report: the diagram, then a data dictionary. It is a listing, not a project — the PDF page under Pictures is the diagram alone."},
        {DocumentFormat::Markdown, "Markdown data dictionary", "md", false,
         "Tables for a repository README. Text only: a picture embedded in Markdown is stripped by most readers, so the diagram is not in it."},
        {DocumentFormat::Html, "HTML report", "html", true,
         "One self-contained page carrying the diagram as inline SVG above the data dictionary."},
        {DocumentFormat::Csv, "CSV listing", "csv", false,
         "Every element as one row, for a spreadsheet."},
    };
    return formats;
}

const DocumentFormatInfo& document_format(DocumentFormat format) {
    const auto& formats = document_formats();
    const auto found = std::find_if(formats.begin(), formats.end(),
                                    [format](const DocumentFormatInfo& info) { return info.format == format; });
    return found == formats.end() ? formats.front() : *found;
}

DocumentResult draw_document(DiagramView& view, const Project& project, DocumentFormat format, QByteArray& out) {
    if (project.entities.empty() && project.relationships.empty() && project.attributes.empty())
        return {false, "The project is empty, so there is nothing to list."};
    const auto listing = listing_of(project);
    out.clear();
    switch (format) {
        case DocumentFormat::Markdown: out = markdown_of(listing); return {true, {}};
        case DocumentFormat::Csv: out = csv_of(listing); return {true, {}};
        case DocumentFormat::Html: out = html_of(view, listing); return {true, {}};
        case DocumentFormat::Pdf: return pdf_of(view, listing, out);
    }
    return {false, "That document format is not one ERDFlow writes."};
}

DocumentResult write_document(DiagramView& view, const Project& project, DocumentFormat format, const QString& path) {
    QByteArray bytes;
    auto result = draw_document(view, project, format, bytes);
    if (!result) return result;
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return {false, file.errorString()};
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return {false, file.errorString()};
    }
    if (!file.commit()) return {false, file.errorString()};
    return result;
}

} // namespace erdflow::desktop
