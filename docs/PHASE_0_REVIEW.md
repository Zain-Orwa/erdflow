# Phase 0 — Architecture Review

**Review date:** 2026-09-14

**Outcome:** Complete. ADR-001 through ADR-013 are accepted.

This review completes the architecture work requested before continuing
implementation. Acceptance defines the design contract; it does not claim
that the corresponding features, security controls, or performance targets
have been implemented or tested. Phase 1 remained incomplete at this review.
Later implementation evidence is tracked in [implementation status](IMPLEMENTATION_STATUS.md).

## Decisions and Review Results

| Decision | Accepted direction and review result |
|---|---|
| [ADR-001](adr/ADR-001-STABLE-IDENTITY-STRATEGY.md) | Previously accepted: typed UUIDv7 identity. Updated older overview documents that still described the encoding as undecided. |
| [ADR-002](adr/ADR-002-LAYERED-ARCHITECTURE-AND-DEPENDENCY-DIRECTION.md) | Previously reviewed: inward dependencies, Qt-independent Application/Domain, and explicit desktop assembly of adapters. |
| [ADR-003](adr/ADR-003-DESKTOP-UI-TECHNOLOGY.md) | Qt 6 Widgets desktop with C++20. Accessibility checks begin with the shell. Canvas backend and minimum Qt minor version remain separate decisions. |
| [ADR-004](adr/ADR-004-CORE-LANGUAGE-STRATEGY.md) | C++20 now; Rust only with evidence. Clarified Qt parent ownership at adapters without weakening core ownership rules. |
| [ADR-005](adr/ADR-005-PROJECT-FILE-AND-PERSISTENCE-BOUNDARY.md) | Versioned `.erdx` document mapping and safe replacement. Clarified dirty-state handling during saves and preservation of unknown data during editable round trips. |
| [ADR-006](adr/ADR-006-COMMAND-UNDO-STRATEGY.md) | Qt-independent semantic commands and compact undo data. Require one execution/history path, failure-safe history recording, and notifications after successful commit. |
| [ADR-007](adr/ADR-007-BACKGROUND-WORK-CONCURRENCY-STRATEGY.md) | Single-owner mutation with isolated background computation. Added session tokens, non-reused state tokens, ordered saves, and cooperative shutdown rules. |
| [ADR-008](adr/ADR-008-CROSS-LEVEL-MAPPING-AND-PROVENANCE.md) | Explicit typed lineage and indexed lookup. Distinguished live mappings from historical baseline references needed after source deletion. |
| [ADR-009](adr/ADR-009-VALIDATION-GATES-AND-CONVERSION-POLICY.md) | Separate editing/save/conversion gates. Clarified semantic determinism with fresh IDs and identical conversion gates across display modes. |
| [ADR-010](adr/ADR-010-CHANGE-PROPAGATION-AND-REGENERATION.md) | Three-way review and explicit upstream approval. Clarified atomic target/mapping/baseline/history changes and preservation of manual edits outside the generated baseline. |
| [ADR-011](adr/ADR-011-IMPORT-ARCHITECTURE.md) | Parse → preview → review → apply. SQL import never executes input. Resource limits begin with external-input support; destination application waits for its prerequisites. |
| [ADR-012](adr/ADR-012-DATA-SOURCE-ARCHITECTURE.md) | Capability-aware, paged sources; live connections initially read-only. Clarified capability enforcement, structured SQL requests, query-result staleness, and project-local data undo/save behavior. |
| [ADR-013](adr/ADR-013-FUTURE-RUST-BOUNDARY.md) | Optional, coarse-grained Rust boundary with explicit ownership. Corrected panic recovery limits and prohibited assumptions about cross-language atomic layout. |

## Phase 0 Exit Criteria

The evidence below satisfies the architecture criteria in
[Roadmap §21](5.ERDFlow_ROADMAP.md#21-phase-0-exit-criteria).

| Required criterion | Evidence |
|---|---|
| Clear product scope | [Product](1.ERDFlow_PRODUCT.md), §§1–7 |
| Clear V1/non-V1 boundaries | Product §§43–44 |
| Accepted stable identity | ADR-001 |
| Accepted Domain/UI separation | ADR-002, ADR-003 |
| Accepted persistence boundary | ADR-005 |
| Accepted command/undo direction | ADR-006 |
| Accepted background-work direction | ADR-007 |
| Accepted mappings/provenance direction | ADR-008 |
| Accepted validation/conversion policy | ADR-009 |
| Accepted controlled propagation | ADR-010 |
| Accepted import boundary | ADR-011 |
| Accepted Data Source boundary | ADR-012 |
| Clear first implementation milestone | Product §45 and Roadmap Phases 1–17: progressively build the Conceptual Chen ERD Editor |

## Five System Qualities

The review applies [Product §4.1](1.ERDFlow_PRODUCT.md#41-system-quality-requirements):

- **Sustainable:** One initial implementation language, bounded current scope,
  deliberate dependencies, and no speculative Rust or plugin framework.
- **Scalable:** Indexed lineage, bounded input/page/history storage, measured
  snapshots, and background computation with controlled application.
- **Secure:** Untrusted inputs remain data, secrets stay out of ordinary
  project storage, and adapters validate requests and enforce capabilities.
- **Maintainable:** Clear ownership, reusable core contracts, semantic commands,
  structured diagnostics, and testable boundaries.
- **Usable:** Work-in-progress can be saved; blockers are explained; manual
  edits and undo are protected; basic accessibility starts with the shell.

These are requirements to verify as features are built, not measured outcomes
of this documentation review.

## Deferred Implementation Decisions and Gates

An accepted ADR can deliberately leave mechanics open. The following choices
are not Phase 0 blockers, but must be resolved before their dependent feature
is delivered.

| When | Required decision or evidence |
|---|---|
| Phase 1 — build foundation | Minimum supported Qt/compiler versions, warning policy, Debug/Release verification, test harness, platform build instructions, and an actual launch check. |
| Phases 3–4 — commands/domain | Minimal command/history representation, UUIDv7 generator, ownership/containers, and create/rename/delete undo tests. Phase 3 may begin with `RenameProject` on minimal metadata. |
| Phase 5 — canvas | Prototype and benchmark the canvas backend; decide layout representation and check interaction/accessibility. |
| Phase 15 — persistence | Encoding/schema, bounded decoding, migration rules, lossless editable round trips, saved-state tracking, and failed-save/round-trip fixtures. |
| First expensive operation | Executor, snapshots, session/state tokens, cancellation, and stale-result tests. Do not postpone these until Phase 38 if imports already require them. |
| Phases 18–21 — import preparation | Parser libraries, supported syntax/dialects, limits, source consistency, and bounded preview fixtures. Production apply waits for destination support. |
| Phases 22–24 — conversion/schema | Rule-specific gates, identity continuity, initial mappings returned with conversion, and full Schema import/apply/undo/save tests when the destination is available. |
| Phases 25–26 — regeneration | Baseline identity context, generated-vs-manual state, review scope, retained choices, atomic application and undo, and stale-proposal tests. Partial transactions remain unsupported until their baseline policy is defined. |
| Phases 27–31 — propagation/physical/SQL | Explicit propagation eligibility, physical types, initial dialect support, and Physical import application once its destination is ready. |
| Phases 32–33 — local data | Typed values, row identity, paging/cache limits, local persistence/undo, schema evolution, query generations, and complete CSV/JSON data import workflows. |
| Before distribution | Actual dependency/module license review, supported-platform packaging, reliability checks, and measured performance/security validation. |
| Future Rust or live writes | Separate feature-specific ADR and evidence; current acceptance does not introduce Rust, database deployment, or live write-back. |

## Technical References Checked

The review checked selected adapter details against primary documentation;
it did not select new dependencies or upgrade the project's toolchain.

- Qt's [UI overview](https://doc.qt.io/qt-6/topics-ui.html) supports the Widgets
  choice for the desktop shell; [licensing documentation](https://doc.qt.io/qt-6/licensing.html)
  leaves exact module/distribution review necessary before release.
- Qt's [object ownership documentation](https://doc.qt.io/qt-6/objecttrees.html)
  explains the parent-child lifetime model used at the UI boundary.
- [`QSaveFile`](https://doc.qt.io/qt-6/qsavefile.html) documents temporary-file
  replacement and the loss of atomicity when direct-write fallback is enabled.
- [`QUndoStack::push`](https://doc.qt.io/qt-6/qundostack.html#push) invokes `redo`,
  so a desktop adapter must avoid executing an already-applied edit twice.
- PostgreSQL's [command execution documentation](https://www.postgresql.org/docs/current/libpq-exec.html)
  distinguishes parameter values from SQL command text and provides separate
  identifier escaping. ERDFlow's adapter policy keeps those concerns separate.
- Rust's [FFI unwinding guidance](https://doc.rust-lang.org/nomicon/ffi.html#ffi-and-unwinding)
  distinguishes catchable unwinding panics from process-aborting failures.

## Verification and Next Step

Review checks cover ADR status consistency, local document links and anchors,
Markdown fence balance, changed-line whitespace, and alignment of roadmap
dependencies with accepted decisions. Runtime checks are deferred because
this step changes documentation only.

Next: Phase 1. Repair the incomplete `MainWindow` declarations, verify the
minimal build and launch, then finish the warning/test/build-documentation
requirements. The earlier setup milestone remains a historical record.
