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
| Source delivery mechanism and retention reviewed | **Release owner** | Recommended: publish the source with every GitHub release, and never delete a release while its packages are offered. The packages' `QT-LGPL-INFORMATION.md` says so (`ERDFLOW_QT_SOURCE_OFFER` in `.github/workflows/package.yml`) |
| Application terms preserve LGPL rights | Done | ERDFlow is MIT and has no other terms; `QT-LGPL-INFORMATION.md` and Help -> Licences state the rights |
| SPDX SBOM from each release package | Done | `ERDFlow-0.1.0-<platform>.spdx.json`, validated with the SPDX project's tools in the workflow |
| Replacement instructions tested | Done | The replacement tests above follow `QT-LGPL-INFORMATION.md` |
| macOS signing and notarization reviewed against the LGPL | **Release owner** | The plan asks for professional legal review. Signing does not stop replacement: `QT-LGPL-INFORMATION.md` says how to sign a modified copy for one's own use, and the workflow tests exactly that |
| Release owner sign-off | **Release owner** | Set the repository variable `ERDFLOW_REDISTRIBUTION_APPROVED` to `true` (below) |

## Platform obligations (`packaging/README.md`)

| Item | State |
| --- | --- |
| Qt source offer set | Done: the packages carry it, and the offer's release blocker is gone |
| AppImage runtime notices and LGPL source | Done: the licences of libfuse, squashfuse, zstd, zlib, mimalloc and musl ship in `appimage-runtime/`; the runtime, libfuse 3.15.0 with its patch and squashfuse are published with the release |
| Ubuntu libraries in the AppImage | Done: their source packages, at the exact versions, are collected by the workflow and published with the release |
| AppRun, hooks and linuxdeploy additions audited | Done: `AppRun`, linuxdeploy-plugin-qt's hook (MIT, notice shipped), `qt.conf`, the desktop entry and icon; Qt's translations removed; `libOpenGL.so.0` and `libgpg-error.so.0` added, without which the AppImage did not start on fresh Ubuntu, Debian and Fedora systems |
| Visual C++ runtime | Versions recorded in each Windows package. **Release owner:** confirm the entitlement to redistribute them (below) |
| Inno Setup engine and notices | Done: the Inno Setup licence ships with the uninstaller it covers |
| Final-package SPDX SBOM | Done |
| macOS Developer ID signing, hardened runtime, notarization | Ready in the workflow; **needs the release owner's credentials** |
| Windows Authenticode signing | **Not set up**: needs a signing service or certificate (below) |
| Linux detached signature | Ready: `SHA256SUMS.asc`; **needs the release owner's key** |
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

## What only the release owner can do

1. **Source offer.** Confirm the wording of `ERDFLOW_QT_SOURCE_OFFER` in
   `.github/workflows/package.yml`, and that releases will not be deleted while
   their packages are offered.
2. **Visual C++ runtime.** Confirm you hold a Visual Studio licence that lets
   you redistribute its runtime files. Visual Studio Community is free for an
   individual developer and its terms include this; see
   <https://learn.microsoft.com/en-us/visualstudio/releases/2022/redistribution>.
3. **Apple** (for the two macOS packages to open without a Gatekeeper
   warning):
   - membership of the Apple Developer Program;
   - a *Developer ID Application* certificate, exported with its private key
     as a `.p12`;
   - an App Store Connect API key with the Developer role, for the notary
     service;
   - the six repository secrets in `packaging/README.md`. Never paste them into
     a chat or an issue.
4. **Windows signing** (without it, SmartScreen warns that the publisher is
   unknown). Choose one:
   - SignPath Foundation, free for open-source projects and built for GitHub
     Actions;
   - Microsoft's Artifact Signing (Trusted Signing), if your country and
     status qualify;
   - a code-signing certificate held in a cloud key store from a certificate
     authority.

   The workflow's signing step is added once you have chosen.
5. **OpenPGP key** for `SHA256SUMS.asc`: create one, publish its public key
   (in the repository or on a key server), and set the two secrets.
6. **Real machines.** Install, start, open the example, export, quit and
   uninstall on:
   - Windows 11, or Windows Sandbox;
   - a Mac with Apple silicon and an Intel Mac, after notarization, opening the
     downloaded disk image normally so Gatekeeper checks it;
   - a Linux desktop.
7. **Approve and release.**
   - Set `ERDFLOW_REDISTRIBUTION_APPROVED` to `true`.
   - Tag the release commit `v0.1.0`.
   - Run *Desktop Packages* on the tag: `gh workflow run package.yml --ref v0.1.0`.
   - Gather its files with `packaging/release/assemble-release.sh <run-id>`.
     It checks every sum and that every package says it was approved, then
     prints the command that creates the release as a draft.
   - Publish the draft once its files are checked.
