# Packaging

How ERDFlow is turned into installable packages. Ordinary development builds
are unaffected: everything here is switched on only by
`-DERDFLOW_PACKAGING=ON`, which the package workflow sets.

| Platform              | Package                                      | Tools                                  |
|-----------------------|----------------------------------------------|----------------------------------------|
| Windows x86-64        | `ERDFlow-<version>-windows-x86_64-setup.exe` | windeployqt, Inno Setup 6.7.1          |
| macOS Apple silicon   | `ERDFlow-<version>-macos-arm64.dmg`          | macdeployqt, CPack DragNDrop           |
| macOS Intel           | `ERDFlow-<version>-macos-x86_64.dmg`         | macdeployqt, CPack DragNDrop           |
| Linux x86-64          | `ERDFlow-<version>-linux-x86_64.AppImage`    | linuxdeploy, linuxdeploy-plugin-qt     |

Qt stays dynamically linked. Each package carries the shared Qt libraries and
plugins ERDFlow uses, the notices in `notices/`, and nothing from the test
suite.

**Status: unsigned test candidates.** No package is signed with a Developer ID
or a Windows code-signing certificate, the macOS packages are not notarized,
and redistribution obligations remain open (`notices/THIRD-PARTY-NOTICES.md`).
None of them is ready to publish.

## Layout

```
packaging/
  packaging.cmake            included from CMakeLists.txt when ERDFLOW_PACKAGING is ON
  icons/
    erdflow-icon.svg         source artwork (the erdflow.app mark)
    make-icons.sh            regenerates the three files below from it
    erdflow.icns             macOS
    erdflow.ico              Windows
    erdflow.png              Linux (512 px)
  macos/
    Info.plist.in            ERDFlow.app's Info.plist
    finish-bundle.cmake      runs after macdeployqt; see below
  windows/
    erdflow.rc.in            icon and version details compiled into erdflow.exe
    erdflow.iss.in           the Inno Setup script
  linux/
    erdflow.desktop          desktop entry for the AppImage
  notices/
    THIRD-PARTY-NOTICES.md   ERDFlow, Qt, Lucide, Visual C++ runtime
    QT-LGPL-INFORMATION.md.in source/replacement instructions
    package_notices.py      inventory and licence generator
    test_package_notices.py generator regression checks
    write-notices.cmake.in  runs after dependency deployment
    licenses/              verbatim upstream licence texts and provenance
    LGPL-3.0.txt, GPL-3.0.txt
```

The package workflow is `.github/workflows/package.yml`. It runs only when
started by hand from the Actions tab. It builds and tests on each platform,
packages, checks the package, installs or launches it, and uploads it with its
SHA-256 checksum as a workflow artifact. A last job checks the four checksums
and writes `SHA256SUMS`. It creates no release and no tag, and publishes
nothing.

## What `ERDFLOW_PACKAGING` changes

- **macOS:** the program is built as `ERDFlow.app` (executable `ERDFlow`),
  with its icon, bundle identifier `app.erdflow.ERDFlow`, version, copyright
  and minimum macOS version in `Info.plist`.
- **Windows:** `erdflow.exe` is a windowed program (no console) with its icon
  and version details.
- **All:** `cmake --install` installs the program and the notices, and on
  macOS and Windows runs Qt's deployment tool.

The tests run against the packaged build too, in the workflow, before
anything is packaged.

## Building packages locally

### macOS

```sh
cmake -S . -B build/package -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
      -DERDFLOW_PACKAGING=ON \
      -DCMAKE_PREFIX_PATH=<official-Qt-prefix> \
      -DCMAKE_OSX_ARCHITECTURES=arm64 \
      -DCMAKE_OSX_DEPLOYMENT_TARGET=13.0
cmake --build build/package --parallel
ctest --test-dir build/package --output-on-failure
cpack --config build/package/CPackConfig.cmake
# -> build/package/dist/ERDFlow-<version>-macos-arm64.dmg
```

Set the deployment target to the `minos` of the Qt you build with
(`vtool -show-build <Qt>/lib/QtCore.framework/QtCore`); the workflow does this
itself.

`macos/finish-bundle.cmake` runs after macdeployqt. macdeployqt copies every
plugin a Qt installation has, and what those plugins need, so a Qt with more
modules than ERDFlow uses puts unwanted components in the bundle; Homebrew's Qt
even supplies the GPL-only Qt Virtual Keyboard. The step removes that plugin,
removes plugins that could never load, removes frameworks and libraries that
nothing loads, stops the build if anything GPL-only remains or anything is
loaded from outside the bundle, and finally signs the bundle ad hoc (Apple
silicon will not run unsigned code) and checks the signature.

A DMG built against Homebrew's Qt is for local testing only: it also contains
the Homebrew libraries that Homebrew's Qt is linked against (ICU, GLib, D-Bus
and image libraries). Strict notice generation rejects missing SBOMs and
unidentified native binaries. `-DERDFLOW_ALLOW_INCOMPLETE_NOTICES=ON` permits
only a local test build, prominently marked INCOMPLETE; it is never set in the
workflow. The workflow builds with the official Qt binaries.

### Windows

From a Visual Studio 2022 developer prompt, with Qt's `bin` folder on `PATH`
and Inno Setup 6 installed:

```bat
cmake -S . -B build\package -A x64 -DBUILD_TESTING=ON -DERDFLOW_PACKAGING=ON
cmake --build build\package --config Release --parallel
ctest --test-dir build\package --build-config Release --output-on-failure
cmake --install build\package --config Release --prefix build\package\stage
iscc build\package\packaging\erdflow.iss
:: -> build\package\dist\ERDFlow-<version>-windows-x86_64-setup.exe
```

The installer is per-user: it needs no administrator rights and installs into
`%LOCALAPPDATA%\Programs\ERDFlow`, with a Start menu entry and, if chosen, a
desktop shortcut. The Visual C++ runtime is installed beside `erdflow.exe`.
Its `AppId` in `erdflow.iss.in` identifies ERDFlow across versions and must
never change: `{A5A5C30E-6D62-457C-98AD-F0F6F3275E13}`.

### Linux

```sh
cmake -S . -B build/package -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=ON \
      -DERDFLOW_PACKAGING=ON -DCMAKE_INSTALL_PREFIX=/usr
cmake --build build/package --parallel
ctest --test-dir build/package --output-on-failure
DESTDIR=$PWD/build/package/AppDir cmake --install build/package
export QMAKE=<Qt>/bin/qmake LD_LIBRARY_PATH=<Qt>/lib
linuxdeploy-x86_64.AppImage --appdir build/package/AppDir \
  --desktop-file build/package/AppDir/usr/share/applications/erdflow.desktop \
  --icon-file build/package/AppDir/usr/share/icons/hicolor/512x512/apps/erdflow.png \
  --plugin qt
cmake -D ERDFLOW_NOTICES_ROOT="$PWD/build/package/AppDir" \
  -P build/package/packaging/write-notices.cmake
# Use the runtime and tool hashes pinned in .github/workflows/package.yml.
LDAI_RUNTIME_FILE=<verified-runtime-x86_64> \
LDAI_OUTPUT=ERDFlow-<version>-linux-x86_64.AppImage \
  linuxdeploy-x86_64.AppImage --appdir build/package/AppDir --output appimage
```

The workflow pins linuxdeploy `1-alpha-20251107-1` and linuxdeploy-plugin-qt
`1-alpha-20250213-1` and checks their SHA-256 sums before running them. The
AppImage runs on distributions with glibc at least as new as the build
machine's (Ubuntu 24.04, glibc 2.39).

### Icons

`icons/make-icons.sh` regenerates `erdflow.icns`, `erdflow.ico` and
`erdflow.png` from `erdflow-icon.svg` on a Mac (headless Google Chrome,
`iconutil`, Python 3). The generated files are committed so builds need none
of these tools.

## Metadata and audit status

`project(ERDFlow VERSION 0.1.0)` in `CMakeLists.txt` supplies the executable's
application version, Windows resources and installer, macOS plist and DMG,
Linux artifact name, and generated notices. The `.erdx` format is separate.
The copyright comes from the root `LICENSE`.

The owner approved the official website mark and `app.erdflow.ERDFlow` bundle
identifier. The existing Windows AppId is preserved. Ordinary application
behavior and UI are unchanged. Packaging rejects static Qt and unsupported
Windows/Linux architectures.

The generator inventories staged Mach-O, PE and ELF binaries using the build's
Qt SPDX files, explicit runtime declarations and, on Linux, Debian package
copyright files (including referenced common licence texts). Unknown binaries,
missing texts and non-LGPL Qt modules fail the build. `inventory_complete`
means that this scan found no gaps; `redistribution_approved` remains false.
The JSON is not an SPDX SBOM or proof that a binary matches an upstream SBOM.
Binary hashes are recorded before signing/container assembly. Scripts, outer
installer/runtime code and final-container changes require separate review.

## Before a public release

- Provide exact corresponding Qt source and build information under ERDFlow's
  control; set `ERDFLOW_QT_SOURCE_OFFER` only to the reviewed delivery terms.
- Complete AppImage runtime dependency notices and static LGPL source/relinking
  obligations; audit AppRun/hooks and final linuxdeploy output.
- Verify exact Visual C++ runtime versions, redistribution entitlement and
  applicable terms; audit the final Inno Setup engine and embedded components.
- Generate a final-package SPDX SBOM and retain the source/licensing evidence.
- Test Qt replacement/relinking on every target and review the macOS signing
  interaction. The About/Licences UI gate remains a separate product change.
- Resolve existing visual snapshot failures through owner review, without
  changing baselines as part of this packaging task.
- Developer ID signing, hardened runtime and notarization for both DMGs;
  Authenticode signing for Windows; Linux detached signatures.
- Test installation, upgrade, launch and removal on clean supported systems.
  A mounted DMG smoke test is not a clean-machine installation test.
- Decide whether to register the `.erdx` file type with each installer.

See `STAGE-I2A-REVIEW.md` for the local evidence and remaining blockers.
