# ADR-002 — Layered Architecture and Dependency Direction

**Status:** Accepted

**Date:** 2026-08-31  
**Reviewed:** 2026-09-14

**Project:** ERDFlow  
**Decision Scope:** Core architectural boundaries and allowed dependency direction

---

## 1. Context

ERDFlow is a desktop-first visual database design application.

Its long-term product flow is:

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

The application will also eventually support:

- deterministic import,
- versioned `.erdx` persistence,
- undo/redo,
- background work,
- mappings/provenance,
- safe regeneration,
- controlled Schema → Conceptual propagation,
- database connections,
- future AI adapters,
- future plugins,
- possible VS Code integration,
- possible future Rust core components.

The architecture therefore needs to prevent UI, persistence, import, and external-service concerns from becoming tightly coupled to the core database-design logic.

The central architectural risk is:

> If Qt widgets, file formats, or external systems become the source of truth, ERDFlow becomes difficult to test, evolve, reuse, and safely transform.

---

## 2. Decision

ERDFlow will use a:

> **Layered architecture with ports/adapters at external boundaries.**

The main dependency direction is:

```text
Presentation
     ↓
Application
     ↓
Domain
```

Infrastructure does not sit "under" the Domain as something the Domain depends on.

Instead, Infrastructure implements interfaces/ports required by the Application/Core side.

Conceptually:

```text
                  ┌─────────────────────┐
                  │    Presentation     │
                  │       Qt 6          │
                  └──────────┬──────────┘
                             │
                             ▼
                  ┌─────────────────────┐
                  │    Application      │
                  │ Commands / Use Cases│
                  └──────────┬──────────┘
                             │
                             ▼
                  ┌─────────────────────┐
                  │       Domain        │
                  │ Models / Rules      │
                  └─────────────────────┘

         ┌────────────────────────────────────┐
         │      Infrastructure / Adapters     │
         │ .erdx • Import • Export • DB • AI │
         └────────────────────────────────────┘
                 ▲
                 │ implements ports
                 │ required by Application/Core
```

---

## 3. Core Dependency Rule

Dependencies must point inward toward the Domain.

Allowed:

```text
Presentation → Application
Application → Domain
Infrastructure → Application/Core interfaces
Infrastructure → Domain types where necessary
```

Not allowed:

```text
Domain → Presentation
Domain → Qt
Domain → Infrastructure
Domain → filesystem
Domain → database driver
Domain → AI provider
```

The Domain is the most stable inner layer.

### 3.1 Application Assembly

The desktop startup code is the composition root: it constructs concrete
Infrastructure adapters and supplies them to Application use cases through
their ports. This assembly code may reference Presentation, Application,
Domain, and Infrastructure to connect them.

Widgets still use Application-facing APIs. Application code does not construct
concrete Infrastructure adapters or depend on desktop startup code. Runtime
calls through a port do not reverse the source-code dependency direction.

Add this wiring only as actual capabilities need it; no dependency-injection
framework is required.

---

## 4. Why This Architecture

ERDFlow has several very different responsibilities:

```text
drawing
database modeling
conversion
validation
undo
persistence
import/export
background work
data access
future AI
```

If those responsibilities directly know about each other, complexity grows quickly.

Example of a bad dependency chain:

```text
QGraphicsItem
    ↓
writes JSON
    ↓
calls SQL generator
    ↓
updates data grid
```

This would make UI code responsible for database semantics.

Instead:

```text
Qt user action
      ↓
Application command
      ↓
Domain change
      ↓
result/event
      ↓
UI refresh
```

and separately:

```text
Application
      ↓
ProjectStore port
      ↓
ErdxProjectStore adapter
      ↓
filesystem
```

---

## 5. Layer 1 — Presentation

The Presentation layer is the Qt 6 desktop interface.

Responsibilities include:

- main window,
- menu/ribbon,
- explorer,
- properties panel,
- conceptual canvas,
- schema workspace,
- physical/table workspace,
- SQL workspace,
- Data Workspace,
- dialogs,
- status/progress UI,
- user confirmation prompts,
- keyboard/mouse/touch input,
- rendering.

Presentation may use Qt types freely.

Examples:

```text
QString
QWidget
QGraphicsItem
QTableView
QAbstractItemModel
QAction
QUndoStack
```

But these types must not become the Domain model.

---

## 6. Presentation Is Not the Source of Truth

Critical invariant:

```text
QGraphicsRectItem != Entity
```

Correct:

```text
Domain Entity
     │
     └── represented by Qt EntityGraphicsItem
```

The UI may be destroyed and rebuilt.

The semantic object must still exist.

Example:

```text
EntityId = E1
Name = Student
```

may appear simultaneously in:

```text
Canvas
Explorer
Properties
Search
Validation panel
```

All views refer to the same semantic object.

---

## 7. Presentation Responsibilities

Presentation is responsible for:

```text
how information looks
how the user interacts
how commands are triggered
how results are displayed
```

Presentation is not responsible for:

```text
what a valid M:M conversion means
how a weak entity converts
whether a Schema edit is conceptually meaningful
how project identity works
how `.erdx` is serialized
```

---

## 8. Layer 2 — Application

The Application layer coordinates user intentions and use cases.

Examples:

```text
CreateEntity
RenameEntity
DeleteEntity
AddAttribute
CreateRelationship
ChangeCardinality
ConvertToRelational
SaveProject
ImportSql
ReviewRegeneration
ApplyPropagation
```

Application knows:

- which Domain operation to perform,
- when validation is required,
- when to ask for review,
- when to invoke persistence/import/export ports,
- how to coordinate background work,
- how to organize undoable actions.

Application should not know how Qt renders a rectangle.

---

## 9. Application Commands

Meaningful mutations should pass through Application commands.

Example:

```text
User edits entity name
      ↓
RenameEntityCommand
      ↓
Domain Entity updated
      ↓
Application result
      ↓
Presentation refresh
```

This provides a single place for:

- validation,
- undo,
- change tracking,
- propagation classification,
- testing.

---

## 10. Application Is Not the Domain

The Application layer coordinates actions.

The Domain contains the database meaning.

Example:

```text
Application:
"Convert this Conceptual Model"

Domain/Core:
"Apply deterministic conversion rules"
```

The Application should not duplicate conversion rules.

---

## 11. Layer 3 — Domain

The Domain contains ERDFlow's reusable database-design meaning.

Examples:

```text
Project
ConceptualModel
Entity
Attribute
Relationship
RelationshipParticipant
RelationalSchema
Relation
PhysicalModel
Table
Column
Mapping
Baseline
ValidationIssue
```

It also contains or exposes deterministic rules such as:

```text
validation
conversion
change classification
mapping/provenance rules
```

---

## 12. Domain Independence

The Domain must not depend on:

```text
Qt
filesystem APIs
JSON libraries
SQL database drivers
HTTP
AI SDKs
plugin SDKs
VS Code APIs
```

The Domain should prefer framework-independent C++ types.

Examples:

```cpp
std::string
std::vector
std::optional
std::chrono
```

and ERDFlow-owned types such as:

```text
EntityId
AttributeId
RelationshipId
LogicalType
Cardinality
Participation
```

---

## 13. Domain Stability

The Domain is expected to change more slowly than the UI.

Example:

The UI may evolve from:

```text
QGraphicsScene
```

to another rendering approach later.

The Conceptual rule:

```text
M:M relationship
```

does not change because the rendering engine changes.

Therefore the Domain must remain isolated from presentation technology.

---

## 14. Infrastructure / Adapters

Infrastructure connects ERDFlow to external mechanisms.

Examples:

```text
.erdx persistence
filesystem
SQL parser implementation
CSV parser
JSON parser
database driver
export system
future AI provider
future plugin system
background execution adapter
```

Infrastructure is not the source of Domain truth.

---

## 15. Port / Adapter Principle

Where the Application needs an external capability, it should depend on an interface/port.

Example:

```cpp
class ProjectStore
{
public:
    virtual ~ProjectStore() = default;

    virtual LoadProjectResult load(...) = 0;
    virtual SaveProjectResult save(...) = 0;
};
```

Then Infrastructure provides:

```text
ErdxProjectStore
```

The exact interface is deferred.

The important architectural direction is:

```text
Application defines need
Infrastructure implements mechanism
```

---

## 16. Dependency Inversion

Without dependency inversion:

```text
Application
    ↓
QFile
```

Application becomes tied directly to Qt filesystem behavior.

Preferred:

```text
Application
    ↓
ProjectStore port
    ↑
ErdxProjectStore adapter
    ↓
QSaveFile / filesystem
```

This allows testing with:

```text
InMemoryProjectStore
```

without touching disk.

---

## 17. Persistence Example

Wrong:

```cpp
class Entity
{
public:
    QJsonObject toJson() const;
};
```

This couples Domain Entity to a persistence representation and Qt.

Preferred:

```text
Domain Project
      ↓
ProjectDocumentMapper
      ↓
ErdxDocument
      ↓
Serializer
```

Persistence details remain outside the Domain.

---

## 18. Import Example

Wrong:

```text
CSV importer
      ↓
creates QGraphicsItems directly
```

Preferred:

```text
CSV file
   ↓
CsvImporter
   ↓
Import Intermediate Representation
   ↓
Application review/mapping
   ↓
Domain command
   ↓
Project
   ↓
Presentation updates
```

---

## 19. SQL Generation Example

Wrong:

```text
SQL workspace widget
      ↓
contains conversion rules
```

Preferred:

```text
Physical Model
      ↓
SQL generation core
      ↓
SqlGenerationResult
      ↓
Application
      ↓
SQL workspace displays result
```

---

## 20. Data Workspace Example

Presentation:

```text
QTableView
```

Adapter:

```text
Qt Table Model
```

Core boundary:

```text
DataSource
```

Possible implementations:

```text
ProjectLocalDataSource
ImportedDataSource
FutureDatabaseDataSource
```

This keeps Qt table rendering separate from row-storage/query behavior.

---

## 21. Undo / Redo Boundary

Qt provides:

```text
QUndoCommand
QUndoStack
```

ERDFlow may use them in desktop adapters that connect Presentation to
Application commands. These adapters remain outside the reusable Application
layer.

But core semantic commands must not require Qt.

Conceptually:

```text
ERDFlow Command
      ↓
Qt Undo Adapter
      ↓
QUndoStack
```

This preserves future reuse in:

```text
tests
CLI tools
VS Code extension
future Rust core
future web/service layer
```

---

## 22. Background Work Boundary

The UI thread must not execute known heavy computations that can be safely
isolated. The Application/UI coordination thread initially owns live project
mutation; workers compute on consistent snapshots or isolated inputs.

Application coordinates:

```text
Import
Validation
Conversion
Auto-layout
Large Save/Load
```

through a background execution boundary.

Conceptually:

```text
Application
      ↓
TaskRunner port
      ↑
QtBackgroundExecutor
```

Domain operations remain callable synchronously as normal functions.

Threading is orchestration.

It is not part of Domain meaning.

Workers return structured results without mutating the live Project or Qt
widgets. Application checks whether a result is still applicable before
applying it through the normal command path. ADR-007 defines the detailed
concurrency policy.

---

## 23. Controlled Mutation

Meaningful Domain mutation should occur through controlled Application flows.

Avoid:

```text
Widget directly edits Entity vector
```

Prefer:

```text
Widget
  ↓
Application Command
  ↓
Domain
```

This supports:

- undo,
- validation,
- auditability,
- propagation,
- future collaboration.

---

## 24. Read-Only Presentation Models

Presentation may construct read-oriented adapters/view models.

Examples:

```text
ExplorerViewModel
ConceptualCanvasViewModel
PropertiesViewModel
SchemaTableModel
DataGridTableModel
```

These may transform Domain information into Qt-friendly form.

They do not replace the Domain.

---

## 25. Cross-Level Conversion Direction

The normal product direction is:

```text
Conceptual
      ↓
Relational
      ↓
Physical
      ↓
SQL
```

These transformations belong to Domain/Core services or pure transformation modules.

Presentation requests conversion.

Application coordinates.

Domain/Core performs transformation.

Infrastructure is not responsible for semantic conversion rules.

---

## 26. Controlled Back-Propagation

Schema → Conceptual is not a permanent two-way synchronization engine.

Application coordinates:

```text
Schema edit
      ↓
classification
      ↓
conceptual equivalent?
      ↓
Explain
      ↓
Ask
      ↓
Apply if approved
```

Domain/Core provides the semantic facts.

Presentation asks the user.

---

## 27. Mapping / Provenance Placement

Mappings/provenance are part of ERDFlow's semantic/project model.

They are not merely UI metadata.

They support:

- conversion lineage,
- regeneration,
- explanations,
- controlled propagation.

Therefore their meaning belongs to the Domain/Core.

Persistence only serializes them.

---

## 28. Stable Identity Placement

Stable IDs from ADR-001 belong to the Domain/Core.

Presentation holds/references them.

Infrastructure persists them.

No outer layer invents replacement identities for Domain objects.

---

## 29. Errors Across Layers

Domain/Core returns structured errors.

Examples:

```text
ValidationIssue
ConversionError
InvalidMapping
```

Application returns use-case results.

Infrastructure may return:

```text
PersistenceError
ImportError
DatabaseError
```

Presentation decides how to show them.

Avoid:

```text
Domain
  ↓
QMessageBox
```

---

## 30. Logging Boundary

Domain may produce structured diagnostics/events where useful.

But it should not depend directly on a UI logger.

Logging implementation belongs outside the Domain.

Sensitive imported/database values should not be logged by default.

---

## 31. Configuration Boundary

UI preferences:

```text
theme
window size
panel visibility
```

belong to Presentation/Application configuration.

Database/project semantics:

```text
logical types
relationship participation
dialect selection
```

belong to project/domain configuration where appropriate.

Do not mix user-interface settings with semantic project state.

---

## 32. Repository Direction

The repository may evolve toward:

```text
erdflow/
│
├── app/
│   └── desktop/
│
├── application/
│
├── domain/
│
├── infrastructure/
│
├── tests/
│
├── docs/
└── assets/
```

This directory structure expresses responsibility.

It must not become bureaucracy.

Create directories/files only when implementation needs them.

---

## 33. Build Target Direction

Longer term, CMake may expose targets conceptually similar to:

```text
erdflow_domain
erdflow_application
erdflow_infrastructure
erdflow_desktop
```

This can help enforce dependency direction.

Not required immediately.

The first implementation may start with fewer targets and split as code grows.

---

## 34. Compile-Time Boundary Enforcement

Where practical, build configuration should prevent forbidden dependencies.

Example:

```text
erdflow_domain
```

should not link:

```text
Qt6::Widgets
Qt6::Gui
```

If the Domain requires Qt to compile, the architectural boundary has probably leaked.

QtCore should also be avoided in Domain unless a future ADR explicitly justifies an exception.

---

## 35. Testability

Layer separation must produce direct test benefits.

### Domain tests

No GUI required.

Examples:

```text
Entity rename
M:M conversion
weak-entity validation
mapping lookup
```

### Application tests

Use fake/in-memory ports.

Examples:

```text
SaveProject use case
Rename command
Propagation decision
```

### Infrastructure tests

Use actual serializers/parsers.

### Presentation tests

Test Qt-specific interaction only where needed.

---

## 36. Fake Adapters

Ports allow simple test doubles.

Example:

```text
FakeProjectStore
FakeDataSource
FakeTaskRunner
```

This allows Application behavior to be tested without:

```text
filesystem
network
database
Qt windows
```

---

## 37. Future VS Code Extension

A future VS Code extension should not recreate ERDFlow's database rules.

Preferred future direction:

```text
VS Code UI
      ↓
ERDFlow Application/Core API
      ↓
Domain
```

The desktop Qt UI and VS Code UI may differ.

The underlying semantic rules should remain consistent.

This architecture makes that possible.

---

## 38. Future Rust Boundary

Rust may later implement selected core capabilities.

This architecture prepares for that by ensuring the reusable core does not require Qt objects.

Bad boundary:

```text
Rust function receives QGraphicsItem*
```

Good boundary:

```text
Structured model data
      ↓
Core operation
      ↓
Structured result
```

The FFI mechanism remains undecided.

---

## 39. Future AI Boundary

AI is an external capability.

Conceptually:

```text
Application
      ↓
AI capability port
      ↑
AI provider adapter
```

AI returns:

```text
proposal
```

not direct uncontrolled Domain mutation.

Then:

```text
proposal
  ↓
validation
  ↓
review
  ↓
application command
```

---

## 40. Future Database Boundary

Database connections belong to Infrastructure/Data Source adapters.

The Domain does not depend on:

```text
PostgreSQL driver
MySQL driver
ODBC
SQLite connection API
```

A database adapter translates between external database behavior and ERDFlow's core contracts.

---

## 41. Future Plugin Boundary

Plugins are future adapters/extensions.

The Domain must remain useful without any plugin system.

Potential plugin points may later include:

```text
Importer
Exporter
Dialect
DataSource
AI provider
Validation extension
```

No plugin framework is required now.

---

## 42. Alternatives Considered

### Alternative A — Traditional Monolithic Qt Application

Example:

```text
Qt widgets
+ business rules
+ file handling
+ SQL logic
```

all inside UI classes.

### Advantage

Fastest to prototype initially.

### Rejected

Because ERDFlow has long-lived database semantics and multiple future frontends/integrations.

The cost of separating concerns later would be large.

---

## 43. Alternative B — Strict Four-Layer Stack

Example:

```text
Presentation
    ↓
Application
    ↓
Domain
    ↓
Infrastructure
```

### Rejected

Because this incorrectly suggests:

```text
Domain → Infrastructure
```

ERDFlow instead uses dependency inversion at external boundaries.

Infrastructure implements ports required by inner layers.

---

## 44. Alternative C — Full Hexagonal Architecture Everywhere

Every dependency becomes a port/interface from day one.

### Advantage

Maximum abstraction.

### Rejected for initial implementation

It would create excessive ceremony.

ERDFlow will use ports/adapters where external mechanisms or substitution matter.

Do not wrap trivial pure Domain operations in unnecessary interfaces.

---

## 45. Alternative D — Microservices

Rejected for the desktop core.

ERDFlow is a local-first desktop application.

Distributed services may exist later for:

```text
sync
collaboration
AI
```

but they are not the architecture of the desktop product.

---

## 46. Alternative E — Qt Everywhere

Use Qt types in every layer.

### Advantage

Convenient during early development.

### Rejected

Because it couples core logic to:

- Qt,
- desktop implementation,
- future migration difficulty,
- future Rust/VS Code reuse barriers.

Qt belongs at outer boundaries.

---

## 47. Practical Boundary Rule

Not every file needs an interface.

Use an abstraction when:

- an external system is involved,
- replacement/testing matters,
- implementation choice should remain outside the Domain,
- more than one implementation is likely,
- the boundary protects architectural direction.

Do not create interfaces merely to say the architecture is "clean."

---

## 48. Dependency Smell Examples

If we see:

```cpp
#include <QGraphicsItem>
```

inside:

```text
domain/
```

that is a warning.

If we see:

```cpp
QMessageBox::warning(...)
```

inside conversion code, that is a violation.

If we see:

```text
Entity::saveToFile()
```

that directly serializes `.erdx`, that is likely a persistence leak.

If we see:

```text
CsvImporter creates canvas objects
```

that is a UI coupling violation.

---

## 49. Allowed Qt Boundary

Qt is expected in:

```text
app/desktop/
```

and potentially Qt-specific infrastructure/adapters such as:

```text
QtBackgroundExecutor
QtSettingsStore
ErdxProjectStore using QSaveFile
```

Qt-specific code remains outside the Domain.

---

## 50. Allowed Domain Dependencies

The Domain may depend on:

- C++ standard library,
- ERDFlow-owned small foundational value types,
- carefully selected framework-independent libraries if justified.

A future external core library must be documented if it becomes architecturally significant.

---

## 51. Application Dependency Rule

Application may depend on:

```text
Domain
Application-owned ports/interfaces
```

The reusable Application layer must not depend directly on:

```text
Qt, including QtCore and Qt undo classes
concrete Infrastructure adapters
database drivers
filesystem implementation
AI SDKs
```

Qt-specific integration belongs in desktop or Infrastructure adapters. Ports
and use-case results use framework-independent types so Application behavior
can be tested without Qt.

---

## 52. Infrastructure Dependency Rule

Infrastructure may depend on:

```text
Application ports
Domain value/model types
QtCore or other libraries
filesystem
parsers
database drivers
external APIs
```

Infrastructure is allowed to be technology-specific.

That is its purpose.

---

## 53. Presentation Dependency Rule

Presentation may depend on:

```text
Application API
Presentation view models/adapters
Qt
```

It may read Domain-friendly result types through Application-facing APIs.

Avoid giving widgets unrestricted mutable access to the full Project graph.

---

## 54. Data Flow Example — Create Entity

```text
User clicks Entity
      ↓
Qt Presentation
      ↓
CreateEntityCommand
      ↓
Application
      ↓
ConceptualModel
      ↓
Entity created with EntityId
      ↓
Result
      ↓
Qt canvas creates representation
```

---

## 55. Data Flow Example — Save Project

```text
User clicks Save
      ↓
Presentation
      ↓
SaveProject use case
      ↓
Application
      ↓
ProjectStore port
      ↓
ErdxProjectStore adapter
      ↓
ProjectDocumentMapper
      ↓
Serializer / filesystem
```

Domain does not know a save button was clicked.

---

## 56. Data Flow Example — Convert

```text
User clicks Convert
      ↓
Presentation
      ↓
Application use case
      ↓
Validation
      ↓
Domain/Core conversion
      ↓
Candidate Schema
      ↓
Application review flow
      ↓
Presentation
```

---

## 57. Data Flow Example — Large Import

```text
User selects CSV
      ↓
Presentation
      ↓
Application
      ↓
Importer port / adapter
      ↓
Background executor
      ↓
Import result
      ↓
Application review
      ↓
Domain commands
      ↓
Presentation refresh
```

---

## 58. Data Flow Example — Safe Regeneration

```text
Conceptual change
      ↓
Application
      ↓
Generate candidate
      ↓
Domain mappings/baseline comparison
      ↓
Reviewable change set
      ↓
Presentation
      ↓
User accepts/rejects
      ↓
Application applies approved changes
```

---

## 59. Architecture Invariants

### Invariant 1

The Domain does not depend on Qt.

### Invariant 2

The Domain does not depend on Infrastructure.

### Invariant 3

Qt graphics items are not semantic database objects.

### Invariant 4

Presentation triggers Application use cases rather than directly owning Domain rules.

### Invariant 5

Application coordinates behavior but does not duplicate Domain rules.

### Invariant 6

External systems are connected through adapters/ports where the boundary matters.

### Invariant 7

Persistence representation is separate from Domain representation.

### Invariant 8

Importers do not manipulate canvas widgets directly.

### Invariant 9

Background execution is orchestration, not Domain meaning.

### Invariant 10

Stable IDs remain Domain/Core values.

### Invariant 11

Mappings/provenance are semantic project information, not UI decorations.

### Invariant 12

Structured errors flow outward; Domain code does not show UI dialogs.

### Invariant 13

Future frontends should reuse the same core rules rather than reimplement them.

---

## 60. First Implementation Scope

Do not build every port immediately.

For the first Conceptual Editor milestone, start with the smallest useful path:

```text
Qt Presentation
      ↓
Application Commands
      ↓
Minimal Conceptual Domain
```

Initial Infrastructure can remain minimal.

Add persistence when Phase 15 requires `.erdx`.

Add import adapters when the Import phase begins.

Add Data Source adapters when Data Workspace begins.

---

## 61. Initial Build Boundary Recommendation

A reasonable early structure is:

```text
domain/
    conceptual/

application/
    commands/

app/
    desktop/
```

Later:

```text
infrastructure/
    persistence/
    import/
```

Do not create dozens of empty folders now.

---

## 62. Consequences — Positive

This decision provides:

- testable Domain logic,
- clean Qt boundary,
- easier future VS Code reuse,
- easier future Rust migration,
- replaceable persistence/import mechanisms,
- better control over undo/redo,
- safer background work,
- clearer ownership,
- less accidental coupling.

---

## 63. Consequences — Costs

This architecture adds:

- more explicit boundaries,
- some interface/adapter code,
- mapping between Qt and Domain types,
- more discipline than a quick prototype.

These costs are acceptable because ERDFlow is intended to become a serious multi-stage database-design product rather than a temporary demo.

---

## 64. Risk — Over-Abstraction

A clean architecture can become over-engineered.

Risk:

```text
interface for everything
factory for every class
dependency injection everywhere
```

ERDFlow must avoid this.

Rule:

> Introduce an abstraction only when it protects a real architectural boundary or provides a real testing/substitution benefit.

---

## 65. Risk — Domain Becoming a God Layer

Keeping logic out of the UI does not mean putting everything in one giant Domain object.

Avoid:

```text
Project
    knows everything
    performs everything
```

Use focused domain models and pure services/transformation modules where appropriate.

---

## 66. Risk — Application Becoming a God Layer

Application should orchestrate.

It should not absorb:

- conversion algorithms,
- SQL parser internals,
- UI rendering logic,
- persistence serialization.

---

## 67. Enforcement During Code Review

When adding code, ask:

```text
Which layer owns this responsibility?
What does this file depend on?
Could this code be tested without Qt?
Is this an external mechanism or Domain meaning?
```

If the answer is unclear, stop and resolve the boundary before adding more code.

---

## 68. ADR Relationship

This ADR depends on:

```text
ADR-001 — Stable Identity Strategy
```

because stable IDs are part of the Domain/Core boundary.

It will later be complemented by:

```text
ADR-005 — Project File and Persistence Boundary
ADR-006 — Command / Undo Strategy
ADR-007 — Background Work / Concurrency Strategy
ADR-008 — Cross-Level Mapping and Provenance
ADR-009 — Validation Gates and Conversion Policy
```

---

## 69. Decision Outcome

ERDFlow adopts:

```text
Layered architecture
+
inward dependency direction
+
ports/adapters at external boundaries
```

with:

```text
Presentation → Application → Domain
```

and Infrastructure implementing outward-facing mechanisms without creating a Domain → Infrastructure dependency.

---

## 70. Final Principle

> Keep database meaning in the Domain, coordinate user intent in the Application, render interaction in Presentation, and keep external mechanisms behind adapters.

That dependency direction must remain stable as ERDFlow grows.

---

## 71. Review Record — 2026-09-14

Outcome: accepted after checking the dependency rules against the product
requirements, Architecture v0.1, accepted ADR-001, and the proposed command
and concurrency directions in ADR-006 and ADR-007. Their subsequent reviews
are recorded in the [completed Phase 0 review](../PHASE_0_REVIEW.md).

This review clarified application assembly, Qt-independent Application code,
and the boundary between background computation and live project mutation.

The decision supports the five qualities in
[Product §4.1](../1.ERDFlow_PRODUCT.md#41-system-quality-requirements):

- **Sustainable:** Introduce only the boundaries needed for current features;
  avoid unnecessary frameworks and speculative adapters.
- **Scalable:** Isolate expensive computation and keep live mutation controlled;
  measure actual performance during implementation.
- **Secure:** Keep external mechanisms behind adapters, validate their results
  before application, and avoid logging sensitive values by default.
- **Maintainable:** Give each responsibility one owner and test Domain and
  Application behavior independently of Qt.
- **Usable:** Keep UI feedback and interaction in Presentation, backed by
  structured results and recoverable Application commands.

Acceptance records the architecture decision, not implementation completion or
proof that performance and security targets have been met. Build boundaries
and behavioral checks will be added with the relevant implementation phases.
