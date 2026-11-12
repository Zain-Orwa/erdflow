# ERDFlow 0.3.2

ERDFlow 0.3.2 tells you when an update is ready, and lets you take it at once.

## What is new

- **Update and Restart.** When a newer ERDFlow has been fetched and is ready,
  a small **Update and Restart** button appears at the top of the window, at
  the right. Press it and ERDFlow closes, updates itself and opens again as
  the new version. If you have unsaved changes you are asked about them
  first, exactly as when you close ERDFlow yourself, and you can cancel.
- **The button is there only while an update is ready.** The rest of the time
  nothing is shown, and nothing else in the window has moved.
- **Updates otherwise work as before.** ERDFlow still looks for a newer
  release a little after it opens, fetches it in the background, and takes it
  only when its signature is ERDFlow's own and the download matches the
  release's checksums. If you leave the button alone, the update is put in
  place when you close ERDFlow. With *Automatic Updates* turned off
  (Home → Settings), nothing is fetched and the button never appears.
- If ERDFlow is open twice, the button asks you to close the other one first:
  an update is put in place when the last ERDFlow closes.

**You will first see the button with the release after this one.** ERDFlow
0.3.0 and 0.3.1 have no button to show. They fetch 0.3.2 in the background
and take it when they are closed, so close ERDFlow completely and open it
again. From 0.3.2 on, each new release says that it is ready.

Editor behaviour and projects are unchanged from 0.3.1 (file format 37).

## Downloads

| System | File | Requires |
| --- | --- | --- |
| Windows, 64-bit | `ERDFlow-0.3.2-windows-x86_64-setup.exe` | Windows 10 (version 1903 or later) or Windows 11. Installs for the current user; no administrator rights needed. |
| macOS, Apple silicon | `ERDFlow-0.3.2-macos-arm64.dmg` | macOS 13 or later. Open the disk image and drag ERDFlow to Applications. |
| macOS, Intel | `ERDFlow-0.3.2-macos-x86_64.dmg` | macOS 13 or later. |
| Linux, 64-bit | `ERDFlow-0.3.2-linux-x86_64.AppImage` | A distribution with glibc 2.39 or later, such as Ubuntu 24.04, Debian 13 or Fedora 40. Make it executable and run it. On Wayland it runs through XWayland. |

## These packages are not signed by Apple or Microsoft

ERDFlow is a free project and this release is not signed with an Apple
Developer ID, not notarized by Apple, and not signed with a Windows
code-signing certificate. Your system will therefore warn that it cannot
verify the developer. The warnings are expected; they do not mean anything is
wrong with the download, but they also mean that only the checksums below
tell you the file is the one published here.

**Check the download first.** Compare its SHA-256 with the line for it in
`SHA256SUMS` from this release page:

- Windows (PowerShell): `Get-FileHash .\ERDFlow-0.3.2-windows-x86_64-setup.exe -Algorithm SHA256`
- macOS: `shasum -a 256 -c SHA256SUMS --ignore-missing`
- Linux: `sha256sum -c SHA256SUMS --ignore-missing`

`SHA256SUMS.ed25519` authenticates the installer checksums for ERDFlow's
automatic updater using its embedded public key. No separate GPG signature
is included.

**Windows.** When you start the installer, Microsoft Defender SmartScreen may
say *Windows protected your PC*. Choose **More info**, check that the file is
`ERDFlow-0.3.2-windows-x86_64-setup.exe`, then **Run anyway**. The publisher is
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
ERDFlow-0.3.2-linux-x86_64.AppImage`) and run it. No FUSE library needs to be
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
