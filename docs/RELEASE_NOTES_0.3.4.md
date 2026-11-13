# ERDFlow 0.3.4

ERDFlow 0.3.4 adds explicit optional relationship cardinalities in both
Conceptual and Relational Design, and corrects the schema's opening position.

## What is new

- **Zero or one and zero or many.** Relationship endpoint controls offer
  0..1, 1..1, 0..many and 1..many directly in both workspaces. Crow's Foot
  draws a hollow circle with a bar or a foot. Chen, Min–max and Bachman use
  their own participation marks. Normal continues to draw plain lines.
- **Schema participation is explicit.** A schema connector can have an
  optional minimum at either end. Minimum choices are stored independently
  of column NULL / NOT NULL constraints and data values. Existing primary
  keys, foreign keys, UNIQUE and identifier controls retain their meanings.
- **Saved bounds and undo.** Optional bounds survive save/reopen, undo/redo,
  connector direction changes and conversion from a standalone schema to a
  conceptual diagram. A full endpoint choice is one undo step.

## What is corrected

- **The whole schema stays inside the window.** Opening a saved schema while
  the window finishes maximising could leave its full-height canvas partway
  down the stage, hiding the bottom and the view controls. The opening
  animation now settles into the existing full-view layout. Table positions,
  scrolling, pan and zoom handlers are unchanged.

## Project compatibility

New saves use **ERDX format 38** to retain explicit schema minima. ERDFlow
0.3.4 opens files from all earlier formats. Versions 0.3.3 and earlier cannot
open format-38 files; update ERDFlow before opening a project saved by 0.3.4.
Zero participation means no related records are required; it does not insert
a numeric zero or NULL into a column. SQL generation remains future work.

## Updating an installed copy

ERDFlow 0.3.2 and 0.3.3 show **Update and Restart** after the signed update
has downloaded. Older copies take the update when closed completely and
opened again. The website links the same installers as this release.

## Downloads

| System | File | Requires |
| --- | --- | --- |
| Windows, 64-bit | `ERDFlow-0.3.4-windows-x86_64-setup.exe` | Windows 10 (version 1903 or later) or Windows 11. Installs for the current user; no administrator rights needed. |
| macOS, Apple silicon | `ERDFlow-0.3.4-macos-arm64.dmg` | macOS 13 or later. Open the disk image and drag ERDFlow to Applications. |
| macOS, Intel | `ERDFlow-0.3.4-macos-x86_64.dmg` | macOS 13 or later. |
| Linux, 64-bit | `ERDFlow-0.3.4-linux-x86_64.AppImage` | A distribution with glibc 2.39 or later, such as Ubuntu 24.04, Debian 13 or Fedora 40. Make it executable and run it. On Wayland it runs through XWayland. |

## These packages are not signed by Apple or Microsoft

ERDFlow is a free project and this release is not signed with an Apple
Developer ID, not notarized by Apple, and not signed with a Windows
code-signing certificate. Your system will therefore warn that it cannot
verify the developer. The warnings are expected; they do not mean anything is
wrong with the download, but they also mean that only the checksums below
tell you the file is the one published here.

**Check the download first.** Compare its SHA-256 with the line for it in
`SHA256SUMS` from this release page:

- Windows (PowerShell): `Get-FileHash .\ERDFlow-0.3.4-windows-x86_64-setup.exe -Algorithm SHA256`
- macOS: `shasum -a 256 -c SHA256SUMS --ignore-missing`
- Linux: `sha256sum -c SHA256SUMS --ignore-missing`

`SHA256SUMS.ed25519` authenticates the installer checksums for ERDFlow's
automatic updater using its embedded public key. No separate GPG signature
is included.

**Windows.** When you start the installer, Microsoft Defender SmartScreen may
say *Windows protected your PC*. Choose **More info**, check that the file is
`ERDFlow-0.3.4-windows-x86_64-setup.exe`, then **Run anyway**. The publisher is
shown as unknown. On a PC where *Smart App Control* is on, Windows blocks
unsigned programs outright and offers no way to run them; ERDFlow cannot be
installed there until it is signed.

**macOS.** Open the disk image and drag ERDFlow to Applications. The first
time you open it, macOS says it cannot verify that ERDFlow is free of malware.

- macOS 15 (Sequoia) and later: choose **Done**, open **System Settings ->
  Privacy & Security**, find *"ERDFlow" was blocked* near the bottom, choose
  **Open Anyway**, and confirm with your password.
- macOS 13 (Ventura) and 14 (Sonoma): Control-click ERDFlow in Applications,
  choose **Open**, then **Open** again.

macOS remembers the choice; later starts open ERDFlow directly.

**Linux.** Make the AppImage executable (`chmod +x
ERDFlow-0.3.4-linux-x86_64.AppImage`) and run it. No FUSE library needs to be
installed. If it says it cannot mount the AppImage, because the system does
not allow FUSE mounts, run it with `--appimage-extract-and-run`.

## Licences

ERDFlow is free software under the MIT licence. It uses Qt 6.11.2 under the
GNU LGPL version 3; the Qt source code, and the source of the other
components whose licences call for it, are published with this release (see
`SOURCES.md`). It checks update signatures with Monocypher, dedicated to the
public domain under CC0-1.0. Help → Licences in ERDFlow shows where each
package keeps its licence texts and notices.

## Known limitations

- `.erdx` files are not yet associated with ERDFlow by the installers; open
  them from ERDFlow's own Open command.
- The interface is in English.
