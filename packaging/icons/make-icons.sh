#!/bin/sh
# Regenerates ERDFlow's application icons from erdflow-icon.svg, the ERDFlow
# mark used as the website's favicon. Run on macOS from this folder:
#
#   sh make-icons.sh
#
# Needs Google Chrome (to draw the SVG at each size with a transparent
# background), iconutil and Python 3, all without extra packages. Writes:
#   erdflow.icns    macOS application icon
#   erdflow.ico     Windows executable and installer icon
#   erdflow.png     Linux icon (512 x 512)
set -eu

here=$(cd "$(dirname "$0")" && pwd)
chrome="/Applications/Google Chrome.app/Contents/MacOS/Google Chrome"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT

cp "$here/erdflow-icon.svg" "$work/icon.svg"
cat > "$work/icon.html" <<'HTML'
<!doctype html><html><body style="margin:0;background:transparent">
<img src="icon.svg" style="display:block;width:100vw;height:100vh">
</body></html>
HTML

# Draws the mark at one size, straight from the vector, so small sizes stay sharp.
render() {
    "$chrome" --headless --disable-gpu --hide-scrollbars --default-background-color=00000000 \
        --window-size="$1,$1" --screenshot="$2" "file://$work/icon.html" >/dev/null 2>&1
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
    entries += struct.pack("<BBBBHHII", s % 256, s % 256, 0, 0, 1, 32, len(data), offset)
    offset += len(data)
open(out, "wb").write(header + entries + b"".join(images))
PY

render 512 "$here/erdflow.png"
echo "Wrote erdflow.icns, erdflow.ico and erdflow.png in $here"
