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
  These act only when everything selected is a symbol: an entity's box is sized
  by the name it has to hold, so the commands stay disabled for one. It lands
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
- Versioned `.erdx` JSON save/load, currently format version 16. Versions 1 to
  15 still open and upgrade on save. Incomplete but structurally valid diagrams
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
| 0 — Architecture | Completed review; ADR-001–013 accepted. ADR-014 records implementation choices. ADR-015 records export and interchange formats, whose picture half is now implemented and whose relational half is not; ADR-016 records project organisation and the start experience, which is not implemented; ADR-017 records review comments, which are implemented and are deliberately kept apart from an element's description and from the Note element; ADR-018 records that search narrows the diagram rather than walking hit to hit, which is implemented. |
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
| 35 — Export | Partial, and pulled forward the way Phase 15 was, because ADR-015 splits export into halves with different prerequisites and neither the pictures nor the listings need anything later. Implemented: the pictures, their options, the project carried inside SVG and PNG, and all four documentation listings, offered together under Export. Not implemented: a multi-page PDF of a project, which waits for the multiple pages of Phase 16; the published JSON Schema for `.erdx`; the outline-text option for a pixel-exact handoff; and the whole schema half — `.sql`, Mermaid ER and DBML — which cannot precede the Phase 24 workspace it would read from. |

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
group align-to-grid/bounds; stale gesture cancellation; and incident-only live connector updates.

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
