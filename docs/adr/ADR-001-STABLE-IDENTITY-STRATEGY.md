# ADR-001 — Stable Identity Strategy

**Status:** Accepted  
**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** Persistent identity for semantic and cross-level project elements

---

## 1. Context

ERDFlow is not only a drawing application.

A single project may evolve through:

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

Objects may be renamed, moved, converted, regenerated, deleted and restored,
copied, imported, or represented in several views.

Therefore ERDFlow cannot use mutable properties such as:

```text
name
canvas position
table name
attribute name
array index
memory address
```

as object identity.

Example:

```text
Entity:
    Name = Student
```

later becomes:

```text
Entity:
    Name = UniversityStudent
```

ERDFlow must still know that both states refer to the same semantic object.

Stable identity is also required by:

- undo/redo,
- mapping and provenance,
- Conceptual → Relational conversion,
- Relational → Physical conversion,
- generation baselines,
- three-way regeneration review,
- controlled Schema → Conceptual propagation,
- persistence,
- import,
- future collaboration,
- future VS Code integration,
- future Rust reuse.

---

## 2. Decision

ERDFlow will use:

> **Strongly typed UUIDv7 identifiers for persistent semantic identity.**

The underlying identifier value is a standards-based **128-bit UUID Version 7**.

Domain identifiers are strongly typed so identifiers from different semantic
categories cannot be accidentally mixed.

Conceptually:

```cpp
EntityId
AttributeId
RelationshipId
RelationId
ColumnId
```

may all contain UUIDv7 values, but they are different C++ types.

Example:

```text
EntityId:
019c1d42-36d2-7c3a-8f42-5cb8e9b8a128
```

The human-readable object name remains separate:

```text
Name:
Student
```

---

## 3. Why UUIDv7

UUIDv7 is standardized by RFC 9562.

Its high-order bits contain a Unix-epoch millisecond timestamp and the
remaining UUID space provides uniqueness through random/monotonic data.

This gives ERDFlow several useful properties:

- globally unique IDs without a central database,
- creation on offline desktop clients,
- safe future creation from several applications or devices,
- compatibility with future services,
- sortable locality compared with fully random UUIDs,
- a standardized 128-bit representation,
- broad cross-language interoperability.

This fits ERDFlow's local-first architecture better than identifiers that
require a central sequence generator.

---

## 4. Identity Is Not a Name

This is a fundamental invariant.

Wrong:

```text
Entity identity = "Student"
```

Correct:

```text
EntityId = 019c1d42-36d2-7c3a-8f42-5cb8e9b8a128
Name     = "Student"
```

Rename:

```text
Name = "UniversityStudent"
```

Identity remains:

```text
EntityId = 019c1d42-36d2-7c3a-8f42-5cb8e9b8a128
```

---

## 5. Identity Is Not Canvas Position

Wrong:

```text
Entity identity = x:420, y:190
```

Moving the object would appear to create a new entity.

Correct:

```text
EntityId = stable
Layout.x = editable
Layout.y = editable
```

Layout is presentation state.

Identity is semantic state.

---

## 6. Identity Is Not a Memory Address

C++ object addresses are process-local and temporary.

Wrong:

```text
Entity* pointer address
→ persistent identity
```

Addresses change across:

- application restarts,
- allocations,
- copies,
- serialization,
- containers,
- future Rust boundaries.

Pointers may reference objects at runtime.

They must never become persistent IDs.

---

## 7. Identity Is Not a Container Index

Wrong:

```text
entities[4]
```

as identity.

Deleting or inserting earlier elements changes indexes.

Correct:

```text
EntityId → Entity
```

Container order may change while identity remains stable.

---

## 8. Strongly Typed IDs

ERDFlow should prevent accidental cross-type comparisons where practical.

Conceptually:

```cpp
EntityId entity;
AttributeId attribute;
```

These should not silently behave as interchangeable raw UUIDs.

A possible future C++ design is:

```cpp
template <typename Tag>
class StrongId;
```

with aliases or wrappers such as:

```cpp
using EntityId = StrongId<EntityTag>;
using AttributeId = StrongId<AttributeTag>;
```

The exact implementation syntax is deferred.

The architectural decision is the type distinction.

---

## 9. Initial ID Categories

Persistent IDs will eventually include at least:

```text
ProjectId

EntityId
AttributeId
RelationshipId
SpecializationId
PageId

RelationId
SchemaAttributeId
PrimaryKeyId
ForeignKeyId

TableId
ColumnId
ConstraintId
IndexId

MappingId
BaselineId
```

Additional persistent semantic object types may receive their own ID type
when introduced.

---

## 10. Semantic IDs vs Layout IDs

Not every visual object automatically deserves a semantic identifier.

Example:

```text
Student Entity
```

has:

```text
EntityId
```

Its canvas representation may reference that ID.

For layout structures that have independent persistent meaning, ERDFlow may
also use IDs such as:

```text
PageId
ConnectorLayoutId
AnnotationId
```

where justified.

Do not create persistent identifiers for ephemeral UI state such as:

```text
hover highlight
selection rectangle
temporary drag guide
context menu
```

---

## 11. Domain Independence From Qt

The Domain must not use `QUuid` as its fundamental public identity type.

Reason:

```text
Domain
```

must remain independent of:

```text
Qt
Presentation
platform UI
```

The Domain should own or depend on a small framework-independent UUID value
representation.

Conceptually:

```text
StrongId<EntityTag>
    ↓
Uuid128
    ↓
16-byte value
```

The exact implementation may use standard C++ storage such as a fixed
16-byte value.

Qt adapters may convert between ERDFlow IDs and `QUuid` where useful.

---

## 12. UUID Generation Boundary

Creation of new IDs should happen through a small identity-generation
boundary rather than by scattering UUID-generation calls throughout the
application.

Conceptually:

```cpp
class IdGenerator
{
public:
    EntityId newEntityId();
    AttributeId newAttributeId();
    RelationshipId newRelationshipId();
};
```

The exact API is not locked.

The important rule is:

> ID generation is centralized enough to test and replace, while identity
> values themselves remain simple Domain values.

---

## 13. UUID Version

New ERDFlow persistent IDs use:

```text
UUID Version 7
```

Older/imported projects may theoretically contain other UUID versions in the
future if migration or compatibility requires it, but new ERDFlow-created
persistent identities use UUIDv7 unless a future ADR changes the policy.

---

## 14. Canonical Text Representation

When UUIDs are serialized to `.erdx`, logs, diagnostics, or textual exchange,
ERDFlow should use the canonical hexadecimal UUID form:

```text
xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
```

Example:

```text
019c1d42-36d2-7c3a-8f42-5cb8e9b8a128
```

Preferred persisted form:

```text
lowercase
without braces
with hyphens
```

This gives predictable serialization and easier cross-language parsing.

---

## 15. Binary Representation

In memory, implementations should prefer the compact 128-bit value rather
than storing every ID permanently as a heap-allocated string.

Conceptually:

```text
16 bytes
```

rather than:

```text
36-character string
```

The canonical string representation is primarily for:

- persistence,
- debugging,
- logs,
- text interchange.

Exact storage type is an implementation detail.

---

## 16. Human-Friendly Debug Labels

Documentation may show short labels such as:

```text
E-104
A-20
R-310
```

These are useful for diagrams and explanations.

They are **not** the persistent identity format.

ERDFlow may later derive temporary/debug display aliases from UUIDs.

Example:

```text
EntityId:
019c1d42-36d2-7c3a-8f42-5cb8e9b8a128

Debug label:
E-...a128
```

A user-facing short label must never replace the real stable ID in storage.

---

## 17. Creation Rule

Creating a new semantic object generates a new stable ID.

Example:

```text
Create Entity
      ↓
new EntityId
      ↓
Entity(Student)
```

The ID is created once.

Normal editing does not regenerate it.

---

## 18. Rename Rule

Rename preserves identity.

```text
Student
EntityId = X
```

becomes:

```text
UniversityStudent
EntityId = X
```

This rule applies to:

- entities,
- attributes,
- relationships,
- relations,
- schema attributes,
- tables,
- columns,
- constraints,
- indexes,
- other identified objects.

---

## 19. Move Rule

Moving a visual representation preserves semantic identity.

```text
Student at x=100
EntityId = X
```

becomes:

```text
Student at x=850
EntityId = X
```

Only layout state changes.

---

## 20. Delete + Undo Rule

If an object is deleted and then restored through Undo, the restored object
must recover the same ID.

Example:

```text
EntityId X
    ↓ delete
removed
    ↓ undo
EntityId X
```

Undo is restoration of the same semantic object.

It is not creation of a new one.

---

## 21. Delete + Manual Recreate Rule

If the user deletes an object and later independently creates a new object
with the same name, it receives a new ID.

Example:

```text
Student
EntityId = X

delete
```

Later:

```text
Create Entity("Student")
EntityId = Y
```

Even though the names match:

```text
X != Y
```

This prevents name-based identity confusion.

---

## 22. Copy / Paste Rule

Copying a semantic object to create another object must generate new IDs for
the copied semantic objects.

Example:

```text
Original:
EntityId = X
```

Copy/paste as a new entity:

```text
Copy:
EntityId = Y
```

The copy may record an origin relationship if useful, but it is a distinct
semantic object.

---

## 23. Duplicate View Rule

Creating another visual representation of the **same** semantic element does
not create a new semantic ID.

Example:

```text
EntityId X
```

may appear in:

```text
Explorer
Canvas
Properties
Search results
```

All those views reference:

```text
EntityId X
```

---

## 24. Save / Load Rule

Saving and loading must preserve IDs exactly.

```text
Before Save:
EntityId = X

After Load:
EntityId = X
```

Generating new IDs during load would destroy:

- mappings,
- baselines,
- undo-related meaning,
- cross-level provenance,
- future collaboration history.

Therefore deserialization restores persisted IDs.

---

## 25. Import Rule

Imported objects receive ERDFlow IDs.

Example:

```text
SQL:
CREATE TABLE Student (...)
```

Import:

```text
TableId = new ERDFlow UUIDv7
```

If the source format contains its own stable identifier, ERDFlow may preserve
that separately as source/provenance metadata.

Do not blindly use:

```text
table name
column position
source line number
```

as ERDFlow identity.

---

## 26. Conversion Rule

A Conceptual object and its generated Relational object are not the same
object.

Example:

```text
Conceptual Entity
EntityId = E
```

generates:

```text
Relational Relation
RelationId = R
```

and ERDFlow records:

```text
E → R
```

through mapping/provenance.

Do not reuse `EntityId` as `RelationId`.

Different modeling levels have distinct identities.

---

## 27. M:M Conversion Example

Conceptual:

```text
Student M:M Course

Relationship:
Enrolled

RelationshipId = REL-X
```

Conversion creates:

```text
Enrollment Relation
RelationId = RELATION-Y
```

Mapping:

```text
REL-X
   ↓ generated as
RELATION-Y
```

The mapping explains lineage.

The IDs remain type-distinct.

---

## 28. Regeneration Rule

When regeneration determines that a generated downstream object represents
the same continuing semantic target, ERDFlow should preserve/reuse its
existing target ID where possible.

Example:

```text
Conceptual Entity E
      ↓
Relation R
```

Conceptual adds an attribute and regenerates.

If `R` is still the same generated Relation, it should remain:

```text
RelationId = R
```

rather than receiving a new ID every regeneration.

This is essential for:

- manual downstream edits,
- diff quality,
- stable references,
- mapping continuity.

---

## 29. New Generated Object Rule

If regeneration creates a genuinely new downstream object, it receives a new
ID.

Example:

```text
New Conceptual M:M Relationship
      ↓
new Junction Relation
      ↓
new RelationId
```

---

## 30. Baseline Rule

Generation baselines preserve the IDs that existed when the baseline was
accepted.

This allows comparison between:

```text
Previous Generated Baseline
Current User-Edited Schema
New Generated Candidate
```

without relying on names.

---

## 31. Mapping Rule

Cross-level mappings refer to objects by stable ID.

Conceptually:

```text
MappingId = M

Source:
    EntityId = E

Target:
    RelationId = R
```

Never:

```text
SourceName = "Student"
TargetName = "Student"
```

as the authoritative mapping identity.

Names may be included only for human-readable explanations.

---

## 32. Referential Integrity Inside a Project

References between model objects should use typed stable IDs.

Example:

```text
RelationshipParticipant
    EntityId = E
```

rather than:

```text
RelationshipParticipant
    entityName = "Student"
```

Similarly:

```text
ForeignKey
    SourceRelationId
    TargetRelationId
```

rather than name-only references.

---

## 33. Lookup Strategy

The Domain/Application architecture should permit efficient lookup such as:

```text
EntityId → Entity
AttributeId → Attribute
RelationId → Relation
ColumnId → Column
MappingId → Mapping
```

The exact container is deferred.

Candidates include:

- hash maps,
- indexed registries,
- typed stores.

The Scale invariant remains:

> Common stable-ID and mapping lookups should not require repeated full-model
> scans when indexed lookup is practical.

---

## 34. Ordering Rule

UUIDv7 has time-ordered characteristics.

ERDFlow must not confuse that property with semantic ordering.

Wrong:

```text
UUID order
→ database row order
```

or:

```text
UUID order
→ canvas order
```

UUID ordering may be useful internally for locality/debugging.

Business, display, creation, or row ordering must use explicit fields when
required.

---

## 35. Timestamp Privacy Consideration

UUIDv7 contains a coarse creation timestamp.

Therefore a UUIDv7 can reveal approximately when the ID was created.

ERDFlow IDs are identifiers, not secrets.

They must never be used as:

- authentication tokens,
- passwords,
- authorization secrets,
- unguessable security capabilities.

If future contexts require opaque security tokens, those use a separate
security mechanism.

---

## 36. Collision Handling

UUID collisions are extremely unlikely when generated correctly.

However, persistence/import code should still treat duplicate IDs inside a
project as invalid structural data.

On load/import:

```text
duplicate stable ID
      ↓
validation error
```

ERDFlow must not silently merge unrelated objects merely because IDs collide.

---

## 37. Null / Invalid IDs

Domain APIs should distinguish:

```text
valid ID
```

from:

```text
no ID / invalid ID
```

Prefer explicit optionality where an identifier may legitimately be absent.

Avoid using a magic ordinary UUID as a sentinel value if the type system can
express absence.

Exact representation is deferred.

---

## 38. Serialization Validation

When loading `.erdx`, every persisted ID should be checked for:

- valid syntax,
- expected identifier presence,
- duplicate identity,
- reference integrity,
- compatible element type.

Malformed identity is a project validation error.

---

## 39. Future Collaboration

UUIDv7 supports future creation of objects on multiple clients without
requiring a central integer allocator.

Example:

```text
Desktop A creates Entity
Desktop B creates Entity
VS Code extension creates Entity
```

Each can create identifiers locally.

This does not itself solve collaborative conflict resolution.

It only avoids making identity generation a central-server dependency.

---

## 40. Future VS Code Extension

A future VS Code extension may work with the same `.erdx` project/core.

Stable UUID-based IDs allow:

```text
Desktop ERDFlow
VS Code extension
Future service
```

to refer to the same semantic objects consistently.

No VS Code-specific ID scheme is required.

---

## 41. Future Rust Boundary

The UUID representation must be easy to exchange with Rust.

Preferred boundary forms:

```text
16 raw bytes
```

or:

```text
canonical UUID string
```

depending on the interface.

Do not expose Qt-specific UUID types across the future Rust boundary.

---

## 42. Qt Integration

Qt may be used as an adapter.

Modern Qt provides `QUuid`, including UUIDv7 generation in Qt versions that
support `QUuid::createUuidV7()`.

However:

```text
Qt UUID API
```

is not the Domain identity contract.

The core contract is:

```text
RFC 9562 UUIDv7 value
+
strong ERDFlow semantic type
```

This keeps the Domain portable even if Qt versions or frontends change.

---

## 43. Alternatives Considered

### Alternative A — Auto-Increment Integers

Example:

```text
EntityId = 1
EntityId = 2
EntityId = 3
```

### Advantages

- compact,
- simple,
- fast,
- easy to read.

### Rejected as the global persistent identity strategy

Because independent/offline creators can collide.

Future:

```text
Desktop
VS Code
Cloud
Import worker
```

would require coordination or ID remapping.

Integers may still be used for local array indexes or database-specific
surrogate keys where appropriate.

---

## 44. Alternative B — UUIDv4

### Advantages

- standardized,
- broadly supported,
- decentralized,
- very low collision probability,
- straightforward generation.

### Why Not Preferred

UUIDv7 provides the same decentralized 128-bit identity model while adding
time-ordered characteristics and is the modern standardized choice for new
time-oriented UUID generation where suitable.

UUIDv4 remains technically acceptable, but UUIDv7 better matches ERDFlow's
future persistence/indexing direction.

---

## 45. Alternative C — ULID

### Advantages

- sortable,
- readable Base32 text,
- decentralized,
- popular in application systems.

### Why Not Preferred

ERDFlow prefers the standardized UUID ecosystem.

UUIDv7 now provides a standardized time-ordered UUID format and has direct
support in modern libraries/frameworks.

Choosing UUIDv7 avoids introducing a separate identifier specification when
the required properties are already available through the UUID standard.

---

## 46. Alternative D — Names as IDs

Rejected.

Names are editable and may not be unique.

Example:

```text
Student.Name
```

can be renamed.

Two schemas may also contain similarly named objects.

Names are presentation/domain labels, not stable identity.

---

## 47. Alternative E — Path-Based IDs

Example:

```text
Conceptual.Student.StudentID
```

Rejected as persistent identity.

Renaming or moving any parent changes the path.

Paths may be generated for display/search.

They are not identity.

---

## 48. Alternative F — One Universal Untyped UUID Alias

Example:

```cpp
using Id = Uuid;
```

for every object.

Rejected as the preferred Domain API because code could accidentally pass:

```text
AttributeId
```

where:

```text
EntityId
```

was expected.

Strong semantic types make invalid combinations harder to express.

---

## 49. Consequences — Positive

This decision gives ERDFlow:

- identity independent of names,
- safe renaming,
- safe save/load,
- reliable mappings,
- reliable baseline comparison,
- better diff quality,
- safe undo restoration,
- offline generation,
- future multi-client compatibility,
- future Rust interoperability,
- future VS Code interoperability.

---

## 50. Consequences — Costs

This decision also has costs:

- 128-bit IDs use more memory than 32/64-bit integers,
- UUID strings are visually long,
- strongly typed wrappers require some implementation effort,
- UUIDv7 exposes approximate creation time,
- lookup indexes/registries must be maintained correctly.

These costs are acceptable for ERDFlow's requirements.

---

## 51. Performance Consideration

ERDFlow should not optimize away stable identity prematurely.

At realistic design-project sizes, correctness and provenance are more
important than saving a few bytes per ID.

When performance matters:

```text
UUID value
→ compact 16-byte in-memory representation

lookup
→ indexed container
```

rather than replacing stable identity with fragile integer positions.

---

## 52. Persistence Example

Conceptually, a future `.erdx` representation may contain:

```json
{
  "id": "019c1d42-36d2-7c3a-8f42-5cb8e9b8a128",
  "name": "Student"
}
```

A relationship participant may use:

```json
{
  "entity_id": "019c1d42-36d2-7c3a-8f42-5cb8e9b8a128"
}
```

The exact `.erdx` schema is defined later.

This example only illustrates identity separation.

---

## 53. Domain Example

Conceptually:

```cpp
struct Entity
{
    EntityId id;
    std::string name;
};
```

not:

```cpp
struct Entity
{
    std::string name; // also used as identity
};
```

This is illustrative, not the final C++ implementation.

---

## 54. Relationship Example

Conceptually:

```cpp
struct RelationshipParticipant
{
    EntityId entityId;
    Cardinality cardinality;
    Participation participation;
};
```

This gives the participant a stable reference to its entity.

---

## 55. Mapping Example

Conceptually:

```cpp
struct Mapping
{
    MappingId id;
    ElementReference source;
    ElementReference target;
    MappingKind kind;
};
```

`ElementReference` must preserve both:

```text
semantic type
stable UUID identity
```

The exact variant/tagging strategy is deferred to ADR-008.

---

## 56. Identity Lifecycle Summary

```text
CREATE
  ↓
Generate UUIDv7

EDIT
  ↓
Preserve ID

RENAME
  ↓
Preserve ID

MOVE
  ↓
Preserve ID

SAVE / LOAD
  ↓
Preserve ID

DELETE + UNDO
  ↓
Restore same ID

DELETE + NEW CREATE
  ↓
New ID

COPY AS NEW OBJECT
  ↓
New ID

GENERATE CROSS-LEVEL OBJECT
  ↓
New target-level ID + Mapping

REGENERATE SAME TARGET
  ↓
Reuse target ID where lineage proves continuity
```

---

## 57. Invariants

### Invariant 1

Every persistent semantic object has a stable identity.

### Invariant 2

Names are not identifiers.

### Invariant 3

Canvas positions are not identifiers.

### Invariant 4

Memory addresses are not identifiers.

### Invariant 5

Container indexes are not persistent identifiers.

### Invariant 6

Renaming preserves identity.

### Invariant 7

Moving preserves semantic identity.

### Invariant 8

Undo restoration restores the original identity.

### Invariant 9

Manual recreation creates a new identity.

### Invariant 10

Copying into a new semantic object creates new identity.

### Invariant 11

Save/load preserves identity.

### Invariant 12

Different modeling levels use distinct typed identities.

### Invariant 13

Cross-level continuity is expressed through mappings, not by reusing the same
typed ID.

### Invariant 14

New ERDFlow persistent IDs use UUIDv7.

### Invariant 15

Domain identity types do not depend on Qt.

### Invariant 16

Persistent text representation is canonical UUID text.

### Invariant 17

UUID ordering does not define business or row ordering.

### Invariant 18

IDs are not security secrets.

---

## 58. Implementation Scope for the First Domain Milestone

Do not implement every ID type immediately.

For the first Conceptual Editor Domain, implement only the IDs actually used:

```text
ProjectId
EntityId
AttributeId
RelationshipId
```

Add:

```text
PageId
SpecializationId
```

when those features arrive.

Add relational and physical IDs only when their corresponding models are
implemented.

The **strategy is global**.

The **implementation remains incremental**.

---

## 59. First Implementation Test Cases

When the Domain implementation begins, test at least:

### Test 1 — Creation

```text
Create two EntityIds
→ IDs differ
```

### Test 2 — Rename

```text
Create Entity
Save EntityId
Rename Entity
→ EntityId unchanged
```

### Test 3 — Type Safety

Where supported by the C++ design:

```text
EntityId
```

must not silently substitute for:

```text
AttributeId
```

### Test 4 — Round Trip

```text
ID
→ canonical string
→ parse
→ same ID
```

### Test 5 — Invalid Parse

Malformed UUID text returns a structured failure.

### Test 6 — Delete / Undo

Undo restores the same semantic ID.

---

## 60. Decision Outcome

ERDFlow adopts:

```text
RFC 9562 UUIDv7
+
strong semantic ID types
+
framework-independent Domain representation
+
canonical UUID text persistence
```

as its stable identity strategy.

This decision becomes the foundation for:

```text
Domain modeling
Undo/Redo
Persistence
Mappings
Provenance
Baselines
Diff/Review
Controlled propagation
Future multi-client integration
```

---

## 61. Follow-Up Decisions

This ADR intentionally does **not** decide:

- exact C++ `StrongId` implementation,
- exact UUIDv7 generator implementation/library,
- exact hash-container type,
- exact generic `ElementReference` representation,
- exact `.erdx` JSON/package schema,
- exact mapping representation,
- exact Rust FFI representation.

Those decisions should be made at the phase where they become necessary.

---

## 62. Final Principle

> Identity describes **which object it is**.  
> Names, positions, types, and other properties describe **what that object
> currently looks like or means**.

ERDFlow must never confuse those two responsibilities.
