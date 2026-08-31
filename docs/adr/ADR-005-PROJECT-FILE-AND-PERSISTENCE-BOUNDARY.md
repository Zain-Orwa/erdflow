# ADR-005 — Project File and Persistence Boundary

**Status:** Proposed  
**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** Native `.erdx` project persistence, serialization boundaries, versioning, loading, saving, and migration responsibilities

---

## 1. Context

ERDFlow is a project-based desktop application.

A project may eventually contain:

```text
Conceptual Model
Relational Schema
Physical Model
Layout information
Mappings / Provenance
Generation Baselines
Project settings
Data Workspace metadata
Imported-source metadata
```

The project must survive:

```text
save
close
application restart
load
future ERDFlow upgrades
cross-platform transfer
```

The persistence system therefore cannot be treated as a simple:

```text
"dump whatever C++ objects exist today"
```

operation.

The persisted project format must remain stable enough to outlive internal
class refactoring.

---

## 2. Decision

ERDFlow will use a dedicated native project format:

```text
.erdx
```

with an explicit persistence boundary:

```text
Domain Project
      ↓
ProjectDocumentMapper
      ↓
Versioned ErdxDocument
      ↓
Serializer
      ↓
.erdx file
```

Loading follows the reverse conceptual direction:

```text
.erdx file
      ↓
Parser / Decoder
      ↓
Versioned ErdxDocument
      ↓
Migration if required
      ↓
ProjectDocumentMapper
      ↓
Validated Domain Project
```

The Domain does **not** serialize itself directly.

---

## 3. Core Rule

Persistence representation and Domain representation are separate.

Wrong:

```cpp
class Entity
{
public:
    QJsonObject toJson() const;
    void fromJson(const QJsonObject&);
};
```

Preferred:

```text
Entity
   ↓
ProjectDocumentMapper
   ↓
ErdxEntityDocument
```

The Domain describes database meaning.

The persistence layer describes how that meaning is stored.

---

## 4. Why This Separation Matters

Without this boundary, changing a Domain class can silently become a
file-format breaking change.

Example:

Today:

```text
Entity
├── id
├── name
└── attributes
```

Later:

```text
Entity
├── id
├── displayName
├── description
└── attributes
```

If Domain classes serialize themselves directly, internal refactoring and
file-format evolution become tightly coupled.

With a mapper:

```text
Domain vNext
      ↕ mapper
ErdxDocument v1/v2
```

the application can evolve both sides deliberately.

---

## 5. Native Project Extension

The official ERDFlow project extension is:

```text
.erdx
```

Example:

```text
University.erdx
Hospital.erdx
StoreDesign.erdx
```

`.erdx` represents an editable ERDFlow project.

It is different from exported formats such as:

```text
.sql
.png
.pdf
.csv
.json
```

---

## 6. `.erdx` Is the Working Project Format

The `.erdx` file is not merely an export.

It is the native working document.

It must eventually preserve enough state for the user to continue working.

Examples include:

```text
semantic models
stable IDs
diagram layout
mappings/provenance
generation baselines
project-level settings
```

Ephemeral UI state should not automatically be persisted.

---

## 7. File Format Version

Every `.erdx` document must contain an explicit format version.

Conceptually:

```json
{
  "format_version": 1
}
```

This version identifies the persisted document schema.

It does not identify:

```text
application version
Conceptual model revision
Schema revision
Physical model revision
```

Those are different concepts.

---

## 8. File Format Version vs Application Version

Example:

```text
ERDFlow application:
0.8.0
```

may still write:

```text
.erdx format_version = 2
```

Several ERDFlow releases may share the same file-format version.

Only bump the file-format version when persisted representation changes in
a way that requires migration/compatibility handling.

---

## 9. File Format Version vs Model Revision

Generation/change-management may later track:

```text
Conceptual Revision
Schema Revision
Physical Revision
Baseline Revision
```

These are project history/state concepts.

They must not be confused with:

```text
format_version
```

which describes how the file is encoded.

---

## 10. Initial Document Shape

The exact `.erdx` encoding is not locked by this ADR.

Conceptually, however, the document will contain sections such as:

```text
ErdxDocument
├── FormatVersion
├── ProjectMetadata
├── ConceptualDocument
├── RelationalDocument
├── PhysicalDocument
├── LayoutDocument
├── MappingDocument
├── BaselineDocument
├── ProjectSettingsDocument
└── DataWorkspaceMetadataDocument
```

Only sections corresponding to implemented features need to exist initially.

---

## 11. Incremental Persistence Scope

The first `.erdx` implementation should persist only the product that
actually exists.

For the early Conceptual Editor:

```text
Project metadata
Stable IDs
Entities
Attributes
Relationships
Relationship participants
Cardinality
Participation
Basic comments
Layout positions
Pages when available
```

Do not invent empty Physical/SQL/Data sections purely because the future
architecture mentions them.

---

## 12. Project Document Mapper

`ProjectDocumentMapper` translates between:

```text
Domain Project
```

and:

```text
ErdxDocument
```

Responsibilities include:

- mapping stable IDs,
- mapping semantic model values,
- mapping layout state,
- interpreting optional persisted sections,
- validating representational assumptions,
- preserving version-specific meaning.

It should not perform low-level file I/O.

---

## 13. Serializer

The Serializer converts:

```text
ErdxDocument
```

to/from the chosen encoded file representation.

Responsibilities include:

```text
encode
decode
syntax-level errors
document structure parsing
```

It does not decide database semantics.

---

## 14. Project Store

Application code should use a storage port.

Conceptually:

```cpp
class ProjectStore
{
public:
    virtual ~ProjectStore() = default;

    virtual LoadProjectResult load(/* source */) = 0;
    virtual SaveProjectResult save(
        /* destination */,
        const Project& project) = 0;
};
```

The exact signature is deferred.

The architectural rule is:

```text
Application depends on ProjectStore abstraction
```

not directly on:

```text
QFile
QSaveFile
std::fstream
JSON parser
archive library
```

---

## 15. ErdxProjectStore

Infrastructure provides:

```text
ErdxProjectStore
```

Conceptual responsibilities:

```text
Application
    ↓
ProjectStore
    ↑
ErdxProjectStore
    ↓
Mapper
    ↓
Serializer
    ↓
Filesystem
```

This is consistent with ADR-002.

---

## 16. Safe Save Strategy

Saving must protect the existing project from partial writes.

Preferred flow:

```text
Serialize new project
      ↓
Write temporary destination
      ↓
Flush / complete write
      ↓
Commit/replace original
```

If writing fails:

```text
existing .erdx remains intact
```

---

## 17. Qt QSaveFile

For the Qt desktop Infrastructure adapter, `QSaveFile` is a strong candidate
for full-document saves.

Its write-to-temporary-and-commit behavior matches ERDFlow's safe-save
requirement.

Important:

```text
QSaveFile
```

belongs in Infrastructure.

It must not leak into the Domain.

---

## 18. Save Is a Use Case

User action:

```text
Ctrl+S
```

does not serialize Domain classes directly from `MainWindow`.

Preferred:

```text
Presentation
      ↓
SaveProject use case
      ↓
ProjectStore
      ↓
ErdxProjectStore
```

---

## 19. Load Is a Use Case

Similarly:

```text
Presentation
      ↓
OpenProject use case
      ↓
ProjectStore
      ↓
ErdxProjectStore
      ↓
Validated Project
      ↓
Presentation rebuilds views
```

The UI is reconstructed from the loaded Project.

The file does not reconstruct Qt widgets as persisted objects.

---

## 20. Stable IDs Must Survive Round Trip

ADR-001 requires identity preservation.

Therefore:

```text
Before Save:
EntityId = X
```

must become:

```text
After Load:
EntityId = X
```

The loader must not generate replacement IDs for valid persisted objects.

---

## 21. References Use IDs

Persisted references should use stable IDs.

Example conceptually:

```json
{
  "relationship_id": "...",
  "participants": [
    {
      "entity_id": "...",
      "cardinality": "many"
    }
  ]
}
```

Do not persist name-only references as authoritative identity.

---

## 22. Duplicate ID Validation

Loading a project containing duplicate IDs where uniqueness is required is
a structural error.

Example:

```text
Entity A → EntityId X
Entity B → EntityId X
```

must not be silently merged.

Load validation should report the invalid project structure.

---

## 23. Missing Reference Validation

Example:

```text
RelationshipParticipant
    entity_id = X
```

but no Entity with ID X exists.

This is a broken project reference.

The loader/mapping-validation stage must detect it.

---

## 24. Unknown Fields

Where practical, future `.erdx` readers should tolerate unknown fields that
do not affect required interpretation.

This improves forward evolution.

However, ERDFlow must not blindly ignore unknown structural data if doing so
could change database meaning.

Exact compatibility policy is defined with the concrete encoding.

---

## 25. Missing Optional Fields

Optional fields should have documented defaults.

Example:

```text
description missing
→ empty/none
```

Do not treat every new optional field as a mandatory migration.

---

## 26. Required Fields

Required structural fields include things such as:

```text
format version
required stable IDs
required object type information
required references
```

when necessary to reconstruct valid project meaning.

Missing required fields produce a structured load error.

---

## 27. Migration

When persisted representation changes:

```text
ErdxDocument v1
      ↓
Migration
      ↓
ErdxDocument v2
      ↓
Mapper
      ↓
Current Domain Project
```

Prefer migrating the persisted document representation rather than forcing
old representation rules into current Domain objects.

---

## 28. Migration Direction

Primary supported migration direction is:

```text
old → current
```

Automatic downgrade:

```text
new → old
```

is not required.

If export to an older format is ever needed, treat it as a separate feature.

---

## 29. Migration Chain

Conceptually:

```text
v1 → v2
v2 → v3
```

may be applied sequentially when loading a v1 project into software using
v3.

Whether migrations are stepwise or direct is an implementation decision.

The requirement is deterministic, tested migration.

---

## 30. Migration Must Preserve Identity

Format migration must preserve semantic stable IDs unless there is an
extraordinary documented reason.

Changing:

```text
EntityId
```

during ordinary file-format migration would destroy lineage.

---

## 31. Migration Tests

Every supported migration should have fixtures.

Example:

```text
tests/fixtures/erdx/v1/
```

and tests:

```text
load v1
migrate
validate
compare expected current representation
```

Do not rely only on manually opening old projects.

---

## 32. Unknown Future Version

If ERDFlow encounters:

```text
format_version = 9
```

but only understands through:

```text
format_version = 4
```

it must not guess.

Preferred behavior:

```text
This project was created by a newer ERDFlow file format.
This version cannot safely open it.
```

Structured error first; user-facing wording is Presentation responsibility.

---

## 33. Corrupted Project

A malformed `.erdx` file must not crash the application.

Load pipeline:

```text
Read
↓
Decode
↓
Version check
↓
Migrate
↓
Structural validation
↓
Map
↓
Domain validation
```

Failure at any stage returns a structured error.

---

## 34. Partial Load

The default initial policy is:

> Do not silently open a semantically incomplete project as if it were valid.

Recovery/partial-load tooling may be added later.

If corruption prevents authoritative reconstruction, report it clearly.

---

## 35. Autosave

Autosave is not decided by this ADR.

Future possibilities:

```text
periodic autosave
recovery snapshot
temporary recovery file
```

belong to reliability/recovery design later.

This ADR only requires safe explicit project saving.

---

## 36. Backups

Automatic project backup history is deferred.

Safe atomic replacement is the initial requirement.

Backups may later supplement atomic save.

---

## 37. Lock Files

Multi-process file locking behavior is not selected yet.

If users later open the same `.erdx` file simultaneously in multiple
processes, ERDFlow will need a conflict policy.

This is deferred.

---

## 38. Project Location

The Domain must not depend on an absolute filesystem path.

The active application session may know:

```text
/Users/.../University.erdx
```

The Project's semantic meaning does not depend on that OS path.

This helps portability.

---

## 39. Internal Paths

If `.erdx` later becomes a package/archive containing internal assets, use
portable logical paths.

Avoid storing machine-specific absolute paths where possible.

Example preferable:

```text
assets/image-12.png
```

rather than:

```text
/Users/zain/Desktop/image-12.png
```

---

## 40. External Resource References

If a project intentionally references an external resource, ERDFlow must
distinguish:

```text
embedded resource
external reference
```

and clearly define portability behavior.

This is deferred until images/attachments require it.

---

## 41. Encoding Decision

This ADR deliberately does not lock whether `.erdx` is:

```text
plain JSON
compressed JSON
ZIP-based package
binary container
another structured format
```

The first encoding should optimize for:

- correctness,
- debuggability,
- migration simplicity,
- cross-language interoperability,
- reasonable file size,
- future extension.

A dedicated implementation decision can select the encoding when Phase 15
begins.

---

## 42. Why Not Lock JSON Now

JSON is a strong candidate because it is:

- human-inspectable,
- mature,
- portable,
- easy to migrate,
- easy to consume from C++/Rust/TypeScript.

But future `.erdx` may need embedded:

```text
images
large sample data
binary metadata
```

A package/container may eventually be preferable.

Therefore this ADR locks the boundary and versioning, not the container
encoding.

---

## 43. Why `.erdx` Is Not Raw SQLite by Default

SQLite could store project state.

Advantages:

```text
transactions
structured queries
incremental changes
mature engine
```

But it also couples the native document format to a database storage model
and complicates simple migration/inspection for an early modeling project.

ERDFlow does not currently require random-access persistence of millions of
project objects.

Therefore SQLite is not selected as the default native project format at
this stage.

It may remain useful elsewhere.

---

## 44. Why Not Serialize C++ Memory

Raw memory dumps are rejected because they depend on:

```text
compiler
endianness
padding
class layout
pointer representation
implementation language
```

They would also block clean future Rust interoperability.

`.erdx` must be a logical document format, not a memory snapshot.

---

## 45. Why Not Use Qt Serialization as the Domain Contract

Qt may help implement the serializer.

But the `.erdx` specification must not be defined as:

```text
whatever QDataStream currently writes from our Qt classes
```

The persisted meaning should be framework-independent.

This permits future:

```text
Rust
VS Code tooling
CLI
migration utilities
```

to understand the format.

---

## 46. Deterministic Serialization

Where practical, equivalent project state should serialize predictably.

Useful properties may include:

- stable field names,
- canonical UUID strings,
- explicit enums,
- deterministic ordering of persisted collections where meaningful.

Do not depend on incidental hash-container iteration order.

This improves:

```text
tests
diffing
debugging
reproducibility
```

---

## 47. Semantic Ordering vs Serialization Ordering

Deterministic file ordering does not imply semantic database ordering.

A serializer may sort entities by ID or another stable rule to produce
predictable files.

That ordering does not define:

```text
canvas order
database row order
business order
```

---

## 48. Human Diff Friendliness

If the initial encoding is textual, ERDFlow should prefer reasonably
diff-friendly output.

This helps Git usage.

Example:

```text
.erdx committed to a repository
```

However, Git-friendliness must not override correctness or future packaging
needs.

---

## 49. Git Compatibility

ERDFlow projects may be stored in Git repositories.

The persistence design should therefore avoid unnecessary nondeterministic
changes that make every save rewrite unrelated content differently.

This does not promise perfect semantic merging of `.erdx` files.

Future source-control-aware tooling may build on stable IDs and structured
documents.

---

## 50. Sensitive Data

`.erdx` may eventually contain:

```text
sample rows
comments
connection metadata
import metadata
```

ERDFlow must not assume project files are non-sensitive.

Do not store secrets such as:

```text
database passwords
access tokens
API keys
```

directly in normal project documents unless a future security design
explicitly addresses secure storage.

---

## 51. Database Connection Credentials

Future database connection configuration must separate:

```text
non-secret connection metadata
```

from:

```text
credentials/secrets
```

Credentials should use an appropriate secure credential mechanism rather
than plain `.erdx` storage.

This is future work but the boundary must permit it.

---

## 52. Data Workspace Storage

Project-local/sample data may later be persisted in `.erdx`.

Large live database contents should not be copied into the project
automatically.

The project should store only the information required by the selected Data
Source behavior.

Exact local data encoding is deferred.

---

## 53. Mapping Persistence

Mappings/provenance are project-semantic information.

When implemented, they should persist with the project.

Example:

```text
EntityId E
→ RelationId R
```

must survive save/load if downstream models rely on the mapping.

---

## 54. Baseline Persistence

Generation baselines must survive save/load if they are required to protect
manual downstream edits after reopening a project.

Therefore baseline persistence is part of future `.erdx` evolution.

It should not be treated as temporary UI state.

---

## 55. Layout Persistence

Diagram layout is part of the working project experience.

Persist:

```text
node positions
connector geometry where needed
page association
diagram-specific visual state
```

Do not infer semantic meaning from layout.

Layout data remains separate from semantic model data.

---

## 56. Ephemeral UI State

Examples that generally do not belong in the project document:

```text
hover state
open context menu
temporary selection rectangle
drag preview
active tooltip
```

Some session conveniences such as active tab or zoom may be stored separately
as user/session preferences if desired.

---

## 57. Project Metadata

Potential project metadata includes:

```text
ProjectId
name
description
created_at
modified_at
```

Exact fields are defined incrementally.

Timestamps should not replace stable identity.

---

## 58. Created / Modified Time

If persisted, timestamps should use a cross-platform representation.

The exact serialized format is deferred, but it should be:

- unambiguous,
- timezone-aware where needed,
- independent of locale display formatting.

Do not persist:

```text
"31/08/26 04:20"
```

as the only authoritative machine-readable timestamp.

---

## 59. Save-Time Mutation

Saving should not unexpectedly modify semantic project content merely to
serialize it.

Example:

```text
Save
```

must not generate new semantic IDs.

File metadata such as:

```text
modified_at
```

may update deliberately.

---

## 60. Dirty State

Application/session state should track whether unsaved changes exist.

Conceptually:

```text
Project state changed
      ↓
dirty = true
```

successful save:

```text
dirty = false
```

The dirty flag is Application/session state.

It is not the authoritative method of detecting semantic differences.

---

## 61. Save Failure

On save failure:

```text
existing valid project file remains intact
dirty state remains true
error returned
```

The Application decides follow-up behavior.

Presentation displays the error.

---

## 62. Load Failure

On load failure:

```text
current open Project must not be destroyed
```

until a replacement project has been successfully loaded/validated.

Conceptually:

```text
load candidate
      ↓
validate
      ↓
success?
  yes → replace/open
  no  → keep current state
```

---

## 63. New Project

A new unsaved Project exists independently of a filesystem destination.

Example:

```text
New Project
ProjectId = X
Path = none
```

First save chooses:

```text
destination.erdx
```

Project identity does not depend on filename.

---

## 64. Save As

`Save As` changes the document destination.

It does not automatically mean:

```text
new ProjectId
```

The user is saving the same logical project to another location unless a
future explicit "Duplicate Project" feature says otherwise.

This distinction matters for identity.

---

## 65. Duplicate Project

A future explicit duplication/fork operation may intentionally create:

```text
new ProjectId
```

and potentially regenerate selected internal project-scoped identifiers
according to a defined policy.

That behavior is not part of ordinary Save As.

Deferred.

---

## 66. Import Is Not Load

Important distinction:

```text
Open .erdx
```

means:

```text
restore ERDFlow project
```

while:

```text
Import SQL/CSV/JSON
```

means:

```text
interpret external content and propose changes/new model data
```

Import follows ADR-011 later.

Do not combine import parsing with `.erdx` project loading.

---

## 67. Export Is Not Save

Similarly:

```text
Save
→ preserve editable ERDFlow project
```

```text
Export
→ produce another representation
```

Examples:

```text
SQL
PNG
PDF
CSV
JSON
```

Exports need not preserve all ERDFlow project information.

---

## 68. Persistence Error Types

Infrastructure should return structured failures.

Possible categories:

```text
FileNotFound
PermissionDenied
WriteFailed
DecodeFailed
UnsupportedFormatVersion
MigrationFailed
StructuralValidationFailed
ReferenceValidationFailed
```

Exact enum/type is deferred.

Do not make UI strings the persistence error model.

---

## 69. Validation Layers During Load

Loading should distinguish:

### Encoding/Syntax Validation

Can the file be decoded?

### Document Structural Validation

Are required document fields present and valid?

### Reference Validation

Do IDs/references point to valid objects?

### Domain Validation

Does the reconstructed project satisfy Domain invariants?

A project may be syntactically valid JSON yet semantically invalid.

---

## 70. Load Pipeline

Recommended conceptual pipeline:

```text
Read bytes
    ↓
Decode
    ↓
Check format_version
    ↓
Migrate document if needed
    ↓
Validate document structure
    ↓
Map to candidate Domain Project
    ↓
Validate references/invariants
    ↓
Return Project
```

---

## 71. Save Pipeline

Recommended conceptual pipeline:

```text
Current Domain Project
    ↓
Validate required save invariants
    ↓
ProjectDocumentMapper
    ↓
Current ErdxDocument
    ↓
Serialize
    ↓
Safe temporary write
    ↓
Commit
```

Not every modeling warning should block save.

Saving an incomplete design may still be perfectly legitimate.

Save validation and conversion-readiness validation are different policies.

---

## 72. Save Must Permit Work in Progress

ERDFlow is a design tool.

Users must be able to save incomplete models.

Example:

```text
Entity without a final identifier
```

may be invalid for conversion but valid as work-in-progress project state.

Therefore:

```text
conversion-blocking validation
```

does not automatically mean:

```text
save-blocking validation
```

Save should block only when project state cannot be represented safely.

---

## 73. Recovery of Work-in-Progress Models

Because incomplete design is valid project state, `.erdx` must preserve
unresolved design information where the Domain permits it.

This supports:

```text
save now
finish later
```

---

## 74. Cross-Platform Portability

A `.erdx` file created on:

```text
macOS
```

should be openable on:

```text
Linux
Windows
```

when the project uses supported features.

Avoid platform-specific binary representations in the logical format.

---

## 75. Endianness / Architecture Independence

The format must not depend on:

```text
x86_64
ARM64
machine endianness
pointer width
compiler ABI
```

This is another reason to use a structured document representation.

---

## 76. Future Rust Compatibility

The file format must be understandable independently of C++ classes.

Future Rust code should be able to read/write the same logical `.erdx`
schema.

This is one of the explicit reasons for:

```text
Domain
≠
Persistence representation
```

---

## 77. Future VS Code Compatibility

A future VS Code extension may need to:

```text
inspect
preview
validate
possibly edit
```

`.erdx` project information.

A documented, versioned, framework-independent format makes this possible.

No VS Code-specific persistence variant should be required.

---

## 78. File Specification

Once the concrete encoding is chosen, ERDFlow should maintain a document
describing the `.erdx` schema.

Possible location:

```text
docs/ERDX_FORMAT.md
```

This file is not required before the first persistence implementation.

---

## 79. Golden Fixtures

Persistence tests should include known project files.

Examples:

```text
minimal.erdx
basic_student_course.erdx
weak_entity.erdx
multi_page.erdx
```

as corresponding features are implemented.

---

## 80. Round-Trip Testing

Critical test:

```text
Domain Project A
      ↓ save/encode
.erdx
      ↓ load/decode
Domain Project B
```

Then verify semantically relevant equality.

Especially:

```text
stable IDs
references
cardinality
participation
layout
```

---

## 81. Determinism Test

For equivalent project state, repeated saves should not introduce
unnecessary random differences.

Examples of bad nondeterminism:

```text
random field order
random object ordering
new IDs on every save
irrelevant timestamps everywhere
```

---

## 82. Corruption Tests

Test malformed examples:

```text
invalid syntax
missing format_version
unsupported version
duplicate EntityId
relationship to missing EntityId
invalid enum value
truncated file
```

Expected behavior:

```text
structured error
no crash
no silent merge
```

---

## 83. Atomic Save Tests

Test:

```text
existing project A
attempt save B
simulate write failure
```

Expected:

```text
project A remains readable
```

Exact failure simulation depends on implementation.

---

## 84. Performance

Initial project files are expected to be moderate.

Do not build complex incremental persistence before measurements justify it.

First priority:

```text
correctness
safe saving
versioning
migration
portability
```

Later, if large projects make full-document serialization too slow, optimize
based on evidence.

---

## 85. Background Saving

Large save/load operations may later run through the background task system.

However:

- snapshot creation must be safe,
- completion/error handling must be clear,
- UI must not mutate destroyed project state.

The persistence boundary should remain callable independently of threading.

---

## 86. Snapshot During Save

Saving should operate from a consistent project state.

For early project sizes:

```text
consistent copy/snapshot
```

may be acceptable.

If snapshot creation becomes expensive, SCALE.md's snapshot strategy applies.

Persistence code must not race with live mutation.

---

## 87. Alternative A — Domain Objects Serialize Themselves

Rejected.

Reason:

- Domain becomes coupled to file representation,
- Qt/JSON libraries may leak inward,
- migrations become harder,
- internal refactoring threatens file compatibility.

---

## 88. Alternative B — Raw Binary C++ Serialization

Rejected.

Reason:

- compiler/ABI dependence,
- poor portability,
- poor migration,
- blocks future Rust/VS Code tooling,
- difficult debugging.

---

## 89. Alternative C — SQLite as Native Project File

Not selected for the initial project format.

It may be technically strong, but ERDFlow does not yet need transactional
random-access persistence for project-model scale.

A structured document is simpler to inspect, migrate, test, and exchange.

---

## 90. Alternative D — Plain JSON Permanently

Not yet selected.

JSON is the leading simple initial candidate.

But locking it permanently now may constrain future embedded binary assets or
large local data.

The concrete encoding will be decided when persistence implementation starts.

---

## 91. Alternative E — ZIP Package Immediately

Not selected yet.

A package format may become useful when projects contain:

```text
images
attachments
larger local data
multiple internal documents
```

But an archive layer may be unnecessary complexity for the first Conceptual
Editor.

---

## 92. Alternative F — One File Per Model

Example:

```text
conceptual.json
schema.json
layout.json
```

as loose project-directory files.

Not selected as the default user project representation.

ERDFlow should present a coherent native project file:

```text
University.erdx
```

Its internals may later contain multiple logical documents if packaged.

---

## 93. Consequences — Positive

This decision gives ERDFlow:

- explicit native project format,
- Domain independence from serialization,
- safe file evolution,
- migrations,
- stable-ID preservation,
- cross-platform portability,
- future Rust compatibility,
- future VS Code compatibility,
- testable persistence,
- atomic save architecture,
- clear distinction between Save, Import, and Export.

---

## 94. Consequences — Costs

This introduces:

- document mapping code,
- versioning discipline,
- migration code,
- duplicated representation between Domain and persisted document,
- more tests.

These are intentional costs for long-lived project safety.

---

## 95. Architecture Invariants

### Invariant 1

`.erdx` is ERDFlow's native editable project format.

### Invariant 2

Domain classes do not directly own their file serialization.

### Invariant 3

Persistence uses a Document Mapper boundary.

### Invariant 4

Every `.erdx` document has an explicit format version.

### Invariant 5

Old supported formats migrate toward the current format.

### Invariant 6

Stable semantic IDs survive save/load and migration.

### Invariant 7

Malformed or unsupported files produce structured errors.

### Invariant 8

Saving must protect the previous valid file against partial writes.

### Invariant 9

Work-in-progress models may be saved even if they are not conversion-ready.

### Invariant 10

Save, Import, and Export are distinct operations.

### Invariant 11

The file format is independent of C++ memory layout and Qt UI objects.

### Invariant 12

The exact `.erdx` container/encoding is intentionally deferred.

---

## 96. First Persistence Implementation Scope

When Phase 15 begins, implement only:

```text
Project metadata
ProjectId
Conceptual Model
EntityId
AttributeId
RelationshipId
relationship participants
cardinality
participation
comments needed by current product
layout state
format_version
safe save/load
```

Then add new document sections as product layers arrive.

---

## 97. First Persistence Tests

At minimum:

```text
1. New Conceptual project → save → load
2. IDs remain identical
3. Rename survives round trip
4. M:M survives round trip
5. Layout survives round trip
6. Duplicate IDs rejected
7. Missing references rejected
8. Unsupported future format rejected
9. Existing file survives simulated failed save
10. Incomplete but representable Conceptual model can still save
```

---

## 98. Follow-Up Decisions

This ADR intentionally leaves open:

- exact `.erdx` encoding,
- JSON library,
- archive/package strategy,
- compression,
- embedded asset strategy,
- autosave,
- recovery,
- lock files,
- backup history,
- detailed migration framework,
- exact ProjectStore signatures.

Those decisions should be made when implementation evidence requires them.

---

## 99. Relationship to Other ADRs

Depends on:

```text
ADR-001 — Stable Identity Strategy
ADR-002 — Layered Architecture and Dependency Direction
ADR-003 — Desktop UI Technology
ADR-004 — Core Language Strategy
```

Later decisions will build on it:

```text
Command / Undo Strategy
Background Work
Mapping / Provenance
Change Propagation
Import Architecture
Data Source Architecture
```

---

## 100. Decision Outcome

ERDFlow adopts:

```text
.erdx native project file
+
versioned document representation
+
ProjectDocumentMapper
+
ProjectStore port
+
Infrastructure serializer/storage adapter
+
safe atomic-style save
+
migration path
```

without locking the exact `.erdx` encoding before the persistence
implementation phase.

---

## 101. Final Principle

> Persist the meaning of an ERDFlow project, not the accidental shape of today's C++ classes.

That rule allows the project format, Domain model, UI, and future
implementation languages to evolve independently without sacrificing user
projects.
