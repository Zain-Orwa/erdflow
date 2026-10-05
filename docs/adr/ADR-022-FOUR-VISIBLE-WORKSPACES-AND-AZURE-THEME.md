# ADR-022 — Four Visible Workspaces, Theme Token Resolver, and the Home Menu

> **Settled work.** Do not change, replace or re-style anything in this
> document to suit something new you have been asked to build. If what you are
> building genuinely contradicts what is written here, stop and ask Zain, who
> owns this project: say what you want to change, what the application will
> look like afterwards, and whether it is a gain or a loss. He decides.
> Full rule: [CLAUDE.md](../../CLAUDE.md).

**Status:** Accepted

**Date:** 2026-09-23
**Project:** ERDFlow
**Decision Scope:** Visible product workspaces, theme token architecture, menu bar, start-route readiness

**Amends:** ADR-003 §, ADR-008 §7, ADR-012, ADR-016, ADR-021 §2.1

---

## 1. Context

The Master UI Specification introduces a Home screen, a new default theme
(`ERDFlow Azure`), and a product language of **four** visible levels. The
repository's accepted architecture described **five**, and its `Theme` record
could not express the new theme's token set. Four blockers were reported and
four decisions were taken. This records them.

Nothing here is a silent revision. Where an accepted ADR said something else,
it is amended by name below and keeps its own text.

---

## 2. Decision 1 — Azure is fully specified; existing themes derive

A **theme token resolver** sits between the `Theme` record and everything that
paints.

```text
Existing theme                         ERDFlow Azure
  accent, border, muted                  full semantic token set
        │                                        │
        └────────▶ Token Resolver ◀──────────────┘
                        │
                   complete tokens
```

`ERDFlow Azure` defines the whole semantic set explicitly: primary and its
hover, pressed, soft and faint variants; two border strengths; heading,
primary, secondary and muted text; hover and selected-card surfaces; gold,
gold-soft and lavender accents; radii, shadows and typography.

The other nineteen themes define none of it and are **not** modified. The
resolver derives sensible values for them from what they already carry, so
they keep working exactly as they do now, and any of them may be upgraded to
full tokens later without a migration.

**Resolution happens once, when a theme is applied** — never inside a paint
path.

### Why

Widening the `Theme` record would have required giving nineteen working themes
values for two dozen new fields, which is maintenance work with no benefit and
a real risk of changing themes that are already right.

---

## 3. Decision 2 — Four visible workspaces

The product shows four:

```text
Conceptual ERD  →  Relational Design  →  SQL  →  Data
```

The previously separate **Relational Schema** and **Physical / Table Design**
stages are one visible workspace, **Relational Design**. A user does not
describe the same table twice.

### What this does not mean

It does **not** flatten the model. Internally the two concerns stay apart:

```text
Relational Design workspace
├── relational structure     relations, columns, primary keys,
│                            foreign keys, logical constraints
└── physical implementation  SQL types, precision/scale/length, indexes,
                             defaults, generated columns, dialect options
```

Physical detail must not contaminate pure relational rules. The rule is:

> **Visible product workspaces: four. Internal semantic layers: as granular as
> the architecture needs.**

### Consequence for ADR-021

ADR-021 §2.1 reserved `TableId` for "the future Physical/Table Design level".
That reservation **stands**, and its reasoning is unchanged: `Relation` and
`Table` remain distinct internal concepts, and the lineage
`Entity → Relation → Table` is intact. What changes is only that `Table` will
surface inside the Relational Design workspace rather than in a workspace of
its own.

---

## 4. Decision 3 — Menu bar keeps `Edit`

```text
File · Home · Edit · Insert · Design · View · Help
```

The specification's mock showed no `Edit`. It stays, because Undo, Redo and the
conventional editing commands live there and moving them to match a picture
would cost predictability for nothing.

| Menu | Holds |
| --- | --- |
| **File** | New, Open, Save, Save As, Import/Export entry points, Close, Exit |
| **Home** | Home/Welcome, New Project, Open Project, Recent, Examples, Templates |
| **Edit** | Undo, Redo, Cut/Copy/Paste, Delete, selection actions |
| **Insert** | Creation actions for the active workspace |
| **Design** | Arrange, Appearance, conversion controls, workspace design settings |
| **View** | View, layout and display controls |
| **Help** | Help, about, documentation |

`Insert` and `Design` change with the active workspace: Entity/Attribute/
Relationship/Specialization/Note on the Conceptual side; Table/Column/Foreign
Key/Note on the Relational Design side.

---

## 5. Decision 4 — `Relational Design First` is visible but disabled

The Home screen shows all five start routes in their intended order from the
first release of the screen. **`Relational Design First` is disabled**, with a
`Coming soon` badge and a tooltip, until ADR-021 **Step B** is built and
verified.

The disabled styling keeps the card's visual weight — it must read as *not yet*
rather than as broken or missing.

### Readiness gate

The card is enabled only when all of these exist and are tested:

- relations created by hand, with no origin;
- relation creation commands;
- relation persistence;
- relation layout persistence;
- key and foreign-key creation;
- a save/load round trip;
- tests proving a schema-first project works with no Conceptual ERD.

Enabling it because the Home screen exists is not sufficient.

---

## 6. Start routes

> **Amended by §9.14.** Zain settled on 2026-09-23 that From Template /
> Example and Import Existing are not cards: they are the sidebar's Templates
> and Import rows. The Home screen shows **three** cards. The list below is
> kept as it was decided.

The Home screen offers **five** ways to start, superseding the three recorded
in ADR-016:

1. Conceptual Model First
2. Relational Design First *(disabled until Step B; see §5)*
3. SQL First *(disabled until the SQL-first workflow exists; see §9.2)*
4. From Template / Example
5. Import Existing

Five routes above four levels is deliberate: **Import Existing is an action,
not a modelling level.**

---

## 7. Documents amended by this decision

| Document | What changes |
| --- | --- |
| `docs/1.ERDFlow_PRODUCT.md` | Level chain; §28 becomes part of Relational Design |
| `docs/2.ERDFlow_ARCHITECTURE.md` | Level chain |
| `docs/5.ERDFlow_ROADMAP.md` | Level chain; Phases 24 and 29 marked merged into / superseded by Relational Design (§9.5) |
| `docs/IMPLEMENTATION_STATUS.md` | Level references and ADR-021 wording |
| `docs/adr/ADR-003-DESKTOP-UI-TECHNOLOGY.md` | Names a Physical/Table Design workspace |
| `docs/adr/ADR-008-CROSS-LEVEL-MAPPING-AND-PROVENANCE.md` | §7 five-level list |
| `docs/adr/ADR-012-DATA-SOURCE-ARCHITECTURE.md` | Names Physical/Table Design |
| `docs/adr/ADR-016-PROJECT-ORGANISATION-AND-START-EXPERIENCE.md` | Three start routes → five; what Create Project does with the details (§9.10) |
| `docs/1.ERDFlow_PRODUCT.md` §*The start screen* | Pointer to the Home screen this ADR describes |
| `docs/DEVELOPMENT.md` | Focused pull requests (§9.13) |
| `docs/adr/ADR-021-…` | §2.1 clarified, not reversed |

Each keeps its own text and gains a pointer here, so the earlier decision stays
readable rather than being rewritten out of history.

---

## 8. Consequences

**Good.** A simpler product to learn and navigate. The theme resolver means one
fully specified theme without touching nineteen working ones. `Edit` stays where
people expect it. The Home screen can show the whole direction without promising
a route that cannot finish.

**Costs.** Two dozen semantic tokens must be derived convincingly from three
fields for the existing themes — a derived token will rarely be as good as a
chosen one, and some themes will look merely acceptable where Azure looks
right. The Relational Design workspace must hold two concerns without letting
either leak into the other, and that discipline is easier to state than to keep.

**Watch for.** The temptation to enable `Relational Design First` early, and the
temptation to let a dialect-specific physical detail be treated as a relational
rule because they now share a workspace.

---

## 9. Confirmed and extended, 2026-09-23

Building the Home screen through stage UI-6 raised three questions and
several gaps. Zain answered all of them on 2026-09-23, and the answers are
recorded here so that the repository, not a conversation, is where they live.
None of them reverses §2–§6. Each either answers a question those sections
left open or states a limit they did not.

### 9.1 The menu bar is the native menu bar; the ribbon is separate

Decision 3 (§4) means the application's **native, top-level menu bar**:
`File · Home · Edit · Insert · Design · View · Help`. It does not mean the
editor's ribbon.

The ribbon is a separate, workspace-specific tool system. It is not removed or
renamed to match the menu bar. Three things are called "Home" and they are
distinct:

| Name | What it is |
| --- | --- |
| **Home screen** | The page ERDFlow opens on (`HomePage`) |
| **Home menu** | The native menu between File and Edit: application navigation and common starting actions |
| **Home tab** | The ribbon tab holding the modelling tools of the open workspace |

### 9.2 `SQL First` is visible but disabled, like `Relational Design First`

The same product-safety principle as §5 applies: a route is enabled only when
the workflow behind it works end to end, never because its card exists.

SQL is a real product level in both directions:

```text
Relational Design ──generate──▶ SQL
Relational Design ◀──parse───── SQL
```

`SQL First` is enabled only when ERDFlow can reliably accept SQL or DDL
(written, pasted or imported), parse it, produce valid Relational Design
objects, keep them, save and load them correctly, regenerate SQL from them,
and prove all of that in tests. Until then it is shown in its place, disabled,
and marked *Coming soon*.

### 9.3 Azure's primary stays `#1E88E5`; `#1976D2` is an accessibility override

`#1E88E5` is ERDFlow Azure's primary and brand colour and is not replaced
globally. Where white text sits on a filled blue surface and must reach this
project's 4.5:1 contrast, `#1976D2` (Azure's own hover shade, 4.55:1) is used
instead. The first instance is a selected row.

This is an **accessibility override derived from the theme**, not a design
deviation. Where accessibility and pixel-exact matching of the reference
conflict, accessibility wins.

### 9.4 Four visible workspaces; the internal model stays as granular as it needs

Visible product workspaces: **four**, namely Conceptual ERD, Relational
Design, SQL and Data. Internal semantic concepts: **may be more than four.**

Relational Design presents relational structure (relations, columns, primary
and foreign keys, logical constraints) and physical implementation detail
(SQL types, length, precision, scale, indexes, defaults, generated columns,
dialect and performance settings) in one workspace. Internally the two stay
distinct wherever that helps correctness, maintainability, scale, safety or
future dialect support. `RelationId` remains the Relational Design identity.
A visible standalone Physical Design workspace is not to be reintroduced, and
"Relational Schema" is not the current user-facing name of the workspace.

### 9.5 Roadmap phases keep their history

Roadmap phases that separately name a Relational Schema workspace (Phase 24)
and a Table / Physical Design workspace (Phase 29) are marked **merged into,
and superseded by, Relational Design** as visible workspaces. Their text is
kept, and the work they list is still work. Where an ADR or record said
something else, it gains an amendment note rather than being rewritten.

Start routes are not product levels. There are five routes (§6) above four
levels.

### 9.6 The visual reference is 4:3, at 1440 × 1080

The canonical reference viewport is **1440 × 1080**. It is used for the
canonical SVG, screenshot comparison, visual-regression tests, layout audits,
and implementer–reviewer comparison.

It is a reference, **not a window size.** The application stays responsive.
Type stays readable, components keep their proportions, cards wrap rather
than shrink below their minimum width, and the learning panel and sidebar
collapse only at their intentional breakpoints. The interface is never scaled
like a bitmap or squashed to hold 4:3.

### 9.7 The window keeps its native frame

The canonical SVG draws window controls in a custom top bar. Reproducing them
would need a frameless window, a change to the native desktop shell with real
platform risk, so it is **not done in this programme**. The native frame stays,
and the safe parts of the top bar may be recreated inside the existing Qt
Widgets architecture. The SVG is a visual reference, not permission to
destabilise the shell. Where safety and native behaviour conflict with
decorative exactness, safety wins.

### 9.8 The hero illustration: motion as built, and no more

> **Amended by §9.15.** The drawing and its motion were corrected on
> 2026-09-23: four panels travel along rings around the database instead of
> three drifting in place.

Accepted as built: Qt Widgets, painted vector drawing (no bitmap, no QML), and
card data reusable in product mode (*Conceptual / Relational / SQL*),
marketing mode (*Design / Convert / Generate*) and custom mode.

The motion is fixed at:

- a 12-second loop, drifting at most about 4 px across and 3 px up and down
  (authored units);
- a faint connector pulse and a subtle database breathing, with each
  connector attached to its moving card;
- hover freezes only the hovered card, which then resumes from where it
  stopped;
- the clock runs only while the Home screen is visible;
- turning motion off returns every card to its resting place.

It is not to be made more aggressive. The illustration is decorative and must
never draw attention from the project-creation controls.

### 9.9 Reduced motion goes through one seam

Qt offers no reliable cross-platform reduced-motion setting, and fragile
platform hacks are not to be added just to claim automatic detection. For now
the preference comes in through one explicit switch, `ERDFLOW_REDUCED_MOTION`.
That switch sits behind a single function
(`WelcomeFlowIllustration::platform_prefers_stillness`), where a native reading
can be added later without touching its callers. This is a documented
limitation.

### 9.10 Create Project must use every detail it asks for

> **Superseded by §9.19:** Home no longer asks for any details. Where a
> project's name and location are asked is still to be decided.

The details form asks for a name, a location, a description, and whether to
create a project folder. **All four must take effect.** At present only the
name does, and that is a gap, not a design.

- With *Create project folder* ticked, the project is written to
  `<location>/<name>/<name>.erdx`.
- Unticked, it is saved according to the location and file model of ADR-016.
- The description is never silently discarded. It is kept in the project's
  metadata once the file format has a place for it.
- Nothing may bypass the persistence boundary of ADR-005.

### 9.11 Unwired controls are gaps, not removals

> **Amended by §9.14:** Import now sits in one group with Examples and
> Templates.

The Home sidebar is, in order: Home, New Project, Open Project, Recent ·
Examples, Templates · Import · Settings, Help. Recent, Templates, Import,
Settings and Help, and the learning panel's *Open an example project* and
*View tutorials*, are to be **wired**, not removed because they are inert
today. The nine sidebar icons are drawn as one coherent family, never a mix of
styles.

### 9.12 What the remaining stages must deliver

**UI-7** dresses the real workspaces in Azure without changing what they do:
Conceptual ERD, Relational Design, SQL and Data as they are built, and the
Explorer, Properties, toolbars and ribbon, dialogs and contextual controls.
Relational Design must not offer conceptual-only creation tools. It offers
Table / Relation, Column, primary and foreign keys, Note, Arrange, Appearance
and the physical-detail controls. Conceptual offers Entity, Attribute,
Relationship, Specialization and the like. Outdated user-facing wording such
as "Schema preview" or "Relational Schema", where it names the unified
workspace, becomes **Relational Design**. Internal terminology that is still
semantically right is not renamed.

**UI-8** adds the quality layer: visual-regression infrastructure at the
1440 × 1080 reference, with reference screenshots of the Home default,
selected-card and hover states and of the Azure Conceptual and Relational
Design workspaces. It also adds keyboard-navigation, focus-order,
accessibility and contrast audits, and a full comparison against the Master
UI Specification, the canonical SVG and the tokens.

### 9.13 Work reaches the repository as focused pull requests

Unrelated changes are not bundled into one pull request for convenience. Two
changes share one only when a real technical dependency makes them
inseparable. Before the first push of a batch of work, the split is shown
file by file, with the dependencies between the pull requests, passing
tests, no secrets, and no untracked implementation file left behind, and it
is approved. See `docs/DEVELOPMENT.md`.

### 9.14 Home corrected, second round, 2026-09-23

> **Amended by §9.16:** a rule now parts Import from Examples and Templates,
> the cards keep a door's shape rather than filling the line, and a window
> too short for the page scrolls rather than squash the cards.
>
> **Amended by §9.18, then §9.19:** each card carries its own *+ Create*,
> and nothing is asked under them.
>
> **Amended by §9.20:** the cards are titled *Conceptual Design (ERD)*,
> *Relational Schema* and *SQL Project*. The sidebar has no New Project row,
> and the learning panel has no *Open an example project* link.
>
> **Amended by §9.22:** Examples, Templates and Import are no longer in the
> sidebar. Each is offered in the workspace it works on.

Zain reviewed the Home screen as built and corrected it. None of this is a
silent revision: §6 and §9.11 carry pointers here and keep their own text.

**Three start cards, not five.** *From Template / Example* and *Import
Existing* are not cards. They are places to go rather than ways of modelling,
and they live only in the sidebar, as its Templates and Import rows. The
cards are Conceptual Model First, Relational Design First and SQL First, in
that order, with the specification's copy. The second and third stay
disabled and marked *Coming soon* (§5, §9.2). An earlier pass had enabled
both. That was a defect against §5 and §9.2 and has been reverted.

**Import sits with Examples and Templates.** The sidebar is: Home, New
Project, Open Project, Recent · Examples, Templates, Import · (space) ·
Settings, Help. A rule parts the first two groups; Settings and Help stand at
the foot of the rail. **Export does not appear on Home.** It belongs to an
open project.

**One page, never scrolled.** Everything on Home is on screen at once. Where
a window is short, what gives way is, in order: the spacing, the illustration,
then the size of the card drawings and of the details heading. The words, the
fields and the buttons keep their size. It is tested at the size ERDFlow
opens at (1440 × 920) and at 1366 × 740 and 1280 × 720. A window shorter than
that still reaches everything through a scroll bar, as a last resort.

**The cards share one line.** They never wrap onto a second row. Each is as
wide as the line allows, up to 380 px, and its height follows from its width.
A card added later makes every card narrower rather than starting a new row.
This replaces the specification's fixed 180 × 292 cards that wrap.

**The top of Home.** The native menu bar (File · Home · Edit · Insert ·
Design · View · Help) stays on Home and is never hidden in favour of an
in-window copy. While Home is showing, the ribbon's rows give way to a slim
in-window bar carrying only the ERDFlow mark, a Settings button and a Theme ▼
control. Its menus are the window's own, not copies. Settings holds Theme,
Icons and Notation. The alternative, an in-window bar with its own File,
Home, Insert… buttons replacing the native menus, was rejected. It would show
the menus twice on macOS and make them non-native elsewhere.

Consequences recorded with it:

- Create Project now uses every detail (§9.10). With the folder ticked the
  project is written to `<location>/<name>/<name>.erdx`, otherwise to
  `<location>/<name>.erdx`. The description is kept in the file (format
  version 27). A name is kept exactly as typed inside the project; only the
  file name replaces characters a file system refuses. An existing file is
  never overwritten. The project is written before it is put in front, so a
  failure leaves the open work untouched and is said on the form.
- Every sidebar row and learning link is wired. Recent lists the ten
  projects last opened, saved or created. Templates and Examples open the
  bundled University project untitled, which is what starting from a template
  is (ADR-016) while it is the only one. Import opens the same chooser as
  Open Project until SQL and database sources have somewhere to go. Settings
  and Help open their menus beside the row. *View tutorials* opens the quick
  guide.
- Home's text is sized in the specification's pixels. It had been converted
  to points at 96 dpi, which made it a quarter smaller than specified on
  macOS.
- Where Azure's colours would put ordinary text under 4.5:1, the colour is
  deepened only as far as needed (§9.3): the chosen sidebar row and Create
  Project take `#1976D2`, the learning links and the form's error line are
  darkened slightly. `#1E88E5` stays the brand primary everywhere else.

### 9.15 Home fidelity correction, 2026-09-23

> **Amended by §9.16:** the 3° swing is replaced by a full, continuous
> revolution; hovering no longer holds a panel; the lines are straight
> segments; the cards stop at 315 pixels and keep a door's shape.

Zain sent a correction brief with pictures of the Home screen as built and as
wanted. It keeps everything in §9.14 and corrects how the page looks. Three
of those corrections supersede something written earlier, as noted.

**The illustration is a large database with four panels circling it.**
`WelcomeFlowIllustration` now draws a three-tier database in layered blues
on a rounded platform, over a soft blue glow with no edge. Four panels stand
around it, read by their marks rather than by words: a small structure of
boxes, an entity–relationship–entity with its gold diamond, a table on a blue
panel, and a page of SQL on a warm one. Each panel leans towards the database
and has a faint shadow. Smooth curved lines run from the panels into the
platform. The panels' titles (Conceptual ERD, Relationships, Relational
Design, SQL) are what a screen reader reads and what pointing at a panel
reveals under it. The **three start cards and the four panels are different
things**: the cards are controls, the panels are decoration.

**The motion is orbital, and supersedes §9.8's drift.** Each panel sits on a
ring around the database's own centre and travels along it, 3° either side
of where it rests, once every 12 seconds, easing at each end. Each starts at
a different point in the loop. A panel behind the database is slightly
smaller and fainter than one beside it (0.965 and 0.86 at the back). The
database stays where it is and only breathes. Each line is fixed to its
panel's edge and bends as the panel moves. Hover still freezes only the panel
pointed at, and turning motion off still puts every panel back where it
rests. This moves a panel up to about ten authored pixels along its ring,
more than the four §9.8 allowed. It stays a swing of a few degrees rather
than a full revolution, so the picture is recognisably the same at every
moment. A full slow revolution was not chosen, because it would read as a
carousel beside the headings.

**The illustration stands beside the welcome and is larger.** It is no
longer in the header row. It stands at the right of the centre, its foot
level with *Create a new project*, rising into the room kept above the title,
up to 230 pixels tall. It steps aside when the page is short or the words
leave too little room across.

**The cards fill their line** (supersedes §9.14's 380-pixel limit). The
three cards share the whole line, stopping only at 560 pixels each on a very
wide window. Their drawings grow a little with them, up to 1.3 times. A card
that cannot be taken yet keeps its colours at half strength and its words
readable: the title in the secondary text colour, the body muted, the badge
as it was. A chosen card carries a faint pale-blue light at its top.

**The page reads from the top.** A fixed 64 pixels is kept above the welcome
for the illustration. There is more room between the subtitle and *Create a
new project*. What a tall window has to spare goes mostly under the form.

One page (§9.14) still holds at 1440 × 920, 1366 × 740 and 1280 × 720, and
the reference pictures, including one of the illustration on its own, were
taken again.

### 9.16 Home, final fidelity pass, 2026-09-24

Zain sent a further brief with the reference pictures. Two of the choices in
§9.15 were an agent's own, and he corrected both: *orbit* means a real,
continuous revolution, and *responsive* means keeping proportions, never
stretching.

**The panels revolve.** All four panels travel the one elliptical orbit
round the database, 178 × 80 authored units, a whole revolution every 18
seconds at a constant 20° a second, without end. They start a quarter-turn
apart and so stay a quarter-turn apart. At rest, and at the start, they
stand at the four diagonals: the relationships and table panels behind the
database, the hierarchy and SQL panels in front of it at either side.
Hovering highlights a panel's border and stops nothing. Only a hidden Home
screen or reduced motion stops the orbit; reduced motion shows the starting
composition. A panel at the back is smaller (0.92) and fainter (0.78) and is
painted behind the database; a panel at the front is painted in front of it.
Panels stay upright and lean a little towards the middle at the sides. The
whole drawing and every panel scale uniformly, one factor for both axes.

**The lines are straight.** Each is two straight segments with one bend,
2–2.4 pixels in light Azure (`#79BDF2`), with rounded ends and joins, worked
out again every frame. It leaves the panel at the point on its edge that
faces the platform: the inner side for a panel beside the database, the foot
for one behind, the top for one in front. It ends on the platform with a
bead. The bend runs across first from a side and down or up first from the
foot or top, turning gradually between them at a corner so it never snaps.
The glow and the platform, which do not move, are drawn once for each size
and theme and kept. A frame redraws only the lines, the panels and the
database, about thirty times a second.

**The cards are door-shaped.** Each is 1.10 times as tall as it is wide, its
height derived from its width. Widths run from the line's share down, never
past 315 pixels. On a wide window the cards stop at 315, the gaps widen a
little, and the group stands centred. The drawing area grows from 88 to 105
pixels with the card. The helper line stands at the card's foot with the
spare height above it. A card not yet available shows its title at 76%, its
body and helper at 74%, and its drawing at 60%; the badge is at full
strength. Only a card narrower than its words allow (on narrow windows) is
taller than 1.10; none is ever wider than its shape.

**Short windows.** One page still holds at 1440 × 920 and 1440 × 1080, and at
1366 × 740 in compact mode. There the cards narrow, keeping their shape, to
fit the room the rest leaves. On a shorter window the page scrolls rather
than squash the cards, as the brief prefers. The cards are sized from a
width that allows for a scroll bar, so a scroll bar coming and going never
changes their height. The details form's error line is its own full-width
row and is not wrapped. Both stop the page and its scroll bar from setting
each other off; an earlier arrangement recursed until the stack ran out.

**The sidebar has a rule before Import:** Home, New Project, Open Project,
Recent · Examples, Templates · Import · (space) · Settings, Help.

The reference pictures were taken again.

### 9.17 The hero's lines bend twice, and its panels take any content, 2026-09-24

Zain approved the orbit and corrected the lines. A line that bends once, a
run out of the panel and then a slant into the platform, is not what the
reference shows. **Every line now bends twice**, giving three segments:

- A short straight run leaves the panel. It goes across from a panel at the
  side, and down or up from one behind or in front. As before, it leaves from
  the point on the edge that faces the platform, which slides round the edge
  as the panel travels.
- A slant carries it over towards the platform.
- A second straight run enters the platform. It points the same way as the
  first and is **the same length**.

Every line uses the same rule, and no panel has coordinates of its own. The
runs are 26 authored units long, but never longer than a third of the
distance the line covers, so the middle segment stays a slant and never
folds back. Near a corner of the panel the runs turn gradually from across
to down; they never snap. The two bends have small rounded corners
(4 authored units), not curves. Where a panel covers its own platform point,
as one straight in front of the database does, the runs shrink to nothing
and the line lies hidden under the panel, as it did before.

**For a panel behind, the line plugs into the database's side.** The
platform point for a panel behind the database is often behind the database
too. The last run was then partly hidden, and in the resting composition the
two runs looked unequal. The line now ends where it goes behind the
database, so both runs are fully visible and equal, and its bead sits half
visible at the database's edge. The adjustment fades as a panel moves round
behind and its runs turn from across to down. Otherwise the hidden part
would be measured up through the whole height of the database, and the
line's end would race up its side.

**The panels can carry other content.** Two optional fields were added to
`HeroOrbitItem`. `draw` paints a caller's content on the panel face in
place of the mark. It is clipped to the panel's rounded shape and never
seen by the orbit, lines or database. `on_press` is what pressing the panel
does, and a pointing hand shows over a panel that has one. The product's
four panels set neither, so they stay decoration and a press passes through
them. Nothing promotional or external is switched on; this only means that
later content does not require rewriting the orbit.

Tests check four points and two bends at the quarter positions, equal and
parallel runs, a first bend outside its panel, clear bends on the panels
behind, and that no point of any line moves more than a little in any
hundredth of a second over a full revolution. The reference pictures of the
hero and Home were taken again.

### 9.18 The cards name what they make, and only the name is asked, 2026-09-24

> **Superseded by §9.19**, the same day. Kept as it was decided.

Zain approved a simpler way to start. Choosing and creating stay two acts:
pressing a card only selects it, and nothing is made until the button is
pressed.

**The card titles** are now *Create Conceptual Design*, *Create Relational
Design* and *Create SQL Project*. Only the titles changed. Each card keeps
the specification's two lines, its place and its drawing. The second and
third cards stay disabled and marked *Coming soon* (§5, §9.2).

**The form asks only for the name.** The *Project details* heading is gone.
No heading replaces it, because *Create a new project* above the cards
already says what the area is for. Under the cards and their rule there is:

```text
Project name
[ University Management ]
Saved to ~/Documents/ERDFlow/University Management/
› More options
                                 [ Create Conceptual Design ] [ Cancel ]
```

- **The save line** is always shown and follows the name, the location and
  the folder choice as they change. It names the folder exactly as Create
  will, using the same rule for characters a file system refuses. Under the
  home folder it starts with `~`, except on Windows. A long path is shortened
  in its middle, and the whole path is in its tooltip.
- **More options** opens to Location with Browse… and Description
  (optional), side by side. *Create project folder* appears level with the
  buttons, where it stood before. The defaults are unchanged: Documents/ERDFlow,
  with the folder ticked. The side-by-side layout keeps Home one page with
  More options open at 1366 × 740 as well. Stacked, it was 91 px too tall.
- **The button follows the chosen card** and uses the card's title as its
  words. Only *Create Conceptual Design* can be chosen at present.
- **A problem with the location opens More options.** This covers an empty
  location, a project already at that place, a folder that cannot be made,
  and a file that cannot be written. The error line says what is wrong, and
  the field it is about is then visible.

§9.10 still holds: every detail still takes effect. The Home and card
reference pictures were taken again, and a picture of Home with More options
open was added.

### 9.19 Each card creates; nothing is asked under the cards, 2026-09-24

Zain corrected §9.18. The Home screen ends with the row of cards.

**Each card is laid out top to bottom** as title, then a *Coming soon* row,
then the drawing, the description, and the card's own **+ Create** button,
centred at its foot:

```text
┌──────────────────┐  ┌──────────────────┐  ┌──────────────────┐
│ Conceptual Design│  │ Relational Design│  │ SQL Project      │
│                  │  │ Coming soon      │  │ Coming soon      │
│      [icon]      │  │      [icon]      │  │      [icon]      │
│ description      │  │ description      │  │ description      │
│    [+ Create]    │  │    [+ Create]    │  │    [+ Create]    │
└──────────────────┘  └──────────────────┘  └──────────────────┘
```

- The titles are *Conceptual Design*, *Relational Design* and *SQL Project*.
  The descriptions are still the specification's.
- The *Coming soon* row is kept, empty, on the card that can be taken, so the
  three drawings and descriptions line up across the row.
- The specification's helper line ("Best for…") is kept in the card's data
  but no longer drawn. With it, the card's contents would no longer fit its
  door shape (1.10 times as tall as wide), which was settled earlier.
- **+ Create** is filled in the primary, as the chosen sidebar row is. It is
  a real button: it is reached by Tab straight after its card, and is read
  out as *Create Conceptual Design*. It is disabled along with a card that
  cannot be taken yet, and says why in its tooltip. The keyboard starts on
  the chosen card's + Create.
- **Pressing a card still only chooses it.** Only its + Create starts.
  Conceptual's + Create opens a new, untitled conceptual project, as File ›
  New project does. Nothing is written until the project is saved.
- **Removed from Home:** the name, the save line, More options, Location,
  Browse, Description, *Create project folder*, the separate Create button
  and **Cancel**. Where a project's name and location are asked is decided
  separately.

Consequences recorded with it:

- Cancel was the only way from Home back to the work already open. Home can
  now be left only by starting or opening something, through a card, New
  Project, Open Project, Recent, an example or a template.
- Creating a project from Home no longer writes a file, and no longer adds it
  to Recent. That happens when it is first saved.
- `ProjectDetailsForm` is no longer used by Home. Its files are kept for the
  separate decision on where name and location are asked.

The Home and card reference pictures were taken again. The More options
picture from §9.18 was removed.

### 9.20 Navigation cleanup and the Relational Schema card, 2026-09-24

> **Amended by §9.22:** the sidebar is now Home, Open Project, Recent ·
> (space) · Settings, Help. Examples, Templates and Import moved into the
> workspaces, so the learning panel's missing example link is now reached
> from the Conceptual workspace's menus.

Zain removed two entries that duplicated something already on Home, and
renamed one card.

- **No *New Project* in the sidebar.** The cards are where a project is
  started, so the row repeated them. The sidebar is now: Home, Open Project,
  Recent · Examples, Templates · Import · (space) · Settings, Help. File ›
  New project and the Home menu's New project stay.
- **No *Open an example project* in the learning panel.** The sidebar's
  Examples row opens the same example. *View tutorials* stays.
- **The second card is titled *Relational Schema*.** Its tooltip says
  "relational schema" to match. Its description is unchanged: *Start directly
  with tables, columns, keys and constraints.* Nothing else changed: not the
  badge, drawing, button, size, disabled state or the other cards.

  This title differs from the workspace, which is still called *Relational
  Design* everywhere else in ERDFlow (§3, §9.4). The SQL card's description
  also still says "the relational design". Both were left as they were,
  because Zain asked for this one card only.

The Home and *Coming soon* card reference pictures were taken again.

**Later the same day, the first card was titled *Conceptual Design (ERD)*.**
Nothing else changed. The title still fits on one line at the reference
width, and the cards keep their door shape at every size tested. The Home
and first-card reference pictures were taken again.

### 9.21 Live demos under the cards, 2026-09-24 (in progress)

Zain asked for a small, animated, painted demo under each start card,
showing that kind of project being made. It is to be built in seven stages,
each stopped at and reported. Stage 1 is done.

**The rules the stages keep:**

- **Non-interactive decoration.** The demos are painted, never built from
  the real workspaces or connected to the real conversion. Nothing in them
  can be pressed or focused.
- **One component, `HomeLiveDemo`.** It takes a `HomeDemoKind` (Conceptual,
  Relational, Sql). The shell handles size, theme, the floor, and later the
  timeline and pausing. Each kind's scene only draws into the space the
  shell gives it.
- **Aligned under the cards.** Each demo is exactly as wide as its card and
  stands directly under it, following the card wherever the row puts it.
  Every scene is authored at 280 × 200 and scaled evenly, never stretched.
- **The cards never give way.** The demo row sits on top of the page rather
  than in its column, so it takes only the empty space under the cards,
  keeping 12 px clear of the page's foot. No card moves, changes size or
  scrolls because of it. At 1440 × 1080 each demo is drawn at its card's
  full width (about 268 × 191). On a shorter window it is drawn smaller. Below 45%
  of its authored size it is not drawn at all.
- **No box.** A demo floats on a soft pool of the theme's pale primary, with
  no border. It uses theme tokens only, so Plain shows it in grey.

**Stage 1** built the shell, the three demos under their cards, and the
floor. It has no scenes and no animation yet. The desktop test checks at
five window sizes that there are three demos in the cards' order, each as
wide as its card, under it and centred on it, all the same height, with none
reaching into the next card's column and none taking keyboard focus. It also
checks that hiding the demos moves no card at all. The Home reference
picture was taken again.

**Stage 2** built the Conceptual scene. A scene is a list of data, not
freehand drawing. Each piece records its shape (entity, attribute,
relationship, connector or key mark), its label, where it stands, and the
step it arrives in. One painter in `home_demo_scenes` draws any scene at any
moment, so tests ask what is showing without comparing pixels. The scene has
twelve steps and takes 10.7 s a pass:

1. empty;
2. Student;
3. Course;
4. the Enrolled diamond;
5. the line from Student;
6. the line from Course;
7. Student's ID and Name, each growing its line before its oval;
8. Course's ID and Name, the same way;
9. Enrollment Date under the diamond;
10. both IDs underlined as keys;
11. the model held;
12. a fade.

A shape fades in and grows from 95% to full size. A line is drawn out from
the entity. Every shape stands on a faint shadow 1.8 units lower. Colours
are the Conceptual canvas's own (`entity_fill`, `attribute_border`, and so
on), shown in grey under Plain. Lettering is 11 units for entities and 9 for
everything else, which is about 10.5 and 8.6 px at the reference size. At
first it was set 2 units smaller, and could not be read at card size. The
floor moved down to 87% of the demo's height, under Enrollment Date. There
is no clock yet (stage 5), so Home shows the finished model.

**Stage 3** built the Relational Schema scene with the same pieces-as-data
approach. It has twelve steps and takes 11.0 s a pass:

1. empty;
2. the Students table;
3. its rows, StudentID and Name;
4. Courses;
5. its rows, CourseID and Name;
6. the Enrollments bridge;
7. its rows, StudentID, CourseID and EnrollmentDate;
8. the key marks;
9. the Students line;
10. the Courses line;
11. held;
12. a fade.

The tables look like the Relational Design view's: corners rounded by 2.5,
a header in the entity's colours (the relationship's for the bridge), the
theme's base colour for the body, a key gutter with its rule, faint lines
between rows, and column names in a fixed-width face. A primary key is a
small painted key, upright, in the theme's warning colour. The view draws
its key from the chosen icon set, which Home does not know, so the key is
painted as the cards paint their own drawings. A foreign key is the letters
FK in the theme's valid green. Each line runs from the right edge of the
referenced key's own row to the left edge of the exact row that references
it, with square corners, in a lane of its own. It has Crow's Foot ends: two
bars at the key, and a crow's foot with a bar at the foreign key. There is
no diamond, and no conceptual shape of any kind. Following the brief, the
bridge has only its three rows and no primary key of its own. ERDFlow's own
conversion would give it a separate key by default (ADR-021 §5b).

At first the rows were 15 units and the column names 8.5, which came out
about 8 px at the reference size and could not be read. They are now 17
and 9.5.

**Stage 4** built the SQL scene. It has ten steps and takes about 10 s a
pass:

1. empty;
2. an editor panel rises, titled `university.sql`;
3. `CREATE TABLE Students (` is typed;
4. its two columns;
5. `);`;
6. the Courses table;
7. the Enrollments table, with `REFERENCES Students` and
   `REFERENCES Courses`;
8. a strip at the editor's foot, "✓ 3 tables created";
9. held;
10. a fade.

The script is typed a character at a time at one steady speed, 40
characters a second. Each typing step lasts exactly as long as its run
takes at that speed, and a line break counts as a character, so there is a
short pause at each line's end. Code pieces arrive at a constant rate
rather than easing. `demo_code_at` gives the script typed so far at any
moment, which is what the tests check. A caret follows the typing. The
script is set in the fixed-width face, 9 units on 12-unit lines, with faint
line numbers. Its colours come from the theme: keywords in the primary,
types in lavender, numbers in amber, and punctuation muted. Each is
deepened where it would not read on the page, and all are grey under Plain.
The script is nine lines, and every line fits the editor. Nothing is run.

**Stage 5** added the clock. One `HomeDemoClock` plays all three demos from
a single 30 fps timer. Each demo is given the time since the clock started,
less its own delay: 0 s for Conceptual, 1 s for Relational and 2 s for SQL.
A demo whose turn has not yet come stands at its empty start. After its
first pass each goes round at its own length (10.7, 11.0 and about 10 s), so
the three drift apart instead of repeating together. Their first pieces
arrive at 0.6, 1.6 and 2.5 s, so no two begin moving at once.

- Time is always given to the clock, never read inside a demo. Tests and
  pictures can therefore set any moment (`show_at`) without waiting.
- Stopping the clock shows every demo finished. The visual suite stops it
  before taking pictures, so the Home reference picture is unchanged.
- Starting the clock begins every demo from its start.
- A demo standing in a step where nothing changes (the empty start and the
  hold) is not repainted on each tick.
- Home starts the clock when it is built. Until Stage 6, the clock keeps
  ticking while Home is hidden. A hidden widget is not painted, so this
  costs only the timer's wake-ups. Stage 6 is where it will pause.

**Stage 6** added reduced motion and pausing.

- **Reduced motion.** Home asks the one reduced-motion seam (§9.9,
  `WelcomeFlowIllustration::platform_prefers_stillness`) whether the demos
  are to play. The hero asks the same seam, and the environment variable is
  read nowhere else. Where stillness is asked for, the clock never runs and
  each demo shows its scene finished: the complete ERD, the three-table
  schema, and the written script with its result. So Home still looks
  complete.
- **Wanting to play and ticking are separate.** The clock ticks only while
  the demos are wanted and the demo row can be seen. It watches the row for
  hide and show events.
  - Away from Home (the pages switching, for instance), it stops, costing
    nothing.
  - Back on Home, every demo starts again from the beginning, with the same
    0 / 1 / 2 s offsets.

  The brief allowed either resuming or restarting. Restarting was chosen, so
  each return to Home shows all three being made again. It is the same rule
  for all three.
- **Nothing ticks before Home is first shown.**

**The demos move into the cards (Zain, 2026-09-25).** Zain changed where the
demos stand. They are to be inside their cards, not in a row under them:
title, the *Coming soon* row, the demo, then *+ Create*. The card's
description is no longer drawn. It is still read out by accessibility tools,
because the accessibility audit (§9.12, UI-8) requires every card to have a
description. All three cards stay one height, with their *+ Create* buttons
on one line.

- **Conceptual** was rebuilt by another agent to Zain's brief, and is taken
  as finished. It is a horizontal ERD centred on the card: Student,
  Enrolled and Course on one line, with Enrolled over the centre of
  *+ Create*. Each entity's ID and Name branch from one point on the entity.
  Crow's Foot zero-or-many ends sit on both sides, and Enrollment Date hangs
  under the diamond. It has fourteen steps.
- **Relational Schema** was moved into its card, as step 1 of Zain's order.
  Its arrangement is the approved one: Students upper left, Courses lower
  left, Enrollments on the right, with row-exact PK-to-FK lines. The whole
  arrangement was moved down 22 units so it is centred in the box, because a
  demo inside a card has no floor below it. It is scaled evenly into the
  card. Its sequence is now fifteen steps, one table or row at a time:
  1. empty;
  2. Students;
  3. StudentID, with its orange key;
  4. Name;
  5. Courses;
  6. CourseID, with its orange key;
  7. Name;
  8. Enrollments;
  9. StudentID, with its green FK;
  10. CourseID, with its green FK;
  11. EnrollmentDate;
  12. the Students line;
  13. the Courses line;
  14. held;
  15. a fade.

  A key is marked in the same step as its own row, just after it.
- **SQL** still stands under its card until step 2.

How the change was made:

- A card that holds a demo keeps it with `StartRouteCard::set_live_demo`.
  The card's special cases are keyed on "holds a demo" rather than on being
  the Conceptual card, which leaves the Conceptual card's layout unchanged.
- A demo held in a card draws no floor (`HomeLiveDemo::set_floor`).

Fixed along the way:

- The Conceptual card's accessible description had been set to empty, and
  the accessibility audit was failing on it. It is restored.
- The card reference pictures drew stand-alone cards without their demos.
  They now draw them holding their demos.

The Home picture and the four card pictures were retaken.

**SQL moves into its card (step 2).** The SQL demo is now held inside its
card, so all three are. The row under the cards had nothing left in it and
was removed (`DemoLine` is gone). The clock now follows the row of cards,
which is hidden and shown with Home, so it still stops when Home is left.

- **The script.** It uses Zain's words: `VARCHAR(100)` names, and an
  Enrollments table of `StudentID INT`, `CourseID INT` and
  `EnrollmentDate DATE`, with no REFERENCES clauses. It is set compactly
  so all ten lines fit in the editor at a readable size: two-space indents,
  and each table closed on its last column's line. Zain's draft had
  four-space indents, `);` on lines of its own and blank lines between
  tables, which is fifteen lines. That would not fit a card at a size that
  can be read.
- **The sequence.** It has eleven steps, about 9.5 s a pass:
  1. empty;
  2. the editor;
  3. `CREATE TABLE Students (`;
  4. its columns;
  5. `CREATE TABLE Courses (`;
  6. its columns;
  7. `CREATE TABLE Enrollments (`;
  8. its three columns;
  9. "✓ 3 tables created";
  10. held;
  11. a fade.

  The typing speed is still 40 characters a second.
- **The editor** is 228 wide and centred in the box. Its code is set at 10
  units on 13-unit lines, up from 9, because the card scales a demo to
  about 83% of its authored size.
- **The cards** are unchanged: 269 × 334, with + Create on one line.

Only the Home picture was retaken.

**Still pictures, with the motion kept behind a switch (Zain, 2026-09-25).**
Zain asked for all three demos to stand still: each shows its finished
scene as a solid picture, with the same example as before. The motion code
was kept so it can be switched back on for one, two or all of the demos
when he asks. Step 3, timing and polish, was set aside with the motion.

- **The switch** is the `played` list in `HomePage::build_centre`. It has one
  line per card, giving the demo's kind, its delay and `moves`. All three
  are `false`.
- **The clock** takes a per-demo flag (`HomeDemoClock::add(demo, delay,
  plays)`, `set_playing`). A demo that does not play always shows its scene
  finished. With none playing, the clock never ticks.
- **When a demo is switched on**, the earlier rules apply to it: reduced
  motion through §9.9, pausing while Home is hidden, and starting again from
  the beginning when Home returns.

**The third card is titled *SQL Script (DDL)*.** Zain asked for a name that
fits a product converting between the levels in both directions, since
"SQL Project" did not. *SQL Script (DDL)* was proposed to him to try. It
matches *Conceptual Design (ERD)*: the level, and what it is written in. The
Home picture was retaken.

**Each demo on a raised screen; the first card shows the canvas itself
(Zain, 2026-09-25).**

- **The screen.** Inside its card, every demo is now shown on a small screen
  that stands out of the card. It is drawn in the style of the Home screen's
  floating hero panels:
  - a pale face lit from above, with a catch of light along its top edge;
  - a soft shadow under it;
  - four pixels of its own edge showing beneath, which gives it depth;
  - a slim header naming what it shows ("Conceptual ERD", "Relational
    Design", "university.sql") and three small dots.

  What it shows sits in a well on the workspace's own ground: the canvas
  colour for a model, the page for a script. A scene is fitted into the well
  by its own extent (`demo_scene_bounds`), so it fills the screen. For SQL
  the screen is the editor: its own frame is left out (`framed`), so there
  is no box inside a box.
- **Card 1 is the real canvas.** Zain asked for the Conceptual card to show
  the real panel, made small, so a reader sees what the workspace looks like.
  `home_demo_canvas` builds a small example with the Editor's own commands:
  Student and Course (148 × 86), Enrolled (190 × 110, many to many), ID as a
  key and Name on each (150 × 60), and Enrollment Date on the relationship.
  Enrollment Date is widened to 210 to hold its name, as a person would
  widen it; at 150 the canvas shortens it. The example is drawn by the
  Conceptual workspace's own `DiagramView` into a `QPicture`, once for each
  theme. The recording replays crisply at the size of the card, and is shown
  while the demo stands still. Switched to move, the demo shows its painted
  scene, since that is what can move.
- **The cost.** At the real sizes the lettering is small inside a card,
  about 7 px at the reference size. That is what the real panel looks like
  made small.
- The Home picture and the four card pictures were retaken.

### 9.22 Examples, Templates and Import belong to their workspace, 2026-10-03

Zain moved Examples, Templates and Import off Home's sidebar. They are not
global: each works on one design level, so each is offered in the workspace
it belongs to (2026-10-03), and in every menu that offers it (2026-10-04).

- **The sidebar** is Home, Open Project, Recent · (space) · Settings, Help.
  Its Import row only opened the Open dialog, which Open Project still does.
- **Conceptual Design** offers every existing example and the template, since
  all of them are Conceptual diagrams: in the File menu, which is also the
  ribbon's File tab (*Open example*, *Company Database*, *University
  Database*, and *New from template*, added there), in the Home menu of the
  menu bar, and through the header's *Open example*. Import is File ›
  Import and the ribbon's Import tab, unchanged: an ERDFlow project, a
  picture carrying one, and *From another tool…*, unavailable with its
  reason.
- **Relational Design** is offered none of those while it is in front -- a
  schema drawn by hand, or a diagram's schema given the whole window. Its
  Import keeps only *From another tool…*, since what it would read is what
  other tools write about tables. It has no examples or templates of its
  own yet; they are the next piece of work.
- Nothing about what an example, the template or an import makes changed,
  and no shared code was copied: the same entries are shown or put away as
  the workspace in front changes (`MainWindow::set_workspace_in_front`).
- The Home and sidebar reference pictures still pass and were not retaken.
