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
    Picture, Note, FullView, Dismiss, Symbols
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

} // namespace erdflow::desktop
