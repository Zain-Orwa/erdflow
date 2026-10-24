# ADR-024 — Automatic updates

> **Settled work.** Do not change, replace or re-style anything in this
> document to suit something new you have been asked to build. If what you are
> building genuinely contradicts what is written here, stop and ask Zain, who
> owns this project: say what you want to change, what the application will
> look like afterwards, and whether it is a gain or a loss. He decides.
> Full rule: [CLAUDE.md](../../CLAUDE.md).

**Status:** Accepted

**Date:** 2026-10-23
**Project:** ERDFlow
**Decision Scope:** How an installed ERDFlow brings itself up to the newest release

---

## 1. Context

ERDFlow 0.1.0 and 0.2.0 have no way to update themselves: somebody who has one
must notice a new release, download it and install it over the old. Zain asked
(2026-10-23) for an installed copy to update silently when it is opened again.

Three things were his to decide, and he decided them when asked:

1. **When the new version is put in place: when ERDFlow closes.** It is
   fetched while somebody works, and installed as ERDFlow quits, so the next
   start is the new version. Nothing interrupts work, and starting is never
   slowed.
2. **How an update is known to be his: every release is signed.** An Ed25519
   key, made for this, signs each release; ERDFlow carries the public half and
   takes no update whose signature does not hold. Checksums alone would only
   catch a broken download, not a changed release.
3. **Whether it can be turned off: yes, in Settings.** *Automatic Updates*,
   on by default, in the Home screen's Settings menu, after Theme, Icons and
   Notation.

The product must stay useful offline (*ERDFlow_PRODUCT.md*, requirement 21):
an update is something extra when the network is there, never something
waited on.

## 2. Decision

### 2.1 What is fetched

Every release already publishes `SHA256SUMS`, which names each package (with
its version and the computer it is for) and its SHA-256. The release workflow
now also signs that one file with the update key, publishing the raw 64-byte
Ed25519 signature as `SHA256SUMS.ed25519`. One signature covers every package.

An installed ERDFlow, five seconds after opening:

1. fetches `SHA256SUMS` and `SHA256SUMS.ed25519` from the newest release
   (`…/releases/latest/download/`), over HTTPS only;
2. checks the signature against the public key it carries, with Monocypher
   (`third_party/monocypher`); without a valid signature it does nothing more;
3. reads the line for its own computer — `windows-x86_64`, `macos-arm64`,
   `macos-x86_64` or `linux-x86_64` — and goes on only if that version is newer
   than itself. Package names are held to the shape ERDFlow's own are written
   in, so no name can lead outside the update's folder;
4. fetches that package into its cache folder and checks its SHA-256 against
   the signed list;
5. on macOS, copies `ERDFlow.app` out of the disk image there and then, so
   closing has only to swap it in;
6. remembers that the update is ready (`updates/stagedVersion`,
   `updates/stagedPath`).

The files are fetched with the system's own `curl`, which macOS and Windows 10
and later always have and most Linux desktops do. ERDFlow therefore carries no
networking or TLS library, and its packages' contents and licences are
unchanged. Where there is no `curl`, there are no updates.

### 2.2 What happens on closing

If an update is ready, newer than what is running, and this is the last
ERDFlow open (each open ERDFlow holds a lock file, so two open together update
once, when the last closes):

- **macOS:** a shell waits for ERDFlow to have exited, copies the new bundle in
  beside the old, and only then trades them; if anything fails, the old bundle
  is put back as it was.
- **Windows:** the Inno Setup installer runs with nothing shown
  (`/VERYSILENT /SUPPRESSMSGBOXES /NORESTART /CLOSEAPPLICATIONS
  /NORESTARTAPPLICATIONS`). It installs where the old version was, for the
  same user, and does not start ERDFlow.
- **Linux:** the new AppImage is copied in beside the running one and renamed
  over it; the running ERDFlow keeps the file it started from.

### 2.3 Which copies update

Only a copy built as a package (`ERDFLOW_PACKAGING`), and only where it was
installed:

- **macOS:** a bundle in a folder its user may write to, not run from the disk
  image and not translocated by Gatekeeper.
- **Windows:** a copy with the installer's uninstaller beside it.
- **Linux:** an AppImage in a folder its user may write to.

A build from source never updates itself. Neither does a run made with
`--smoke-test` or `--screenshot`.

### 2.4 Anything that goes wrong

Is let go without a word. A failed fetch, a bad signature, a checksum that does
not match, a full disk: the update is not made, and ERDFlow tries again the next
time it opens. The running copy is never touched until it has closed.

## 3. The key

The private half of the update key is the repository secret
`ERDFLOW_UPDATE_SIGNING_KEY` (PEM). Zain keeps a copy outside the repository.
The public half is `release_key_hex` in `infrastructure/updater.cpp`. The
release workflow checks that the secret's public half is that one before
publishing a signature, and `assemble-release.sh` checks the signature again.

**If the key is lost**, no later release can be signed for copies that carry
it: they stop updating, and must be updated by hand once, to a version that
carries a new key. **If it leaks**, whoever holds it could sign a release, but
would still need to publish it on ERDFlow's GitHub releases; a new key is then
made, and the next release, carrying it, is installed by hand once.

## 4. Consequences

- ERDFlow 0.1.0 and 0.2.0 cannot update themselves; the first version with this
  must be installed by hand. From then on, each release reaches installed
  copies on their own.
- An installed ERDFlow contacts GitHub each time it opens, unless Automatic
  Updates is turned off.
- A release without `SHA256SUMS.ed25519` is not offered to installed copies;
  they stay where they are and take the next signed one.
- Windows SmartScreen and macOS Gatekeeper are not met on updating: the files
  are fetched by ERDFlow, not a browser, and carry no download mark. Windows'
  Smart App Control, which refuses unsigned programs outright, still applies.
