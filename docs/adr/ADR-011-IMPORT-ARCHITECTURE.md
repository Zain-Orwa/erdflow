# ADR-011 — Import Architecture

**Status:** Proposed  
**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** Import boundaries, parsing, normalization, validation, preview/review, provenance, and safe application of external SQL/CSV/JSON data

---

## 1. Context

ERDFlow will support importing external information into a project.

The initial deterministic import formats are:

```text
SQL
CSV
JSON
```

Future possibilities include:

```text
XLSX
database metadata
direct database connections
textual requirements
AI-assisted interpretation
```

Import is not the same as opening an `.erdx` project.

Import means:

> interpret external material and convert it into a reviewable ERDFlow proposal.

External inputs may be incomplete, ambiguous, malformed, or structurally different from ERDFlow's internal model.

The import architecture therefore needs to protect:

- the live Project,
- stable identity,
- validation rules,
- undo/redo,
- mappings/provenance,
- future provider extensibility,
- future Rust implementation options.

---

## 2. Decision

ERDFlow will use:

> **Adapter-based importers that parse external formats into structured import results, then pass those results through validation, review, and an explicit Application command before mutating the Project.**

Conceptually:

```text
External Source
      ↓
Format Adapter / Parser
      ↓
Normalized Import Representation
      ↓
Interpretation / Mapping
      ↓
Validation
      ↓
Import Proposal / Preview
      ↓
User Review
      ↓
ApplyImportCommand
      ↓
Live Project
```

Importers never directly create Qt graphics items or mutate the live Project during parsing.

---

## 3. Core Rule

Import is a proposal pipeline.

Wrong:

```text
Read SQL file
      ↓
parser directly edits Project
      ↓
canvas objects appear immediately
```

Preferred:

```text
Read SQL file
      ↓
parse
      ↓
build structured proposal
      ↓
validate
      ↓
preview
      ↓
user accepts
      ↓
Application command
      ↓
Project mutation
```

---

## 4. Import Is Not Load

Opening:

```text
University.erdx
```

means restoring an ERDFlow project.

Importing:

```text
schema.sql
customers.csv
sample.json
```

means interpreting external content.

Therefore:

```text
Open Project
≠
Import
```

This distinction is architectural, not just UI terminology.

---

## 5. Import Is Not Export

Similarly:

```text
Import
→ external representation into ERDFlow
```

```text
Export
→ ERDFlow representation into another format
```

These are separate pipelines.

---

## 6. Initial Deterministic Formats

The initial supported deterministic import formats are:

```text
SQL
CSV
JSON
```

They do not all mean the same thing.

ERDFlow must not pretend that importing a CSV provides the same semantic information as importing DDL.

---

## 7. SQL Import Meaning

SQL DDL may contain explicit database structure such as:

```text
CREATE TABLE
columns
primary keys
foreign keys
constraints
types
indexes
```

Therefore SQL is primarily suitable for constructing or proposing:

```text
Relational Schema
and/or
Physical/Table Design
```

depending on the imported dialect and information available.

It does not automatically reconstruct an authoritative Conceptual ERD.

---

## 8. CSV Import Meaning

CSV normally represents rows of tabular data.

It may provide:

```text
column names
sample values
possible type evidence
possible uniqueness evidence
possible nullability evidence
```

But it usually does not explicitly provide:

```text
primary key semantics
foreign key relationships
entity meaning
relationship cardinality
conceptual participation
```

Therefore CSV import must distinguish:

```text
known facts
```

from:

```text
inferred suggestions
```

---

## 9. JSON Import Meaning

JSON may represent:

```text
records
nested objects
arrays
document-like structures
custom structured metadata
```

Its interpretation depends heavily on shape.

ERDFlow should parse JSON deterministically but must not claim that every nested object has one obvious relational/conceptual interpretation.

Ambiguous structural interpretation should remain reviewable.

---

## 10. No Silent Semantic Invention

The importer must not silently invent unsupported database semantics.

Example:

```text
CSV column:
customer_id
```

does not prove:

```text
primary key
```

unless a deterministic user-selected rule or explicit metadata establishes it.

Similarly:

```text
order_id
```

does not automatically prove a foreign key to another imported file.

---

## 11. Known vs Inferred Information

Import results should distinguish:

```text
Explicit
Inferred
UserConfirmed
Unknown
```

where useful.

Example:

```text
SQL PRIMARY KEY
→ Explicit

CSV column appears unique
→ Inferred

User confirms it is the key
→ UserConfirmed
```

Exact enum names are deferred.

---

## 12. Import Adapter Boundary

Each format has an adapter.

Conceptually:

```text
IImporter / Importer Port
        ↑
SQL Import Adapter
CSV Import Adapter
JSON Import Adapter
```

The exact C++ interface is deferred.

The architectural requirement is:

> format-specific parsing belongs outside the Domain.

---

## 13. Application Ownership

Application coordinates the workflow.

Conceptually:

```text
Application
├── choose importer
├── start parse task
├── receive structured result
├── request validation
├── present proposal
└── apply accepted import
```

The parser does not own the full user workflow.

---

## 14. Domain Independence

The Domain must not depend on:

```text
SQL parser library
CSV parser library
JSON parser library
QFile
QJsonDocument
database driver
```

Domain receives ERDFlow-owned structured information.

---

## 15. Normalized Import Representation

ERDFlow should introduce a format-independent intermediate representation where that genuinely helps.

Conceptually:

```text
ImportDocument
├── SourceMetadata
├── Structures
├── Fields
├── Constraints
├── Relationships
├── SampleData?
├── Evidence
├── Issues
└── Unknowns
```

This representation is not necessarily identical to the final Domain model.

---

## 16. Why an Intermediate Representation

Without an intermediate layer, every parser would need to know:

```text
how to construct Domain objects
how to assign layout
how to resolve conflicts
how to apply undo
how to interact with UI
```

That creates unnecessary coupling.

Instead:

```text
Parser
→ external facts

Interpreter
→ ERDFlow proposal

Application
→ controlled mutation
```

---

## 17. Do Not Over-Generalize the Intermediate Model

SQL, CSV, and JSON differ significantly.

The normalized representation should capture shared useful facts without forcing every source into one artificial schema.

Format-specific details may remain attached as structured metadata.

---

## 18. Import Pipeline

Recommended pipeline:

```text
1. Select source
2. Detect/select format
3. Read input
4. Parse syntax
5. Normalize external facts
6. Interpret into ERDFlow candidate
7. Validate candidate
8. Show issues/assumptions
9. User reviews mappings/options
10. Apply through command
```

---

## 19. Parsing vs Interpretation

These are separate concerns.

### Parsing

Answers:

```text
What does the external file literally contain?
```

### Interpretation

Answers:

```text
What ERDFlow structures can validly be proposed from that content?
```

Example:

```text
CSV parser:
column "id" exists

Interpreter:
"id" might be a candidate identifier
```

Do not merge those claims.

---

## 20. Syntax Errors

Malformed input should produce structured parse errors.

Examples:

```text
invalid SQL syntax
malformed CSV quoting
invalid JSON
```

These are parser errors.

They are not normal Domain validation warnings.

---

## 21. Structural Import Issues

Input may parse successfully but still contain structural problems.

Examples:

```text
duplicate SQL table declaration
foreign key target missing from imported set
inconsistent CSV row widths
JSON root shape unsupported
```

These should be reported before live mutation.

---

## 22. Candidate Validation

After interpretation, the candidate ERDFlow model must be validated using ADR-009 principles.

Possible result:

```text
valid
valid with warnings
blocked
requires decision
```

---

## 23. Preview First

Import should show a preview before application where the operation is non-trivial.

The preview may show:

```text
objects to create
objects to update
unresolved mappings
warnings
inferences
conflicts
sample rows
```

---

## 24. Apply Is Explicit

Only after acceptance:

```text
ApplyImportCommand
```

mutates the live Project.

This makes import compatible with ADR-006.

---

## 25. Undo

An accepted import should normally be one atomic undoable operation.

Example:

```text
Import SQL Schema
```

Undo restores the Project state from before the import.

Redo restores the same imported semantic IDs created during the original accepted operation.

---

## 26. Stable IDs

External identifiers do not replace ERDFlow stable IDs.

Example:

```text
SQL table name = Student
```

gets an ERDFlow:

```text
RelationId
or
TableId
```

depending on target level.

External names/IDs may be preserved separately as provenance.

---

## 27. External IDs

Some sources may contain stable external identifiers.

These may be stored as:

```text
external provenance/reference
```

but must not become ERDFlow's authoritative semantic identity by default.

---

## 28. Import Provenance

Imported objects should record useful origin information.

Examples:

```text
ImportedFromSQL
ImportedFromCSV
ImportedFromJSON
```

Potential metadata:

```text
source file
source object name
source dialect
source field
explicit vs inferred status
```

Do not store sensitive or machine-specific data unnecessarily.

---

## 29. Mapping and Provenance

ADR-008 applies.

Import provenance is not automatically the same as:

```text
Conceptual → Relational mapping
```

Example:

```text
SQL table imported into Physical Design
```

may have provenance from SQL but no Conceptual source.

---

## 30. Reverse Engineering

SQL import may eventually support:

```text
Physical → Relational reconstruction
```

or even:

```text
Relational → Conceptual proposal
```

But reverse engineering upward is not guaranteed to recover the original design intent.

Therefore such results must be proposals.

---

## 31. Loss of Information

A physical SQL schema may not contain enough information to reconstruct:

```text
Conceptual relationship names
participation semantics
derived attributes
conceptual specialization intent
original composite attributes
```

ERDFlow must not claim lossless Conceptual recovery where information no longer exists.

---

## 32. SQL Dialects

SQL varies by database.

Possible future dialects:

```text
PostgreSQL
MySQL
MariaDB
SQLite
SQL Server
Oracle
```

SQL import architecture should support dialect-specific adapters or parser configuration.

Do not hard-code one dialect into the Domain.

---

## 33. Dialect Detection

Automatic dialect detection may be attempted where reliable.

But if ambiguous:

```text
ask the user
```

or require explicit selection.

Do not silently choose a dialect if that changes semantics.

---

## 34. Unsupported SQL

A parser may encounter unsupported statements.

The import result should distinguish:

```text
supported and interpreted
ignored with explicit warning
unsupported and blocking
```

depending on whether safe partial import is possible.

---

## 35. Partial SQL Import

Partial import may be useful.

Example:

```text
CREATE TABLE supported
stored procedure unsupported
```

ERDFlow may still import tables if the user is clearly informed.

Unsupported content must never silently disappear.

---

## 36. SQL DML

Initial SQL import should focus on structural SQL where appropriate.

Statements such as:

```text
INSERT
UPDATE
DELETE
```

are data operations, not schema definition.

If future Data Workspace import supports them, that should be an explicit separate workflow.

---

## 37. CSV Schema Inference

CSV may propose:

```text
column names
candidate types
candidate nullable status
candidate uniqueness
```

Inference must be evidence-based.

Example:

```text
all observed values are integers
```

supports:

```text
candidate Integer
```

It does not prove future values will always be integers.

---

## 38. CSV Sample Limits

For large CSV files, ERDFlow may inspect a sample first.

If type inference uses only a sample:

```text
the result must record that limitation
```

Do not present sampled inference as absolute certainty.

---

## 39. CSV Headers

CSV import must handle:

```text
header present
header absent
```

through explicit user choice or reliable detection.

Generated names such as:

```text
Column1
Column2
```

are acceptable only when clearly presented as generated placeholders.

---

## 40. CSV Multiple Files

Future import may support several CSV files together.

This enables potential relationship suggestions based on:

```text
column names
value overlap
user confirmation
```

But those suggestions remain inference unless explicit metadata exists.

---

## 41. JSON Arrays

Example:

```json
[
  {"id": 1, "name": "A"},
  {"id": 2, "name": "B"}
]
```

may naturally suggest one tabular structure.

Still, key/relationship semantics are not automatically guaranteed.

---

## 42. Nested JSON

Example:

```json
{
  "customer": {
    "id": 1,
    "orders": [...]
  }
}
```

can map to several relational designs.

Therefore nested JSON interpretation may require:

```text
flatten
separate structures
embed as JSON field
```

depending on user intent and target level.

No silent universal rule.

---

## 43. Import Target Level

The user or importer must know the intended target.

Possible targets:

```text
Relational Schema
Physical Design
Data Workspace
Conceptual proposal
```

The same input may support more than one target.

---

## 44. Target Selection

Example:

```text
CSV
```

could mean:

```text
create sample data table
```

or:

```text
infer a schema candidate
```

These are different workflows.

The UI should make the target explicit.

---

## 45. Import Into Empty Project

Simplest workflow:

```text
Empty target model
      ↓
Import proposal
      ↓
create structures
```

This should be implemented before complex merge imports.

---

## 46. Import Into Existing Project

Later imports may need to merge with existing structures.

This introduces:

```text
match
create
update
skip
conflict
```

The architecture should support review.

Do not silently merge by name.

---

## 47. Matching Existing Objects

Stable ERDFlow IDs usually do not exist in external input.

Matching may use:

```text
explicit previous provenance
external stable IDs
user mapping
careful name/signature matching
```

Ambiguous matches require review.

---

## 48. Never Merge by Name Alone Without Review

Example:

```text
existing Relation "User"
imported Table "User"
```

does not prove they are the same semantic object.

Name can be evidence, not absolute identity.

---

## 49. Import Conflict

Example:

```text
existing Student.Name = Text
imported Student.Name = Integer
```

This requires:

```text
conflict classification
review
```

not silent replacement.

---

## 50. Import Proposal

Conceptually:

```text
ImportProposal
├── Source
├── TargetLevel
├── Creates
├── Updates
├── Matches
├── Conflicts
├── Inferences
├── Warnings
├── UnsupportedItems
└── Provenance
```

Exact representation is deferred.

---

## 51. Background Work

Large imports run under ADR-007.

Flow:

```text
source snapshot/file descriptor
      ↓
background parser
      ↓
structured result
      ↓
review
      ↓
apply
```

No worker directly mutates the live Project.

---

## 52. Cancellation

Large parsing/import preparation should support cooperative cancellation where practical.

Cancellation:

```text
does not partially mutate Project
```

because application happens only after the proposal is ready.

---

## 53. Progress

Import tasks may report meaningful stages:

```text
Reading
Parsing
Analyzing
Validating
Preparing Preview
```

Percentages should only be shown when measurable.

---

## 54. Source Revision

When importing into an existing Project, the proposal should be associated with the target Project revision.

If the Project changes before application:

```text
proposal may be stale
```

and must be revalidated/recomputed as necessary.

---

## 55. File Change During Import

If practical, the importer may track source metadata such as:

```text
size
modified time
hash
```

to detect changed inputs.

Exact policy is deferred.

---

## 56. Security

External files are untrusted input.

Parsers must be designed defensively.

Risks include:

```text
malformed files
huge files
deeply nested JSON
pathological CSV
SQL parser edge cases
memory exhaustion
```

---

## 57. Resource Limits

Importers should eventually enforce reasonable limits or streaming strategies for:

```text
file size
row count
nesting depth
field size
```

Exact thresholds must be evidence-driven.

---

## 58. Streaming

Large CSV/JSON import may benefit from streaming.

The architecture should not require:

```text
load entire file into one giant string
```

for all formats.

Initial implementation may stay simple for moderate files.

---

## 59. Parser Library Choice

This ADR does not select:

```text
SQL parser library
CSV parser library
JSON library
```

Each should be evaluated when implementing that importer.

Selection criteria include:

```text
license
correctness
maintenance
dialect support
performance
C++20 compatibility
cross-platform support
```

---

## 60. No Handwritten SQL Parser by Default

SQL parsing is complex.

ERDFlow should not build a full SQL grammar from scratch unless there is a compelling reason.

Prefer a mature parser or dialect-aware parsing approach.

---

## 61. CSV Parser

CSV appears simple but includes edge cases:

```text
quoted delimiters
embedded newlines
escaping
encodings
```

Use a robust implementation rather than naïve string splitting.

---

## 62. JSON Parser

JSON should use a mature parser.

Do not write a custom JSON parser for the product.

The Domain remains independent of whichever parser library is selected.

---

## 63. Encoding

Importers must handle text encoding deliberately.

UTF-8 should be the preferred internal textual interchange representation where practical.

Non-UTF-8 source handling may require explicit conversion/detection later.

---

## 64. File Paths

Source file paths belong to the import/session/infrastructure context.

The imported Domain model must not depend on absolute file paths for its meaning.

---

## 65. Source Metadata Persistence

Useful provenance may persist:

```text
original filename
format
dialect
import timestamp
```

But avoid persisting fragile machine-specific absolute paths unless the feature explicitly needs source relinking.

---

## 66. Reimport

Future workflow may support:

```text
reimport same source
```

If provenance can reliably identify continuity, ERDFlow may produce a reviewable update proposal.

This is future work.

---

## 67. Reimport Is Not Automatic Sync

Importing a file once does not create permanent synchronization with that file.

Reimport is explicit unless a future feature deliberately adds watching/sync.

---

## 68. Watch Mode

Automatic file watching is deferred.

If later added, changes still produce:

```text
proposal
```

rather than uncontrolled live mutation.

---

## 69. Data Workspace Import

CSV and JSON may be imported into a local/sample Data Source.

That workflow should use:

```text
DataSource
```

architecture from ADR-012.

Do not force Data Workspace rows into core schema objects.

---

## 70. Schema Inference from Data

If user asks ERDFlow to infer schema from data:

```text
data evidence
      ↓
inference proposal
      ↓
user review
      ↓
schema creation
```

Inference confidence must remain visible.

---

## 71. AI-Assisted Import

Future AI may help interpret:

```text
ambiguous headers
nested JSON
requirements
schema suggestions
```

But AI is an optional interpretation adapter.

It does not replace deterministic parser facts.

---

## 72. Deterministic Facts vs AI Suggestions

Example:

```text
CSV has 7 columns
```

is deterministic.

```text
customer_id probably identifies Customer
```

may be heuristic/AI suggestion.

ERDFlow must distinguish them.

---

## 73. AI Cannot Bypass Review

Future AI-generated import proposals still follow:

```text
validate
review
apply command
```

---

## 74. Import and Mappings

If an import creates a downstream object from a known source object within ERDFlow, mappings may be established.

If the source is merely an external file, use provenance rather than pretending it is a Conceptual mapping.

---

## 75. Import and Baselines

Importing a Schema does not automatically create a Conceptual-generation baseline.

Generation baseline means:

```text
ERDFlow generated this target from a known upstream model
```

Imported external content is different.

---

## 76. Imported Schema as Starting Point

A user may begin from imported SQL.

Then:

```text
Relational/Physical model
```

becomes the working starting point.

Future Conceptual reconstruction is optional and reviewable.

---

## 77. Import and Undo Memory

A large import may produce a large undo payload.

SCALE.md applies.

Possible future optimization:

```text
compact imported change set
```

rather than full-project snapshots.

---

## 78. Import Atomicity

Accepted import should be atomic.

Either:

```text
all selected changes apply successfully
```

or:

```text
live Project remains unchanged
```

where practical.

---

## 79. Incremental Apply

For extremely large datasets, Data Workspace import may need chunked/transactional application.

That is a Data Source concern.

Structural model import should remain logically atomic from the user's perspective.

---

## 80. Import Error Categories

Possible structured categories:

```text
SourceReadError
UnsupportedFormat
ParseError
UnsupportedConstruct
StructuralError
InferenceWarning
Conflict
ValidationBlocked
Cancelled
StaleProposal
```

Exact enum names are deferred.

---

## 81. User-Facing Explanations

Presentation may show:

```text
3 tables detected
1 unsupported statement
2 inferred types
1 relationship requires confirmation
```

Core provides structured facts.

UI provides wording and visuals.

---

## 82. Import Preview Navigation

Future UI may allow clicking an imported proposal item to inspect:

```text
source location
target object
warning
inference evidence
```

This requires source-location metadata where parser libraries can provide it.

---

## 83. Source Locations

Parsers should preserve useful location information when practical.

Examples:

```text
line/column in SQL
row/column in CSV
JSON path
```

This greatly improves diagnostics.

---

## 84. SQL Source Location

Example:

```text
schema.sql:42:7
Unsupported ALTER TABLE form
```

is more useful than:

```text
Import failed
```

---

## 85. CSV Source Location

Example:

```text
row 102
expected 6 fields, found 8
```

---

## 86. JSON Source Location

Useful diagnostics may reference:

```text
JSON path:
$.customers[4].orders
```

and parser line/column when available.

---

## 87. Import Testing

Every importer requires fixture-based tests.

Examples:

```text
valid minimal input
malformed input
unsupported constructs
edge cases
large sample
ambiguous structure
```

---

## 88. SQL Tests

Test at least:

```text
CREATE TABLE
primary key
foreign key
M:M junction-style schema
composite key
nullable columns
dialect-specific syntax
unsupported statement reporting
```

---

## 89. CSV Tests

Test:

```text
quoted commas
embedded newlines
empty cells
header/no-header
different row widths
UTF-8 text
large file/sample inference
```

---

## 90. JSON Tests

Test:

```text
array of objects
nested objects
arrays inside objects
null values
mixed field types
deep nesting limit
malformed JSON
```

---

## 91. Round-Trip Is Not Required

Import is not necessarily lossless.

Example:

```text
SQL → ERDFlow → SQL
```

may produce semantically equivalent but textually different SQL.

Do not define import correctness as byte-for-byte round-trip.

---

## 92. Semantic Equivalence

Where appropriate, tests should verify:

```text
imported constraints
keys
types
relationships
```

rather than exact original formatting.

---

## 93. Plugin Importers

Future plugin architecture may allow new import formats.

Potential:

```text
XLSX
DBML
PlantUML-like schema
vendor metadata
```

The Application import contract should make this possible.

Plugin architecture itself is deferred.

---

## 94. Future Rust

Import parsers are plausible future Rust candidates because they process untrusted external input.

But ADR-004 applies:

```text
C++ first unless concrete evidence justifies Rust
```

A clean importer boundary makes later replacement possible.

---

## 95. Future VS Code Extension

The VS Code extension may call the same import/application logic or a shared core service.

It should not create separate SQL/CSV/JSON semantic rules.

---

## 96. Alternative A — Parser Directly Mutates Domain

Rejected.

Reason:

- no preview,
- poor undo,
- hard cancellation,
- format logic leaks inward,
- partial-failure risk.

---

## 97. Alternative B — Parser Directly Creates Qt Items

Rejected.

The canvas is not the database model.

---

## 98. Alternative C — One Universal Generic Parser

Rejected.

SQL, CSV, and JSON have fundamentally different syntax and semantics.

They can share an import contract without sharing one parser.

---

## 99. Alternative D — Infer Everything Automatically

Rejected.

External data often lacks enough semantic information.

ERDFlow must expose uncertainty.

---

## 100. Alternative E — Never Infer Anything

Also rejected.

Useful suggestions such as candidate data types can save work if clearly labeled as inferred and reviewable.

---

## 101. Alternative F — Import Directly Into Conceptual ERD Always

Rejected.

SQL/CSV/JSON often describe lower-level or data-level structures more naturally.

Target level must be explicit.

---

## 102. Alternative G — Treat Import as Project Load

Rejected.

An external file is not an `.erdx` project and does not contain ERDFlow's complete semantic state.

---

## 103. Consequences — Positive

This decision gives ERDFlow:

- safe external input handling,
- format independence,
- deterministic parser facts,
- explicit inference,
- preview/review,
- undoable imports,
- provenance,
- future plugin support,
- future Rust compatibility,
- future AI-assisted interpretation without core coupling.

---

## 104. Consequences — Costs

This requires:

- parser adapters,
- normalized import structures,
- proposal models,
- import validation,
- preview UI,
- provenance metadata,
- conflict handling,
- more tests.

These costs are necessary for trustworthy import behavior.

---

## 105. Initial Implementation Order

Recommended order:

```text
1. SQL structural import
2. CSV data/schema-inference import
3. JSON data/schema-inference import
```

But each importer should be added only when the target model/workspace it feeds already exists.

Do not implement all parsers before their destination workflows exist.

---

## 106. First SQL Import Scope

Keep the first SQL importer intentionally narrow:

```text
CREATE TABLE
columns
primary keys
basic foreign keys
basic nullability
basic logical/physical types
```

Then expand dialect support based on tests.

---

## 107. First CSV Import Scope

Initial CSV workflow:

```text
open file
preview rows
detect/use header
infer candidate column types
show uncertainty
allow user correction
apply to local/sample data or schema candidate
```

---

## 108. First JSON Import Scope

Initial JSON workflow should start with:

```text
array of similar objects
```

before tackling arbitrary deeply nested documents.

This keeps interpretation deterministic and testable.

---

## 109. Architecture Invariants

### Invariant 1

Import is distinct from `.erdx` project loading.

### Invariant 2

Format parsers are adapters outside the Domain.

### Invariant 3

Parsing does not mutate the live Project.

### Invariant 4

External facts and inferred semantics are distinguished.

### Invariant 5

Non-trivial imports produce a reviewable proposal before application.

### Invariant 6

Accepted structural imports apply through Application commands.

### Invariant 7

Imported ERDFlow objects receive ERDFlow stable IDs.

### Invariant 8

External identifiers may be preserved as provenance, not authoritative identity by default.

### Invariant 9

Ambiguous semantics are not silently invented.

### Invariant 10

Unsupported content is reported explicitly.

### Invariant 11

Large imports use background execution where needed.

### Invariant 12

Import target level is explicit.

### Invariant 13

SQL, CSV, and JSON share an architecture but retain format-specific semantics.

### Invariant 14

Import provenance is not confused with Conceptual→Relational mapping.

---

## 110. Relationship to Other ADRs

Depends on:

```text
ADR-001 — Stable Identity Strategy
ADR-002 — Layered Architecture and Dependency Direction
ADR-004 — Core Language Strategy
ADR-006 — Command / Undo Strategy
ADR-007 — Background Work / Concurrency Strategy
ADR-008 — Cross-Level Mapping and Provenance
ADR-009 — Validation Gates and Conversion Policy
ADR-010 — Change Propagation and Regeneration
```

Works with future:

```text
ADR-012 — Data Source Architecture
ADR-013 — Future Rust Boundary
```

---

## 111. Open Implementation Decisions

This ADR intentionally leaves open:

- exact importer C++ interface,
- exact normalized import representation,
- exact SQL parser library,
- exact CSV parser library,
- exact JSON parser library,
- exact SQL dialect support matrix,
- exact inference algorithms,
- exact confidence representation,
- exact partial-import policy per format,
- exact merge/reimport workflow,
- exact streaming thresholds,
- exact plugin importer API,
- exact source-location abstraction.

These should be decided when each importer is implemented.

---

## 112. Decision Outcome

ERDFlow adopts:

```text
External Source
      ↓
Format Adapter
      ↓
Structured Parsed Facts
      ↓
Interpretation / Inference
      ↓
Validation
      ↓
Reviewable Import Proposal
      ↓
ApplyImportCommand
      ↓
Project
```

with initial deterministic support focused on:

```text
SQL
CSV
JSON
```

and no silent semantic invention.

---

## 113. Final Principle

> Parse what the source actually says, label what ERDFlow only infers, and never modify the user's project until the proposed import is understood and accepted.
