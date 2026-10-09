// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "picture_export.hpp"

#include <QByteArray>
#include <QString>
#include <vector>

namespace erdflow::desktop {

// A written listing of the project rather than a picture of it: what the
// diagram says, set out as a data dictionary somebody can read, search and
// paste into a document. These read the model that already exists and need no
// conversion, so they are cheap to produce and cost nothing to keep correct.
//
// They are listings, not interchange. Nothing re-imports them, and none of them
// claims to carry the notation: an ISA triangle and a multivalued oval are
// named in words here, not encoded in a form another tool could draw. That is
// the whole difference between this and the relational exports, which cannot
// exist until there is a schema to generate them from.
enum class DocumentFormat { Pdf, Markdown, Html, Csv };

struct DocumentFormatInfo {
    DocumentFormat format;
    const char* label;
    const char* suffix;
    // Whether the listing carries the diagram itself. A page and a web page
    // can hold a picture; a Markdown table and a CSV row cannot hold one that
    // would survive being read where they are read.
    bool carries_diagram;
    const char* caution;
};
[[nodiscard]] const std::vector<DocumentFormatInfo>& document_formats();
[[nodiscard]] const DocumentFormatInfo& document_format(DocumentFormat format);

struct DocumentResult {
    bool ok = false;
    QString error;
    explicit operator bool() const { return ok; }
};

// Writes the project as a listing. The view is asked only for the diagram the
// page-shaped formats put at the top; the words all come from the project.
DocumentResult draw_document(ExportView view, const domain::Project& project,
                             DocumentFormat format, QByteArray& out);
DocumentResult write_document(ExportView view, const domain::Project& project,
                              DocumentFormat format, const QString& path);

} // namespace erdflow::desktop
