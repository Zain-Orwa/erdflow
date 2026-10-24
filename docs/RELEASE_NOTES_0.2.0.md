# ERDFlow 0.2.0

ERDFlow 0.2.0 adds Practice Data: put rows into the schema's tables and see
its keys and constraints at work before any SQL is written.

## What is new

- **Practice Data.** *Convert All to Tables*, on the Model and Design menus,
  turns the schema's tables into cards of rows to edit. Each table starts with
  three rows made from its columns' names and types, pointing at rows that
  exist. Foreign keys are chosen from the rows of the table they reference, and
  `NULL` stays distinct from an empty value. *Reset* makes the starting rows
  again and *Clear Table* empties a table. *Show Schema* goes back.
- **Constraints active, or off.** At the top of Practice Data. Active, as a
  project starts, an edit that breaks a primary key, a foreign key, `UNIQUE`,
  `NOT NULL` or the column's type is refused, and the card says which rule and
  why. Off, the edit is kept and the cells that break a rule are marked.
- **Constraints for Connect.** On a schema drawn by hand, Connect's menu has
  a *Constraints* section under *How the lines run*. Off, as a project starts,
  Connect joins any row to any row or table at once, asking nothing. Active,
  it asks before it makes or uses a column and turns away a connection a key
  would not allow, as it did in 0.1.0.
- **Rows are saved.** Practice rows and both choices are kept in the project
  file, and every change is one step for Undo.

A project saved by 0.2.0 is in file format 37, which ERDFlow 0.1.0 cannot open.
0.2.0 opens every file 0.1.0 saved.

Not in this release: generating SQL, and multi-page diagrams.

## Downloads

| System | File | Requires |
| --- | --- | --- |
| Windows, 64-bit | `ERDFlow-0.2.0-windows-x86_64-setup.exe` | Windows 10 (version 1903 or later) or Windows 11. Installs for the current user; no administrator rights needed. |
| macOS, Apple silicon | `ERDFlow-0.2.0-macos-arm64.dmg` | macOS 13 or later. Open the disk image and drag ERDFlow to Applications. |
| macOS, Intel | `ERDFlow-0.2.0-macos-x86_64.dmg` | macOS 13 or later. |
| Linux, 64-bit | `ERDFlow-0.2.0-linux-x86_64.AppImage` | A distribution with glibc 2.39 or later, such as Ubuntu 24.04, Debian 13 or Fedora 40. Make it executable and run it. On Wayland it runs through XWayland. |

## These packages are not signed by Apple or Microsoft

ERDFlow is a free project and this release is not signed with an Apple
Developer ID, not notarized by Apple, and not signed with a Windows
code-signing certificate. Your system will therefore warn that it cannot
verify the developer. The warnings are expected; they do not mean anything is
wrong with the download, but they also mean that only the checksums below
tell you the file is the one published here.

**Check the download first.** Compare its SHA-256 with the line for it in
`SHA256SUMS` from this release page:

- Windows (PowerShell): `Get-FileHash .\ERDFlow-0.2.0-windows-x86_64-setup.exe -Algorithm SHA256`
- macOS: `shasum -a 256 -c SHA256SUMS --ignore-missing`
- Linux: `sha256sum -c SHA256SUMS --ignore-missing`

`SHA256SUMS` is not signed in this release; it is as trustworthy as the GitHub
release page you downloaded it from.

**Windows.** When you start the installer, Microsoft Defender SmartScreen may
say *Windows protected your PC*. Choose **More info**, check that the file is
`ERDFlow-0.2.0-windows-x86_64-setup.exe`, then **Run anyway**. The publisher is
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
ERDFlow-0.2.0-linux-x86_64.AppImage`) and run it. No FUSE library needs to be
installed. If it says it cannot mount the AppImage, because the system does
not allow FUSE mounts, run it with `--appimage-extract-and-run`.

## Licences

ERDFlow is free software under the MIT licence. It uses Qt 6.11.2 under the
GNU LGPL version 3; the Qt source code, and the source of the other
components whose licences call for it, are published with this release (see
`SOURCES.md`). Help → Licences in ERDFlow shows where each package keeps its
licence texts and notices.

## Known limitations

- `.erdx` files are not yet associated with ERDFlow by the installers; open
  them from ERDFlow's own Open command.
- The interface is in English.
