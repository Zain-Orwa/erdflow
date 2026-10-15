# ERDFlow 0.1.0 release record

The evidence for releasing ERDFlow 0.1.0, against the gate in
`docs/QT_LGPL_COMPLIANCE.md` and the list in `packaging/README.md`. It says
what is done, with where to see it, and what only the release owner, Zain, can
do. It is not legal advice.

**Prepared:** 2026-10-15 on `release/0.1.0-prep`, from `main` at `5bb78c4`.

## Licence and distribution gate (`docs/QT_LGPL_COMPLIANCE.md`)

| Requirement | State | Evidence |
| --- | --- | --- |
| Final package contents enumerated | Done | Each package's `ERDFlow-0.1.0-<platform>-inventory.json` and `THIRD-PARTY-LICENSES.txt`, generated from the package itself |
| Every component has a licence classification | Done | The inventories list no problems; an unidentified binary stops the build |
| No GPL-only component unintentionally present | Done | Qt is used under LGPL-3.0; the workflow refuses Qt Virtual Keyboard; the Windows package no longer carries Qt Network (`7ec7c30`) |
| Qt dynamically linked, replaceability tested | Done | The workflow replaces Qt SVG in the installed package on each platform and starts ERDFlow again |
| Licence texts and notices ship with each package | Done | Checked by the workflow on every platform |
| About/Licences exposes the installed notices | Done | Help -> Licences (`app/desktop/licences.cpp`, `licences_tests`) |
| Exact Qt source, patches and build information under ERDFlow's control | Ready; final when the release is published | The sources job collects the official Qt 6.11.2 archives, checked against their published SHA-256; each package's Qt build information is kept; all are published as assets of the release. ERDFlow applies no patches to Qt |
| Source delivery mechanism and retention reviewed | Done (owner, 2026-10-15) | The source is published with every GitHub release and kept there while its packages are offered; the packages' `QT-LGPL-INFORMATION.md` says so (`ERDFLOW_QT_SOURCE_OFFER` in `.github/workflows/package.yml`), wording accepted as written |
| Application terms preserve LGPL rights | Done | ERDFlow is MIT and has no other terms; `QT-LGPL-INFORMATION.md` and Help -> Licences state the rights |
| SPDX SBOM from each release package | Done | `ERDFlow-0.1.0-<platform>.spdx.json`, validated with the SPDX project's tools in the workflow |
| Replacement instructions tested | Done | The replacement tests above follow `QT-LGPL-INFORMATION.md` |
| macOS signing and notarization reviewed against the LGPL | Not applicable to 0.1.0 | 0.1.0 has no Developer ID signature or notarization (owner, 2026-10-15): the app is signed ad hoc, as Apple silicon requires, which binds it to no one. `QT-LGPL-INFORMATION.md` says how to sign a modified copy for one's own use, and the workflow tests exactly that. The review is needed only if a Developer ID signature is added later |
| Release owner sign-off | **Release owner** | Set the repository variable `ERDFLOW_REDISTRIBUTION_APPROVED` to `true` (below) |

## Platform obligations (`packaging/README.md`)

| Item | State |
| --- | --- |
| Qt source offer set | Done: the packages carry it, and the offer's release blocker is gone |
| AppImage runtime notices and LGPL source | Done: the licences of libfuse, squashfuse, zstd, zlib, mimalloc and musl ship in `appimage-runtime/`; the runtime, libfuse 3.15.0 with its patch and squashfuse are published with the release |
| Ubuntu libraries in the AppImage | Done: their source packages, at the exact versions, are collected by the workflow and published with the release |
| AppRun, hooks and linuxdeploy additions audited | Done: `AppRun`, linuxdeploy-plugin-qt's hook (MIT, notice shipped), `qt.conf`, the desktop entry and icon; Qt's translations removed; `libOpenGL.so.0` and `libgpg-error.so.0` added, without which the AppImage did not start on fresh Ubuntu, Debian and Fedora systems |
| Visual C++ runtime | Permitted: every file shipped (`concrt140`, `msvcp140`, `msvcp140_1`, `msvcp140_2`, `msvcp140_atomic_wait`, `msvcp140_codecvt_ids`, `vcruntime140`, `vcruntime140_1`, version 14.51.36247.0) is from `VC\redist`, which Microsoft's Distributable list lets licensed users of Visual Studio Community, Professional or Enterprise 2022 distribute unmodified with their program (below) |
| Inno Setup engine and notices | Done: the Inno Setup licence ships with the uninstaller it covers |
| Final-package SPDX SBOM | Done |
| macOS Developer ID signing, hardened runtime, notarization | Deferred (owner: no paid membership). The packages are signed ad hoc; the workflow signs and notarizes only if the secrets are ever set. The release notes say the packages are not signed by Apple and how to open them |
| Windows Authenticode signing | Deferred until free signing is available: SignPath Foundation, applied for after 0.1.0 is released (below). The release notes say the installer is not signed and how to run it |
| Linux detached signature | Deferred (owner: no key for now); the workflow signs `SHA256SUMS` only if a key is set. The release notes say it is not signed |
| Clean-machine install, upgrade and removal | Done on fresh CI machines (below); **a check on real machines is the release owner's** |
| `.erdx` file association | Not in 0.1.0; stated in the release notes |
| Visual reference pictures | Unchanged: six macOS-26 references are stale (known since 2026-09-25); CI skips them. Not a distribution matter |

## What the workflow verifies on every run

- **Windows:** the installer installs per user, the program reports 0.1.0 and
  starts; installed again over itself, as an upgrade installs; every program
  and library loads only from the package or from Windows itself (never a
  Visual C++ runtime the machine happens to have); Qt SVG replaced and started
  again; uninstalled with nothing left behind.
- **macOS (Apple silicon and Intel):** the disk image verifies; the app is
  signed, single-architecture, loads nothing from outside its bundle, and
  starts; copied into `/Applications`, started there, Gatekeeper's verdict
  taken (required once notarized), Qt SVG replaced and re-signed for own use,
  started again, removed.
- **Linux:** the AppImage starts on the build machine and on fresh Ubuntu
  24.04, Debian 13 and Fedora 42 systems; unpacked, Qt SVG replaced, started
  again.

## The Visual C++ runtime's terms

Microsoft's Distributable list for Visual Studio 2022
(<https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution>,
dated 2025-11-11) says: "If you have a validly licensed copy of such software,
you may copy and distribute with your program the unmodified form of the files
listed below", and, for the Visual C++ runtime, "any of the files within the
following folder and its subfolders": `VC\redist`. The Distribution
Requirements of the Visual Studio terms (read in the Visual Studio 2019
Professional and Enterprise terms, which use the same Distributable Code
section; the 2022 Community terms page could not be read as text) are met as
follows:

| Requirement | ERDFlow |
| --- | --- |
| Add significant primary functionality to it | ERDFlow is a complete application |
| Require distributors and end users to agree to terms that protect it at least as much | `THIRD-PARTY-NOTICES.md` in every Windows package states Microsoft's terms for the files |
| Indemnify Microsoft against claims arising from the distribution or use of the application | An obligation of the distributor, not of the package |
| Do not use Microsoft's trademarks to suggest endorsement | The notices only name Microsoft as the files' owner |
| Do not make it subject to an Excluded License | The files are unmodified, and not covered by ERDFlow's MIT licence or the LGPL |

What this rests on: the distributor holding a validly licensed copy of Visual
Studio 2022. Visual Studio Community is free for an individual working on their
own applications, and for open-source projects. The Windows packages are built
on GitHub's runner, whose Visual Studio is GitHub's; the right to redistribute
comes from the release owner's own Community licence, which costs nothing.
Whether the notice alone satisfies "agree to terms" is a legal judgement; an
installer page that the user must accept would satisfy it beyond doubt, at
the cost of one more step in installing.

## What only the release owner can do

All of it is free.

1. **Visual Studio Community.** Confirm you hold, or accept, a Visual Studio
   Community 2022 licence: installing Visual Studio Community is enough, and
   costs nothing. Decide whether the installer should also show a licence page
   (above); it does not now.
2. **Real machines.** Install, start, open the example, export, quit and
   uninstall on:
   - Windows 11, or Windows Sandbox, going through SmartScreen as the release
     notes describe;
   - a Mac with Apple silicon and an Intel Mac, opening the downloaded disk
     image normally and following the release notes' Gatekeeper steps;
   - a Linux desktop.
3. **Approve and release.**
   - Set `ERDFLOW_REDISTRIBUTION_APPROVED` to `true`.
   - Tag the release commit `v0.1.0`.
   - Run *Desktop Packages* on the tag: `gh workflow run package.yml --ref v0.1.0`.
   - Gather its files with `packaging/release/assemble-release.sh <run-id>`.
     It checks every sum and that every package says it was approved, then
     prints the command that creates the release as a draft.
   - Publish the draft once its files are checked.
4. **After the release (optional).** Apply to SignPath Foundation for free
   Windows signing (<https://signpath.org>). It asks for:
   - a project already released in the form to be signed, and some
     reputation;
   - an OSI licence for all components;
   - multi-factor authentication for the team;
   - named authors, reviewers and approvers;
   - a "Code signing policy" page;
   - a privacy statement. ERDFlow sends nothing anywhere.

   Ask them whether they count Qt, which ERDFlow uses under the LGPL while The
   Qt Company also sells it commercially, against "without commercial
   dual-licensing for all components", and whether they treat the Visual C++
   runtime as system libraries. The workflow's signing step is added once they
   accept.
