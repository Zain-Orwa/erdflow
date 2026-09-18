#pragma once

#include "diagram_view.hpp"

#include <QByteArray>
#include <QSize>
#include <QString>
#include <vector>

namespace erdflow::desktop {

// A picture of the diagram, which any system can open and no system can edit
// back into a model. Two of these carry the project inside them as well, in a
// place every other reader of that format already ignores, so one file is both
// the picture a recipient opens and the project ERDFlow reopens without loss.
// The lossy and the niche formats carry nothing: a file that looks like it
// holds the project and does not is worse than one that never claimed to.
enum class PictureFormat { Svg, Png, Jpeg, WebP, Tiff, Pdf };

// How much of the diagram the picture holds.
enum class PictureExtent { WholeDiagram, Selection, CurrentView };

// What the diagram stands on. The theme colour is the canvas as the editor is
// showing it; white is for a destination that assumes paper; and nothing at
// all is for a picture that has to sit on someone else's background.
enum class PictureBackground { Transparent, ThemeColour, White };

struct PictureOptions {
    PictureFormat format = PictureFormat::Svg;
    PictureExtent extent = PictureExtent::WholeDiagram;
    PictureBackground background = PictureBackground::Transparent;
    // How large the picture is drawn, as a multiple of the diagram's own size.
    double scale = 1.0;
    // Only a PDF is measured in dots per inch, since it is the one format whose
    // page has a physical size rather than a number of pixels.
    int resolution = 300;
    // Room left around the diagram, in the diagram's own units, so nothing is
    // drawn hard against the edge of the file.
    double margin = 24.0;
    // Whether the picture carries the project. Ignored by the formats that
    // cannot hold one.
    bool carry_project = true;
};

struct PictureResult {
    bool ok = false;
    QString error;
    // Set when the picture was written but went without the project. It is not
    // a failure: the picture is valid and the reason is worth saying plainly,
    // because a recipient who expects to reopen it needs to know.
    QString carried_note;
    bool carried_project = false;
    // What was actually drawn, for the window to report.
    QSize pixels;
    explicit operator bool() const { return ok; }
};

// What each format is called, what it is written as, and what it can do. The
// list is the one source for the dialog, the file filters and the tests, so
// none of them can claim a format the writer does not have.
struct PictureFormatInfo {
    PictureFormat format;
    const char* label;
    const char* suffix;
    // A raster is measured in pixels and takes a scale; a vector or a page is not.
    bool raster;
    bool carries_project;
    bool keeps_transparency;
    // Said where the format is offered, when there is something a person should
    // know before choosing it. Empty for the formats with nothing to warn about.
    const char* caution;
};
[[nodiscard]] const std::vector<PictureFormatInfo>& picture_formats();
[[nodiscard]] const PictureFormatInfo& picture_format(PictureFormat format);
// Whether this build can write the format at all. Qt writes SVG and PDF
// itself, but every raster is a plugin, and a build missing one must not offer
// a format it would then fail to produce.
[[nodiscard]] bool picture_format_available(PictureFormat format);

// The rectangle, in scene units, that an extent covers before its margin is
// added. Empty when there is nothing there to draw.
[[nodiscard]] QRectF picture_extent(const DiagramView& view, PictureExtent extent);

// A picture beyond these is refused rather than attempted, because the
// allocation would be the failure rather than the picture. They are generous:
// the larger is a wall-sized print of a diagram at four times its own size.
inline constexpr int max_picture_edge = 20000;
inline constexpr long long max_picture_pixels = 80'000'000;

// Draws the diagram into bytes. The payload is opaque here: this half of
// export knows how to put bytes inside an SVG and a PNG, and nothing at all
// about what those bytes mean. Empty payload means the picture carries none.
PictureResult draw_picture(DiagramView& view, const PictureOptions& options,
                           const QByteArray& payload, QByteArray& out);

// The same, written to a file, replaced atomically the way a project is, so a
// failed export never leaves a half-written picture where a good one was.
PictureResult write_picture(DiagramView& view, const PictureOptions& options,
                            const QByteArray& payload, const QString& path);

// The project bytes a picture is carrying, or nothing when it carries none.
// A file ERDFlow did not write is an ordinary picture, not an error.
[[nodiscard]] QByteArray payload_of_picture(const QByteArray& picture);
[[nodiscard]] QByteArray payload_of_picture_file(const QString& path);
// Whether a file's name says it is a picture ERDFlow could be carrying a
// project inside.
[[nodiscard]] bool may_carry_project(const QString& path);

// A picture read back in is bounded the way a project file is, with room for
// the picture drawn around the project it carries.
inline constexpr qsizetype max_carrier_bytes = 64 * 1024 * 1024;

} // namespace erdflow::desktop
