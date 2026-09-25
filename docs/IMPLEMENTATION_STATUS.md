# Implementation status

> **Settled work.** Do not change, replace or re-style anything in this
> document to suit something new you have been asked to build. If what you are
> building genuinely contradicts what is written here, stop and ask Zain, who
> owns this project: say what you want to change, what the application will
> look like afterwards, and whether it is a gain or a loss. He decides.
> Full rule: [CLAUDE.md](../CLAUDE.md).

**Updated:** 2026-09-23

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
  version 16.
- A selection's lines can be locked or released together. Right-click an
  element and **Lock these connectors** pins every line the selection touches
  where it is drawn now, so they stop sliding as the shapes are moved;
  **Release** hands the joins back. Each is offered only while it has something
  to do, and covers whatever is selected: one element, a region, or, from the
  menu on empty canvas, every connector in the diagram. A line's bend and
  corners are left as they are, and the whole set is one edit however many
  lines it covers. Inheritance links are left out, since they are anchored to
  their triangle and have no joins to pin.
- Turning the wheel, or dragging two fingers, **moves** the diagram; holding the
  platform's own zoom key — **⌘** on a Mac, **Ctrl** elsewhere — and turning
  **zooms** it, about the pointer, so what is under the pointer stays under it.
  A trackpad pinch zooms as it always did. The wheel used to zoom on its own,
  but only on a device that reported no scroll phase, so the same turn of the
  same wheel zoomed on one machine and scrolled on another depending on what the
  driver chose to say; with a key of its own for zooming there is no longer
  anything to guess at. The raft's **+** and **−** say so in their tooltips,
  naming the key the way the platform names it.
- A choice or a number never changes because the pointer passed over it. A
  wheel or a trackpad turn over a combo box or a spin box goes to the panel
  behind it, so the panel scrolls and the value stays; values change by
  pressing the control and choosing, or by typing. This holds for the
  notation picker on the toolbar as well.
- A row of a dropped-down list lights up under the pointer, the way a row of the
  Explorer and an entry of a menu do, so it is plain which one a press would
  take. The sample drawn beside it is inked from the surface it is actually
  standing on: a highlighted row is painted in the theme's accent, and a sample
  drawn in that same accent would vanish into it, so a highlighted row's sample
  is drawn in whatever reads on the accent instead. This is decided from the
  state the row is really painted in rather than from a list's own idea of what
  is selected, because under a stylesheet the two can disagree. The notation
  samples that write their pair — Chen's **M** and min–max's **(1,M)** — keep
  room of their own at the end, so the line stops short of the writing rather
  than running through the characters the reader is being shown.
- **Check model** is a switch: it opens the findings and puts them away again,
  wearing a green tick while they are closed and a red cross while they are
  open. It follows the panel however that is opened or closed, including from
  the View menu and from Full view.
- Native Qt window with modeling toolbar, Explorer, Properties, model checks,
  status/zoom display, and a university example.
- The small raft of view controls on the diagram — full view, fit, pan and zoom
  — can be **moved and put away**. It is dragged by a grip at its top, because
  every button on it does something when pressed and it needs somewhere to be
  taken hold of that is not one of them. Where it is put is remembered as a
  fraction of the view, so resizing the window keeps it where it was rather than
  letting it drift towards a corner, and it is held inside the view however far
  it is pushed, so it can never end up off the side where nothing could reach
  it. Right-clicking it offers to put it away; right-clicking the diagram then
  offers it back, as does **View → View controls on the diagram**, and that
  offer appears only while it is away. It is shown to begin with.
- Fitting the diagram into the view and searching it are drawn as different
  things. In the coloured set they were the same file: a magnifying glass for
  both, saying "look" for one and "look" for the other. Fitting is now a frame
  with a mark in each corner, which is what the line-art set and the drawn set
  already said, so all three agree.
- Every Explorer row is drawn as **the element itself** rather than as a badge
  for its kind, using the canvas's own drawing: a derived attribute is dashed
  there as it is on the diagram, a multivalued one is doubled, a weak entity
  wears its second border, a relationship is a diamond and an identifying one a
  double diamond. The rows are drawn large enough for those differences to be
  told apart at a glance, which is the whole reason for drawing the element
  rather than its kind. Pointing at a row says the same thing in words —
  **Derived attribute**, **Multivalued attribute**, **Weak entity**, **Partial
  key** — for anyone pointing rather than reading the shape. A key attribute is
  the one kind the shape cannot show, since in Chen notation a key is the
  underline beneath its name and a row's drawing carries no name; the words say
  it instead.
- **How many attributes belong to a row** is written at the end of it, quietly,
  in the same column for every row that has one — on an entity, on a
  relationship that carries attributes, and on a composite attribute whose parts
  hang off it. It is painted beside the name rather than written into it, since
  a number inside a name reads as part of what the element is called. The group
  rows count the same way, so the tree counts in one place and one way, and a
  row with nothing under it carries no number at all rather than a nought.
- The Explorer's fold marks stand against its **right-hand edge** rather than
  in front of each row. The panel is on the left of the window and the diagram
  fills the middle, so the hand comes back from the canvas to the panel's near
  edge: a mark in front of a row means crossing the whole width of the panel to
  open a group and crossing back to carry on, while one against the right edge
  is the first thing reached. Every group folds from the same column whatever
  depth it sits at, and the indentation is unchanged, since that is what says
  what belongs to what. Pressing a mark folds that group and does nothing else,
  so reaching for one never throws away the selection being worked with.
- A row of tabs above the toolbar, the way an office application arranges its
  commands: **File** drops the File menu from its tab; **Home** is the modeling
  toolbar, with Note after Connect; **Insert** carries Picture and Symbols; **Design** carries Theme,
  Icons, Notation and Lines; **Export** carries everything that leaves and
  **Import** everything that comes back, side by side;
  **View** carries the panels, framing, grid and
  align-to-grid; **Help** carries the guide and About. The rows are built from the same
  actions as the menus and the Home toolbar, so a tool chosen or locked on one
  row is chosen or locked on the other. A chosen tab wears the theme's accent,
  and the row it brings up is set in the theme's own ink at a heavier weight
  than the interface around it, so the row in front of you reads as the thing
  you just chose rather than as a strip of quiet text that looks the same
  whichever tab is showing. Home is left alone, being the drawing tools, which
  their icons already tell apart. The Export entries go quiet while
  there is nothing drawn to hand on, rather than the row coming and going as
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
  These act only when everything selected is a symbol: an entity is pulled
  about by its edges instead, under an edit of its own, so the commands stay
  disabled for one. It lands
  where the pointer last was over the diagram rather than in the middle of the
  view, and is left unchosen, so picking several in a row does not keep
  swapping the properties panel over to them; one that would land exactly on
  another steps down and across until it finds room. It is moved, coloured,
  copied, deleted and undone like any other element, and underneath it is a
  note marked plain, which is what the file records. The gallery is a tool
  window that never takes activation, so the field being written in keeps its
  caret while its character is chosen.
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
- **Plain shows no colour at all** (Zain, 2026-09-24). Under Plain, everything
  ERDFlow itself inks is grey: every icon of every set, the Modern artwork
  included (shown as the greys of its own brightness), the painted
  check-model tick, the Home start cards' drawings and their "Coming soon"
  badges, the learning panel's wave, and the Relational Schema's key marks and
  relationship lines. Each schema line keeps a grey of its own, so crossing
  lines can still be told apart. The primary-key mark takes the grey of the
  PK letters. Plain's derived tokens are all greys too. The one exception is a
  person's own content: a colour chosen for an element is kept, and the Colour
  menu still shows real colours. Under every other theme the Modern artwork
  keeps its colours as before. Tests check every icon of every set, the Home
  screen and the schema for any coloured pixel under Plain.
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
- **A default is a starting size, not a ruling.** A newly drawn entity is
  148 by 86, and every other body is proportioned to sit beside it. The
  figures are marked in the source as defaults that are not to be changed:
  they decide what somebody meets when they draw an element and nothing more,
  since every element is then pulled about by its corners and edges or given
  a width and a height in the Properties panel, and a size given by hand is
  kept for that element and stored with the document.
- **An attribute is sized by hand, as an entity is.** Both hold a name, and
  how wide and how tall each is are two separate questions — a long name
  wants width where a second line wants height — so both answer to each of
  their four edges as well as to their corners, the side that is pulled
  moving and the side opposite it staying where it was. An attribute whose
  name does not fit the default can now be given room rather than eliding.
- **What is drawn on the diagram is scaled apart from what is sized by it.**
  Two figures, each tunable without disturbing the other or the defaults
  above. `lettering_scale` carries the type, because a shape only has to be
  recognised where a name has to be read: text is the one part of a diagram
  that must resolve into letters rather than into an outline, and it had been
  set small enough that a reader zoomed in before the diagram read at all.
  `connector_scale` carries the lines and everything on them — their weight,
  the crow's feet, the grips that bend and route them, the padlock that pins
  their ends, the mark that says a line carries a remark. A connector is what
  a reader traces across a crowded diagram, and a control they cannot see is
  a control they do not know they have.

  The shapes turned out not to be what was hard to read; enlarging them was
  the wrong answer twice before the lettering and the connectors were
  separated out. This is the conceptual canvas only — the Relational Schema
  sizes its own tables from what they hold and refers to neither figure.
- **The diagram is lettered for legibility.** One `lettered()` helper gives
  every name on the canvas its face and its size, so nothing on the diagram
  can drift out of step with the rest of it. The face is asked for as a list
  — Inter, Source Sans 3, IBM Plex Sans, Segoe UI, SF Pro Text, and the
  platform's own interface face last — because no one face is on every
  machine, and each is a humanist sans of much the same proportions, chosen
  for a tall x-height and open apertures. An uppercase I, a lowercase l and a
  figure 1 that cannot be taken for one another matter more on a diagram than
  they do elsewhere, because a name read wrongly is a model read wrongly.
  Weight and underlining are left to whoever asked, so a key attribute is
  still underlined and an entity still bold. Two places are deliberately left
  out: the overlay that already compensates for the zoom, and the notation
  samples, which are drawn into a pixmap of their own rather than in diagram
  units.
- Associative entities: choose Associative as a relationship's kind in Properties and it
  takes the entity body size and palette, since it converts to a relation of its
  own, and is drawn as a filled diamond inside an unfilled rectangle, which is
  what keeps it distinguishable from a solid entity. It may then take
  part in further relationships, as an entity does. Adopting or dropping the
  shape resizes it in the same undoable edit.
- An entity is pulled about by its own edges. Choosing one draws eight handles
  just inside its outline — its four corners and the middle of each of its four
  sides — and hauling any of them moves that side while the side opposite it
  stays exactly where it was; a corner moves the two that meet at it. The
  handles sit inside the outline rather than straddling it, so the box the
  shape draws is still the box it occupies and nothing measured from that box
  moves. Its relationships and its attributes follow the edge while it is being
  pulled. A box is held between 70 and 2000 units wide and 44 and 1400 tall, a
  pull runs through one named edit, "Resize entity", so it undoes in a step,
  and Escape abandons one in progress. A selected line answers the pointer
  first where the two overlap, since an end grip lies on the outline of the
  entity it joins.
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
- **One conceptual model, no modes.** ERDFlow had a Basic mode and a
  Convertible mode; it does not any more. The data was stored identically in
  both, so the mode only ever governed which fields the Properties panel drew,
  and a preference belonging to a person was being kept in the document. A
  project file no longer records a mode, there is no switch beside the
  **CONCEPTUAL** badge, and nothing is created, destroyed or re-identified by
  the removal.
- An attribute carries a **logical type** — Text, Integer,
  Decimal, Boolean, Date, DateTime, Binary, UUID — with a **length** where the
  type is measured. Choosing a type that is not measured drops the number rather
  than carrying one that would mean nothing later. No database is named: the
  choice between `VARCHAR(100)` and `NVARCHAR(100)` belongs to the physical
  stage.
- The fields conversion needs — logical type, length, the identifier/required/
  unique rules, and the schema comment — sit together in a **For the schema**
  section of the Properties panel that folds away. It is shut until somebody
  opens it, which is how the diagram was drawn before the fields had a section
  of their own, and it is opened and closed by the same fold mark the Explorer
  uses, drawn by the style so it follows the theme. Whether it is open is a
  **user preference**: it is kept with the application's settings, never in the
  project, it survives being applied to whatever element is looked at next, and
  folding it is not an edit — it costs no revision and cannot be undone or
  saved. Folding hides questions and never answers: everything inside stays in
  the model and stays visible to validation, readiness and conversion.
- It also carries the **rules a table will enforce** — identifier, required,
  unique. **Being the identifier and being drawn as a key are one fact**
  (Zain, 2026-09-24; they were kept apart before). Choosing Key on the diagram
  makes the attribute the primary key, and ticking Identifier draws it as a
  key; taking either off does both. A project opened from before is brought
  into agreement as it opens: a key oval becomes the primary key and an
  identifier is drawn as a key. A composite keeps its shape and may still
  identify a row; in Parts mode its parts are then the key columns.
- Entities, attributes and relationships carry a **comment for the schema**,
  which is what a generated table or column says about itself. It is not the
  description, which says what the thing means to a reader, and it is not a
  review comment, which is a remark about the work. Anything that becomes
  nothing — a triangle, a picture, a note — is refused one.
- An attribute with no type yet leaves a question open, and `validate` does not
  treat that as a fault: a half-answered model is still a model, and still
  saveable. Whether a model is *ready to be converted* is a separate question
  that `readiness` will answer, which is Phase 14's job and is not built.
- A relationship side records **whether it was answered**, beside what it says.
  A side that has only been drawn reads Many and Partial, which is also exactly
  what a deliberate M:M looks like, so without this a conversion could not tell
  a decision from a silence and would build junction tables out of questions
  nobody was asked. Answering a side through the Properties panel confirms it,
  whatever values are chosen. An unconfirmed side is a readiness question rather
  than a fault: the model stays valid and the diagram draws it the same.
- An attribute's type is chosen from the **whole SQL data type catalogue**,
  grouped as SQL Server groups it — exact and approximate numerics, character
  and Unicode strings, binary strings, dates and times, and the rest — with the
  ones reached for most at the top of the list. A type that takes a size carries
  one: a length for the string and binary types, a precision with a scale for
  `decimal` and `numeric`. The three SQL Server is removing — `text`, `ntext`
  and `image` — still read and still save, and say so when offered.
- **Conversion decisions** travel with the project: the table naming convention,
  each hierarchy's mapping strategy, what each composite attribute becomes,
  which side of a one-to-one carries the key, a bridge table's name, whether a
  bridge with no identifier of its own is keyed by its participants' foreign
  keys, an entity's chosen identifier, and any table name typed over the
  derived one. Each is
  keyed by stable identity, so an answer survives renaming and is never asked
  twice; each is an ordinary edit, so it undoes; and taking an answer back
  returns that question to its default rather than recording a different answer.
- Versioned `.erdx` JSON save/load, currently format version 20. Versions 1 to
  19 still open and upgrade on save. A file written before version 20 names its
  types from the small portable set that preceded the catalogue — those are read
  as the SQL types they always meant, so a portable `text` becomes `varchar` and
  a `boolean` becomes `bit` — and records no conversion decisions at all. A version 17 file names a conceptual mode;
  it still opens, everything it says about what it becomes is kept, and only the
  mode itself is discarded. A file written before version 19 has no record of
  which sides were answered, so every side in it is read as answered — the
  alternative would greet a finished diagram with a readiness question about
  every line on it. Incomplete but structurally valid diagrams
  can be saved. Invalid/unsupported files leave the open project intact.
- Safe file replacement, Save/Discard/Cancel protection, focused text committed
  before save, clean-state tracking, and reopening the file saved at the prompt.
- **Comments** — remarks left on the work while reviewing it. A comment is not
  the Note element, which is a card placed on the canvas and part of the
  drawing, and it is not an element's description, which documents the model
  and is meant to travel forward into the schema. It is about the work rather
  than part of it, and per [ADR-017](adr/ADR-017-REVIEW-COMMENTS.md) the three
  are kept apart.
- A comment is **pinned rather than placed**: it has no position, colour or
  transparency of its own. It can be pinned to an **element**, to a **line** —
  a remark about a cardinality belongs on the line rather than on either shape
  it joins — or to a **range of text** inside a name or description, which is
  how a remark comes to be about one word rather than a whole thing. Right-click
  an element or a line for **Comment…**, or choose some words in the Name or
  Description field and take **Comment on selection…** from the field's own
  menu.
- **One remark may be pinned to several things at once**, so it is written once
  and appears on all of them. Selecting several elements and choosing **Comment
  on selection…** does exactly that, and the properties panel says how many
  other things each remark also covers.
- Pointing at something carrying a remark **shows what was written**. A
  commented element or line carries a small mark whether or not remarks are
  being shown, because a remark nobody can see is a remark nobody can find. The
  mark is solid when pointing would say something and hollow when it would not.
- Two levels of hiding, which are different in kind and stored differently.
  **View → Show comments** (Ctrl+Shift+M) quiets every remark at once; it is
  how the diagram is being looked at, like the grid, so it is not saved with
  the document and does not enter the undo history. **Hide** on a single
  comment, in the properties panel, puts that remark away without deleting it;
  that belongs to the comment, travels with the document and undoes like any
  other edit.
- The properties panel lists the remarks on whatever is selected, and each can
  be read, reworded, put away, brought back or deleted there.
- A comment never outlives what it is pinned to. Deleting an element or cutting
  a line unpins every comment pinned to it, and a comment left pinned to
  nothing goes with it in the same edit, so one undo brings back the element,
  the line and the remark about them together. Editing text a remark is pinned
  into neither refuses the edit nor drops the remark: the range is held inside
  the text the field now has. Ranges are counted in characters, so they mean
  the same thing in the file, in the domain and in the panel.
- A comment records no author, because there are no accounts to name one. That
  is not a statement that comments have no author: the format is additive, so
  an author is added when accounts are.
- An inheritance link cannot carry a comment. It is anchored to its triangle
  rather than being a connector and has no identity of its own to pin to, so a
  remark about one goes on the triangle.
- **Search** narrows the diagram to what is being looked for, rather than only
  walking from one match to the next, per
  [ADR-018](adr/ADR-018-SEARCH-NARROWS-THE-DIAGRAM.md). A **Search** button in
  the document header, **Edit → Search…** and Ctrl+F each drop a bar in above
  the canvas, holding the box, the kind and the options, and closing on Escape
  or its own ✕. The button is there because the bar takes no room until it is
  asked for, and that is only worth doing while something on screen remains to
  ask with: it is what says the search exists at all, and what there is to
  reach for once the bar has been closed. It sits in the header rather than
  among the drawing tools, being about looking at the document rather than
  adding to it, and because that row is already tight enough to start dropping
  the names its tools are known by.
- Names are matched without regard to case, among **entities**, **attributes**,
  **relationships**, **hierarchies**, or all of them. A kind chosen with nothing
  typed asks for every element of that kind, which is how "show me only the
  entities" is asked for. Typing settles before the diagram is filtered on it,
  so writing a word is one change rather than six.
- The options answer **two separate questions**, each as a set of alternatives,
  so choosing one answer cancels the other answer to *that* question and leaves
  the other question alone.
  - **What to keep** — *Only what matches*, or *What matches, and what it
    touches*. What it touches means one step out: what belongs to a match, the
    relationships and hierarchies it takes part in, and the far side of those.
    Following the joins to their end would fetch most of a well-joined diagram
    and leave the choice doing nothing.
  - **What to do with the rest** — *Fade it*, which keeps the diagram's shape so
    a match is seen where it sits and leaves no line hanging from a shape that
    has gone; or *Hide it*, for when a clean view is wanted more than the
    context, where a line goes with whichever of its ends goes.
- The two are deliberately not rivals. Keeping a match's neighbours **and**
  taking the rest away is the clearest view of all — a sub-diagram of the match
  and what it touches, with nothing else on the page — and making either choice
  rule the other out would lose it.
- What was found keeps its full strength and wears a ring.
- What was found is brought to the middle of the view, and no closer than it
  already was: the diagram zooms out only when what was found would not
  otherwise fit, and never zooms in, since being found should move the diagram
  rather than take the reader somewhere they did not ask to go.
- A search is a way of looking, not an edit. It changes nothing in the model, is
  not saved, does not enter the history and does not dirty the project, the same
  rule the grid and the comment switch follow. Closing it puts the whole diagram
  back, so a filter is never left on behind a bar nobody can see.
- Work leaves ERDFlow through **Export** and comes back through **Import**, the
  pair the database tools it sits beside use. Each has a tab of its own on the
  ribbon and a menu under File, side by side, because a reader looking for one
  expects the other in the same place.
- **Export → ERDFlow project** writes a copy of the project itself, losing
  nothing. It is not Save As: the project being worked on keeps its own file and
  its own unsaved state, so this is a copy put somewhere rather than a change of
  where the work lives. That, with **SVG** and **PNG**, makes three formats that
  come back whole.
- **Import** brings another project's contents **into** the one being worked on,
  rather than replacing it, which is what the word means in this field and what
  distinguishes it from Open. It reads an `.erdx`, or an SVG or PNG carrying a
  project. Everything arrives with identities of its own, so a project copied
  from this very one can be imported without a single collision; it is put down
  clear of what is already drawn, selected and brought into view; and the whole
  import undoes in one step. Reading what other tools write — SQL, CSV, JSON —
  is named in the menu and left disabled, because those describe tables rather
  than a conceptual diagram and there is nowhere to put them until the
  Relational Schema workspace exists.
- As a **picture**: **SVG**, **PNG**, **JPEG**, **WebP**, **TIFF** or a **PDF**
  page. The three anyone reaches for sit on the menu and the rest are gathered
  behind **Other picture formats**, so a common choice is never hunted for among
  rare ones. **Copy as picture** puts the selection, or the whole diagram when
  nothing is selected, on the clipboard as a raster and a vector at once, so
  whatever it is pasted into takes whichever it prefers. A format this build has
  no writer for is not offered rather than offered and then failed.
- As a **written listing**, for the people who want words rather than a drawing:
  a **PDF document** — a paginated report with the diagram above a data
  dictionary — a **Markdown data dictionary** for a repository README, a
  self-contained **HTML report** carrying the diagram as inline SVG, and a
  **CSV** of every element as one row. All four read the model that is already
  there and need no conversion. They name the notation in words rather than
  encoding it: a key on a weak entity is listed as a **partial key**, a
  relationship says which sides are mandatory and what role each plays, and a
  hierarchy says whether it is disjoint and whether it is total. They are
  listings, not interchange, and each says so in its own footer: nothing
  re-imports them. Written twice from the same model they come out byte for
  byte the same, so one kept beside the project in version control shows a
  change only where one was made. The PDF is the exception, since a PDF records
  when it was made.
- The listings are built with Qt's own rich text and PDF writer. ERDFlow still
  has no runtime dependency beyond C++20 and Qt.
- **Export with options…** opens one dialog over every format, documents
  above pictures, because somebody handing work on chooses between a report and
  a picture before choosing between PNG and SVG. The options that decide whether
  a picture is usable ship with it rather than after it: the **extent** (whole
  diagram, selection or current view), the **background** (transparent, the
  theme's canvas colour, or white), the **scale** for a raster or the
  **resolution** for a page, and the **margin** left around the diagram. The
  dialog says what pressing Export will produce, in the units that format is
  measured in, and turns off what cannot be asked for: an extent with nothing in
  it, the picture options a document has none of, and carrying the project in a
  format that cannot hold one. A picture larger than ERDFlow will draw is
  refused with its size rather than attempted.
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
| 0 — Architecture | Completed review; ADR-001–013 accepted. ADR-014 records implementation choices. ADR-015 records export and interchange formats, whose picture half is now implemented and whose relational half is not; ADR-016 records project organisation and the start experience, of which the Home screen and its three start cards (with Templates and Import in its sidebar, ADR-022 §9.14) are implemented and recent projects, templates, project folders and thumbnails are not (see *The Home screen and the Azure theme*); ADR-017 records review comments, which are implemented and are deliberately kept apart from an element's description and from the Note element; ADR-018 records that search narrows the diagram rather than walking hit to hit, which is implemented; ADR-021 records relational identity and provenance, which is implemented; ADR-022 records four visible workspaces, the theme token resolver, the menu bar and the start-route readiness gate, of which the token resolver, the Azure theme and the readiness gate are implemented, the menu bar order is implemented for the native menu bar, the workspaces wear Azure with Relational Design offering none of the conceptual tools (§9.12), and the Home screen, its card states and both workspaces are held to reference pictures at 1440 × 1080 (see *The Home screen and the Azure theme*). |
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
| 13 — One model | Complete. One conceptual model with no modes; logical types with a length, the identifier/required/unique rules and the schema comment, all always present and gathered in a collapsible "For the schema" section whose state is a user preference rather than project data. The earlier Basic/Convertible mode design is withdrawn and removed; files that recorded a mode still open. |
| 14 — Readiness | Partial. A relationship side records whether it was answered, so a default can no longer be read as a choice; the full SQL type catalogue is offered with sizes and scales; and the conversion decisions persist against stable identities. Not implemented: `readiness` itself, and the structured policy of ADR-009 — Domain Invariant Violation, Blocking Error, Warning, Information, Unresolved Decision — that conversion will gate on. |
| 15 — Project files | Single-page native format foundation delivered early to protect the current editor's work. No historical migration or recovery system. |
| 16–17 — Pages/editor milestone | Not complete; multiple pages, the remaining conceptual semantics, and the recent projects, templates, project folders and thumbnails of ADR-016 are required. The start screen itself is built. |
| 18 onward | Import, provenance, physical design, SQL, data, and later production/ecosystem features remain planned. Schema generation has a preview ahead of them — see below. |
| 24 — Schema workspace | Not started as a workspace. What exists is a **preview** of it: the conversion read as tables, drawn beside the diagram, with the questions it cannot settle asked on the tables they concern. It owns no relational objects, has no identities of its own, and is worked out afresh from the project every time. See the section below. |
| 35 — Export | Partial, and pulled forward the way Phase 15 was, because ADR-015 splits export into halves with different prerequisites and neither the pictures nor the listings need anything later. Implemented: the pictures, their options, the project carried inside SVG and PNG, and all four documentation listings, offered together under Export. Not implemented: a multi-page PDF of a project, which waits for the multiple pages of Phase 16; the published JSON Schema for `.erdx`; the outline-text option for a pixel-exact handoff; and the whole schema half — `.sql`, Mermaid ER and DBML — which cannot precede the Phase 24 workspace it would read from. |

## The schema preview

A picture of what the Conceptual model becomes, raised over the diagram. It is
derived: `schema_preview(project)` is a pure function with no identity and no
storage, worked out again on every change, so it cannot drift from the model.
The Relational Schema workspace of Phase 24 — relations with identities of
their own and a history of their own — is still a later and separate thing.

**Implemented.**

- **The mapping**, by the course rules: an entity becomes a table; a composite
  gives its roots, its whole, or both; a derived attribute is listed as
  `ignored` rather than silently dropped; a multivalued attribute becomes its
  own table; one-to-many puts the key on the many side; many-to-many becomes a
  bridge with a key of its own; an associative relationship the same, plus the
  associated key; a self reference points at its own table; and a hierarchy
  maps all **three** ways — table per subclass, single table with a
  discriminator, and table per concrete class.
- **Conversion decisions**, asked on the tables they concern rather than in a
  dialog, and kept in the project against stable identities: the ISA strategy,
  what a composite becomes, which side of a 1:1 carries the key, and what keys
  a bridge that has no identifier of its own (ADR-021 §5b). An answer
  in force is filled in; one that is only the default is filled faintly.
- **Types and sizes**, answered where the column is. An unanswered type is
  drawn as a dashed blank; pressing it opens the whole SQL Server catalogue in
  one searchable run, most used first. A measured type grows a second cell for
  its size, asked in the terms that type is measured in. Columns sharing a
  name are gathered so one answer serves all of them, in a single edit.
- **Editing the schema away from the diagram** (ADR-010 §39–48): a column
  added here only, or an attribute hidden from here only. Every such change is
  put to the user before the diagram is touched, and declining records the
  difference rather than refusing it. The panel says how far the two levels
  have come apart.
- **Connectors under the hand.** Orthogonal and row-exact, routed around the
  tables or straight through, kept apart in lanes and fanned where several
  land on one row. Any straight run can be pushed sideways; either end can be
  moved around its table's outline or pulled off it and left hanging, which is
  reported and not refused. A line always keeps enough straight length at each
  end for its cardinality symbols. A generated line runs from the exact
  primary-key row it references to the exact foreign-key row. Which side each
  end leaves by, left or right, is chosen by trying all four pairings against
  the router and keeping the cleanest (shortest, fewest turns, least shared
  lane). When two are equally clean, the foreign key's left side wins. A line
  shaped by hand skips this choice and keeps its shape, so automatic routing
  never overrides manual control.
- **Several tables are gathered and coloured together.** A band drawn from
  the empty canvas takes every table it touches — touches rather than
  swallows, since a band drawn across a row of tables is meant to take them
  and asking for every edge to be inside would make a wide table almost
  impossible to catch. Holding Shift or Control and pressing a table adds it
  or takes it out, so a group can be gathered one at a time; held, a press
  never picks the table up, because gathering and moving are different acts.
  Pressing inside a group that has already been gathered keeps the group,
  which is what makes a selection usable. **Dragging any table of a gathered
  group moves the whole group** the same distance, so it keeps its
  arrangement (2026-09-24; before, only the table under the hand moved). The
  group stops as a whole when its first table reaches the top or left of the
  schema, rather than piling up against the edge. The move is one edit, so
  one undo puts every table back. As on the diagram, a press on the schema
  gives it the keyboard, and **Ctrl+A (Cmd+A on macOS) gathers every
  table**, so the whole schema can be moved together. Before, the keyboard
  stayed with the diagram behind it, and Select All selected the diagram
  instead. One gathered behaves as it always
  did — ringed, with what it is joined to, and the rest faded; several means
  exactly those, so nothing is drawn in with them.

  They are coloured from the row's own menu, out of the same palette the
  canvas offers, because a table here and the entity it came from are one
  element wearing one colour: it is the same edit either way, and the colour
  already travelled both ways. `swatches()` and `swatch_icon()` moved out of
  the canvas's own file to be shared rather than copied, since two palettes
  would drift apart. Applies to everything gathered, or to the table pressed
  where nothing is.
- **Looking at it**: pressing a table rings it and everything it is joined to
  and fades the rest; chips narrow by where a table came from; a search of its
  own picks out names; headers say what each table came from.
- **Arrangement is part of the document.** Table positions, table sizes,
  line routes and end anchors live in the project, so they undo, they redo,
  and they save. A drag is one step of history, written when the hand lets go,
  and moving or resizing one table never moves another: the packing is worked
  out from the size each table would have had anyway. Every edge and every
  corner of a table is a handle; the side that is pulled moves and the side
  opposite it stays put, so a left or top edge carries the table's corner with
  it and writes the place in the same edit as the size. Room given to a
  table's height is shared out between its rows, and a table is never pulled
  shorter than the rows and questions it holds.

- **A row is ruled into columns, and they are named.** Under the table's own
  header a second row says what each column holds -- Column, Type,
  Constraints -- and rules run between them down as far as the rows go. They
  stop at the last row, so the empty slot that adds a column stays the clear
  place to press that it is and the footer's questions are not ruled into
  columns they have nothing to do with. The three constraints share the one
  heading, because between them they are the single question of what the
  table enforces, and keep their own ruled columns underneath where telling
  them apart is what matters. The type column is as wide as the widest type
  in that table, held between a floor and a ceiling and worked out once for
  the table, because a column whose edge moved from row to row could not be
  ruled off at all. Every column is measured from what it holds -- the
  longest name, the widest type, the longest list of constraints -- and a
  table's own width follows from the sum of them, so nothing is cut off by
  the rule beside it before a hand has chosen a width. Tables are therefore
  no longer all one width, and the automatic packing steps by the widest of
  them so the arrangement stays a regular grid rather than a pile in which a
  wide table lands on its neighbour. That step is measured from the width each
  table would have had anyway, never from one a hand gave it -- the same rule
  the packing already followed for heights, and for the same reason: a table
  pulled wider must not push its neighbours across.
- **Narrowing a table folds its columns away from the right, one at a time.**
  The constraints give way first, then the type, and a table at its narrowest
  is left with the gutter holding its keys and the names beside it -- the
  least a table can be and still be a table, since a name and whether it is
  the key are the two things no other column can stand in for. Every column
  comes back, in the reverse order, as the table is widened again. Nothing
  springs back and nothing is hidden at a width the table chose for itself: a
  name column is never measured below a floor, so a table left alone always
  has room for everything it holds.
- **A type and its length are one column, written the way SQL writes it**:
  `varchar(50)`, `decimal(10,2)`. One word to read and two things to press --
  the name asks what type it is, the brackets ask how long -- and a length
  nobody has given yet shows what it wants, `varchar(n)`, rather than a
  number. Neither half wears a box except under the pointer, since the rules
  either side are what say where the column is.
- **The constraints a column carries, all of them in one column**, written
  the way the generated SQL will write them: `PK, NOT NULL, IDENTITY`. One
  column rather than one each, because between them they are the single
  question of what the table enforces and a list reads as the one answer to
  it -- and because a column that already says what the DDL says can become
  that DDL rather than having to be translated into it. A key is not also
  said to be unique: it is unique by being the key. Nullability always says
  which way it went, `NOT NULL` or `NULL`, because a column that may be empty
  and one nobody has decided about become different SQL.
- **Pressing it opens the list of what can be said about that column**, with
  what is already true ticked. A list rather than switches because several
  apply at once and a few cannot apply together. Nothing in it is greyed out:
  every entry can be chosen, and one that cannot take says why in the status
  bar, which tells a reader learning the rules something a dead menu entry
  does not. The outermost pixels of a row are left to the edge that pulls the
  table, or the right edge could never be taken hold of.
- **Every column has somewhere to put them.** A key the conversion invented
  has no attribute behind it and no identity of its own, so whether it counts
  itself up is remembered against the element whose table it belongs to, as
  its name already was (`SchemaOverrides::counting_keys`) -- and that is the
  commonest place of all to want it, a surrogate key being what `IDENTITY` is
  for. A unique foreign key says the side carrying it sees one row and no
  more, so it is read from the relationship's shape and setting it sets that
  side's cardinality on the diagram. A primary key answers that it is unique
  already, and a foreign key that it takes its value from what it points at,
  rather than doing nothing when pressed.
- **Nothing on the schema is read-only because it was derived.** Pressing a
  mark changes what the table enforces, and where the fact lives on the
  diagram the diagram is what changes: a foreign key is `NOT NULL` because the
  side it points at is total, so pressing its nullability sets that
  participation and the ERD follows. Where a mark cannot change -- a primary
  key can never be empty -- it is pressed like any other and says what would
  have to happen first, in the status bar rather than by being made dead under
  the pointer. An edit that will not take reports there too, rather than in a
  box that has to be dismissed: too much ceremony for a mark the hand is still
  resting on, and it would stop the next press dead.
- **The primary key, set from the schema.** On the row's menu: making a column
  the key sets the identifier rule and draws the attribute as a key attribute
  on the diagram, in one edit and so in one step of history, and makes it
  required since a key is never empty. Taking the key off is the same command,
  which is what moving a key between columns is made of. A composite,
  multivalued or derived attribute is refused with a reason rather than
  quietly reshaped.
- **Auto increment**, the one constraint with nothing on the diagram behind
  it: a Chen ERD has no way of saying a value is generated rather than
  recorded, so it is a fact about the table alone. Only a whole-number type
  can carry it, and a Decimal or Numeric only with no digits after the point;
  anything else is refused with a reason, since a generated column of the
  wrong type is SQL that will not run. New `Attribute::auto_increment` and
  `SchemaColumn::auto_increment`, format version 25.

- **The Relational Schema owns its identities** (ADR-021). A relation is no
  longer keyed by the conceptual element it was derived from: it has a
  `RelationId` of its own and records that element as its `origin`, with the
  conversion rule that produced it. A foreign key has a `ForeignKeyId` for the
  same reason. `TableId` is deliberately not used, being reserved for `Table`,
  which stays a distinct internal concept and surfaces inside the Relational
  Design workspace rather than a workspace of its own (ADR-022).

  The identity is *derived* from the origin rather than drawn from the
  generator, so the same project opened twice yields the same relations and
  everything kept against them — place, width, height, typed name, added
  columns, invented key names, counting keys, shaped lines — is still theirs.
  It depends on the origin alone and never on the conversion rule, or
  answering a conversion question differently would orphan all of it.

  Validation asks the question forwards, since a derived identity cannot be
  turned back into its origin: it works out what identities the project could
  produce and checks the schema's state refers to one of them. Format version
  26; a file written before it is migrated on load by the same derivation, so
  the same old file always produces the same relations.

  Behaviour is unchanged by design. Relations made by hand, schema-first
  projects, SQL import and three-way reconciliation are the next steps and are
  not built.

**Not implemented.** `readiness()` itself and the gate it feeds (Phase 14);
relational objects with identities of their own, and editing them as such
(Phase 24); the generation baseline and three-way reconciliation (Phase 25);
and `.sql`, Mermaid ER and DBML export, which cannot precede the workspace
they would read from.

- **Renaming, and which way it travels.** A table's name and a column's are
  opened for typing by double-clicking them. Where the thing came from the
  diagram, the rename goes there: renaming a table renames the entity,
  relationship or attribute it was made from, and renaming a derived column
  renames its attribute, so the two levels never come to disagree about what
  something is called. A name is not the kind of difference ADR-010 records --
  that is for structure, a column the schema has and the diagram does not. A
  column added on the schema is renamed in place, having nothing behind it. A
  name previously typed over a derived one is given up when the table is
  renamed, in the same edit, or it would go on masking the name just chosen.
- **A key the conversion invented can be renamed too.** Where nothing
  identifies a table, a key is made for it and named after the table. That is a
  guess, so it can be taken back: the name is remembered against the element the
  table came from, and every foreign key pointing at that table follows it,
  since a foreign key is named for the key it points at. Emptying the name hands
  it back to the rule.
- **The schema says what it did.** Hovering a row says whether renaming it will
  reach the diagram, and a key the conversion invented says that it is the
  primary key, why it exists and that the name can be changed. Generating a key
  is right; doing it silently left the user with a name they did not choose and
  no sign that choosing another was allowed.
- **The key that was drawn is the key** (Zain, 2026-09-24). An entity's key
  attributes are its table's primary key: one column, or a composite key where
  several are drawn. Nothing is invented beside them, so the example's tables
  are keyed by their own `ID` rather than a generated `StudentID` next to an
  ordinary `ID` column. A foreign key carries every column of the key it points
  at, each named for the one it points at, qualified with the table's name
  where that clashes (`ProfessorsID`); a one-to-one marks it unique only where
  the key is a single column, since a column's own flag cannot say that a
  combination is unique. A key is made only for an entity with no key
  attribute, and a **notice says so**, once for each table, naming the entity
  and the key made for it. Bridge and multivalued tables keep their own key by
  the course rules and are not announced. A foreign key is no longer counted
  among the types still open, or among the names that share an answer, since
  its type is its key's.
- **Bridge keys: the identifier first, the participant keys only by choice**
  (Zain, 2026-09-24; ADR-021 §5b). A bridge is the table a many-to-many or
  associative relationship becomes. Its key is chosen in this order:
  - **A key the user defined.** This is a key attribute drawn on the
    relationship, or a column added to the bridge on the schema and made its
    primary key. It becomes the bridge's primary key, and the participants'
    foreign keys stay ordinary foreign keys beside it.
  - **Otherwise, a separate key of its own**, such as `EnrolledID`. This is
    the default. It is generated, can be renamed, and is not announced.
  - **Only where somebody chooses it, the participant keys.** The
    participants' foreign keys together become one composite primary key
    (`StudentID + CourseID`). Where a participant's own key has several
    columns, all of them take part.

  The choice is asked on the bridge table: *"Enrolled has no defined
  identifier. Bridge primary key:"*, with the answers *Separate key
  (default)* and *Use participant keys (composite PK)*. Where the bridge has
  an identifier, the question is not shown, so nothing ever offers to replace
  a key somebody drew. An answer given earlier never outranks an identifier
  drawn later: the identifier takes over and the question goes away. The
  earlier answer is kept, not erased, so it applies again if the identifier
  is removed. Answering is an ordinary edit (`Editor::set_bridge_key`, kept
  in `ConversionDecisions::bridge_key`, format version 28). It undoes, it is
  saved, and it is removed with the relationship. The answer is handled by
  `MainWindow::answer_decision`, which passes it on to the editor.

  A bridge's foreign keys are `NOT NULL` under either strategy, and the
  bridge end of each of its lines is drawn as mandatory. A row of Enrolled
  that names no student is not an enrollment. Before this, the foreign keys
  took the participation of the side they pointed at, and so were nullable
  by default.

  **Not yet reachable from the diagram.** The model's existing rule
  `attribute.key.relationship` refuses a key attribute owned by a
  relationship. So although the conversion would honour a key drawn on
  Enrolled, the editor refuses to draw one. Today a bridge gets a
  user-defined key only through a column added on the schema and made its
  primary key. ADR-021 §5b records this conflict, which is open and waiting
  for Zain's decision.
- **A primary key is marked twice**, with the letters `PK` and a solid golden
  key beside them, upright with its teeth pointing down. The key is artwork in
  the icon set (`key.svg`) rather than a shape drawn in code, so it is a key
  rather than an approximation of one, with a painted fallback for the sets that
  have no file for it. It is filled rather than outlined for the reason the
  diagram's padlock is: it is read at a glance in a small space, where a
  hairline reads as a smudge. The gold keeps
  the theme's warning hue and is raised in saturation and brightness until it
  looks like a key, since an ink chosen to be read as words is a bronze on light
  paper. The glyph comes from the icon set in use rather than being drawn into
  the schema, so it follows the chosen artwork like every other icon, and the
  key gutter is wide enough to hold both marks rather than the mark being shrunk
  to fit.
- **A foreign key is always marked `FK`, in green** (2026-09-24). This holds
  when the column is also part of a composite primary key, as the participant
  keys of a bridge are when somebody chooses them. The gutter never shows a
  merged `PK FK` marker. That such a column belongs to the key is shown in its
  Constraints cell, `PK, NOT NULL`. The orange `PK` marker and the golden key
  are kept for key columns that are not foreign keys, so orange means
  primary key and green means foreign key.
- **A misplaced line end is told what is wrong and why.** An end still goes
  exactly where the hand puts it, including where the schema cannot mean it:
  nothing springs back. But a line on the schema joins two rows rather than two
  tables — the foreign key it draws and the key that key points at — so an end
  left elsewhere makes the drawing read as a schema it is not. On release, such
  an end is named: the row it belongs on, the row it was left on, and why that
  row cannot serve — an ordinary column, because a key can only run to a key; a
  foreign key of its own, running somewhere else; a column the conversion left
  out, which nothing can point at; a key of that table, but not the one this
  points at; or off the table, joining nothing. Said once when the hand lets go,
  not while it is still moving.

  The two ends are told apart, because they are wrong in different ways: a
  line runs *from* the key that points *to* the key pointed at, so the near
  end has to sit on a column that does the pointing and the far end on the
  column pointed at. A primary key is a perfectly good column to land on and a
  very bad one to start from, and earns a different answer at each end — the
  near end is told it is "a key this table is identified by, not a key that
  points anywhere", the far end that it is "a key of this table, but not the
  one X points at". It was previously reported at the near end as an ordinary
  column, which a primary key is not.
- **A warning comes up where the hand was, not along the bottom of the
  window.** A `Notice` laid over the whole window and put beside the point the
  pointer let go of: a remark about a connection belongs where the connection
  was attempted, because that is where the person is looking, and a line along
  the bottom of the window is too far from it to be read at all. It stands
  below and to the right of that point, clear of the pointer so that what it
  is about is not covered by what is said about it, and turns to the other
  side where there is no room, so it never leaves the window. It keeps that
  place when the window changes shape under it. Drawn in the theme's own amber —
  the colour every palette already has for a warning, and the one a note on
  the canvas is drawn in, so the two read as the same kind of thing said in
  two places. What went wrong is in bold and why is under it, because the two
  are read at different speeds: the first says whether to stop, the second
  says what to do.

  It is never dismissed and never taken away on a clock. A warning that has to
  be clicked away stops the work it is about; one that goes after so many
  seconds is gone before a slow reader has finished and still there long after
  a quick one has. So it waits for the hand: something went wrong under the
  pointer, the pointer stops while it is read, and moving on again is what
  says it has been. It rises into place as it arrives and sinks back as it
  goes, the movement tied to the fade so the two can never come apart. The
  pointer passes straight through it, so it can never block the next thing to
  be pressed, and undo takes it away with the thing it was about. The status
  bar still keeps the record after it has gone.
- **Another column is added where it will be read.** The table under the
  pointer shows an empty slot beneath its last row, which lights when the
  pointer is on it; pressing it makes the row and opens its name for typing in
  the row itself. Nothing is asked first. A dialog wanting a name before the
  column exists puts a question in front of the thing it is about, and the two
  it used to ask — the name, and whether the diagram should follow — are
  answered by the row appearing and by reflecting. Reflecting is the default
  because the schema and the diagram are two views of one model: the column is
  an attribute on the model and the schema follows from it. A column meant for
  the schema alone is the departure, and is asked for by name on the menu.

**Held to the diagram's own rules.** The schema half of a project is state
like any other, and it is now asked the same questions the diagram half is.

- **Deleting an element takes its answers and its schema edits with it**, in
  the same edit, so one undo brings them all back together. This is what a
  remark pinned to an element has always done. Before it, an answer left
  pointing at nothing made the deletion itself impossible — `validate()`
  refuses such an answer, and refusing it refused the whole edit — and a column
  added to a table whose element was gone simply stayed and was written to the
  file. Three separate refusals were reachable by two ordinary actions in
  order: renaming a table and then deleting it, deleting the attribute an
  entity was told to identify itself by, and disconnecting the side of a
  relationship that was named as carrying the key.
- **`validate()` reads the schema.** A column added to a missing element, a
  hidden attribute that no longer exists, a table placed off the canvas or
  pulled to a negative size, and a line shaped from a link that is gone are all
  refused, and refused on load as well as on edit. Added columns are held to
  the name, identity, comment and type rules a drawn attribute is held to,
  including the rule that a scale cannot exceed its precision. Nothing asked
  any of this before.
- **A schema column's identity comes from the guarded generator**, and both it
  and a comment's are handed back to that guard when a project is opened.
  Forgotten on open, either could be issued a second time and given to a new
  element, leaving two things sharing one identity with nothing to say so.
- **The undo budget counts what it retains.** The conversion answers, the
  schema's edits and its arrangement are swapped whole rather than key by key,
  so one answer stores a copy of every answer beside it. They were the only
  fields costing the 32 MiB budget nothing, which a history of them could pass
  unnoticed; four hundred typed table names were accounted 389 KiB and in fact
  retained 17.7 MiB.

**Format.** Version 26 gives the Relational Schema identities of its own and
keys its state by them; version 25 records whether a column counts itself up;
version 24
records what a key the conversion invented has been
renamed to; version 21 records where the schema differs from the diagram;
version 22 records how it has been arranged; version 23 records how tall a
table has been pulled as well as how wide; version 28 records which bridges
somebody chose to key by their participants' foreign keys. Older files open,
and read correctly as having none of these.

The Part 1 checklist is a coverage inventory, not a replacement for semantic
prerequisites. Persistence was deliberately pulled into this usable slice so
newly drawn diagrams can survive application shutdown. This does not advance
conversion, SQL, AI, or data features ahead of the roadmap.

## The Home screen and the Azure theme

Built under ADR-022, in the stages of the Azure programme whose list is kept
outside the repository at `erdflow-notes/UI-AZURE-TASKS.md`. What is built:

- **ERDFlow Azure** is a twentieth theme, `erdflow.azure`, and the theme of a
  fresh profile. A profile that has already chosen a theme keeps it, and the
  nineteen themes that were there before are unchanged and still offered.
  Azure is fully specified as a set of tokens (`Tokens` in `theme.hpp`): colours,
  radii, shadows and text styles. The other nineteen derive theirs from their
  own palette through a resolver that runs once when a theme is put on and is
  cached, never while painting. Azure's primary is `#1E88E5`. A selected row,
  where white lettering sits on filled blue, uses `#1976D2` instead: an
  accessibility override derived from Azure (ADR-022 §9.3), because white on
  `#1E88E5` is 3.68:1 and every theme is held to 4.5:1 for ordinary text.
- **ERDFlow opens on the Home screen** (`HomePage`), with the work behind it.
  **Home** in the menu bar goes back to it. Panels that were open are put away
  while it is up and come back with the work; a panel that was closed stays
  closed. A file named on the command line, or `--example`, goes straight to
  the work.
- **The top of Home.** The native menu bar (File · Home · Edit · Insert ·
  Design · View · Help) stays on Home. The ribbon's rows give way, while Home
  is up, to a slim bar (`AppTopBar`) carrying only the ERDFlow mark, a
  **Settings** button and a **Theme ▼** control, as Zain settled (ADR-022
  §9.14). Theme opens the window's own theme menu, with its preview on hover.
  Settings holds the window's own Theme, Icons and Notation menus. The ribbon
  row that was in front comes back when Home is left. The status line stays.
- **Opens filling the screen.** An ordinary launch shows the window
  maximised. 1440 × 920 is the size it returns to when un-maximised, and a
  run with `--screenshot` or `--size` is shown at that size, or the one asked
  for, rather than maximised.
- **One page.** Everything on Home is on screen at once at the window's
  un-maximised size (1440 × 920), at the 1440 × 1080 reference, and at 1366 × 740,
  which the tests check. On a short window the spacing closes up first, then
  the illustration steps aside, the card drawings shrink, and the cards
  narrow, keeping their shape. Words, fields and buttons keep their size.
  On a shorter window still, the centre scrolls rather than squash the cards
  (ADR-022 §9.16). The learning panel goes below 1180 pixels wide and the
  sidebar below 880, rather than either being squashed.
- **The sidebar** (`HomeSidebar`) is eight buttons: Home, Open Project,
  Recent · Examples, Templates · Import · and, at the foot, Settings and
  Help. There is no New Project row, because the cards are where a project
  is started (ADR-022 §9.20). Each has its outline icon from the same Lucide family as the rest
  of the window. A single press activates a row. The list it replaced only
  answered a double press on most platforms. Up, Down, Home and End move
  between rows, and Tab reaches every one. The chosen row is `#1976D2` with
  white lettering. Every row is wired: Open Project and Import open the Open
  chooser; Recent opens the list of the
  ten projects last opened, saved or created; Examples and Templates open the
  bundled University project untitled, which is what starting from a template
  is (ADR-016); Settings and Help open their menus beside the row.
- The centre says **Welcome to ERDFlow** and *Design. Model. Convert.
  Generate.* beside a **drawn illustration** (`WelcomeFlowIllustration`,
  ADR-022 §9.15): a large three-tier database on a rounded platform over a
  soft glow, with four panels around it, read by their marks: a structure of
  boxes, an entity–relationship–entity, a table on a blue panel and a page of
  SQL. Each panel leans towards the database; straight lines with two bends
  join them to the platform (ADR-022 §9.17). It stands at the right of the welcome, its foot level with
  *Create a new project*, up to 230 pixels tall, and steps aside on a short
  or narrow page. It is painted, not loaded, in the theme's own tokens, at
  any size. It is a component: the panels are data (`HeroOrbitItem`), a
  caller can give it other words, and a panel given no ring is placed on one.
  A caller can also draw a panel's content itself (`draw`, clipped to the
  panel) and give it something to do when pressed (`on_press`). The
  product's own panels use neither and stay decoration.
- **The panels revolve round the database, and can always be made still**
  (ADR-022 §9.16). All four travel one elliptical orbit, a full revolution
  every 18 seconds at a constant 20° a second, a quarter-turn apart. A panel
  behind the database is smaller and fainter and passes behind it; one in
  front passes in front. The database stays put and breathes. Each panel's
  line is three straight segments with two slightly rounded bends: a run out
  of the panel, a slant, and a run into the platform. The two runs point the
  same way and are the same length (ADR-022 §9.17). The line is worked out
  every frame from the point on the panel's edge that faces the platform.
  For a panel behind, it ends where it goes behind the database, so both
  runs stay visible. It pulses faintly. Pointing at a panel highlights its border and stops nothing. The
  glow and platform are drawn once and kept, and a frame costs only what
  moves. The clock runs only while the Home screen is showing. Setting
  `ERDFLOW_REDUCED_MOTION=1` starts it still, at the starting composition.
  Nothing is conveyed by the motion alone.
- **Three ways to start**, as cards in a fixed order with fixed wording:
  *Conceptual Design (ERD)*, *Relational Schema* and *SQL Script (DDL)* (titles
  changed 2026-09-24, ADR-022 §9.19 and §9.20). *From
  Template / Example* and *Import Existing* are not cards. They are the
  sidebar's Templates and Import rows (ADR-022 §9.14). The cards **share one
  line**, never wrap, and are **door-shaped**: 1.10 times as tall as wide,
  never wider than 315 pixels. On a wide window they stop there and the
  group is centred. The drawing area grows from 88 to 105 pixels with the
  card, and + Create stands at its foot. A card not yet available
  shows its words at about three quarters strength and its drawing at 60%,
  with the badge at full strength. A chosen card has a faint blue light at
  its top (§9.15, §9.16). Each is a real
  control: it takes keyboard focus and is read out by name and description.
  Pressing a card **chooses** it and nothing more; the card's own **+ Create**,
  centred at its foot, is what starts. A card reads top to bottom: title,
  the *Coming soon* row (empty on a card that can be taken), drawing,
  description, + Create. The helper line is no longer drawn.
  *Relational Schema* and *SQL Project* are shown in their
  places, marked *Coming soon* and not enabled, each saying why in its
  tooltip: the first waits for ADR-021 Step B, the second for SQL to be
  parsed into Relational Design and round-tripped (ADR-022 §9.2).
- **Nothing is asked under the cards** (ADR-022 §9.19): the page ends with
  them. Conceptual's **+ Create** opens a new, untitled conceptual project,
  as File › New project does, and nothing is written until it is saved.
  There is no Cancel. Home is left by starting or opening something. Where
  a project's name and location are asked is still to be decided.
  `ProjectDetailsForm` is kept for that decision, but Home no longer uses it.
- **Live demos under the cards** (`HomeLiveDemo`, ADR-022 §9.21), **Stage 1
  of 7**. Each card has a demo under it, as wide as the card, centred and
  following it. The demo row sits on top of the page, so it takes only the
  empty space under the cards and never moves or resizes one. **Stage 2**:
  the Conceptual scene. Student, Course, Enrolled, the lines, five
  attributes and the underlined keys arrive in twelve steps. Scenes are
  stored as data and drawn by one painter (`home_demo_scenes`). There is no
  clock yet, so Home shows the finished model. **Stage 3**: the Relational
  Schema scene. Students, Courses and the Enrollments bridge fill in a row
  at a time. The keys are marked, with PK as a gold key and FK in green,
  and each foreign key's line runs row to row with Crow's Foot ends. There
  is no diamond. **Stage 4**: the SQL scene. An editor rises, a nine-line
  script creating the three tables is typed at a steady 40 characters a
  second with a caret and theme syntax colours, and "✓ 3 tables created"
  appears at its foot. Nothing is run. **Stage 5**: one `HomeDemoClock`
  plays all three from a single 30 fps timer. The demos start 0, 1 and 2 s
  in, and each then loops at its own length. Stopped, the demos stand
  finished; the visual suite stops them for its pictures. **Stage 6**:
  reduced motion comes through the hero's one seam (§9.9). Where stillness
  is asked for, the demos never move and show their scenes finished. The
  clock ticks only while the demo row is shown. It stops when Home is left,
  and every demo starts again from the beginning when Home returns. **Moved
  into the cards** (2026-09-25): the Conceptual demo (a horizontal ERD, by
  another agent) and the Relational Schema demo (15 steps, a key marked
  with its row) are held inside their cards, in place of each card's
  drawing and description. Descriptions are still read out. All three
  cards stay one height at 269 × 334, with + Create on one line. **SQL in
  its card** (step 2): Zain's script, set compactly in ten lines, is typed
  head then columns for each table, then "✓ 3 tables created". All three
  demos are now inside their cards, the row under the cards is removed,
  and the clock follows the card row. **Still pictures** (2026-09-25): all
  three demos stand still, each showing its finished scene. The motion is
  kept behind a per-card switch (the `played` list in
  `HomePage::build_centre`, `moves`), and all three are off. The third card
  is now titled *SQL Script (DDL)*. **Raised screens**: each demo is shown
  on a small screen standing out of its card, drawn in the style of the hero
  panels. The Conceptual card's screen shows the real Conceptual canvas: a
  small example drawn by `DiagramView` at the real element sizes, recorded
  once for each theme (`home_demo_canvas`).
- **The learning panel** (`HomeLearningPanel`) has its four topics with their
  icons, and *View tutorials* is a button that opens the quick guide, which is
  the only tutorial there is yet. There is no *Open an example project* link,
  because the sidebar's Examples row does that (ADR-022 §9.20). *Design today. Build tomorrow.* is set in
  light italic over the wave at its foot.
- Home's text is in the specification's pixel sizes. Where one of Azure's
  colours would put ordinary text under 4.5:1 it is deepened only as far as
  needed: Create Project and the chosen row use `#1976D2`, and the learning
  links and the form's error line are darkened slightly.

- **The workspaces wear Azure** (ADR-022 §9.12). The theme dresses the whole
  window, so the Conceptual workspace, Relational Design, the panels, menus,
  dialogs and ribbon all take Azure's colours with nothing about what they do
  changed. The interface now names the relational workspace **Relational
  Design** wherever it said "schema" for it: the header button (formerly
  *Preview schema*), its search, its Full button and status message, the
  resize grip, the properties section *For Relational Design*, *Add column in
  Relational Design only*, and its empty state. Internal names are unchanged.
- **Relational Design offers only its own tools.** While it has the whole
  window, the header's badge reads RELATIONAL DESIGN rather than CONCEPTUAL.
  The ribbon and its drawing tools (Entity, Attribute, Relationship,
  Specialization, Connect, Note) are put away with the diagram, and so is
  Insert › Picture, which places a picture on the diagram nobody can see.
  What stays is its own: Arrange, Appearance, undo and redo, its search and
  the theme. The Conceptual workspace keeps Select, Entity, Attribute,
  Relationship, Specialization, Connect and Note.
- **Reference pictures and an audit** (`tests/visual_tests.cpp`, the `visual`
  suite). Home, the illustration on its own, a card at rest, under the
  pointer, chosen and *Coming soon*, a sidebar row under the pointer, and
  the Conceptual and Relational Design workspaces are drawn at the 1440 × 1080 reference (ADR-022 §9.6) with the
  illustration still and the caret steady. Each is compared with a picture
  kept under `tests/visual/<system>-<major version>`, and fails if more than
  0.4% of its pixels differ. Fonts differ between systems and releases, so a
  machine with no pictures of its own is told so and not compared. Pictures
  are kept for `macos-26` today. The audit checks that Tab reaches every Home
  control in reading order, that Tab leaves the description field (it used
  to type a tab into it), that the focused card, row, link and Theme button
  each show where the keyboard is, that every card and row has an accessible
  name, and that thirteen text-and-surface pairs in Azure read at 4.5:1 or
  better.

Not built yet, though some of it is on screen:

- **Relational Design First** and **SQL First** are disabled until their
  workflows exist (ADR-022 §5, §9.2).
- There is one template, the bundled University project. A gallery of
  templates, *Save as template*, project folders as a library and thumbnails
  (ADR-016) are not built. **Import** opens the same chooser as Open Project
  until SQL and database sources have somewhere to go.
- *View tutorials* opens the quick guide. There are no tutorials beyond it.
- The platform's own reduced-motion setting is not read. Qt has no
  cross-platform way to ask, so `ERDFLOW_REDUCED_MOTION` is the only way in
  today, behind one function where a native reading would plug in (ADR-022
  §9.9).
- Relational Design is still a panel over the diagram rather than a
  workspace of its own. It cannot yet make a table by hand (ADR-021 Step B),
  so it has no Table tool to offer, and Insert has no relational entries.
  SQL and Data are not built, so there is nothing of theirs to dress.
- Reference pictures exist for one system and release. The audit is
  offscreen and does not prove what a real screen reader announces.

## System qualities in this implementation

| Requirement | Concrete evidence | Current limit |
| --- | --- | --- |
| Sustainable | C++20 and Qt; no additional runtime dependency or speculative framework. | Packaging and dependency distribution review remain future work. |
| Scalable | Ordered ID maps, incident-edge lookup, incremental canvas projection, bounded history and input. | Full validation and Explorer rebuild still run at command boundaries; large operations are synchronous. |
| Secure | Strict field/version/type/Unicode/reference checks; duplicate JSON key rejection; size/depth/count limits; atomic save replacement. | This is not a completed production security audit. |
| Maintainable | Qt-free Domain/Application, injected persistence/ID ports, desktop assembly in `main.cpp`, tests at each boundary. | Full future Domain design has deliberately not been implemented. |
| Usable | Focused property editing, named undo actions, usable example, warnings that permit unfinished drafts, protected open/save. | Screen-reader support, broader keyboard/high-DPI/device checks, and production polish remain unverified. |

## Verification

All seven CTest suites pass in Debug, Release, and AddressSanitizer +
UndefinedBehaviorSanitizer builds. They cover the core, real persistence adapter,
canvas interactions, themes, desktop workflows, application startup, and the
reference pictures and accessibility audit of the Azure Home screen and
workspaces. See [development instructions](DEVELOPMENT.md)
for reproducible commands and [performance baseline](PERFORMANCE_BASELINE.md) for
measurements. GUI checks currently use Qt's offscreen platform on macOS; the
rendered example was visually inspected. This does not prove native dialogs,
platform accessibility, or interaction performance on Windows/Linux.

Useful regression checks include comments — that one remark pinned to several
things is found on each of them, that a range held in shortened text neither
refuses the edit nor blocks a save, that deleting a thing takes the remarks
about it and one undo brings them all back, that the switch quiets every remark
without deleting any, and that all three kinds of target survive a round trip
through the file and are never confused for one another; the listings — that each names what is
actually on the diagram, calls a weak entity's key a partial key, reads each
side of a relationship as a person would say it, and comes out the same twice
for the same model; picture export — that a picture never shows
the selection rings of the editor that drew it, that its background and size are
the ones asked for, that SVG and PNG give back exactly the project bytes they
were given while still parsing and still drawing, and that an exported picture
opens again as the project it carries; identity-preserving undo/redo; atomic rejection
without losing the redo branch; owned-graph deletion/duplication; participant
roles; hostile file inputs; preserving existing files on failed saves; cancelling
open; saving an active property field; save-then-reopen; Shift group selection;
group align-to-grid/bounds; stale gesture cancellation; incident-only live connector updates;
the schema half — that deleting an element takes the answers about it and the
columns the schema added to it in one undoable edit, that a column added to a
missing element, a hidden attribute that is gone, an arrangement off the canvas
and a line shaped from a vanished link are each refused, that a column's and a
remark's identities are remembered when a project is opened so neither can be
issued twice, and that an edit swapping a whole field costs the undo budget what
it retains; lining up — that a dragged element still meets the edge and the
middle of one far away across the diagram, and is left exactly where it was put
when nothing is within reach; and bridge keys — that a bridge keeps its
separate key until the participant keys are chosen, that choosing them makes
every foreign key a required part of one composite key, that the choice undoes
and redoes, that a key drawn on the relationship afterwards takes over and
removes the question, and that answering on the schema reaches the editor. No
test yet checks that a foreign key which is also part of the key is still
drawn with a green `FK`.

**Not passing as of 2026-09-24.** The uncommitted bridge-key work leaves four
suites failing in Debug:

- `desktop_tests` does not compile. The new bridge test looks the schema view
  up with `findChild<SchemaView>`, and `SchemaView` has no `Q_OBJECT`.
- `persistence` still expects format version 27 where 28 is now written.
- `core` fails the bridge test's step that draws a key on the relationship,
  which is refused by `attribute.key.relationship` (ADR-021 §5b). It also
  fails `schema_names_reach_the_diagram`, which expects the bridge's foreign
  key into a renamed `MatricNo` key to be called `MatricNo`, while the
  foreign-key naming rule now qualifies it with the table's name.
- `visual`: the relational-design baseline predates the bridge-key question
  under Enrolleds and the bridge foreign keys becoming `NOT NULL`, and it has
  not been retaken.

The claims above describe what the tests are written to check, not a passing
run.

## Still to build in Part 1

Logical metadata and conversion readiness;
multiple pages; cross-project clipboard; alignment/distribution; drag resize
handles; freely draggable connector endpoint handles; the relational
half of export, which waits for a schema to generate it from; autosave and
recovery. Connector joins can be pinned at their current positions,
and attribute/participant links support multi-point routes. Attribute kinds currently
form one exclusive enum, so composite keys or other combinations require a
deliberate model/format decision.

Undo history has a conservative 32 MiB accounting budget; older entries are
removed when it is reached, and a single larger edit is rejected. This estimates
history storage, not whole-process memory. The session's issued-ID registry also
grows with newly allocated IDs until New/Open. Files are limited to 8 MiB, with
10,000 elements and 10,000 participants. These are resource limits, not promises
that every maximum-size diagram is already smooth.

### Home card motion replacement — Step A (2026-09-24)

Only the Conceptual card has been migrated: its static drawing is removed,
its existing lightweight `HomeLiveDemo` is now its child, and the description
precedes the demo. The shared card row retains equal heights and aligned Create
buttons. The other two cards and their existing below-card demos await B/C.

The Conceptual scene now builds Student and Course, then each ID (underlined)
and Name, then Enrolled, its relationship lines, optional-many Crow's Foot
symbols, and Enrollment Date. Attribute curves share a short central trunk
on each entity; relationship anchors use the opposite faces. Enrollment Date
joins the diamond's right vertex. Colors come from existing theme tokens.
The existing timeline, deterministic finished state, reduced-motion setting,
and Home visibility handling are reused; no editor/domain behavior changes.

Debug and Release builds succeed. Desktop tests (including ownership, common
anchors, ordered arrivals, cardinalities, containment at five viewport sizes,
card/button alignment and existing motion tests) pass in both. Full CTest runs
are 5/7 in both: the pre-existing core failure `drawn keys are the primary key`
and visual suite remain failing. Visual differences comprise the expected
Home/card changes plus the pre-existing relational-design difference. The
1440×1080 finished Home capture was inspected in `build/visual-output/home.png`;
baselines remain untouched pending Step G. Stop here per the user's staged brief.

### Conceptual Home demo horizontal presentation (2026-09-24)

The follow-up presentation changes only the Conceptual card: Student on the
left, Enrolled centered at authored x=140, Course equally distant on the right,
all on y=100. Both entities' ID/Name branches emerge from their top-center
anchors; horizontal relationship connectors and optional-many symbols use
separate side anchors. Enrollment Date joins the diamond's bottom vertex.
The visible description is removed, and its space is given to the centered
demo; the previous responsive card height budget and Create baseline are
preserved so the other cards and Home layout do not move. Motion ordering,
notation, themes, and the real editor remain unchanged.
