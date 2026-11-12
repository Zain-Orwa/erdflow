# ADR-025 — Update and Restart

> **Settled work.** Do not change, replace or re-style anything in this
> document to suit something new you have been asked to build. If what you are
> building genuinely contradicts what is written here, stop and ask Zain, who
> owns this project: say what you want to change, what the application will
> look like afterwards, and whether it is a gain or a loss. He decides.
> Full rule: [CLAUDE.md](../../CLAUDE.md).

**Status:** Accepted (Zain, 2026-11-12: asked for it, and for it to be released once built)

**Date:** 2026-11-12
**Project:** ERDFlow
**Decision Scope:** How an installed ERDFlow says that an update is ready, and how the update is taken at once

**Adds to:** ADR-024 (automatic updates), which stays exactly as it is

---

## 1. Context

ERDFlow 0.3.0 and 0.3.1 update themselves without a word (ADR-024): a newer
release is fetched while somebody works, and put in place when ERDFlow closes.

On 2026-11-12, with 0.3.1 fetched and waiting on his own computer, Zain found
that nothing said so. The old version stayed on screen until ERDFlow was quit
completely and opened again, and nothing told him to do that. He asked for a
small message at the top of the window, as other applications show one, there
only while an update is ready; pressed, ERDFlow restarts as the new version.
Nothing else was to change.

## 2. Decision

### 2.1 What is shown

A small button, **Update and Restart**, with a turning arrow before its words.

- **Only while an update is waiting.** That is: a newer release has been
  fetched, its signature and checksum have held, its files are still there,
  and Automatic Updates is on. These are the conditions under which closing
  ERDFlow would put it in place (ADR-024 §2.2). The rest of the time the
  button is not there at all, and nothing stands in its place.
- **At the top of the window, at the right.** On the Home screen it stands on
  Home's bar, after the name and before the way back into the workspace and
  Theme. In a workspace it stands at the right end of the ribbon's row of
  tabs. The two rows are never showing together, so neither are the two
  buttons.
- **Laid over the row, in room the row was not using.** It is not added to
  what either row lays out. Nothing already standing there is moved, resized
  or reordered, and neither row is any taller. The ribbon's tabs remain
  exactly File, Home and Settings.
- **Drawn in the theme's own primary,** at the strength lettering reads on,
  so under Plain it is grey. Where a row is too narrow for its words it keeps
  its mark alone; where there is no room for that either, it stays away
  rather than cover anything.
- **Its hint names the version** and says what pressing does, that unsaved
  changes are asked about first, and that closing ERDFlow by hand updates it
  just the same.

Nothing else is shown: no dialog, no badge, no sound. A fetch that fails is
still let go without a word (ADR-024 §2.4).

### 2.2 What pressing does

ERDFlow closes exactly as it does when it is closed by hand. Unsaved work is
asked about — Save, Discard or Cancel — and Cancel keeps ERDFlow open with
nothing done.

Closing is what puts the update in place, by the steps of ADR-024 §2.2,
unchanged. The one thing more is that ERDFlow is opened again afterwards:

- **macOS:** the same shell that trades the bundles then opens the installed
  one (`open -n`). If the new bundle could not be put in place, the old one
  is put back as before, and is what opens.
- **Windows:** the installer is run with the same words as before and one
  more, `/RELAUNCH=1`. The installer's own script
  (`packaging/windows/erdflow.iss.in`) starts the ERDFlow it has just
  installed when it is given that word, and starts nothing without it.
- **Linux:** once the new AppImage has been renamed over the old, the
  installed AppImage is started. Where the one that was running had to be
  unpacked to run, on a system that does not let an AppImage be mounted, the
  new one is told to unpack itself too (`APPIMAGE_EXTRACT_AND_RUN`).

Two things can stop it, and each is said in a notice under the button:

- **Another ERDFlow is open.** An update waits for the last ERDFlow to close
  (ADR-024 §2.2), so it cannot be put in place from this one. Nothing is
  closed.
- **What was ready has gone,** for instance because its files were cleared
  away. The button is taken away; ERDFlow fetches the update again the next
  time it opens.

### 2.3 What stays as it was

- How an update is found, fetched, checked and made ready (ADR-024 §2.1).
- The *Automatic Updates* switch in Home's Settings. Turned off, nothing is
  put in place, so the button is away; turned on again, it is back.
- Closing ERDFlow by hand: the update is put in place silently, and ERDFlow
  is not opened again.
- Which copies update (ADR-024 §2.3). A build from source never updates
  itself and never shows the button, except when started with the
  development option `--update-ready <version>`, which shows it so that it
  can be looked at and photographed. Pressed there, it says there is no
  update to put in place and goes away.

## 3. Consequences

- **The release that first carries this reaches installed copies the old
  way.** ERDFlow 0.3.0 and 0.3.1 have no button to show: they fetch that
  release silently and take it when they are closed. From the release after
  it, installed copies show the button.
- The installer that runs for an update is always the newer release's own, so
  from this release on every installer understands `/RELAUNCH=1`. An
  installer that did not would update ERDFlow and leave it closed.
- ERDFlow still looks for a release once each time it opens (ADR-024 §2.1). A
  copy left open for days learns of a release only when it is next opened.
- If the update cannot be put in place after the button is pressed, ERDFlow
  comes back as the version it was (macOS and Linux), and the button comes
  out again the next time it looks.

## 4. Where it is built and checked

- `infrastructure/updater.{hpp,cpp}`: `Updater::waiting`, `on_looked`,
  `others_open`, `open_again`, and `silent_install_arguments`.
- `app/desktop/update_button.{hpp,cpp}`: the button.
- `app/desktop/main_window.{hpp,cpp}`: `offer_update`, `update_and_restart`,
  `say_about_update`, `update_button`.
- `app/desktop/main.cpp`: tells the window what is waiting after each look,
  and what pressing does.
- Tests: the `update` suite (what is waiting, another ERDFlow open, the
  installer's words, and — on macOS and Linux — an update put in place and
  ERDFlow opened again or not); the desktop window-tools part (the button on
  both rows, nothing moved, the Settings switch, the notice).
- The package workflow runs the Windows installer as an update asked to open
  ERDFlow again runs it, and checks that ERDFlow is opened.
