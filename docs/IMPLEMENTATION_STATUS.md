# Implementation status

**Updated:** 2026-09-16

**Scope:** Part 1 — a single-page Conceptual ERD editor foundation.

This is the record of implemented behavior. The numbered Product, Architecture,
Domain Model, Scale, and Roadmap documents also describe capabilities that have
not been built yet. Their presence in those documents is not a completion claim.

## What works

- Native Qt window with modeling toolbar, Explorer, Properties, model checks,
  status/zoom display, and a university example.
- A row of tabs above the toolbar, the way an office application arranges its
  commands: **File** drops the File menu from its tab; **Home** is the modeling
  toolbar, with Note after Connect; **Insert** carries Picture; **Design** carries Theme,
  Icons, Notation and Lines; **View** carries the panels, framing, grid and
  align-to-grid; **Help** carries the guide and About. The rows are built from the same
  actions as the menus and the Home toolbar, so a tool chosen or locked on one
  row is chosen or locked on the other. Convert and Export tabs wait until
  there is something to convert or export.
- Pictures and notes as visual aids on the canvas. **Insert → Picture…** places
  an image from a file (PNG and JPEG bytes are kept as they are; other formats
  and large images are re-encoded, scaled to at most 1024 pixels) in the
  middle of the view; **Note**, on the Home toolbar after Connect, places a
  note by a click and opens it for its title, with its text edited in
  Properties. Both are also offered by the canvas's right-click menu, placed
  at the point clicked. Both are moved, coloured,
  duplicated, deleted, undone and saved like elements, appear in the explorer
  under groups of their own, and are not database objects: nothing connects to
  them and no attribute belongs to them. Project files are version 11.
- Connections join where they are clicked. With **Connect ▾ → Join where I
  click** (the default, remembered between sessions) each end of a new line is
  pinned to the point clicked on its shape, in the same edit as the connection;
  **Join automatically** restores the sliding joins. A selected line carries a
  square grip on each end that can be dragged to any point on the same shape,
  and two lines may leave one point. The padlock still releases a line's joins.
- Six themes covering both the application chrome and the diagram, chosen under
  **View → Theme** and remembered between sessions. **View → Icons** chooses
  between glyphs painted from the active theme and embedded SVG artwork with
  light/dark variants; the icon choice is also remembered.
- Entity rectangles, attribute ovals, relationship diamonds, names and descriptions.
- Stable typed UUIDv7 IDs for projects, entities, attributes, relationships, and
  each relationship participant. Rename and undo preserve identity.
- Entity, relationship, and composite-attribute ownership of attributes; key
  underline, composite children, multivalued double oval, derived dashed oval.
- Per-participant `1`/`M` cardinality and partial/total participation. These
  represent `0..1`, `1..1`, `0..M`, and `1..M`.
- A relationship side's context menu edits its constraints and can hide their
  drawing without changing the stored cardinality or participation. Visibility
  is undoable and saved with the participant.
- A binary relationship's ratio — `1:1`, `1:M`, `M:1`, `M:M` — is pickable
  directly in Properties, writing both sides in one edit, with a **Reverse sides**
  button that swaps their constraints. Every notation reads the same participant
  records, so the picker and the drawing cannot disagree. A relationship with
  other than two sides keeps only its per-side controls.
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
- Selecting an element draws every link touching it heavier and lifts it above
  the other links, so what it connects to can be read at a glance.
- Single/multiple/rubber-band selection, Select All, zoom (mouse wheel, or a
  trackpad pinch: apart to zoom in, together to zoom out), pan (two fingers
  travelling together, the middle mouse button, or the hand), fit, grid and align to grid,
  selection navigation, and cancellation of an uncommitted drag.
- One drag or duplicate/delete group is one history entry. Deletion restores
  dependent attributes and participant records on undo. Duplicate generates new
  IDs, including participant IDs, and remaps the copied subgraph.
- Generalization and specialization: one **ISA** toolbar entry offers both
  directions from its dropdown, which choose only which way the triangle points.
  The triangle is placed on the canvas like any other element and wired up by
  hand: the first entity connected to it is the one it generalises, and every
  entity connected after that becomes a subtype. Either can be detached again
  in Properties, and a triangle waiting to be connected is work in progress
  rather than an error. The ISA triangle points the way the hierarchy is read,
  up at the supertype when generalising and down at the subtypes when
  specialising, so the direction is stored and can be changed in Properties.
  Inheritance links are anchored to the triangle's points — the supertype at its
  top, the subtypes at its bottom — and curve to reach whatever they connect,
  so moving an entity bends the line instead of sliding the attachment.
  Each carries a disjoint/overlapping constraint and total/partial completeness,
  the two inputs a later conversion needs to choose a relational mapping.
  Hierarchies nest, and inheritance cycles are rejected.
- Every label in Properties is bold and written in the theme's own ink: the
  words and the controls beside them already tell the rows apart, so colour is
  kept for what carries meaning. The heading naming the kind being edited
  stays a title in the theme's accent. The colour an element is drawn in on
  the diagram, the one its owner chose or the one its theme gives its kind,
  fills the shape its name is written in: the Name field is the element's own
  shape, at the proportions it is drawn with on the diagram, with the name
  inside it in ink chosen against that colour. A shape asks for the width its
  name needs and takes what the panel has, so nothing runs out of the panel at
  any width: a name too long for the room is shown from its start, and on a
  card it is elided. Each end of a card is the same again, smaller: a rectangle for an
  entity, an oval for an attribute, a diamond for a relationship, a triangle
  for an ISA, with the double border of a weak entity, the second diamond of
  an identifying one, the box of an associative one, and a picture's own
  picture. It is the canvas's drawing, so the panel and the diagram cannot
  disagree, and it carries a colour the user chose, which the icon sets cannot.
  A card names both ends of what it is about, as **Student → WorkOn**: a side
  means nothing on its own, it is that element as it takes part in this one. The panel is redrawn when the theme changes.
- Elements line up with their neighbours as they are dragged, the way a page
  layout does. An edge or a middle that comes within a few pixels of another
  element's meets it exactly, and a thin guide is drawn along what the two now
  share. Nothing is pulled out of place when nothing is near. A selection can
  also be lined up at once from the canvas's right-click **Align**, on left
  edges, centres, right edges, top edges, middles or bottom edges, as one edit.
- Lines break at right angles. **Right-angle lines** is what the canvas draws
  unless told otherwise, chosen on Connect's own arrow beside **Curved** and
  **Straight**: a line leaves each shape squarely to the face it meets, turns,
  and comes in to the other the same way, so two elements standing in line are
  joined by one clean run rather than by a kink. Where an end has been pinned
  by clicking, the line leaves that point; where it has not, it leaves the
  middle of the face that looks at the other shape. The shape is worked out
  rather than stored, so it follows both elements as they move, and it becomes
  an ordinary route the moment the line is taken hold of.
- A line's end can be carried away from its shape. Dragging an end grip across
  its own shape moves the join around the outline as before; carried past the
  shape and let go in open canvas, the end stops there, leaving a corner at
  that point rather than the gesture coming to nothing. The join and the
  stopping point are written as one edit.
- Each side's number and role are drawn beside the shape they belong to and
  clear of the line, rather than the role sitting in the middle of the line
  and cutting it. They are laid out along the line as it actually arrives,
  which for a right-angle line is its last segment rather than its middle.
- Recursive relationships. An entity's Properties says whether it **relates to
  itself**; ticking it creates the relationship, with both sides on that
  entity, in one edit, and clearing it takes those relationships away. A
  relationship meeting the same entity twice is drawn the way it is drawn by
  hand: the first side runs straight to the diamond, and the second leaves the
  far corner and returns around the entity at right angles into another of its
  faces. The loop keeps to whichever side carries fewer of the entity's
  attributes, follows both shapes as they are moved, and stores nothing until
  the line is taken hold of, when its corners become an ordinary route to
  shape. Double-clicking hands it back.
- Connecting two entities creates the relationship they mean. Two entities have
  no line of their own in Chen notation, so rather than refusing the pair,
  **Connect** creates a relationship midway between them, joins both sides to
  it, selects it and opens it for its name, saying so in the status bar. A pair
  may be read more than once, as Employee both works on and manages a Project:
  each new relationship steps aside along the perpendicular to the line joining
  the two entities, alternating sides, so it does not land on the one already
  there, and is then an element like any other to drag or place by its
  coordinates. The
  relationship, both of its sides and their joins are one edit, so one undo
  takes the whole thing back.
- Weak entities and identifying relationships: an entity's **Kind** in
  Properties is Regular or Weak, and a relationship's is Regular, Identifying
  or Associative. A weak entity is drawn with a double border and its key
  attribute as a partial key with a dashed underline; an identifying
  relationship as a double diamond. A weak entity without an identifying
  relationship, or the reverse, is a warning, not a fault. The ISA triangle
  wears **d** or **o** for disjoint or overlapping, and a total specialization
  draws its link to the supertype as a double line. Project files are version 13.
- Associative entities: choose Associative as a relationship's kind in Properties and it
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
- Attribute links leave their owner from one shared point per side, run a short
  trunk and branch from there to each attribute, so a cluster of attributes reads
  as one connection fanning out rather than several meeting the body separately.
- Connectors are drawn curved or straight, chosen from the arrow on the Connect
  tool itself, with each option drawn as a sample rather than only named.
- Connector shaping for attribute and participant links: select a link and drag
  its handle to bend it, or drag the selected line to add route corners. Corners
  can be moved and removed individually; double-clicking the line clears its
  route or bend. The padlock pins or releases both endpoint joins. These changes
  are undoable, saved, and removed with their link. ISA links retain their fixed
  triangle anchors. Dragging nodes follows the pointer; align to grid is opt-in.
- Element colours can be set for one element or a selection from the context
  menu, using a swatch or custom colour, and reset to the theme colour. A
  **Transparency** bar, below the colours in that menu, fades
  the surface from solid to outline-only over whatever colour it has, the
  theme's own included, for one element or the whole selection. Colour
  changes are undoable and saved with the project.
- Versioned `.erdx` JSON save/load, currently format version 10. Versions 1 to 9
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
| 8 — Participation | Implemented partial/total and one/many combinations on the same participant, including a min–max notation option. |
| 9 — Advanced attributes | Partial: composite, multivalued, derived and partial keys (on weak entities) implemented. Combined kind semantics remain open. |
| 10–12 — Weak/ISA/associative | Implemented: weak entities with identifying relationships, associative entities, and ISA generalization/specialization including nesting and the disjoint/total rules, drawn on the triangle. |
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
group align-to-grid/bounds; stale gesture cancellation; and incident-only live connector updates.

## Still to build in Part 1

Logical metadata and conversion readiness;
multiple pages; cross-project clipboard; alignment/distribution; drag resize
handles; freely draggable connector endpoint handles; search; export;
autosave and recovery. Connector joins can be pinned at their current positions,
and attribute/participant links support multi-point routes. Attribute kinds currently
form one exclusive enum, so composite keys or other combinations require a
deliberate model/format decision.

Undo history has a conservative 32 MiB accounting budget; older entries are
removed when it is reached, and a single larger edit is rejected. This estimates
history storage, not whole-process memory. The session's issued-ID registry also
grows with newly allocated IDs until New/Open. Files are limited to 8 MiB, with
10,000 elements and 10,000 participants. These are resource limits, not promises
that every maximum-size diagram is already smooth.
