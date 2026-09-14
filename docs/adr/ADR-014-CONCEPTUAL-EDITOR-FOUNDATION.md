# ADR-014 — Conceptual Editor Foundation

**Status:** Accepted  
**Date:** 2026-09-14

## Context and decision

The first useful Part 1 slice needs a real canvas, stable identity, undoable
commands, and files that retain its diagrams. ADR-001–006 deliberately left the
concrete mechanics open. This decision records the implemented choices.

- Use Qt 6.9+ and `QUuid::createUuidV7()` in an Infrastructure ID generator.
  Domain IDs remain typed 128-bit values with no Qt types.
- Use `QGraphicsView`/`QGraphicsScene` and custom Chen items. Compare the current
  project against projected IDs at command boundaries. During pointer movement,
  update only edges incident to moved nodes; commit one layout delta on release.
- Store entities, attributes, relationships and layout in ordered ID maps. Keep
  stable participant records inside relationships. This provides predictable
  iteration and lookup without maintaining duplicate speculative indexes.
- Use named Application operations with per-object map-node deltas. The same
  delta toggles apply/undo/redo without allocating replacement nodes. Prepare
  history storage before mutation, validate, and roll back rejected edits. Keep
  one Application history with a conservative 32 MiB accounting budget.
- Let QAction adapt that history directly. A second QUndoStack would introduce
  duplicate ownership/execution without adding needed behavior to this slice.
- Map the single conceptual project to strict versioned UTF-8 JSON `.erdx`
  documents. Use bounded decoding and `QSaveFile` with direct-write fallback
  disabled. Reject unknown fields/versions rather than silently discarding them.
- Save/load are synchronous and bounded for this initial slice. Large work,
  cancellation, and background snapshots require further measurements and the
  ADR-007 session/state safeguards before being introduced.

## Scope and sequencing

Basic Chen semantics were built on the command/domain foundation. Single-page
persistence from Phase 15 is included before weak/ISA/associative semantics so
this working editor can protect users' drawings. This is a deliberate change in
execution sequence, not completion of all intervening roadmap phases.

Only Conceptual is exposed as a working workspace. Navigation to future stages
will be added alongside actual destinations. Attribute kinds are exclusive in
format version 1; partial keys and combined kinds require a subsequent semantic
and compatibility decision. No speculative full-project schema is serialized.

## Alternatives and consequences

Qt Quick or a custom renderer would add a second UI stack or basic interaction
infrastructure. The tested QGraphicsView prototype is sufficient for the current
slice; its benchmark remains a baseline, not an unlimited scale guarantee.

Whole-project snapshots for each undo would copy unrelated objects. Map-node
deltas retain affected objects, while full structural validation keeps early
rules centralized. Incremental validation is deferred until profiling justifies
its complexity. Explorer rebuilding and synchronous file work remain known
scale limits.

A ZIP/container format has no current asset/multi-page requirement. JSON gives
inspectable fixtures and straightforward validation. This format is deliberately
versioned so future changes can be reviewed rather than accepted lossily.

## Evidence

See [implementation status](../IMPLEMENTATION_STATUS.md),
[development and verification](../DEVELOPMENT.md),
[performance baseline](../PERFORMANCE_BASELINE.md), and
[format specification](../ERDX_FORMAT.md). Regression suites exercise core,
persistence, canvas, and the integrated desktop.

Primary Qt references: [UUIDv7 generator](https://doc.qt.io/qt-6/quuid.html#createUuidV7),
[QGraphicsView](https://doc.qt.io/qt-6/qgraphicsview.html), and
[QSaveFile](https://doc.qt.io/qt-6/qsavefile.html).
