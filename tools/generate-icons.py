from pathlib import Path
icons_dir = Path("/Users/zain/Developer/erdflow/assets/icons")
icons_dir.mkdir(parents=True, exist_ok=True)

palettes = {
    "blue": ("#62E8FF", "#0A84FF", "#2C45D9"),
    "purple": ("#D19CFF", "#8D4BFF", "#4934CC"),
    "cyan": ("#52F0F0", "#08BEE8", "#1475E8"),
    "green": ("#7AF7B5", "#23D49A", "#0D8F89"),
    "orange": ("#FFD56A", "#FF9F38", "#FF6B31"),
}

def defs(c1,c2,c3):
    return f"""
    <defs>
      <linearGradient id="bg" x1="0.12" y1="0" x2="0.9" y2="1">
        <stop offset="0" stop-color="{c1}"/>
        <stop offset="0.48" stop-color="{c2}"/>
        <stop offset="1" stop-color="{c3}"/>
      </linearGradient>
      <!-- The gloss across the top, fading out rather than cut off, which is
           what a blur would have done if filters were safe to rely on. -->
      <linearGradient id="gloss" x1="0" y1="0" x2="0" y2="1">
        <stop offset="0" stop-color="#FFFFFF" stop-opacity="0.72"/>
        <stop offset="0.45" stop-color="#FFFFFF" stop-opacity="0.22"/>
        <stop offset="1" stop-color="#FFFFFF" stop-opacity="0"/>
      </linearGradient>
      <!-- Light bouncing back up off the bottom of the plate. -->
      <linearGradient id="bounce" x1="0" y1="1" x2="0" y2="0">
        <stop offset="0" stop-color="#FFFFFF" stop-opacity="0.34"/>
        <stop offset="1" stop-color="#FFFFFF" stop-opacity="0"/>
      </linearGradient>
      <radialGradient id="shine" cx="0.3" cy="0.16" r="0.62">
        <stop offset="0" stop-color="#FFFFFF" stop-opacity="0.75"/>
        <stop offset="0.5" stop-color="#FFFFFF" stop-opacity="0.16"/>
        <stop offset="1" stop-color="#FFFFFF" stop-opacity="0"/>
      </radialGradient>
      <!-- The glyph is glass rather than paint: lit from above, denser below. -->
      <linearGradient id="glass" x1="0.2" y1="0" x2="0.7" y2="1">
        <stop offset="0" stop-color="#FFFFFF" stop-opacity="1"/>
        <stop offset="0.42" stop-color="#F2F7FF" stop-opacity="0.99"/>
        <stop offset="1" stop-color="#C8DAF5" stop-opacity="0.97"/>
      </linearGradient>
      <linearGradient id="glass2" x1="0" y1="0" x2="1" y2="1">
        <stop offset="0" stop-color="#FFFFFF" stop-opacity="0.75"/>
        <stop offset="1" stop-color="#FFFFFF" stop-opacity="0.18"/>
      </linearGradient>
      <radialGradient id="rim" cx="0.5" cy="0.5" r="0.5">
        <stop offset="0.82" stop-color="#FFFFFF" stop-opacity="0"/>
        <stop offset="1" stop-color="#FFFFFF" stop-opacity="0.5"/>
      </radialGradient>
    </defs>
    """

def frame(c1,c2,c3, glyph):
    return f"""<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
{defs(c1,c2,c3)}
  <!-- Contact shadow, stacked rather than blurred so no filter is needed. -->
  <rect x="17" y="21" width="94" height="94" rx="27" fill="#0E1E4A" opacity="0.10"/>
  <rect x="15" y="19" width="98" height="98" rx="28" fill="#0E1E4A" opacity="0.10"/>
  <!-- The plate: vivid, rounded almost to a squircle, as the artwork is. -->
  <rect x="12" y="10" width="104" height="104" rx="31" fill="url(#bg)"/>
  <!-- A bright rim all round, brightest along the top edge. -->
  <rect x="12" y="10" width="104" height="104" rx="31" fill="url(#rim)"/>
  <rect x="13.2" y="11.2" width="101.6" height="101.6" rx="30" fill="none"
        stroke="#FFFFFF" stroke-opacity="0.55" stroke-width="1.6"/>
  <!-- Gloss over the upper half, bounce light off the lower. -->
  <rect x="12" y="10" width="104" height="58" rx="30" fill="url(#gloss)"/>
  <rect x="20" y="82" width="88" height="30" rx="15" fill="url(#bounce)"/>
  <ellipse cx="50" cy="34" rx="30" ry="21" fill="url(#shine)"/>
  {glyph}
</svg>
"""

S = 'fill="url(#glass)" stroke="#FFFFFF" stroke-opacity="0.9" stroke-width="2.4"'
LINE = 'fill="none" stroke="#FFFFFF" stroke-opacity="0.94" stroke-width="7" stroke-linecap="round" stroke-linejoin="round"'
LINE_THIN = 'fill="none" stroke="#FFFFFF" stroke-opacity="0.94" stroke-width="5" stroke-linecap="round" stroke-linejoin="round"'

g = {}
g["select"] = f'''
  <path d="M39 31 L86 63 L66 69 L78 91 L66 98 L54 75 L40 91 Z" fill="#1A3A8A" opacity="0.22" transform="translate(3,5)"/>
  <path d="M39 31 L86 63 L66 69 L78 91 L66 98 L54 75 L40 91 Z" {S}/>
'''
g["entity"] = f'''
  <rect x="31" y="39" width="66" height="50" rx="8" fill="#173B85" opacity="0.20" transform="translate(2,5)"/>
  <rect x="31" y="39" width="66" height="50" rx="8" {S}/>
'''
g["attribute"] = f'''
  <ellipse cx="64" cy="65" rx="35" ry="24" fill="#2E2A88" opacity="0.18" transform="translate(2,5)"/>
  <ellipse cx="64" cy="65" rx="35" ry="24" {S}/>
'''
g["relationship"] = f'''
  <path d="M64 31 L98 64 L64 97 L30 64 Z" fill="#173B85" opacity="0.20" transform="translate(2,5)"/>
  <path d="M64 31 L98 64 L64 97 L30 64 Z" {S}/>
'''
g["isa"] = f'''
  <path d="M64 28 L88 57 H40 Z" {S}/>
  <path d="M64 57 V72 M64 72 H38 M64 72 H90 M38 72 V88 M64 72 V88 M90 72 V88" {LINE_THIN}/>
  <rect x="29" y="86" width="18" height="15" rx="4" {S}/>
  <rect x="55" y="86" width="18" height="15" rx="4" {S}/>
  <rect x="81" y="86" width="18" height="15" rx="4" {S}/>
'''
g["connect"] = f'''
  <rect x="25" y="34" width="35" height="35" rx="9" {S}/>
  <rect x="69" y="64" width="35" height="35" rx="9" {S}/>
  <path d="M55 59 C67 56 72 61 76 68" {LINE_THIN}/>
  <circle cx="58" cy="58" r="6" {S}/>
  <circle cx="72" cy="67" r="6" {S}/>
'''
g["note"] = f'''
  <path d="M35 28 H79 L95 44 V100 H35 Z" {S}/>
  <path d="M79 28 V45 H95" fill="#FFD56A" stroke="#FFFFFF" stroke-opacity="0.75" stroke-width="2"/>
  <path d="M47 57 H76 M47 70 H84 M47 83 H73" stroke="#5D75A8" stroke-width="5" stroke-linecap="round"/>
'''
g["comment"] = f'''
  <path d="M27 38 Q27 28 37 28 H91 Q101 28 101 38 V74 Q101 84 91 84 H57 L43 97 V84 H37 Q27 84 27 74 Z" {S}/>
  <circle cx="49" cy="57" r="5" fill="#0A84FF"/>
  <circle cx="64" cy="57" r="5" fill="#0A84FF"/>
  <circle cx="79" cy="57" r="5" fill="#0A84FF"/>
'''
g["pan"] = f'''
  <path d="M42 70 V48 C42 43 49 43 49 48 V63 V38 C49 33 56 33 56 38 V62 V34 C56 29 63 29 63 34 V62 V40 C63 35 70 35 70 40 V65 V46 C70 41 77 41 77 46 V70 C77 89 68 99 55 99 C42 99 34 89 34 77 C34 68 42 66 42 70 Z" {S}/>
'''
g["zoom"] = f'''
  <circle cx="56" cy="56" r="28" fill="none" stroke="#FFFFFF" stroke-opacity="0.96" stroke-width="9"/>
  <circle cx="56" cy="56" r="21" fill="#7CB7FF" opacity="0.35"/>
  <path d="M76 76 L96 96" {LINE}/>
'''
g["new-project"] = f'''
  <path d="M37 27 H77 L92 42 V92 H37 Z" {S}/>
  <path d="M77 27 V43 H92" fill="#D8E6FF" stroke="#FFFFFF" stroke-width="2"/>
  <circle cx="82" cy="84" r="18" fill="#D5EEFF" stroke="#FFFFFF" stroke-width="2"/>
  <path d="M82 75 V93 M73 84 H91" stroke="#2B73E0" stroke-width="6" stroke-linecap="round"/>
'''
g["open"] = f'''
  <path d="M26 46 H51 L59 54 H102 L92 95 H25 Z" {S}/>
  <path d="M30 43 V34 H59 L66 42 H95 V53" {LINE_THIN}/>
'''
g["save"] = f'''
  <path d="M31 27 H91 L98 34 V99 H31 Z" {S}/>
  <rect x="45" y="30" width="37" height="21" rx="4" fill="#6F79D8" opacity="0.75"/>
  <rect x="45" y="67" width="39" height="24" rx="5" fill="#D7EDFF" stroke="#FFFFFF" stroke-width="2"/>
'''
g["import"] = f'''
  <path d="M31 72 H97 V94 H31 Z" {S}/>
  <path d="M64 29 V72 M48 56 L64 72 L80 56" {LINE}/>
'''
g["export"] = f'''
  <path d="M31 72 H97 V94 H31 Z" {S}/>
  <path d="M64 73 V30 M48 46 L64 30 L80 46" {LINE}/>
'''
g["undo"] = f'''
  <path d="M46 39 L28 54 L46 69" {LINE}/>
  <path d="M33 54 H68 C85 54 95 66 95 80 C95 91 90 98 84 103" {LINE}/>
'''
g["redo"] = f'''
  <path d="M82 39 L100 54 L82 69" {LINE}/>
  <path d="M95 54 H60 C43 54 33 66 33 80 C33 91 38 98 44 103" {LINE}/>
'''
g["duplicate"] = f'''
  <rect x="34" y="31" width="48" height="48" rx="9" {S}/>
  <rect x="50" y="48" width="48" height="48" rx="9" {S}/>
'''
g["delete"] = f'''
  <path d="M43 42 H85 L82 95 H46 Z" {S}/>
  <path d="M38 42 H90 M52 33 H76" {LINE_THIN}/>
  <path d="M55 54 V83 M65 54 V83 M75 54 V83" stroke="#6D83AA" stroke-width="4" stroke-linecap="round"/>
'''
g["search"] = g["zoom"]
g["conceptual"] = f'''
  <rect x="47" y="27" width="34" height="23" rx="6" {S}/>
  <rect x="24" y="78" width="31" height="23" rx="6" {S}/>
  <rect x="73" y="78" width="31" height="23" rx="6" {S}/>
  <path d="M64 50 V65 M64 65 H39 V78 M64 65 H89 V78" {LINE_THIN}/>
'''
g["schema"] = f'''
  <rect x="28" y="30" width="72" height="67" rx="10" {S}/>
  <path d="M28 49 H100 M52 49 V97 M76 49 V97 M28 72 H100" stroke="#6B82AB" stroke-width="4"/>
'''
g["physical"] = f'''
  <rect x="30" y="29" width="68" height="18" rx="7" {S}/>
  <rect x="30" y="54" width="68" height="18" rx="7" {S}/>
  <rect x="30" y="79" width="68" height="18" rx="7" {S}/>
  <circle cx="87" cy="38" r="4" fill="#0A84FF"/>
  <circle cx="87" cy="63" r="4" fill="#0A84FF"/>
  <circle cx="87" cy="88" r="4" fill="#0A84FF"/>
'''
g["sql"] = f'''
  <path d="M49 39 L29 64 L49 89 M79 39 L99 64 L79 89 M69 33 L58 95" {LINE}/>
'''
g["data"] = f'''
  <ellipse cx="64" cy="38" rx="29" ry="12" {S}/>
  <path d="M35 38 V85 C35 92 48 98 64 98 C80 98 93 92 93 85 V38" fill="url(#glass)" stroke="#FFFFFF" stroke-width="2"/>
  <path d="M35 60 C35 67 48 72 64 72 C80 72 93 67 93 60 M35 81 C35 88 48 93 64 93 C80 93 93 88 93 81" fill="none" stroke="#6F7ED5" stroke-width="3"/>
'''
g["explorer"] = f'''
  <path d="M30 38 H55 L61 45 H98 V58 H30 Z" {S}/>
  <path d="M43 59 V72 M43 72 H64 M43 72 H86 M64 72 V88 M86 72 V88" {LINE_THIN}/>
  <rect x="54" y="85" width="20" height="16" rx="4" {S}/>
  <rect x="76" y="85" width="20" height="16" rx="4" {S}/>
'''
g["properties"] = f'''
  <path d="M31 40 H97 M31 64 H97 M31 88 H97" {LINE_THIN}/>
  <circle cx="55" cy="40" r="8" {S}/>
  <circle cx="80" cy="64" r="8" {S}/>
  <circle cx="47" cy="88" r="8" {S}/>
'''
g["validate"] = f'''
  <circle cx="64" cy="64" r="37" {S}/>
  <path d="M44 65 L58 79 L86 48" fill="none" stroke="#FFFFFF" stroke-width="9" stroke-linecap="round" stroke-linejoin="round"/>
'''
g["theme"] = f'''
  <path d="M64 31 A33 33 0 0 0 64 97 V31 Z" fill="#FFFFFF" opacity="0.92"/>
  <path d="M64 31 A33 33 0 0 1 64 97 C81 91 92 79 97 64 C92 49 81 37 64 31 Z" fill="#D9E7FF" opacity="0.92"/>
  <path d="M64 31 V97" stroke="#FFFFFF" stroke-width="4"/>
  <path d="M36 64 H27 M101 64 H92 M64 27 V20 M64 108 V101" stroke="#FFFFFF" stroke-width="4" stroke-linecap="round"/>
'''
g["settings"] = f'''
  <path d="M64 29 L72 33 L81 30 L88 37 L85 46 L90 54 L99 57 V67 L90 71 L85 79 L88 88 L81 95 L72 92 L64 99 L56 92 L47 95 L40 88 L43 79 L38 71 L29 67 V57 L38 54 L43 46 L40 37 L47 30 L56 33 Z" {S}/>
  <circle cx="64" cy="64" r="13" fill="#5876C9" opacity="0.70" stroke="#FFFFFF" stroke-width="3"/>
'''

icon_order = ["select","entity","attribute","relationship","isa",
    "connect","note","comment","pan","zoom",
    "new-project","open","save","import","export",
    "undo","redo","duplicate","delete","search",
    "conceptual","schema","physical","sql","data",
    "explorer","properties","validate","theme","settings"]
palette_cycle = ["cyan","blue","purple","cyan","purple",
                 "cyan","orange","green","blue","purple",
                 "cyan","blue","purple","cyan","purple",
                 "cyan","blue","purple","cyan","purple",
                 "cyan","blue","purple","cyan","purple",
                 "cyan","blue","purple","cyan","purple"]
for name, pal in zip(icon_order, palette_cycle):
    c1,c2,c3 = palettes[pal]
    (icons_dir / f"{name}.svg").write_text(frame(c1,c2,c3,g[name]), encoding="utf-8")
print("wrote", len(icon_order), "icons")
