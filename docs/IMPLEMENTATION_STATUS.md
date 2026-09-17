# Implementation status

**Updated:** 2026-09-17

**Scope:** Part 1 — a single-page Conceptual ERD editor foundation.

This is the record of implemented behavior. The numbered Product, Architecture,
Domain Model, Scale, and Roadmap documents also describe capabilities that have
not been built yet. Their presence in those documents is not a completion claim.

## What works

- The diagram has paper of its own, chosen under **View → Background** and so
  also on the Design row: **None**, the plain canvas its theme gives it;
  **Squares**, like graph paper; **Lines**, ruled like a notebook; **Dots**; or
  **Picture…**, one of the user's own drawn once behind the diagram, covering
  the view and fixed to it, so zooming never magnifies it. A background
  picture is kept at up to 2560 pixels, at the best size a project file has
  room for. A picture
  carries a **Strength** bar saying how much of it shows, from full down to
  none, which is what holds it behind the diagram rather than in front; a
  ruling is drawn as the ruling it is and is offered no such bar. While a
  ruling is in use the editing grid's own dots step aside. The paper travels
  with the document rather than with the application, unlike the theme: a
  diagram drawn on graph paper opens on graph paper. Project files are
  version 14.
- A selection's lines can be locked or released together. Right-click an
  element and **Lock these connectors** pins every line the selection touches
  where it is drawn now, so they stop sliding as the shapes are moved;
  **Release** hands the joins back. Each is offered only while it has something
  to do, and covers whatever is selected: one element, a region, or, from the
  menu on empty canvas, every connector in the diagram. A line's bend and
  corners are left as they are, and the whole set is one edit however many
  lines it covers. Inheritance links are left out, since they are anchored to
  their triangle and have no joins to pin.
- A choice or a number never changes because the pointer passed over it. A
  wheel or a trackpad turn over a combo box or a spin box goes to the panel
  behind it, so the panel scrolls and the value stays; values change by
  pressing the control and choosing, or by typing. This holds for the
  notation picker on the toolbar as well.
- **Check model** is a switch: it opens the findings and puts them away again,
  wearing a green tick while they are closed and a red cross while they are
  open. It follows the panel however that is opened or closed, including from
  the View menu and from Full view.
- Native Qt window with modeling toolbar, Explorer, Properties, model checks,
  status/zoom display, and a university example.
- A row of tabs above the toolbar, the way an office application arranges its
  commands: **File** drops the File menu from its tab; **Home** is the modeling
  toolbar, with Note after Connect; **Insert** carries Picture and Symbols; **Design** carries Theme,
  Icons, Notation and Lines; **Export** carries the picture commands; **View**
  carries the panels, framing, grid and
  align-to-grid; **Help** carries the guide and About. The rows are built from the same
  actions as the menus and the Home toolbar, so a tool chosen or locked on one
  row is chosen or locked on the other. The Export entries go quiet while there
  is nothing drawn to make a picture of, rather than the row coming and going as
  work starts. A Convert tab still waits, because there is nothing to convert to.
- **Insert → Symbols…** opens a gallery of the characters a conceptual diagram
  wants and a keyboard has not got: relational algebra (select, project,
  rename, the six joins, union, intersection, difference, product, division),
  logic and sets, arrows, mathematics, Greek letters, punctuation and marks,
  about 130 emoji, and the people an ERD is usually about: men, women, older
  people, children and babies, whole standing figures, and the roles an entity
  is named after, with man and woman forms of the common ones — student,
  teacher, health worker, office worker, technologist and scientist. Every
  character is named, and the search reaches across all eight
  groups at once, so the natural join is found by typing "join" rather than
  hunted for by eye. A character goes into whatever field was last being
  written in, at the caret, and the gallery stays open so a caption can take
  several. "Being written in" means where the keyboard actually is, so a
  character never lands in a box that has closed or in a field the user never
  went to. Committing a name rebuilds the properties panel, so the caret goes
  back to where the writing stopped rather than in front of the name. With
  nothing being written in, the character goes on the diagram instead, drawn
  bare: no card, no border and no title, the way an emoji sits in a line of
  chat. It is sized to the box it is given, so resizing the box resizes the
  character, and a chosen colour is the colour it is written in. A symbol is
  the one element with a size of its own to choose, and there are three ways to
  choose it: its four corner grips, hauled by hand and keeping its proportions
  with the opposite corner anchored; **Edit → Enlarge symbol** and **Shrink
  symbol** (Ctrl+Shift+= and Ctrl+Shift+-, and the same pair on the canvas's
  right-click menu), which step it by a quarter about its own centre; and a
  **Size** field in the properties panel for an exact figure. All three go
  through one named edit, "Resize symbol", so any of them undoes in a step.
  Sizes are held between 16 and 4000 units, a drag in progress is previewed on
  the diagram and written only when the grip is let go, and Escape abandons it.
  These act only when everything selected is a symbol: an entity's box is sized
  by the name it has to hold, so the commands stay disabled for one. It lands
  where the pointer last was over the diagram rather than in the middle of the
  view, and is left unchosen, so picking several in a row does not keep
  swapping the properties panel over to them; one that would land exactly on
  another steps down and across until it finds room. It is moved, coloured,
  copied, deleted and undone like any other element, and underneath it is a
  note marked plain, which is what the file records, so project files are
  version 15. The gallery is a tool window that never takes activation, so
  the field being written in keeps its caret while its character is chosen.
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
- Nineteen themes covering both the application chrome and the diagram, chosen under
  **View → Theme** and remembered between sessions. **View → Icons** chooses
  between three sets, and the choice is remembered: Outline, the default, is
  single-weight line art inked from the active theme, with a second inking for
  a tool that is on so its glyph reads against the accent it sits on; Modern is
  coloured artwork with light and dark variants; Painted is glyphs drawn from
  the theme with no files at all, and stands in whenever a file cannot be read.
  The outline set is Lucide (ISC) for the ordinary commands and ERDFlow's own
  drawings, on the same grid and stroke weight, for the Chen shapes Lucide has
  no icon for.
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
- Versioned `.erdx` JSON save/load, currently format version 15. Versions 1 to
  14 still open and upgrade on save. Incomplete but structurally valid diagrams
  can be saved. Invalid/unsupported files leave the open project intact.
- Safe file replacement, Save/Discard/Cancel protection, focused text committed
  before save, clean-state tracking, and reopening the file saved at the prompt.
- Work leaves ERDFlow as a picture. An **Export** tab and an Export menu under
  File write the diagram as **SVG**, **PNG**, **JPEG**, **WebP**, **TIFF** or a
  **PDF** page, and **Copy as picture** puts the selection, or the whole diagram
  when nothing is selected, on the clipboard as a raster and a vector at once so
  whatever it is pasted into takes whichever it prefers. A format this build has
  no writer for is not offered rather than offered and then failed.
- The options that decide whether an exported picture is usable ship with it
  rather than after it: the **extent** (whole diagram, selection or current
  view), the **background** (transparent, the theme's canvas colour, or white),
  the **scale** for a raster or the **resolution** for a page, and the **margin**
  left around the diagram. The dialog says what pressing Export will produce, in
  the units that format is measured in, and turns off what cannot be asked for:
  an extent with nothing in it, and carrying the project in a format that cannot
  hold one. A picture larger than ERDFlow will draw is refused with its size
  rather than attempted.
- A picture is of the diagram and not of the editor looking at it: no grid, no
  paper, no selection rings and no handles, and exporting gives the selection
  back exactly as it found it.
- An exported file carries nothing tying it to the machine that wrote it. Fonts
  are the one thing SVG cannot carry cheaply, so its text names a chain ending
  in a CSS generic family — `'Helvetica Neue', Helvetica, Arial, 'Liberation
  Sans', sans-serif` — rather than Qt's own `Sans Serif`, which no CSS engine
  resolves. The reader uses the first of those its machine has, and the generic
  at the end exists everywhere. This keeps the text selectable and searchable at
  the cost of exact metrics on a machine without the first font; the
  outline-text option that would trade the other way is not implemented.
- **SVG and PNG carry the whole project inside them**, in an SVG `metadata`
  element and a PNG text chunk respectively, both of which every other reader of
  those formats ignores. One file is then the picture a recipient opens and the
  project ERDFlow reopens without loss: **Open** accepts `.svg` and `.png`
  beside `.erdx`. The payload is the same bytes `.erdx` holds at the same
  version, so one reader serves both. A project opened out of a picture has no
  project file of its own, so the next save asks where it should go rather than
  overwriting the picture. A project too large to travel inside a picture is
  written as a picture without it and says so; a picture ERDFlow did not write
  is reported as a picture carrying no project, not as a damaged one. The lossy
  and niche formats carry no payload at all.

The example is available using **Open example** or `build/erdflow --example`.
It is the university diagram in full: Student, Course and Professor, the
Enrolled, Teaches and Mentor relationships between them, and one of every kind
of attribute — a key on each entity, a composite name with First, Mid and Last
hanging off it, a derived age, a multivalued phone, and an enrollment date that
belongs to the relationship rather than to either side of it.
The normal startup is an empty project. Text properties commit on focus loss;
geometry changes use **Apply position and size**.

## Roadmap coverage

| Phase | Evidence and remaining work |
| --- | --- |
| 0 — Architecture | Completed review; ADR-001–013 accepted. ADR-014 records implementation choices. ADR-015 records export and interchange formats, whose picture half is now implemented and whose relational half is not; ADR-016 records project organisation and the start experience, which is not implemented. |
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
| 16–17 — Pages/editor milestone | Not complete; multiple pages, the remaining conceptual semantics, and the start screen, templates and project folders of ADR-016 are required. |
| 18 onward | Import, schema generation, provenance, physical design, SQL, data, and later production/ecosystem features remain planned. |
| 35 — Export | Partial, and pulled forward the way Phase 15 was, because ADR-015 splits it in two and the picture half needs only a canvas that draws. Pictures, their options, and the project carried inside SVG and PNG are implemented. Not implemented: a multi-page PDF of a project, which waits for the multiple pages of Phase 16; the HTML report, Markdown data dictionary and CSV listings; the published JSON Schema for `.erdx`; the outline-text option for a pixel-exact handoff; and the whole schema half — `.sql`, Mermaid ER and DBML — which cannot precede the Phase 24 workspace it would read from. |

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

Useful regression checks include picture export — that a picture never shows
the selection rings of the editor that drew it, that its background and size are
the ones asked for, that SVG and PNG give back exactly the project bytes they
were given while still parsing and still drawing, and that an exported picture
opens again as the project it carries; identity-preserving undo/redo; atomic rejection
without losing the redo branch; owned-graph deletion/duplication; participant
roles; hostile file inputs; preserving existing files on failed saves; cancelling
open; saving an active property field; save-then-reopen; Shift group selection;
group align-to-grid/bounds; stale gesture cancellation; and incident-only live connector updates.

## Still to build in Part 1

Logical metadata and conversion readiness;
multiple pages; cross-project clipboard; alignment/distribution; drag resize
handles; freely draggable connector endpoint handles; search; the documentation
and relational halves of export; autosave and recovery. Connector joins can be pinned at their current positions,
and attribute/participant links support multi-point routes. Attribute kinds currently
form one exclusive enum, so composite keys or other combinations require a
deliberate model/format decision.

Undo history has a conservative 32 MiB accounting budget; older entries are
removed when it is reached, and a single larger edit is rejected. This estimates
history storage, not whole-process memory. The session's issued-ID registry also
grows with newly allocated IDs until New/Open. Files are limited to 8 MiB, with
10,000 elements and 10,000 participants. These are resource limits, not promises
that every maximum-size diagram is already smooth.
