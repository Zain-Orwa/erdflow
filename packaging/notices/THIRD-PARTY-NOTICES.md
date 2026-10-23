# ERDFlow third-party notices

This file travels with every ERDFlow package, beside:

- `LICENSE`: ERDFlow's own licence;
- `THIRD-PARTY-LICENSES.txt`: generated when the package is built, from the
  programs and libraries the package actually contains. It lists each Qt
  library and plugin, the third-party code compiled into them, every other
  identified native library in the staging folder, and available licence texts.
  Its inventory is preparation evidence, not a completed redistribution audit;
- `QT-LGPL-INFORMATION.md`: how ERDFlow uses Qt under the LGPL, where the
  Qt source code is, and how to replace the Qt libraries;
- `LGPL-3.0.txt` and `GPL-3.0.txt`: the GNU licence texts;
- `LICENSE-lucide.txt`: the licence of the Lucide and Feather icons.

## ERDFlow

ERDFlow is distributed under the MIT licence; see `LICENSE`.

## Qt

ERDFlow uses the Qt libraries (Qt Core, Qt Gui, Qt Widgets and Qt SVG, and
the Qt libraries and plugins those need) from The Qt Company Ltd. and other
contributors, under the GNU Lesser General Public License version 3. ERDFlow
links to them dynamically. They are the official Qt binaries, with their code
unmodified, kept as separate files in the package so that they can be replaced;
see `QT-LGPL-INFORMATION.md`.

The Qt libraries contain third-party code under its own licences, such as
FreeType, HarfBuzz, PCRE2, libpng, libjpeg-turbo, zlib and Unicode data. Each
one present in this package, with its version, licence and copyright notice, is
listed in `THIRD-PARTY-LICENSES.txt`, as recorded in the Software Bill of
Materials that The Qt Company publishes with those binaries.

## Lucide and Feather icons

Part of ERDFlow's icon set comes from Lucide, under the ISC licence, and some
of those icons derive from Feather, under the MIT licence; see
`LICENSE-lucide.txt`.

## Monocypher

ERDFlow checks the signature on an update before putting it in place with
Monocypher 4.0.2 (<https://monocypher.org>), by Loup Vaillant, Michael Savage,
Fabio Scotoni and contributors, compiled into ERDFlow unmodified. Monocypher
is dual-licensed under CC0-1.0 and the BSD 2-Clause licence; ERDFlow uses it
under CC0-1.0, which dedicates it to the public domain. Its source, and how
it was taken and checked, are in ERDFlow's source, under
`third_party/monocypher`.

## Windows package: Microsoft Visual C++ runtime

The Windows package contains Microsoft Visual C++ runtime libraries
(`msvcp140*.dll`, `vcruntime140*.dll`, `concrt140.dll`) beside `erdflow.exe`,
from the Microsoft Visual C++ 2015-2022 Redistributable that came with the
Visual Studio 2022 used to build ERDFlow. They are copyright Microsoft
Corporation, unmodified, and redistributed as Distributable Code under the
Microsoft Software License Terms for Visual Studio 2022 (see
<https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution>).
They are not covered by ERDFlow's MIT licence. Microsoft's terms for them
apply: they may be used only as part of ERDFlow, and may not be modified, or
reverse engineered, decompiled or disassembled except where the law allows it.
These terms concern Microsoft's files only and do not limit your rights under
the LGPL to modify the Qt libraries and reverse engineer for debugging such
modifications. Their exact versions are listed in `THIRD-PARTY-LICENSES.txt`
and in the package's inventory.

The installer and the uninstaller beside `erdflow.exe` (`unins000.exe`) are
made with Inno Setup 6.7.1, copyright Jordan Russell and Martijn Laan, under
the Inno Setup License (`Inno-Setup-6.7.1-license.txt`). Their copyright
notices and web site addresses are kept as Inno Setup places them.

## Linux package

**ICU.** The Linux package contains the ICU libraries that come with Qt for
Linux, under the Unicode licence; its text is in `THIRD-PARTY-LICENSES.txt`.

**Operating-system libraries.** Some libraries are copied, unmodified, from
the Ubuntu 24.04 system the package was built on. Each is listed in
`THIRD-PARTY-LICENSES.txt` with the Ubuntu package and version it came from
and that package's copyright file. Some of them (GLib, libgcrypt and
libsystemd among them) are under the GNU Lesser General Public License version
2.1; the source packages of all of them, at the exact versions in this
AppImage, are published with each ERDFlow release on its GitHub Releases page.

**AppImage runtime.** An AppImage begins with a small program, the AppImage
runtime, that mounts and starts the application inside it. ERDFlow uses the
`type2-runtime` from the AppImage project, release `20251108`, under the MIT
licence (text in `THIRD-PARTY-LICENSES.txt`). It is statically linked with:

| Component | Version | Licence | Text in `appimage-runtime/` |
| --- | --- | --- | --- |
| libfuse, with the AppImage project's patch to `mount.c` | 3.15.0 | LGPL-2.1 (the library) | `libfuse-3.15.0-LGPL2.txt` |
| squashfuse | 0.5.2 | BSD-2-Clause | `squashfuse-0.5.2-LICENSE.txt` |
| zstd | 1.5.6 | BSD-3-Clause | `zstd-1.5.6-LICENSE.txt` |
| zlib | 1.3.1 | Zlib | `zlib-1.3.1-LICENSE.txt` |
| mimalloc | 2, as packaged by Alpine Linux 3.21 | MIT | `mimalloc-2.1.7-LICENSE.txt` |
| musl C library | 1.2.5 | MIT | `musl-1.2.5-COPYRIGHT.txt` |

The complete source of the runtime, of libfuse 3.15.0 with the patch, and of
squashfuse 0.5.2, from which the runtime can be rebuilt and relinked with a
modified libfuse, is published with each ERDFlow release on its GitHub
Releases page. The runtime's own source and build files are also at
<https://github.com/AppImage/type2-runtime/tree/20251108>.

**Added by linuxdeploy.** `AppRun` starts `usr/bin/erdflow`, and
`apprun-hooks/linuxdeploy-plugin-qt-hook.sh` is a start-up script from
linuxdeploy-plugin-qt, under the MIT licence
(`appimage-runtime/linuxdeploy-plugin-qt-LICENSE.txt`). `usr/bin/qt.conf`
tells Qt where its plugins are. Nothing else is added.

## Status

Unless `THIRD-PARTY-LICENSES.txt` says this package was approved for
distribution by ERDFlow's release owner, it is a test build and is not to be
distributed (`docs/QT_LGPL_COMPLIANCE.md` in ERDFlow's source).
