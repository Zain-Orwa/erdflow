# ADR-013 — Future Rust Boundary

**Status:** Accepted

**Reviewed:** 2026-09-14

**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** Architectural boundary for any future Rust modules, including data exchange, ABI safety, ownership, errors, threading, versioning, and migration strategy

---

## 1. Context

ERDFlow currently uses:

```text
C++20
+
Qt 6
+
CMake
```

for the desktop application and initial core.

ADR-004 deliberately decided:

```text
C++20 now
Rust later only when evidence justifies it
```

Rust is therefore not part of the first implementation.

However, several future subsystems could eventually benefit from Rust:

```text
parsers
validation
conversion
diff/regeneration
large deterministic transformations
security-sensitive external-input processing
reusable non-Qt core modules
future CLI/service components
```

If Rust is introduced later, ERDFlow must avoid an improvised boundary that leaks:

```text
Qt pointers
C++ object layouts
std::vector ABI
Rust references
allocator ownership
exceptions/panics
thread assumptions
```

across languages.

This ADR defines the boundary rules now without requiring Rust today.

---

## 2. Decision

ERDFlow will use:

> **A narrow, coarse-grained, framework-independent language boundary for any future Rust modules.**

The boundary must exchange:

```text
stable IDs
versioned structured input
versioned structured output
explicit error results
explicit ownership
```

and must never expose Qt objects or internal C++ class layouts.

For future in-process Rust integration, ERDFlow's compatibility target is:

> **a C-ABI-compatible boundary or an adapter that can be reduced to equivalent C-compatible semantics.**

The exact binding technology is intentionally deferred.

---

## 3. Core Rule

Good future boundary:

```text
ConceptualSnapshot
      ↓
Rust Conversion Module
      ↓
RelationalConversionResult
```

Bad future boundary:

```text
QGraphicsItem*
std::vector<Entity*>*
QObject*
      ↓
Rust
```

The Rust side receives domain-relevant data, not desktop implementation objects.

---

## 4. Rust Is Still Optional

This ADR does not introduce Rust into the repository.

No immediate requirement for:

```text
Cargo
rustc
FFI libraries
Rust source tree
```

exists.

Rust is added only when ADR-004's evidence threshold is satisfied.

---

## 5. Why Define the Boundary Before Rust Exists

The best way to preserve future language flexibility is to keep today's core interfaces:

```text
framework-independent
coarse-grained
value-oriented
explicit
```

This improves C++ architecture even if Rust is never introduced.

---

## 6. Primary Boundary Shape

Preferred conceptual form:

```text
C++ Application/Core
        │
        │ structured request
        ▼
Rust Module Boundary
        │
        │ structured result
        ▼
C++ Application/Core
```

The Qt Presentation layer does not call Rust directly for normal Domain behavior.

---

## 7. Application/Core Owns Integration

The normal dependency path remains:

```text
Qt Presentation
      ↓
Application
      ↓
Core Port
      ↓
Rust Adapter
```

Rust is an implementation behind an explicit boundary.

---

## 8. No Qt Types Across the Boundary

Forbidden across Rust FFI:

```text
QString
QByteArray
QVariant
QObject*
QGraphicsItem*
QWidget*
QUuid
QModelIndex
```

Convert these at the C++ adapter edge.

---

## 9. No C++ STL ABI Across the Boundary

Do not expose:

```text
std::string
std::vector
std::optional
std::variant
std::shared_ptr
```

directly as a public binary ABI between C++ and Rust.

Their ABI is not the interoperability contract.

---

## 10. No Rust-Specific ABI Assumption

Do not expose Rust-native layout such as:

```text
Vec<T>
String
Option<T>
Result<T, E>
&T
Box<T>
```

directly to C++ as if their memory layout were a stable language-neutral ABI.

---

## 11. C-Compatible Semantics

If Rust runs in-process, the boundary should use C-compatible concepts such as:

```text
fixed-width integers
byte spans
explicit lengths
opaque handles
plain tagged enums
plain result codes
explicit allocation/free functions
```

Exact header-generation tooling is deferred.

---

## 12. Coarse-Grained Calls

Preferred:

```text
convert_conceptual_snapshot(...)
```

Less desirable:

```text
get_entity_name(...)
get_entity_attribute_count(...)
get_attribute_name(...)
get_attribute_type(...)
...
```

for thousands of tiny calls.

Coarse-grained calls reduce:

- FFI overhead,
- ownership complexity,
- API fragility,
- cross-language chatter.

---

## 13. Snapshot-Based Input

Rust modules should preferably receive:

```text
immutable snapshot
```

or equivalent isolated input.

This aligns with ADR-007.

Example:

```text
Conceptual revision 125
      ↓
serialized/structured snapshot
      ↓
Rust validator/converter
```

Rust should not receive mutable access to the live Project.

---

## 14. Structured Result

Rust should return a complete structured result such as:

```text
ValidationResult
ConversionResult
DiffResult
ImportParseResult
```

rather than mutating C++ memory incrementally through callbacks.

---

## 15. Stable IDs

ADR-001 IDs remain authoritative across languages.

A UUID may cross the boundary as:

```text
16 raw bytes
```

or:

```text
canonical UUID text
```

depending on the chosen contract.

Do not replace ERDFlow identity with Rust object addresses.

---

## 16. Strong Typing on Each Side

Across the binary boundary, IDs may need a common representation.

Inside each language, restore strong typing:

```text
C++:
EntityId
RelationId

Rust:
EntityId
RelationId
```

Do not collapse everything into an untyped string inside the implementation.

---

## 17. UTF-8

Text crossing the Rust boundary should use:

```text
UTF-8
```

as the default encoding.

Length must be explicit.

Do not rely on:

```text
null termination
locale-specific encoding
platform-native wchar_t
```

unless a specific API requires it.

---

## 18. Ownership Rule

The side that allocates memory owns the corresponding deallocation mechanism.

Example:

```text
Rust allocates result buffer
→ Rust-provided free function releases it
```

or:

```text
C++ allocates caller buffer
→ Rust writes only within explicit capacity
```

Do not allocate in one runtime and free through an unrelated allocator on the other side.

---

## 19. No Shared Mutable Ownership

Avoid designs where:

```text
C++
and
Rust
```

both believe they own and can mutate the same object graph.

Preferred:

```text
input ownership clear
result ownership clear
transfer rules explicit
```

---

## 20. Opaque Handles

If a future module genuinely requires persistent native state, opaque handles may be used.

Conceptually:

```text
ErdflowRustEngineHandle*
```

C++ treats the handle as opaque.

Only Rust API functions operate on the underlying Rust object.

---

## 21. Handles Are an Exception

Prefer stateless/coarse functions for deterministic transformations where practical.

Use long-lived handles only when they provide a concrete benefit such as:

```text
parser session
database engine adapter
large cached index
```

---

## 22. Panic Boundary

A Rust panic must never unwind across the C ABI into C++.

Rust FFI entry points must define and test their panic behavior. With an
unwinding panic strategy, catch Rust panics on the Rust side before returning
across the boundary and translate recoverable failures into:

```text
structured fatal/internal error
```

Do not assume every native failure can become a result: `catch_unwind` does
not catch aborting panics, and `panic=abort` terminates the process. Native
crashes and allocation aborts are not made recoverable by this boundary.
If host-process survival is required for such failures, evaluate process
isolation. See the [Rust FFI unwinding guidance](https://doc.rust-lang.org/nomicon/ffi.html#ffi-and-unwinding).

---

## 23. C++ Exception Boundary

Likewise, C++ exceptions must not cross into Rust through an FFI boundary.

The adapter catches/translates them before the boundary where needed.

---

## 24. Structured Errors

Expected failures return structured errors.

Conceptually:

```text
ErrorCode
+
optional structured details
+
human-readable diagnostic
```

Examples:

```text
InvalidInput
UnsupportedFeature
Cancelled
StaleInput
ParseFailure
InternalFailure
```

Exact codes are module-specific.

---

## 25. Error Text Is Not the API

Do not make logic depend on:

```text
"Error: entity missing"
```

string matching.

Use stable machine-readable error codes.

Text is for diagnostics/presentation.

---

## 26. Validation Issues

Validation-specific results may return arrays of structured:

```text
IssueCode
Severity
ElementReference
BlockingScope
Metadata
```

matching ADR-009 semantics.

---

## 27. Conversion Results

A future Rust converter returns something conceptually equivalent to:

```text
CandidateModel
Mappings
AppliedRules
ValidationIssues
UnresolvedDecisions
```

It does not directly write into the live C++ Project.

---

## 28. Import Results

A future Rust parser may return:

```text
parsed external facts
source locations
unsupported constructs
syntax issues
```

C++ Application still owns:

```text
review
interpretation policy
application command
```

unless a larger core module is intentionally migrated.

---

## 29. Threading

A Rust function must not assume it can mutate Qt UI state.

The task executes under ADR-007.

Conceptually:

```text
C++ TaskRunner
      ↓
Rust computation
      ↓
Result
      ↓
C++ Application
      ↓
Qt update
```

---

## 30. Thread Affinity

Qt GUI thread rules remain authoritative for Presentation.

Rust code should not retain:

```text
QObject*
QWidget*
```

for later worker-thread callbacks.

---

## 31. Callbacks

Avoid frequent cross-language callbacks where a result can be returned in one structured response.

Callbacks may be justified for:

```text
cancellation
progress
streaming
```

but should have strict lifetime/thread rules.

---

## 32. Cancellation

A future Rust operation should support cooperative cancellation when the task is long-running.

Possible boundary representations include:

```text
atomic cancellation flag
opaque cancellation token
callback
```

Exact mechanism is deferred.

An atomic flag is conceptual, not permission to share the memory layout of
`std::atomic` with a Rust atomic type. Define a compatible accessor/opaque
token or callback contract with explicit lifetime, thread, and memory-order
rules when selecting the bridge.

---

## 33. Progress

Progress callbacks should be:

```text
optional
bounded/throttled
structured
```

and must not call Qt UI directly from arbitrary Rust worker threads.

---

## 34. Determinism

Moving a module from C++ to Rust must not change intended semantic behavior merely because the implementation language changed.

For equivalent:

```text
input
options
rule version
```

the semantic result should remain equivalent.

---

## 35. Parity Testing

Before replacing an existing C++ module:

```text
C++ implementation
vs
Rust implementation
```

must be tested against common fixtures.

Compare:

```text
semantic output
IDs/lineage rules
validation codes
error cases
performance
memory
```

---

## 36. Differential Testing

For deterministic modules, ERDFlow should use differential tests where practical.

Example:

```text
same ConceptualModel
      ↓
C++ converter
      ↓
Result A

same ConceptualModel
      ↓
Rust converter
      ↓
Result B

Assert semantic equivalence
```

---

## 37. Migration Strategy

A module migration follows:

```text
1. Existing C++ behavior is tested
2. Boundary contract is defined
3. Rust implementation is added
4. Parity tests run
5. Performance/safety benefit measured
6. Rust adapter becomes selected implementation
7. Old C++ implementation removed only when confidence is sufficient
```

---

## 38. No Big-Bang Rewrite

ERDFlow will not migrate:

```text
Domain
Application
Persistence
Import
Conversion
DataSource
UI
```

all at once.

Migrate one justified subsystem at a time.

---

## 39. Candidate Modules

Strong future candidates may include:

```text
SQL parser
JSON/CSV hardened parser
validation engine
conversion engine
three-way diff engine
large graph analysis
```

This is not a commitment to migrate them.

---

## 40. Weak Rust Candidates

The following are weaker first candidates:

```text
Qt widgets
QMainWindow shell
Properties panel
QTableView model adapter
desktop menu wiring
```

These are naturally integrated with the existing C++/Qt layer.

---

## 41. UI Remains C++/Qt

The default long-term UI architecture remains:

```text
Qt 6
+
C++
```

Rust does not need to render ERDFlow's desktop UI.

---

## 42. C++ Application Layer May Remain

Even if substantial core algorithms move to Rust, the C++ Application layer may remain responsible for:

```text
Qt-facing coordination
commands
task scheduling
project-session behavior
```

unless evidence later supports a larger migration.

---

## 43. Possible Long-Term Shape

Example:

```text
Qt/C++ Presentation
        ↓
C++ Application
        ↓
Core Ports
        ↓
Rust Modules
├── Validation
├── Conversion
└── Import Parsing
```

This is one possible outcome, not a required final architecture.

---

## 44. Alternative Long-Term Shape

A larger future reusable core could become:

```text
Rust Core
├── Domain DTOs
├── Validation
├── Conversion
└── Diff
```

used by:

```text
Qt Desktop
VS Code
CLI
Service
```

But this requires a future explicit architecture decision.

---

## 45. Boundary DTOs

Cross-language data should use dedicated transfer representations.

Do not expose internal class layouts directly.

Conceptually:

```text
ConceptualSnapshotDto
RelationalResultDto
ValidationIssueDto
```

The DTO shape is a contract.

Internal C++ and Rust structures may differ.

---

## 46. DTO Versioning

If a boundary becomes stable across independently versioned components, DTOs need explicit versioning.

Early internal builds may evolve them freely.

Do not promise public ABI stability prematurely.

---

## 47. ABI Version

A future native Rust library should expose a small ABI/version query.

Conceptually:

```text
erdflow_core_api_version()
```

This helps detect incompatible binaries.

Exact mechanism is deferred.

---

## 48. API Compatibility

If desktop and Rust library ship together, strict long-term backward ABI compatibility may not initially be necessary.

However, incompatibility should fail clearly rather than crash mysteriously.

---

## 49. Dynamic vs Static Linking

This ADR does not decide whether Rust is integrated as:

```text
static library
dynamic library
```

The decision depends on packaging and module needs.

Both must obey the same boundary rules.

---

## 50. Build Integration

If Rust is introduced:

```text
Cargo
```

becomes responsible for Rust compilation.

CMake remains responsible for the main C++/Qt build.

Integration must be explicit and reproducible.

---

## 51. Build Reproducibility

CI should pin/support an intentional Rust toolchain policy when Rust arrives.

Do not rely on:

```text
whatever Rust version happens to be installed
```

for production builds.

---

## 52. Toolchain Independence

Until Rust exists:

```text
C++ developers should not need Rust installed
```

just to build ERDFlow.

After a Rust module becomes mandatory, the repository/build documentation must state that requirement clearly.

---

## 53. Generated Bindings

Potential tools may later include:

```text
cbindgen
CXX
bindgen
custom C headers
```

No tool is selected now.

The selection must support the chosen ownership/error/version contract.

---

## 54. Why C-ABI-Compatible Semantics

C ABI provides a conservative interoperability baseline understood by:

```text
C++
Rust
other native languages
```

It avoids relying on unstable C++ class ABI or Rust-native layout.

This does not mean every call must literally be handwritten raw C.

An adapter generator may provide ergonomic wrappers.

---

## 55. Process Boundary Alternative

Some future capabilities may be better isolated out-of-process.

Example:

```text
AI service
heavy external analyzer
sandboxed parser
remote service
```

Then the boundary may use:

```text
IPC
RPC
serialized messages
```

rather than in-process FFI.

---

## 56. In-Process vs Out-of-Process

Choose based on needs.

### In-process

Good for:

```text
low latency
core algorithms
tight desktop packaging
```

### Out-of-process

Good for:

```text
fault isolation
independent deployment
security sandboxing
service reuse
```

This ADR allows both while preserving the same structured-contract philosophy.

---

## 57. Serialization Format

For large/complex boundary DTOs, ERDFlow may use a serialized representation.

Candidates could include:

```text
custom binary DTO
MessagePack-like format
Protocol Buffers
FlatBuffers
JSON for debugging/simple cases
```

No format is selected here.

---

## 58. Do Not Use JSON Automatically for Hot Paths

JSON is easy to inspect but may create unnecessary:

```text
parsing
allocation
size
```

for high-frequency large core operations.

Measure before selecting boundary encoding.

---

## 59. Do Not Optimize Before Measurement

Conversely, do not introduce a complex binary protocol before the workload exists.

The initial Rust module should use the simplest boundary that satisfies correctness and measured performance.

---

## 60. Zero-Copy

Zero-copy designs may be attractive later.

They also increase:

```text
lifetime complexity
alignment constraints
ownership coupling
ABI fragility
```

Therefore zero-copy is not a default requirement.

Correct ownership comes first.

---

## 61. Large Models

For large snapshots, avoid one FFI call per object.

Prefer:

```text
contiguous encoded snapshot
```

or another batch representation.

---

## 62. Memory Limits

Rust integration does not eliminate memory-budget concerns.

A conversion may still hold:

```text
C++ live Project
+
boundary snapshot
+
Rust decoded model
+
candidate result
+
C++ reconstructed candidate
```

Potential duplicate memory must be measured.

---

## 63. Conversion Copy Cost

When evaluating Rust migration, include:

```text
marshalling cost
copy cost
allocation cost
decode/encode cost
```

not only algorithm runtime.

A faster inner algorithm can still produce a slower end-to-end operation if the boundary is poorly designed.

---

## 64. Benchmark End-to-End

Performance comparisons should measure:

```text
C++ call start
→ boundary
→ Rust work
→ boundary
→ usable C++ result
```

not only Rust's internal function time.

---

## 65. Security

Rust can reduce classes of memory-safety defects inside Rust code.

It does not automatically make the entire import/conversion pipeline secure.

Still required:

```text
input limits
validation
resource limits
logic correctness
safe FFI
dependency review
```

---

## 66. Unsafe Rust

FFI often requires some:

```text
unsafe
```

Rust code.

Keep unsafe code concentrated near the boundary where practical.

Do not let unsafe pointer manipulation spread through the Rust core.

---

## 67. Dependency Review

Rust crates introduced into ERDFlow should be reviewed for:

```text
license
maintenance
security posture
dependency tree
platform support
```

as with C++ dependencies.

---

## 68. Supply-Chain Discipline

Do not add dozens of crates merely because they are convenient.

Core native dependencies affect:

```text
build reliability
security
binary size
maintenance
```

---

## 69. Licensing

Rust itself does not remove licensing obligations.

Each dependency/crate must be compatible with ERDFlow's eventual distribution model.

---

## 70. Logging

Rust modules may emit structured diagnostics.

Avoid requiring Rust to know about Qt logging UI.

C++ adapter may route messages into ERDFlow's logging infrastructure.

---

## 71. Sensitive Data

Do not log:

```text
database passwords
tokens
full sensitive datasets
```

from Rust or C++ by default.

---

## 72. Tracing

Future performance work may use cross-boundary timing/tracing IDs.

This is optional.

The boundary should permit correlation without exposing internal pointers.

---

## 73. Testing Without Qt

Rust core tests should run without Qt.

Likewise, C++ Domain/Application tests around the boundary should be able to use:

```text
fake adapter
```

or reference implementation.

---

## 74. Fallback Implementation

During migration, ERDFlow may temporarily support:

```text
C++ implementation
Rust implementation
```

behind the same port.

This enables:

```text
parity tests
benchmarking
safe rollback
```

---

## 75. Runtime Switching

End-user runtime switching between implementations is not required.

A development/test build flag may be sufficient.

---

## 76. Feature Flags

Rust internal feature flags should not leak into ERDFlow product semantics.

The Application sees:

```text
capability
result
```

not Cargo-specific concepts.

---

## 77. Failure Isolation

If a Rust in-process module crashes through a fatal native error, it can still terminate the application.

Rust is not equivalent to process isolation.

For untrusted high-risk functionality, an out-of-process architecture may be safer.

---

## 78. Parser Sandboxing

A future parser handling hostile/untrusted files could be considered for process isolation if threat modeling justifies it.

This is not required for initial import support.

---

## 79. DataSource Boundary

If a future DataSource implementation uses Rust, the same rules apply:

```text
structured queries
structured rows
explicit ownership
no Qt pointers
```

Qt's `QAbstractItemModel` remains C++ Presentation.

---

## 80. Persistence Boundary

If `.erdx` parsing/serialization later moves to Rust:

```text
versioned ErdxDocument
```

is the contract.

Rust must not depend on C++ class memory layout.

ADR-005 already supports this.

---

## 81. Mapping Boundary

Cross-level mappings from ADR-008 should cross as:

```text
typed element references
mapping IDs
rule codes
origin metadata
```

not C++ object addresses.

---

## 82. Regeneration Boundary

A future Rust regeneration engine could accept:

```text
Baseline
Current Target
New Candidate
Mappings
```

and return:

```text
ChangeProposal
```

without directly modifying live project state.

---

## 83. Command Boundary

Normal UI commands do not need to cross into Rust individually unless a future core architecture intentionally moves command handling.

Do not create an FFI call for every mouse/UI action without need.

---

## 84. UI Events

Mouse movement, hover, drag frames, and Qt events should remain in C++ Presentation.

Rust is not intended as a per-frame UI event processor.

---

## 85. Auto-Layout

Auto-layout could become a Rust candidate if algorithmic scale justifies it.

Boundary:

```text
LayoutGraphSnapshot
→
LayoutProposal
```

is clean and coarse-grained.

---

## 86. AI

Future AI provider integration is not automatically a Rust responsibility.

AI remains behind its own provider-agnostic interface.

If Rust is used internally later, it still cannot bypass:

```text
validation
review
commands
```

---

## 87. VS Code Extension

A future VS Code extension may interact with a Rust core through:

```text
native addon
CLI/service
WebAssembly
RPC
```

depending on deployment constraints.

No mechanism is selected today.

The important shared asset is the structured core contract.

---

## 88. WebAssembly

Rust may compile well to WebAssembly.

That could eventually help reuse selected deterministic core logic in web/VS Code contexts.

However:

```text
WASM compatibility
```

is not a current requirement.

Do not distort the native architecture solely for hypothetical WASM use.

---

## 89. CLI

A future CLI may be a strong early consumer of a reusable Rust or C++ core.

Example:

```text
erdflow validate project.erdx
erdflow generate schema
```

This can help prove core independence from Qt.

---

## 90. Version Skew

If C++ desktop and Rust module can ever be updated separately, compatibility negotiation becomes necessary.

Possible:

```text
API version
feature flags/capabilities
```

This is deferred until independent versioning exists.

---

## 91. Same-Release Packaging

Initial Rust integration should preferably ship:

```text
desktop executable
+
matching Rust library
```

from the same release pipeline.

This minimizes version-skew complexity.

---

## 92. Diagnostics

On startup or module load failure, ERDFlow should produce a clear diagnostic such as:

```text
Rust core library incompatible
```

rather than crashing due to missing symbols.

Exact loader behavior depends on static/dynamic linking.

---

## 93. ABI Surface Size

Keep the native ABI small.

A smaller surface is:

```text
easier to test
easier to version
easier to audit
less fragile
```

---

## 94. Internal Rust API May Be Rich

The Rust implementation may have a rich internal module structure.

Only the interop façade needs to remain small.

---

## 95. Internal C++ API May Be Rich

Likewise, C++ adapters can convert between ergonomic C++ Domain types and compact boundary DTOs.

Do not force C-compatible design throughout the whole C++ codebase.

---

## 96. Boundary Adapter

Conceptually:

```text
C++ Domain/Application
      ↓
RustCoreAdapter
      ↓
C-compatible interop layer
      ↓
Rust implementation
```

The adapter owns conversion between:

```text
C++ types
and
boundary DTOs
```

---

## 97. No Rust Types in Presentation

Qt Presentation should not include Rust-generated raw FFI headers broadly.

Contain them inside Infrastructure/core adapter targets where possible.

---

## 98. Build Target Isolation

If Rust arrives, create a dedicated target/module boundary rather than scattering FFI calls across the codebase.

Conceptually:

```text
erdflow_rust_adapter
```

Exact target name is deferred.

---

## 99. Repository Layout

Possible future layout:

```text
rust/
└── core/
    ├── Cargo.toml
    └── src/

infrastructure/
└── rust_adapter/
```

Not required now.

Do not create empty Rust directories prematurely.

---

## 100. CMake/Cargo Integration

Possible approaches include:

```text
CMake invokes Cargo
Cargo artifact imported into CMake
external integration helper
```

No mechanism is selected.

Build simplicity is part of future evaluation.

---

## 101. Cross-Platform Requirement

Any mandatory Rust module must support:

```text
macOS
Linux
Windows
```

for the same ERDFlow release targets.

Do not adopt a mandatory Rust dependency that blocks one supported desktop platform without an explicit product decision.

---

## 102. Architecture Support

CI must verify the relevant architectures.

Examples may include:

```text
macOS arm64
Windows x64
Linux x64
```

Additional architectures depend on release policy.

---

## 103. Static CRT / Runtime Issues

Native packaging details such as:

```text
MSVC runtime
Rust target
linkage
```

must be tested per platform when Rust becomes real.

Not decided now.

---

## 104. Performance Acceptance

Rust migration is accepted for performance only if end-to-end benchmarks show meaningful benefit relative to added complexity.

Do not rely on language reputation.

---

## 105. Safety Acceptance

Rust migration may also be justified even without speedup if it significantly reduces risk in a high-risk subsystem such as:

```text
untrusted parser
complex ownership-heavy engine
```

The benefit must still be explicit.

---

## 106. Reuse Acceptance

Rust may be justified because one core module needs to serve:

```text
desktop
CLI
VS Code
service
```

with a single reusable implementation.

Again, this should be based on a real product need.

---

## 107. Migration Decision Record

Each real Rust introduction should get its own focused ADR.

Example:

```text
ADR-0XX — Move SQL Parser to Rust
```

That ADR should include:

```text
problem
evidence
boundary
benchmark
migration plan
rollback
```

ADR-013 is only the general boundary policy.

---

## 108. Alternative A — Expose C++ Classes Directly

Rejected.

Reason:

- ABI fragility,
- allocator/lifetime complexity,
- Qt leakage risk,
- difficult Rust bindings.

---

## 109. Alternative B — Use Qt Types in FFI

Rejected.

Rust core should not depend on Qt's object model.

---

## 110. Alternative C — Per-Object Fine-Grained FFI

Rejected as the default.

Too chatty and fragile for large models.

---

## 111. Alternative D — Shared Mutable Object Graph

Rejected.

Ownership and threading become difficult to reason about.

---

## 112. Alternative E — Rewrite Entire Core in Rust at Once

Rejected.

Contradicts ADR-004 and creates unnecessary migration risk.

---

## 113. Alternative F — Never Permit Rust

Rejected.

A clean architecture should preserve the option to use the best implementation technology where evidence supports it.

---

## 114. Alternative G — Pick FFI Library Today

Rejected.

No Rust module exists yet, so choosing:

```text
CXX
cbindgen
bindgen
UniFFI
```

today would be tool-driven architecture.

Boundary semantics matter first.

---

## 115. Consequences — Positive

This decision gives ERDFlow:

- future Rust flexibility,
- protection from Qt/C++ ABI leakage,
- explicit ownership,
- safer errors,
- module-by-module migration,
- easier testing,
- possible reuse across future frontends,
- better long-term language independence.

---

## 116. Consequences — Costs

When Rust is eventually introduced, ERDFlow will pay for:

```text
DTO conversion
FFI code
Cargo integration
cross-language testing
additional CI
additional debugging complexity
possible memory copies
```

These costs are accepted only when the Rust module provides sufficient benefit.

---

## 117. Initial Action

Current implementation action is deliberately:

```text
none
```

Do not add Rust yet.

Instead, maintain the current C++ architecture so that future module boundaries remain clean.

---

## 118. First Rust Evaluation Checklist

Before implementing the first Rust module, answer:

```text
1. Which exact module?
2. What measurable problem exists?
3. Why is Rust the best solution?
4. What is the coarse-grained API?
5. What data crosses the boundary?
6. Who owns every allocation?
7. How are errors represented?
8. How are panics contained?
9. How is cancellation handled?
10. How is version compatibility checked?
11. What is the end-to-end performance cost?
12. How is semantic parity tested?
13. How is the module packaged on macOS/Linux/Windows?
14. What is the rollback plan?
```

If these cannot be answered, Rust introduction is premature.

---

## 119. First Rust Boundary Tests

When Rust is introduced, minimum tests include:

### Test 1 — Valid Request

```text
C++ input
→ Rust
→ expected structured result
```

### Test 2 — Invalid Input

```text
structured error
→ no crash
```

### Test 3 — Panic Containment

Unexpected Rust panic does not unwind through C++.

### Test 4 — Memory Ownership

Repeated calls show no cross-boundary leaks/double frees.

### Test 5 — Unicode

UTF-8 names survive round trip.

### Test 6 — UUID

Stable IDs survive bit-for-bit/canonical round trip.

### Test 7 — Large Model

Boundary handles large snapshots within measured memory/performance budget.

### Test 8 — Parity

Rust output matches the reference C++ semantics.

---

## 120. Architecture Invariants

### Invariant 1

Rust remains optional until justified by evidence.

### Invariant 2

Qt objects never cross the Rust core boundary.

### Invariant 3

C++ internal class layouts are not the FFI contract.

### Invariant 4

Rust-native object layouts are not exposed as the FFI contract.

### Invariant 5

Interop is coarse-grained.

### Invariant 6

Boundary data is structured and versionable.

### Invariant 7

Stable ERDFlow IDs survive cross-language calls.

### Invariant 8

Memory ownership is explicit.

### Invariant 9

Panics and C++ exceptions do not unwind across the boundary.

### Invariant 10

Background Rust computation does not mutate Qt UI or the live Project directly.

### Invariant 11

Rust replacements require parity testing.

### Invariant 12

Performance evaluation includes marshalling and copy costs.

### Invariant 13

A future Rust module must preserve macOS/Linux/Windows support unless explicitly decided otherwise.

### Invariant 14

The exact binding generator/library is deferred until a concrete module exists.

---

## 121. Relationship to Other ADRs

Depends on:

```text
ADR-001 — Stable Identity Strategy
ADR-002 — Layered Architecture and Dependency Direction
ADR-003 — Desktop UI Technology
ADR-004 — Core Language Strategy
ADR-005 — Project File and Persistence Boundary
ADR-007 — Background Work / Concurrency Strategy
ADR-008 — Cross-Level Mapping and Provenance
ADR-009 — Validation Gates and Conversion Policy
ADR-010 — Change Propagation and Regeneration
ADR-011 — Import Architecture
ADR-012 — Data Source Architecture
```

ADR-013 completes the initial architecture-decision sequence defined in the
Phase 0 roadmap.

---

## 122. Open Implementation Decisions

This ADR intentionally leaves open:

- first Rust module,
- exact FFI library,
- exact boundary serialization format,
- static vs dynamic linking,
- exact Cargo/CMake integration,
- exact ABI version scheme,
- exact cancellation primitive,
- exact progress-callback mechanism,
- exact DTO code generation,
- exact process-vs-in-process choice for any future subsystem,
- exact VS Code/Rust integration strategy,
- exact WebAssembly strategy.

These decisions require real implementation evidence.

---

## 123. Decision Outcome

ERDFlow adopts the following future Rust policy:

```text
C++20/Qt today
      ↓
clean core port
      ↓
coarse-grained structured boundary
      ↓
optional Rust module
      ↓
structured result
```

with:

```text
no Qt pointers
no C++ STL ABI
no Rust-native ABI exposure
explicit ownership
structured errors
panic containment
stable IDs
parity testing
```

---

## 124. Final Principle

> Rust may replace an implementation module, but it must never become a reason to weaken ERDFlow's architectural boundaries.

The architecture decides the contract.

The language implements it.

---

## 125. Review Record — 2026-09-14

Outcome: accepted. Confirmed optional coarse-grained Rust integration; corrected panic-recovery guarantees and clarified cancellation ABI requirements. No Rust implementation or bridge is selected.

See [Phase 0 review](../PHASE_0_REVIEW.md) for cross-document findings,
quality requirements, and deferred implementation gates. Acceptance records
the architecture contract, not completion of its implementation or tests.
