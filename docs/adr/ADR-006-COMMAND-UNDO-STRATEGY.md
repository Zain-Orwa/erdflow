# ADR-006 — Command / Undo Strategy

**Status:** Proposed  
**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** User-driven mutations, semantic commands, undo/redo behavior, command grouping, and Qt integration

---

## 1. Context

ERDFlow is an interactive database-design application.

Users will perform many reversible edits such as:

```text
Create Entity
Rename Entity
Delete Entity
Move Diagram Element
Add Attribute
Rename Attribute
Create Relationship
Change Cardinality
Change Participation
Edit Relation
Add Foreign Key
Add Index
```

The product also includes more complex operations such as:

```text
Import
Conceptual → Relational conversion
Regeneration review
Apply approved change proposal
Controlled Schema → Conceptual propagation
```

These edits must be:

- testable,
- undoable where appropriate,
- independent of Qt,
- safe across modeling stages,
- compatible with stable IDs,
- compatible with future mappings/provenance,
- usable by more than one frontend later.

Therefore ERDFlow needs a deliberate command architecture.

---

## 2. Decision

ERDFlow will use:

> **Application-level semantic commands as the authoritative mutation mechanism for user-editable project state.**

Undo/redo will operate on those semantic changes.

Qt's Undo Framework may be used as a desktop integration adapter, but:

> **ERDFlow commands must not inherit from or depend on `QUndoCommand` as their core representation.**

Conceptually:

```text
User Action
    ↓
ERDFlow Application Command
    ↓
Domain Mutation
    ↓
Command Result / Change Record
    ↓
Qt Undo Adapter
    ↓
QUndoStack
```

---

## 3. Why Commands

A command gives a user action an explicit semantic name.

Example:

```text
RenameEntity
```

is much clearer than:

```text
"some object graph changed"
```

Commands help ERDFlow understand:

- what changed,
- which object changed,
- whether the change is undoable,
- whether it may propagate,
- what to show in history,
- how to test the behavior,
- how to merge repeated edits.

---

## 4. Command vs Direct Mutation

Wrong:

```text
Properties panel edits Entity.name directly
```

Preferred:

```text
Properties panel
      ↓
RenameEntityCommand
      ↓
Application
      ↓
Domain
```

This creates one controlled mutation path.

---

## 5. Command Scope

Commands are Application-layer concepts.

They coordinate Domain changes.

Examples:

```text
CreateEntityCommand
RenameEntityCommand
DeleteEntityCommand
AddAttributeCommand
CreateRelationshipCommand
ChangeCardinalityCommand
```

The Domain still defines what constitutes a valid state.

The Application command does not replace Domain rules.

---

## 6. Command Naming

Command names should reflect user intent.

Good:

```text
RenameEntity
ChangeCardinality
AddAttribute
DeleteRelationship
```

Less desirable:

```text
SetStringProperty
UpdateObject
ModifyNode
```

Generic mutation names lose semantic meaning.

---

## 7. Command Input

Commands should carry only the input required to perform the action.

Example:

```text
RenameEntityCommand
├── EntityId
└── NewName
```

The command should not need a pointer to a Qt widget.

Wrong:

```text
RenameEntityCommand(QWidget*)
```

---

## 8. Stable IDs in Commands

Commands should refer to semantic objects through stable IDs.

Example:

```text
EntityId = E
```

not:

```text
entityName = "Student"
```

as identity.

This follows ADR-001.

---

## 9. Command Result

A command may return a structured result.

Possible information includes:

```text
success/failure
validation issues
affected IDs
change metadata
undo data
propagation eligibility
```

The exact result type is deferred.

---

## 10. Undo Principle

Undo should reverse the effect of a completed user action.

Example:

```text
Rename:
Student → UniversityStudent

Undo:
UniversityStudent → Student
```

Stable identity remains unchanged.

---

## 11. Redo Principle

Redo reapplies the same semantic operation.

Example:

```text
Undo Rename
      ↓
Redo
      ↓
UniversityStudent restored
```

Redo must not create new semantic IDs where the original action did not.

---

## 12. Delete + Undo

Deleting an Entity and undoing the action restores the same semantic object.

Example:

```text
EntityId = E
delete
undo
EntityId = E
```

This follows ADR-001.

---

## 13. Create + Undo + Redo

Create:

```text
EntityId = E
```

Undo removes it.

Redo restores:

```text
EntityId = E
```

Redo does not generate a second EntityId.

---

## 14. Move Commands

Layout movement is undoable.

Example:

```text
MoveDiagramElementCommand
├── ElementId
├── OldPosition
└── NewPosition
```

Movement changes layout state.

It does not change semantic database identity.

---

## 15. Command Categories

ERDFlow may distinguish command categories.

### Semantic Commands

Examples:

```text
RenameEntity
AddAttribute
ChangeCardinality
```

### Layout Commands

Examples:

```text
MoveElement
ResizeElement
MoveConnectorBend
```

### Project Commands

Examples:

```text
RenameProject
ChangeProjectSetting
```

### Bulk Commands

Examples:

```text
ApplyImport
ApplyRegenerationProposal
```

The exact enum/type is deferred.

---

## 16. Undoable vs Non-Undoable Operations

Not every operation should necessarily be represented as a normal undo step.

Examples likely undoable:

```text
create
delete
rename
move
change property
apply approved model change
```

Examples usually not normal project-history undo items:

```text
Save Project
Open Project
Export PNG
Copy generated SQL to clipboard
```

These do not mutate the semantic working project in the same way.

---

## 17. Save Is Not an Undo Command

Saving does not create a new semantic project state.

It persists the current state.

Therefore:

```text
Save
```

should not appear as:

```text
Undo Save
```

---

## 18. Open Is Not an Undo Command

Opening another project replaces the active project/session context.

Normal undo history should not cross unrelated project sessions.

---

## 19. Undo History Is Project-Scoped

Undo/redo history belongs to the active project/session.

Conceptually:

```text
Project A
└── Undo History A

Project B
└── Undo History B
```

Multi-project UI is deferred, but the history should not be designed as uncontrolled global state.

---

## 20. Command Execution Flow

Recommended flow:

```text
Presentation
      ↓
Application Command
      ↓
Precondition / Validation
      ↓
Domain Mutation
      ↓
Change Record / Undo Data
      ↓
History
      ↓
Presentation Refresh
```

---

## 21. Validation Before Mutation

Commands should validate applicable preconditions before committing invalid state.

Example:

```text
RenameEntity
```

may validate:

```text
Entity exists
new name is acceptable
```

Domain invariants remain authoritative.

---

## 22. Validation After Mutation

Some operations may require post-mutation validation.

Example:

```text
ChangeCardinality
```

may result in:

```text
new warning
new conversion blocker
```

This does not necessarily mean the edit itself is invalid.

The command can succeed while returning validation information.

---

## 23. Work-in-Progress Models

ERDFlow allows incomplete models.

Therefore command validation must not confuse:

```text
"not conversion-ready"
```

with:

```text
"illegal project state"
```

An Entity may exist without final conversion metadata.

The user must still be able to edit and save it.

---

## 24. Command State

There are two broad implementation approaches:

### Inverse Command

Store enough information to perform the reverse operation.

Example:

```text
Rename:
old name
new name
```

### Snapshot/Patch

Store a before/after change representation.

ERDFlow should prefer compact semantic inverse/delta information where practical.

---

## 25. Avoid Full-Project Copies Per Command

Wrong default:

```text
Every command stores:
entire Project before
+
entire Project after
```

This would scale poorly.

Prefer:

```text
only affected data
```

Example:

```text
RenameEntity
├── EntityId
├── oldName
└── newName
```

---

## 26. Large Commands

Some operations affect many objects.

Examples:

```text
Import
Apply Regeneration Proposal
Bulk Delete
```

For these, a larger patch/delta may be appropriate.

Do not force every operation into tiny per-field commands if that destroys atomicity.

---

## 27. Atomic User Actions

One meaningful user action should normally correspond to one undo step.

Example:

```text
drag Entity from A to B
```

should become:

```text
one Move command
```

not hundreds of commands for each mouse-move event.

---

## 28. Command Compression

Repeated updates may be merged.

Example:

```text
Move x=100→101
Move x=101→102
Move x=102→103
```

during one drag can become:

```text
Move x=100→103
```

Similarly, rapid text editing may later support sensible merge behavior.

---

## 29. Merge Criteria

Commands should merge only when they clearly represent one continuous user action.

Possible requirements:

```text
same command type
same target ID
same editing session/gesture
compatible timing/state
```

Do not merge unrelated edits merely because they are adjacent.

---

## 30. Transaction / Composite Command

Some user actions affect several objects and should undo together.

Example:

```text
Delete Entity
```

may also remove:

```text
connected relationships
relationship participants
layout references
```

These changes should be one atomic command.

---

## 31. Composite Command Example

Conceptually:

```text
DeleteEntityCommand
    ↓
remove Entity
remove associated Attribute links
remove affected Relationship participants
remove layout references
```

Undo restores the complete consistent state.

---

## 32. Cross-Level Commands

Later, a reviewed change may affect multiple modeling levels.

Example:

```text
Apply approved Schema → Conceptual propagation
```

This should appear as one meaningful user action when appropriate.

The command may internally modify several model objects.

---

## 33. Controlled Propagation and Undo

Example:

```text
Schema Attribute:
Name → FullName

User approves Conceptual propagation
```

The accepted operation may produce:

```text
Schema rename
+
Conceptual rename
```

Undo should restore both parts consistently if they were accepted as one atomic action.

---

## 34. Regeneration Review and Undo

Applying an accepted regeneration proposal may modify many relations/attributes.

That application should be undoable as one reviewed operation unless there is strong UX evidence for finer granularity.

Conceptually:

```text
Apply Regeneration Proposal
      ↓
one history entry
```

---

## 35. Import and Undo

Applying an import may create many objects.

Preferred behavior:

```text
Preview Import
      ↓
Apply
      ↓
one atomic undoable import action
```

Undo removes the imported change set.

The parser itself is not an undo command.

---

## 36. Background Work and Commands

Long-running analysis may happen before mutation.

Example:

```text
Generate candidate
```

can run in background.

Only after review:

```text
Apply Candidate Command
```

mutates the live project and enters undo history.

This is safer than letting a worker mutate project state continuously.

---

## 37. Two-Phase Operations

Complex operations should often be:

```text
Analyze / Generate
      ↓
Review
      ↓
Apply Command
```

Examples:

```text
Import
Regeneration
AI proposal
Auto-layout proposal
```

The proposal stage is not necessarily an undoable mutation.

The apply stage is.

---

## 38. Command History Labels

History entries should have meaningful labels.

Examples:

```text
Rename Entity
Add Attribute
Move Student
Change Relationship Cardinality
Apply Schema Regeneration
Import CSV
```

These can be shown in future Edit menus/history UIs.

---

## 39. Qt Undo Framework Integration

Qt provides:

```text
QUndoCommand
QUndoStack
QUndoView
```

ERDFlow may use them at the desktop integration layer.

But the core command model remains Qt-independent.

---

## 40. Why Not Use QUndoCommand Directly Everywhere

If every ERDFlow command inherits from `QUndoCommand`:

```text
Application
      ↓
Qt
```

The reusable Application layer becomes tied to the desktop framework.

That conflicts with ADR-002.

It also makes future:

```text
VS Code
CLI
Rust core
```

reuse harder.

---

## 41. Qt Adapter Direction

Conceptually:

```text
ERDFlow Command
      ↓
QtUndoCommandAdapter
      ↓
QUndoStack
```

The adapter may call:

```text
execute
undo
redo
```

on ERDFlow behavior.

Exact design is deferred.

---

## 42. Alternative Integration

The Application may own a framework-independent command history and expose state to Qt.

Then Qt menus/actions only call:

```text
application.undo()
application.redo()
```

This may be cleaner than wrapping every command in `QUndoCommand`.

Both approaches remain possible.

This ADR locks independence, not the exact adapter structure.

---

## 43. Command Interface

A future conceptual interface may resemble:

```cpp
class ICommand
{
public:
    virtual ~ICommand() = default;

    virtual CommandResult execute() = 0;
    virtual CommandResult undo() = 0;
};
```

But ERDFlow should not prematurely force every command into runtime inheritance.

Alternatives include:

```text
value-type commands
variants
templated command handlers
explicit use-case classes
```

Exact C++ mechanics are deferred.

---

## 44. Avoid Interface Ceremony

The architecture needs semantic commands.

It does not require:

```text
ICommandFactory
ICommandBuilder
ICommandRepository
ICommandDispatcherFactory
```

unless real needs appear.

Keep the first implementation simple.

---

## 45. Command Handler Pattern

Another possible implementation:

```text
RenameEntity
      ↓
RenameEntityHandler
```

This is acceptable if it improves separation.

Not required initially.

The core decision remains explicit semantic operations.

---

## 46. Mutation Ownership

Widgets should not own mutation logic.

Example wrong:

```cpp
void PropertiesWidget::onNameChanged(...)
{
    project.entities()[index].name = ...;
}
```

Preferred:

```text
PropertiesWidget
      ↓
Application::renameEntity(...)
```

which constructs/dispatches the command.

---

## 47. Read Paths May Be Different

Not every read operation needs a command.

Examples:

```text
Get Entity properties
Search entities
Display relation list
```

can use read-oriented queries/view models.

Commands are for meaningful mutation.

---

## 48. Command/Query Separation

ERDFlow may conceptually distinguish:

```text
Commands → change state
Queries  → read state
```

This does not imply adopting a heavy CQRS architecture.

It is simply a useful separation of responsibility.

---

## 49. No Event Sourcing Requirement

ERDFlow is **not** adopting event sourcing.

Command history is not automatically the permanent source of truth.

The authoritative project state remains the Project model persisted in `.erdx`.

Undo history may be session-scoped and bounded.

---

## 50. Why Not Event Sourcing

Full event sourcing would require:

```text
permanent event log
replay semantics
event versioning
migration
compaction
event consistency
```

This is unnecessary for ERDFlow's current desktop product.

Future collaboration may revisit operation logs separately.

---

## 51. Undo Persistence

Normal undo history does not need to persist across application restarts in V1.

Example:

```text
save
close app
reopen
```

does not require restoring the previous undo stack.

This keeps `.erdx` simpler.

Persistent project state and transient editing history are separate.

---

## 52. Future Persistent History

A future history/audit feature may preserve selected operations.

That would be a new product decision.

Do not accidentally make the current undo stack into a permanent audit log.

---

## 53. Undo History Bounds

Undo history must eventually be bounded.

Possible limits:

```text
memory budget
command count
large-command policy
```

SCALE.md requires considering payload size, not only number of commands.

---

## 54. Memory-Aware History

A command may be tiny:

```text
Rename Entity
```

or large:

```text
Apply Import
```

Therefore:

```text
100 commands
```

is not a meaningful memory bound by itself.

Future history limits should consider estimated payload cost.

---

## 55. Large Undo Payloads

Potential large payloads include:

```text
imported objects
mapping changes
baseline changes
bulk deletions
regeneration patches
```

Large operations may store compact deltas rather than full snapshots.

---

## 56. Stable IDs Reduce Undo Payload Complexity

Because commands can refer to:

```text
EntityId
AttributeId
RelationId
```

they do not need to rely on fragile container positions.

This makes undo more robust after unrelated container reorderings.

---

## 57. Command Preconditions

A command should fail safely if its target no longer exists or its assumptions are invalid.

Example:

```text
RenameEntity(E)
```

but E is absent.

Return a structured failure.

Do not dereference stale pointers.

---

## 58. Redo Preconditions

Redo should also validate that the operation can be safely reapplied.

Normally, because undo restored the prior expected state, redo should be valid.

But defensive checks are still useful.

---

## 59. Undo Failure

Undo failure should be rare and treated seriously.

If a command cannot restore a valid previous state, the history/system design likely has a bug.

The UI must not silently continue as if undo succeeded.

---

## 60. Command Side Effects

Commands should avoid hidden external side effects where possible.

Example:

```text
RenameEntityCommand
```

should not unexpectedly write files.

External actions such as Save remain separate use cases.

---

## 61. Notifications / Events

After a command succeeds, Application may expose change notifications.

Examples:

```text
EntityCreated
EntityRenamed
RelationshipChanged
```

These can help Presentation refresh only affected views.

Exact event mechanism is deferred.

---

## 62. Avoid Duplicate Mutation Through Events

Events should describe completed changes.

They should not trigger another uncontrolled duplicate mutation path.

Preferred:

```text
Command mutates
Event notifies
```

not:

```text
Event causes random widgets/services to mutate Domain again
```

---

## 63. Incremental UI Refresh

Because commands identify affected IDs, Presentation can update selectively.

Example:

```text
RenameEntity(E)
      ↓
affected: E
      ↓
Explorer updates E
Canvas label updates E
Properties updates E
```

No need to rebuild the entire project UI.

---

## 64. Command Metadata

Future commands may carry metadata such as:

```text
label
category
affected IDs
timestamp
origin
merge key
```

Do not require all metadata in the first implementation.

---

## 65. Command Origin

Later, a command may originate from:

```text
Desktop UI
VS Code extension
Import apply
AI-approved proposal
future collaboration
```

The Domain mutation rules should remain the same.

Origin may be useful metadata but does not change semantic correctness.

---

## 66. AI and Commands

Future AI must not mutate the Project directly.

Preferred:

```text
AI proposal
      ↓
user review
      ↓
Application command
      ↓
Domain
```

This ensures AI uses the same mutation/undo rules as human edits.

---

## 67. VS Code and Commands

A future VS Code extension can issue equivalent Application commands.

Example:

```text
Rename Entity
```

should mean the same operation whether initiated from Qt desktop or VS Code.

This is a major benefit of keeping commands framework-independent.

---

## 68. Command Serialization

Normal runtime commands do not need to be serialized in V1.

Undo history is not persisted.

If future collaboration/audit requires command serialization, define a dedicated versioned operation format later.

---

## 69. Command Equality

Commands do not require general-purpose equality by default.

Specific tests may compare fields/results.

Avoid unnecessary abstraction.

---

## 70. Threading Rule

Commands that mutate the live Project should be applied through the controlled Application mutation context.

Background workers should not apply arbitrary commands directly to live UI/project state from worker threads.

Recommended:

```text
Worker computes proposal
      ↓
Application receives result
      ↓
Apply command on controlled context
```

---

## 71. Main/UI Thread Application

For the Qt desktop implementation, live project mutation will initially occur on the main/application UI coordination thread unless a later architecture change explicitly introduces another ownership model.

This simplifies:

- undo,
- UI updates,
- ownership,
- consistency.

Heavy computation still runs in background.

---

## 72. Snapshot-Based Background Analysis

Example:

```text
Conceptual snapshot
      ↓
background conversion
      ↓
candidate
      ↓
ApplyCandidateCommand
```

The worker does not continuously mutate the live model.

---

## 73. Auto-Layout

Auto-layout is a special example.

Calculation:

```text
Diagram snapshot
      ↓
background layout
      ↓
proposed positions
```

Application:

```text
ApplyLayoutCommand
```

Undo restores prior positions.

---

## 74. Property Editing

Some UI property editors update on every keystroke.

ERDFlow should avoid generating unusable history like:

```text
S
St
Stu
Stud
Stude
Studen
Student
```

for one rename.

Possible strategies:

- commit on focus loss/Enter,
- merge consecutive edits,
- delayed edit session grouping.

Exact UX is decided during implementation.

---

## 75. Text Edit Merge

If incremental rename commands are emitted, they may merge when:

```text
same target
same property
same edit session
```

Undo should ideally return to the value before the editing session.

---

## 76. Drag Merge

Pointer move events during a drag should not create one command per frame.

Instead:

```text
drag start → old position
drag end   → new position
```

creates one command.

---

## 77. Multi-Selection Move

Moving several selected diagram elements together should usually be one undoable action.

Conceptually:

```text
MoveSelectionCommand
├── Element A old/new
├── Element B old/new
└── Element C old/new
```

---

## 78. Copy/Paste

Copying as new semantic objects creates new stable IDs.

Paste should be one atomic command for the pasted group.

Undo removes the whole pasted group.

Redo restores the same generated IDs used by that paste operation.

---

## 79. Duplicate

A `Duplicate` command follows the same principle as copy/paste:

```text
new semantic IDs
```

stored in the command so redo restores the same duplicated objects.

---

## 80. Delete Graph Effects

Deleting an entity may affect relationship references.

The command must capture enough state to restore all affected elements.

This is why semantic commands are preferable to naïve per-container deletion.

---

## 81. Order of Restoration

Undoing complex deletions must restore dependencies in a safe order.

Example:

```text
restore Entity
restore Attributes
restore Relationship references
restore Layout
```

Exact mechanics belong to implementation/tests.

---

## 82. Undo and Mappings

Later downstream models may have mappings.

If a command changes/removes mapped objects, undo data may need to preserve corresponding mapping state.

Example:

```text
Delete Relation
```

may affect:

```text
mapping/provenance
```

Undo restores both consistently.

---

## 83. Undo and Baselines

Ordinary schema edits should not rewrite generation baselines unless the operation explicitly accepts a new generated baseline.

Baseline changes should occur through deliberate commands/use cases.

This prevents history confusion.

---

## 84. Apply Baseline Command

When the user accepts generated state as a new baseline, that may be a distinct controlled operation.

Whether this is user-visible in undo history is decided when regeneration is implemented.

---

## 85. Command Failure Atomicity

A command should not leave the Project half-mutated if it fails.

Preferred:

```text
validate/precompute
      ↓
apply complete change
```

or use a safe rollback strategy.

Atomicity is especially important for bulk commands.

---

## 86. Bulk Mutation Strategy

For complex operations, prepare a change set before applying.

Example:

```text
Import proposal
      ↓
Validated change set
      ↓
ApplyImportCommand
```

This reduces partial mutation risk.

---

## 87. Change Set

A future `ChangeSet` concept may represent:

```text
adds
removes
updates
mapping changes
layout changes
```

This can support:

- review,
- application,
- undo.

Exact representation is deferred.

---

## 88. Relationship to Diff Engine

Commands and diff solve different problems.

Command:

```text
We know what user intended now.
```

Diff:

```text
We need to compare two existing states.
```

Use commands when semantic intent is available.

Use diff where comparison is genuinely required.

---

## 89. Why Not Use Diff for Every Undo

A generic before/after diff for every edit would:

- lose intent,
- cost more,
- complicate merge behavior,
- make history labels weaker.

Semantic commands are the preferred primary mechanism.

---

## 90. Why Not Use Snapshots for Every Undo

Full snapshots are simple but expensive.

They may be acceptable temporarily for a large rare operation.

They are not the default strategy for normal edits.

---

## 91. Alternative A — Direct Widget Mutation

Rejected.

Reason:

- no central undo behavior,
- duplicated validation,
- hard to test,
- UI becomes source of truth,
- future frontends diverge.

---

## 92. Alternative B — `QUndoCommand` as Core Command

Rejected.

Reason:

- Application depends on Qt,
- future reuse is harder,
- violates ADR-002 dependency intent.

Qt undo remains an adapter option.

---

## 93. Alternative C — Snapshot Every Project State

Rejected as default.

Reason:

- high memory cost,
- poor scale,
- unnecessary copying,
- loses semantic intent.

---

## 94. Alternative D — Generic Property Mutation System

Example:

```text
SetProperty(ObjectId, "name", value)
```

Not selected as the primary user-command model.

It may be useful internally for generic editors, but important operations should preserve semantic names/meaning.

---

## 95. Alternative E — Event Sourcing

Rejected for V1.

Command history is an editing mechanism, not the authoritative persisted project log.

---

## 96. Alternative F — No Undo Until Later

Rejected.

Undo architecture affects mutation design from the beginning.

Adding it after widgets directly mutate Domain state would require expensive restructuring.

A minimal undo foundation should exist early.

---

## 97. First Implementation Scope

Implement only enough for the first Conceptual Editor.

Initial commands:

```text
CreateEntity
RenameEntity
DeleteEntity
MoveDiagramElement
```

Then:

```text
AddAttribute
CreateRelationship
ChangeCardinality
ChangeParticipation
```

Do not build a generalized enterprise command bus.

---

## 98. First Tests

### Test 1 — Create/Undo/Redo

```text
Create Entity E
Undo
→ E absent
Redo
→ E restored with same EntityId
```

### Test 2 — Rename

```text
Student → Learner
Undo
→ Student
Redo
→ Learner
```

ID remains unchanged.

### Test 3 — Delete

```text
Delete Entity E
Undo
→ E and required dependent state restored
```

### Test 4 — Move

```text
A → B
Undo → A
Redo → B
```

Semantic data unchanged.

### Test 5 — Invalid Target

Command against missing ID fails safely.

### Test 6 — One Drag, One History Entry

Many pointer move events produce one final undoable move.

---

## 99. Undo UI

The desktop may expose:

```text
Edit → Undo
Edit → Redo
Ctrl/Cmd+Z
Ctrl/Cmd+Shift+Z or platform equivalent
```

Qt actions can reflect whether history is available.

Exact shortcuts follow platform conventions.

---

## 100. History View

A visual history panel is not required for V1.

If later added, semantic command labels make it possible.

---

## 101. Command Logging

Undo history is not the same as application logging.

Debug logging may record command metadata.

Avoid logging sensitive imported/database content by default.

---

## 102. Architecture Invariants

### Invariant 1

Meaningful user mutations flow through Application commands.

### Invariant 2

Core commands are independent of Qt.

### Invariant 3

Commands reference objects through stable IDs.

### Invariant 4

Undo restores the previous semantic state.

### Invariant 5

Redo reuses the same identities created by the original action.

### Invariant 6

One meaningful user gesture should normally produce one undo step.

### Invariant 7

Large multi-object operations may be atomic composite commands.

### Invariant 8

Normal undo history is session/project-scoped and not persisted in `.erdx`.

### Invariant 9

Background workers compute proposals/results; controlled Application mutation applies them.

### Invariant 10

Save/Open/Export are not ordinary semantic undo commands.

### Invariant 11

Command history must eventually be memory-bounded.

### Invariant 12

Semantic commands are preferred over generic state diffing when intent is already known.

---

## 103. Relationship to Other ADRs

Depends on:

```text
ADR-001 — Stable Identity Strategy
ADR-002 — Layered Architecture and Dependency Direction
ADR-003 — Desktop UI Technology
ADR-004 — Core Language Strategy
ADR-005 — Project File and Persistence Boundary
```

Later ADRs refine:

```text
ADR-007 — Background Work / Concurrency Strategy
ADR-008 — Cross-Level Mapping and Provenance
ADR-009 — Validation Gates and Conversion Policy
ADR-010 — Change Propagation and Regeneration
```

---

## 104. Consequences — Positive

This decision gives ERDFlow:

- controlled mutation,
- reliable undo/redo,
- semantic history,
- stable ID usage,
- testable editing behavior,
- future frontend reuse,
- safer bulk changes,
- better incremental UI refresh,
- cleaner propagation logic.

---

## 105. Consequences — Costs

This introduces:

- command classes/types,
- undo metadata,
- some duplication of old/new values,
- grouping/merge logic,
- more disciplined UI wiring.

These costs are justified because ERDFlow is an editor, not a read-only viewer.

---

## 106. Open Implementation Decisions

This ADR intentionally leaves open:

- exact C++ command representation,
- inheritance vs value types vs variants,
- exact Qt undo adapter structure,
- exact history memory budget,
- exact command merge timing,
- exact `ChangeSet` representation,
- exact event/notification mechanism,
- exact persistent history/audit strategy if ever added.

These should be resolved when the implementation reaches them.

---

## 107. Decision Outcome

ERDFlow adopts:

```text
Application-level semantic commands
+
stable-ID targeting
+
undo/redo based on semantic inverse/delta
+
Qt Undo Framework only as an outer integration option
+
atomic composite commands for multi-object operations
```

---

## 108. Final Principle

> If a user intentionally changes the project, ERDFlow should know what they meant, be able to test it, and—when appropriate—be able to reverse it safely.
