# ADR-008 — Cross-Level Mapping and Provenance

**Status:** Proposed  
**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** How ERDFlow records lineage between Conceptual, Relational, and Physical model elements

---

## 1. Context

ERDFlow is built around progressive refinement:

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

The downstream models are not copies of the upstream model.

They contain different semantic objects.

Example:

```text
Conceptual Entity:
Student

Relational Relation:
Student
```

These two may look related, but they are not the same object.

They require separate identities:

```text
EntityId
RelationId
```

ERDFlow therefore needs an explicit way to answer:

```text
Where did this object come from?
What did this object generate?
Which conversion rule created it?
Which upstream object does this downstream object correspond to?
```

This information is required for:

- deterministic conversion,
- safe regeneration,
- three-way review,
- controlled Schema → Conceptual propagation,
- explanations,
- debugging,
- undo/redo,
- future AI validation,
- future VS Code integration.

---

## 2. Decision

ERDFlow will use:

> **Explicit cross-level mappings and provenance records based on stable IDs.**

Mappings connect different semantic objects across modeling levels.

Conceptually:

```text
Conceptual Entity E-1
      ↓ generated as
Relational Relation R-8
```

The identity relationship is stored explicitly.

It is not inferred from names.

---

## 3. Core Rule

Different modeling levels use different typed IDs.

Example:

```text
EntityId       = E
RelationId     = R
TableId        = T
```

Lineage is represented through mappings:

```text
E → R
R → T
```

Do not reuse:

```text
EntityId
```

as:

```text
RelationId
```

even when both objects have the same visible name.

---

## 4. Why Mapping Exists

Without explicit mappings, ERDFlow would have to guess lineage using:

```text
names
positions
column order
similarity
```

Example:

```text
Conceptual:
Student

Schema:
Student
```

A name match may look obvious.

But after a user renames the Schema relation:

```text
Student
→ UniversityStudent
```

name-based matching breaks.

Stable mappings preserve continuity.

---

## 5. Mapping vs Provenance

ERDFlow distinguishes two closely related ideas.

### Mapping

Answers:

```text
Which source and target objects are connected?
```

Example:

```text
EntityId E
→ RelationId R
```

### Provenance

Answers:

```text
Why/how was the target created?
```

Example:

```text
Relation R
was generated from
Entity E
using
StrongEntityToRelation rule
```

Mappings express lineage.

Provenance explains lineage.

---

## 6. Mapping Record

Conceptually:

```text
CrossLevelMapping
├── MappingId
├── SourceLevel
├── SourceElementRef
├── TargetLevel
├── TargetElementRef
├── MappingKind
├── ConversionRuleId?
├── Origin
└── Metadata?
```

The exact C++ structure is deferred.

---

## 7. Element Reference

A mapping must preserve both:

```text
semantic type
stable ID
```

Conceptually:

```text
ElementReference
├── ElementKind
└── StableId
```

Examples:

```text
Entity / EntityId
Attribute / AttributeId
Relationship / RelationshipId
Relation / RelationId
SchemaAttribute / SchemaAttributeId
Table / TableId
Column / ColumnId
```

This prevents invalid cross-type interpretation.

---

## 8. Mapping ID

Each persistent mapping has its own:

```text
MappingId
```

using the stable identity strategy from ADR-001.

Mappings themselves may need to:

- persist,
- be updated,
- be removed,
- be reviewed,
- participate in regeneration.

Therefore they deserve stable identity.

---

## 9. Source and Target Levels

Initial levels include:

```text
Conceptual
Relational
Physical
```

SQL generation is usually output, not a persistent semantic level requiring one mapping per SQL token.

Data Workspace rows are also not automatically part of cross-level model lineage.

---

## 10. One-to-One Mapping

Example:

```text
Conceptual Entity
      ↓
Relational Relation
```

Mapping:

```text
E1 → R1
```

This is a simple one-to-one lineage case.

---

## 11. One-to-Many Mapping

Some transformations produce several targets.

Example:

```text
Conceptual Multivalued Attribute
PhoneNumber
```

may produce:

```text
Relation StudentPhone
Column StudentId
Column PhoneNumber
```

One source may therefore participate in several mappings.

Do not assume every source has only one target.

---

## 12. Many-to-One Mapping

Some generated structures may derive from multiple upstream elements.

Example:

```text
weak entity identification
```

may combine:

```text
owner key
+
partial key
```

into a generated relational key structure.

Therefore mappings must support multiple source elements contributing to one target.

---

## 13. Many-to-Many Mapping

Complex transformations may involve several source and target elements.

The model must not artificially force all lineage into a single one-to-one pointer.

Mappings may therefore form a graph.

---

## 14. Mapping Graph

Conceptually:

```text
Conceptual
   E1
   A1
   REL1
    │
    ▼
Relational
   R1
   SA1
   FK1
    │
    ▼
Physical
   T1
   C1
   IDX1
```

ERDFlow can traverse this graph when needed.

---

## 15. M:M Example

Conceptual:

```text
Student M:M Course
Relationship: Enrollment
RelationshipId = REL-1
```

Conversion:

```text
Enrollment Relation
RelationId = R-10
```

Mapping:

```text
REL-1
   ↓
R-10
```

Additional mappings may connect:

```text
Student key attribute
→ Enrollment.StudentId

Course key attribute
→ Enrollment.CourseId

Relationship attribute Grade
→ Enrollment.Grade
```

---

## 16. Strong Entity Example

Conceptual:

```text
Entity:
Student
EntityId = E-1
```

Relational:

```text
Relation:
Student
RelationId = R-1
```

Mapping:

```text
E-1
→
R-1
```

Provenance:

```text
Rule:
StrongEntityToRelation
```

---

## 17. Attribute Example

Conceptual:

```text
Attribute:
Name
AttributeId = A-4
```

Relational:

```text
SchemaAttribute:
Name
SchemaAttributeId = SA-8
```

Mapping:

```text
A-4 → SA-8
```

---

## 18. Physical Example

Relational:

```text
SchemaAttributeId = SA-8
LogicalType = Text
```

Physical:

```text
ColumnId = C-21
PhysicalType = VARCHAR(100)
```

Mapping:

```text
SA-8 → C-21
```

Provenance may include:

```text
LogicalTextToVarchar
```

---

## 19. Names Are Not Mapping Keys

Wrong:

```text
Student → Student
```

based only on matching text.

Correct:

```text
EntityId E-1
→
RelationId R-1
```

Names are useful for display.

Stable IDs are authoritative.

---

## 20. Mapping Kind

Possible conceptual mapping kinds include:

```text
GeneratedFrom
EquivalentTo
ExpandedFrom
CollapsedFrom
DerivedFrom
MaterializedFrom
UserLinked
```

The exact list is not locked yet.

Do not create dozens of mapping kinds before real transformation rules exist.

---

## 21. Conversion Rule Identity

A mapping may record which deterministic rule created it.

Example:

```text
Rule:
ManyToManyToJunctionRelation
```

The exact representation may be:

```text
enum
string code
typed rule identifier
```

This is deferred.

The important requirement is stable, explainable rule identity.

---

## 22. Rule Labels Must Be Stable

If mappings persist:

```text
conversion rule identity
```

must not depend on user-facing localized text.

Bad persisted identity:

```text
"Convert many-to-many relationship"
```

Preferred:

```text
rule code:
rel.many_to_many.junction
```

Human-readable explanation can be separate.

---

## 23. Mapping Origin

Mappings may come from different origins.

Possible origins:

```text
Generated
Imported
UserConfirmed
Migrated
Recovered
```

Exact values are deferred.

Origin helps explain whether a connection was created deterministically or manually.

---

## 24. Generated vs User-Created Downstream Objects

A Schema may contain:

```text
generated objects
+
manual Schema-only objects
```

A manual object may have no Conceptual source.

Example:

```text
technical helper relation
```

This is valid.

Do not force every downstream object to have an upstream mapping.

---

## 25. Unmapped Objects

An object may be:

```text
unmapped
```

for legitimate reasons.

Examples:

- manually created Schema relation,
- technical Physical index,
- imported object with no conceptual equivalent.

Unmapped does not mean invalid.

It means ERDFlow has no established cross-level lineage.

---

## 26. Mapping Completeness

Different transformations may have different mapping completeness.

Example:

```text
Entity
→ Relation
```

may be completely mapped.

A complex imported structure may have only partial lineage.

ERDFlow should distinguish:

```text
known mapping
unknown mapping
ambiguous mapping
```

rather than inventing certainty.

---

## 27. Ambiguous Mapping

If ERDFlow cannot determine the correct lineage safely:

```text
do not guess
```

Return an unresolved/ambiguous mapping condition for review.

This follows the broader:

```text
Detect → Explain → Ask → Apply
```

principle.

---

## 28. Mapping Creation

Mappings are created when:

- deterministic conversion creates downstream objects,
- approved propagation establishes a known relation,
- import/migration explicitly establishes lineage,
- a future user-linking workflow deliberately connects objects.

Mappings should not appear as accidental side effects of rendering.

---

## 29. Mapping Deletion

If a semantic object is permanently removed, mappings referencing it must be handled consistently.

Possible behavior:

```text
remove associated mappings
```

or later:

```text
retain historical tombstone
```

for advanced history.

V1 direction:

> remove live mappings that reference deleted live objects, while undo restores them when appropriate.

Historical tombstone behavior is deferred.

---

## 30. Undo and Mapping

If a command creates or removes a mapped object, undo must restore mapping state consistently.

Example:

```text
Delete Relation R
```

also removes:

```text
mapping E → R
```

Undo restores:

```text
R
+
mapping E → R
```

---

## 31. Redo and Mapping

Redo reuses the same semantic identities and restores/removes the same mapping relationships.

Do not generate unrelated replacement mapping IDs during ordinary redo.

---

## 32. Regeneration Continuity

When a new conversion candidate represents the same continuing downstream object, ERDFlow should preserve the existing target identity where lineage proves continuity.

Example:

```text
E1 → R1
```

Conceptual E1 changes slightly.

New candidate still represents the same relation.

Keep:

```text
R1
```

rather than replacing it with:

```text
R2
```

without reason.

---

## 33. Why Target Identity Reuse Matters

If every regeneration creates new downstream IDs:

```text
manual edits
mappings
baseline comparison
references
undo
```

become much harder to preserve.

Stable target identity is therefore important when semantic continuity exists.

---

## 34. Genuinely New Target

If the Conceptual change introduces a genuinely new downstream object:

```text
new target ID
```

is correct.

Example:

```text
new multivalued attribute
→ new relation
```

The new relation gets a new `RelationId`.

---

## 35. Removed Target

If an upstream change means a previously generated target should no longer exist, the regeneration proposal should identify:

```text
candidate removal
```

It should not immediately delete the user's current object.

That decision belongs to review/change management.

---

## 36. Mapping Does Not Mean Automatic Deletion

A mapping says:

```text
these objects are related by lineage
```

It does not mean:

```text
if source disappears, target must be silently deleted
```

Change policy is handled by ADR-010.

---

## 37. Mapping Does Not Mean Two-Way Sync

Mappings enable understanding in both directions.

They do **not** create a continuous synchronization engine.

Example:

```text
Schema rename
```

can use mapping to discover a possible Conceptual equivalent.

But the user still controls whether the change propagates.

---

## 38. Forward Navigation

ERDFlow should support:

```text
source → targets
```

Example:

```text
EntityId
→ generated RelationIds
```

This is needed for conversion review and explanation.

---

## 39. Reverse Navigation

ERDFlow should also support:

```text
target → sources
```

Example:

```text
SchemaAttributeId
→ originating AttributeId
```

This is needed for controlled upstream propagation.

---

## 40. Lookup Performance

Common mapping lookup must not repeatedly scan the full Project.

The architecture should support indexed lookup such as:

```text
source ID → mapping IDs
target ID → mapping IDs
mapping ID → mapping
```

Exact containers are deferred.

---

## 41. Mapping Registry

A future internal structure may resemble:

```text
MappingRegistry
├── byMappingId
├── bySource
└── byTarget
```

This is illustrative.

The architecture requires efficient lookup, not this exact class.

---

## 42. Scale Rule

As defined in SCALE.md:

> Common stable-ID and mapping lookups must avoid repeated full-model scans when indexed lookup is practical.

This ADR makes that requirement concrete for cross-level lineage.

---

## 43. Persistence

Mappings/provenance that are required for project continuity must persist in `.erdx`.

Example:

```text
save
close
reopen
```

must not erase:

```text
Conceptual → Schema lineage
```

if Schema regeneration depends on it.

---

## 44. Persistence Boundary

Mappings are Domain/project-semantic information.

But serialization is still handled through:

```text
ProjectDocumentMapper
```

from ADR-005.

Do not add:

```text
Mapping::toJson()
```

to the Domain merely because mappings persist.

---

## 45. Baselines

Generation baselines should preserve the mapping state associated with the generated downstream model.

Conceptually:

```text
Baseline
├── generated target state
└── mapping snapshot/reference
```

This is necessary for safe three-way comparison later.

---

## 46. Three-Way Review

Future regeneration uses:

```text
Previous Generated Baseline
+
Current User-Edited Schema
+
New Candidate Schema
```

Mappings help answer:

```text
Which old and new elements are the same continuing semantic target?
Which target came from which source?
Which user changes are downstream-only?
```

---

## 47. Mapping and Diff

Mappings reduce the burden on generic structural diff.

Example:

Without mapping:

```text
Student → UniversityStudent
```

may look like:

```text
delete Student
add UniversityStudent
```

With stable IDs/mapping, ERDFlow can recognize:

```text
rename
```

or preserved continuity.

---

## 48. Mapping and Commands

Semantic commands know intent at edit time.

Example:

```text
RenameRelation(R1)
```

Mappings can later help determine:

```text
R1 came from Entity E1
```

This enables classification for potential upstream propagation.

---

## 49. Mapping and Validation

Validation may detect:

```text
dangling mapping
duplicate mapping
invalid type combination
missing target
missing source
```

These are mapping integrity issues.

---

## 50. Dangling Mapping

Example:

```text
Mapping M1:
Source = E1
Target = R1
```

but:

```text
R1 does not exist
```

This is invalid live mapping state.

It should be reported.

---

## 51. Duplicate Mapping

Two identical mappings may be redundant.

Example:

```text
E1 → R1
E1 → R1
```

The system should avoid accidental duplicates unless the mapping model explicitly permits distinct semantic reasons.

Exact uniqueness rule is deferred.

---

## 52. Mapping Type Validation

Example invalid relation:

```text
ColumnId
→ EntityId
```

may be invalid for a mapping kind expecting:

```text
SchemaAttribute → Column
```

Mapping validation should be aware of element kinds.

---

## 53. Provenance Explanation

The UI may later display:

```text
Enrollment was generated from the M:M relationship
Student ↔ Course
using the Junction Relation rule.
```

The Domain/Core provides structured facts.

Presentation turns them into human-readable explanation.

---

## 54. Provenance Is Not UI Text

Do not persist only:

```text
"Generated because this was many-to-many."
```

as the authoritative provenance.

Persist structured data:

```text
Source IDs
Target IDs
Rule Code
Mapping Kind
```

Human-readable text is derived.

---

## 55. Provenance and Debugging

When conversion behaves unexpectedly, provenance enables inspection:

```text
Why does this FK exist?
```

ERDFlow can answer:

```text
generated from Relationship REL-12
using 1:M Foreign Key rule
```

This greatly improves diagnosability.

---

## 56. Provenance and AI

Future AI may suggest a model.

AI-generated proposals must still enter through normal validation/review.

If accepted, ERDFlow may store provenance such as:

```text
Origin = AIProposal
```

but AI provider-specific details should not pollute core mapping semantics.

---

## 57. Provenance and Import

Imported SQL may create:

```text
Physical/Table objects
```

with source provenance such as:

```text
ImportedFromSql
```

This does not automatically establish Conceptual lineage.

Later reverse-engineering may propose additional mappings.

---

## 58. Provenance and Manual Edits

A manually created Schema relation can have:

```text
Origin = Manual
```

with no Conceptual source.

Later, if the user explicitly links it to a Conceptual entity, a mapping may be added.

---

## 59. Provenance Is Not Audit History

Provenance answers:

```text
where did this model element come from?
```

It is not a complete permanent record of every user action.

Undo history and future audit history are separate concerns.

---

## 60. Provenance Is Not Version Control

Git history, future collaboration history, and mapping provenance solve different problems.

Do not turn mapping records into a general version-control system.

---

## 61. Mapping Granularity

Mappings should be as granular as needed to preserve meaningful lineage.

Examples:

```text
Entity → Relation
Attribute → SchemaAttribute
Relationship → ForeignKey/JunctionRelation
SchemaAttribute → Column
```

Do not map every trivial presentation detail.

---

## 62. Layout Is Not Cross-Level Provenance

Canvas positions and connector bend points are layout state.

They are not semantic cross-level mappings.

Example:

```text
Entity position
```

does not map to:

```text
Table position
```

unless a later layout feature explicitly creates such behavior.

---

## 63. SQL Output

Generated SQL may be traceable to Physical elements.

However, V1 does not require persistent IDs for individual SQL tokens or text spans.

Possible future source-map-like SQL provenance is deferred.

---

## 64. Data Rows

Rows in the Data Workspace do not automatically participate in Conceptual/Schema/Physical mapping.

Data lineage is a different problem.

Do not overload the same mapping system prematurely.

---

## 65. Mapping Mutation

Mapping creation/removal should occur through controlled Domain/Application operations.

Avoid:

```text
Qt view creates mapping directly
```

Mapping state is project state.

---

## 66. Mapping Ownership

Mappings belong conceptually to the Project.

Example:

```text
Project
├── ConceptualModel
├── RelationalSchema
├── PhysicalModel
└── CrossLevelMappings
```

The exact owning class/container is deferred.

---

## 67. Mapping Lifetime

A live mapping remains valid while:

```text
source exists
target exists
lineage remains meaningful
```

If meaning changes, the mapping may need replacement or review.

---

## 68. Mapping Reclassification

A mapping may change from:

```text
GeneratedFrom
```

to another state after manual intervention.

Whether mapping kind is mutable or replaced by a new record is deferred.

---

## 69. Manual Override

If a user deliberately changes lineage, ERDFlow should record that explicitly.

Example:

```text
User remaps Schema relation R2
to Conceptual Entity E1
```

This should not masquerade as an untouched deterministic mapping.

---

## 70. Ambiguous Reverse Propagation

Suppose:

```text
Schema Attribute SA1
```

maps to multiple Conceptual sources.

Then:

```text
Rename SA1
```

may not have a single safe upstream equivalent.

Result:

```text
Ambiguous
→ explain
→ require review
```

Do not guess.

---

## 71. Mapping and Controlled Propagation

Possible flow:

```text
Schema edit
      ↓
Find target mappings
      ↓
Find Conceptual sources
      ↓
Classify semantic equivalence
      ↓
Explain
      ↓
Ask
      ↓
Apply if approved
```

This is why reverse lookup is required.

---

## 72. Mapping and Physical-Only Changes

Example:

```text
Add Index
```

may have:

```text
no Conceptual source
```

That is expected.

Mapping absence prevents inappropriate upstream propagation.

---

## 73. Mapping and Generated Technical Columns

A technical FK column may derive from:

```text
Conceptual relationship
+
target entity key
```

Its provenance may therefore reference multiple source elements/rules.

The model must permit this.

---

## 74. Mapping and Specialization

ISA conversion can produce different relational strategies.

Example:

```text
Person
├── Student
└── Employee
```

Depending on selected strategy, several Relations may be generated.

Mappings/provenance must make the chosen strategy explicit.

---

## 75. Mapping and Weak Entities

Weak-entity conversion may involve:

```text
Weak Entity
Owner Entity
Partial Key
Identifying Relationship
```

all contributing to target relational structures.

This is another reason not to force one-to-one mapping only.

---

## 76. Mapping and Composite Attributes

Composite attribute:

```text
Name
├── FirstName
└── LastName
```

may map to:

```text
FirstName column
LastName column
```

The parent composite attribute may have expansion provenance without a direct single target attribute.

---

## 77. Mapping and Derived Attributes

A derived attribute may:

```text
not be persisted downstream
```

or may map to:

```text
generated expression/computed column
```

depending on physical strategy.

Absence of target can be a valid conversion outcome.

---

## 78. No Mapping Required for Every Source

Some Conceptual elements may intentionally have no downstream materialization.

Therefore:

```text
source has no target
```

is not automatically an error.

The conversion rule should explain why.

---

## 79. Conversion Result

Once mapping/provenance is implemented, conversion results should include:

```text
RelationalConversionResult
├── CandidateSchema
├── Mappings
├── AppliedRules
├── ValidationIssues
└── UnresolvedDecisions
```

This replaces any design where mappings are reconstructed later by guessing.

---

## 80. Candidate Mappings

Mappings generated during background conversion are initially candidate data.

They become live project mapping state only if the candidate/review is accepted.

This follows ADR-007.

---

## 81. Stale Candidate Mapping

If a conversion result is stale:

```text
its candidate mappings are also stale
```

Do not merge them into the live Project independently.

---

## 82. Mapping Versioning

Mappings may need revision/baseline context.

The exact version fields are deferred.

But future safe regeneration must be able to distinguish:

```text
mapping from baseline
mapping in current project
mapping in new candidate
```

---

## 83. Mapping Persistence Version

Mapping serialization evolves under `.erdx` format versioning.

Do not create an independent incompatible persistence version system unless necessary.

---

## 84. Mapping Index Rebuild

Derived lookup indexes need not all be persisted.

Example:

```text
bySource index
byTarget index
```

may be rebuilt from mapping records on load.

Persist semantic mapping data.

Rebuild performance indexes where practical.

---

## 85. Memory Strategy

Mappings add memory overhead.

Possible structures:

```text
mapping record
source index
target index
baseline mapping state
candidate mapping state
```

This overhead is a known scale cost.

Measure before optimizing.

---

## 86. Compact IDs

ADR-001's 128-bit in-memory IDs are suitable for mapping indexes.

Avoid permanently storing all mapping keys as duplicated long strings in memory if a compact UUID representation is available.

---

## 87. Query API

Future core queries may conceptually include:

```text
targetsOf(sourceRef)
sourcesOf(targetRef)
mappingsFor(elementRef)
mappingById(mappingId)
```

Exact API is deferred.

---

## 88. No UI Dependency

Mapping query APIs must not return:

```text
QGraphicsItem*
QTreeWidgetItem*
```

They return semantic references/data.

Presentation resolves those references into views.

---

## 89. No Persistence Dependency

Mapping logic must not require JSON objects.

Persistence adapters convert mapping records separately.

---

## 90. No Database Driver Dependency

Mappings are internal ERDFlow project semantics.

They do not depend on PostgreSQL/MySQL/etc.

---

## 91. Future Rust Compatibility

Mappings are excellent candidates for a language-neutral representation.

At a future Rust boundary, exchange:

```text
stable IDs
element kinds
mapping kinds
rule codes
```

rather than Qt types or C++ pointers.

---

## 92. Future VS Code Compatibility

A VS Code extension can use mapping information to provide:

```text
Go to generated relation
Show conceptual source
Explain generated SQL/table
```

without recreating lineage rules.

---

## 93. Alternative A — Reuse Same ID Across Levels

Example:

```text
EntityId E1
also used as RelationId
```

Rejected.

Reason:

- different abstractions are different objects,
- one source may generate many targets,
- many sources may generate one target,
- type safety is lost,
- future transformations become ambiguous.

---

## 94. Alternative B — Name-Based Matching

Rejected.

Names are mutable and not necessarily unique.

Rename would destroy lineage.

---

## 95. Alternative C — Recompute Mapping Every Time

Rejected as the primary strategy.

Some deterministic mappings can be recomputed, but manual downstream edits and regeneration continuity require persistent lineage.

---

## 96. Alternative D — Store Only Source ID on Target Object

Example:

```text
Relation.sourceEntityId
```

Too limited.

Why:

- one-to-many,
- many-to-one,
- rule provenance,
- manual linking,
- multi-level relationships

all require a richer structure.

---

## 97. Alternative E — Full General Graph Database

Rejected.

Mappings form a graph conceptually, but ERDFlow does not need a graph-database subsystem.

Simple typed records + indexes are sufficient unless evidence proves otherwise.

---

## 98. Alternative F — No Provenance, Only Mapping

Rejected.

Mapping alone says:

```text
A → B
```

but not:

```text
why
by which rule
through which origin
```

ERDFlow needs explanations and review.

---

## 99. Alternative G — Persist Human Explanations Only

Rejected.

Human text is not structured enough for reliable regeneration or propagation.

---

## 100. Consequences — Positive

This decision gives ERDFlow:

- reliable lineage,
- rename-safe tracking,
- explainable conversion,
- safer regeneration,
- better diff quality,
- controlled back-propagation,
- future navigation across stages,
- future AI validation context,
- future VS Code integration,
- strong debugging capability.

---

## 101. Consequences — Costs

This introduces:

- additional project data,
- mapping indexes,
- persistence complexity,
- mapping lifecycle rules,
- extra validation,
- additional memory usage.

These costs are necessary for ERDFlow's progressive-refinement model.

---

## 102. Initial Implementation Scope

Do not implement the full mapping system during the first Conceptual Editor milestone.

Mapping implementation begins with:

```text
Conceptual → Relational conversion
```

Initial supported cases should include:

```text
Entity → Relation
Attribute → SchemaAttribute
Relationship → generated FK or Junction Relation
Relationship Attribute → SchemaAttribute
```

Add Physical mappings later.

---

## 103. Initial Tests

### Test 1 — Entity Mapping

```text
Entity E
→ Relation R
```

lookup forward and reverse.

### Test 2 — Rename

Rename E.

Mapping remains:

```text
E → R
```

### Test 3 — M:M

```text
Relationship REL
→ Junction Relation R
```

mapping exists with correct rule.

### Test 4 — Delete / Undo

Delete mapped target.

Mapping removed.

Undo restores both target and mapping.

### Test 5 — Persistence

Save/load preserves mapping IDs and source/target IDs.

### Test 6 — Invalid Mapping

Mapping to missing target fails validation.

### Test 7 — One-to-Many

One source maps to multiple valid targets.

### Test 8 — Reverse Lookup

Target finds source without full-model scan.

---

## 104. Architecture Invariants

### Invariant 1

Different modeling levels use distinct stable identities.

### Invariant 2

Cross-level lineage is explicit.

### Invariant 3

Mappings use stable IDs, not names.

### Invariant 4

Mappings support one-to-one, one-to-many, and many-to-one relationships where required.

### Invariant 5

Provenance records structured origin/rule information.

### Invariant 6

Mappings do not imply continuous bidirectional synchronization.

### Invariant 7

Unmapped downstream objects are valid when intentionally manual/technical.

### Invariant 8

Ambiguous lineage is not guessed.

### Invariant 9

Mappings required for continuity persist in `.erdx`.

### Invariant 10

Common source/target lookup is indexed or otherwise efficient.

### Invariant 11

Candidate mappings from background work are applied only with the accepted candidate.

### Invariant 12

Mapping state participates in undo when the associated semantic change is undoable.

---

## 105. Relationship to Other ADRs

Depends on:

```text
ADR-001 — Stable Identity Strategy
ADR-002 — Layered Architecture and Dependency Direction
ADR-004 — Core Language Strategy
ADR-005 — Project File and Persistence Boundary
ADR-006 — Command / Undo Strategy
ADR-007 — Background Work / Concurrency Strategy
```

This ADR is a prerequisite for:

```text
ADR-009 — Validation Gates and Conversion Policy
ADR-010 — Change Propagation and Regeneration
```

and later conversion/regeneration implementation.

---

## 106. Open Implementation Decisions

This ADR intentionally leaves open:

- exact `ElementReference` C++ representation,
- exact `MappingKind` enum,
- exact `Origin` enum,
- exact rule-code representation,
- exact mapping registry class,
- exact duplicate-mapping policy,
- exact baseline mapping snapshot representation,
- exact historical/tombstone behavior,
- exact user-driven remapping UI,
- exact SQL-source-map support.

These should be decided when implementation reaches the corresponding feature.

---

## 107. Decision Outcome

ERDFlow adopts:

```text
typed stable IDs
+
explicit cross-level mapping records
+
structured provenance
+
forward and reverse lineage lookup
+
persistent mapping continuity
```

as the foundation for progressive refinement across:

```text
Conceptual
→ Relational
→ Physical
```

---

## 108. Final Principle

> ERDFlow must never have to guess which downstream object came from which upstream idea when that lineage can be known and recorded explicitly.
