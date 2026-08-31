# ADR-009 — Validation Gates and Conversion Policy

**Status:** Proposed  
**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** Validation severity, conversion readiness, blocking vs non-blocking issues, unresolved decisions, and when deterministic transformations may proceed

---

## 1. Context

ERDFlow supports progressive refinement:

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

These transformations should be deterministic where database rules are known.

However, not every project state is equally ready for conversion.

Examples:

```text
Entity has no identifier
Relationship cardinality is incomplete
Specialization mapping strategy is not chosen
Logical type is missing
A reference points to a deleted object
A warning exists but does not make conversion impossible
```

ERDFlow therefore needs a clear policy for:

```text
what is invalid
what blocks conversion
what merely warns
what requires a user decision
what may proceed
```

Without this policy, a button such as:

```text
Convert Anyway
```

could become dangerously ambiguous.

---

## 2. Decision

ERDFlow will use:

> **Structured validation with explicit conversion gates.**

Validation issues are classified by meaning, not merely by UI color.

The initial policy distinguishes:

```text
Domain Invariant Violation
Blocking Conversion Error
Non-Blocking Warning
Advisory / Information
Unresolved Conversion Decision
```

Conversion follows:

```text
Validate
   ↓
Classify Issues
   ↓
Resolve / Review
   ↓
Transform
   ↓
Return Structured Result
```

---

## 3. Core Rule

A deterministic conversion may proceed only when:

```text
all required Domain invariants hold
```

and:

```text
no unresolved blocking condition prevents a valid deterministic result
```

Warnings may allow conversion.

Hard invalidity may not be bypassed by a generic "Convert Anyway" action.

---

## 4. Why This Matters

ERDFlow must distinguish:

```text
"the model is incomplete"
```

from:

```text
"the model is impossible to interpret safely"
```

and from:

```text
"the model is valid but may have a design concern"
```

These are different conditions and need different behavior.

---

## 5. Domain Invariant Violation

A Domain invariant violation means the Project or model is structurally invalid in a way that ERDFlow itself should not normally permit.

Examples:

```text
RelationshipParticipant references missing EntityId
Duplicate stable ID where uniqueness is required
Mapping references missing source/target
Impossible internal cardinality representation
Broken ownership relationship
```

These are not ordinary modeling warnings.

They indicate:

```text
invalid internal/domain state
```

---

## 6. Invariant Violations Are Blocking

If a required Domain invariant fails:

```text
conversion must not proceed
```

because the source model cannot be trusted as a valid semantic input.

Preferred behavior:

```text
detect
report
prevent transformation
```

---

## 7. Blocking Conversion Error

A Blocking Conversion Error means:

> The model may be representable as work-in-progress, but a deterministic target cannot be produced safely yet.

Example:

```text
Specialization exists
but no required relational mapping strategy has been selected
```

If multiple valid transformations exist and ERDFlow cannot deterministically choose one:

```text
conversion is blocked
```

until the decision is resolved.

---

## 8. Blocking Error Examples

Potential examples:

```text
Entity requiring an identifier for the selected conversion strategy has none
Relationship cardinality is missing where target placement depends on it
Weak Entity lacks required owner information
Partial key information is missing
Specialization strategy required but unresolved
Required logical type is missing for selected downstream step
Foreign-key source/target reference is incomplete
```

The exact rule set evolves with feature implementation.

---

## 9. Non-Blocking Warning

A Non-Blocking Warning means:

> ERDFlow can still produce a valid deterministic target, but the design deserves attention.

Examples may include:

```text
very long generated identifier name
nullable attribute that may be unexpected
possible redundancy
unusual but valid cardinality pattern
derived attribute not materialized downstream
```

Warnings should not silently stop conversion.

---

## 10. Warning Policy

With only warnings:

```text
conversion may proceed
```

The UI should:

```text
show warnings
allow review
allow user to continue
```

No special "ignore all validation" mode is needed.

---

## 11. Advisory / Information

Information means:

```text
useful explanation
```

without indicating an error.

Examples:

```text
M:M relationship will become a junction relation
Multivalued attribute will become a separate relation
Derived attribute will not be stored by default
Schema target name was normalized
```

Information does not block conversion.

---

## 12. Unresolved Conversion Decision

An Unresolved Conversion Decision means:

> The source model is valid, but several legitimate deterministic target strategies exist and ERDFlow needs an explicit choice.

Example:

```text
ISA hierarchy
```

may support several relational strategies.

Possible choices:

```text
Single Table
Class Table / Joined
Concrete Table
```

If no default policy has been selected:

```text
conversion waits for user/project decision
```

---

## 13. Decision vs Error

An unresolved decision is not necessarily an error in the Conceptual model.

The model may be semantically valid.

The problem is:

```text
target transformation is underdetermined
```

This distinction matters.

---

## 14. Structured Validation Issue

Conceptually:

```text
ValidationIssue
├── IssueCode
├── Severity
├── MessageKey / Explanation
├── ElementReference?
├── RuleId?
├── SuggestedResolution?
└── BlockingScope?
```

The exact C++ structure is deferred.

---

## 15. Severity Is Not Enough Alone

A simple:

```text
Error / Warning / Info
```

enum may be insufficient.

ERDFlow also needs to know:

```text
does this block save?
does this block Conceptual → Relational?
does this block Relational → Physical?
does this block SQL generation?
```

Therefore blocking scope/policy may be separate from presentation severity.

---

## 16. Save Validation Is Different

ERDFlow is a design tool.

Users must be able to save work-in-progress.

Example:

```text
Conceptual model missing final identifier
```

may be:

```text
not conversion-ready
```

but still:

```text
valid to save
```

Therefore:

```text
Save Gate
≠
Conversion Gate
```

---

## 17. Save Blocking

Saving should block only when ERDFlow cannot safely represent/persist the Project.

Examples:

```text
broken stable ID structure
impossible internal references
serialization-critical invalid state
```

Ordinary incomplete modeling should not block save.

---

## 18. Conceptual → Relational Gate

Before Conceptual → Relational conversion:

```text
run relevant Conceptual validation
```

Then:

```text
Domain invariant violation
→ stop

Blocking conversion error
→ stop

Unresolved required decision
→ resolve or stop

Warnings only
→ allow conversion

Information only
→ allow conversion
```

---

## 19. Relational → Physical Gate

The same general policy applies.

Before Relational → Physical conversion:

```text
validate relevant relational state
```

Examples that may block:

```text
invalid key reference
missing type information required for physical mapping
unresolved dialect-sensitive decision
```

Warnings may still allow conversion.

---

## 20. Physical → SQL Gate

Before SQL generation:

```text
validate physical model for selected dialect
```

Possible blocking issues:

```text
unsupported physical type
invalid identifier for selected dialect after resolution rules
broken constraint reference
missing required dialect choice
```

---

## 21. Import Validation Is Separate

Import uses validation too, but import has its own stages:

```text
syntax validation
structural validation
candidate-model validation
```

Import errors do not automatically use the same exact policy as conversion.

ADR-011 will refine import-specific behavior.

---

## 22. Conversion Result

A conversion should return structured output.

Conceptually:

```text
RelationalConversionResult
├── CandidateSchema
├── Mappings
├── AppliedRules
├── ValidationIssues
└── UnresolvedDecisions
```

If blocked:

```text
CandidateSchema
```

may be absent or explicitly incomplete depending on the operation.

The API must make that state explicit.

---

## 23. No Silent Partial Target by Default

If a blocking issue prevents deterministic conversion of required structure:

```text
do not silently omit that structure
```

Example:

```text
unknown relationship semantics
```

must not quietly disappear from the generated Schema.

---

## 24. Explicit Partial Preview

A future conversion preview may support:

```text
partial candidate
+
clearly marked unresolved elements
```

for educational/review purposes.

But it must be explicit.

It must not masquerade as a valid completed conversion.

---

## 25. "Convert Anyway" Policy

ERDFlow should avoid a generic button whose meaning is:

```text
ignore every validation failure
```

If the UI uses wording like:

```text
Continue
Convert with Warnings
Proceed
```

it applies only when:

```text
no hard blocker remains
```

---

## 26. Hard Invalidity Cannot Be Waived

Examples:

```text
dangling Entity reference
missing required target identity
broken internal mapping
```

cannot be waived by user preference.

The software cannot safely interpret them.

---

## 27. User-Resolvable Decisions

Some blockers can be resolved through explicit user choice.

Example:

```text
Choose ISA mapping strategy
```

Flow:

```text
Unresolved Decision
      ↓
Explain Options
      ↓
User Selects
      ↓
Validation Re-run
      ↓
Conversion
```

---

## 28. Project-Level Defaults

Some decisions may later be stored as project settings.

Example:

```text
Default specialization strategy
```

Then future conversions can be deterministic without repeated prompts.

The exact settings are deferred.

---

## 29. Conversion Options

Conversion may accept explicit options.

Conceptually:

```text
ConversionOptions
├── specializationStrategy
├── namingPolicy
├── other rule-specific choices
```

Options must be separate from hidden UI assumptions.

---

## 30. Determinism

For:

```text
same valid source
+
same conversion options
+
same rule version
```

ERDFlow should produce the same semantic target result.

Thread scheduling must not change meaning.

---

## 31. Rule Versioning

Conversion rules may evolve across ERDFlow versions.

The exact rule-version strategy is deferred.

However, applied-rule identity should be recorded where provenance requires it.

---

## 32. Validation Rule Identity

Validation issues should use stable machine-readable rule codes.

Example:

```text
conceptual.entity.identifier.missing
```

rather than using user-facing English text as the identifier.

This supports:

```text
tests
localization
documentation
future tooling
```

---

## 33. Human Explanation

Presentation may render:

```text
Entity "Student" has no identifier.
```

But the core issue is structured:

```text
Code:
conceptual.entity.identifier.missing

Element:
EntityId E1
```

---

## 34. Element References

Validation issues should reference stable IDs where possible.

Example:

```text
Issue
→ EntityId E1
```

This lets the UI:

```text
select object
zoom to object
open properties
```

without matching names.

---

## 35. Multiple Elements

Some issues involve several elements.

Example:

```text
duplicate generated name conflict
```

may involve:

```text
Relation R1
Relation R2
```

The issue model may need multiple references.

Exact representation is deferred.

---

## 36. Suggested Resolution

Validation may optionally provide structured resolution hints.

Example:

```text
Choose an identifier
```

or:

```text
Select a specialization mapping strategy
```

The suggestion should not directly mutate the Project.

---

## 37. No Automatic Repair Without Policy

ERDFlow should not silently "fix" structural meaning.

Example:

```text
missing Entity identifier
```

must not automatically create:

```text
id INT
```

unless the user/project policy explicitly requested such a rule.

---

## 38. Technical Auto-Resolution

Some purely technical formatting issues may be resolved deterministically.

Example:

```text
identifier quoting
name escaping
```

for a specific SQL dialect.

These are not equivalent to changing Conceptual semantics.

---

## 39. Validation Timing

Validation may happen:

```text
during editing
on explicit Validate
before conversion
before export/generation
during load
during import
```

Different contexts may run different rule subsets.

---

## 40. Incremental Validation

Small edits may trigger targeted validation.

Example:

```text
Change Relationship cardinality
      ↓
validate affected relationship
```

Full-project validation remains available.

SCALE.md governs performance strategy.

---

## 41. Full Validation

Run full relevant validation:

```text
before conversion
before important generation
on explicit user request
```

unless an equivalent cached/incremental result is proven current.

---

## 42. Cached Validation

Validation results may be cached later.

But cache invalidation must be correct.

Do not use stale validation to authorize conversion.

---

## 43. Background Validation

Large validation may run in background under ADR-007.

The result is associated with:

```text
ProjectId
SourceRevision
```

If stale:

```text
do not treat as authoritative for the current model
```

---

## 44. Validation and Revision

Before conversion:

```text
validation result revision
```

must correspond to:

```text
conversion input revision
```

or validation must be re-run on the exact conversion snapshot.

---

## 45. Preferred Conversion Pipeline

Strong direction:

```text
Capture consistent snapshot
      ↓
Validate snapshot
      ↓
Classify issues
      ↓
If allowed:
convert same snapshot
```

This avoids:

```text
validate revision 10
convert revision 11
```

---

## 46. Validation + Conversion in One Background Task

For large models, validation and conversion may run inside one background task against the same immutable snapshot.

Conceptually:

```text
Snapshot
   ↓
Validate
   ↓
Allowed?
   ↓ yes
Convert
```

This is a good consistency model.

---

## 47. Validation Does Not Mutate

Validation is read-only.

It returns issues.

It does not directly repair Project state.

---

## 48. Conversion Does Not Mutate Source

Conversion is also read-only with respect to the source snapshot.

It produces:

```text
candidate target
```

not direct source mutation.

---

## 49. Candidate Is Not Automatically Accepted

A valid conversion result is still a candidate until the Application workflow accepts/applies it.

This is especially important once downstream user edits exist.

---

## 50. First-Time Conversion

For the first Conceptual → Relational conversion, Application may allow a simpler flow:

```text
Validate
Convert
Preview
Accept
```

Once an editable Schema already exists, regeneration rules apply.

---

## 51. Regeneration Validation

Future regeneration must validate:

```text
source Conceptual state
candidate Schema
mapping/provenance consistency
review proposal
```

ADR-010 refines change-management behavior.

---

## 52. Validation of Generated Candidate

A deterministic conversion engine should normally produce a structurally valid candidate.

Still, candidate validation is valuable as a defensive check.

Example:

```text
conversion bug produces duplicate key reference
```

Candidate validation catches it before application.

---

## 53. Defensive Validation

The system may validate both:

```text
source prerequisites
```

and:

```text
generated target invariants
```

This improves reliability.

---

## 54. Severity Model

A practical first conceptual model:

```text
FatalInvariant
Error
Warning
Info
DecisionRequired
```

But exact enum names are not locked.

The semantics are more important than labels.

---

## 55. Conversion Scope

A validation issue may block one transformation but not another.

Example:

```text
missing physical length
```

may not block:

```text
Conceptual → Relational
```

but may block:

```text
Physical → SQL
```

Therefore blocking scope is operation-specific.

---

## 56. Example Blocking Scope

Conceptually:

```text
Issue:
Missing LogicalType

Blocks:
Relational → Physical

Does not block:
Conceptual editing
Save
```

This prevents over-broad validation behavior.

---

## 57. Work-in-Progress Philosophy

ERDFlow should help users build incomplete models gradually.

Validation should guide.

It should not constantly prevent normal editing.

Therefore:

```text
editing freedom
+
clear readiness feedback
```

is preferred over:

```text
force complete correctness at every keystroke
```

---

## 58. Basic Mode

Basic/Learning Mode may intentionally permit less engineering metadata.

Validation should respect the current modeling mode.

Example:

```text
LogicalType missing
```

may not be relevant while purely learning conceptual notation.

But conversion readiness may still explain what additional data is required.

---

## 59. Convertible Mode

Convertible/Engineering Mode exposes richer requirements.

Validation may require:

```text
logical types
identifiers
other conversion metadata
```

for selected transformations.

Both modes still use one underlying Conceptual Model.

---

## 60. Readiness

The core should expose concrete validation facts.

Example:

```text
2 blocking errors
3 warnings
1 unresolved decision
```

The UI may derive:

```text
Schema Readiness: 72%
```

if product design wants it.

The percentage is not authoritative Domain truth.

---

## 61. Readiness Percentage

If implemented, a readiness score must not hide blockers.

Example:

```text
95% ready
```

with one critical blocker still means:

```text
cannot convert
```

Therefore blockers are always shown explicitly.

---

## 62. Validation Panel

Future UI may display:

```text
Errors
Warnings
Decisions
Information
```

Clicking an issue can navigate to the affected element.

This is Presentation behavior.

---

## 63. Conversion Dialog

Future conversion UI may show:

```text
Ready to Convert
Applied Rules
Warnings
Required Decisions
```

The core provides structured data.

Presentation controls layout.

---

## 64. No UI Strings in Core Policy

The Domain should not decide:

```text
red icon
yellow icon
dialog title
```

It returns issue semantics.

---

## 65. Save vs Convert Example

Model:

```text
Student
Name
```

but no identifier.

Possible policy:

```text
Save:
allowed

Conceptual editing:
allowed

Conceptual → Relational:
may be blocked depending on conversion strategy
```

This is exactly why validation contexts must differ.

---

## 66. M:M Example

Conceptual:

```text
Student M:M Course
Relationship: Enrollment
```

If both sides and identifiers are sufficient:

```text
Info:
M:M will produce junction relation

Conversion:
allowed
```

---

## 67. Missing Cardinality Example

Relationship:

```text
Student ? Course
```

If FK placement/junction decision depends on cardinality:

```text
Blocking Conversion Error
```

until resolved.

---

## 68. Weak Entity Example

Weak Entity:

```text
Dependent
```

but no owner:

```text
Blocking Conversion Error
```

because the relational identity cannot be derived correctly.

---

## 69. Derived Attribute Example

Derived:

```text
Age
```

Possible policy:

```text
Info:
Derived attribute is not materialized by default.
```

Conversion may proceed.

---

## 70. Naming Conflict Example

Two generated relations would have the same target name.

Possible policy:

```text
Warning or Blocking Decision
```

depending on whether the naming policy can deterministically resolve it.

If safe deterministic naming exists:

```text
warning/info
```

If user choice is required:

```text
DecisionRequired
```

---

## 71. Dialect Example

Physical model contains:

```text
UUID
```

selected dialect does not support the configured mapping.

Possible:

```text
Blocking SQL Generation Error
```

until a target type/policy is selected.

---

## 72. Validation of Manual Schema Edits

Schema edits may create:

```text
conversion warnings
upstream propagation candidates
technical-only changes
```

Validation should not automatically push changes upstream.

That is ADR-010 behavior.

---

## 73. Validation and Mapping

Mapping validation may detect:

```text
dangling mapping
ambiguous reverse lineage
type-incompatible mapping
```

Some of these are Domain invariant issues.

Others may be warnings/decisions during regeneration.

---

## 74. Validation and Baselines

Baseline integrity must be checked when used for three-way review.

If required baseline state is corrupted:

```text
safe regeneration may be blocked
```

rather than guessed.

---

## 75. Imported Models

Imported structures may initially contain incomplete semantics.

Import workflow may allow:

```text
candidate with warnings
```

for review before Project application.

The exact import gate is ADR-011.

---

## 76. Parser Errors

Syntax/parse errors are not ordinary Domain validation warnings.

Example:

```text
invalid SQL syntax
```

belongs to import parsing.

After parsing succeeds, Domain validation begins.

---

## 77. Conversion Error vs Implementation Error

A conversion error means the input cannot currently be transformed under policy.

An implementation error means ERDFlow itself failed unexpectedly.

These must not be conflated.

---

## 78. Unexpected Internal Failure

Example:

```text
conversion algorithm violates target invariant
```

This is not:

```text
user has one more validation warning
```

It should be treated as an internal failure/bug.

---

## 79. Error Result Discipline

Core operations should make failure state explicit.

Avoid returning:

```text
empty schema
```

with no indication whether:

```text
conversion succeeded with zero relations
```

or:

```text
conversion failed
```

---

## 80. Partial Success

Where partial success is supported in future:

```text
status
completed portions
unresolved portions
issues
```

must be explicit.

Not inferred from missing objects.

---

## 81. Validation Rule Organization

Rules should be grouped by concern.

Possible categories:

```text
ConceptualStructure
RelationshipSemantics
WeakEntityRules
SpecializationRules
RelationalIntegrity
PhysicalDialect
MappingIntegrity
BaselineIntegrity
```

Exact module organization is deferred.

---

## 82. Rule Dependencies

Avoid rules silently depending on UI order.

Validation should operate from the model/snapshot only.

---

## 83. Rule Determinism

Validation for the same snapshot/options should produce semantically equivalent results regardless of thread scheduling.

Ordering may be normalized for stable display/tests.

---

## 84. Issue Ordering

For user clarity, UI may sort issues by:

```text
blocking first
then decisions
then warnings
then info
```

or by location.

Core may provide stable ordering keys if needed.

---

## 85. Issue Deduplication

Validation should avoid flooding the user with duplicate consequences of one root issue where practical.

Example:

```text
missing Entity target
```

should not create dozens of identical follow-on errors if they add no value.

Exact deduplication policy is deferred.

---

## 86. Root Cause Preference

Prefer reporting:

```text
Relationship references missing Entity
```

rather than many secondary failures caused by that broken reference.

---

## 87. Validation Extensibility

Future plugins may provide additional validation.

Core invariant rules remain trusted built-in rules.

Plugin rules must not be able to redefine fundamental Project validity without an explicit extension contract.

Deferred.

---

## 88. AI Validation

Future AI may suggest:

```text
possible normalization issue
```

This is advisory.

Deterministic Domain validation remains authoritative for hard structural rules.

AI cannot turn a hard invariant failure into a valid model.

---

## 89. Validation and Future Rust

Validation has a clean possible future Rust boundary:

```text
Model Snapshot
      ↓
Validator
      ↓
ValidationResult
```

This ADR does not require Rust.

It simply keeps the contract framework-independent.

---

## 90. Validation and VS Code

A future VS Code extension can invoke the same validation logic and show the same issue codes.

This avoids frontend-specific rule divergence.

---

## 91. Alternative A — Convert Everything and Hope

Rejected.

This could generate silently incorrect downstream models.

---

## 92. Alternative B — Block Conversion on Any Warning

Rejected.

This would make the tool unnecessarily rigid and frustrating.

Warnings exist specifically because the operation can remain valid.

---

## 93. Alternative C — One Generic Error Severity

Rejected.

It cannot express:

```text
work-in-progress
warning
hard invariant failure
decision required
operation-specific blocker
```

---

## 94. Alternative D — UI Decides What Blocks

Rejected.

Presentation should not invent semantic conversion policy.

Blocking behavior belongs to Application/Domain rules.

---

## 95. Alternative E — Auto-Fix Everything

Rejected.

Some fixes change design meaning.

ERDFlow should not silently invent user intent.

---

## 96. Alternative F — Force Complete Model Before Save

Rejected.

ERDFlow must support work-in-progress design.

---

## 97. Alternative G — Separate Validation Engines for Every UI

Rejected.

Desktop, future VS Code, and future services should reuse the same core rules.

---

## 98. Consequences — Positive

This decision gives ERDFlow:

- predictable conversion behavior,
- safe handling of incomplete models,
- clear blockers,
- useful warnings,
- explicit decision points,
- reusable validation across frontends,
- better testing,
- safer deterministic transformation,
- better user trust.

---

## 99. Consequences — Costs

This requires:

- structured issue types,
- operation-specific validation rules,
- readiness policy,
- validation tests,
- decision-resolution workflows,
- more deliberate conversion APIs.

These costs are justified.

---

## 100. Initial Implementation Scope

For the first Conceptual Editor:

implement validation only for rules that exist.

Examples:

```text
stable reference integrity
relationship participant validity
cardinality representation
basic weak-entity consistency when feature arrives
```

Do not implement future Physical/SQL rules before those models exist.

---

## 101. Initial Conversion Scope

When Conceptual → Relational begins, implement:

```text
source validation
blocking classification
warnings
required decisions
conversion on same snapshot
candidate validation
```

for only the conversion rules then supported.

---

## 102. Initial Tests

### Test 1 — Warning Does Not Block

```text
valid model + warning
→ conversion allowed
```

### Test 2 — Blocking Error Stops

```text
required conversion information missing
→ no accepted candidate
```

### Test 3 — Invariant Violation Stops

```text
relationship references missing EntityId
→ conversion blocked
```

### Test 4 — Save Still Allowed

```text
conversion-blocked work-in-progress model
→ save allowed if representable
```

### Test 5 — Decision Required

```text
valid specialization
strategy unresolved
→ DecisionRequired
→ conversion waits
```

### Test 6 — Same Snapshot

```text
validation + conversion operate on same revision
```

### Test 7 — Candidate Validation

```text
generated candidate violates target invariant
→ internal/conversion failure
```

---

## 103. Architecture Invariants

### Invariant 1

Validation is structured, not UI-text driven.

### Invariant 2

Domain invariant violations block transformations that rely on valid Domain state.

### Invariant 3

Blocking conversion errors cannot be bypassed by a generic "Convert Anyway" action.

### Invariant 4

Warnings may allow conversion.

### Invariant 5

Information never blocks conversion.

### Invariant 6

Unresolved legitimate strategy choices are represented explicitly as decisions.

### Invariant 7

Save validation and conversion validation are different policies.

### Invariant 8

Work-in-progress models may be saved when they are safely representable.

### Invariant 9

Validation and conversion operate on a consistent source state.

### Invariant 10

Validation does not mutate the Project.

### Invariant 11

Conversion does not mutate its source.

### Invariant 12

Generated candidates are validated before controlled application where appropriate.

---

## 104. Relationship to Other ADRs

Depends on:

```text
ADR-001 — Stable Identity Strategy
ADR-002 — Layered Architecture and Dependency Direction
ADR-004 — Core Language Strategy
ADR-006 — Command / Undo Strategy
ADR-007 — Background Work / Concurrency Strategy
ADR-008 — Cross-Level Mapping and Provenance
```

This ADR is required before:

```text
Conceptual → Relational conversion implementation
Relational → Physical conversion implementation
ADR-010 — Change Propagation and Regeneration
```

---

## 105. Open Implementation Decisions

This ADR intentionally leaves open:

- exact severity enum names,
- exact blocking-scope representation,
- exact issue-code naming convention,
- exact readiness-score formula if any,
- exact project-level conversion defaults,
- exact specialization strategies,
- exact UI wording,
- exact warning confirmation UX,
- exact incremental validation cache,
- exact plugin validation extension model.

These should be decided when concrete features require them.

---

## 106. Decision Outcome

ERDFlow adopts:

```text
structured validation
+
operation-specific conversion gates
+
explicit blockers
+
non-blocking warnings
+
explicit unresolved decisions
+
same-snapshot validate-then-transform
```

as the policy for deterministic model conversion.

---

## 107. Final Principle

> ERDFlow should never confuse "unfinished", "questionable", and "invalid". Each state deserves a different response.
