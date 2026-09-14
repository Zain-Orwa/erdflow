# ADR-004 — Core Language Strategy

**Status:** Accepted

**Reviewed:** 2026-09-14

**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** Primary implementation language for the current core and strategy for future Rust adoption

---

## 1. Context

ERDFlow is a desktop-first visual database design application.

Its architecture separates:

```text
Presentation
     ↓
Application
     ↓
Domain
```

with Infrastructure connected through adapters.

The project must support:

```text
Conceptual ERD
Relational Schema
Physical Design
SQL generation
Data Workspace
Import/export
Validation
Mappings/provenance
Generation baselines
Diff/review
Undo/redo
Background work
```

The desktop UI decision has already selected:

```text
Qt 6
C++20
Qt Widgets
```

The remaining language question is:

> What language should ERDFlow use for its reusable application/core logic now, and how should the project prepare for possible Rust adoption later?

---

## 2. Decision

ERDFlow will use:

> **C++20 as the primary implementation language for the current desktop application, Application layer, Domain layer, and initial Infrastructure.**

Rust is:

> **a future optional core implementation language for selected modules, introduced only when a concrete engineering benefit justifies it.**

ERDFlow will **not** begin as a mixed C++/Rust application.

The architecture will keep the core sufficiently framework-independent so that selected modules can later move behind a stable language boundary without rewriting the desktop UI.

---

## 3. Current Language Model

For the first major implementation eras:

```text
Qt Presentation
      │
      │ C++20
      ▼
Application
      │
      │ C++20
      ▼
Domain
      │
      │ C++20
      ▼
Core rules / conversion / validation
```

Infrastructure is also initially implemented in C++ where practical.

This produces one primary implementation language for the first product.

---

## 4. Why C++20 Now

C++20 is the best current fit because:

- Qt's primary native API is C++,
- the desktop UI is already C++,
- ERDFlow requires strong native desktop integration,
- one language reduces early integration complexity,
- the Domain can remain independent of Qt even while written in C++,
- C++ is capable of implementing deterministic conversion, validation, parsing, model management, and data structures,
- CMake provides a mature build path,
- no language bridge is required before the product itself is proven.

The first engineering goal is to build a correct product architecture.

It is not to demonstrate a multi-language stack.

---

## 5. C++20 Is Not the Same as Qt Coupling

Choosing C++20 for the core does **not** mean choosing Qt types for the core.

Example:

```cpp
struct Entity
{
    EntityId id;
    std::string name;
};
```

is consistent with this ADR.

This would be undesirable in Domain code:

```cpp
struct Entity
{
    QUuid id;
    QString name;
};
```

if those Qt types become the fundamental reusable Domain representation.

Language choice and framework dependency are separate decisions.

---

## 6. Why One Primary Language First

A mixed-language architecture creates additional concerns:

```text
FFI boundary
build integration
data marshalling
error translation
ownership across languages
debugging across languages
toolchain complexity
packaging
CI complexity
developer cognitive load
```

Those costs are justified only when they solve a real problem.

ERDFlow does not yet have evidence that they are necessary.

Therefore:

> The product must first earn the complexity of a second systems language.

---

## 7. Rust Is Deferred, Not Rejected

Rust remains strategically attractive for selected future modules.

Possible candidates include:

```text
import parsers
validation engine
conversion engine
diff engine
project-format processing
large deterministic transformations
service-side reusable core
security-sensitive parsing
```

But these candidates are hypotheses.

They are not migration commitments.

---

## 8. Evidence Required Before Introducing Rust

Rust should be introduced only when at least one concrete need exists.

Examples:

### Reuse Requirement

A core module must be shared between:

```text
Qt desktop
VS Code extension backend
future service
CLI
```

### Safety Requirement

A parser or complex memory-sensitive subsystem would materially benefit from stronger memory-safety guarantees.

### Performance Requirement

Profiling identifies a real bottleneck and a Rust implementation offers a justified engineering path.

### Reliability Requirement

A subsystem has unacceptable classes of ownership/concurrency defects in its current implementation.

### Ecosystem Requirement

A Rust library/ecosystem capability offers substantially better value than available alternatives.

Without such evidence:

```text
stay in C++
```

---

## 9. Rust Must Not Be Introduced for Fashion

The following are not sufficient reasons:

```text
"Rust is newer"
"Rust is popular"
"Rust is safer in general"
"Rust may be faster"
"We planned to use Rust someday"
"We reached the Rust phase in the roadmap"
```

The decision must be based on a concrete module and measurable benefit.

---

## 10. No Big-Bang Rewrite

If Rust is introduced later, ERDFlow will not rewrite the entire application.

Preferred approach:

```text
Existing C++ system
      ↓
identify isolated module
      ↓
define stable boundary
      ↓
implement Rust version
      ↓
test parity
      ↓
measure
      ↓
adopt only if beneficial
```

This is incremental replacement.

---

## 11. Likely Long-Term Shape

A possible future architecture is:

```text
Qt / C++ Presentation
        │
        ▼
C++ Application boundary
        │
        ▼
Reusable Core API
        │
        ├── C++ modules
        └── selected Rust modules
```

Another future possibility:

```text
Qt Desktop
VS Code Extension
CLI
Service
      \   |   |   /
       Shared Core Contracts
               ↓
        Rust-heavy reusable core
```

Neither is committed today.

---

## 12. Stable Boundary First

Future language replacement is easier if today's interfaces use structured, framework-independent values.

Good:

```text
ConceptualModelSnapshot
      ↓
convertToRelational()
      ↓
RelationalConversionResult
```

Bad:

```text
QGraphicsItem*
      ↓
Rust conversion function
```

The first boundary is portable.

The second binds core logic to Qt and UI memory structures.

---

## 13. Domain Types

Initial Domain types should favor:

```cpp
std::string
std::vector
std::optional
std::variant
std::chrono
std::array
```

and ERDFlow-owned types such as:

```text
EntityId
AttributeId
RelationshipId
Cardinality
Participation
LogicalType
ValidationIssue
```

Avoid using framework-specific types solely for convenience.

---

## 14. Ownership Strategy

C++ ownership should be explicit.

Default direction:

```text
values where practical
clear parent ownership
RAII
smart pointers only where ownership requires them
non-owning references where lifetime is clear
```

Do not use raw owning pointers.

This rule applies to reusable Application/Domain ownership. In Qt adapters,
parent-owned QObjects may use Qt's
[parent-child lifetime model](https://doc.qt.io/qt-6/objecttrees.html);
raw pointers held by views are non-owning references. Do not give the same
object competing Qt-parent and smart-pointer owners.

The exact object graph is defined during Domain implementation.

---

## 15. Value-Oriented Core

ERDFlow should prefer value-oriented data where practical.

Example:

```text
Entity
Attribute
RelationshipParticipant
ValidationIssue
Mapping record
```

This improves:

- testing,
- snapshots,
- serialization,
- deterministic transformations,
- future FFI.

Not every object must be immutable.

The goal is clear ownership and predictable data flow.

---

## 16. C++ Exceptions

This ADR does not require or prohibit exceptions globally.

A later coding-policy decision may define exception use.

Current direction:

- expected domain/application failures should use structured result/error types where appropriate,
- exceptions may remain suitable for exceptional internal failures or library boundaries,
- exceptions must not become the only representation of normal validation outcomes.

Example:

```text
invalid conceptual readiness
```

is normally a validation result, not a crash-style exception.

---

## 17. C++ Undefined Behavior Risk

C++ gives ERDFlow significant control but also permits unsafe behavior.

Known risks include:

```text
dangling references
invalid pointer ownership
iterator invalidation
data races
undefined behavior
manual lifetime mistakes
```

The response is not immediate language replacement.

Initial mitigations include:

- RAII,
- standard containers,
- strong value types,
- limited raw pointer ownership,
- sanitizers,
- compiler warnings,
- unit tests,
- code review,
- immutable snapshots for background work.

---

## 18. Compiler Warnings

ERDFlow should compile its own code with strong warnings.

Initial Unix-like direction may include:

```text
-Wall
-Wextra
-Wpedantic
```

with additional warnings added carefully.

MSVC equivalents should be used on Windows.

The exact warning policy is a build-system decision.

Do not globally silence warnings to make a build green.

---

## 19. Sanitizers

During development, C++ builds should eventually support tools such as:

```text
AddressSanitizer
UndefinedBehaviorSanitizer
ThreadSanitizer where supported/useful
```

according to platform/toolchain availability.

Sanitizers complement tests.

They are especially important for native C++ code.

---

## 20. Static Analysis

Static analysis may later include tools such as:

```text
clang-tidy
compiler diagnostics
other focused analyzers
```

Introduce them incrementally.

Do not create a huge lint bureaucracy before implementation exists.

---

## 21. C++ Standard Version

The project baseline is:

```text
C++20
```

Do not depend on C++23/26 features unless a later ADR intentionally raises the baseline.

This keeps the initial cross-platform compiler requirements manageable.

---

## 22. Standard Library First

Where the C++ standard library solves a problem well, prefer it before adding external dependencies.

Examples:

```text
containers
strings
optional values
variants
algorithms
time utilities
```

External libraries are acceptable when they provide clear value.

Every foundational dependency should have a reason.

---

## 23. No Custom Reinvention Without Need

Do not implement custom versions of:

```text
string
vector
smart pointer
UUID parser
JSON parser
thread pool
```

merely to avoid dependencies or prove low-level ability.

ERDFlow is a product.

Use mature components where appropriate, while preserving architectural boundaries.

---

## 24. Qt Types at Adapters

Qt-specific conversion is allowed near Presentation/Infrastructure.

Example:

```text
Domain std::string
      ↕
Presentation QString
```

A small adapter/helper can handle the conversion.

Do not spread repeated conversions randomly across the entire codebase.

---

## 25. Stable IDs and Language Interop

ADR-001 chose strongly typed UUIDv7 identities.

This fits future Rust interoperability.

A language boundary can exchange identity as:

```text
16-byte UUID value
```

or:

```text
canonical UUID string
```

depending on the eventual FFI contract.

Qt-specific `QUuid` is not required across that boundary.

---

## 26. Conversion Functions

Deterministic transformations should be designed as structured functions/services.

Conceptually:

```cpp
RelationalConversionResult
convertToRelational(
    const ConceptualModel& model,
    const ConversionOptions& options);
```

This form is easy to:

- unit test,
- benchmark,
- move behind another implementation later.

Avoid conversion functions that directly manipulate Qt objects.

---

## 27. Parsing Strategy

Initial import parsing may be implemented in C++.

If future parsing workloads reveal strong reasons for Rust, parsers are a good candidate for isolated migration because their interfaces can naturally be:

```text
bytes/text
   ↓
parser
   ↓
structured result
```

That creates a clean language boundary.

---

## 28. Validation Strategy

Validation may initially be C++.

A good validation interface is also portable:

```text
Model Snapshot
      ↓
Validator
      ↓
ValidationIssue[]
```

If validation later moves to Rust, Presentation does not need to change.

---

## 29. Conversion Strategy

Conceptual → Relational conversion is initially C++.

The architecture should ensure the conversion engine does not depend on:

```text
Qt
filesystem
UI
database drivers
```

That isolation preserves future language flexibility.

---

## 30. Diff / Review Strategy

The eventual diff/review engine may become algorithmically significant.

Initially implement it in C++.

Only consider moving it later if:

- profiling,
- complexity,
- safety,
- reuse

provide concrete justification.

---

## 31. Data Source Strategy

Database/Data Source adapters remain separate from core language decisions.

A future Rust database adapter is possible.

A C++ database adapter is also possible.

The Data Source contract should not assume one implementation language.

---

## 32. Build-System Consequences

Initial build:

```text
CMake
C++ compiler
Qt 6
```

No Cargo requirement.

If Rust is introduced later:

```text
CMake
+
Cargo
+
explicit integration layer
```

becomes a deliberate architecture change.

This avoids adding a second build system before needed.

---

## 33. CI Consequences

Initial CI can focus on:

```text
CMake configure
C++ build
tests
platform matrix
```

Future Rust adoption would add:

```text
rust toolchain
cargo build/test
FFI compatibility tests
```

This additional maintenance cost is part of any future Rust decision.

---

## 34. Debugging Consequences

Single-language C++ debugging is simpler during the early product.

Mixed-language debugging can involve:

```text
C++ stack
FFI boundary
Rust stack
ownership transfer
error translation
```

This cost must be justified by actual benefit.

---

## 35. Error Boundary for Future Rust

If Rust appears later, errors must cross the boundary structurally.

Avoid exposing Rust panics as normal C++ application behavior.

Conceptually:

```text
Rust Result
      ↓
FFI-safe result/error representation
      ↓
C++ Application
```

No implementation is selected yet.

---

## 36. Memory Boundary for Future Rust

The language boundary should avoid ambiguous shared ownership.

Prefer:

```text
caller-owned input
structured output
explicit allocation/free rules
```

rather than:

```text
both languages mutate the same complex object graph
```

Coarse-grained calls are preferred over chatty per-object FFI.

---

## 37. Coarse-Grained FFI

Good future FFI:

```text
convert full conceptual snapshot
→ relational result
```

Less desirable:

```text
call Rust once for every attribute getter
```

A coarse boundary reduces:

- overhead,
- ownership complexity,
- API fragility.

---

## 38. FFI Technology Is Not Selected

This ADR does not choose:

```text
C ABI
CXX
cbindgen
UniFFI
custom bridge
```

The correct bridge depends on the actual future module.

Choosing it now would be premature.

---

## 39. Rust Version/Edition Is Not Selected

No Rust toolchain baseline is needed until Rust is introduced.

At that time, select:

- stable Rust,
- an edition appropriate to the implementation date,
- reproducible toolchain policy.

Do not maintain an unused Rust toolchain today.

---

## 40. Why Not Rust for Everything Now

A Rust-first core with Qt C++ UI is technically possible.

However, it would immediately require:

- an FFI design,
- duplicate build tooling,
- type conversion,
- two-language debugging,
- packaging coordination,
- additional learning and implementation overhead.

ERDFlow's current risk is product/domain complexity.

It is not lack of a second systems language.

Therefore this is rejected for the first implementation.

---

## 41. Why Not Rewrite Qt Bindings in Rust

ERDFlow should not attempt to avoid C++ by building the whole desktop UI through Rust/Qt bindings.

The selected desktop framework is natively C++.

Using the native C++ interface is simpler and better supported for the current product direction.

Rust can be valuable without owning the UI.

---

## 42. Alternative A — C++20 Everywhere Permanently

### Advantages

- one language,
- simplest Qt integration,
- mature tooling,
- no FFI.

### Why Not Lock Permanently

ERDFlow may later need:

- reusable service-side core,
- safer parsers,
- cross-product core modules,
- Rust-specific ecosystem advantages.

Therefore C++20 is the current primary language, not an eternal prohibition on Rust.

---

## 43. Alternative B — Rust Core From Day One

### Advantages

- memory safety,
- strong ownership model,
- modern concurrency guarantees,
- reusable non-Qt core.

### Rejected for Now

It introduces architectural and toolchain complexity before the first product milestone has proven the need.

---

## 44. Alternative C — Rust Everywhere Including UI

Rejected because ERDFlow selected Qt 6 as its desktop framework and Qt's primary development model is C++.

This would create unnecessary binding complexity.

---

## 45. Alternative D — Python Core

### Advantages

- fast prototyping,
- rich parsing/data ecosystem.

### Rejected as the primary core

ERDFlow's current architecture targets:

- native application performance,
- strong static typing,
- long-lived core models,
- C++ desktop integration.

Python may still appear in development tooling, scripts, or experiments.

It is not the primary application/core language.

---

## 46. Alternative E — TypeScript/Web Core

Rejected for the native desktop core.

A future VS Code extension may use TypeScript at its frontend boundary.

That does not require ERDFlow's semantic core to become TypeScript.

---

## 47. Future VS Code Extension

A VS Code extension will likely contain frontend-specific code in TypeScript because that is natural for VS Code extensions.

Possible future structure:

```text
VS Code / TypeScript
       ↓
extension integration boundary
       ↓
ERDFlow reusable core
```

The extension should reuse or call ERDFlow logic rather than independently reimplementing database rules.

The exact integration method is future work.

---

## 48. Cross-Platform Requirement

C++ code must remain portable across:

```text
macOS
Linux
Windows
```

Avoid unnecessary platform APIs in Domain/Application.

Platform-specific behavior belongs behind outer adapters.

---

## 49. ABI Stability

ERDFlow does not need to promise a public stable C++ ABI during early development.

Internal interfaces can evolve.

If a future plugin or language boundary requires a stable binary contract, that should be addressed explicitly then.

Do not freeze internal C++ class layouts prematurely.

---

## 50. API Stability

Internal APIs are allowed to evolve before v1.

However, architectural boundaries should remain deliberate.

Stable IDs and persisted project meaning require more care than private helper APIs.

---

## 51. Header Discipline

Domain headers should avoid unnecessary dependencies.

Prefer:

- forward declarations where appropriate,
- standard-library types,
- ERDFlow-owned domain headers.

Do not include Qt Widgets from Domain headers.

---

## 52. Namespace Direction

ERDFlow code should live under an explicit project namespace.

Conceptually:

```cpp
namespace erdflow
{
}
```

More specific sub-namespaces may later include:

```text
erdflow::domain
erdflow::application
erdflow::infra
```

The exact namespace layout should stay simple.

---

## 53. Coding Style

This ADR does not lock full formatting/style rules.

A later developer guide may define:

- naming,
- formatting,
- include ordering,
- `const` policy,
- error types,
- ownership conventions.

Prefer automated formatting over manual style debates once implementation begins.

---

## 54. Testing Language

Core automated tests should initially be C++.

This keeps test code close to implementation.

The exact test framework is selected during repository/build foundation.

---

## 55. Benchmark Language

Core benchmarks should initially be C++.

If a module moves to Rust, Rust-specific benchmarks may be added.

Cross-language comparisons must measure equivalent behavior.

---

## 56. Performance Principle

Do not assume:

```text
C++ = fast
Rust = fast
```

without measuring the algorithm and implementation.

The primary performance drivers are:

- algorithms,
- data structures,
- memory behavior,
- I/O,
- unnecessary copies,
- rendering architecture,
- concurrency design.

Language alone does not solve poor architecture.

---

## 57. Security Principle

C++ memory safety risks are real.

Therefore:

```text
untrusted input
```

must be handled carefully.

This is especially important for:

- SQL import,
- JSON import,
- CSV import,
- `.erdx` loading,
- future network/database input.

If parser hardening becomes a major concern, Rust may become particularly attractive for those modules.

---

## 58. Background Work

Background execution may initially use Qt/C++ infrastructure.

Core tasks should still accept structured input and return structured results.

This allows the executor technology and even task implementation language to change later.

---

## 59. Snapshot Compatibility

Immutable/consistent snapshots used for conversion and background work also make future Rust boundaries easier.

Example:

```text
ConceptualSnapshot
      ↓
conversion
      ↓
RelationalCandidate
```

is a better language boundary than sharing the live mutable Project object.

---

## 60. Persistence Compatibility

The persisted `.erdx` format must not depend on C++ object memory layout.

Wrong:

```text
dump raw C++ struct bytes
```

Preferred:

```text
versioned document representation
```

This makes files independent of:

- compiler,
- architecture,
- C++ implementation,
- future Rust implementation.

---

## 61. Migration Compatibility

A future Rust core must be able to consume the same logical project representation without changing semantic IDs or forcing project-file rewrites solely because implementation language changed.

Language migration must be invisible to project semantics where practical.

---

## 62. Decision Invariants

### Invariant 1

C++20 is the primary implementation language for the current ERDFlow desktop/application/core.

### Invariant 2

Qt remains a Presentation/adapter concern rather than the Domain language model.

### Invariant 3

ERDFlow does not begin as a mixed C++/Rust application.

### Invariant 4

Rust is optional and evidence-driven.

### Invariant 5

No big-bang Rust rewrite is planned.

### Invariant 6

Future Rust adoption happens module by module behind explicit boundaries.

### Invariant 7

Core interfaces use framework-independent structured data.

### Invariant 8

FFI technology is not selected until a real Rust module exists.

### Invariant 9

Persistence does not depend on C++ memory layout.

### Invariant 10

Language choice does not replace profiling, testing, or sound algorithms.

---

## 63. First Implementation Scope

The first implementation requires only:

```text
C++20
Qt 6
CMake
C++ test tooling
```

No Rust directory.

No Cargo workspace.

No FFI.

No language bridge.

The first goal is to build:

```text
Desktop shell
Application command path
Minimal Conceptual Domain
Canvas foundation
```

cleanly in C++20.

---

## 64. Future Rust Evaluation Checklist

Before adding Rust, answer:

```text
1. Which exact module?
2. What problem does migration solve?
3. What evidence shows the problem exists?
4. Why is Rust better than improving the current C++ module?
5. What is the proposed boundary?
6. What data crosses it?
7. Who owns memory?
8. How are errors represented?
9. What is the build/CI cost?
10. How will parity be tested?
11. How will performance be measured?
12. Can the change be rolled back?
```

If those questions cannot be answered clearly, do not introduce Rust yet.

---

## 65. Relationship to Other ADRs

Depends on:

```text
ADR-001 — Stable Identity Strategy
ADR-002 — Layered Architecture and Dependency Direction
ADR-003 — Desktop UI Technology
```

Later ADRs will refine:

```text
Persistence boundary
Command/Undo strategy
Concurrency
Mappings/provenance
Validation/conversion
Future Rust boundary
```

This ADR defines the language strategy those decisions operate within.

---

## 66. Consequences — Positive

This decision gives ERDFlow:

- one primary implementation language at the start,
- direct Qt integration,
- simpler builds,
- simpler debugging,
- lower early complexity,
- native performance,
- portable core design,
- a deliberate path to Rust later,
- no premature FFI dependency.

---

## 67. Consequences — Costs

ERDFlow accepts:

- C++ memory-safety risks,
- need for strong ownership discipline,
- need for sanitizers/static analysis,
- potential future migration work if Rust becomes justified,
- responsibility to keep Qt/framework types out of the reusable core.

These costs are manageable and visible.

---

## 68. Decision Outcome

ERDFlow adopts:

```text
C++20 now
+
framework-independent core design
+
Rust later only where evidence justifies it
```

The project explicitly rejects:

```text
mixed C++/Rust from day one
```

and also rejects:

```text
a permanent promise that Rust can never be used
```

---

## 69. Final Principle

> Use one language while the product is young. Preserve clean boundaries so a second language can earn its place later.

ERDFlow's architecture should make future Rust adoption possible without making present-day Rust adoption mandatory.

---

## 70. Review Record — 2026-09-14

Outcome: accepted. Confirmed C++20 with evidence-driven optional Rust; clarified parent-owned Qt lifetimes at outer adapters while retaining explicit ownership in reusable code.

See [Phase 0 review](../PHASE_0_REVIEW.md) for cross-document findings,
quality requirements, and deferred implementation gates. Acceptance records
the architecture contract, not completion of its implementation or tests.
