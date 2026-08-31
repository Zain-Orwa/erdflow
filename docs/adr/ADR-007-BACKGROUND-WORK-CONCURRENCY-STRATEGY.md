# ADR-007 — Background Work / Concurrency Strategy

**Status:** Proposed  
**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** UI-thread ownership, background work, task execution, cancellation, progress, consistency, and safe application of asynchronous results

---

## 1. Context

ERDFlow is a desktop productivity application.

Some operations are expected to become expensive as projects grow:

```text
Large SQL import
Large CSV / JSON import
Conceptual → Relational conversion
Relational → Physical conversion
Validation
Auto-layout
Large save/load
Reverse engineering
Large SQL generation
Future database refresh
Future AI operations
```

These operations must not freeze the desktop interface.

At the same time, ERDFlow has mutable project state, undo/redo, mappings,
baselines, and multiple views.

Concurrency therefore cannot be treated as:

```text
"just run everything on another thread"
```

The application needs a controlled ownership model.

---

## 2. Decision

ERDFlow will use:

> **Single-owner live project mutation with background computation on consistent snapshots or isolated inputs.**

The normal model is:

```text
UI / Application Thread
        │
        │ creates task input
        ▼
Background Executor
        │
        │ pure / isolated computation
        ▼
Structured Result
        │
        ▼
Application Layer
        │
        │ validates staleness / applicability
        ▼
Apply through normal Application command
        │
        ▼
Live Project + UI refresh
```

Background workers do **not** directly mutate the live Project or Qt widgets.

---

## 3. Core Concurrency Rule

The live editable Project has one controlled mutation owner.

For the desktop application, that owner is initially the main
Application/UI coordination thread.

This keeps:

- undo/redo,
- command ordering,
- UI updates,
- project consistency,
- mapping changes,
- baseline changes

easy to reason about.

---

## 4. UI Thread Responsibilities

The UI thread owns:

```text
Qt widget interaction
rendering
mouse / keyboard events
menus
dialogs
selection
view updates
application of completed project commands
```

The UI thread should not perform known heavy work when that work can be
isolated safely.

---

## 5. Background Work Responsibilities

Background tasks may perform:

```text
parsing
validation
conversion
candidate generation
diff preparation
auto-layout
large serialization preparation
search/index building
future database querying
future AI processing
```

provided they operate on:

```text
immutable snapshot
isolated input
read-only data
worker-owned data
```

rather than the live mutable UI/project graph.

---

## 6. Why This Model

Alternative designs where many worker threads mutate the same Project would
introduce:

```text
locks around Domain objects
data races
complex undo ordering
hard-to-reproduce bugs
UI synchronization problems
mapping inconsistencies
baseline inconsistencies
```

ERDFlow does not need that complexity for the current product.

The selected model favors:

```text
parallel computation
+
serialized mutation
```

---

## 7. Task Lifecycle

A background task conceptually follows:

```text
Create Input
    ↓
Submit
    ↓
Running
    ↓
Progress / Cancellation checks
    ↓
Completed / Failed / Cancelled
    ↓
Return Structured Result
    ↓
Check Result Is Still Applicable
    ↓
Review if needed
    ↓
Apply Command
```

---

## 8. Task Input

Task input should be self-contained enough that the worker does not need
uncontrolled access to live Application state.

Examples:

```text
ConceptualModelSnapshot
ImportFileDescriptor
PhysicalModelSnapshot
LayoutGraphSnapshot
SearchSnapshot
```

---

## 9. Snapshot Principle

A snapshot represents a consistent state at a particular point in time.

Example:

```text
Conceptual Revision 42
      ↓ snapshot
Conversion Task
```

The worker computes against revision 42 even if the user later continues
editing the live model.

---

## 10. Snapshot Creation Is Part of the Performance Budget

Moving heavy work off-thread is not sufficient if building the snapshot
freezes the UI.

Therefore:

> Snapshot creation itself must be measured.

Initial implementations may use simple copies while project sizes are
moderate.

If snapshot creation becomes expensive, later strategies may include:

```text
structural sharing
immutable substructures
copy-on-write
versioned components
incremental snapshot construction
```

Do not introduce those mechanisms before profiling justifies them.

---

## 11. No Unsafe Background Copy of Live Mutable State

A worker must not simply begin traversing a mutable Project while the UI is
editing it.

Wrong:

```text
UI editing Project
      +
worker simultaneously copying Project
```

without a consistency strategy.

The task input must be made consistent before or through a safe snapshot
mechanism.

---

## 12. Structured Results

Workers return results.

They do not directly perform UI behavior.

Examples:

```text
RelationalConversionResult
ValidationResult
ImportPreview
LayoutProposal
SqlGenerationResult
PersistencePreparationResult
```

These results are ordinary structured data.

---

## 13. Result Application

A background result does not automatically mutate the Project.

Application decides:

```text
still applicable?
review required?
validation required?
user approval required?
```

Then the Application applies accepted changes through normal command rules.

---

## 14. Stale Results

A result can become stale.

Example:

```text
Conceptual revision 42
      ↓
start conversion

User edits model
Conceptual revision 43

Conversion for revision 42 finishes
```

ERDFlow must not blindly apply the old candidate to revision 43.

The result should carry enough source-version/revision information for
Application to detect this.

---

## 15. Stale Result Policy

When a result is stale, possible behavior includes:

```text
discard
offer rerun
allow review only
rebase/recompute later
```

The exact UX depends on operation type.

Default safety rule:

> Do not silently apply a result generated from an outdated source state.

---

## 16. Revision Tokens

Long-running tasks should be associated with the source state they used.

Conceptually:

```text
TaskInput
├── ProjectId
├── SourceRevision
└── Snapshot
```

Result:

```text
TaskResult
├── ProjectId
├── SourceRevision
└── Payload
```

Exact revision representation is deferred.

---

## 17. Project Identity Check

If the user closes Project A and opens Project B while a task for A is still
running, the completed task must not apply to B.

Result application must verify:

```text
ProjectId
+
relevant revision/context
```

---

## 18. Cancellation

Long-running tasks should support cancellation where practical.

Examples:

```text
large import
auto-layout
validation
conversion
reverse engineering
future database query
future AI task
```

Cancellation is cooperative.

The worker periodically checks a cancellation signal and exits safely.

---

## 19. Cancellation Safety

Cancelled work must not leave the Project half-modified.

Preferred model:

```text
worker computes isolated result
      ↓
cancelled?
      ├── yes → discard
      └── no  → return result
```

Because the worker does not mutate the live Project, cancellation is much
simpler.

---

## 20. Progress Reporting

Tasks may report progress when progress is meaningfully measurable.

Examples:

```text
Parsing 42%
Importing 64%
Validating 80%
```

If progress cannot be estimated reliably, show indeterminate progress.

Do not invent fake percentages.

---

## 21. Progress Is Presentation Data

Workers may expose structured progress information.

Presentation decides how to render it.

Examples:

```text
progress bar
status bar
task panel
spinner
```

The Domain does not know about progress widgets.

---

## 22. Error Handling

Background errors must return through structured task results.

Examples:

```text
ImportError
ConversionError
PersistenceError
Cancelled
```

Do not show:

```text
QMessageBox
```

from a worker.

Application/Presentation handles user-facing behavior.

---

## 23. Exceptions Across Task Boundary

Unhandled exceptions must not escape uncontrolled from worker execution.

The executor boundary should translate unexpected failures into a safe task
failure result or controlled exception handling path.

Exact C++ mechanics are deferred.

---

## 24. TaskRunner Port

Application should depend on a task execution abstraction.

Conceptually:

```cpp
class TaskRunner
{
public:
    // submit structured work
};
```

The exact API is not locked.

The architectural intent is:

```text
Application
      ↓
TaskRunner
      ↑
QtBackgroundExecutor
```

---

## 25. Qt Background Adapter

The initial desktop implementation may use Qt concurrency facilities.

Potential tools include:

```text
QtConcurrent
QThreadPool
QRunnable
QFuture
QPromise
```

The exact choice depends on the operation.

This ADR does not lock one mechanism for every task.

---

## 26. Why Not One Dedicated Thread Per Feature

Creating permanent threads for:

```text
import
validation
conversion
save
layout
```

would waste resources and complicate lifecycle management.

A shared bounded task execution model is preferable for ordinary
CPU/background work.

Dedicated threads may still be appropriate for special long-lived I/O or
external connection cases later.

---

## 27. Worker Pool

The initial strategy should use a bounded worker pool rather than unbounded
thread creation.

Thread count should be appropriate to the machine.

Do not hard-code:

```text
32 workers
```

for every device.

Exact scheduling policy is deferred.

---

## 28. CPU-Bound Work

Examples:

```text
validation
conversion
diff
layout
parsing
```

may benefit from worker-pool execution.

Do not parallelize internally until profiling shows value.

One background task may initially execute single-threaded.

---

## 29. I/O-Bound Work

Examples:

```text
large file read
database query
future network request
```

may use different mechanisms from CPU work.

The Application should not care about the low-level mechanism.

It sees a task/result boundary.

---

## 30. Nested Parallelism

Avoid uncontrolled nested parallelism.

Example:

```text
TaskRunner starts conversion task
conversion starts 16 more threads
each parser starts more threads
```

This can oversubscribe the machine.

Parallelism policy should remain bounded and measured.

---

## 31. Deterministic Core

Concurrency must not change deterministic modeling behavior.

For the same valid source and options:

```text
Conceptual → Relational
```

should produce the same semantic result regardless of scheduling.

Do not make result meaning depend on thread timing.

---

## 32. Deterministic Ordering

Parallel internal computation may produce nondeterministic iteration order.

Where result order matters for persistence/tests/UI consistency, normalize it
explicitly.

Do not depend on race-completion order.

---

## 33. Live Project Locking

The initial architecture should avoid fine-grained locks throughout Domain
objects.

The preferred design is:

```text
single controlled mutation owner
+
background snapshots
```

A future architecture may revisit this only if measured requirements demand
true concurrent mutation.

---

## 34. No Qt Widget Access from Workers

Qt GUI objects belong to the GUI thread.

Background tasks must not directly mutate:

```text
QWidget
QGraphicsItem
QTableView
QTreeView
```

Workers return data.

Presentation updates widgets after results reach the UI/Application context.

---

## 35. Canvas Example

Wrong:

```text
worker auto-layout
      ↓
moves QGraphicsItems directly
```

Preferred:

```text
Diagram snapshot
      ↓
background auto-layout
      ↓
LayoutProposal
      ↓
Application
      ↓
ApplyLayoutCommand
      ↓
Qt visuals update
```

---

## 36. Conversion Example

```text
Conceptual revision 25
      ↓ snapshot
Background conversion
      ↓
RelationalConversionResult
      ↓
revision still current?
      ↓
Review
      ↓
Apply approved result
```

---

## 37. Validation Example

Small incremental validation may run synchronously if it is cheap.

Large/full-project validation may run in the background.

Rule:

> Threading is based on measured cost, not feature labels.

---

## 38. Import Example

```text
User selects CSV
      ↓
Application
      ↓
background parse / inspect
      ↓
ImportPreview
      ↓
user mapping/options
      ↓
background preparation if needed
      ↓
ApplyImportCommand
```

The parser does not mutate the Project while reading.

---

## 39. Save Example

Early small saves may be synchronous if they remain instant.

For larger projects:

```text
consistent snapshot
      ↓
background mapping/serialization preparation
      ↓
safe write
      ↓
completion result
```

The exact save threading model is deferred until persistence exists.

---

## 40. Save Consistency

A saved file must correspond to one coherent Project state.

If the user edits during a background save, that edit may remain unsaved and
leave the Project dirty.

That is acceptable.

What is not acceptable is a file containing an inconsistent mixture of two
project revisions.

---

## 41. Dirty State During Background Save

Conceptually:

```text
Save starts at revision 100
User edits → revision 101
Save revision 100 completes successfully
```

Result:

```text
file contains revision 100
live project is revision 101
dirty remains true
```

This is safer than incorrectly marking revision 101 as saved.

---

## 42. Load Example

Large project loading may happen in background until it reaches the point
where Application installs the new Project.

Current Project should remain intact until candidate load succeeds.

---

## 43. Close While Task Running

When a Project closes:

```text
cancel related tasks where practical
invalidate their application context
release task ownership safely
```

Late results must not mutate destroyed state.

---

## 44. Application Shutdown

On application shutdown:

- cancel cancellable tasks,
- wait or terminate according to safe task semantics,
- do not leave unsafe writes half-completed,
- do not allow callbacks into destroyed UI.

Exact shutdown policy is implementation-specific.

---

## 45. Task Ownership

Every task should have an identifiable owner/context.

Examples:

```text
ProjectId
Application session
Import dialog workflow
```

This makes cancellation and result routing safer.

---

## 46. Task IDs

ERDFlow may use task identifiers for:

```text
progress
cancellation
diagnostics
task panel
```

Task IDs are operational identity.

They are distinct from semantic Project object IDs.

Exact representation is deferred.

---

## 47. Task Priority

Priority scheduling is not required initially.

Possible later priorities:

```text
interactive
normal
low-priority maintenance
```

Only add if real contention appears.

---

## 48. Debouncing

Some expensive operations may be triggered frequently by edits.

Example:

```text
full validation after every keystroke
```

Instead, Application may:

```text
debounce
cancel previous task
run latest state
```

This prevents wasted work.

---

## 49. Latest-Wins Pattern

For operations such as live validation/search:

```text
Task A starts for revision 10
Task B starts for revision 11
```

If A finishes after B:

```text
A is stale
```

and should be ignored.

This is a useful pattern for UI responsiveness.

---

## 50. Conversion Is Not Latest-Wins Automatically

A conversion may involve explicit user review.

A stale conversion result should normally be clearly invalidated/re-run
rather than silently replaced while the user is reviewing it.

Task policy depends on workflow semantics.

---

## 51. No Hidden Mutation During Analysis

Operations named:

```text
validate
analyze
preview
generate candidate
calculate layout
```

should be read-only with respect to the live Project.

Mutation occurs only through a deliberate apply step.

---

## 52. Two-Phase Heavy Operations

Preferred:

```text
Phase 1:
background compute

Phase 2:
controlled apply
```

Examples:

```text
Import
Regeneration
Auto-layout
AI-assisted generation
```

This aligns with ADR-006.

---

## 53. Background Task and Undo

Background computation itself is not normally an undo history entry.

The eventual applied change is.

Example:

```text
Generate candidate
→ no undo entry

Apply candidate
→ undoable command
```

---

## 54. Background Task and Baselines

A worker may read baseline snapshots.

It must not update baseline state directly.

Accepting a new baseline is a controlled Application operation.

---

## 55. Background Task and Mappings

A conversion worker may produce candidate mappings/provenance.

Those mappings become live project state only when the corresponding
candidate is accepted/applied.

---

## 56. Data Workspace

Future database-backed Data Sources may perform I/O asynchronously.

Examples:

```text
fetch rows
filter
sort
refresh
```

The Data Grid should receive results without blocking the UI.

Remote Data Source behavior is separate from Domain mutation.

---

## 57. Remote Row Editing

Future remote row edits may require:

```text
optimistic UI
pending state
database transaction
error recovery
```

This ADR does not define that workflow.

The TaskRunner/DataSource architecture must not make it impossible.

---

## 58. Future AI

AI requests are naturally background operations.

Conceptually:

```text
Model context
      ↓
AI adapter
      ↓
Proposal
      ↓
Validation
      ↓
User review
      ↓
Apply command
```

AI never mutates Project state directly from a background callback.

---

## 59. Future Rust

If selected core modules later move to Rust, their execution can still fit
the same task model:

```text
snapshot/input
      ↓
Rust core call
      ↓
structured result
```

The concurrency architecture does not depend on the implementation language
of the worker.

---

## 60. Future VS Code Extension

A future extension may use a different async runtime.

That is acceptable.

The reusable Application/Core contract should remain conceptually:

```text
input
→ async/scheduled operation
→ structured result
→ controlled apply
```

Qt-specific threading is not the product's semantic concurrency model.

---

## 61. Alternative A — Everything on UI Thread

Rejected.

Simple initially, but heavy operations would freeze:

```text
rendering
input
window movement
feedback
```

and produce poor desktop UX.

---

## 62. Alternative B — Shared Mutable Project with Locks Everywhere

Rejected for initial architecture.

It adds:

- locking complexity,
- deadlock risk,
- data races,
- difficult undo semantics,
- difficult testing.

ERDFlow does not currently need concurrent live mutation.

---

## 63. Alternative C — Actor Model for Every Domain Object

Rejected as unnecessary complexity.

Entity, Attribute, Relationship, etc. do not each need asynchronous mailboxes.

The application is not a distributed actor system.

---

## 64. Alternative D — Dedicated Thread Per Project

Not required now.

A project may have many different kinds of tasks and idle periods.

A general bounded executor is simpler.

A future architecture can revisit if project-scale workloads demand it.

---

## 65. Alternative E — `std::async` Everywhere

Not selected as the architectural contract.

It can be useful, but ERDFlow needs:

```text
task ownership
progress
cancellation
UI integration
bounded execution
```

so a deliberate task abstraction is more appropriate.

---

## 66. Alternative F — QtConcurrent as Core Contract

Rejected.

QtConcurrent may implement the desktop adapter, but Application/Core should
not require Qt concurrency types.

---

## 67. Alternative G — Coroutines Everywhere

C++20 coroutines may eventually be useful for selected async workflows.

They are not selected as the universal concurrency model.

Coroutines solve syntax/control flow; they do not by themselves solve:

```text
thread ownership
task scheduling
cancellation semantics
Project consistency
```

---

## 68. Consequences — Positive

This decision gives ERDFlow:

- responsive UI,
- simpler project ownership,
- fewer data races,
- safe cancellation,
- clear undo integration,
- stale-result protection,
- reusable core operations,
- compatibility with future Rust/VS Code implementations,
- measured path to optimization.

---

## 69. Consequences — Costs

This introduces:

- snapshot/result structures,
- task coordination,
- cancellation logic,
- revision checks,
- more explicit workflow states,
- some copy/memory overhead.

These are acceptable trade-offs for correctness and responsiveness.

---

## 70. Scale Considerations

Known memory risk:

```text
Live Project
+
Task Snapshot
+
Generated Candidate
+
Mappings
+
Baseline
+
Undo Data
```

This can become substantial.

SCALE.md requires measuring this peak-memory scenario.

Do not assume background execution is free.

---

## 71. Snapshot Memory

The first implementation may copy simple model structures.

Later, if memory becomes a bottleneck, evaluate:

```text
structural sharing
immutable nodes
copy-on-write
compact snapshots
```

based on measurements.

---

## 72. Cancellation Frequency

Cancellation checks should be frequent enough to feel responsive but not so
frequent that they dominate computation.

Exact granularity is algorithm-specific.

---

## 73. Progress Frequency

Progress events should be throttled if needed.

Do not send thousands of UI updates per second.

Progress reporting itself can become a performance problem.

---

## 74. Thread-Safe Logging

Background diagnostics may use a thread-safe logging adapter.

Logs must remain separate from project mutation.

Avoid logging sensitive data.

---

## 75. Testing Strategy

Concurrency behavior requires deterministic tests where possible.

Test the logic around:

```text
stale results
cancellation
project close
result application
revision mismatch
error propagation
```

without relying only on timing sleeps.

---

## 76. First Concurrency Tests

### Test 1 — Result Applies When Current

```text
Task source revision = 5
Live revision = 5
→ result eligible
```

### Test 2 — Stale Result Rejected

```text
Task source revision = 5
Live revision = 6
→ result not silently applied
```

### Test 3 — Wrong Project Rejected

```text
Task ProjectId = A
Current ProjectId = B
→ no application
```

### Test 4 — Cancellation

```text
cancel task
→ no live Project mutation
```

### Test 5 — Error

```text
worker fails
→ structured error
→ Project unchanged
```

### Test 6 — Apply Through Command

```text
candidate accepted
→ normal Application command
→ undoable where appropriate
```

---

## 77. Initial Implementation Scope

Do not build a large task framework before heavy work exists.

At first:

```text
main/UI thread owns Project mutation
```

When the first genuinely heavy operation arrives, introduce the minimal
TaskRunner abstraction.

Likely first candidates:

```text
large import
full validation
conversion
```

---

## 78. Qt Implementation Direction

When needed, begin with a small Qt-backed executor.

Possible path:

```text
QThreadPool
+
QtConcurrent / QRunnable
+
future/promise result delivery
```

Choose based on the actual first workload.

Do not mix several mechanisms without need.

---

## 79. No Premature Internal Parallelism

Example:

```text
Conceptual → Relational
```

should first be implemented correctly and measured.

Only then decide whether the conversion itself needs to use multiple worker
threads internally.

Background execution and internal parallelism are different concerns.

---

## 80. Revision Strategy Dependency

This ADR assumes ERDFlow can identify relevant source revisions.

The exact model revision mechanism is still open.

Possible approaches:

```text
monotonic revision counter
component revision
generation token
immutable snapshot identity
```

This will be selected when implementation needs stale-result detection.

---

## 81. Architecture Invariants

### Invariant 1

The live Project has one controlled mutation owner.

### Invariant 2

Background workers do not directly mutate Qt widgets.

### Invariant 3

Background workers do not directly mutate the live Project.

### Invariant 4

Heavy operations use consistent snapshots or isolated inputs.

### Invariant 5

Background results are structured data.

### Invariant 6

Results are checked for applicability before mutation.

### Invariant 7

Stale results are not silently applied.

### Invariant 8

Cancellation leaves the live Project valid.

### Invariant 9

Applied changes go through normal Application command rules.

### Invariant 10

Task execution technology is an adapter concern, not Domain meaning.

### Invariant 11

Snapshot creation itself is part of the performance budget.

### Invariant 12

Thread count and parallelism are bounded and measurement-driven.

---

## 82. Relationship to Other ADRs

Depends on:

```text
ADR-001 — Stable Identity Strategy
ADR-002 — Layered Architecture and Dependency Direction
ADR-003 — Desktop UI Technology
ADR-004 — Core Language Strategy
ADR-005 — Project File and Persistence Boundary
ADR-006 — Command / Undo Strategy
```

Later ADRs build on it:

```text
ADR-008 — Cross-Level Mapping and Provenance
ADR-009 — Validation Gates and Conversion Policy
ADR-010 — Change Propagation and Regeneration
ADR-011 — Import Architecture
ADR-012 — Data Source Architecture
```

---

## 83. Open Implementation Decisions

This ADR intentionally leaves open:

- exact `TaskRunner` C++ API,
- exact Qt concurrency classes,
- exact worker-pool size,
- exact task priority system,
- exact revision token representation,
- exact snapshot implementation,
- exact progress event mechanism,
- exact cancellation token implementation,
- exact background save strategy,
- exact coroutine usage,
- exact future Rust async integration.

These should be decided when real workloads exist.

---

## 84. Decision Outcome

ERDFlow adopts:

```text
single-owner live mutation
+
background computation
+
consistent snapshots / isolated inputs
+
structured results
+
revision/staleness checks
+
cooperative cancellation
+
controlled Application apply
```

with Qt concurrency used only as the desktop execution adapter.

---

## 85. Final Principle

> Compute in the background. Mutate deliberately. Never let concurrency make project meaning ambiguous.
