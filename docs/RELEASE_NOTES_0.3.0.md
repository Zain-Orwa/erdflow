# ERDFlow 0.3.0

ERDFlow 0.3.0 keeps itself up to date: from this version on, a new release
reaches you without your having to download it.

## What is new

- **Automatic updates.** A little after ERDFlow opens, it looks for a newer
  release. If there is one, it fetches it in the background while you work,
  and puts it in place when you close ERDFlow, so the next time you open it,
  it is the new version. Nothing asks you anything, and nothing interrupts
  your work. Offline, it simply does not look.
- **Every update is checked.** Each release is signed, and ERDFlow takes an
  update only when its signature is ERDFlow's own and the download matches
  the release's checksums exactly.
- **You can turn it off.** Home → Settings → *Automatic Updates*. It is on
  unless you turn it off.

**Installing 0.3.0 itself is done by hand**, once, as before: 0.1.0 and 0.2.0
cannot update themselves. Every release after 0.3.0 arrives on its own.

While Automatic Updates is on, ERDFlow contacts GitHub each time it opens, to
see whether there is a newer release. It sends nothing about you or your
projects; GitHub sees only what any download shows it, such as your network
address.

Projects are unchanged from 0.2.0 (file format 37).

Not in this release: generating SQL, and multi-page diagrams.

## Downloads

| System | File | Requires |
| --- | --- | --- |
| Windows, 64-bit | `ERDFlow-0.3.0-windows-x86_64-setup.exe` | Windows 10 (version 1903 or later) or Windows 11. Installs for the current user; no administrator rights needed. |
| macOS, Apple silicon | `ERDFlow-0.3.0-macos-arm64.dmg` | macOS 13 or later. Open the disk image and drag ERDFlow to Applications. |
| macOS, Intel | `ERDFlow-0.3.0-macos-x86_64.dmg` | macOS 13 or later. |
| Linux, 64-bit | `ERDFlow-0.3.0-linux-x86_64.AppImage` | A distribution with glibc 2.39 or later, such as Ubuntu 24.04, Debian 13 or Fedora 40. Make it executable and run it. On Wayland it runs through XWayland. |

## These packages are not signed by Apple or Microsoft

ERDFlow is a free project and this release is not signed with an Apple
Developer ID, not notarized by Apple, and not signed with a Windows
code-signing certificate. Your system will therefore warn that it cannot
verify the developer. The warnings are expected; they do not mean anything is
wrong with the download, but they also mean that only the checksums below
tell you the file is the one published here.

**Check the download first.** Compare its SHA-256 with the line for it in
`SHA256SUMS` from this release page:

- Windows (PowerShell): `Get-FileHash .\ERDFlow-0.3.0-windows-x86_64-setup.exe -Algorithm SHA256`
- macOS: `shasum -a 256 -c SHA256SUMS --ignore-missing`
- Linux: `sha256sum -c SHA256SUMS --ignore-missing`

`SHA256SUMS` is not signed in this release; it is as trustworthy as the GitHub
release page you downloaded it from.

**Windows.** When you start the installer, Microsoft Defender SmartScreen may
say *Windows protected your PC*. Choose **More info**, check that the file is
`ERDFlow-0.3.0-windows-x86_64-setup.exe`, then **Run anyway**. The publisher is
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
ERDFlow-0.3.0-linux-x86_64.AppImage`) and run it. No FUSE library needs to be
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
