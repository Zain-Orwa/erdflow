# ERDFlow 0.3.3

ERDFlow 0.3.3 adds a fifth notation, controls for a schema connector's
relationship, a way to add a column from Practice Data, and two corrections.

## What is new

- **Normal notation.** The Notation picker, the Notation menu and the
  Schema's Appearance menu now offer five choices: Chen, Min–max, Crow's
  foot, Bachman and **Normal**. Normal draws a relationship's lines with
  nothing at their ends: no symbol, no letter and no numbers, in the
  Conceptual diagram and in the Schema alike. It changes only how the
  diagram is shown; a diagram still opens in Crow's foot.
- **Relationship controls on a schema connector.** With a connector chosen
  in the Schema, Properties offers its relationship type (one-to-one,
  one-to-many, many-to-one, many-to-many), each end's One or Many, and
  **Flip relationship**. Many-to-many makes a junction table, and changing
  it back takes the junction away. Each change can be undone.
- **Add Column in Practice Data.** Every table card has **Add Column**
  beside Add Row. It asks for a name and adds the column to the schema, or
  the attribute to the diagram where the table came from one; the new cells
  read NULL.

## What is corrected

- **The double line belongs to Chen.** A relationship side with total
  participation is drawn as two lines in Chen only. Min–max, Crow's foot and
  Bachman already say the minimum in their own marks, and now keep a single
  line. In Chen, for a relationship between two entities, the double line is
  drawn at the entity that must take part, which is read from the minimum at
  the other end. A total specialization keeps its double line in every
  notation.
- **Properties stays where you scrolled it.** Choosing something in the
  Properties panel no longer sends the panel back to its top.

Your projects are not changed by any of this: the file format is still 37,
and a diagram's constraints are stored exactly as they were.

**If you have 0.3.2, this is the first update that shows the Update and
Restart button.** A little after ERDFlow opens, the button appears at the top
right of the window; press it and ERDFlow restarts as 0.3.3. ERDFlow 0.3.0
and 0.3.1 have no button: they fetch 0.3.3 in the background and take it when
they are closed, so close ERDFlow completely and open it again.

## Downloads

| System | File | Requires |
| --- | --- | --- |
| Windows, 64-bit | `ERDFlow-0.3.3-windows-x86_64-setup.exe` | Windows 10 (version 1903 or later) or Windows 11. Installs for the current user; no administrator rights needed. |
| macOS, Apple silicon | `ERDFlow-0.3.3-macos-arm64.dmg` | macOS 13 or later. Open the disk image and drag ERDFlow to Applications. |
| macOS, Intel | `ERDFlow-0.3.3-macos-x86_64.dmg` | macOS 13 or later. |
| Linux, 64-bit | `ERDFlow-0.3.3-linux-x86_64.AppImage` | A distribution with glibc 2.39 or later, such as Ubuntu 24.04, Debian 13 or Fedora 40. Make it executable and run it. On Wayland it runs through XWayland. |

## These packages are not signed by Apple or Microsoft

ERDFlow is a free project and this release is not signed with an Apple
Developer ID, not notarized by Apple, and not signed with a Windows
code-signing certificate. Your system will therefore warn that it cannot
verify the developer. The warnings are expected; they do not mean anything is
wrong with the download, but they also mean that only the checksums below
tell you the file is the one published here.

**Check the download first.** Compare its SHA-256 with the line for it in
`SHA256SUMS` from this release page:

- Windows (PowerShell): `Get-FileHash .\ERDFlow-0.3.3-windows-x86_64-setup.exe -Algorithm SHA256`
- macOS: `shasum -a 256 -c SHA256SUMS --ignore-missing`
- Linux: `sha256sum -c SHA256SUMS --ignore-missing`

`SHA256SUMS.ed25519` authenticates the installer checksums for ERDFlow's
automatic updater using its embedded public key. No separate GPG signature
is included.

**Windows.** When you start the installer, Microsoft Defender SmartScreen may
say *Windows protected your PC*. Choose **More info**, check that the file is
`ERDFlow-0.3.3-windows-x86_64-setup.exe`, then **Run anyway**. The publisher is
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
ERDFlow-0.3.3-linux-x86_64.AppImage`) and run it. No FUSE library needs to be
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
