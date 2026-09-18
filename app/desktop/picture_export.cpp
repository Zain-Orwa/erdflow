#include "picture_export.hpp"

#include <QBuffer>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageWriter>
#include <QMarginsF>
#include <QPageSize>
#include <QPainter>
#include <QPdfWriter>
#include <QSaveFile>
#include <QSvgGenerator>
#include <QXmlStreamReader>

#include <algorithm>
#include <cmath>

namespace erdflow::desktop {
namespace {

// Where the project rides inside a picture. In SVG it is an element of its own
// inside the metadata element the format already reserves for exactly this; in
// PNG it is a text chunk under a keyword of ERDFlow's own. Both are skipped by
// every other reader of those formats, so a picture carrying a project is an
// ordinary picture to everything that is not ERDFlow.
constexpr char payload_namespace[] = "https://erdflow.app/erdx";
constexpr char payload_keyword[] = "erdflow-project";

[[nodiscard]] QByteArray image_handler(PictureFormat format) {
    switch (format) {
        case PictureFormat::Png: return "png";
        case PictureFormat::Jpeg: return "jpeg";
        case PictureFormat::WebP: return "webp";
        case PictureFormat::Tiff: return "tiff";
        case PictureFormat::Svg:
        case PictureFormat::Pdf: break;
    }
    return {};
}

// The end of an opening tag, found without being fooled by a '>' that is
// inside one of its attributes.
[[nodiscard]] qsizetype end_of_tag(const QByteArray& xml, qsizetype from) {
    char quote = 0;
    for (auto cursor = from; cursor < xml.size(); ++cursor) {
        const auto character = xml.at(cursor);
        if (quote) {
            if (character == quote) quote = 0;
        } else if (character == '"' || character == '\'') {
            quote = character;
        } else if (character == '>') {
            return cursor;
        }
    }
    return -1;
}

// Qt names its default families in words -- "Sans Serif", "Serif",
// "Monospace" -- and no CSS engine knows any of them, so an SVG naming one
// falls back to whatever the reader happens to default to. Each is replaced by
// a chain that ends in the CSS generic family it meant. The chain is what makes
// the file portable: the reader uses the first name its machine actually has,
// and the generic at the end exists everywhere. A family the user chose by name
// is left exactly as it was; only Qt's own aliases are rewritten.
//
// This is the one thing an SVG cannot carry cheaply. Naming a chain keeps the
// text selectable and searchable at the cost of exact metrics on a machine
// without the first font; embedding the outlines instead would keep the
// metrics and lose the text, which is a trade for a later option rather than a
// default.
struct FontChain { const char* alias; const char* chain; };
const std::vector<FontChain>& font_chains() {
    static const std::vector<FontChain> chains{
        {"Sans Serif", "'Helvetica Neue', Helvetica, Arial, 'Liberation Sans', sans-serif"},
        {"Serif", "Georgia, 'Times New Roman', 'Liberation Serif', serif"},
        {"Monospace", "'SF Mono', Menlo, Consolas, 'Liberation Mono', monospace"},
    };
    return chains;
}

void name_portable_fonts(QByteArray& svg) {
    for (const auto& chain : font_chains()) {
        const QByteArray named = QByteArray("font-family=\"") + chain.alias + "\"";
        svg.replace(named, QByteArray("font-family=\"") + chain.chain + "\"");
    }
}

bool put_payload_in_svg(QByteArray& svg, const QByteArray& payload) {
    const auto open = svg.indexOf("<svg");
    if (open < 0) return false;
    const auto close = end_of_tag(svg, open + 4);
    if (close < 0) return false;
    QByteArray block = "\n<metadata>\n<erdflow:project xmlns:erdflow=\"";
    block += payload_namespace;
    block += "\" encoding=\"base64\">";
    block += payload.toBase64();
    block += "</erdflow:project>\n</metadata>";
    svg.insert(close + 1, block);
    return true;
}

[[nodiscard]] QByteArray payload_from_svg(const QByteArray& svg) {
    QXmlStreamReader reader(svg);
    while (!reader.atEnd()) {
        if (reader.readNext() != QXmlStreamReader::StartElement) continue;
        if (reader.name() != QLatin1String("project")) continue;
        if (reader.namespaceUri() != QLatin1String(payload_namespace)) continue;
        const auto encoded = reader.readElementText().toLatin1();
        const auto decoded = QByteArray::fromBase64Encoding(
            encoded, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
        return decoded ? *decoded : QByteArray();
    }
    return {};
}

[[nodiscard]] QByteArray payload_from_png(const QByteArray& png) {
    QImage image;
    if (!image.loadFromData(png, "png")) return {};
    const auto encoded = image.text(QString::fromLatin1(payload_keyword)).toLatin1();
    if (encoded.isEmpty()) return {};
    const auto decoded = QByteArray::fromBase64Encoding(
        encoded, QByteArray::Base64Encoding | QByteArray::AbortOnBase64DecodingErrors);
    return decoded ? *decoded : QByteArray();
}

[[nodiscard]] QColor background_colour(DiagramView& view, const PictureOptions& options, bool keeps_transparency) {
    switch (options.background) {
        case PictureBackground::ThemeColour: return view.canvas_colour();
        case PictureBackground::White: return Qt::white;
        case PictureBackground::Transparent: break;
    }
    // A format with nowhere to keep transparency is given paper instead of a
    // colour chosen for it, since the alternative is whatever the encoder
    // happens to leave behind, which is usually black.
    return keeps_transparency ? QColor(Qt::transparent) : QColor(Qt::white);
}

} // namespace

const std::vector<PictureFormatInfo>& picture_formats() {
    // SVG leads because it is the default download: it is the only raster-free
    // picture that also carries the project, and it reads at any size.
    static const std::vector<PictureFormatInfo> formats{
        {PictureFormat::Svg, "SVG picture", "svg", false, true, true, ""},
        {PictureFormat::Png, "PNG picture", "png", true, true, true, ""},
        {PictureFormat::Jpeg, "JPEG picture", "jpg", true, false, false,
         "JPEG smears the edges of text and lines, and cannot carry the project. Prefer PNG unless the destination insists on JPEG."},
        {PictureFormat::WebP, "WebP picture", "webp", true, false, true,
         "WebP keeps the picture small, but carries no project and is not read everywhere."},
        {PictureFormat::Tiff, "TIFF picture", "tiff", true, false, true,
         "TIFF is for print and publishing workflows that ask for it. It carries no project."},
        {PictureFormat::Pdf, "PDF page", "pdf", false, false, false,
         "A PDF page is measured in dots per inch rather than pixels, and carries no project."},
    };
    return formats;
}

const PictureFormatInfo& picture_format(PictureFormat format) {
    const auto& formats = picture_formats();
    const auto found = std::find_if(formats.begin(), formats.end(),
                                    [format](const PictureFormatInfo& info) { return info.format == format; });
    return found == formats.end() ? formats.front() : *found;
}

bool picture_format_available(PictureFormat format) {
    // SVG and PDF are written by Qt itself. The rasters are plugins, and a
    // build without one must not offer a format it cannot actually write.
    const auto handler = image_handler(format);
    if (handler.isEmpty()) return true;
    const auto supported = QImageWriter::supportedImageFormats();
    return std::find(supported.begin(), supported.end(), handler) != supported.end();
}

QRectF picture_extent(const DiagramView& view, PictureExtent extent) {
    switch (extent) {
        case PictureExtent::Selection: return view.selection_bounds();
        case PictureExtent::CurrentView: return view.view_bounds();
        case PictureExtent::WholeDiagram: break;
    }
    return view.diagram_bounds();
}

PictureResult draw_picture(DiagramView& view, const PictureOptions& options,
                           const QByteArray& payload, QByteArray& out) {
    const auto& info = picture_format(options.format);
    PictureResult result;
    if (!picture_format_available(options.format))
        return {false, QString("This build of ERDFlow cannot write %1.").arg(QString::fromUtf8(info.label)), {}, false, {}};

    auto source = picture_extent(view, options.extent);
    if (source.isEmpty()) {
        const auto what = options.extent == PictureExtent::Selection ? "Nothing is selected, so there is nothing to export."
                        : options.extent == PictureExtent::CurrentView ? "The view is empty, so there is nothing to export."
                        : "The diagram is empty, so there is nothing to export.";
        return {false, QString::fromUtf8(what), {}, false, {}};
    }
    const auto margin = std::max(0.0, options.margin);
    source = source.adjusted(-margin, -margin, margin, margin);

    const auto scale = std::clamp(options.scale, 0.05, 40.0);
    const auto wide = static_cast<long long>(std::llround(source.width() * scale));
    const auto high = static_cast<long long>(std::llround(source.height() * scale));
    if (wide < 1 || high < 1) return {false, "That scale makes a picture with nothing in it.", {}, false, {}};
    if (wide > max_picture_edge || high > max_picture_edge || wide * high > max_picture_pixels)
        return {false, QString("A picture that size (%1 by %2) is larger than ERDFlow will draw. "
                               "Lower the scale, or export a selection.").arg(wide).arg(high), {}, false, {}};
    const QSize pixels(static_cast<int>(wide), static_cast<int>(high));
    result.pixels = pixels;

    out.clear();
    QBuffer buffer(&out);
    if (!buffer.open(QIODevice::WriteOnly)) return {false, "The picture could not be drawn.", {}, false, pixels};

    if (options.format == PictureFormat::Svg) {
        const QRectF target(0, 0, pixels.width(), pixels.height());
        {
            QSvgGenerator generator;
            generator.setOutputDevice(&buffer);
            generator.setSize(pixels);
            generator.setViewBox(QRect(QPoint(0, 0), pixels));
            QPainter painter(&generator);
            const auto colour = background_colour(view, options, info.keeps_transparency);
            if (colour.alpha() != 0) painter.fillRect(target, colour);
            view.render_diagram(painter, target, source);
        }
        buffer.close();
    } else if (options.format == PictureFormat::Pdf) {
        {
            QPdfWriter writer(&buffer);
            writer.setCreator("ERDFlow");
            writer.setResolution(std::clamp(options.resolution, 72, 1200));
            // A diagram unit is a pixel at the usual 96 per inch, so a page is
            // the diagram's own size in inches, at whatever scale was asked for.
            const QSizeF points(source.width() * scale * 72.0 / 96.0, source.height() * scale * 72.0 / 96.0);
            writer.setPageSize(QPageSize(points, QPageSize::Point, QString(), QPageSize::ExactMatch));
            writer.setPageMargins(QMarginsF(0, 0, 0, 0));
            QPainter painter(&writer);
            const QRectF target(0, 0, writer.width(), writer.height());
            const auto colour = background_colour(view, options, info.keeps_transparency);
            if (colour.alpha() != 0) painter.fillRect(target, colour);
            view.render_diagram(painter, target, source);
        }
        buffer.close();
    } else {
        QImage image(pixels, QImage::Format_ARGB32_Premultiplied);
        if (image.isNull()) return {false, "There was not enough memory for a picture that size.", {}, false, pixels};
        image.fill(background_colour(view, options, info.keeps_transparency));
        {
            QPainter painter(&image);
            view.render_diagram(painter, QRectF(QPointF(0, 0), QSizeF(pixels)), source);
        }
        // The project goes in before the file is encoded rather than being cut
        // back into it afterwards, so a PNG is written once.
        if (info.carries_project && options.carry_project && !payload.isEmpty()) {
            image.setText(QString::fromLatin1(payload_keyword), QString::fromLatin1(payload.toBase64()));
            result.carried_project = true;
        }
        QImageWriter writer(&buffer, image_handler(options.format));
        if (options.format == PictureFormat::Jpeg || options.format == PictureFormat::WebP) writer.setQuality(92);
        if (!writer.write(image)) {
            buffer.close();
            out.clear();
            return {false, writer.errorString(), {}, false, pixels};
        }
        buffer.close();
    }

    if (options.format == PictureFormat::Svg) name_portable_fonts(out);
    if (options.format == PictureFormat::Svg && options.carry_project && !payload.isEmpty()) {
        if (put_payload_in_svg(out, payload)) result.carried_project = true;
        else result.carried_note = "The picture was written, but the project could not be placed inside it.";
    }
    if (info.carries_project && options.carry_project && payload.isEmpty() && result.carried_note.isEmpty())
        result.carried_note = "The picture was written without the project inside it.";
    result.ok = true;
    return result;
}

PictureResult write_picture(DiagramView& view, const PictureOptions& options,
                            const QByteArray& payload, const QString& path) {
    QByteArray bytes;
    auto result = draw_picture(view, options, payload, bytes);
    if (!result) return result;
    // Replaced in one step, the way a project file is, so a failed write never
    // leaves a half-drawn picture where a good one was.
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    if (!file.open(QIODevice::WriteOnly)) return {false, file.errorString(), {}, false, result.pixels};
    if (file.write(bytes) != bytes.size()) {
        file.cancelWriting();
        return {false, file.errorString(), {}, false, result.pixels};
    }
    if (!file.commit()) return {false, file.errorString(), {}, false, result.pixels};
    return result;
}

QByteArray payload_of_picture(const QByteArray& picture) {
    if (picture.size() > max_carrier_bytes) return {};
    if (picture.startsWith(QByteArray("\x89PNG\r\n\x1a\n", 8))) return payload_from_png(picture);
    if (picture.indexOf("<svg") >= 0) return payload_from_svg(picture);
    return {};
}

QByteArray payload_of_picture_file(const QString& path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    if (file.size() > max_carrier_bytes) return {};
    return payload_of_picture(file.read(max_carrier_bytes));
}

bool may_carry_project(const QString& path) {
    const auto suffix = QFileInfo(path).suffix().toLower();
    return suffix == "svg" || suffix == "png";
}

} // namespace erdflow::desktop
