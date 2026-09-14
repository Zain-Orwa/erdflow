# Implementation status

**Updated:** 2026-09-14  
**Scope:** Part 1 — a single-page Conceptual ERD editor foundation.

This is the record of implemented behavior. The numbered Product, Architecture,
Domain Model, Scale, and Roadmap documents also describe capabilities that have
not been built yet. Their presence in those documents is not a completion claim.

## What works

- Native Qt window with modeling toolbar, Explorer, Properties, model checks,
  status/zoom display, and a university example.
- Entity rectangles, attribute ovals, relationship diamonds, names and descriptions.
- Stable typed UUIDv7 IDs for projects, entities, attributes, relationships, and
  each relationship participant. Rename and undo preserve identity.
- Entity, relationship, and composite-attribute ownership of attributes; key
  underline, composite children, multivalued double oval, derived dashed oval.
- Per-participant `1`/`M` cardinality and partial/total participation. These
  represent `0..1`, `1..1`, `0..M`, and `1..M`.
- Four notations for reading those bounds, chosen from the toolbar picker or
  **View → Notation**, and
  drawn per participant end so each side stays correct as entities are moved:
  Chen (`1`/`M` with a doubled line for total participation), min–max (`(0,M)`),
  crow's foot (maximum against the entity, minimum just inboard), and Bachman
  (arrowhead for many, filled or hollow circle for mandatory or optional).
  The notation is a display choice and is not stored in the project file.
- Recursive and multi-participant relationships with independent participant IDs
  and editable roles. Cardinality labels follow their entity endpoint.
- Create, rename, describe, move, set size through Properties, delete, duplicate,
  connect/disconnect, change owner/kind/cardinality/participation, and undo/redo.
- Tools are one-shot by default: a single click on a toolbar tool places one
  element and returns to Select. Double-clicking the tool locks it, marking the
  button with a padlock, so it keeps placing until another tool is chosen or
  Escape is pressed. Select cannot be locked.
- Single/multiple/rubber-band selection, Select All, zoom, pan, fit, grid/snap,
  selection navigation, and cancellation of an uncommitted drag.
- One drag or duplicate/delete group is one history entry. Deletion restores
  dependent attributes and participant records on undo. Duplicate generates new
  IDs, including participant IDs, and remaps the copied subgraph.
- Generalization and specialization: one **ISA** toolbar entry offers both
  directions from its dropdown. Specialization works top-down — click the entity
  to specialise and connect subtypes afterwards. Generalization works bottom-up —
  select the subtypes, then click the entity that generalises them, and they are
  adopted in one gesture. Both build the same structure.
  Each carries a disjoint/overlapping constraint and total/partial completeness,
  the two inputs a later conversion needs to choose a relational mapping.
  Hierarchies nest, and inheritance cycles are rejected.
- Associative entities: mark a relationship associative in Properties and it
  takes the entity body size and palette, since it converts to a relation of its
  own, and is drawn as a filled diamond inside an unfilled rectangle, which is
  what keeps it distinguishable from a solid entity. It may then take
  part in further relationships, as an entity does. Adopting or dropping the
  shape resizes it in the same undoable edit.
- Names can be edited in two places: double-click an element to type its name on
  the canvas itself, or use the Properties panel. Return or clicking away commits
  as one undoable rename; Escape keeps the previous name.
- Connecting works either as one press-drag-release gesture or as click-then-click.
  While connecting, a dashed line follows the pointer from the source and a valid
  drop target is outlined. Creating an attribute while one element is selected
  attaches it to that owner directly.
- Connector shaping: select a link, drag its handle to bend it aside, double-click
  to straighten. The bend is one undoable edit, is saved, and is dropped with the
  link it belongs to. Dragging nodes follows the pointer; snap to grid is opt-in.
- Versioned `.erdx` JSON save/load, currently format version 4. Versions 1 to 3
  still open and upgrade on save. Incomplete but structurally valid diagrams
  can be saved. Invalid/unsupported files leave the open project intact.
- Safe file replacement, Save/Discard/Cancel protection, focused text committed
  before save, clean-state tracking, and reopening the file saved at the prompt.

The example is available using **Open example** or `build/erdflow --example`.
The normal startup is an empty project. Text properties commit on focus loss;
geometry changes use **Apply position and size**.

## Roadmap coverage

| Phase | Evidence and remaining work |
| --- | --- |
| 0 — Architecture | Completed review; ADR-001–013 accepted. ADR-014 records implementation choices. |
| 1 — Build foundation | Exit criteria met: layered CMake targets, warning flags, Debug/Release, passing suites, macOS launch, and committed repository state. Windows/Linux instructions exist but those platforms are unverified. |
| 2 — Shell | Functional desktop shell delivered. Future Schema/Table/SQL/Data navigation waits for usable destinations. |
| 3 — Commands | Implemented Qt-free semantic operations, atomic deltas, dirty state, bounded undo/redo. |
| 4 — Minimal domain | Implemented and independently tested without Qt. |
| 5 — Canvas | Implemented with QGraphicsView; incremental projection, incident-only connector updates while dragging, measured prototype. |
| 6 — Basic Chen | Implemented; university example and desktop workflow tests. |
| 7 — Cardinality | Implemented per participant, with correct entity-side labels. |
| 8 — Participation | Implemented partial/total and one/many combinations on the same participant. A separate min/max text notation toggle is not implemented. |
| 9 — Advanced attributes | Partial: composite, multivalued, derived implemented. Partial keys and combined kind semantics remain open with weak entities. |
| 10–12 — Weak/ISA/associative | Partial: associative entities and ISA generalization/specialization implemented, including nesting and the disjoint/total rules. Weak entities and identifying relationships remain. |
| 13–14 — Modes/readiness | Basic properties and structural checks exist. Convertible mode, logical types, key groups, and conversion-readiness policy are not implemented. |
| 15 — Project files | Single-page native format foundation delivered early to protect the current editor's work. No historical migration or recovery system. |
| 16–17 — Pages/editor milestone | Not complete; multiple pages and the remaining conceptual semantics are required. |
| 18 onward | Import, schema generation, provenance, physical design, SQL, data, and later production/ecosystem features remain planned. |

The Part 1 checklist is a coverage inventory, not a replacement for semantic
prerequisites. Persistence was deliberately pulled into this usable slice so
newly drawn diagrams can survive application shutdown. This does not advance
conversion, SQL, AI, or data features ahead of the roadmap.

## System qualities in this implementation

| Requirement | Concrete evidence | Current limit |
| --- | --- | --- |
| Sustainable | C++20 and Qt; no additional runtime dependency or speculative framework. | Packaging and dependency distribution review remain future work. |
| Scalable | Ordered ID maps, incident-edge lookup, incremental canvas projection, bounded history and input. | Full validation and Explorer rebuild still run at command boundaries; large operations are synchronous. |
| Secure | Strict field/version/type/Unicode/reference checks; duplicate JSON key rejection; size/depth/count limits; atomic save replacement. | This is not a completed production security audit. |
| Maintainable | Qt-free Domain/Application, injected persistence/ID ports, desktop assembly in `main.cpp`, tests at each boundary. | Full future Domain design has deliberately not been implemented. |
| Usable | Focused property editing, named undo actions, usable example, warnings that permit unfinished drafts, protected open/save. | Screen-reader support, broader keyboard/high-DPI/device checks, and production polish remain unverified. |

## Verification

All six CTest suites passed in Debug, Release, and AddressSanitizer +
UndefinedBehaviorSanitizer builds. They cover the core, real persistence adapter,
canvas interactions, themes, desktop workflows, and application startup. See [development instructions](DEVELOPMENT.md)
for reproducible commands and [performance baseline](PERFORMANCE_BASELINE.md) for
measurements. GUI checks currently use Qt's offscreen platform on macOS; the
rendered example was visually inspected. This does not prove native dialogs,
platform accessibility, or interaction performance on Windows/Linux.

Useful regression checks include identity-preserving undo/redo; atomic rejection
without losing the redo branch; owned-graph deletion/duplication; participant
roles; hostile file inputs; preserving existing files on failed saves; cancelling
open; saving an active property field; save-then-reopen; Shift group selection;
group snap/bounds; stale gesture cancellation; and incident-only live connector updates.

## Still to build in Part 1

Weak entities/identifying relationships/partial keys; logical metadata and
conversion readiness;
multiple pages; cross-project clipboard; alignment/distribution; drag resize
handles; connector endpoint handles and multi-point routing; search; export;
autosave and recovery. Connectors carry one bend each, which is enough to route
around an overlap but not to follow an arbitrary path. Attribute kinds currently
form one exclusive enum, so composite keys or other combinations require a
deliberate model/format decision.

Undo history has a conservative 32 MiB accounting budget; older entries are
removed when it is reached, and a single larger edit is rejected. This estimates
history storage, not whole-process memory. The session's issued-ID registry also
grows with newly allocated IDs until New/Open. Files are limited to 8 MiB, with
10,000 elements and 10,000 participants. These are resource limits, not promises
that every maximum-size diagram is already smooth.
