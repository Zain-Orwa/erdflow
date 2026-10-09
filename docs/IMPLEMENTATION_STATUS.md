# Implementation status

> **Settled work.** Do not change, replace or re-style anything in this
> document to suit something new you have been asked to build. If what you are
> building genuinely contradicts what is written here, stop and ask Zain, who
> owns this project: say what you want to change, what the application will
> look like afterwards, and whether it is a gain or a loss. He decides.
> Full rule: [CLAUDE.md](../CLAUDE.md).

**Updated:** 2026-10-06

**Scope:** Part 1 — the Conceptual ERD editor, one page per project, and
Relational Design: a schema worked out from the diagram, or drawn by hand in a
project that starts from its schema. The Home screen starts either. SQL and
Data are not built.

This is the record of implemented behavior. The numbered Product, Architecture,
Domain Model, Scale, and Roadmap documents also describe capabilities that have
not been built yet. Their presence in those documents is not a completion claim.

## Current state at a glance (2026-10-09)

Checked against the code at `3e29264` on 2026-10-09. Each item has its own entry under
*What works*; this list only says what is finished, so that nothing below is
read as still to do.

**Complete.**

- Relational Design's tools, on a schema drawn by hand: *Select | Table |
  Connect*, Select to begin with; Table places a table where the schema is
  pressed, and a double click locks it (*Table 🔒*); Connect hands back
  after one connection unless locked (*Connect 🔒*); Escape and the other
  tools put either down. A schema worked out from a diagram works in Select.
- Connecting tables safely on a schema drawn by hand (Stage 5): a connection starts on the primary key
  it references and makes or uses the foreign key where it is let go;
  a key let go on another table's key offers *Use as PK + FK* or *Create New
  FK*; where it would land is lit green or red while it is drawn; a strip
  under each table stands for the table itself.
- The Schema workspace, Stages 1–4: one selection shared by the canvas, the
  **Schema Explorer** and **Properties**; Properties editable for names,
  types, sizes and rules; a table's columns edited in rows of their own; a
  chosen column's Properties redesigned.
- A key's order kept apart from the order a table lists its columns in
  (Task 4A, format 32), and the stored display order of a derived table's
  columns (Task 4B, format 33) -- foundations only; see below.
- Disconnecting a side of a relationship takes everything that refers to it
  in one step: the names typed over its foreign key, its place in a column
  order, its shaped schema line, a one-to-one choice of it, and the pins on
  its line.
- Projects that start from their schema, drawn by hand and converted into
  their diagram; a foreign key renamed on any schema.
- The **Company Database** and **University Database** examples.
- Relational Design's own examples, **Company Database — Relational** and
  **University Database — Relational**, and its template, **Basic
  Relational Schema** (2026-10-05).
- **A key drawn on a relationship** keys the relationship's own table, and is
  allowed exactly where there is one: a many-to-many or associative
  relationship (Zain, 2026-10-05; ADR-021 §5b, its open conflict closed).
- The two *Table detail* presentations: **Physical schema** (named so on
  2026-10-02; it was *Names, types and constraints*) and **Compact schema**.
- The schema card's **Constraints** cell writes every constraint a column
  has, and pressing it offers the whole set, the keys handled exactly as in
  Properties (committed in `009a4d6`).
- The workspace shell (committed in `0085e62`): the clean-up that followed
  the ribbon -- Model and Theme menus, Conceptual Explorer folding, Home's
  theming; one workspace row on the diagram as on the schema, led by Home,
  *Schema | Conceptual* and the title; the diagram's Search a real field;
  the schema's working tools shown only under Home's tab; and the diagram's
  raft of view controls on the schema.
- Export from whichever workspace is in front, the schema included, through
  the same document and picture exports (committed in `3e29264`).

**Column reordering is not built.** Task 4A is complete. Task 4B is
complete as a foundation: a derived table can be listed in a stored order,
and `Editor::set_column_order` sets one, but nothing in the interface calls
it. There is no user-facing way to reorder a table's columns: pressing a row
on the schema chooses that column and takes hold of its table, so a drag
moves the whole table; no menu, key or panel reorders columns; and a column
added to a schema drawn by hand always goes at the end. The interaction is
pending, and its gesture is not decided.

**Also not finished.** Room to keep dragging schema tables downward is
pending.

What is not built, and what is waiting on a decision, is listed under *Still
to build in Part 1* at the end. The state of the test suites is under
*Verification*.

## What works

- **Approved full-window start cards** (2026-09-27). The raised pale-blue
  window is now the whole card, including its title, three dots, preview and
  centered blue **+ Create** button. The inner preview window and its shadow
  are removed; no divider separates the preview from the action. SQL retains
  its Coming soon badge and disabled route. Create with AI remains constructed
  and available through `set_ai_offered`, hidden by default. Existing preview
  scenes, animation controls, accessibility and route wiring are retained.

- **Optional compact schema view** (2026-09-27). Under Relational Design →
  Appearance → Table detail, **Compact schema** hides the Column/Type/Constraints
  heading row, type and constraint columns, and table configuration footers,
  retaining names, PK/FK
  marks and relationship lines. The choice is remembered; full detail remains
  the default. Compact tables fit their column and table names. Hand-set sizes
  are preserved and restored when returning to **Names, types and constraints**
  (now called **Physical schema**; see below).
  A compact table's header carries its name alone: the *ENTITY …* / *BRIDGE
  FOR …* tag, which could only be cut to a few letters and an ellipsis at that
  width, is left out. While compact, tables are moved but not pulled by their
  edges, so a size given there never folds the full view's columns.

  Its names as they stand (recorded 2026-10-02; nothing renamed). On screen:
  the heading **Table detail**, over **Names, types and constraints** (the
  default) and **Compact schema**. Inside: the setting `schemaNamesOnly`, the
  heading `schemaDetailHeading`, the choices `schemaDetailFull` and
  `schemaDetailNames`, and the view's switch `SchemaView::set_names_only`.
  *Detailed Schema* and *Full Schema* are not names ERDFlow has used, for this
  or for any stage. *Physical Design* was planned as a level and a workspace
  of its own (Roadmap Phase 29); ADR-022 §9.4–9.5 merged physical detail into
  Relational Design instead. Whether the two choices become *Compact schema*
  and *Physical schema* is a decision waiting on Zain (see *Still to build in
  Part 1*). *(Decided and done 2026-10-02 -- next paragraph.)*

  **Renamed 2026-10-02 (Zain).** *Names, types and constraints* is now
  **Physical schema**, its tooltip *Show physical column details, including
  types and constraints.* Only the words changed: the view draws exactly what
  it drew (checked pixel for pixel against the build before the rename, in
  both choices), *Compact schema* is unchanged, and every internal name above
  stays as it was, so a remembered choice opens as the same view. Physical
  schema is the physical detail the schema holds today -- each column's name,
  key marks, SQL type with its length, precision and scale, `NULL` or `NOT
  NULL`, `UNIQUE` and `IDENTITY`, with the provenance and conversion
  questions a diagram's tables already carry. The rest of a physical design
  -- defaults, checks, indexes, constraint names, referential actions,
  computed columns, a chosen dialect -- is not in the model yet and remains
  future work; nothing in the view claims it.
- **A project can start from its schema** (2026-09-27, ADR-021 Step B). Home's
  *Relational Schema* card is enabled: its **+ Create** opens an untitled
  project whose Relational Design fills the window, with no diagram behind it
  (`Editor::new_schema_project`, `SchemaOverrides::standalone`). While it
  starts from its schema, Full, Close and the header's *Relational Design*
  button are put away, since there is no diagram to go back to, and the
  narrowing chips too, since every table came from here. The status line says
  how many tables there are and that there is no diagram yet, and columns are
  drawn upright rather than in the italic that means "not on the diagram".
  - *Its tools are in the header* (2026-09-27). The bar the schema carries in
    a diagram project -- its title, its state, Arrange and Appearance -- is
    put away, since the header already says *Relational Design*, and the
    header carries instead **Table** (the table mark, from each icon set:
    *schema* in the coloured set, *relational* in the line art, drawn in the
    painted one), **Connect ▾**, **Arrange ▾** and **Appearance ▾**, the last
    two opening the very menus the schema's bar opens. What the bar said
    (*2 tables · no diagram yet*, types open, ends not connected) is said in
    the status line where a diagram's counts are. The title gives way before
    the tools do and is said whole on hover, so the header fits a window 1440
    wide. **Convert to Conceptual Design** stands in the Conceptual preview's
    bar beside Close, and on the Design menu. Converting gives the tools back
    to the schema's own bar.
  - *The header reads as groups* (2026-09-27): **← Home**; a two-part switch
    **Schema | Conceptual**, Schema first and lit in the accent, in place of
    the *RELATIONAL DESIGN* badge (Conceptual is the button that raises the
    preview, and lights softly while it is up; Schema puts the preview away);
    a hairline; the title with a pencil that asks for the project's name as
    the Explorer does; room; **Table**, **Connect ▾**, **Arrange ▾** (layers
    mark) and **Appearance ▾** (its own mark) on one quiet, rounded style; a
    hairline; **Undo** and **Redo**, lighter; a hairline; the search, with a
    glass, *Search schema design…*, 240 wide where there is room and down to
    150 where there is not; **Theme ▾**. Nothing in it says *Relational*.
    The title keeps its width up to 140 so the search narrows first. The
    empty schema says *Create a table to start designing your schema.*
    Dressed by stylesheet rules keyed on the header's `schemaFirst`
    property (`MainWindow::wear_schema_first_header`); converting puts the
    diagram's header back exactly as it was.
  - *Connect* is the diagram's tool with its own mark: pressed, a press
    anywhere on a primary key's row (not only its key gutter) draws a
    connection from that key, let go on the table that refers to it or on the
    column there that is to hold it, as below (Stage 5, 2026-10-01; until then
    it drew from any column to the table it pointed at). Used once it
    is put down again; a double click locks it (*Connect 🔒*); Escape or a
    second press puts it down. Its arrow carries how the lines run -- *Around
    the tables* or *Straight there*, the same entries as in Arrange.
  - *Tables* are made by **Table** in the header, or by *Add table here* on
    the empty schema's right-click menu; a double click on the empty schema
    makes none (Zain, 2026-10-06). A table is put where it was asked for and its name opened for
    typing; it starts with one column, an `int` primary key named for the
    table -- `TableID`, and `StudentID` once the table is named `Student`
    (`Editor::create_relation`). A key still called by the table's old name
    follows a rename in the same edit; a key named by hand keeps its name
    (`Editor::rename_table`; Zain, 2026-09-27, replacing the plain `ID`). Columns are added from the slot under a
    table, as on any schema, and named in their row. A column is made the
    primary key, or taken off it, from its right-click menu. A table is
    deleted from its menu or with Delete, taking its foreign keys with it,
    and the History says *Deleted Table "Employee" with its 3 columns*.
  - *A foreign key* is drawn from the key it is to reference: press a
    primary key's key gutter (where `PK` and `FK` are written) and let go on
    the table that refers to it, or on the column there that is to hold it,
    in another table or its own (Stage 5, below: until 2026-10-01 a line was
    drawn the other way, from the column to the key, and could re-point a
    foreign key and retype its column). The line follows the pointer, and the
    row under it is washed in the accent while it starts on a key that can be
    referred to. The column's menu still offers *References ▸ Table.Column*
    (`Editor::add_foreign_key`, which re-points and takes the key's type as
    before) and *Remove the foreign key*. A change to a key's type or size is
    carried to every foreign key pointing at it. Let go anywhere else, it
    says why where the hand let go. A foreign key's own type, and taking the
    key off a column something points at, are refused with the reason
    (`SchemaView::linked`, `MainWindow::link_schema_rows`).
  - *The schema is the main surface, and the Conceptual Design rises from
    below it* (2026-09-27) -- a diagram and its schema turned the other way
    up. The schema fills the stage from the top with no grip at its top edge,
    since it is not a curtain raised over anything. The header's
    **Conceptual Design** button, in the place a diagram's *Relational
    Design* button stands, raises a panel from the bottom of the stage over
    the lower part of the schema: a grip at its top (drag, arrow keys, double
    click for half or full, the height remembered), its title, what it shows
    (*2 entities · 1 relationship · preview only*, and how many things the
    rules could not carry exactly, listed on hover), and **Close**. It shows
    the diagram **Convert** would draw -- the same rules, from the same table
    places and element sizes (`MainWindow::schema_conversion_inputs`) -- on a
    canvas of its own over an editor of its own, worked out again whenever
    the schema changes, and fitted to the diagram when it opens. Nothing is
    converted and nothing is written to the project. It is panned and zoomed;
    anything that would change it (a double click, a right click, Delete,
    typing) is turned away with the reason beside the pointer and in the
    status line, since a change there would be lost with the next edit to the
    schema. Converting puts the preview and its button away and gives the
    schema back its grip; Undo brings them back.
- **A schema drawn by hand converts into its diagram** (2026-09-27).
  **Convert to Conceptual Design** (in the Conceptual preview's bar and on
  the Design menu) draws, in one edit,
  every table as an entity and every foreign key as a relationship, by the
  course rules run backwards (`domain::diagram_from_schema`,
  `Editor::convert_schema_to_diagram`): a table's primary key columns become
  its key attributes (ADR-021 §5a); a foreign key is one-to-many from the
  table it points at, one-to-one where it is `UNIQUE`, the side pointed at
  taking part totally where it is `NOT NULL`; a key into the same table is a
  relationship of the entity with itself, its two sides given roles (the
  key's word, *Manager*, and the table's, *Employee*); and a join table --
  two `NOT NULL` foreign keys into tables that nothing points back at, keyed
  by the pair, by a whole-number key of its own, or by nothing -- is a
  many-to-many relationship with its other columns as attributes. A
  relationship is named for the role a foreign key plays where its name says
  one, and *Has* otherwise. Afterwards the diagram is the model and the
  schema is worked out from it, as in a project begun as a diagram, so edits
  on either side reach the other (Zain chose this over an unlinked copy).
  Table names (the project's naming is set to *As the diagram draws them*),
  column names, types and sizes, `NOT NULL`, `UNIQUE`, `IDENTITY`, what every
  foreign key points at and what it was called, each table's place and size,
  hand-shaped lines, colours and comments are carried across, so the schema
  reads the same. The diagram keeps the schema's arrangement, left to right
  and top to bottom, with each entity's attributes above and below it and
  each relationship placed clear of them; an element whose name would be cut
  short is made wide enough for it (`width_for_name`), and every other is
  made at its default size. What the rules cannot say exactly -- foreign keys
  listed after a table's other columns, a key made for a table that had
  none, a foreign key that was part of a primary key -- is said in the status
  line and beside the diagram. The diagram comes in front with the schema
  open beneath it; one Undo gives the schema drawn by hand back.
- **A foreign key can be renamed on any schema** (2026-09-27). Double-clicking
  a foreign key the conversion made opens its name for typing, as every other
  column's does. A typed name is kept (`SchemaOverrides::foreign_key_names`,
  `Editor::rename_foreign_key`) and no longer follows the key it points at, as
  a typed table name no longer follows its entity; clearing it hands the name
  back to the rule. Its hint says so. This replaces the refusal "A foreign key
  is named for the key it points at", by Zain's choice.
- **The Schema workspace's foundation, and one selection shared by what shows
  it** (Stage 1, 2026-09-27 to 2026-09-29). Whenever the schema is what is
  being worked on, the two side docks hold the Schema's own **Explorer** and
  **Properties**, either side of the schema canvas, which is unchanged;
  whenever the diagram is, they hold the diagram's, exactly as before. This
  holds for both ways into the schema: always, in a project begun from its
  schema; in a diagram's, once **Relational Design** is raised, once the
  schema is pressed, and in **Full** -- and the diagram's come back once the
  diagram is pressed or the schema is put away (`MainWindow::wear_schema_panels`,
  `follow_pressed_half`). In Full the other panels and Home's drawing row still go (the ribbon's tabs stay, 2026-10-06),
  and the header runs the whole width over the two panels, as it does over a
  schema drawn by hand (`lay_header_over_panels`). Both panels began as
  shells; the Explorer's frame was *Schema ▸ Tables, Relationships* with
  their counts, and is now a tree (Stage 2, below).
  Properties began by saying what is chosen and nothing more -- *Schema --
  No object selected.*, *Table* and its name, *Column* and its name with its
  table and whether it is a primary or foreign key, *Relationship* with which
  column points at which, or how many tables are gathered -- and is now a
  read-only inspector (Stage 3, below). What is chosen is kept in one place, the schema view, as a
  `SchemaSelection` (`app/desktop/schema_selection.hpp`: nothing, a table,
  several tables, a column, or a foreign key), read through
  `SchemaView::selection_now` and set through `SchemaView::choose`; the
  canvas's highlight and Properties both read it, so they cannot disagree. It
  is named by the schema's own identities -- a table by its `RelationId`, a
  column by its table and its `SchemaColumnId`, a line by its `ForeignKeyId`
  -- never by a name, a row or a place on the screen, so it survives a
  rename, and never by the diagram's. A heading chooses its table; a row, its
  type or its constraints choose the column, whose row wears a faint wash of
  the accent; a line chooses the foreign key it stands for, puts the tables
  down and is drawn lit; the empty schema chooses nothing. Choosing is how
  the schema is being looked at: it writes nothing, marks nothing unsaved,
  adds nothing to Undo and is not saved with the project. The Conceptual
  canvas is untouched by any of it. A column worked out from a diagram has
  no `SchemaColumnId` of its own yet, so it is named by its table and what
  the schema keeps it by -- its foreign key and part, the table's generated
  key or discriminator, or, as provenance rather than identity, the
  attribute it came from; giving such columns identities of their own is
  for a later stage. Later stages, not built: editing keys, foreign keys
  and relationships in Properties; making a foreign key where none is found;
  the conversion rules; checking the schema.

  *Since built (noted 2026-10-02):* Properties edits names, types, sizes and
  rules (Stage 4), a table's columns in rows of their own, and a chosen
  column's primary key and, on a schema drawn by hand, its foreign key,
  through the canvas's own paths (the Column Properties redesign); a
  relationship's own facts are still read-only there. Making a foreign key
  where none is found is Stage 5's *Create & Connect*. Checking the schema is
  still not built. A column worked out from a diagram still has no
  `SchemaColumnId`, but every column now has a stable identity made of the
  model's own (`ColumnIdentity`, Task 4B), which a column order is kept by;
  the selection handle described above is unchanged.
- **The Schema Explorer is the schema as a tree** (Stage 2, 2026-09-29). In
  the schema's own words only: *Schema* ▸ *Tables* (counted) ▸ each table by
  its name, in the schema's order ▸ *Columns* (counted), *Primary Key* and
  *Foreign Keys* (counted); and *Relationships* (counted) ▸ every foreign
  key again, named with the table that holds it (`Student.CourseID →
  Course.CourseID`). A column says what kind of key it is at the end of its
  row -- *PK*, *FK*, or *PK FK* for a column that is both -- and wears the
  golden key where it is a primary key, the link where it is a foreign key,
  and both side by side, in the one icon slot, where it is both (Zain,
  2026-10-01; until then a PK + FK column wore only the key, `look.both`).
  A primary key of several columns is
  listed whole and counted; a foreign key of several columns is one row,
  `(A, B) → Table(A, B)`, never one per column. A key into its own table is
  listed pointing back into it, with no second table. A table with no
  foreign keys shows no empty group for them. A value the conversion leaves
  out (a derived one) is listed as the schema lists it, slanted, and never
  counted. Everything is read from what the schema view shows
  (`fill_schema_explorer`, `app/desktop/schema_explorer.*`), in both ways
  into the schema, and nothing is kept beside it. Every row that stands for
  something carries the schema view's own `SchemaSelection` and a key made
  from the schema's identities (`schema_key`), never from a name or a
  place. Pressing a row chooses it on the schema through
  `SchemaView::choose`, brings it into view there and shows it in
  Properties; several tables gathered are several tables. Choosing on the
  canvas lights its rows in the Explorer -- every row for it, as the
  diagram's Explorer lights an attribute twice: a column under Columns and
  under Primary Key, a foreign key under its table and under Relationships
  -- and opens the tree onto it (a foreign key under Relationships, so no
  table has to open). The tree is made afresh whenever the schema changes;
  what was opened or folded by hand is kept by what it stands for, the list
  stays where it was scrolled, and what is chosen stays lit, under its new
  name after a rename; a table deleted takes its rows and its lit rows with
  it. Groups, tables and columns open as the diagram's do the first time:
  the schema and its two groups open, each table folded with its columns
  open and its keys folded. Nothing in the tree can be edited, and choosing
  there writes nothing and adds nothing to Undo. The tree is drawn by the
  diagram Explorer's own tree and delegate; a glyph keeps its resting
  drawing on an open row (its "on" drawing is inked for the accent and all
  but vanished there) and takes the accent-ink drawing only when chosen.
  Long names are cut with an ellipsis and said whole on hover.
- **Schema Properties is a read-only inspector** (Stage 3, 2026-10-01).
  **Built read-only; Stage 4, below, made a table's name and a column's
  name, type, size and three rules editable, and nothing else.** Whatever is
  chosen on the schema -- on the
  canvas or in the Explorer -- Properties says what it is, under folding
  section headings drawn as the diagram's panel draws its own, each value
  written plainly with no field around it, cut short with an ellipsis where
  the panel is narrow and said whole on hover. It reads the schema view's one
  `SchemaSelection` and the schema as it is now, and keeps nothing, so a
  rename, an Undo or Redo, or a change the conversion makes is shown at once,
  and something deleted while chosen is never described afterwards. Only what
  the schema holds is said; nothing a database might also have (a default,
  a constraint name, cardinality) is made up. With **nothing chosen**:
  *Schema*, counting its *Tables*, stored *Columns*, tables with a *Primary
  Key*, and *Foreign Keys* (a key of several columns counted once). A
  **table**: its *Name*; its *Columns* (a derived value listed with them is
  said not to be one); its whole *Primary Key*, several columns where it is
  composite and *None* where there is none; its *Foreign Keys*, counted and
  listed in the Explorer's words; and what it is *Referenced By* -- a key
  into its own table counted once on each side. A **column**: *Name*,
  *Table*, *Data Type* as its Type cell writes it (*Not set* until chosen);
  *Nullable*, *Unique* (a key of one column is unique as the key; one column
  of several is not) and *Identity*; its *Key Role*, *Primary Key*, *Foreign
  Key* or *Primary Key + Foreign Key*, each read on its own; the whole
  primary key it belongs to; the whole foreign key it belongs to, with which
  *Member* of it it is where the key has several columns; what it
  *References*; and what is *Referenced By* it. A **value the conversion
  leaves out** is headed *Derived value* and said not to be stored, with
  nothing a column would have. A **foreign key**: *Foreign Key*, *From*,
  *To*, *Self Reference*; then its *Referencing* and *Referenced* table and
  column, or columns in order for a key of several, which stays one key. In
  a schema worked out from a diagram, tables and columns also say their
  *Source*: the Conceptual diagram, generated by the conversion (with why,
  for a key made because nothing identified the table), or Relational Design
  only. Several tables gathered say how many, as before. The foreign keys and
  primary keys are read by one shared module (`app/desktop/schema_facts.*`)
  that the Explorer now reads too, so the two say the same key the same way;
  the type is written by the schema's own `written_type`. One implementation
  serves both ways into the schema. Choosing and reading write nothing, mark
  nothing unsaved and add nothing to Undo; the Conceptual Properties panel is
  unchanged. A column belongs to at most one foreign key, because the schema
  gives each column at most one; a schema drawn by hand makes each foreign
  key one column wide, so a key of several columns arises only from a
  conversion. (The column view described here was redesigned on 2026-10-01;
  see "A chosen column's Properties, redesigned". The foreign-key view was
  redesigned on 2026-10-02; see "A chosen relationship's Properties,
  redesigned".)
- **Schema Properties edits a table's name and a column's own values**
  (Stage 4, 2026-10-01). **Editable: a table's Name; a column's Name, Data
  Type, Size (for a type that is measured), Nullable, Unique and Identity.
  Still read-only: the primary key and Key Role, foreign keys and which
  column belongs to one, References and Referenced By, everything about a
  relationship (From, To, Referencing, Referenced, Self Reference), Source,
  the overview's counts, and a derived value, which is not a column.
  Editing keys, foreign keys and relationships from Properties is not
  built.** An editable value is a field and looks like one; everything else
  stays written plainly as in Stage 3. Every change goes exactly where the
  same change made on the schema goes, and nothing in Properties writes to
  the model itself: a name to `MainWindow::rename_from_schema` (the canvas's
  double-click rename), a type or a size to the schema's own type and size
  pickers (`ask_column_type`, `ask_column_size`, opened from the field), and
  a rule to `toggle_schema_constraint` (the canvas's list of constraints).
  So each is checked by the Editor, refused for the same reasons and with
  the same words, is one step of Undo, marks the project unsaved, and
  reaches the diagram exactly where the canvas's edit does: in a schema
  worked out from a diagram a table's name is its entity's, a column's name,
  type and rules are its attribute's, and a foreign key's Nullable and
  Unique are the relationship's participation and cardinality; in a schema
  drawn by hand each is the schema's alone. A name commits on Return or on
  leaving the field, is ignored unchanged, and is put back on Escape, as the
  canvas's name box behaves; a field put away because the panel is made
  afresh (something else chosen, the thing deleted) writes nothing. Where
  the model turns a change away, the panel is made afresh from the model so
  no field goes on showing what was refused. Refused as before: a primary
  key made nullable; a foreign key's type changed where it takes its key's
  (the key's own type changes, and the foreign key follows in the same
  edit, as the model already did); Identity on a type that is not a whole
  number; a column given no name; a name the model's text rules refuse. A
  key of one column shows Unique ticked, "as the primary key", and pressing
  it says a primary key is unique already rather than changing anything. The
  model has no rule against two tables, or two columns of one table, sharing
  a name, and Properties adds none: such a rename is accepted, as it is on
  the canvas. The type of a foreign key, a key or a discriminator the
  conversion made has nothing behind it to change; pressing its field says
  so. The Conceptual Properties panel is unchanged.
- **Schema tools: Select, Table and Connect** (Zain, 2026-10-01). A schema
  drawn by hand has *Select | Table | Connect | Arrange | Appearance* in its
  header, one tool in hand at a time, Select to begin with (the pointer glyph
  and checked look of the other tools). **Table** no longer makes a table when
  pressed: it is a placing tool, its pointer a cross as on the diagram, and a
  press on the schema makes the table there (header under the pointer)
  through the same `add_schema_table` the empty schema's *Add table here*
  uses, then hands back to Select -- before the new table opens its name, as the
  diagram's placing tools hand back. A double click locks Table (*Table 🔒*)
  for placing several; pressed again, as Connect is, it is put down.
  **Connect** keeps every Stage 5 rule and hands back to Select after one
  use unless locked, as before. Escape puts Table or Connect down and lets go
  of a line half drawn (with Select in hand, one drawn from a key's gutter
  too); pressing Select or the other tool puts the one in hand down; a press
  anywhere else in the window puts it down, the schema and its scrollbars
  excepted, by the diagram's own rule. With Table locked, each table placed
  opens its name for typing, so the first Escape closes that name -- the
  table kept with the name it was given, Table still locked -- and a second
  puts Table down. This is intended: it is what the diagram's locked Note
  does, the one placing tool there that opens a name (checked 2026-10-02
  with real input against both; the diagram's other placing tools open no
  name, and one Escape puts them down, as one puts Table down when no name
  is open). Tool state is the interface's only:
  nothing is written, marked unsaved or added to Undo. A schema worked out
  from a diagram has no header tools and works in Select, as its only tool.
  With Select in the row, the header no longer had room at 1440: there,
  **Arrange** and **Appearance** show their icons alone (named on hover),
  taking their words back wherever every word fits (`HeaderToolButton`,
  `fit_schema_header_words`).
- **Where a connection would land is lit** (Zain, 2026-10-02). While a
  foreign key is being drawn on a schema drawn by hand -- with Connect, or
  from a key's gutter -- the place it would land is lit by what letting go
  there would do, asked of the very plan the drop follows
  (`plan_connection`, through `SchemaView::link_target`): in the theme's
  *valid* colour, solid, where it goes on -- Create & Connect, Use &
  Connect, the PK-to-PK choice, or an existing identical foreign key, which
  is reported rather than refused -- and in its *error* colour, dashed,
  where it is turned away (another type, a column already referring
  elsewhere, a non-key or composite-key start, a column referencing itself).
  The row under the pointer is lit with a faint wash and an outline of its
  own; the rest of a table gets a ring standing clear of its border, so it
  is never taken for the outline of a chosen table. The empty schema lights
  nothing. Under Plain both colours are greys and the solid or dashed line
  tells them apart. Nothing is lit until the line leaves its own row, and it
  is worked out from the line itself, so it goes out with it: on letting go,
  on Escape, on putting Connect down, and when the schema is hidden with a
  line half drawn (which now lets go of the line, as Escape does). It is not
  a selection and changes nothing. This replaces the accent or grey wash
  the row under the line used to get, which said only whether the line had
  started on a one-column key.
- **A key's order is its own** (Zain, 2026-10-02; format version 32). What a
  key of several columns is, and what each part of a foreign key points at,
  no longer follows the order a table lists its columns in. Each column added
  on the schema carries `key_order`, its place in its table's primary key
  (`SchemaColumn::key_order`, kept in step by `number_primary_key`: a column
  joining the key takes its place where it stands in the list, a column
  leaving or removed closes the key up behind it). The conversion reads every
  key in that order -- its own columns in the order it makes them, the ones
  added on the schema by their places -- and gives each table its key as an
  explicit list (`PreviewTable::primary_key`), taken once at the end, which
  is what Properties, the Explorer, Connect and the conversion into a diagram
  now read; a generated foreign key's parts, and a name typed over one (kept
  by part), count along that list. Foreign keys drawn by hand were already by
  identity. A file before version 32 is given the order its key was read in
  then, once, as it opens, so it means exactly what it did. Nothing on the
  screen changes. The conceptual model still has no order of its own for a key
  of several attributes: the conversion takes them in the order of their
  identities, as before.
- **A derived table's columns can keep an order** (Task 4B, 2026-10-02;
  format version 33). The foundation only: nothing on the screen offers it
  yet, and with no order given every table is listed exactly as before.
  `SchemaOverrides::column_order` keeps, per relation, the order its columns
  are listed in, by `ColumnIdentity` -- the attribute a column came from, its
  own `SchemaColumnId`, a foreign key's `{key, part}`, the relation a key was
  invented for (`InventedKeyColumn`), or the specialization a discriminator
  serves (`DiscriminatorColumn`) -- never by name or position.
  `PreviewColumn` now records the last two (`invented_for`,
  `discriminates`), and `column_identity` names any column. `schema_preview`
  lists each table that has an order after its key is taken: named columns
  first, the rest in conversion order; `primary_key` and every
  `references_column` into the table are carried to the new rows, so a key
  and every foreign key mean what they meant. A column named that the table
  does not hold at the moment is passed over and kept, so it comes back to
  its place. `Editor::set_column_order` is the one edit that changes an order
  -- one undo step, refused for a schema drawn by hand, keeping the place of
  a column that is away; empty clears it. Deleting an element, removing a
  schema column or disconnecting a side takes its columns out of every order
  in the same step. Validation refuses an order for a table or column the
  model can no longer make, or one naming a column twice, but not a column
  this table simply does not hold today.

  Reordering columns is **not** built on top of it yet. Nothing in the
  interface calls `set_column_order`. Pressing a row on the schema chooses
  that column and takes hold of its table, so a drag moves the whole table;
  no menu, key or panel moves a column within its table; and a column added
  to a schema drawn by hand always goes at the end. Task 4A is complete;
  Task 4B is complete as this foundation; the reordering interaction is
  pending, with its gesture undecided.
- **A derived table lists its columns by key role, and an owner's attributes
  in the order they were made** (2026-10-03; format version 34).
  `schema_preview` lists each table worked out from the diagram in three
  groups once its key is taken: the primary key's columns, a column that is a
  foreign key as well among them; then every other column; then the columns
  that are only foreign keys. Each group keeps the order the conversion made
  it in, and `primary_key` and every `references_column` are carried along, so
  no key or foreign key changes meaning. A stored column order (Task 4B) is
  still applied after it, and a schema drawn by hand is not regrouped. Within
  the conversion an owner's attributes -- and a composite's parts, in the
  composite's place -- come in the order they were created:
  `Attribute::creation_order` is one past the largest in the project when an
  attribute is made, and again the first time an attribute still called
  *Attribute* (the name the canvas places it with), or nothing, is given a
  name of its own -- so ellipses placed first and named afterwards list in
  the order they are named. Naming it again, or changing its owner, never
  changes it; it is given anew, after everything already there and in the
  originals' order, to what Duplicate, Import and *Convert to Conceptual
  Design* bring in. Equal
  numbers, possible only in a project built some other way, fall back to the
  order of identities. This replaces the identity order the Task 4A entry
  above describes for a key of several attributes. The Conceptual Explorer
  lists attributes in the same order. Format 34 writes the number on every
  attribute; an older file is numbered as it opens in the order of its
  identities, which is how it listed before, and opening it neither writes the
  file nor leaves the project unsaved. Foreign keys, invented keys and
  discriminators keep, within their group, the order the conversion gives
  them: a foreign key follows its relationship's identity, which no stored
  order governs.
- **Disconnecting a side takes the names typed over its foreign key**
  (fixed 2026-10-02). `Editor::disconnect` left them behind, and a name for
  a key that is gone is refused, so a side whose foreign key had a part
  named by hand could not be disconnected at all ("A foreign key name refers
  to a missing key."). Every name kept against that side's `ForeignKeyId`,
  each part of a key of several columns included, now goes in the same step
  as the side, as deleting the side already did; one undo brings them back
  exactly. Names of any other key are kept, including the key whose other
  end was disconnected, which comes back under them when that end is
  connected again.
- **Disconnecting a side takes everything that refers to it** (fixed
  2026-10-02). Three more things were left behind by `Editor::disconnect`,
  each refusing it: the shape given to its key's line on the schema ("A
  schema line refers to a link that no longer exists."), a one-to-one key
  chosen to be kept on that side ("The chosen side does not belong to this
  relationship."), and a comment pinned to the side's line on the diagram
  ("A comment is pinned to something that is no longer there."). They now go
  in the same step, by deleting's own rules: the line's shape is dropped;
  the choice is cleared and handed to no other side, while a choice of the
  other side stands; the comment is unpinned from the line, and a comment
  pinned to nothing else goes, as one does when what it is pinned to is
  deleted (`unpin_comments`, now shared by `erase` and `disconnect`).
  Disconnecting a side now leaves exactly what deleting that side leaves, and
  one undo restores all of it.
- **A foreign key the conversion makes keeps its key's scale** (fixed
  2026-10-02). It took the key's type and length but not its scale, so a
  foreign key into a `decimal(10,2)` key read `decimal(10,0)` -- on the
  canvas, in Properties, and for anything else reading the schema. It now
  takes all three, so it reads `decimal(10,2)`, each column of a key of
  several columns taking its own; a bridge's foreign keys are made the same
  way; and a key whose size changes is followed, since the schema is worked
  out afresh from the diagram. A foreign key drawn by hand always took the
  scale, and follows its key's size through `carry_type`, as before.
- **A strip under each table to let go on** (Zain, 2026-10-02). While a
  line is drawn, a row's height just under every table (2 px below its
  foot, where the slot for adding a column stands, which gives way to it
  while the line is drawn) is offered faintly -- dashed, empty, unworded --
  and stands for the table itself: hovered, it is lit and the table ringed
  by the table's own plan, as its heading would be; let go on, it is let go
  on the table, so Stage 5 decides as it always does (Create & Connect, Use
  & Connect, refused, already there). A row is always found first, then
  the strip, then the rest of a table, alike while drawing and on letting go
  (`SchemaView::link_spot`). It is read off where the table is drawn now
  (`drop_strip`), so it is under the last row however many there are, or
  under the heading of a table with none, and goes with the table when it
  moves or grows. It is no column, no selection and nothing kept: no model
  object, no Explorer or Properties entry, nothing saved or undone.
- **A chosen column's Properties, redesigned** (Zain, 2026-10-01; replaces
  the Stage 3/4 column view described above). **Properties**, then one card:
  a blue column mark (a cylinder), **Column** in the accent and bold, the
  column's name, and *Table.Column*. Then five folding sections, each with a
  mark in the theme's accent and open until the hand folds it (remembered
  under `schemaColumnPropertySection/<title>`, apart from every other
  inspector): **Column Info** (where General was: *Name*, typed over through
  `rename_from_schema`; *Table*, read; *Data Type*, the type with its size in
  one field, chosen from the schema's own list, a measured type then asking
  its size as the table list's rows do -- there is no separate Size field;
  *Source*, read, with its note where the conversion made a key);
  **Constraints**, one row each with its mark, name, meaning and a switch --
  *Primary Key* (the golden key) / *This column is the primary key*,
  *Foreign Key* (a chain) / *References a column in another table*, *NULL*
  (a dashed circle) / *May be empty*, *NOT NULL* (a prohibited sign in the
  theme's error colour) / *Required — cannot be empty*, *UNIQUE* (a
  fingerprint) / *No duplicate values*, *IDENTITY* (a lightning bolt) /
  *Auto-generated number*; **Key Details** (where Keys was: *Key Role*;
  *Key Column*, or *Key Columns* for a composite key, listed whole; a
  primary key and foreign key over different columns also says its *Foreign
  Key Column*, and a member of a foreign key of several columns its
  *Member*); **References** (what a foreign key *References*, and
  *Referenced By* counted, one table row to a line, *None* at zero); and
  **Source** (*Conceptual diagram*, *Generated by the conversion*,
  *Relational Design only*, or *Schema-first*). The switches are check boxes
  underneath (`RuleSwitch`) and do what the canvas does: NULL and NOT NULL
  are one fact, so either switch turns it over and they are never both on;
  UNIQUE and IDENTITY toggle as before (a key of one column shows UNIQUE on
  and says why it cannot be pressed off); *Primary Key* triggers the row
  menu's own key action; *Foreign Key* off is the row menu's *Remove the
  foreign key*, and on offers the keys a foreign key can reference and then
  takes the path a line drawn by Connect takes (Stage 5: checked first,
  asked about, never retyping), the column staying chosen. Where a switch
  cannot apply -- a key or foreign key the conversion made, a foreign key on
  a schema worked out from a diagram -- it is still pressable and says why.
  The table view, the derived-value view and the relationship view are
  unchanged. New line-art drawings, Lucide, inked from the theme:
  `database`, `file-text`, `shield`, `link`, `zap`, `ban`, `circle-dashed`,
  `fingerprint`.
- **A chosen relationship's Properties, redesigned** (Zain, 2026-10-02;
  replaces the Stage 3 foreign-key view described above; awaiting Zain's
  review of the screenshot). Laid out as a chosen column is, saying what
  the old view said and nothing new, still only read: nothing is offered,
  nothing reaches the Editor. **Properties**, then one card: the Schema
  Relationships mark, **Relationship** in the accent and bold, the two ends
  as `key_words` writes them (*Students.ProfessorID → Professors.ID*, or
  *Shipments(InvoiceNo, InvoiceYear) → Invoices(InvoiceNo, Year)*), carried
  onto a second line between words where the panel is narrow (an arrow
  that does not fit starts the next line) and cut short with the whole on
  hover only where a single name is wider than the panel; then *Foreign
  Key*. Then three folding sections, each with its mark and open until the
  hand folds it (remembered under `schemaRelationshipPropertySection/<title>`,
  apart from every other inspector): **Relationship Info** (where General
  was: *Type* Foreign Key, *Self Reference* Yes/No, and for a key of several
  columns *Column Pairs*, each own column → the column it references, in the
  key's own order from `gather_keys`); **Referencing** (the table holding
  the key, then its columns in the key's order, each with the marks it wears
  on the schema -- the golden key and orange **PK** where it is a primary
  key, the green chain and **FK** -- so a PK + FK column keeps both; a key
  of several columns is one card, its columns counted); a small *↓
  references*; **Referenced** (the table, then the columns pointed at, the
  golden key and **PK** where each is a primary key). From and To are said
  once, in the card, no longer again under General. Roles are said in
  words and marks as well as colour, and under Plain everything is grey. A
  line that goes away while chosen (disconnected, undone) leaves the
  schema summary, as before. Field keys: `Identity/Relationship`,
  `Identity/Kind`, `General/Type`, `General/Self Reference`, `General/Column
  Pairs`, `Referencing|Referenced/Table`, `/Column` or `/Columns` (counted),
  `/Role`; `General/From` and `General/To` are gone. The table and column
  views are unchanged.
  **Reorganised the same day (Zain, 2026-10-02): General and Ends.** The
  identity card is as above. *Relationship Info* is called **General** again
  (same card: Type, Self Reference, Column Pairs for a composite key).
  *Referencing* and *Referenced* are no longer sections of their own: one
  **Ends** section (the chain mark) holds two cards, the **Referenced end**
  first, then the **Referencing end**, each saying which end it is, its
  table, its *Column* or *Columns* (counted) with the marks above, and its
  **Cardinality**: *One (1)*, *Zero or One (0..1)*, *One or Many (1..N)* or
  *Zero or Many (0..N)*, one per end however many columns the key has. The
  cardinality is only read (Zain's choice): the schema keeps none of its
  own, so it says what the line is drawn with -- the referencing end from
  the foreign key (`optional_link`, from whether it may be empty;
  `one_to_one`, from whether it is unique or a key, or for a key the
  conversion made from the diagram's participants), the referenced end
  always *One (1)*. It changes where it always has: the foreign key column's
  NULL / NOT NULL and UNIQUE, on the canvas and in Column Properties. The
  *↓ references* line is gone with the sections it stood between. **No
  Constraints section** (Zain): the schema has no ON DELETE or ON UPDATE,
  and Mandatory would only be the column's NOT NULL again; it comes when
  referential actions are part of the model. Folding is remembered as
  `schemaRelationshipPropertySection/General` and `/Ends`, both open the
  first time. New field key: `Referencing|Referenced/Cardinality`.
  Found while checking (not changed): making a foreign key the conversion
  made UNIQUE turns its one-to-many relationship one-to-one, and the
  conversion then puts the key on the other table by default --
  *Students.ProfessorID* becomes *Professors.StudentID*. That is the
  existing UNIQUE switch; it is why the cardinality here is not offered as a
  choice.
  *(Since 2026-10-02 the UNIQUE switch asks instead -- next entry.)*
- **Making a generated foreign key UNIQUE asks which table keeps it** (Zain,
  2026-10-02). Where turning UNIQUE on for a foreign key the conversion made
  would turn its relationship one to one -- the side carrying the key is
  many, the side it points at is one -- ERDFlow asks before changing
  anything, because the relational conversion rule allows either table to
  hold a one-to-one's foreign key. *One-to-One Relationship*: "Making
  "ProfessorID" UNIQUE changes this relationship to one-to-one. Choose which
  table should keep the foreign key:", then each answer with the key it
  leaves -- **Keep FK in Students** (the default: *Students.ProfessorID →
  Professors.ID*), **Move FK to Professors** (*Professors.StudentID →
  Students.ID*, worked out by the conversion itself from a copy of the
  project, so nothing is made to show it), and **Cancel**, which changes
  nothing. For a table's link to itself the two answers read *Keep FK as
  <column>* and *Move FK to the other side*. Either answer is one edit
  (`Editor::set_cardinality(participant, maximum, keeps_key)`, "Make the side
  one"): the side made one, as before, and the relationship's existing
  one-to-one key (`ConversionDecisions::one_to_one_key`) set to the side
  chosen, replacing any older answer. Kept, the key is the same key, so its
  typed name, its line and its place in the table stay with it; moved, the
  conversion makes it on the other table from that answer. A key of several
  columns is kept or moved whole. Turning UNIQUE off, a column that is not a
  foreign key, a hand-drawn schema, a bridge's keys and a one-to-one drawn on
  the diagram are as they were and ask nothing. No format change: the answer
  is the one-to-one answer already saved.
- **Direct table Properties and a Schema Relationships icon** (2026-10-01).
  Selecting a table shows **Properties**, then one **Table Name** editor with
  its table icon, and one compact editable row for every stored column. There
  is no repeated table/name header. Rows are numbered *1, 2, …* only for
  presentation; their handles are the
  existing `SchemaColumnRef`s. Names reuse the Stage 4 `NameField` and
  `rename_from_schema` path, including validation, propagation and Undo/Redo.
  PK shows the existing golden key and **PK**; FK shows green **FK**; a
  column that is both shows both independently. The rows are where a
  column is edited, in place: there is no **…** menu and no *Column
  Properties* item, and pressing a row's background or number (it takes no
  focus) changes nothing, so the table stays chosen and its list stays put
  (Zain, 2026-10-01; until then those opened the detailed column inspector,
  where it was hard to get back to the list). A column's own inspector is
  still shown when the column is chosen on the canvas or in the Explorer.
  Clicking the name field edits it in place. The name takes all the room the
  other two do not need (at least 56 px); **Data Type** is as wide as
  `VARCHAR(255)` / `DECIMAL(10,2)` need and **Constraints** 72 px, both
  giving up room only where the panel is too narrow for all three. Each row
  also offers one **Data Type** control, showing
  size/precision/scale in the value, and one **Constraints** control. Type
  selection uses the existing type list and opens the existing size picker
  for measured types (reselect the current type to edit its size). Constraints
  reuse the canvas's PK/FK actions and Nullability, Unique and Identity menu
  actions; no validation or command path is duplicated. The menu is
  classified (Zain, 2026-10-01): **Primary Key**, **Foreign Key** (taking a
  foreign key off at its foot), a rule, **NULL — may be empty** and **NOT
  NULL — required** as two opposite choices (the column's own ticked; the
  other is the same nullability toggle, refused where it always was; the
  ticked one changes nothing), a rule, **UNIQUE — no duplicate values** and
  **IDENTITY — auto-generated number**. Foreign Key appears where it did
  before, on tables drawn by hand. The canvas's own constraints list keeps
  its words (**SUPERSEDED 2026-10-08**: the canvas's list is now the whole
  set as well -- see "The card's Constraints cell" below). Independent states
  appear together, with elision and complete tooltips for narrow docks.
  **+ Add Column** uses the same add command and canvas naming as before.
  Non-stored derived values are listed separately with
  their existing read-only inspector. Table key details, References and Source
  remain available below the rows. **Keys**, **References** and **Source**
  have icon headers and rounded fact cards, expanded by default. Their manual
  fold choices are remembered separately from the detailed inspectors. Primary
  Key lists the complete key (including composite keys); Foreign Keys and
  Referenced By have explicit count badges, including zero, and reuse the
  existing gathered-key and incoming-reference facts. Source shows
  **Schema-first**, **Conceptual diagram**, or the existing schema-only source
  when applicable. Folding only changes a UI preference. Column and
  relationship inspectors and the Schema overview retain their existing
  presentation and commands. Only the Schema Explorer's **Relationships**
  group uses the new three-connected-nodes glyph; Conceptual relationships,
  foreign-key row marks and Connect keep their existing icons. The new UI and
  glyph follow the existing themes, including monochrome, and all icon modes.
  Relationship logic and Stage 1–5 semantics are unchanged.
- **Safe schema relationship creation and foreign-key resolution** (Stage 5,
  2026-10-01). **The invariant: connecting tables never silently repurposes
  or removes an existing primary key.** Zain's rule: the row a connection
  starts on is the key being referenced and stays exactly what it is; the
  table it is let go on is the referencing side, which holds or receives the
  foreign key (END.FK → START.PK). Direction comes from where the line
  started, never from where tables sit or the order they were made. Nothing
  is changed until it is agreed to, and what is agreed to is one edit
  (`Editor::connect_foreign_key`), so one Undo takes all of it back and one
  Redo puts all of it back under the same identities. What a drop would do is
  worked out first, by identity, not name (`plan_connection`,
  `app/desktop/schema_facts.*`):
  - *Let go on the table*, it looks for, in order: a foreign key there
    already referencing this key (*Relationship already exists*, nothing
    made); a column there called what a new one would be -- the key's name,
    or `Parent` and the key's name for a key into its own table, as the
    conversion names one -- which is offered if it can hold the key; and
    otherwise *No foreign key found*, offering **Create & Connect**: a new
    column, appended to the table, of the key's type and size and nothing
    else of the key's (not its key role, uniqueness or counting up), nullable
    as every new column is.
  - *Let go on a row*, that column is the one offered: an ordinary column is
    *Existing column found* → **Use & Connect**. A primary-key column asks
    *Use primary-key column for relationship?* and offers two ways (Zain,
    2026-10-01; until then **Use as PK + FK** alone), both referring to the
    key started on: **Use as PK + FK** -- the key let go on stays the key and
    is the foreign key as well -- or **Create New FK** -- both keys are left
    exactly as they are and the table gets a column of its own, found or
    named by the table-drop rules above (`plan_new_column`): a foreign key
    already there to this key is used and nothing made; a compatible
    ordinary column of that name is asked about (*Existing column found*,
    a second box); where the name is taken by a column that cannot hold the
    key -- a key, one of another type, one already referencing elsewhere --
    *Foreign key column name* asks for the new column's name (Zain,
    2026-10-01; `QInputDialog` "schemaConnectName", **Create & Connect**),
    proposing the referenced table's name and its key's (`CustomerID` for
    `Customer.ID`; the key's name numbered, `TableID2`, where it already
    starts with its table's), editable, never taken silently; an empty or
    taken name is asked about again, saying why. Buttons Cancel, Use as PK +
    FK, Create New FK in that order (Create
    New FK takes `YesRole` so macOS puts it at the right rather than left of
    Cancel); Return presses Create New FK, Escape Cancel. Making the key
    only a foreign key (PK → FK) is never offered. A key column is never
    taken for a table let go on.
  - *Refused*, said where the hand let go: a line started on anything but a
    primary key (*Cannot create relationship*; Zain chose this over keeping
    the old gesture); a key of several columns (composite foreign keys cannot
    be drawn by hand yet, and no key to part of one is made); a column already
    referencing another key (a drag never re-points a foreign key, Zain's
    choice; remove it first); a column whose type would have to change (a
    column takes the key's type only where it has none yet,
    `domain::takes_key_type`); a column referencing itself; the same foreign
    key again.
  - Cancel changes nothing at all. Reusing a column and then undoing leaves
    the column as it was, with only the foreign key gone. The Editor command
    refuses the same cases itself, whatever calls it.
  - The foreign key made is chosen, so its line is lit, its Explorer rows
    are lit and Properties shows it.
  A schema worked out from a diagram takes its foreign keys from the
  diagram's relationships: Connect is not offered there, as before, and
  `connect_foreign_key` refuses its tables. Many-to-many is not drawn
  directly: one line is one foreign key. Fixed 2026-10-01: the buttons
  *Create & Connect* and *Use & Connect* drew as "Create  Connect" / "Use
  Connect" on macOS, the lone `&` being taken for a shortcut mark; written
  `&&` now. Not changed: the column menu's
  *References ▸*, which can still re-point a foreign key and retype its
  column.
- **Create with AI is switched off for now** (2026-09-26). One constant,
  `create_with_ai_offered` in `start_route_card.hpp`, is `false`: every
  card still makes its Create with AI exactly as before, but does not show
  it, and **+ Create** stands alone in the middle of the card at the size
  it has beside it. Setting the constant to `true` brings the pair back
  exactly as it was, + Create on the left and Create with AI on the right,
  the two centred together (`StartRouteCard::set_ai_offered` does the same
  for one card). The card's size and content are unchanged.
- **A Home card's two actions light up under the pointer** (2026-09-26).
  Pointed at, **+ Create** is drawn brighter with a pale edge, and **Create
  with AI** takes a soft blue tint and an accent outline; each lights alone,
  and is exactly as it was at rest once the pointer leaves. Create with AI
  lights up though it is disabled (a style sheet gives a disabled button no
  hover, so the card marks it `lit` as the pointer enters and leaves, and
  styles the mark). Size, place, words, the spark, the pressed look and what
  pressing does are unchanged, and so is the card
  (`StartRouteCard::eventFilter`, the `lit` rules in `StartRouteCard::wear`).
- **The shared-name type list is only as wide as its entries** (2026-09-26).
  Under **Columns that share a name**, each name's **Give them all a type…**
  box is stretched across its row, and Qt opened its list at least as wide as
  the box, most of the window. The list now opens only as wide as its widest
  entry (with room for its scrollbar and a menu entry's margins), from the
  box's left edge; the box itself, the entries, their order and what
  choosing one does are unchanged (`SnugComboBox` in `main_window.cpp`, used
  for that box alone). The width is measured from the entries' words in the
  lettering each is drawn in: on macOS the list is drawn as a menu, whose
  entries report the list's width rather than their words', and the first
  version, sized by that, cut longer entries short (fixed the same day). The
  families' titles in that list (— Exact numerics — and so on) are set a
  little bold and in the theme's grey, so they read as headings over the
  types beneath them.
- **Back to Home, always** (2026-09-26). **← Back to Home** stands first in
  the workspace's header whatever the project is and however it was opened,
  on the diagram and on the schema, whether the schema shares the stage or
  fills the window, since Home is the door every project is come in by and
  a change of mind can always go back to choose another card. It returns to
  Home as the Home command does, leaving the project open behind it. (It
  first came only with an example or the template opened from Home; Zain
  asked for it everywhere the same day.)
- **The diagram has one row, as the schema does** (Zain, 2026-10-08). While
  the diagram is in front and the ribbon's Home row is showing, the header's
  own Home, **Schema | Conceptual** switch and project title lead that row,
  before Save, and the header strip under it (Back to Home | CONCEPTUAL |
  title) is put away, so the canvas starts under the row. They are the same
  widgets, carried there and back (`MainWindow::place_workspace_identity`,
  container `conceptualIdentity`), never copies. Compact in this row, as
  Zain chose: Home is its arrow alone (named on hover and to a screen
  reader), the switch's halves have 10 px padding, the rule and the rename
  pencil stay in the header (the project is renamed from the Explorer, as it
  was), and the title gives way before the tools do: the row is fitted with
  it at no more than 48 px and it then takes what room is left, up to 140 px,
  elided with the whole name on hover (`TitleLabel`, `fit_toolbar`). In a
  diagram the switch reads the other way round: Conceptual lit, Schema
  raising the schema it converts to (as Convert to Schema does) and lit while
  it is up, Conceptual putting it away (`wear_workspace_switch`). Under
  another ribbon tab, or with the row put away for full view, the header has
  them back and shows exactly as before. Relational Design -- a schema drawn
  by hand, or a diagram's schema with the whole window -- has its header
  exactly as before. Width cost at the reference sizes (Azure, offscreen):
  1440, the tools as before; 1280, 24 px icons, Notation without its label,
  Search / Model / Theme as icons; 1920, 24 px icons with their words and
  Notation without its label.
- **The diagram's Search is a field, as the schema's is** (Zain,
  2026-10-08). The Search button in the diagram's row is replaced by the
  schema's own `SearchField`, dressed by the same rule, with the glass, a
  clear button and "Search conceptual design…", just left of Model, Theme
  outermost. It is the search bar's text box (`SearchBar::use_text`): typing
  in it opens the bar and narrows the diagram exactly as before; the bar keeps
  the kind, Options, the count and ✕, without a box of its own; Find (Ctrl+F)
  puts the caret in it; Escape and ✕ close the search, put the whole diagram
  back and clear it. Its least width in this row is 120 px (the schema's is
  150), as Zain chose so 1280 keeps the Notation picker; the row is fitted
  with it at its least and it takes spare room up to 240 px. Width cost, also
  Zain's choice: at 1440 the row steps down (24 px icons, Notation unlabelled,
  Model and Theme as icons), and the tools' names come back from about
  1850 px. Leaving a schema drawn by hand no longer squeezes the side panels:
  the header is put back between them hidden, and shown only if it has to be.
- **The schema's working tools follow the ribbon's tab** (Zain, 2026-10-08).
  While Relational Design is in front its header is Home's row, and the
  ribbon did not know it: under File and Settings the header's tools stood
  under the chosen row. Now the ribbon says when its row changes
  (`Ribbon::on_row_changed`, `home_in_front`, `schema_in_front`) and the
  window shows the header's drawing tools, its undo and redo with the search,
  Model and Theme only while Home is the tab in front
  (`MainWindow::wear_schema_header_for_tab`, also applied wherever those
  groups are shown). Home, Schema | Conceptual and the title stay under every
  tab, as Zain chose, as the diagram's do. Nothing is rebuilt: the same
  widgets are hidden and shown, so the tool in hand, the search's text and the
  project are as they were. The diagram's own rows are unchanged.
- **The schema has the diagram's raft of view controls** (Zain, 2026-10-08).
  The raft is made by one builder for both (`MainWindow::make_view_raft`,
  placed by `place_raft`): over the bottom-right of the schema's scroll area,
  the same parts in the same order and size -- grip, Full view, Fit, Pan, +,
  −, a rule, the side panels -- named `schemaControls`, `schemaRaft…`.
  Full view puts the panels beside the schema away and back with its own
  account of them (`put_schema_panels_away`), so the schema's whole-window
  view, which borrows the diagram's, is undisturbed. The schema is drawn at
  its actual size, so Fit, + and − stay pressable and say in the status bar
  that it cannot be fitted or zoomed yet, as Zain chose. Pan is a schema tool
  (`SchemaTool::Pan`, `SchemaView::set_panning`): a drag scrolls the schema
  and touches nothing on it; one drag hands it back, a double click locks it
  (the lock mark on the button), Escape puts it down. The side panels'
  button is the diagram's own action. The raft is put away and brought back
  with the diagram's (View ▸ View controls on the diagram, or the empty
  schema's own menu while it is away). The diagram's raft is built by the
  same builder and is pixel-identical to before.
- **Return from Home to the workspace** (2026-09-27). Once a workspace has
  been in front, Home's top bar offers the way back into it beside Theme:
  **Return to Conceptual Design →**, or **Return to Relational Design →** when
  the schema had the whole window. It leaves Home, which puts the workspace
  back exactly as it was, schema and full view included. A fresh start, with
  no workspace yet, shows nothing there (`AppTopBar::set_return_to`,
  `MainWindow::show_home`).
- **The template is a starting frame, not the example** (2026-09-26).
  **Templates** (Home's sidebar, and New from template) used to open the
  University example. It now opens the general things a diagram is made of,
  each named for what it is: an Entity with an Attribute, a Relationship, and
  another Entity with an Attribute, joined through the relationship with every
  line unlocked, untitled and unsaved (`MainWindow::load_template`). The
  bodies are at their default sizes except the diamond, drawn 280 x 120 so
  its word is not cut short, as the example's Enrollment Date oval is. The
  example itself is unchanged and still opens from Examples.
- **History** (2026-09-26). **View → History**, the last entry in View (and
  so the last button on the ribbon's View row), opens a panel on the right
  listing every step Undo can take back, oldest first, each said in words
  with the time it was made: *Created Entity "Student"*, *Added Attribute
  "student_id" to "Student"*, *Moved Entity "Course"*, *Deleted Relationship
  "Enrolls"*, *Renamed Table "Student" to "Students"*, *Added Column "grade"
  to "Students"*, *Deleted Entity "Course" with its 2 attributes*. The first
  row, **Start**, is the project before the oldest step kept. Pressing a row
  goes to just after that step by undoing or redoing; steps undone stay in
  the list, fainter and in italics, until a new edit takes their place,
  exactly as Redo has them. It is the undo history itself, not a second
  record, so the two always agree, and like it it starts again when a
  project is opened or created and keeps what the 32 MiB budget keeps. Each
  step's words are worked out as it is made, from what it changed
  (`application/history.hpp`: `HistoryEntry`, `HistoryChange`, `describe`;
  `Editor::history`, `history_position`, `go_to`), and each keeps the list of
  things it changed so a fuller account can be given later. Who made a step
  is not recorded yet; the entry is where it will go once there are accounts
  and several people and agents at work.

- Attributes can be resized by their edges and corners, as entities are
  (2026-09-26, fix). Their eight handles were drawn, but a drag was sized by
  the symbol's corner-only rule, and letting go sent the new size to the
  symbols' command, which refused it with "Only a symbol can be resized".
  They now use the entity's pull and their own command,
  `Editor::resize_attributes` ("Resize attribute"), clamped to the entity's
  limits.

- An entity's, relationship's or attribute's name is drawn with its box once
  the box has been resized by hand (2026-09-26). The first resize by a handle
  or through Properties keeps the size the element had as the size its name
  is drawn for (`Project::lettering`, saved from format version 29), and the
  name then grows and shrinks with the box by the smaller of the two changes
  (`domain::lettering_factor`, held between a quarter and sixteen times):
  a corner pulled out to twice the size draws the name twice as tall, while
  pulling only the width out gives a long name room without enlarging it.
  It follows the box live while a handle is pulled, the rename box follows
  it too, and a copy keeps it. An element never resized has no entry and is
  drawn exactly as before, so every existing diagram is unchanged.

- Conceptual relationship diamonds can be resized with the existing selection
  handles on all four edges and corners. The label stays centered and connectors
  follow the boundary during dragging. Resize is undoable; defaults and existing
  document dimensions remain unchanged until explicitly resized.

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
- **The raft shows and hides the side panels** with one button (Zain,
  2026-10-03; one button in place of three, 2026-10-06). Below its **−**,
  after a thin rule in the theme's border colour, a button of the same size,
  **Show/Hide Side Panels** (`viewSidePanels`). Each press takes the next
  step of *Both → Properties only → Neither → Both*, worked out from the
  panels' own entries in the View menu (`QDockWidget::toggleViewAction`)
  rather than counted, so a panel shown or put away from the menu, the View
  row or a workspace coming back is where the next press starts; the
  Explorer alone, which the cycle never leaves, goes on to both. Its picture
  is the panels that are out -- both side sections (`side-panels`), the right
  one (`panel-right`), the left one (`panel-left`), or the bare frame
  (`no-panels`, line art only) -- and its hover text says which and what the
  next press does. Panels are hidden, not rebuilt, and come back at the width
  they had; what is chosen, the project, its history and its unsaved state
  are untouched. The raft keeps its width and its corner. The line art is
  Lucide's panel-left, panel-right, columns-3 and square.
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
  commands. Three stand there for good (Zain, 2026-10-06), each with an icon
  before its name: **File**, **Home** and **Settings**. **Home** is the
  modeling toolbar, with Note after Connect and then **Insert ▾**, which drops
  the Insert menu (Picture and Symbols; Insert has no tab of its own any
  more). Home's Theme button is put away there -- Settings' Design row offers
  the theme -- so the modeling row keeps its words and the notation picker at
  the 1440 px reference width. **File** gathers **Export**, everything that
  leaves, and **Import**, everything that comes back, with **Open & Save**
  beside them dropping the File menu (New, Open, Save, Save as, the examples
  and templates) that the File tab used to drop. **Settings** gathers
  **Design** (Background, Theme, Icons, Notation and Lines), **View** (the
  panels, Checks, framing, zoom, Grid, Align Grid, the raft, comments and
  History) and **Help** (Guide and About). While File or Settings is chosen,
  its rows' own tabs stand beside the three after a thin line, each bringing
  up its row, and the chosen one is marked as well; each opens on the row
  last chosen under it, Export and Design to begin with. With the Relational
  Schema in front the tabs stay, so the window is found in the same place in
  both workspaces: Home brings up no row there (the schema's tools are in its
  header), and the rows leave out what acts on the conceptual diagram alone
  -- Background and Lines; Checks, Full, Fit, 100%, Zoom, Grid, Align Grid,
  the raft and comments -- while View gains **Panels**, the raft's side-panel
  button (the schema has had a raft of its own since 2026-10-08, carrying the
  same button; see *The schema has the diagram's raft of view controls*). The
  menus are untouched either way
  (`Ribbon::set_schema_in_front`, `keep_to_conceptual`, `add_for_schema`).
  The rows are built from the same
  actions as the menus and the Home toolbar, so a tool chosen or locked on one
  row is chosen or locked on the other. A chosen tab wears the theme's accent,
  and the row it brings up is set in the theme's own ink at a heavier weight
  than the interface around it, so the row in front of you reads as the thing
  you just chose rather than as a strip of quiet text that looks the same
  whichever tab is showing. Home is left alone, being the drawing tools, which
  their icons already tell apart. The Export entries go quiet while
  there is nothing to hand on, rather than the row coming and going as
  work starts -- each by what it exports, in the workspace in front, since
  2026-10-09 (see *Export follows the workspace in front*). A Convert tab
  still waits, because there is nothing to convert to.
- **The clean-up that followed the ribbon** (Zain, 2026-10-06 and
  2026-10-07; accepted 2026-10-08). Built on the File | Home | Settings
  ribbon above:
  - **Model** and **Theme**, two menus at the top right of both workspaces,
    Theme outermost. Model holds what is done to the model as a whole: on the
    diagram *Convert to Schema*, *Check model* and *Open example*; on a schema
    drawn by hand *Convert to Conceptual* and Relational Design's own
    examples and template; on a diagram's schema with the whole window the
    way back to the diagram and those examples (`fill_model_menu`). The
    separate conversion, example and *Check model* buttons are put away.
    Theme drops the window's one theme menu, which Settings ▸ Design and the
    View menu keep as well. On the diagram, Search stands before the two
    (a field since 2026-10-08).
  - A narrow window gives way in a fixed order. On the diagram's row the
    icons shrink and the names go before the Notation picker does. In the
    schema's header its own tools give up their words first and Model and
    Theme last, the header held to the window's width
    (`fit_schema_header_words`).
  - The Conceptual Explorer's Entities, Attributes and Relationships start
    folded in a new project, keep whatever fold is given them through every
    edit, count what they hold while folded, and are not opened by choosing
    an element on the canvas; another project starts folded again.
  - Home: a start card that is clicked is chosen and takes the keyboard, in
    the theme's own accent (it used to fill pale blue, the three dots' brush
    left on). The hero's panels, database, platform and lines follow dark
    themes, the database drawn in three readable tiers of the accent's hue;
    light themes are unchanged.
  - No visual baseline was retaken: `card-selected` differs from its
    baseline (3.58016%) until Zain has looked at it.
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
  **Total participation is a double line in every notation** (Zain,
  2026-10-03; until then only in Chen): two lines of the connector's own
  weight, a narrow gap apart (1.6 px at the diagram's scale), one either side
  of where the single line runs, so both leave the diamond's corner together
  and stay parallel round every bend (`alongside`, mitred at each corner).
  Each notation's end symbols are drawn as before; a partial side, and a
  total specialization's double line, are unchanged. A cardinality or role
  label, whose box has always covered part of the line beside it, now hides
  that stretch of the nearer of the two lines. *(For Chen's and min-max's
  number, no longer -- next paragraph. A role's label is as it was.)*
  **Chen's and min-max's number sits at the entity's end** (Zain,
  2026-10-03). Its box is placed from its real size: 2 units out from the
  entity along the run that touches it (the last run of a bent, elbow,
  routed or looping line), and 1 unit clear of the line -- of both lines of
  a total participation -- measured at a chosen line's weight, so choosing a
  line never moves it or lets it cover the line. It sits above a level run;
  beside a standing one, away from where the line turns next, or else to the
  right; opposite the role where the side has one; and is slid along the
  line if a slanted line would leave it over the entity's corner. It moves
  with the line, as before. Crow's foot and Bachman, which write no number,
  keep their old box (which is only part of where a line can be pressed),
  pixel for pixel.
- Recursive and multi-participant relationships with independent participant IDs
  and editable roles. Cardinality labels follow their entity endpoint.
- Create, rename, describe, move, set size through Properties, delete, duplicate,
  connect/disconnect, change owner/kind/cardinality/participation, and undo/redo.
- Tools are one-shot by default: a single click on a toolbar tool places one
  element and returns to Select. Double-clicking the tool locks it, marking the
  button with a padlock, so it keeps placing until another tool is chosen or
  Escape is pressed. Select cannot be locked.
- An attribute placed with the Attribute tool stands on its own and is
  connected by hand, whatever is selected (2026-09-26). It used to be joined
  to a selected entity, relationship or composite attribute. To attach
  attributes as they are placed, an owner is locked instead, in any of
  three places: the open padlock an entity, relationship or composite
  attribute wears in its top-left corner while it is selected; **Lock as
  attribute owner** in its right-click menu; or the same button under its
  name in Properties. The owner then wears a closed padlock there, and
  every attribute placed is attached to it with its line drawn; with the
  Attribute tool locked too, one after another. Pressing the closed padlock,
  which also answers with the Attribute tool in hand, or **Unlock attribute
  owner** in the other two places, lets it go, as does deleting the owner,
  a composite being made plain, or another project being opened. Only one
  owner is locked at a time. The lock is not saved in the file. Connecting
  two entities still creates the relationship between them, as before.
- An attribute put down within a short reach (100 units) of another
  entity, relationship or composite attribute, and nearer to it than to the
  locked owner, is asked about before anything is placed (2026-09-26): "You
  are locked to Mentor, but this attribute is nearer Student." **Continue**
  attaches it to the locked owner, and that element is not asked about
  again while the lock lasts; **Unlock** lets the lock go and places the
  attribute on its own, to be connected by hand; **Cancel** places nothing.
- **An attribute's line leaves its owner from the middle of the side facing
  it** (2026-09-26, replacing the sliding join and its stub). Every
  attribute on one side of an entity, relationship or composite attribute
  leaves from the same point (the middle of a box's edge, a diamond's point,
  an ellipse's end), each by its own straight line in every line style,
  with no stub, so lines never hook round or run across the body. Moving
  an attribute about on that side leaves the point where it is; carried past
  a corner, its line moves to the middle of the side it now faces (Zain
  chose this over a point fixed wherever the attribute goes). Nothing is
  stored, so the line stays unlocked; one pinned by hand, or dragged to a
  point, is drawn straight from where it was pinned. Attributes placed on a
  locked owner are unlocked like any other and follow the same rule (they
  used to be pinned to a shared exit).
- **Connect no longer pins a line where it was clicked** (2026-09-26).
  "Join where I click", which was the default and pinned both ends there,
  is gone from Connect's arrow, and with it the choice it was one half of;
  a choice remembered from before is ignored. Every line Connect draws
  starts unlocked. Lines already pinned in saved diagrams stay pinned until
  unlocked by their padlock or right-click.
- A click anywhere in the window outside the diagram puts down whatever
  tool is in hand, locked or not, and takes up Select (2026-09-26): the
  Explorer, Properties, the ribbon, the header, the Relational Design panel
  and so on. Inside the diagram a click does what the tool does, placing
  what it places. The diagram's own zoom controls and scrollbars count as
  inside it, and a button that chooses a tool still chooses that tool.
  The click itself still does its ordinary job, and the tool is put down
  once it has, so a name being typed on the diagram is kept as before
  (`MainWindow::pressed_outside_canvas`, watched by `PressWatch`).
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
  takes the whole thing back. Its two lines start unlocked (2026-09-26):
  neither end is pinned to where the entity was clicked, whatever Connect's
  join setting, so each slides round its shapes as they move, until it is
  locked by hand. Every other connection Connect makes is unchanged.
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
  pair the database tools it sits beside use. Each has a row of its own on the
  ribbon, side by side under its File tab (2026-10-06), and a menu under File,
  because a reader looking for one expects the other in the same place.
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

  *Corrected 2026-10-02:* that destination exists now -- Relational Design,
  in a project begun as a diagram and as the whole workspace of one begun
  from its schema. What is missing is reading SQL, CSV or JSON and building
  relational objects from what is read. *Import ▸ From another tool…* is
  still disabled, and its tooltip still gives the old reason; rewording it is
  a pending interface-text change, not made by this correction.
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
- **Export follows the workspace in front** (committed in `3e29264`,
  2026-10-09). With Relational Design in front -- a schema drawn by hand, or a
  diagram's schema with the whole window -- the documents and pictures are of
  the schema, through the same exports as the diagram's; otherwise of the
  diagram. **Copy as picture** copies whichever is in front. Each Export entry
  is on only while what it exports exists there: the project copy while the
  project holds anything; the documents while there is something to list;
  the pictures while there is something drawn; with the schema in front,
  while it has a table. Saved or not, the project can be exported, and
  exporting leaves its unsaved state as it was. Where the file goes is still
  asked in the save dialog. The tests drive every Export command whole by
  giving the destination where the dialog would ask
  (`MainWindow::choose_export_location`, which the application never sets),
  so no test depends on answering a modal dialog: they write each document
  and picture to real temporary files, check what they hold, and check that
  a cancelled destination writes nothing.
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

Two large Conceptual diagrams stand beside it, for testing and demonstrating
the workspace at size: **File → Company Database** / **University Database**
and the same two in the **Home** menu (`build_company_database`,
`build_university_database` in `app/desktop/conceptual_examples.cpp`). They
are built with the Editor's own commands and open clean, with no file of their own,
as the original example does; Home's sidebar **Examples** row still opens the
original.

- **Company Database** — 14 entities, 87 attributes, 17 relationships.
  Employee in the middle with Department, Skill, Office, Job and the weak
  Dependent (identifying Supports, partial key *Name*) around it; Project
  below with Team and Task; Client, Invoice and Payment to the left of it;
  Product and Supplier under it. Supervises is recursive (Supervisor /
  Supervisee); Qualified In, Works On, Staffed By, Assigned To, Uses and
  Supplies are many-to-many; Manages, Qualified In and Works On carry
  attributes; Name is composite, Age derived, Phone and Location multivalued.
- **University Database** — 14 entities, 81 attributes, 19 relationships.
  Student, Section and Course on the middle row, joined by the associative
  Enrollment; Professor, Department and Faculty above; Exam, Exam Result and
  Assignment below Section; Program, Library Book (associative Book Loan),
  Club and Scholarship to the outer left. Prerequisite is recursive
  (Successor / Predecessor); Heads is one-to-one; Enrollment and Book Loan
  carry attributes; Name is composite, Age derived, Phone and Author
  multivalued.

Both are laid out by hand: each box is wide enough for its name at the
diagram's lettering, attributes stand in columns or rows on the faces their
owner's lines leave free, and diamonds stand level with what they join so
most lines run straight. On the entities carrying many lines (Employee,
Project, Student, Section and a few more) the line ends are pinned along the
faces, so no two cardinality marks share a point; nothing has a stored route,
so every line still follows what it joins when it is moved. No two shapes
overlap and no relationship line crosses another.

Two things the model could not say when these were drawn are drawn as near as
it allowed. A key attribute on a relationship was refused by validation
(`attribute.key.relationship`), so Enrollment and Book Loan carry no
EnrollmentID / LoanID of their own; conversion gives their tables a generated
key instead (`EnrollmentID`, `Book LoanID`). Since 2026-10-05 such a key may
be drawn on a many-to-many or associative relationship (ADR-021 §5b); the
examples are unchanged. There is no separate
partial-key kind; Dependent's *Name* is drawn as a key on a weak entity, which
the canvas underlines in dashes as a partial key.

Relational Design has examples of its own (2026-10-05, ADR-022 §9.23):
**File → Company Database — Relational** / **University Database —
Relational**, the same two in the **Home** menu and the header's Open
example, offered while Relational Design is in front
(`build_company_database_relational`, `build_university_database_relational`
in `app/desktop/relational_examples.cpp`). They are the same two domains
designed again as tables -- not converted from the diagrams above -- and each
opens as a project that starts from its schema, clean and untitled on disk,
with no diagram. Every table, column, key and foreign key is made by the
Editor's schema commands, as the Table tool, Connect and Properties make
them: a table placed where it stands with its key named for it, each column
typed and ruled, surrogate keys counting themselves up (IDENTITY), and each
foreign key connected to the key it references.

- **Company Database — Relational** -- 22 tables, 25 foreign keys. Office,
  Job, Department, Employee, Skill, Client, Project, Team, Task, Invoice,
  Payment, Product and Supplier; junction tables EmployeeSkill,
  ProjectAssignment, TeamMember, TeamProject, ProjectProduct and
  SupplierProduct, each keyed by its two foreign keys and carrying the
  relationship's own columns; EmployeePhone and DepartmentLocation for what
  may be held more than once; Dependent keyed by (EmployeeID,
  DependentName). Employee.SupervisorID references Employee itself;
  Department.ManagerID is a unique foreign key to Employee, left empty until
  a manager is appointed.
- **University Database — Relational** -- 21 tables, 26 foreign keys.
  Faculty, Department, Professor, Program, Course, Classroom, Section, Exam,
  Assignment, Student, LibraryBook, Club and Scholarship; junction tables
  Enrollment, CoursePrerequisite (both keys referencing Course), ClubMember,
  StudentScholarship and ExamResult (one result for each student in each
  exam); BookLoan with a key of its own, since a book may be borrowed again;
  StudentPhone and BookAuthor for what may be held more than once.
  Department.HeadID is a unique foreign key to Professor.

Both are laid out by hand, grouped by area with each junction table beside
the tables it joins: no table over another, no line through a table, and no
line or width stored, so every line still routes itself when a table is
moved. Age is not a column in either, since it is worked out rather than
stored.

A table's height is a matter of its rows, but its width is measured from its
lettering, which each platform draws at its own size (2026-10-06, found when
Windows CI's tables crowded one another). So each column of tables -- the
tables placed at one x -- stands 150 px clear of the widest table in the
column before it, as the schema measures them (94 px in the template), and
moves with the lettering rather than into its neighbour
(`SchemaView::full_width`). On macOS that is exactly where every table was
placed; nothing there moved. Built without a schema to measure against, as
the tests build them, the tables stay where they are placed.

**New from template: Basic Relational Schema**, in the same places
(`build_basic_relational_schema`), is Relational Design's template: Parent
(ParentID, Name) and Child (ChildID, Name, ParentID), Child.ParentID
referencing Parent.ParentID, keys whole numbers and names nvarchar(100), NOT
NULL. It opens untitled, as the Conceptual template does, with the two keys
level so the line between them runs straight.

The normal startup is an empty project. Text properties commit on focus loss;
geometry changes use **Apply position and size**.

## Roadmap coverage

| Phase | Evidence and remaining work |
| --- | --- |
| 0 — Architecture | Completed review; ADR-001–013 accepted. ADR-014 records implementation choices. ADR-015 records export and interchange formats, whose picture half is now implemented and whose relational half is not; ADR-016 records project organisation and the start experience, of which the Home screen and its three start cards (with Templates and Import in its sidebar, ADR-022 §9.14), a Recent list and one starting template are implemented and a template gallery, project folders and thumbnails are not (see *The Home screen and the Azure theme*; corrected 2026-10-02); ADR-017 records review comments, which are implemented and are deliberately kept apart from an element's description and from the Note element; ADR-018 records that search narrows the diagram rather than walking hit to hit, which is implemented; ADR-021 records relational identity and provenance, which is implemented; ADR-022 records four visible workspaces, the theme token resolver, the menu bar and the start-route readiness gate, of which the token resolver, the Azure theme and the readiness gate are implemented, the menu bar order is implemented for the native menu bar, the workspaces wear Azure with Relational Design offering none of the conceptual tools (§9.12), and the Home screen, its card states and both workspaces are held to reference pictures at 1440 × 1080 (see *The Home screen and the Azure theme*). |
| 1 — Build foundation | Exit criteria met: layered CMake targets, warning flags, Debug/Release, passing suites, macOS launch, and committed repository state. Windows/Linux instructions exist but those platforms are unverified. |
| 2 — Shell | Functional desktop shell delivered. Relational Design is reachable: raised over a diagram at half or full height, or as the whole workspace of a project begun from its schema. SQL and Data navigation waits for those workspaces. (Corrected 2026-10-02.) |
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
| 16–17 — Pages/editor milestone | Not complete; multiple pages, the remaining conceptual semantics, and the template gallery, project folders and thumbnails of ADR-016 are required. The start screen, its Recent list and one starting template are built. (Corrected 2026-10-02.) |
| 18 onward | Import, SQL, data, and later production/ecosystem features remain planned. Provenance is implemented (ADR-021). Physical design is merged into Relational Design (ADR-022 §9.4–9.5); of it, the SQL Server type catalogue with sizes and scales, `NOT NULL`, `UNIQUE` and `IDENTITY` are built, and indexes, defaults and dialect settings are not. (Corrected 2026-10-02; this row said schema generation had only a preview.) |
| 24 — Schema workspace | Merged into **Relational Design** (ADR-022 §9.5); partial. Relations and foreign keys have identities and provenance of their own (ADR-021, format 26). A schema can be drawn by hand in a project that starts from its schema, and converted into its diagram (format 30). Relational Design has its own Explorer, Properties, tools and safe connection rules (Stages 1–5). A key's order and a derived table's column order are kept apart from how columns are listed (formats 32 and 33). A schema worked out from a diagram is still the conversion read as tables, worked out afresh every time, with the questions it cannot settle asked on its tables (see the section below). Not built: a history of its own apart from the project's; tables or connections made by hand in a project begun as a diagram; checking the schema; SQL. (Corrected 2026-10-02; until then this row said the workspace was not started and owned no identities.) |
| 35 — Export | Partial, and pulled forward the way Phase 15 was, because ADR-015 splits export into halves with different prerequisites and neither the pictures nor the listings need anything later. Implemented: the pictures, their options, the project carried inside SVG and PNG, and all four documentation listings, offered together under Export. Not implemented: a multi-page PDF of a project, which waits for the multiple pages of Phase 16; the published JSON Schema for `.erdx`; the outline-text option for a pixel-exact handoff; and the whole schema half — `.sql`, Mermaid ER and DBML. The relational model they would read from now exists, in Relational Design; no generator for any of them is built. (Corrected 2026-10-02; this row said they had to wait for that workspace.) |

## The schema preview

A picture of what the Conceptual model becomes, raised over the diagram. It is
derived: `schema_preview(project)` is a pure function with no identity and no
storage, worked out again on every change, so it cannot drift from the model.
The Relational Schema workspace of Phase 24 — relations with identities of
their own and a history of their own — is still a later and separate thing.

*Superseded in part (2026-10-02):* relations and foreign keys have had
identities of their own since format 26 (ADR-021), a schema can be drawn by
hand in a project that starts from its schema (format 30), and Phase 24 is
merged into Relational Design (ADR-022 §9.5). What is still to come of it is
a history of its own apart from the project's. A schema worked out from a
diagram is still worked out as described here.

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
- **Bold connectors** (2026-09-25). A line between tables is drawn at 2.6 px,
  3.4 px under the pointer or when its table is picked out, and its
  cardinality ends at 2.2 px, so connections read at a glance across a full
  schema. Only the weight changed; routes, colours and notation are as
  before. The diagram canvas is untouched.
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
  not built. (Relations made by hand and schema-first projects, with their
  conversion into a diagram, were built on 2026-09-27; see *What works*.)

**Not implemented.** `readiness()` itself and the gate it feeds (Phase 14);
the generation baseline and three-way reconciliation (Phase 25); and `.sql`,
Mermaid ER and DBML export, for which no generator is built. *(Corrected
2026-10-02: this list also named relational objects with identities of their
own, and editing them as such. Both are built -- ADR-021 identities, schemas
drawn by hand, and the Relational Design entries under What works. It also
said the exports could not precede the workspace they read from, which now
exists.)*

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

  **Reachable from the diagram since 2026-10-05** (Zain; ADR-021 §5b, the
  conflict closed). A key attribute may belong to a relationship exactly
  where the relationship is represented by a table of its own -- many to
  many, or associative (`domain::has_own_table`, which the conversion uses
  to decide which relationships become tables). There it is the bridge's
  whole primary key, by any of the ways a key is made: the attribute's kind,
  the Identifier tick, the schema's primary-key toggle, or a key attribute
  moved onto the relationship. On a one-to-one or one-to-many relationship,
  which becomes a foreign key and has no table to key, it is refused, saying
  why and naming both. Whatever would take a keyed relationship's table away
  -- a side made one, a side cut or its entity deleted, the associative shape
  taken off a relationship whose sides no longer make a bridge -- is refused
  the same way until the key is made an ordinary attribute; nothing is
  dropped, demoted or ignored. `attribute.key.relationship` is the same
  validation rule, now conditional, so opening and saving follow it too. The
  file format and its version are unchanged. Properties offers a key
  attribute a relationship as its owner only where the relationship has a
  table of its own, and its hint says so.
- **A primary key is marked twice**, with a golden key and the letters `PK`
  (the key redrawn 2026-09-26, to Zain's reference). The key is a polished
  golden key held bow up and pointing down, with the teeth at its foot: its
  own vector drawing, `assets/marks/primary-key.svg`, with a gold gradient,
  a darker gold edge, a hole that shows what is behind it, and a soft shine.
  It is drawn through `primary_key_mark()` in `icons.cpp`, so a table can wear
  the same key later. It is the same key whichever icon set is chosen, and it
  goes grey, keeping its shading, under the Plain theme. It stands right
  before `PK` so the two read as one mark beside the name. The row, its
  height, the gutter and the letters are as they were. (Before, the key was
  the icon set's key glyph, inked in the theme's warning hue raised to gold,
  at the gutter's far left.)
- **A foreign key is always marked `FK`, in green** (2026-09-24;
  **SUPERSEDED in part 2026-10-01** by the next entry: a column that is both
  keys now shows the key mark and `PK` as well as `FK`. `FK` is still always
  green and `PK` orange.) This holds
  when the column is also part of a composite primary key, as the participant
  keys of a bridge are when somebody chooses them. The gutter never shows a
  merged `PK FK` marker. That such a column belongs to the key is shown in its
  Constraints cell, `PK, NOT NULL`. The orange `PK` letters are kept for key
  columns that are not foreign keys, so orange means primary key and green
  means foreign key.
- **The card's Constraints cell** (Zain, 2026-10-08). It writes, in one
  order whatever order things were chosen in, `PK`, `FK`, `NULL` or `NOT
  NULL`, `UNIQUE`, `IDENTITY` (e.g. `FK, NULL`, `PK, FK, NOT NULL`, `NOT
  NULL, UNIQUE`). `FK` is written as well as marked in the gutter; the
  column's width is measured from what it writes, so a table whose longest
  list gains `FK, ` is that much wider. The words are drawn in the theme's
  muted ink, the Data Type's own, not the name's. A new ordinary column is
  `NULL`, because the model says so (`required = false`); there is no unset
  state and no warning. Pressing the cell lists **Primary Key**, **Foreign
  Key**, a rule, **NULL — may be empty** / **NOT NULL — required** (one
  ticked, never both), a rule, **UNIQUE — no duplicate values**, **IDENTITY
  — auto-generated number**. Primary Key and Foreign Key run exactly what
  their Properties switches run (`MainWindow::press_schema_primary_key`,
  `press_schema_foreign_key`, shared by both): the Foreign Key goes through
  the Stage 5 Connect path and its question; nothing is greyed out, and a
  key that cannot be put on or taken off says why in the status bar. The
  rules are the Properties list's own (`populate_schema_rules`). Desktop
  tests cover a schema drawn by hand and one converted from a diagram,
  Properties agreeing both ways, Undo/Redo, save and load, conversion to a
  diagram, and the ink in a light and a dark theme.
- **A column that is both keys wears both marks** (Zain, 2026-10-01). Its
  gutter shows the golden key, the orange `PK`, then the green `FK`, side by
  side, each in its own colour -- separate marks, not a merged badge -- so
  neither role hides the other. Until then such a row showed only the green
  `FK`. The gutter (46 px) and the row are unchanged: the key keeps the size
  it has in every key row, and in that row only the two pairs of letters are
  drawn just as much smaller as the room beside it asks (about 88% on macOS,
  none offscreen). PK only and FK only are drawn as before.
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
somebody chose to key by their participants' foreign keys; version 29 keeps the
size an element's name is drawn for; version 30 holds a schema drawn by hand --
its tables and the foreign keys between them -- in a project that starts from
its schema; version 31 keeps a name typed over a foreign key. Older files open,
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
  is up, to a slim bar (`AppTopBar`) carrying only the ERDFlow mark and a
  **Theme ▼** control (ADR-022 §9.14). Theme opens the window's own theme
  menu, with its preview on hover. The bar's own Settings gear, and the
  divider between it and Theme, were taken out on 2026-09-26 (Zain: one
  Settings, not two); Settings is the sidebar's row, which holds the
  window's own Theme, Icons and Notation menus. The ribbon row that was in
  front comes back when Home is left. The status line stays.
- **Opens filling the screen.** An ordinary launch shows the window
  maximised. 1440 × 920 is the size it returns to when un-maximised, and a
  run with `--screenshot` or `--size` is shown at that size, or the one asked
  for, rather than maximised.
- **`--theme` is for that run only** (fixed 2026-09-25). A run with it had
  been saving that theme as the profile's choice, because the window
  remembers whatever theme it is given. The profile's own choice, or its
  having made none, is now put back after the window is built.
- **No heading over the cards** (2026-09-25). *Create a new project* was
  removed, put back, and removed again by Zain the same day. Each card's
  own + Create says what it does. The heading's room is kept, since the
  label is hidden but keeps its size, so the cards stand where they stood.
  That room is also where the hero's pictures are measured to end clear
  of. The gap under the subtitle is 74 px plus the kept room and 12 px; on
  a short window, 14 px and 6 px.
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
  *(Corrected 2026-10-02: since 2026-09-26 Templates opens the general
  starting frame, not the University project -- see* The template is a
  starting frame, not the example. *Examples still opens the University
  example; the two large examples are in the File and Home menus.)*
- **The hero is three columns read as one line** (2026-09-25), spread over
  about 90 % of the centre.
  - *The middle, centred:* a small *Welcome to ERDFlow* in the accent between
    two thin rules, then *Design. Convert. Generate.* at 31 px. "Design." is
    navy, "Convert." is in the accent and "Generate." is in a deeper shade of
    it. Under it, on one line, *Balance ideas, schemas, and SQL in one visual
    workflow.*
  - *The left:* a product page, turned 2.5° anticlockwise and a little away.
    It floats in a faint blue light over a soft shadow and has a glass face.
    It shows a window bar, *Welcome to ERDFlow*, *Turn your ideas into real
    databases with the power of AI.* and four tiles: Design, Convert,
    Generate and AI. It shows no tables or diagrams.
  - *The right:* the database illustration, with a blue light behind it and
    a shadow under its platform.
  - *Sizing:* the page and the illustration get columns of the same width,
    at most 320 px, on one level. A column is also kept small enough that the
    illustration's top stays on the page and both pictures end clear of
    *Create a new project*.
  - *The line:* one smooth line joins them. It comes out from under the
    page's right edge and goes in under the platform, past its near corner,
    with nothing fixed at either end. It passes under both ends of the
    subtitle, so it never crosses a word. It is a rounded tube in one light
    blue (Zain, 2026-09-25): deeper at its edges, paler towards its middle,
    with light along its centre, over a soft glow. Nothing is fixed on it.
    - *Electrons:* four coloured electrons travel along it on the
      illustration's clock, cyan and violet going to the database, amber
      and green coming back. They move when the orbit moves and rest where
      it rests. They are hidden inside the page and under the platform,
      so they come out of one and go into the other.
    - *The page lights up:* when an electron coming back reaches the page,
      the page glows in that electron's colour for a second and a half.
    - *Cost:* only the pixels round each electron, and the page while it
      glows, are redrawn each frame. The tube stays in the kept picture.
  - *Stepping aside:* both pictures step aside on a short page, or where a
    column beside the words would be under 150 px.
  - *Drawing:* the page, rules, line and light are painted by
    `HomeHeroBackdrop` in `home_page.cpp`, once into a kept picture per size,
    scene and theme, in the theme's tokens (grey under Plain). The
    illustration's own drawing is unchanged.
- The welcome is set beside a **drawn illustration** (`WelcomeFlowIllustration`,
  ADR-022 §9.15): a large three-tier database on a rounded platform over a
  soft glow, with four panels around it, read by their marks: a structure of
  boxes, an entity–relationship–entity, a table on a blue panel and a page of
  SQL. Each panel leans towards the database; straight lines with two bends
  join them to the platform (ADR-022 §9.17). It stands in the hero's right
  column, as described above, and steps aside on a short
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
  runs stay visible. It pulses faintly. It is drawn as a small conduit
  rather than a plain stroke (Zain, 2026-09-25):
  - a soft glow round it, a light-blue wall and a paler core, so it reads
    as a channel with two edges;
  - fine marks of data in the core, drifting outward;
  - tiny glowing lights travelling both ways, two out to the panel and one
    back into the platform, a lap every 3.6 seconds, fading in and out at
    the ends;
  - a lit port where it plugs into the platform, and a smaller one half
    under the panel's edge.

  The route is unchanged. Stood still, the lights rest where the clock
  left them. The SQL panel is in the interface's blues rather than gold
  (Zain, 2026-09-25): a pale panel, a blue page, and "SQL" lettered in a
  deep shade of the drawing's blue, which reads on the pale panel in every
  theme. Gold stays available to any panel that asks for it, and the small
  diamond in the relationships panel is still gold. Pointing at a panel highlights its border and stops nothing. The
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
  at its foot, is what starts. A card reads top to bottom: title,
  the *Coming soon* row (empty on a card that can be taken), drawing,
  description, then the buttons. The helper line is no longer drawn.
- **+ Create and Create with AI, side by side on every card**
  (2026-09-25). The two share one row at the card's foot, which runs
  between the card's margins, so the pair is centred and all six buttons
  stand on one line across the cards. Each is as wide as its words plus an
  even share of what is left. Where a card is too narrow to leave 6 pixels
  either side of *Create with AI*, the second button says only *AI*. This
  happens below about 1366 pixels wide with the learning panel showing. Its
  mark and its name read aloud are unchanged. With the ways between the
  cards, that is at 1366 × 740 as well. *Create with AI* is white, with a
  fine pale-blue outline and navy words. Its mark is Lucide's `sparkles`
  (`assets/icons-outline/sparkles.svg`), drawn filled (`solid_pixmap`) and
  shaded from the accent to a violet turned from it. AI is a later feature,
  so the button is **disabled on every card**, and it says *Creating with AI
  is coming soon.* when pointed at. Tab passes over it.
- **Every card's buttons look the same, whether or not the card can be
  taken yet** (Zain, 2026-09-25). + Create is blue and Create with AI white
  on all three cards. On *Relational Schema* and *SQL Script (DDL)* both
  buttons are still disabled: they do not answer, and their tooltips say
  why. The *Coming soon* badge is what says a route is not built.
  *(Corrected 2026-10-02: Relational Schema's + Create has been enabled
  since 2026-09-27; only SQL Script (DDL)'s are disabled now. Create with AI
  is switched off on every card for the time being -- see* Create with AI
  is switched off for now.*)*
- **The cards are pale glass** (2026-09-25). Each card is:
  - white with a breath of the accent, a little more towards its foot;
  - edged in a fine pale blue, with light caught along its top and a soft
    reflection in its top right corner (faint on a dark theme);
  - lifted off the page by a wide blue-grey shadow, over the faintest wash
    of the accent across the row.

  The chosen card is told apart only gently, by a slightly firmer edge and
  a faint light at its top; it has no heavy blue outline. A card that can
  be taken, while pointed at or reached by the keyboard, is coloured a
  little deeper than the others and than itself at rest: the hover colour
  carried towards the accent, with the hover edge (Zain, 2026-09-25). A card holding a
  preview keeps no room for the description it does not draw, so its parts
  sit close together.
- **Each preview is a raised tile of tinted glass, like the hero's welcome
  page** (Zain, 2026-09-25, chosen from three styles). Its face is a shade
  lighter than the card, with a fine blue edge, light along its top and a
  quiet header with its name. The glass shows a 9 px thickness under it. The
  card draws a soft blue light round it and a shadow under it, 14 px from
  the card's edges. What it shows (the Conceptual canvas, the tables, the
  script) is drawn straight onto the glass, with no white page inside.
- **The card buttons stand up** (2026-09-25). + Create and Create with AI
  are each a little lighter at the top than the foot. The card draws a soft
  shadow under each, + Create's in its own blue.
- **The way between the cards** (`HomeFlowBridge`, 2026-09-25). Between
  Conceptual and Relational: *Convert to Schema* over an arrow pointing on,
  and an arrow pointing back over *Back to ERD*. Between Relational and SQL:
  *Generate to SQL* and *Back to Schema*.
  - *Drawing:* each arrow is a small 3D tube: a shaft shaded deeper at its
    edges and lit along its top, a solid head lit from above, two beads
    trailing from its tail, and a soft shadow under it.
  - *Colour:* forward is the accent. Back is the violet turned from it,
    the Create with AI spark's colour, a little lighter, with its words in
    the same violet softened slightly. Grey under Plain.
  - *Placement:* each stands in a column of its own in the card line,
    62–92 px wide, never over a card. It sits level with the middle of the
    previews either side.
  - *Sizing:* on a wide window the columns widen to 92 px before the group
    gains margins, and the cards still stop at 315 px.
  - *Behaviour:* it is a sign and does nothing when pressed. The keyboard
    passes over it, and it is read out as, for example, "Convert to Schema,
    Back to ERD".
  *SQL Project* is shown in its place, marked *Coming soon* and not
  enabled, saying why in its tooltip: it waits for SQL to be parsed into
  Relational Design and round-tripped (ADR-022 §9.2). *Relational Schema*
  was marked the same way until 2026-09-27, when ADR-021 Step B was built;
  its **+ Create** now opens a project that starts from its schema.
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
- **Examples, Templates and Import are offered in the workspace they belong
  to, not on Home's rail** (Zain, 2026-10-03). The rail is Home, Open
  Project and Recent, then Settings and Help at its foot; this replaces what
  the entries above and ADR-022 §9.14 and §9.20 say about its Examples,
  Templates and Import rows. Every example and the template are Conceptual
  diagrams, so they stand in the File menu -- *Open & Save* under the ribbon's File tab --
  beside Import: *Open example*, *Company Database*, *University Database*
  and, new there, *New from template*; Import's own tab and the header's
  *Open example* are as they were. While Relational Design is in front, a
  schema drawn by hand or a diagram's schema given the whole window, those
  entries and Import's two ERDFlow formats are put away
  (`MainWindow::set_workspace_in_front`), and Import keeps only *From
  another tool…*, still unavailable with its reason. The Home menu in the
  menu bar offers its examples and *New from template* on the same terms:
  while the diagram is in front, and not while Relational Design is
  (2026-10-04). Relational Design has no examples or templates of its own
  yet; they are the next piece of work (Zain, 2026-10-04).
- **Relational Design has its own examples and template** (Zain,
  2026-10-05; ADR-022 §9.23), which replaces the last sentence above. While
  Relational Design is in front, the File menu and the Home menu offer
  *Company Database — Relational*, *University Database — Relational* and
  *New from template: Basic Relational Schema*, and the header has an Open
  example of its own: a folder mark with its menu arrow, named on hover,
  dropping the same three. While the diagram is in front they are put away
  and the Conceptual ones are offered, as before. The header of a schema
  drawn by hand had no room for a worded button at the 1440 px reference
  width, so the mark is what Zain chose; with a long project name the
  header is still too wide at 1440 px, and it is the mark, last in the row,
  that goes past the edge (ADR-022 §9.23).
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
  Home's drawing tools (Entity, Attribute, Relationship, Specialization,
  Connect, Note) are put away with the diagram -- the ribbon's tabs stay,
  their rows keeping to what can act on the schema (2026-10-06) -- and so is
  Insert › Picture, which places a picture on the diagram nobody can see.
  What stays is its own: Arrange, Appearance, undo and redo, its search and
  the theme. The Conceptual workspace keeps Select, Entity, Attribute,
  Relationship, Specialization, Connect and Note.
- **Undo and redo beside the schema whenever it is open** (2026-09-25).
  The header's Undo and Redo, the same actions the Edit menu has, now show
  whenever Relational Design is open, not only while it has the whole
  window. Before, with the schema sharing the stage, the only Undo buttons
  were on the ribbon's Home row, so with any other tab in front there was
  none on screen. Its search and the theme still come out only in full
  view, since otherwise the diagram's own are showing
  (`MainWindow::place_schema_header_tools`). The keyboard's Undo reached the
  schema in both layouts already.
- **The schema panel is always the stage's width** (2026-09-26, fix). The
  panel is laid over the stage by hand, and was laid out again only when the
  window changed size. Pulling Properties (or Explorer) wider or narrower,
  or closing and opening it, changes the stage without changing the window,
  so the panel kept its old width: a strip of diagram showed between it and
  Properties, or it ran on under the stage's edge. The stage is now watched
  and the panel is laid out whenever the stage is resized, so it follows a
  side panel's edge continuously while it is dragged. Only the panel's
  width changes; the tables, their places and the zoom are untouched.
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

- **SQL First** -- the *SQL Script (DDL)* card -- is disabled until its
  workflow exists (ADR-022 §9.2). *(Corrected 2026-10-02: this said
  Relational Design First was disabled too. Its card, Relational Schema, has
  created projects that start from their schema since 2026-09-27.)*
- There is one template, the general starting frame -- an Entity, a
  Relationship and another Entity, each with what it is made of -- and it is
  not an example. The examples are the University example (**Open example**)
  and the two large ones, **Company Database** and **University Database**.
  A gallery of templates, *Save as template*, project folders as a library
  and thumbnails (ADR-016) are not built. **Import** on Home opens the same
  chooser as Open Project. Reading SQL and database sources is not built;
  where they would go, Relational Design, now exists. *(Corrected
  2026-10-02: this said the one template was the bundled University project,
  and that SQL and database sources had nowhere to go.)*
- *View tutorials* opens the quick guide. There are no tutorials beyond it.
- The platform's own reduced-motion setting is not read. Qt has no
  cross-platform way to ask, so `ERDFLOW_REDUCED_MOTION` is the only way in
  today, behind one function where a native reading would plug in (ADR-022
  §9.9).
- In a project begun as a diagram, Relational Design is raised over the
  diagram, at half or full height, with its own Explorer and Properties while
  it is in front; a schema worked out from a diagram still gains tables only
  from the diagram. In a project begun from its schema, Relational Design is
  the whole workspace, and tables are made by hand with the header's
  *Select | Table | Connect* tools. Both workspaces have the ribbon's File,
  Home and Settings tabs (Zain, 2026-10-06); Relational Design puts Home's
  drawing row away and carries its own tools in the header instead, so the
  ribbon has no Table tool. *(Corrected 2026-10-02: this said Relational
  Design was still only a panel over the diagram.)*
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

**2026-10-09**, at `3e29264` (*Add workspace-aware export support*): all
six functional suites pass in Debug and in Release -- `core`,
`persistence`, `canvas`, `theme`, `desktop` and `desktop_smoke` -- with the
whole desktop suite run, nothing skipped, including the export-state tests
(`export_state_tests()`, about 2 s; they used to hang on a save dialog in
the working tree, and now give the destination where the dialog would ask).
`desktop` took 54.2 s in Debug and 50.9 s in Release, close to ctest's 60 s
limit in Debug -- a risk, not a failure. `visual` fails on three reference
pictures, `card-selected` (3.58016 %), `conceptual` (5.7545 %) and
`relational-design` (8.39088 %); no reference picture was retaken.

**2026-10-05**, after Relational Design's examples and template were added,
the keyless-entity edit of 2026-10-03 was reverted, and ADR-021 §5b was
decided (a key drawn on a relationship with a table of its own): six of the
seven suites pass in Debug and in Release -- `core`, `persistence`, `canvas`,
`theme`, `desktop` (about 52 s in Debug and 42 s in Release) and
`desktop_smoke`. Only `visual` fails, on two reference pictures, conceptual
(3.30736 %) and relational-design (7.29392 %; 6.98843 % before the header
gained Relational Design's Open example mark). No reference picture was
retaken.

**As of 2026-10-02, five of the seven CTest suites pass, in Debug and in
Release.** The two that do not:

- `core` -- 64 of its 65 tests pass. `drawn_keys_are_the_primary_key` fails at
  `core_tests.cpp:2319`, where a key drawn on a relationship is refused by
  `attribute.key.relationship`: the open conflict in ADR-021 §5b, waiting for
  Zain's decision. It has failed since 2026-09-24.
- `visual` -- two reference pictures differ from their baselines, conceptual
  (3.14403 %) and relational-design (6.98843 %), because the workspaces have
  changed since they were taken. They are not retaken until Zain decides.

`persistence` (30 tests), `canvas`, `theme`, `desktop` and `desktop_smoke`
pass. The `desktop` suite passes, but in Debug it has taken between 42 and
127 seconds when run alone, past ctest's 60-second limit, which ctest then
reports as a timeout. The AddressSanitizer + UndefinedBehaviorSanitizer runs
of the desktop suite recorded with the Schema workspace work were clean,
the last on 2026-10-01.

*(Corrected 2026-10-02: this section opened by saying all seven suites pass
in Debug, Release and the sanitizer build, which has not been true since
2026-09-24.)* The suites cover the core, real persistence adapter,
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
drawn with a green `FK`. *(Superseded 2026-10-01: desktop tests now check that
a column which is both keys shows the key mark and `PK`, then the green `FK`.)*

**Not passing as of 2026-09-24.** *(SUPERSEDED -- see the status at the top
of this section. Since then the desktop suite compiles and passes,
persistence passes at format version 33, and core fails only the drawn-key
test, for the reason given there.)* The uncommitted bridge-key work leaves four
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

*Rewritten 2026-10-02 from an audit of the code. This section used to list
drag resize handles, which are built -- everything that can be resized is
pulled by all four edges and corners -- and logical metadata, which is built
(Phase 13). It said the relational half of export waited for a schema to
generate it from; that schema now exists. What it listed that is still not
built is kept below.*

**Not built.**

- **Reordering a table's columns** -- the interaction. Its foundation is
  built (Task 4B; `Editor::set_column_order`, which nothing in the interface
  calls yet). The gesture is not decided.
- **SQL**: generating DDL from the schema; `.sql` export; reading SQL in (the
  *SQL Script (DDL)* card, and *Import ▸ From another tool…*).
- **Mermaid ER** and **DBML** export.
- **Conversion readiness**: `readiness()` and the ADR-009 gate it feeds
  (Phase 14).
- **The generation baseline** and three-way reconciliation (Phase 25).
- **Templates** beyond the one starting frame: a gallery, *Save as template*,
  thumbnails, and project folders as a library (ADR-016).
- **A table's and a column's comment on the schema side.** The model carries
  them; only the diagram's Properties shows an element's schema comment.
- **A foreign key drawn by hand into a key of several columns.** Connect
  refuses it as not supported yet.
- **Tables, and Connect, made by hand in a project begun as a diagram.**
- **Checking the schema**: a Check Model for Relational Design.
- **Renaming a discriminator column.**
- **Naming in conversion.** A schema converted into its diagram names each
  relationship from its foreign key's column, or *Has* where the column says
  nothing more; the names the conversion gives the other way have faults
  recorded with the two large examples (naive plurals, missing spaces).
- **Interface wording:** the disabled *Import ▸ From another tool…* still
  says there is nowhere to put SQL, CSV and JSON.
- **Technical debt:** `SchemaView` has no `Q_OBJECT`, so tests find it by its
  object name; the Debug desktop suite can run past ctest's time limit.
- **Longer term:** the Data workspace; multiple pages; autosave and recovery;
  a cross-project clipboard; spacing shapes out evenly (alignment guides and
  group alignment are built); freely draggable connector endpoint handles.

Connector joins can be pinned at their current positions,
and attribute/participant links support multi-point routes. Attribute kinds currently
form one exclusive enum, so composite keys or other combinations require a
deliberate model/format decision.

**Product decisions needed (Zain).** Each waits on a decision, not on
building:

- ~~The names of the two *Table detail* choices~~ -- **decided 2026-10-02**:
  *Physical schema* and *Compact schema* (see *Optional compact schema
  view*). What Physical schema adds next -- defaults, checks, indexes and
  the rest of a physical design -- is building work, not a naming decision.
- ~~Whether Relational Design gets the ribbon~~ -- **decided 2026-10-06**:
  it has the same File, Home and Settings tabs, keeps its tools in its
  header, and its rows leave out what acts on the diagram alone.
- The gesture for reordering columns.
- *Strong* or *Regular* for an entity that is not weak. Today an entity is
  *Regular* or *Weak*, and a relationship *Regular* or *Identifying*.
- What optimization is to cover.
- What icon and information polish is to cover. Known: the Outline icons'
  "on" drawing is hard to see on the Conceptual Explorer's open group rows.
- Borderless, free text boxes on the diagram, and whether notes or text
  belong on the schema. Already available on the diagram: the card note, the
  plain note and review comments.
- Any presentation of cardinality and optionality beyond what is there: the
  four notations (Chen, Min–max, Crow's foot -- the default -- and Bachman,
  chosen from either canvas's Appearance menu) and *Show constraints on this
  side*. For example, showing them as facts in schema Properties.
- The order of a key of several attributes on the diagram, which follows
  the attributes' identities, so one added later can land in the middle.
- The row menu's *References ▸*, which still re-points and retypes a foreign
  key, against Stage 5's rules.
- Whether Home's sidebar *Examples* row offers the two large examples too.
- In a project begun from its schema: whether the Conceptual preview becomes
  editable, and whether Design gets a Conceptual Design entry.
- Retaking the conceptual and relational-design reference pictures.
- How the uncommitted work reaches `main`: pull requests #23–#29 are open,
  and everything since 2026-09-27 is uncommitted; format-on-save restyled
  three source files whole along the way.
- ~~ADR-021 §5b: whether a key may be drawn on a relationship. The
  validation rule `attribute.key.relationship` refuses it, which is the
  failing core test.~~ **Decided 2026-10-05:** allowed exactly where the
  relationship is represented by a table of its own (many to many, or
  associative); refused elsewhere. Built; the core test passes.
- What adding another workspace to a project looks like.

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
