// ERDFlow — settled work. Do not change, replace or re-style anything here to
// suit something new you have been asked to build. If what you are building
// genuinely contradicts what is here, stop and ask Zain, who owns this project:
// say what you want to change, what the application will LOOK like afterwards,
// and whether it is a gain or a loss. He decides. Fixing a real defect is not
// covered by this — fix it and say what was wrong. Full rule: CLAUDE.md.
#pragma once

#include <QByteArray>
#include <QString>

#include <array>
#include <optional>

namespace erdflow::desktop {

// Dr.icons: illustrations of what an entity commonly stands for -- a student,
// a course, an order -- put on the conceptual diagram as pictures, beside the
// entity they illustrate (Zain, 2026-10-14). They are pictures and nothing
// more: placing one changes no entity, and nothing matches them to entities by
// name. Once placed, a Dr.icon is an ordinary picture, its pixels kept in the
// project like any other, so a project carrying one opens anywhere.
//
// The key is the icon's lasting name and the name of its file; the label is
// what the gallery shows and what the placed picture is called.
struct DrIcon {
    const char* key;
    const char* label;
};

// All seventeen, in the order the gallery shows them.
[[nodiscard]] const std::array<DrIcon, 17>& dr_icons();

// The icon with this key, or nothing for a key that is not one of them.
[[nodiscard]] std::optional<DrIcon> dr_icon(const QString& key);

// Where the icon's picture is compiled in: a transparent PNG, 384 pixels
// square. The artwork is painted rather than drawn, so it is shipped as the
// pixels it is; an SVG wrapped round the same pixels would add nothing. Every
// placed icon keeps its own copy in the project, which a file limit of 8 MiB
// bounds, so the size is what stays sharp at an entity's height to 250% on a
// high-density screen, rather than at 300% for half as many icons.
[[nodiscard]] QString dr_icon_resource(const DrIcon& icon);

// The encoded PNG exactly as compiled in, ready to be kept in a project.
// Empty only if the resource is missing, which a build cannot produce.
[[nodiscard]] QByteArray dr_icon_bytes(const DrIcon& icon);

} // namespace erdflow::desktop
