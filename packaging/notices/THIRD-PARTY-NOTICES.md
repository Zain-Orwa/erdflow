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

## Windows package: Microsoft Visual C++ runtime

The Windows package contains Microsoft Visual C++ runtime libraries
(`msvcp140*.dll`, `vcruntime140*.dll` and related files) beside
`erdflow.exe`. They are copyright Microsoft Corporation and are redistributed
subject to the Microsoft Software License Terms for the Visual Studio edition
used to build ERDFlow. Redistribution requires a valid licence and the files
must be eligible unmodified redistributables. Exact versions, the applicable
terms and the distributor's entitlement still require verification on Windows;
this is a release blocker. They are not covered by ERDFlow's MIT licence.
See <https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution>.

The installer and uninstaller use Inno Setup 6.7.1 by Jordan Russell and
Martijn Laan. Preserve its embedded copyright notices and website addresses.
The version-specific terms are at
<https://github.com/jrsoftware/issrc/blob/is-6_7_1/license.txt>.
The final installer engine and embedded compression/runtime components still
require an inventory and applicable notices review; staging erdflow.exe alone
does not cover them.

## Linux package

**ICU.** The Linux package contains the ICU libraries that come with Qt for
Linux, under the Unicode licence; its text is in `THIRD-PARTY-LICENSES.txt`.

**Operating-system libraries.** Some libraries are copied from the Ubuntu
system the package was built on. Each is listed in `THIRD-PARTY-LICENSES.txt`
with the Ubuntu package it came from and that package's copyright file.

**AppImage runtime.** An AppImage begins with a small program, the AppImage
runtime, that mounts and starts the application inside it. ERDFlow uses the
`type2-runtime` from the AppImage project, release `20251108`, under the MIT
licence (text in `THIRD-PARTY-LICENSES.txt`). According to its build files the
runtime is statically linked with libfuse 3.15.0 (with a patch from the
AppImage project), squashfuse 0.5.2, zstd, zlib, mimalloc and the musl C
library. Its source code and build files are at
<https://github.com/AppImage/type2-runtime/tree/20251108>. The notices for the
libraries inside the runtime are not yet complete. In particular, statically
linked LGPL code needs its own source/relinking review; the runtime's MIT
licence does not replace its dependencies' licences. This blocks release.

AppRun, hooks and other files added by linuxdeploy must also be audited after
the final AppImage is extracted. The native-binary scanner cannot identify
licensing of shell scripts or code statically embedded in an outer runtime.

## Status

ERDFlow packages built so far are unsigned test builds. They are not to be
distributed until the release checks in the project's
`docs/QT_LGPL_COMPLIANCE.md` are complete.
