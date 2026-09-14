#pragma once

#include "theme.hpp"

#include <QIcon>

namespace erdflow::desktop {

// Toolbar and menu glyphs. They are drawn rather than loaded so they follow the
// active theme's colours, need no asset pipeline, and stay sharp at any scale.
// The modelling tools reuse their own Chen shapes, so the button for an entity
// looks like the entity it places.
enum class Glyph {
    New, Open, Save, Undo, Redo, Select, Entity, Attribute, Relationship,
    Isa, Connect, Pan, Fit, Check, Duplicate, Rename, Delete
};

[[nodiscard]] QIcon glyph_icon(Glyph glyph, const Theme& colors, int size = 22);

} // namespace erdflow::desktop
