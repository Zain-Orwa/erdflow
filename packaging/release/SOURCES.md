# Source code published with this ERDFlow release

ERDFlow's own source is at <https://github.com/Zain-Orwa/erdflow>, under the
MIT licence; the release's tag names the exact commit its packages were built
from. Beside the packages, each release publishes the source code of the
components in them whose licences call for it, unmodified:

| File | What it is | Why it is here |
| --- | --- | --- |
| `qtbase-everywhere-src-6.11.2.tar.xz` | Qt Base 6.11.2 (Qt Core, Gui, Widgets, DBus, XcbQpa, the platform, style and image plugins) | LGPL-3.0 |
| `qtsvg-everywhere-src-6.11.2.tar.xz` | Qt SVG 6.11.2 (Qt Svg, the SVG image and icon plugins) | LGPL-3.0 |
| `Qt-6.11.2-<platform>-build-information.tar.gz` | The SPDX SBOMs The Qt Company published with the Qt binaries in that platform's package: the exact source commits, configuration, compiler and linker | LGPL-3.0 (build information) |
| `type2-runtime-20251108.tar.gz` | The AppImage runtime at the start of the Linux AppImage, with the patch it applies to libfuse (`patches/libfuse/mount.c.diff`) and its build scripts | Rebuilding and relinking the runtime (LGPL-2.1 of libfuse) |
| `fuse-3.15.0.tar.xz` | libfuse 3.15.0, which the runtime is statically linked with | LGPL-2.1 |
| `squashfuse-0.5.2.tar.gz` | squashfuse 0.5.2, which the runtime is statically linked with | So the runtime can be rebuilt (BSD-2-Clause) |
| `ERDFlow-<version>-linux-x86_64-system-library-sources.tar` | The Ubuntu 24.04 source packages, at the exact versions in the AppImage, of every Ubuntu library it carries; `SOURCE-PACKAGES.txt` inside lists them | LGPL-2.1 and other licences of those libraries |

The Qt archives are the official ones from
<https://download.qt.io/official_releases/qt/6.11/6.11.2/submodules/>; their
SHA-256 sums match those published there and those in ERDFlow's
`packaging/notices/licenses/README.md`. The Qt libraries in ERDFlow's packages
are The Qt Company's official binaries of exactly this source, unmodified.
`SHA256SUMS-sources` lists the SHA-256 of every archive here.

How to replace the Qt libraries in a package, or to rebuild ERDFlow against
another Qt, is in `QT-LGPL-INFORMATION.md` inside each package.
