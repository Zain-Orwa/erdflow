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
    Isa, Connect, Pan, Fit, Check, Duplicate, Rename, Delete, Theme
};

// Which of the two sets a glyph is taken from. The painted one follows the
// theme's colours and needs no files; the modern one is the 3D artwork, which
// carries its own colour and so looks the same under every theme. That is the
// trade, and it is the user's to make.
enum class IconMode { Normal, Modern };

[[nodiscard]] QString icon_mode_key(IconMode mode);
[[nodiscard]] IconMode icon_mode_from_key(const QString& key);
// The name this glyph goes by in the artwork, for the modern set.
[[nodiscard]] QString icon_name(Glyph glyph);

[[nodiscard]] QIcon glyph_icon(Glyph glyph, const Theme& colors, int size = 22,
                               IconMode mode = IconMode::Normal);

} // namespace erdflow::desktop
