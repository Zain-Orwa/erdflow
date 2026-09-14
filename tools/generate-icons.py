#!/usr/bin/env python3
"""Generates ERDFlow's modern icon set.

The set is flat and drawn rather than rendered: a thick rounded outline over a
pale wash of the same hue, on no plate at all. Each concept keeps one colour
wherever it appears, so a relationship is the same red in the toolbar, the tree
and the menus. Nothing here uses an SVG filter, because Qt's renderer handles
them unevenly and an icon that fails to draw is worse than a plain one.
"""
from pathlib import Path

ASSETS = Path(__file__).resolve().parent.parent / "assets"

# One colour per idea. Ink is the outline, wash the fill beneath it.
INK = {
    "navy":   "#22306B",
    "blue":   "#4F6EF7",
    "red":    "#D9503C",
    "purple": "#6B3FD4",
    "green":  "#2E6A45",
    "amber":  "#D9A441",
    "gold":   "#E0B33A",
    "slate":  "#40507F",
}
WASH = {
    "navy":   ("#EDF1FD", "#D7E1F8"),
    "blue":   ("#EEF2FF", "#DCE4FE"),
    "red":    ("#FDEDEA", "#F8D9D2"),
    "purple": ("#F2ECFC", "#E4D8F8"),
    "green":  ("#E9F4EC", "#D5E9DC"),
    "amber":  ("#FDF3DF", "#F9E4BC"),
    "gold":   ("#FDF1D5", "#F8E3AE"),
    "slate":  ("#EEF1F8", "#DCE2F1"),
}

# The same set again for dark themes. A flat icon has no plate to sit on, so a
# navy outline all but disappears on a dark toolbar; the hue is kept and the ink
# lifted instead, with the wash darkened to match.
def lift(hex_colour, amount):
    value = hex_colour.lstrip("#")
    channels = [int(value[i:i + 2], 16) for i in (0, 2, 4)]
    return "#" + "".join(f"{round(c + (255 - c) * amount):02x}" for c in channels)


def sink(hex_colour, amount):
    """The hue laid over the dark ground a dark theme's panels use."""
    ground = (0x16, 0x1E, 0x33)
    value = hex_colour.lstrip("#")
    channels = [int(value[i:i + 2], 16) for i in (0, 2, 4)]
    return "#" + "".join(f"{round(g + (c - g) * amount):02x}" for c, g in zip(channels, ground))


INK_DARK = {name: lift(colour, 0.62) for name, colour in INK.items()}
WASH_DARK = {name: (sink(colour, 0.30), sink(colour, 0.16)) for name, colour in INK.items()}

W = 7.0          # outline weight
WT = 5.0         # lighter weight, for detail inside a shape

on_dark = False


def svg(hue, body):
    light, deep = (WASH_DARK if on_dark else WASH)[hue]
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
  <defs>
    <linearGradient id="wash" x1="0" y1="0" x2="0.35" y2="1">
      <stop offset="0" stop-color="{light}"/>
      <stop offset="1" stop-color="{deep}"/>
    </linearGradient>
  </defs>
  <g fill="none" stroke="{(INK_DARK if on_dark else INK)[hue]}" stroke-width="{W}" stroke-linecap="round" stroke-linejoin="round">
{body}
  </g>
</svg>
"""


F = 'fill="url(#wash)"'
def ink(hue):
    return f'fill="{(INK_DARK if on_dark else INK)[hue]}" stroke="none"'


# The bodies name their colours as they are written, so the whole set is
# rebuilt once per variant rather than being drawn once and recoloured.
def build_all():
    G = {}

    G["select"] = ("blue", f'''
        <path d="M44 26 L44 96 L61 79 L72 102 L86 95 L75 73 L98 71 Z" {F}/>
        <path d="M104 30 L112 22 M108 50 L119 47 M88 16 L91 5" stroke-width="{WT}"/>''')

    G["pan"] = ("blue", f'''
    <path d="M40 76 L22 58 C16 52 22 42 30 46 L40 56
             L40 34 C40 26 51 26 51 34 L51 60 C51 66 57 66 57 60
             L57 22 C57 14 68 14 68 22 L68 60 C68 66 74 66 74 60
             L74 28 C74 20 85 20 85 28 L85 60 C85 66 91 66 91 60
             L91 42 C91 34 102 34 102 42 L102 82
             C102 100 88 114 68 114 C52 114 42 104 40 90 Z" {F}/>''')

    G["entity"] = ("navy", f'''
        <rect x="16" y="34" width="96" height="60" rx="12" {F}/>''')

    G["attribute"] = ("navy", f'''
        <ellipse cx="64" cy="64" rx="48" ry="32" {F}/>''')

    G["relationship"] = ("red", f'''
        <path d="M64 20 L110 64 L64 108 L18 64 Z" {F}/>''')

    G["isa"] = ("amber", f'''
        <path d="M64 18 L110 104 H18 Z" {F}/>''')

    G["connect"] = ("navy", f'''
        <path d="M38 90 L90 38"/>
        <circle cx="32" cy="96" r="12" {F}/>
        <circle cx="96" cy="32" r="12" {F}/>''')

    G["note"] = ("gold", f'''
        <path d="M24 18 H82 L104 40 V110 H24 Z" {F}/>
        <path d="M82 18 V40 H104"/>
        <path d="M42 62 H86 M42 80 H70" stroke-width="{WT}"/>''')

    G["comment"] = ("green", f'''
        <path d="M18 34 Q18 20 32 20 H96 Q110 20 110 34 V74 Q110 88 96 88 H56 L36 106 V88 H32 Q18 88 18 74 Z" {F}/>
        <circle cx="46" cy="54" r="6" {ink("green")}/>
        <circle cx="64" cy="54" r="6" {ink("green")}/>
        <circle cx="82" cy="54" r="6" {ink("green")}/>''')

    G["zoom"] = ("blue", f'''
        <circle cx="56" cy="56" r="34" {F}/>
        <path d="M81 81 L108 108"/>''')
    G["search"] = G["zoom"]

    G["new-project"] = ("navy", f'''
        <path d="M24 14 H76 L102 40 V114 H24 Z" {F}/>
        <path d="M76 14 V40 H102"/>
        <path d="M63 62 V92 M48 77 H78" stroke-width="{WT}"/>''')

    G["open"] = ("navy", f'''
        <path d="M14 100 V28 H48 L58 40 H100 V100 Z" {F}/>
        <path d="M14 100 L30 58 H118 L102 100 Z" {F}/>''')

    G["save"] = ("navy", f'''
        <path d="M18 18 H92 L110 36 V110 H18 Z" {F}/>
        <path d="M40 18 V50 H86 V18"/>
        <rect x="40" y="72" width="48" height="38" rx="5"/>''')

    G["import"] = ("navy", f'''
        <path d="M22 84 V108 H106 V84" {F}/>
        <path d="M64 18 V82 M40 58 L64 82 L88 58"/>''')

    G["export"] = ("navy", f'''
        <path d="M22 84 V108 H106 V84" {F}/>
        <path d="M64 82 V18 M40 42 L64 18 L88 42"/>''')

    G["undo"] = ("blue", f'''
    <path d="M95.9 85.6 A34 34 0 0 0 32.1 62.4" stroke-width="6"/>
    <path d="M24.5 83.0 L18.0 57.2 L46.1 67.5 Z" fill="{(INK_DARK if on_dark else INK)["blue"]}" stroke-width="2"/>''')

    G["redo"] = ("purple", f'''
    <path d="M32.1 85.6 A34 34 0 0 1 95.9 62.4" stroke-width="6"/>
    <path d="M103.5 83.0 L81.9 67.5 L110.0 57.2 Z" fill="{(INK_DARK if on_dark else INK)["purple"]}" stroke-width="2"/>''')

    G["duplicate"] = ("slate", f'''
        <rect x="16" y="16" width="66" height="66" rx="12" {F}/>
        <rect x="46" y="46" width="66" height="66" rx="12" {F}/>''')

    G["delete"] = ("red", f'''
        <path d="M30 34 H98 L90 112 H38 Z" {F}/>
        <path d="M18 34 H110 M48 20 H80"/>
        <path d="M52 52 V94 M76 52 V94" stroke-width="{WT}"/>''')

    G["conceptual"] = ("navy", f'''
        <rect x="44" y="12" width="40" height="28" rx="7" {F}/>
        <rect x="10" y="88" width="40" height="28" rx="7" {F}/>
        <rect x="78" y="88" width="40" height="28" rx="7" {F}/>
        <path d="M64 40 V64 M30 88 V64 H98 V88" stroke-width="{WT}"/>''')

    G["schema"] = ("navy", f'''
        <rect x="12" y="20" width="104" height="88" rx="12" {F}/>
        <path d="M12 48 H116 M46 48 V108 M82 48 V108 M12 78 H116" stroke-width="{WT}"/>''')

    G["physical"] = ("purple", f'''
        <rect x="14" y="16" width="100" height="26" rx="10" {F}/>
        <rect x="14" y="51" width="100" height="26" rx="10" {F}/>
        <rect x="14" y="86" width="100" height="26" rx="10" {F}/>
        <circle cx="96" cy="29" r="5" {ink("purple")}/>
        <circle cx="96" cy="64" r="5" {ink("purple")}/>
        <circle cx="96" cy="99" r="5" {ink("purple")}/>''')

    G["sql"] = ("blue", f'''
        <path d="M42 28 L12 64 L42 100 M86 28 L116 64 L86 100 M74 18 L54 110"/>''')

    G["data"] = ("purple", f'''
        <ellipse cx="64" cy="30" rx="44" ry="16" {F}/>
        <path d="M20 30 V98 C20 108 40 114 64 114 C88 114 108 108 108 98 V30" {F}/>
        <path d="M20 64 C20 74 40 80 64 80 C88 80 108 74 108 64" stroke-width="{WT}"/>''')

    G["explorer"] = ("navy", f'''
        <path d="M12 26 H50 L60 38 H116 V56 H12 Z" {F}/>
        <path d="M34 58 V76 M34 76 H64 M34 76 H94 M64 76 V90 M94 76 V90" stroke-width="{WT}"/>
        <rect x="48" y="90" width="32" height="24" rx="6" {F}/>
        <rect x="84" y="90" width="32" height="24" rx="6" {F}/>''')

    G["properties"] = ("slate", f'''
        <path d="M14 32 H114 M14 64 H114 M14 96 H114" stroke-width="{WT}"/>
        <circle cx="48" cy="32" r="12" {F}/>
        <circle cx="86" cy="64" r="12" {F}/>
        <circle cx="38" cy="96" r="12" {F}/>''')

    G["validate"] = ("green", f'''
        <circle cx="64" cy="64" r="48" {F}/>
        <path d="M42 66 L58 82 L88 46"/>''')

    G["theme"] = ("slate", f'''
        <circle cx="64" cy="64" r="40" {F}/>
        <path d="M64 24 A40 40 0 0 1 64 104 Z" fill="{INK['slate']}" stroke="none"/>
        <path d="M64 12 V4 M64 124 V116 M12 64 H4 M124 64 H116" stroke-width="{WT}"/>''')

    G["settings"] = ("slate", f'''
        <path d="M64 12 L76 19 L90 15 L99 28 L94 41 L102 53 L116 57 V71 L102 75 L94 87
                 L99 100 L90 113 L76 109 L64 116 L52 109 L38 113 L29 100 L34 87 L26 75
                 L12 71 V57 L26 53 L34 41 L29 28 L38 15 L52 19 Z" {F}/>
        <circle cx="64" cy="64" r="17"/>''')
    return G


ORDER = ["select","entity","attribute","relationship","isa","connect","note","comment","pan","zoom",
         "new-project","open","save","import","export","undo","redo","duplicate","delete","search",
         "conceptual","schema","physical","sql","data","explorer","properties","validate","theme","settings"]

for folder, dark in (("icons", False), ("icons-on-dark", True)):
    on_dark = dark
    # The glyph bodies name their colours as they are built, so they are rebuilt
    # rather than reused once the palette beneath them has changed.
    target = ASSETS / folder
    target.mkdir(parents=True, exist_ok=True)
    built = build_all()
    for name in ORDER:
        hue, body = built[name]
        (target / f"{name}.svg").write_text(svg(hue, body), encoding="utf-8")
    print(f"wrote {len(ORDER)} icons to {target}")
