#!/bin/sh
# Regenerates ERDFlow's application icons from the owner's complete square
# artwork, erdflow-logo.png. Run on macOS from this folder:
#
#   sh make-icons.sh
#
# Needs macOS sips, iconutil and Python 3, without extra packages. Writes:
#   erdflow.icns    macOS application icon
#   erdflow.ico     Windows executable and installer icon
#   erdflow.png     Linux icon (512 x 512)
set -eu

here=$(cd "$(dirname "$0")" && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

python3 - "$here/erdflow-logo.png" <<'PY'
import struct, sys
with open(sys.argv[1], "rb") as source:
    header = source.read(24)
if header[:8] != b"\x89PNG\r\n\x1a\n" or len(header) != 24:
    sys.exit("The icon source must be a PNG.")
width, height = struct.unpack(">II", header[16:24])
if width != height or width < 1024:
    sys.exit("The icon source must be square and at least 1024 pixels, so the full logo stays intact.")
PY

# Resample the entire square source at every size. No viewport, crop or zoom:
# the outer circle, lettering and strands must all remain inside the icon.
render() {
    sips --resampleHeightWidth "$1" "$1" "$here/erdflow-logo.png" --out "$2" >/dev/null
}

mkdir "$work/erdflow.iconset"
for size in 16 32 128 256 512; do
    render "$size" "$work/erdflow.iconset/icon_${size}x${size}.png"
    render $((size * 2)) "$work/erdflow.iconset/icon_${size}x${size}@2x.png"
done
iconutil -c icns -o "$here/erdflow.icns" "$work/erdflow.iconset"

for size in 16 24 32 48 64 128 256; do
    render "$size" "$work/ico-$size.png"
done
python3 - "$work" "$here/erdflow.ico" <<'PY'
import struct, sys
work, out = sys.argv[1], sys.argv[2]
sizes = [16, 24, 32, 48, 64, 128, 256]
images = [open(f"{work}/ico-{s}.png", "rb").read() for s in sizes]
header = struct.pack("<HHH", 0, 1, len(images))
offset = len(header) + 16 * len(images)
entries = b""
for s, data in zip(sizes, images):
    # Match the PNG's RGB/RGBA channel count; this artwork is opaque RGB.
    bits = data[24] * {2: 3, 6: 4}[data[25]]
    entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, bits, len(data), offset)
    offset += len(data)
open(out, "wb").write(header + entries + b"".join(images))
PY

render 512 "$here/erdflow.png"
echo "Wrote erdflow.icns, erdflow.ico and erdflow.png in $here"
