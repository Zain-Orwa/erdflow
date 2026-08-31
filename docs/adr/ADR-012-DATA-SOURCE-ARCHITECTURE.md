# ADR-012 — Data Source Architecture

**Status:** Proposed  
**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** Data Workspace abstraction, local/imported/live data sources, paging, sorting/filtering, editing, capabilities, and database write-back boundaries

---

## 1. Context

ERDFlow's product flow includes:

```text
Conceptual ERD
      ↓
Relational Schema
      ↓
Physical/Table Design
      ↓
SQL
      ↓
Data
```

The final Data stage is not merely a static preview.

ERDFlow should eventually provide a spreadsheet-like workspace where users can:

```text
view rows
add rows
edit cells
delete rows
sort
filter
search
refresh
import
export
validate
```

But the data shown there may come from very different places:

```text
project-local sample data
imported CSV/JSON data
future live database queries
future database connections
```

Those sources differ in:

```text
size
latency
query capability
editing capability
transaction behavior
row identity
refresh behavior
security
```

Therefore the Data Workspace must not assume:

```text
all rows live in one in-memory vector
```

or:

```text
every source is a live database
```

---

## 2. Decision

ERDFlow will use:

> **A capability-aware DataSource abstraction behind the Data Workspace.**

Conceptually:

```text
QTableView
      ↓
Qt Table Model Adapter
      ↓
Application Data Workspace
      ↓
DataSource Port
      ↑
      ├── LocalProjectDataSource
      ├── ImportedDataSource
      └── FutureDatabaseDataSource
```

The Qt table is a view.

It is not the authoritative data store.

---

## 3. Core Rule

The Data Workspace interacts with data through a DataSource contract.

Wrong:

```text
QTableWidget
      =
all application data
```

Preferred:

```text
QTableView
      ↓
QAbstractItemModel adapter
      ↓
DataSource
```

This preserves separation between:

```text
presentation
data access
data ownership
```

---

## 4. Initial Data Source Types

ERDFlow recognizes three main source categories.

### Local Project Data

Rows stored as project-owned sample/test data.

### Imported Data

Rows originating from files such as:

```text
CSV
JSON
```

and represented locally for review or experimentation.

### Future Live Database Data

Rows queried from an external database through a database adapter.

These categories share a Data Workspace but do not need identical capabilities.

---

## 5. Data Source Is Not the Schema Model

A Relational Schema defines structure such as:

```text
Relation
Attribute
Primary Key
Foreign Key
Logical Type
```

A DataSource provides row data.

These are related but separate.

Example:

```text
Relation Student
├── StudentId
├── Name
└── CreatedAt
```

is schema.

Rows such as:

```text
1 | Ana | ...
2 | John | ...
```

belong to a DataSource.

---

## 6. Physical Model Is Also Separate

Physical/Table Design may define:

```text
table
column
physical type
index
constraint
```

That still does not mean all live database rows are stored in the Project.

Model metadata and row data are separate concerns.

---

## 7. DataSource Port

The Application layer should depend on a DataSource abstraction.

Conceptually, the port may support operations such as:

```text
describe capabilities
read page
count if available
sort
filter
search
insert
update
delete
refresh
```

The exact C++ interface is deferred.

---

## 8. Capability-Aware Design

Not every DataSource supports every operation.

Example:

```text
LocalProjectDataSource
→ read/write/sort/filter

ReadOnlyImportedDataSource
→ read/sort/filter

LiveDatabaseDataSource
→ capabilities depend on connection/query/permissions
```

Therefore the DataSource should expose capabilities explicitly.

---

## 9. Example Capability Set

Conceptually:

```text
CanRead
CanInsert
CanUpdate
CanDelete
CanSort
CanFilter
CanSearch
CanRefresh
CanCount
CanExport
CanImport
SupportsTransactions
SupportsServerSidePaging
```

Exact enum names are deferred.

The UI enables actions based on capabilities.

---

## 10. No Fake Capabilities

If a source is read-only:

```text
Edit
Delete
Insert
```

should not appear enabled merely because the Data Grid knows how to draw editable cells.

Presentation follows source capabilities.

---

## 11. Local Project Data Source

A local source supports project-contained sample data.

Use cases:

```text
teaching
testing a schema
demonstrating example rows
offline work
small prototypes
```

This is not a replacement for a production database.

---

## 12. Imported Data Source

Imported data may originate from:

```text
CSV
JSON
```

The source may become:

```text
temporary preview data
```

or:

```text
project-local data
```

after user acceptance.

Exact persistence policy is deferred.

---

## 13. Future Database Data Source

A future DatabaseDataSource may connect to:

```text
PostgreSQL
MySQL
MariaDB
SQLite
SQL Server
Oracle
```

through adapters.

The Domain does not depend on database drivers.

---

## 14. Direct Live Write-Back

Direct live database write-back is **not committed for the first Data Workspace implementation**.

Initial live database support, when introduced, should prioritize:

```text
connect
inspect
query/read
refresh
```

before enabling destructive writes.

---

## 15. Why Write-Back Is Deferred

Live editing introduces:

```text
transactions
permissions
locking
concurrent users
constraints
triggers
generated values
network failure
partial failure
audit concerns
```

This deserves a separate implementation phase and safety review.

---

## 16. Read-Only First for Live Databases

A safe first live database milestone is:

```text
read-only browsing
```

with:

```text
paging
sorting
filtering
search
refresh
```

where supported.

Write-back can later be explicitly promoted.

---

## 17. Data Workspace Flow

Conceptually:

```text
User opens Student data
      ↓
Application resolves DataSource
      ↓
Qt model requests visible rows/page
      ↓
DataSource returns structured page
      ↓
Qt model presents rows
```

---

## 18. Qt Model/View

The desktop Presentation will use:

```text
QTableView
+
QAbstractItemModel-derived adapter
```

rather than making `QTableWidget` the primary long-term data architecture.

This follows ADR-003.

---

## 19. Why Model/View

Model/View allows:

```text
large datasets
lazy loading
source-backed views
sorting/filtering
read-only sources
editable sources
```

without requiring all application data to live inside table cells.

---

## 20. Qt Adapter Responsibility

The Qt model adapter translates:

```text
DataSource result
```

into:

```text
rows / columns / roles
```

under Qt's model/view API.

It should not contain database-specific query logic.

---

## 21. Data Page

Data should be retrieved in bounded chunks.

Conceptually:

```text
DataPage
├── Rows
├── Offset / Cursor
├── HasMore
└── OptionalTotalCount
```

Exact representation is deferred.

---

## 22. Why Paging

A live table may contain:

```text
100
10,000
1,000,000
100,000,000
```

rows.

ERDFlow must not require loading the entire source to display the first screen.

---

## 23. Local Data May Still Use Paging Interface

Even if local project data is small, using the same conceptual paging interface can simplify the view layer.

The implementation may satisfy a page request directly from memory.

---

## 24. Offset vs Cursor Paging

Different sources support different strategies.

Possible:

```text
offset/limit
cursor/keyset
in-memory slice
```

The Data Workspace abstraction should not assume all sources use SQL `OFFSET`.

Exact paging method belongs to the adapter.

---

## 25. Total Row Count

Some sources can cheaply provide:

```text
total = 1234
```

Others cannot.

Therefore total row count should be optional/capability-driven.

Do not force an expensive:

```text
COUNT(*)
```

for every view refresh.

---

## 26. Sorting

Sorting changes the **view/query order**.

It does not physically reorder database rows.

Example:

```text
Sort by CreatedAt descending
```

means:

```text
request/display newest rows first
```

It does not mean the table's physical storage order has changed.

---

## 27. CreatedAt Requirement

Sorting by:

```text
CreatedAt
```

requires an actual timestamp/date-time field or equivalent source expression.

ERDFlow must not invent reliable creation order from:

```text
row position
```

unless the source explicitly defines such an order.

---

## 28. No Default Database Row Order Assumption

Without:

```text
ORDER BY
```

or an equivalent explicit ordering contract, relational query results do not have a guaranteed meaningful order.

Therefore ERDFlow must not present:

```text
"original database order"
```

as a semantic guarantee.

---

## 29. Local Data Ordering

Local project data may preserve an explicit local display order if ERDFlow chooses to store one.

That local display order is different from database physical row ordering.

---

## 30. Server-Side Sorting

For large/live sources, sorting should normally be delegated to the DataSource.

Conceptually:

```text
SortSpec
      ↓
DatabaseDataSource
      ↓
query ORDER BY
```

This avoids fetching everything first.

---

## 31. Client-Side Sorting

For small local/imported datasets:

```text
client-side sort
```

may be appropriate.

The Application/adapter decides based on source capability.

---

## 32. Filtering

Filtering should follow the same principle.

Small local source:

```text
filter locally
```

Large live source:

```text
delegate filter to source query
```

when supported.

---

## 33. Filter Specification

The UI should produce a structured filter intent.

Conceptually:

```text
FilterExpression
```

rather than concatenating raw SQL directly from UI text.

This improves:

```text
safety
portability
testing
```

---

## 34. SQL Injection Boundary

Future database filters/searches must use safe parameterization or equivalent driver-supported mechanisms.

Do not construct queries by blindly concatenating user-entered values.

---

## 35. Search

Search may mean different things:

```text
search current page
search loaded rows
search entire source
```

The UI should make the scope understandable.

For live sources, entire-source search may require server-side support.

---

## 36. Refresh

Refresh means:

```text
ask DataSource for current data again
```

It does not mutate the schema.

For a live database, refresh may reveal changes made externally.

---

## 37. External Changes

Future live sources may change outside ERDFlow.

Therefore the Data Workspace must tolerate:

```text
row added externally
row deleted externally
row updated externally
```

after refresh.

---

## 38. Data Revision

DataSource results may carry operational revision/version metadata where available.

This can help with:

```text
optimistic editing
staleness
refresh
```

but is not required for every source.

---

## 39. Row Identity

Editing/deleting rows requires a way to identify a row reliably.

Possible identities:

```text
primary key
composite key
source-specific row handle
local row ID
```

A visible row index is not a stable row identity.

---

## 40. Row Index Is Not Identity

Wrong:

```text
delete row 7
```

after sorting/filtering.

Preferred:

```text
delete row with key {StudentId = 42}
```

or another source-defined identity.

---

## 41. Tables Without Keys

A live database table may lack a usable primary key.

In that case:

```text
safe row update/delete
```

may be unavailable or require explicit limited behavior.

The UI must not pretend reliable editing exists.

---

## 42. Composite Keys

Row identity must support:

```text
composite key
```

Example:

```text
Enrollment(StudentId, CourseId)
```

The DataSource abstraction must not assume one integer ID column.

---

## 43. Generated Values

Insert/update operations may receive values generated by the source:

```text
auto-increment ID
timestamp
computed column
trigger-generated field
```

A future write-capable DataSource must return the resulting authoritative row state.

---

## 44. Local Row Identity

Project-local/sample rows may use internal stable row IDs if useful.

These operational row IDs are separate from:

```text
EntityId
RelationId
TableId
```

Model identity and row identity are different domains.

---

## 45. Editing Workflow

For an editable source:

```text
User edits cell
      ↓
Validate local value
      ↓
Application requests DataSource update
      ↓
Success?
  yes → commit displayed state
  no  → show error / restore pending state
```

Exact optimistic/pessimistic UI behavior is deferred.

---

## 46. Local Data Editing

For LocalProjectDataSource, editing may become a normal project mutation.

It may participate in:

```text
project dirty state
save
undo
```

depending on how local data persistence is defined.

---

## 47. Live Database Editing

Future live database edits are external side effects.

They are fundamentally different from normal project undo.

A database update may commit outside ERDFlow and may be observed by other users/systems.

---

## 48. Undo and Live Databases

Do not assume:

```text
Ctrl+Z
```

can safely undo an already committed database transaction.

A future write-back design must define:

```text
transaction timing
reversal semantics
conflict handling
```

separately.

---

## 49. No Fake Undo for External Side Effects

ERDFlow should not label an operation:

```text
Undo database delete
```

unless it can actually restore the correct external state safely.

This is one reason live write-back is deferred.

---

## 50. Validation

Data value validation may use:

```text
logical types
physical constraints
nullability
length
range
foreign-key evidence
```

depending on the source/model relationship.

Validation should distinguish:

```text
ERDFlow-side validation
```

from:

```text
database authoritative constraint result
```

---

## 51. Database Is Authoritative for Live Writes

If live writes are enabled later, the external database ultimately decides whether a transaction is accepted.

ERDFlow-side pre-validation improves UX but does not replace server constraints.

---

## 52. Data Errors

Possible structured errors include:

```text
ConnectionFailed
QueryFailed
PermissionDenied
ConstraintViolation
RowNotFound
Conflict
Timeout
Cancelled
UnsupportedOperation
```

Exact types are deferred.

Presentation turns them into user-facing messages.

---

## 53. No Database Dialogs in Adapters

A database adapter must not show:

```text
QMessageBox
```

on failure.

It returns structured error data.

---

## 54. Background I/O

Live database queries and large local/imported operations may run in the background.

ADR-007 applies:

```text
background task
      ↓
structured result
      ↓
UI/application update
```

---

## 55. Cancellation

Long-running:

```text
query
refresh
export
import
```

should support cancellation where the underlying source supports it.

Cancellation semantics depend on the adapter.

---

## 56. Progress

For query paging, progress may be indeterminate.

For large imports/exports, measurable progress may be available.

Do not fabricate percentages.

---

## 57. Connection State

Future live DatabaseDataSource should expose connection status separately from project semantics.

Examples:

```text
Disconnected
Connecting
Connected
Failed
```

Exact state model is deferred.

---

## 58. Connection Metadata

A project may eventually store non-secret connection metadata such as:

```text
database type
host alias
database name
schema name
```

depending on product policy.

Secrets are not stored casually in `.erdx`.

---

## 59. Credentials

Passwords, tokens, and other credentials should use platform-appropriate secure storage or external connection configuration.

They should not be stored as ordinary plaintext project data.

This follows ADR-005.

---

## 60. Local vs Live Source Indicator

The Data Workspace should clearly identify whether the user is viewing:

```text
Local Sample Data
Imported Data
Live Database Data
```

This is important because editing consequences differ.

---

## 61. Source Badge / Status

Presentation may show a badge such as:

```text
LOCAL
IMPORTED
LIVE
READ ONLY
```

The exact UI is deferred.

The underlying source category/capabilities are structured.

---

## 62. DataSource Descriptor

Conceptually:

```text
DataSourceDescriptor
├── SourceId
├── SourceKind
├── DisplayName
├── Capabilities
└── ConnectionState?
```

Exact representation is deferred.

---

## 63. Data Source Identity

A configured DataSource may have an ERDFlow-owned operational identity.

This helps:

```text
workspace tabs
refresh routing
saved metadata
```

It is separate from schema element IDs.

---

## 64. Project Persistence

Project-local DataSource metadata may persist in `.erdx`.

Examples:

```text
source type
table association
display settings
local sample data metadata
```

Exact row-storage strategy is deferred.

---

## 65. Live Data Is Not Automatically Persisted

Opening 10,000 live database rows does not mean ERDFlow should serialize those rows into `.erdx`.

Cache behavior is implementation-specific and should not silently turn live data into project data.

---

## 66. Imported Data Persistence

Imported rows may either be:

```text
temporary
```

or:

```text
promoted into project-local sample data
```

The user workflow should make that distinction clear.

---

## 67. Association with Model Objects

A Data Workspace view may be associated with:

```text
RelationId
TableId
```

depending on which design stage owns the row structure.

The exact association is defined by the implemented workspace.

---

## 68. Do Not Reuse Model IDs as DataSource IDs

A TableId identifies a Physical table model object.

A DataSourceId identifies a data-access context.

They may be linked but are not the same thing.

---

## 69. Column Metadata

The Data Grid needs structured column information.

Conceptually:

```text
ColumnDescriptor
├── Name
├── Type
├── Nullable
├── Editable
├── KeyRole?
└── SourceMetadata?
```

Exact representation is deferred.

---

## 70. Display Formatting

The Qt adapter may format values for display.

Examples:

```text
date/time
boolean
binary
null
decimal
```

Display formatting must not alter authoritative stored values unintentionally.

---

## 71. NULL vs Empty String

The Data Workspace must distinguish:

```text
NULL
```

from:

```text
""
```

for sources where that distinction exists.

Presentation should not collapse them into one visual state.

---

## 72. Type Conversion

Editing/import may require conversion from text input to typed value.

Example:

```text
"42"
→ Integer 42
```

Invalid conversion should be reported before or during source update.

---

## 73. Typed Cell Value

The DataSource boundary should avoid treating every cell permanently as a string.

Conceptually support values such as:

```text
Null
Text
Integer
Decimal
Boolean
Date
DateTime
Binary
UUID
```

matching ERDFlow's logical type direction.

Exact variant type is deferred.

---

## 74. Physical Types

A live database may expose vendor-specific types beyond ERDFlow's common logical types.

Adapters may preserve:

```text
source type metadata
```

while mapping to an ERDFlow display/value type where possible.

Unsupported types must be explicit.

---

## 75. Binary Data

Large binary values should not necessarily be loaded/displayed inline as ordinary strings.

Future UI may show:

```text
BLOB
image preview
size
open/export action
```

This is deferred.

---

## 76. Large Text

Very large text fields may also need lazy/detail viewing.

The initial grid should avoid assumptions that all values are small.

---

## 77. Sorting Nulls

Database dialects may differ in:

```text
NULL ordering
```

The source adapter should define/request explicit semantics where necessary.

UI should not assume all backends behave identically.

---

## 78. Case Sensitivity

Search/filter behavior may differ by:

```text
collation
database
locale
source
```

The Data Workspace should not promise universal case semantics unless specified.

---

## 79. Pagination Stability

For live data, page navigation should use a stable ordering where possible.

Paging without deterministic ordering can produce inconsistent pages when data changes.

Exact strategy is adapter-specific.

---

## 80. Query Specification

Future read requests may conceptually use:

```text
DataQuery
├── Projection
├── Filter
├── Sort
├── Page
└── Search?
```

The adapter converts this into the source-specific mechanism.

Do not expose raw SQL as the universal DataSource API.

---

## 81. Raw SQL Mode

A future advanced SQL query workspace may allow raw SQL.

That is a separate power-user feature.

The standard Data Grid should use structured requests.

---

## 82. Export

The Data Workspace may export visible/selected/source data to:

```text
CSV
JSON
```

and later other formats.

The export workflow must clearly define scope:

```text
current page
filtered result
selected rows
entire source
```

---

## 83. Large Export

Exporting an entire large live result may stream data rather than collecting everything in memory.

This is future implementation detail but the architecture must allow it.

---

## 84. Import

Data import from CSV/JSON follows ADR-011.

The DataSource determines how accepted rows are stored.

---

## 85. Schema Changes While Data Is Open

If the user changes table structure while a Data Workspace tab is open:

```text
column removed
type changed
relation deleted
```

the workspace may become stale.

The Application should detect invalid association and refresh/reopen as needed.

Exact UX is deferred.

---

## 86. Local Data and Schema Evolution

Project-local sample data may require migration when structure changes.

Example:

```text
rename column
```

can preserve values via stable schema mappings.

Example:

```text
delete column
```

may be destructive.

This requires explicit policy when local data persistence is implemented.

---

## 87. Live Data and Schema Evolution

If ERDFlow's local Physical model changes, the external live database does not automatically change.

Schema deployment/migration is a separate feature.

The DataSource reads the actual database state.

---

## 88. No Hidden Deployment

Editing Physical Design must not silently execute DDL against a connected database.

Database deployment requires an explicit future workflow.

---

## 89. Refresh Metadata

Future live source refresh may detect that database schema changed externally.

This can produce:

```text
metadata difference
```

for review.

It should not silently rewrite ERDFlow models.

---

## 90. Data Workspace Is Not an ORM

ERDFlow does not need a full object-relational mapping framework for the Data Grid.

The DataSource abstraction should remain focused on:

```text
tabular access
capabilities
query/edit operations
```

---

## 91. No One-Row-Per-Domain-Object Assumption

Data rows are not Domain model objects such as:

```text
Entity
Relationship
Relation
```

Do not merge model object identity with table row identity.

---

## 92. Caching

Adapters may cache:

```text
pages
metadata
prepared queries
```

for performance.

Cache must not become the authoritative source when connected to live data.

Refresh/invalidation rules must be correct.

---

## 93. Cache Size

Cache size must be bounded.

Avoid retaining every page a user ever viewed in a large table.

Exact memory budget is deferred to measurements.

---

## 94. Prefetching

Future Data Grid may prefetch nearby pages for smooth scrolling.

Prefetching is an optimization, not a semantic requirement.

Do not implement before baseline paging works.

---

## 95. Virtualized Viewing

The Data Grid should eventually behave as a virtualized view over the source.

Only necessary rows need to be materialized for display.

Qt model/view supports this architecture.

---

## 96. Multiple Open Data Views

A user may later open:

```text
Student
Course
Enrollment
```

data views simultaneously.

Each view may have:

```text
independent query state
sort
filter
selection
```

while sharing an underlying DataSource.

---

## 97. Query State

Sort/filter/page/search state belongs to the Data Workspace/session.

It is not automatically Domain schema state.

Some view preferences may later persist as workspace metadata.

---

## 98. Transactions

Local project data may use Application-level atomic changes.

Live database transactions, if write-back is later enabled, are source-specific.

The DataSource should expose transaction capability rather than assuming every source supports the same semantics.

---

## 99. Multi-Row Edits

Future bulk editing may require:

```text
transaction
partial failure handling
validation
review
```

This is deferred until write-back.

---

## 100. Concurrency with External Database

Another application/user may change the same row.

Future editing may need:

```text
optimistic concurrency
version columns
compare-and-swap
database transaction
```

depending on backend.

This is a major reason not to treat live rows like ordinary local project fields.

---

## 101. Conflict Example

ERDFlow reads:

```text
StudentId 42
Name = Ana
```

another client changes:

```text
Name = Anna
```

user then edits:

```text
Name = Anamaria
```

A write-capable adapter needs a defined conflict policy.

Deferred.

---

## 102. Database Drivers

Database-specific drivers belong in Infrastructure adapters.

Examples:

```text
PostgreSQLDataSource
SQLiteDataSource
SqlServerDataSource
```

The Domain/Application contract should not depend directly on driver classes.

---

## 103. Driver Selection

No database driver library is selected by this ADR.

Each future backend should be evaluated for:

```text
license
maintenance
security
platform support
async behavior
prepared statements
transactions
type support
```

---

## 104. Future Rust

Database adapters may later be implemented in C++ or Rust.

The DataSource boundary should exchange structured data, not Qt pointers.

ADR-004 and ADR-013 apply.

---

## 105. Future VS Code Extension

A VS Code frontend may display data using a webview/table component while using the same conceptual DataSource contract.

The Qt Data Grid is one frontend.

It is not the architecture itself.

---

## 106. Alternative A — Store All Rows in QTableWidget

Rejected.

Reason:

- UI becomes source of truth,
- poor scaling,
- difficult live database support,
- difficult paging,
- weak separation.

---

## 107. Alternative B — Load Entire Source Into Memory

Rejected as a universal strategy.

It may be acceptable for small local sources but cannot be the DataSource contract.

---

## 108. Alternative C — Use SQL as the Universal API

Rejected.

Local/imported sources may not be SQL databases.

Structured query intent provides a cleaner portable contract.

---

## 109. Alternative D — One DataSource with Every Method Required

Rejected.

Not every source can:

```text
insert
delete
count
refresh
transact
```

Capabilities must be explicit.

---

## 110. Alternative E — Enable Live Write-Back Immediately

Rejected for the initial Data Workspace.

Read/query capability should be proven first.

---

## 111. Alternative F — Treat CSV as a Database

Rejected.

CSV is a file/data format.

ERDFlow may wrap imported CSV rows in a DataSource, but that does not make CSV a relational database.

---

## 112. Alternative G — Persist Every Live Row in `.erdx`

Rejected.

Live database data remains external unless the user explicitly imports/copies it into project-local data.

---

## 113. Consequences — Positive

This decision gives ERDFlow:

- scalable Data Grid architecture,
- local and remote source support,
- explicit capabilities,
- paging,
- source-side sorting/filtering,
- safer future database integration,
- clear row identity,
- clean Qt separation,
- future VS Code/Rust compatibility.

---

## 114. Consequences — Costs

This introduces:

- DataSource interfaces,
- data page/value types,
- Qt model adapters,
- capability handling,
- paging state,
- more asynchronous behavior,
- future backend-specific adapters.

These costs are justified because the Data Workspace must support more than tiny in-memory tables.

---

## 115. Initial Data Workspace Scope

The first implemented Data Workspace should remain deliberately limited.

Recommended first milestone:

```text
LocalProjectDataSource
+
QTableView
+
view rows
+
add/edit/delete local rows
+
sort
+
filter
+
search
+
basic validation
+
CSV import/export
```

Live database connection comes later.

---

## 116. First Live Database Scope

When direct database connectivity is implemented, begin with:

```text
connect
discover selected table
read metadata
read-only row paging
sort
filter
search
refresh
```

Do not enable direct destructive write-back in the same first step.

---

## 117. Initial Tests

### Test 1 — Local Paging

```text
100 rows
page size 25
→ four pages
```

### Test 2 — Sort

```text
sort CreatedAt descending
→ display/query order changes
→ underlying semantic schema unchanged
```

### Test 3 — No Fake CreatedAt

Source has no creation timestamp.

→ ERDFlow does not claim creation-order sorting exists.

### Test 4 — Filter

Filtering returns only matching rows without changing source data.

### Test 5 — Row Identity

After sorting, deleting by row identity affects the intended row, not the old visual index.

### Test 6 — Read-Only Capability

Read-only source:

```text
CanUpdate = false
```

→ edit action unavailable.

### Test 7 — Composite Key

Row identity with two key fields works.

### Test 8 — Large Source

The view can request only a page rather than all rows.

---

## 118. Architecture Invariants

### Invariant 1

The Data Grid is a view, not the authoritative data store.

### Invariant 2

Data access goes through a DataSource abstraction.

### Invariant 3

DataSource capabilities are explicit.

### Invariant 4

The architecture does not assume all rows fit in memory.

### Invariant 5

Sorting/filtering may be delegated to the source.

### Invariant 6

Sorting changes view/query order, not physical row order.

### Invariant 7

`CreatedAt` sorting requires a real timestamp/expression that represents that meaning.

### Invariant 8

Visual row index is not stable row identity.

### Invariant 9

Live database data is not automatically persisted into `.erdx`.

### Invariant 10

Direct live database write-back is deferred until explicitly designed.

### Invariant 11

Database drivers remain Infrastructure adapters.

### Invariant 12

Credentials are not stored as ordinary plaintext project data.

### Invariant 13

Schema/model identity and row identity remain separate.

### Invariant 14

Qt model/view is the desktop adapter, not the reusable DataSource contract.

---

## 119. Relationship to Other ADRs

Depends on:

```text
ADR-002 — Layered Architecture and Dependency Direction
ADR-003 — Desktop UI Technology
ADR-004 — Core Language Strategy
ADR-005 — Project File and Persistence Boundary
ADR-007 — Background Work / Concurrency Strategy
ADR-009 — Validation Gates and Conversion Policy
ADR-011 — Import Architecture
```

Works with future:

```text
ADR-013 — Future Rust Boundary
```

and the Data Workspace roadmap phase.

---

## 120. Open Implementation Decisions

This ADR intentionally leaves open:

- exact `DataSource` C++ interface,
- exact `DataPage` representation,
- exact typed cell-value variant,
- exact query/filter expression model,
- exact local sample-data persistence format,
- exact page size,
- exact cache policy,
- exact Qt `QAbstractItemModel` implementation,
- exact database driver libraries,
- exact live connection model,
- exact transaction/write-back policy,
- exact optimistic concurrency strategy,
- exact database deployment workflow,
- exact local-data undo policy.

These should be decided when implementation reaches the corresponding feature.

---

## 121. Decision Outcome

ERDFlow adopts:

```text
QTableView
      ↓
Qt Model Adapter
      ↓
Application Data Workspace
      ↓
Capability-Aware DataSource
      ↓
Local / Imported / Future Live Database Adapters
```

with:

```text
paging
structured sorting/filtering
stable row identity
background I/O
read-only-first live database access
```

and no assumption that every source is an in-memory editable table.

---

## 122. Final Principle

> The Data Workspace should feel like a spreadsheet, but its architecture must behave like a serious data-access system.
