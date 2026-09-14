# ERDX project format — versions 1 to 6

**Status:** Implemented Conceptual ERD format; version 10 is current  
**Date:** 2026-09-14

## What, why, and how

**WHAT:** An `.erdx` file stores one conceptual model and its single canvas layout
as UTF-8 JSON. It preserves project, element, and relationship participant IDs.

**WHY:** A saved diagram must retain its meaning and editable connections across
restarts. Explicit versions and strict validation prevent an older editor from
silently discarding fields it does not understand.

**HOW:** The Application owns the `ProjectStore` port and open/save use cases.
The Qt Infrastructure adapter validates a candidate before opening it and writes
through `QSaveFile` with direct-write fallback disabled. The Domain remains
independent of Qt and filesystems. See [ADR-005](adr/ADR-005-PROJECT-FILE-AND-PERSISTENCE-BOUNDARY.md),
[ADR-006](adr/ADR-006-COMMAND-UNDO-STRATEGY.md), and
[ADR-009](adr/ADR-009-VALIDATION-GATES-AND-CONVERSION-POLICY.md).

## Version and compatibility

The root object has exactly three fields:

| Field | Value |
| --- | --- |
| `format` | String, exactly `"erdflow"` |
| `format_version` | JSON number, exactly `1` to `6` |
| `project` | Project object described below |

A file with an unsupported version, missing field, unknown
field, duplicate JSON object key, or invalid value is rejected. Escaped-equivalent
keys, such as `"name"` and `"na\u006de"`, count as duplicates.

The same strict field rule applies to every nested object. Unknown fields are
neither ignored nor removed. A future format change must define its compatibility
and migration policy explicitly before writing a new version.

### Compatibility between versions

Each version adds one capability and changes nothing else. The rules are stated
here rather than left to be inferred.

**Version 2** adds connector shapes, so it differs from version 1 by exactly one
project field:

- A version 1 document must **not** contain `connectors`, and a version 2
  document must contain it. A document declaring the wrong shape for its
  version is rejected rather than read leniently.
- Opening a version 1 document succeeds and leaves every connector routed
  automatically. Nothing is lost, because version 1 could not express a shape.

**Version 3** adds associative relationships. A relationship object gains a
required `associative` field, and a participant's `entity` identifier is replaced
by a typed `target` reference, because a participant may now attach to an
associative relationship instead of an entity:

- A version 1 or 2 document must use `entity` and must not carry `associative`;
  a version 3 document must use `target` and must carry `associative`. A document
  whose shape contradicts its declared version is rejected in either direction.
- Opening an earlier document succeeds. Every relationship reads as not
  associative and every participant as targeting an entity, which is exactly
  what those versions could express.

**Version 4** adds specializations, the ISA triangles that carry inheritance.
A version 4 document has one further project field, `specializations`; earlier
versions must not carry it and open with none, which is what they could express.

**Version 5** records which way each hierarchy was read. A specialization
object gains a required `direction`; earlier versions must not carry it and read
as `"specialization"`, which is how they were drawn.

**Version 6** lets a placed ISA triangle wait for its supertype, so
`supertype` may be `null`. Earlier versions always named one, and a document
before version 6 carrying a null supertype is rejected rather than guessed at.

**Version 7** lets a connector pin where it meets each shape. A connector-shape
object gains required `owner_anchor` and `child_anchor`, either of which may be
`null`; earlier versions must not carry them and read as unpinned, which is how
those links were drawn.

**Version 8** lets a connector carry a route of its own. A connector-shape
object gains a required `waypoints` array, which may be empty; earlier versions
must not carry it and read as unrouted, which is the shape they were drawn with.

**Version 9** lets an element carry a surface colour of its own. The project
gains a required `colours` array, which may be empty; earlier versions must not
carry it, and every element in such a file follows its theme, as they always did.

**Version 10** lets one side of a relationship be drawn bare. A participant
gains a required `show_constraints` boolean; earlier versions must not carry it
and read as `true`, which is what every one of those diagrams meant. It changes
only what is drawn: the side keeps its `maximum` and `participation` either way,
and anything reasoning about the relationship still reads them.

Saving always writes version 10, so opening an earlier file and saving upgrades
it in place and an older build will then refuse the result. This one-way upgrade
is acceptable only because no release has shipped. A future version that must
stay readable by older builds needs a different policy, recorded before it is
written.

Whitespace and JSON object property order are not significant. Files written by
the adapter use indented JSON. Array iteration is deterministic for the current
ordered ID stores; element identity never depends on array position. Participant
array order is preserved, and each participant also has its own stable ID.

## Project and element objects

The project object has exactly these fields:

| Field | Type and meaning |
| --- | --- |
| `id` | Project UUIDv7 string |
| `name` | Project name string |
| `entities` | Array of entity objects |
| `attributes` | Array of attribute objects |
| `relationships` | Array of relationship objects |
| `layout` | Array of layout objects |
| `connectors` | Array of connector-shape objects; version 2 onwards |
| `colours` | Array of element-colour objects; version 9 onwards |
| `specializations` | Array of specialization objects; version 4 onwards |

An **entity** object has `id`, `name`, and `description`, all strings.

An **attribute** object has these exact fields:

| Field | Type and meaning |
| --- | --- |
| `id` | Attribute UUIDv7 string |
| `name` | Name string |
| `description` | Description string |
| `kind` | `"normal"`, `"key"`, `"composite"`, `"multivalued"`, or `"derived"` |
| `owner` | `null`, or an element reference |

Kinds are exclusive in this initial model. An attribute may belong to an entity,
a relationship, or another attribute whose kind is `"composite"`. Composite
ownership must be acyclic. A key attribute cannot be directly owned by a
relationship. Unowned attributes are valid work in progress.

A **relationship** object has `id`, `name`, `description`, `associative`, and
`participants`. The first three fields are strings, `associative` is a boolean,
and `participants` is an array of participant objects. A relationship may have
fewer than two participants while being edited.

An **associative** relationship carries its own identity and may take part in
further relationships, as an entity does; it is drawn as a diamond inside a
rectangle. A plain relationship may not be a participant target, a relationship
may not take part in itself, and associative relationships may not take part in
one another in a cycle.

Each **participant** has these exact fields:

| Field | Type and meaning |
| --- | --- |
| `id` | Participant UUIDv7 string |
| `target` | Element reference to an existing entity, or to an associative relationship |
| `maximum` | `"one"` or `"many"` |
| `participation` | `"partial"` or `"total"` |
| `role` | Role name string; may be empty |

Cardinality and participation belong to the participant record. The same entity
may occur more than once in a relationship with distinct participant IDs. Empty
or repeated role names in such a recursive relationship produce a warning.

## References and canvas layout

An **element reference** has exactly two string fields:

```json
{"type": "entity", "id": "019947b9-7111-7000-8000-000000000002"}
```

The `type` is `"entity"`, `"attribute"`, or `"relationship"`. It determines which
typed element store the ID must reference.

A **layout** object has exactly `element`, `x`, `y`, `width`, and `height`.
`element` is an element reference. The other fields are finite JSON numbers.
Coordinates are canvas units; dimensions must be positive. There must be exactly
one layout entry for every entity, attribute, and relationship. Layout references
must not dangle or repeat. Connectors are reconstructed from attribute owners and
relationship participant records.

Semantic and layout state are stored separately. Moving an element changes its
layout entry and preserves its semantic identity.

## Specializations

A **specialization** object has exactly `id`, `name`, `description`,
`direction`, `supertype`, `subtypes`, `constraint`, and `completeness`:

```json
{"id": "019947b9-7111-7000-8000-000000000004", "name": "IS A", "description": "",
 "direction": "specialization",
 "supertype": "019947b9-7111-7000-8000-000000000002",
 "subtypes": ["019947b9-7111-7000-8000-000000000005"],
 "constraint": "disjoint", "completeness": "partial"}
```

`supertype` is an entity reference, or `null` while the triangle has been
placed but not yet connected; that is work in progress rather than an error, the
same as a relationship with too few participants. Every entry of `subtypes`
references an existing entity. An entity
may not be its own subtype, may appear only once among one specialization's
subtypes, and inheritance may not form a cycle. A specialization with no
subtypes yet is valid work in progress.

`direction` is `"generalization"` or `"specialization"`. It is not only a record
of how the hierarchy was reached: the ISA triangle points at the supertype when
generalising and at the subtypes when specialising, so the direction is part of
the notation and is drawn.

`constraint` is `"disjoint"` or `"overlapping"`, and `completeness` is
`"partial"` or `"total"`. These are not decoration: together they choose which
relational mapping a later Conceptual → Relational conversion applies, so they
are stored with the model rather than treated as display state.

A specialization holds no attributes of its own; an attribute owned by one is
rejected. Deleting a supertype removes the specialization with it, and deleting
a subtype detaches it from the specializations that survive.

## Connector shapes

## Element colours

An **element-colour** object has exactly `element`, `red`, `green` and `blue`:

```json
{"element": {"type": "entity", "id": "019947b9-7111-7000-8000-000000000001"},
 "red": 255, "green": 168, "blue": 168}
```

Each channel is a whole number from 0 to 255; anything else is refused rather
than clamped, since a document that cannot say what colour it means is broken
rather than approximate. The `element` must reference an element that exists. At
most one colour may be given per element, and an element with none is drawn in
whatever colour the active theme gives its kind — which is why a document that
has never been recoloured follows the theme everywhere.

A connector is drawn from the record that creates it, so it has no identity of
its own and is addressed by that record. A **connector-shape** object has exactly
`link`, `offset`, `owner_anchor`, `child_anchor` and `waypoints`:

```json
{"link": {"type": "participant", "id": "019947b9-7111-7000-8000-000000000003"},
 "offset": 37.5, "owner_anchor": null, "child_anchor": null, "waypoints": []}
```

The `link` type is `"attribute"` for an attribute's ownership link, or
`"participant"` for one relationship participant, and the ID must reference an
existing record of that kind. An attribute link exists only while the attribute
has an owner, so a shape on an unowned attribute is invalid.

The `offset` is a finite signed perpendicular bend in canvas units, measured from
the straight line between the two endpoints. Its magnitude is bounded by the
canvas limit. An absent entry means the connector is routed automatically, so a
straightened connector stores nothing rather than an offset of zero. At most one
shape may exist per connector; a repeated link is rejected as contradictory.

The two anchors pin where the line meets each shape. They name the ends of the
drawn line rather than kinds of element: `owner_anchor` is the end the line is
drawn from, being an attribute link's owning element or a participant link's
relationship, and `child_anchor` is the far end, being the attribute or the
entity. Either may be pinned without the other. Each is a finite
angle in radians, measured from that shape's centre, and `null` when that end is
left to route itself. An angle is stored rather than a point so the join keeps
its place on the outline when the shape is moved or resized; it has no range to
exceed, so a large value is merely a wound-up angle and is accepted. A connector
that is neither bent, pinned nor routed stores nothing at all.

`waypoints` lists the corners the line is routed through, each an object with
finite `x` and `y` within the canvas limit, in the order they are met walking
from the connector's source to its target. A connector with any waypoints is
routed through them and its `offset` is not consulted; writing a route clears
the bend in the same edit, so a line never carries two accounts of its own
shape. The list is bounded by the same element limit as the rest of the
document.

Because a shape cannot outlive the record that draws it, deleting a relationship,
disconnecting a participant, or detaching an attribute removes the shape in the
same edit, and undo restores both together.

## Identity, text, and resource limits

| Item | Initial enforced limit or rule |
| --- | --- |
| File size | At most 8 MiB (8,388,608 bytes), including JSON formatting |
| Elements | At most 10,000 entities + attributes + relationships combined |
| Participants | At most 10,000 participant records across the project |
| JSON array size | At most 10,000 entries per array |
| JSON nesting | At most 32 object/array levels |
| JSON object fields | At most 16 distinct keys per object before parsing |
| Names and roles | At most 512 UTF-8 bytes each |
| Descriptions | At most 16,384 UTF-8 bytes each |
| Connector shapes | At most 10,000, and at most one per connector |
| Connector bend | Finite, between −100,000 and +100,000 |
| Canvas bounds | All rectangle edges between −100,000 and +100,000 |
| Dimensions | Greater than zero and at most 100,000 |
| IDs | Valid UUIDv7, canonical lowercase text with hyphens and no braces |

All persistent IDs are globally unique within a project, including the project
ID and every participant ID. Load restores the same IDs. Duplicate creates fresh
element and participant IDs, remaps copied references, and preserves those new
IDs across save/load and undo/redo.

Text must contain valid Unicode. Invalid UTF-8 and unpaired escaped UTF-16
surrogates are rejected. NUL, ASCII control characters, DEL, and C1 controls are
rejected; descriptions additionally permit tab, carriage return, and newline.
Names and role names are single-line text.

These limits bound the first implementation; they are not a scalability claim
for every supported diagram size. The file-size limit is independent of the
element limit: many long descriptions may reach 8 MiB before 10,000 elements.
The encoder rejects obviously oversized text before constructing Qt/JSON copies
and checks the final encoded byte count before opening the destination.

## Minimal valid file

An empty conceptual project is a valid saved draft:

```json
{
  "format": "erdflow",
  "format_version": 6,
  "project": {
    "id": "019947b9-7111-7000-8000-000000000001",
    "name": "Untitled",
    "entities": [],
    "attributes": [],
    "relationships": [],
    "layout": [],
    "connectors": [],
    "specializations": []
  }
}
```

## Open and save behavior

Open reads a bounded file, checks JSON structure and duplicate keys, parses the
supported format, then validates domain references, identity, text, kinds, and
layout. The Application replaces the active project only after the candidate
passes validation. An unsuccessful open preserves the current project, dirty
state, and undo/redo history.

Save validates and serializes before touching the destination. `QSaveFile`
writes a temporary file and commits it only after a successful complete write.
Direct-write fallback is disabled so a failed temporary-file operation does not
fall back to truncating the existing file. Failed validation or save leaves the
current editor state dirty. A successful save marks only the captured current
revision as clean; a later edit cannot be cleared by a stale save completion.

Blank names, unowned attributes, incomplete relationships, and ambiguous
recursive role labels produce warnings and can be saved. Corrupt references,
invalid IDs, ownership cycles, unsupported values, and invalid layout block save
and open. Validation does not invent or repair modeling choices.

Persistence is synchronous and bounded in this first implementation. Large-file
background serialization, cancellation, autosave/recovery, backup history, and
format migrations remain future work. Opening another project starts a new
editing session. Undo history, current selection, hover state, and other
transient UI state are not persisted.

## Verification

`tests/persistence_tests.cpp` covers exact graph and Unicode roundtrips, copied
identities, recursive participant IDs, incomplete drafts, unsupported fields and
versions, duplicate keys, malformed Unicode, invalid references and enums,
resource limits, failed-save destination preservation, and failed-open session
preservation. These tests use temporary directories and the real Qt adapter.

`connector shapes persist across versions` covers the version 2 roundtrip, a
version 1 document opening with automatic routing, the upgrade on save, and the
refusal of a document whose connector field contradicts its version.
`invalid connector shapes` covers dangling and wrongly typed links, non-finite
and out-of-range offsets, repeated links, and unknown or missing fields.

Field names are unescaped by the loader rather than by a parser call per key,
so `escaped field name decoding` pins that decoder against JSON: escaped and
plain spellings of one name collide as duplicates, a surrogate pair is refused
for being an unsupported field, and unpaired surrogates and truncated,
non-hexadecimal or unknown escapes are each refused by their own reported
reason. See the [performance baseline](PERFORMANCE_BASELINE.md) for why the
preflight avoids the parser.
