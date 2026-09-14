# ERDX project format — version 1

**Status:** Implemented initial Conceptual ERD format  
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
| `format_version` | JSON number, exactly `1` |
| `project` | Project object described below |

This is the first implemented public project format. There is no earlier public
format to migrate. A file with an unsupported version, missing field, unknown
field, duplicate JSON object key, or invalid value is rejected. Escaped-equivalent
keys, such as `"name"` and `"na\u006de"`, count as duplicates.

The same strict field rule applies to every nested object. Unknown fields are
neither ignored nor removed. A future format change must define its compatibility
and migration policy explicitly before writing a new version.

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

A **relationship** object has `id`, `name`, `description`, and `participants`.
The first three fields are strings; `participants` is an array of participant
objects. A relationship may have fewer than two participants while being edited.

Each **participant** has these exact fields:

| Field | Type and meaning |
| --- | --- |
| `id` | Participant UUIDv7 string |
| `entity` | UUIDv7 string referencing an existing entity |
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
  "format_version": 1,
  "project": {
    "id": "019947b9-7111-7000-8000-000000000001",
    "name": "Untitled",
    "entities": [],
    "attributes": [],
    "relationships": [],
    "layout": []
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

Field names are unescaped by the loader rather than by a parser call per key,
so `escaped field name decoding` pins that decoder against JSON: escaped and
plain spellings of one name collide as duplicates, a surrogate pair is refused
for being an unsupported field, and unpaired surrogates and truncated,
non-hexadecimal or unknown escapes are each refused by their own reported
reason. See the [performance baseline](PERFORMANCE_BASELINE.md) for why the
preflight avoids the parser.
