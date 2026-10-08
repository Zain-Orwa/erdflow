// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include "theme.hpp"

#include <QIcon>
#include <QString>

namespace erdflow::desktop {

// Toolbar and menu glyphs. They are drawn rather than loaded so they follow the
// active theme's colours, need no asset pipeline, and stay sharp at any scale.
// The modelling tools reuse their own Chen shapes, so the button for an entity
// looks like the entity it places.
enum class Glyph {
    New, Open, Save, Undo, Redo, Select, Entity, Attribute, Relationship,
    Isa, Connect, Pan, Fit, Check, Duplicate, Rename, Delete, Theme,
    Picture, Note, FullView, Dismiss, Symbols, Export, Search, Key,
    // A table, the element a Relational Design is drawn with (Zain,
    // 2026-09-27), as Entity is the Conceptual design's.
    Table,
    // The schema's Arrange and Appearance menus, in the header of a schema
    // drawn by hand (Zain, 2026-09-27).
    Arrange, Appearance,
    // The Schema Explorer's Relationships group, distinct from the
    // Conceptual relationship diamond and the Connect command.
    SchemaRelationships,
    // The canvas raft's side-panel buttons (Zain, 2026-10-03): the Explorer
    // on the left, Properties on the right, and both together.
    ExplorerPanel, PropertiesPanel, SidePanels,
    // The ribbon's tabs and the commands on its Design, Export, Import, View
    // and Help rows (Zain, 2026-10-06). Drawn as line art in every set: the
    // painted set has no drawing of its own for them, and the coloured set
    // only where it has artwork by the same name (line_art_only).
    Import, View, Help,
    Background, IconSet, Notation, Lines,
    ProjectFile, PdfDocument, DataDictionary, HtmlReport, CsvListing,
    SvgPicture, PdfPage, MorePictures, CopyPicture,
    ProjectPicture, OtherTool,
    ActualSize, ZoomIn, ZoomOut, Grid, AlignToGrid, CanvasControls, Comments, History,
    Guide, About,
    // The remaining tabs, Home's Insert, and the canvas raft's one panel
    // button when neither panel is showing (Zain, 2026-10-06).
    FileTab, Home, Insert, Settings, NoPanels,
    // The header's Model menu: connected entities, the Home screen's own mark
    // for the conceptual model (Zain, 2026-10-07).
    Model
};

// Which set a glyph is taken from. Painted follows the theme's colours and
// needs no files. Modern is the coloured artwork, which carries its own colour
// and so looks the same under every theme. Outline is line art -- Lucide for
// the ordinary commands, ERDFlow's own for the database shapes Lucide has no
// icon for -- drawn in one stroke weight and inked from the theme, so it is
// both quiet and part of whatever palette is on. That is the trade, and it is
// the user's to make.
enum class IconMode { Normal, Modern, Outline };

[[nodiscard]] QString icon_mode_key(IconMode mode);
[[nodiscard]] IconMode icon_mode_from_key(const QString& key);
// The name this glyph goes by in the artwork, for the modern set.
[[nodiscard]] QString icon_name(Glyph glyph);

[[nodiscard]] QIcon glyph_icon(Glyph glyph, const Theme& colors, int size = 22,
                               IconMode mode = IconMode::Normal);

// One named drawing from the line-art set, inked in one colour. For the places
// that are not commands -- the Home screen's sidebar, its learning panel, its
// bar -- and so have no Glyph of their own. The name is the file's, without
// its extension. An empty pixmap means there is no such drawing; the caller
// decides what stands in, since a row with words beside it can do without.
[[nodiscard]] QPixmap outline_pixmap(const QString& name, const QColor& ink, int size);
// The primary key's mark (Zain, 2026-09-26): a polished golden key held bow up
// and pointing down, drawn from its own vector file in assets/marks rather than
// from an icon set, so it is the same key whichever set is chosen. Made at the
// given size and pixel ratio; greyed, keeping its shading, for a theme with no
// colour. Drawn beside PK on the schema's key columns, and kept here so a
// table can wear the very same key later.
[[nodiscard]] QPixmap primary_key_mark(int size, qreal ratio, bool greyed);
// The same line art with its closed shapes filled in the ink too, for a mark
// that has to read as solid at a small size -- the spark on Create with AI.
// Its open strokes are drawn as they are.
[[nodiscard]] QPixmap solid_pixmap(const QString& name, const QColor& ink, int size);

} // namespace erdflow::desktop
