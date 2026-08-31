# ADR-010 — Change Propagation and Regeneration

**Status:** Proposed  
**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** Forward regeneration, downstream manual edits, three-way comparison, conflict handling, and optional Schema → Conceptual propagation

---

## 1. Context

ERDFlow follows a progressive design flow:

```text
Conceptual ERD
      ↓
Relational Schema
      ↓
Physical Design
      ↓
SQL
      ↓
Data
```

The normal direction is forward.

However, after a downstream model is generated, the user may edit it manually.

Example:

```text
Conceptual:
Student(Name)

Generated Schema:
Student(Name)
```

Then the user changes the Schema:

```text
Student(Name)
→
Student(FullName)
```

Later the Conceptual model changes:

```text
Student(Name, DateOfBirth)
```

ERDFlow must now update the Schema without silently destroying:

```text
FullName
```

or any other downstream work.

ERDFlow also needs a safe way to detect when a downstream edit has a meaningful upstream equivalent.

Therefore change management cannot be implemented as a simple:

```text
regenerate target and replace everything
```

nor as an always-running two-way synchronization engine.

---

## 2. Decision

ERDFlow will use:

> **Controlled forward regeneration with three-way comparison, plus optional user-approved back-propagation for eligible downstream changes.**

The normal forward flow is:

```text
Conceptual
      ↓
deterministic generation
      ↓
Relational Schema
```

Once a downstream model exists and may contain manual edits, regeneration uses:

```text
Previous Generated Baseline
        +
Current User-Edited Target
        +
New Generated Candidate
        ↓
Reviewable Change Proposal
```

Schema → Conceptual propagation follows:

```text
Detect
  ↓
Explain
  ↓
Ask
  ↓
Apply only if approved
```

---

## 3. This Is Not Continuous Bidirectional Synchronization

ERDFlow does **not** maintain two models in permanent live synchronization.

Rejected mental model:

```text
Conceptual ⇄ Schema
```

where every edit instantly mutates both sides.

Accepted model:

```text
Conceptual
   ↓
forward generation
   ↓
Schema
```

and separately:

```text
Schema edit
   ↓
is there a meaningful Conceptual equivalent?
   ↓
yes
   ↓
ask user
   ↓
apply only if approved
```

---

## 4. Why Continuous Two-Way Sync Is Rejected

Continuous two-way synchronization would create difficult questions:

```text
Which side is authoritative?
What if both changed?
What is a technical-only change?
What if one Schema change maps to several Conceptual meanings?
What if a generated object was manually restructured?
What if a rename looks like delete + add?
```

ERDFlow instead uses explicit review and user intent.

---

## 5. Authoritative Direction

The primary semantic direction is:

```text
Conceptual → Relational
```

Conceptual is the upstream source for deterministic logical generation.

Relational Schema remains independently editable.

Those edits are not automatically forced back into Conceptual.

---

## 6. Generated Does Not Mean Locked

A generated Schema may be manually refined.

Examples:

```text
rename relation
rename attribute
add technical helper attribute
add additional relation
modify nullability
add implementation-oriented structure
```

ERDFlow must preserve legitimate downstream work where possible.

---

## 7. Generation Baseline

When a generated target is accepted, ERDFlow records a baseline representing the generated state at that point.

Conceptually:

```text
Conceptual v1
      ↓
Generated Schema v1
      ↓
Baseline v1
```

The baseline represents:

```text
what ERDFlow generated
```

not:

```text
everything the user may later change
```

---

## 8. Current Target

After generation, the user may edit the target:

```text
Baseline Schema
      ↓
manual edits
      ↓
Current Schema
```

The current target is therefore:

```text
generated content
+
user refinements
```

---

## 9. New Candidate

When the upstream model later changes, ERDFlow generates:

```text
New Candidate Schema
```

from the latest Conceptual state.

The new candidate is not immediately installed.

It is compared against:

```text
Previous Baseline
Current Schema
```

---

## 10. Three-Way Comparison

The core regeneration model is:

```text
             Previous Baseline
                /        \
               /          \
      Current Schema    New Candidate
               \          /
                \        /
                Review
```

This allows ERDFlow to distinguish:

```text
what was originally generated
what the user changed manually
what the new upstream model wants to change
```

---

## 11. Why Two-Way Comparison Is Insufficient

Comparing only:

```text
Current Schema
vs
New Candidate
```

cannot reliably tell whether a difference came from:

```text
user manual edit
```

or:

```text
upstream change
```

The baseline provides the missing historical reference.

---

## 12. Example — Safe Addition

Baseline:

```text
Student(
    Name
)
```

Current Schema:

```text
Student(
    FullName
)
```

New Candidate:

```text
Student(
    Name,
    DateOfBirth
)
```

ERDFlow should understand:

```text
FullName
= likely downstream manual modification

DateOfBirth
= new upstream addition
```

The proposal should not simply restore `Name` and remove `FullName`.

---

## 13. Example — Technical Downstream Addition

Baseline:

```text
Student(
    StudentId,
    Name
)
```

Current Schema:

```text
Student(
    StudentId,
    Name,
    SearchKey
)
```

New Candidate:

```text
Student(
    StudentId,
    Name,
    BirthDate
)
```

Expected review:

```text
Preserve SearchKey
Add BirthDate
```

unless a conflict exists.

---

## 14. Identity Is Central

Three-way comparison uses stable IDs from ADR-001.

Names are not sufficient.

Example:

```text
RelationId = R1
Name = Student
```

renamed manually to:

```text
Name = UniversityStudent
```

still remains:

```text
RelationId = R1
```

if continuity is established.

---

## 15. Mapping / Provenance Is Central

ADR-008 provides lineage:

```text
Conceptual Entity E1
      ↓
Relation R1
```

This helps regeneration understand:

```text
which generated target corresponds to which source
```

without guessing from names.

---

## 16. Baseline + Mapping Together

Safe regeneration requires both:

```text
baseline
+
mapping/provenance
```

Baseline tells ERDFlow:

```text
what existed before
```

Mappings tell ERDFlow:

```text
why it existed and where it came from
```

---

## 17. Candidate Generation

Candidate generation is deterministic.

Flow:

```text
Current Conceptual snapshot
      ↓
Validation
      ↓
Conversion
      ↓
Candidate Schema
      +
Candidate Mappings
```

The live Schema is not mutated during candidate generation.

---

## 18. Background Execution

Candidate generation may run in the background under ADR-007.

Result:

```text
Candidate
+
Source Revision
+
Candidate Mappings
```

The result must be checked for staleness before review/application.

---

## 19. Stale Candidate

If Conceptual changes while generation runs:

```text
candidate source revision = 10
current Conceptual revision = 11
```

the candidate is stale.

ERDFlow must not silently apply it.

Preferred behavior:

```text
invalidate
rerun
or clearly mark stale
```

---

## 20. Reviewable Change Proposal

The output of regeneration comparison is not a mutated Schema.

It is:

```text
ChangeProposal
```

Conceptually containing:

```text
Added
Removed
Modified
Renamed
Preserved Manual Change
Conflict
Ambiguous Change
Technical-Only Change
```

Exact representation is deferred.

---

## 21. Change Classification

Potential classes include:

```text
Upstream Addition
Upstream Removal
Upstream Modification
Manual Downstream Change
Non-Conflicting Merge
Conflict
Ambiguous
Technical-Only
```

These classes help the UI explain what will happen.

---

## 22. Conflict

A conflict occurs when upstream and downstream independently modify the same semantic target in incompatible ways.

Example:

Baseline:

```text
Name
```

Current Schema:

```text
FullName
```

New Candidate:

```text
DisplayName
```

ERDFlow cannot know automatically which rename should win.

Result:

```text
Conflict
```

User review is required.

---

## 23. Conflict Is Not an Error

A conflict does not mean the Project is invalid.

It means:

```text
both sides changed meaning
```

and user intent is needed.

---

## 24. Ambiguity

An ambiguous change is one where ERDFlow lacks enough lineage or semantic certainty to classify safely.

Example:

```text
target object replaced manually
mapping no longer clearly represents continuity
```

Rule:

```text
do not guess
```

---

## 25. Conflict Resolution

Possible user choices may include:

```text
Keep Current
Use Generated
Merge
Map Manually
Skip
```

Exact UX is deferred.

The core must represent the unresolved conflict explicitly.

---

## 26. Automatic Non-Conflicting Merge

Some changes can be safely combined.

Example:

```text
Current adds technical SearchKey
Candidate adds BirthDate
```

No collision.

Proposal:

```text
preserve SearchKey
add BirthDate
```

This can be marked as safe/non-conflicting.

---

## 27. Automatic Application Policy

Even non-conflicting proposals should initially remain reviewable.

ERDFlow may later offer:

```text
auto-apply safe changes
```

as a user preference.

Default architecture:

```text
reviewable first
```

---

## 28. Destructive Changes

Potentially destructive changes include:

```text
remove relation
remove attribute
replace key
change relationship structure
```

These must never be silently applied merely because the new candidate lacks the old object.

---

## 29. Removal Example

Baseline:

```text
Student.Email
```

Current Schema:

```text
Student.Email
```

New Candidate:

```text
Email removed upstream
```

Proposal:

```text
Remove Student.Email
```

This is a generated removal.

Application policy may require explicit confirmation.

---

## 30. Removal with Manual Modification

Baseline:

```text
Student.Email
```

Current Schema:

```text
Student.PrimaryEmail
```

New Candidate:

```text
Email removed upstream
```

This is more complex.

The target was manually changed after generation.

Do not silently delete it.

Classify for review.

---

## 31. Mapping Preservation

If a generated target survives regeneration as the same semantic target, preserve:

```text
target stable ID
mapping continuity
```

where possible.

This protects:

- downstream references,
- manual edits,
- undo history,
- future diff quality.

---

## 32. New Target

If the new candidate introduces a genuinely new downstream object:

```text
new target stable ID
```

is created.

Example:

```text
new M:M relationship
→ new junction relation
```

---

## 33. Removed Generated Target

If a generated target is no longer required upstream:

```text
proposal marks removal
```

The live target remains until the change is accepted.

---

## 34. Baseline Update

A new generation baseline is created only after the accepted regeneration result is applied successfully.

Flow:

```text
Generate
Compare
Review
Apply
Validate
Accept
      ↓
New Baseline
```

Do not update the baseline before the live target has actually accepted the generated state.

---

## 35. Baseline Does Not Include Unaccepted Candidate

A rejected or abandoned candidate never becomes the baseline.

---

## 36. Baseline and Manual Edits

The baseline represents generated state.

The current target may continue to include downstream manual changes.

After a new accepted generation, the updated baseline should reflect the newly accepted generated contribution while preserving the distinction from current manual changes.

Exact baseline representation is deferred.

---

## 37. Baseline Persistence

Baselines required for future regeneration must persist in `.erdx`.

This follows ADR-005.

---

## 38. Baseline Mapping State

Baseline context must preserve the mapping/provenance information required to reconstruct generation lineage.

This follows ADR-008.

---

## 39. Schema → Conceptual Propagation

Downstream-to-upstream propagation is optional.

Flow:

```text
Schema Edit
      ↓
Mapping Lookup
      ↓
Semantic Classification
      ↓
Conceptual Equivalent Exists?
      ↓
Explain
      ↓
Ask User
      ↓
Apply if approved
```

---

## 40. Propagation Eligibility

A Schema change is eligible only when ERDFlow can identify a meaningful Conceptual equivalent.

Examples that may be eligible:

```text
rename mapped relation
rename mapped schema attribute
change logical property that has clear conceptual meaning
```

Exact eligible operations are defined incrementally.

---

## 41. Technical-Only Changes

Technical changes normally remain downstream.

Examples:

```text
index
physical data type
dialect-specific constraint
implementation helper column
technical surrogate key
storage option
```

These do not automatically have Conceptual meaning.

---

## 42. No Upstream Prompt for Pure Technical Changes

Example:

```text
Add Index idx_student_email
```

ERDFlow should not ask:

```text
Apply this index to Conceptual ERD?
```

because the Conceptual model does not represent indexes.

---

## 43. Rename Example

Mapping:

```text
Conceptual Attribute A1
→
Schema Attribute SA1
```

User renames:

```text
SA1:
Name → FullName
```

ERDFlow may classify:

```text
eligible Conceptual rename
```

Then ask:

```text
Apply this rename to the Conceptual ERD too?
```

---

## 44. If User Says Yes

Apply:

```text
Schema rename
+
Conceptual rename
```

through controlled Application commands.

The operation should be atomic where appropriate.

---

## 45. If User Says No

Keep:

```text
Schema = FullName
Conceptual = Name
```

This divergence is allowed.

The mapping remains but now records a downstream customization context.

Exact metadata is deferred.

---

## 46. Divergence Is Valid

ERDFlow does not require Conceptual and Schema names to always match.

The models serve different abstraction levels.

Divergence can be intentional.

---

## 47. Propagation Prompt Timing

The prompt may occur:

```text
immediately after edit
```

or:

```text
as part of a review queue
```

Exact UX is deferred.

Architecture only requires explicit user approval before upstream mutation.

---

## 48. No Silent Back-Propagation

This is an invariant.

Even if ERDFlow is very confident:

```text
Schema → Conceptual
```

must not silently change upstream meaning unless a future explicit project setting intentionally enables a narrowly defined safe behavior.

Default:

```text
ask
```

---

## 49. Multi-Source Mapping

If a Schema object maps to multiple Conceptual sources, upstream propagation may be ambiguous.

Example:

```text
generated FK
```

derived from:

```text
relationship
+
target key
```

A rename may not map cleanly to one Conceptual edit.

Result:

```text
not automatically eligible
```

or:

```text
requires explicit review
```

---

## 50. One-to-Many Mapping

If one Conceptual source generated several Schema objects, changing one target does not automatically imply how the source should change.

Again:

```text
do not guess
```

---

## 51. Propagation Classification

Potential classification:

```text
Eligible
TechnicalOnly
Ambiguous
Conflict
Unsupported
```

Exact enum names are deferred.

---

## 52. Change Explanation

Before asking the user, ERDFlow should explain the proposed upstream effect.

Example:

```text
Schema attribute "FullName"
originates from Conceptual attribute "Name".

Propagating this change will rename:
Conceptual Student.Name → Student.FullName
```

---

## 53. Explain Using Stable References

The underlying operation uses stable IDs.

Names are shown only for user comprehension.

---

## 54. Propagation Command

Approved propagation becomes a normal Application command or composite command under ADR-006.

This ensures:

```text
undo
redo
validation
history
```

work normally.

---

## 55. Undo of Propagation

If one user action changed both Schema and Conceptual:

```text
undo
```

should restore both consistently if they were applied atomically.

---

## 56. Redo of Propagation

Redo restores the same semantic changes and stable IDs.

---

## 57. Propagation Validation

Before applying an upstream change:

```text
validate affected Conceptual state
```

If the proposed upstream operation would violate a hard Domain invariant:

```text
do not apply
```

Explain why.

---

## 58. Propagation Is Not Conversion

Back-propagation performs a semantic edit to Conceptual.

It is not reverse-engineering the entire Schema into a new Conceptual model.

These are separate workflows.

---

## 59. Reverse Engineering

Future Schema/SQL → Conceptual reconstruction may exist.

That is an import/reverse-engineering feature.

It is not the same as controlled propagation of a known mapped change.

---

## 60. Manual Schema-Only Objects

A manually created Schema object with no Conceptual mapping normally has no upstream propagation target.

Example:

```text
AuditTrail relation
```

created only in Schema.

No prompt is required.

---

## 61. User-Linked Mapping

Future UI may allow the user to explicitly map a Schema object to a Conceptual object.

Once established, future propagation eligibility may improve.

This feature is deferred.

---

## 62. Candidate Review UI

The future review UI may show:

```text
Added
Removed
Modified
Preserved
Conflicts
Manual Changes
```

with per-item choices.

Exact UI is deferred.

---

## 63. Proposal Is Structured

The review UI must not derive change meaning only from text.

Core returns structured proposal data.

Presentation renders it.

---

## 64. Change Proposal Identity

A proposal may have an operational ID for:

```text
review
cancellation
diagnostics
```

This is separate from semantic object IDs.

Exact representation is deferred.

---

## 65. Proposal Source Revision

A proposal must know:

```text
source Conceptual revision
baseline identity/revision
target revision
```

so Application can detect staleness.

---

## 66. Target Changes During Review

If the user edits the Schema while a regeneration proposal is open:

```text
proposal may become stale
```

ERDFlow should not silently apply it to changed state.

Possible behavior:

```text
invalidate and regenerate
```

or later:

```text
rebase
```

V1 direction:

```text
invalidate/recompute rather than implement complex live rebasing
```

---

## 67. Source Changes During Review

Same rule.

If Conceptual changes after proposal creation:

```text
proposal becomes stale
```

unless it is explicitly recomputed.

---

## 68. Applying Proposal

Apply flow:

```text
Check revisions
      ↓
Check proposal still valid
      ↓
Apply selected change set
      ↓
Validate resulting target
      ↓
Commit command
      ↓
Update mapping/provenance
      ↓
Update baseline
```

If any critical step fails:

```text
do not leave partial state
```

---

## 69. Atomicity

Applying a reviewed proposal should be atomic from the user's perspective where practical.

Either:

```text
accepted change set applied successfully
```

or:

```text
live target remains unchanged
```

---

## 70. Proposal Undo

An accepted regeneration proposal should normally be one undoable history entry.

Example:

```text
Apply Schema Regeneration
```

Undo restores:

```text
previous current Schema
previous mapping state
previous baseline state
```

as appropriate.

---

## 71. Baseline Undo Complexity

Because baseline state participates in regeneration semantics, undoing an accepted regeneration may need to restore the prior baseline.

This is a deliberate high-complexity area.

The exact delta representation is deferred.

---

## 72. Memory Cost

Three-way regeneration may temporarily hold:

```text
Previous Baseline
Current Target
New Candidate
Mappings
Change Proposal
Undo Data
```

This is a known scale risk from SCALE.md.

Measure before optimizing.

---

## 73. No Full Copies Forever Requirement

Initial implementation may use straightforward model copies where project sizes are moderate.

Later, if memory requires:

```text
structural sharing
compact deltas
immutable components
```

may be introduced.

---

## 74. Regeneration Is High Complexity

The roadmap intentionally treats:

```text
Generation Baseline
Three-Way Review
Controlled Back-Propagation
```

as distinct phases.

Do not hide them inside one "sync" feature.

---

## 75. Forward Conversion Before Regeneration

ERDFlow must first prove:

```text
Conceptual → Relational
```

deterministic conversion.

Then:

```text
mapping/provenance
```

Then:

```text
baseline
```

Then:

```text
three-way review
```

Then:

```text
controlled back-propagation
```

This dependency order is required.

---

## 76. Validation Dependency

ADR-009 applies before forward generation.

A blocked Conceptual model does not produce an authoritative new candidate.

Warnings may still permit generation.

---

## 77. Candidate Validation

New candidate Schema should be validated before entering review.

A conversion engine bug should not become a reviewable user change.

---

## 78. Current Target Validation

Current Schema may contain manual changes.

Some may be conversion-invalid or unusual.

Review logic should distinguish:

```text
existing user problem
```

from:

```text
new candidate change
```

where practical.

---

## 79. Mapping Integrity

Three-way review depends on mapping integrity.

Broken mappings may block safe automatic classification.

In that case:

```text
report ambiguity
```

rather than infer lineage by name alone.

---

## 80. Conflict Granularity

Conflicts should be reported at the smallest useful semantic level.

Example:

```text
SchemaAttribute SA1
```

rather than:

```text
entire Schema conflict
```

when only one attribute is affected.

---

## 81. Review Grouping

UI may group related changes:

```text
Entity Student
├── rename
├── add BirthDate
└── preserve SearchKey
```

This is presentation logic.

Core provides relationship/element references.

---

## 82. User Choice Persistence

If the user chooses:

```text
keep current
```

for a conflict, ERDFlow may later need to remember that downstream override.

Exact override metadata is deferred.

---

## 83. Repeated Conflict Prevention

A future optimization may prevent the same resolved manual override from reappearing as a conflict every regeneration.

Possible mechanisms:

```text
override metadata
mapping state
accepted divergence markers
```

Not locked in this ADR.

---

## 84. Manual Override as First-Class Concept

ERDFlow should eventually distinguish:

```text
accidental mismatch
```

from:

```text
intentional downstream override
```

This improves regeneration quality.

Exact model is deferred.

---

## 85. Technical-Only Preservation

Technical downstream objects should normally be preserved during upstream regeneration unless the user explicitly removes them.

Examples:

```text
index
technical column
physical naming customization
```

provided they do not conflict with required generated changes.

---

## 86. Schema vs Physical Scope

This ADR applies conceptually to multiple stage transitions:

```text
Conceptual → Relational
Relational → Physical
```

The first implementation focus is:

```text
Conceptual → Relational
```

Physical regeneration adopts the same principles later.

---

## 87. SQL Generation

SQL is normally regenerated output from the Physical model.

Manual edits to generated SQL text are not automatically synchronized back into Physical design.

If ERDFlow later supports editable SQL with reverse propagation, that requires a separate decision.

---

## 88. Data Workspace

Row data changes do not participate in model-level regeneration.

This ADR concerns structural model evolution.

Data mutation follows Data Source rules later.

---

## 89. AI Proposals

Future AI may propose structural changes.

Those changes enter the same review/application pipeline.

AI does not bypass:

```text
validation
mapping
conflict detection
user approval
commands
```

---

## 90. VS Code Extension

A future VS Code frontend should use the same change-management core.

Desktop and VS Code must not implement different regeneration semantics.

---

## 91. Future Collaboration

Future collaboration may introduce simultaneous edits.

This ADR does not solve distributed multi-user conflict resolution.

However:

```text
stable IDs
semantic commands
baselines
structured change proposals
```

provide useful foundations.

---

## 92. Alternative A — Always Replace Generated Schema

Rejected.

Reason:

```text
destroys manual downstream edits
```

and undermines trust.

---

## 93. Alternative B — Never Regenerate Existing Schema

Rejected.

Users would lose the benefit of progressive refinement.

They would need to manually copy upstream changes.

---

## 94. Alternative C — Continuous Bidirectional Sync

Rejected.

Reason:

- ambiguous authority,
- accidental propagation,
- technical changes leaking upstream,
- difficult conflict semantics,
- poor control of user intent.

---

## 95. Alternative D — Name-Based Merge

Rejected.

Names are mutable and not reliable identity.

Use stable IDs + mappings.

---

## 96. Alternative E — Two-Way Diff Only

Example:

```text
Current
vs
Candidate
```

Rejected as insufficient.

It cannot distinguish:

```text
manual downstream edit
```

from:

```text
new upstream change
```

without the previous baseline.

---

## 97. Alternative F — Automatic Back-Propagation

Rejected as default.

Even apparently simple changes may represent intentional downstream divergence.

User approval remains required.

---

## 98. Alternative G — Never Allow Back-Propagation

Rejected.

Some downstream changes have obvious useful upstream equivalents.

ERDFlow should offer that convenience safely.

---

## 99. Consequences — Positive

This decision gives ERDFlow:

- safe progressive refinement,
- protection of downstream work,
- explicit conflict handling,
- explainable regeneration,
- stable target continuity,
- controlled upstream propagation,
- better user trust,
- reusable change-management semantics.

---

## 100. Consequences — Costs

This introduces significant complexity:

- generation baselines,
- candidate models,
- three-way comparison,
- conflict classification,
- proposal review,
- baseline/mapping undo,
- memory overhead,
- stale-result handling.

These costs are central to ERDFlow's core product promise.

---

## 101. Initial Implementation Scope

Do not implement regeneration during the first Conceptual Editor milestone.

Implementation order:

```text
1. deterministic Conceptual → Relational conversion
2. mapping/provenance
3. editable Schema
4. generation baseline
5. three-way review
6. controlled Schema → Conceptual propagation
```

---

## 102. Initial Regeneration Tests

### Test 1 — Upstream Addition

```text
Baseline:
Student(Name)

Current:
Student(Name)

Candidate:
Student(Name, BirthDate)

→ propose Add BirthDate
```

### Test 2 — Preserve Manual Addition

```text
Current adds SearchKey
Candidate adds BirthDate

→ preserve SearchKey
→ add BirthDate
```

### Test 3 — Rename Conflict

```text
Baseline Name
Current FullName
Candidate DisplayName

→ Conflict
```

### Test 4 — Target ID Preservation

Same continuing generated Relation keeps same `RelationId`.

### Test 5 — Removal Review

Upstream removal becomes proposal, not immediate deletion.

### Test 6 — Stale Proposal

Current revision changes after proposal creation.

→ proposal cannot silently apply.

---

## 103. Initial Back-Propagation Tests

### Test 1 — Eligible Rename

Mapped Schema attribute rename:

```text
Name → FullName
```

→ prompt Conceptual equivalent.

### Test 2 — User Accepts

Conceptual attribute renamed.

### Test 3 — User Declines

Schema remains changed.

Conceptual remains unchanged.

### Test 4 — Technical Change

Add Index.

→ no Conceptual propagation prompt.

### Test 5 — Ambiguous Mapping

One Schema target maps to multiple Conceptual sources.

→ no automatic propagation.

### Test 6 — Undo

Accepted propagation undone atomically.

---

## 104. Architecture Invariants

### Invariant 1

Conceptual → Relational is the primary forward generation direction.

### Invariant 2

Schema remains independently editable.

### Invariant 3

ERDFlow does not use continuous bidirectional synchronization.

### Invariant 4

Safe regeneration compares baseline + current + new candidate.

### Invariant 5

Manual downstream work is not silently destroyed.

### Invariant 6

Destructive or ambiguous changes require review.

### Invariant 7

Stable IDs and mappings are authoritative for continuity.

### Invariant 8

Generated candidates do not mutate the live target until accepted.

### Invariant 9

Stale proposals are not silently applied.

### Invariant 10

Schema → Conceptual propagation requires a meaningful semantic equivalent.

### Invariant 11

Technical-only changes remain downstream.

### Invariant 12

Back-propagation requires explicit user approval by default.

### Invariant 13

Approved multi-level changes use normal Application command/undo rules.

### Invariant 14

Baselines required for safe regeneration persist with the project.

---

## 105. Relationship to Other ADRs

Depends on:

```text
ADR-001 — Stable Identity Strategy
ADR-002 — Layered Architecture and Dependency Direction
ADR-005 — Project File and Persistence Boundary
ADR-006 — Command / Undo Strategy
ADR-007 — Background Work / Concurrency Strategy
ADR-008 — Cross-Level Mapping and Provenance
ADR-009 — Validation Gates and Conversion Policy
```

This ADR is a prerequisite for the roadmap phases:

```text
Generation Baseline
Three-Way Regeneration Review
Controlled Schema → Conceptual Propagation
```

---

## 106. Open Implementation Decisions

This ADR intentionally leaves open:

- exact baseline storage structure,
- exact `ChangeProposal` representation,
- exact diff algorithm,
- exact conflict taxonomy,
- exact manual-override representation,
- exact per-item review UI,
- exact auto-apply-safe-changes preference,
- exact baseline undo delta format,
- exact propagation eligibility table,
- exact physical-model regeneration policy,
- exact future collaborative conflict strategy.

These should be decided when implementation reaches those phases.

---

## 107. Decision Outcome

ERDFlow adopts:

```text
forward deterministic generation
+
persistent generation baselines
+
three-way regeneration comparison
+
reviewable change proposals
+
manual edit preservation
+
explicit conflict handling
+
optional user-approved downstream → upstream propagation
```

instead of live bidirectional synchronization.

---

## 108. Final Principle

> Generate forward confidently, preserve downstream intent carefully, and never move meaning upstream without understanding and user approval.
