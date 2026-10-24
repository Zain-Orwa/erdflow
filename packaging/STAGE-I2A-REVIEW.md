# Stage I2A: release metadata and licensing preparation

Review record, 2026-10-11, branch `feature/desktop-installers` on
`3888aba6641d1a1540882cfa3cd9ede26c53759d`. Nothing here approves
distribution; the packages remain unsigned test candidates.

## Decisions applied

| Item | Value | Source |
| --- | --- | --- |
| Application icon | the erdflow.app website mark (`icons/erdflow-icon.svg`) | owner |
| macOS bundle identifier | `app.erdflow.ERDFlow` | owner |
| Windows installer AppId | `{A5A5C30E-6D62-457C-98AD-F0F6F3275E13}` (unchanged since Stage I1; must never change) | owner: keep stable |
| Copyright | `Copyright (c) 2026 Zain Orwa` (root `LICENSE`) | owner: legal name |
| Version | `project(ERDFlow VERSION 0.1.0)` only | single source |

The version reaches the program (`--version`, through the `ERDFLOW_VERSION`
definition), the Windows version resource and installer, `Info.plist`, the DMG,
AppImage and installer file names (through `<build>/packaging/erdflow-version.txt`)
and the generated notices. The `.erdx` format version is separate and unchanged.

## Licensing evidence

- Qt binaries: official Qt 6.11.2 from The Qt Company, the same that
  `install-qt-action` gives the workflow. Their SPDX SBOMs (`<Qt>/sbom/`) name
  qtbase `ef55f427f2c8b410d34f8a7681020a3000cf6866` and qtsvg
  `17ca512f903f935282ebeca496aac5d11ba4199a`.
- Corresponding source: the official `qtbase`/`qtsvg-everywhere-src-6.11.2`
  archives, whose `.tag` files name those commits; hashes in
  `notices/licenses/README.md`. They are **not yet hosted under ERDFlow's
  control**.
- Licence texts: copied unedited from those archives' `LICENSES/`, plus ICU 73.2
  and the AppImage runtime's own licence (`notices/licenses/README.md`).
- Every package's `THIRD-PARTY-LICENSES.txt` is generated from its own contents
  by `notices/package_notices.py`; an unidentified binary or missing licence
  text stops the build.

### What each package contains (Qt 6.11.2)

| Package | Qt libraries | Qt plugins | Other libraries | Evidence |
| --- | --- | --- | --- | --- |
| macOS arm64 / x86_64 | Core, Gui, Widgets, Svg, DBus | cocoa, macstyle, gif, ico, jpeg, svg, svgicon | none | built and inventoried locally |
| Windows x86_64 | Core, Gui, Widgets, Svg | windows, modernwindowsstyle, gif, ico, jpeg, svg, svgicon (expected) | Visual C++ runtime | from the official binaries' imports; CI only |
| Linux x86_64 | Core, Gui, Widgets, Svg, DBus, XcbQpa | xcb, input contexts, xcb GL integrations, image formats, svgicon (expected) | ICU 73.2; Ubuntu system libraries chosen by linuxdeploy | from the official binaries' ELF dependencies; CI only |

Windows leaves out the Mesa software renderer and the DirectX shader
compilers (`--no-opengl-sw --no-system-d3d-compiler --no-system-dxc-compiler`):
ERDFlow uses neither OpenGL nor Direct3D. Qt for Windows uses the system's
`icuuc.dll` (Windows 10 1903 and later), so no ICU is shipped there.

## Local verification (macOS 26.6.2, Apple silicon)

| Check | Result |
| --- | --- |
| Ordinary Debug and Release builds (Homebrew Qt 6.11.1) | 9 of 10 suites pass; `visual` fails on 6 pictures, as before this stage |
| Package build, official Qt, arm64 | same 9 of 10; DMG built; workflow's macOS verify step passes |
| Package build, official Qt, x86_64 (tests under Rosetta) | same 9 of 10; DMG built; verify step passes |
| Bundle identifier, version, `--version` | `app.erdflow.ERDFlow`, `0.1.0`, `ERDFlow 0.1.0` |
| Minimum macOS | 13.0, from the official QtCore `minos` |
| Inventory | complete, no unidentified binaries; 12 Qt components under LGPL-3.0-only, 41 third-party components inside Qt |
| Homebrew Qt package build | refused: Homebrew's Qt has no SBOM, so 47 binaries (its Qt and the Homebrew libraries) could not be identified |
| Notice generator tests | 6 pass |
| Icons | source SVG identical to the website's `src/app/icon.svg`; `.icns`, `.ico`, `.png` regenerate identically |
| Windows and Linux | not buildable here; generator run on simulated layouts of the official Qt files |

The `visual` failures come from the macOS reference pictures predating the
menu row added in `0085e62`; on the CI runners, which have no reference
pictures of their own, the suite runs its audit and skips the pictures. The
baselines are unchanged.

## Release blockers

1. Qt source code and build information offered from a location under
   ERDFlow's control, and the delivery method and retention reviewed
   (`ERDFLOW_QT_SOURCE_OFFER`; until set, every package says it must not be
   distributed).
2. AppImage runtime: notices for the libraries statically linked into it, and
   the source and relinking obligations of its patched libfuse (LGPL-2.1).
   AppRun, hooks and other linuxdeploy additions audited from the final image.
3. Visual C++ runtime: exact versions, redistribution entitlement and terms;
   Inno Setup's own engine and notices.
4. A final-package SPDX SBOM, replacement tests on every platform, and legal
   review of the macOS signing interaction (`docs/QT_LGPL_COMPLIANCE.md`).
5. The About/Licences screen required by the compliance plan is a product
   change and has not been made.
6. Developer ID signing and notarization; Windows Authenticode; Linux
   detached signatures; clean-machine install, upgrade and removal tests.
7. The first run of `package.yml` on GitHub (Windows and Linux have never been
   built).

## Notes

- The macOS DMGs carry universal (x86_64 + arm64) Qt frameworks, as the
  official Qt ships them; each app itself is single-architecture.
- Wayland plugins are not deployed; the AppImage uses X11 (XWayland on
  Wayland desktops).
- `docs/release_plan.md` says the repository is private; GitHub reports
  `Zain-Orwa/erdflow` as public. The settled document is unchanged.
