# ADR-023 — Practice Data

> **Settled work.** Do not change, replace or re-style anything in this
> document to suit something new you have been asked to build. If what you are
> building genuinely contradicts what is written here, stop and ask Zain, who
> owns this project: say what you want to change, what the application will
> look like afterwards, and whether it is a gain or a loss. He decides.
> Full rule: [CLAUDE.md](../../CLAUDE.md).

**Status:** Accepted

**Date:** 2026-10-23
**Project:** ERDFlow
**Decision Scope:** The first form of the Data workspace: practice rows in the schema's tables

**Amends:** ADR-022 §3 (the visible order of the workspaces)

---

## 1. Context

A brief asked for a screen where a student, having designed a schema, puts
rows into its tables and sees what its keys and rules do to real data, without
writing SQL. It came with a picture of a web dashboard: a top bar reading
Home · Schema · Practice Data · Settings, a left sidebar, and a grid of table
cards.

Several parts of that brief met settled work: the in-window navigation is
File · Home · Settings; ADR-022 names the fourth workspace *Data* and puts it
after SQL; ADR-012 decides how a Data workspace reaches its rows. Zain was
asked, and decided the four questions below.

## 2. Decisions (Zain, 2026-10-23)

1. **Reached by converting, left by Show Schema.** *Convert All to Tables*
   joins the Model menu and the Design menu; *Show Schema* on the page goes
   back. The navigation is not changed: the mock-up's top bar and sidebar are
   not built, the native shell stays as it did for the Home screen (ADR-022),
   and the workspace switch keeps its two halves. (A third half, *Data*, was
   chosen first and taken out again on Zain's revised brief the same day: the
   header of a schema drawn by hand has no room for it at 1280 px.)
2. **Practice Data is the Data workspace**, built first, ahead of SQL. The
   page is titled *Practice Data*; where the header carries a badge, it reads
   *PRACTICE DATA* while the page is up.
3. **Constraints are enforced, and can be relaxed.** While *Constraints
   active* is on, a change that breaks a primary key, foreign key, UNIQUE,
   NOT NULL or the column's type is refused, and the card says which rule and
   why. Off, the change is kept and the cells that break a rule are marked.
4. **Rows are saved, and tables start with rows.** Practice rows are part of
   the project file (format 37). Converting gives each table three rows made
   from its columns' types and names, pointing at rows that exist; Reset puts
   them back, Clear Table empties the table.

## 3. The page

It rises over the whole stage as the schema rises over the diagram (280 ms,
easing out), and lowers again for Show Schema. From a diagram, the schema is
brought up to the whole window underneath it first, so going back lands on the
schema. The side panels are put away while it is up and come back as they
were.

A header (title, subtitle, *Show Schema*, and how many tables were converted
and whether they are ready), one bar of controls (*Search tables*,
*Constraints active*, grid or list), then the cards. Each card is as big as
its whole table -- every column and every row, nothing to scroll -- and they
stand side by side, the next beside the last wherever it fits and on a line
below where it does not (Zain, 2026-10-23); how many share a line depends on
the tables, not on a fixed count. A card is resized by any edge or corner,
and a card pulled smaller than its table scrolls inside. Tables are **not** grouped into kinds and
there is no row of categories; a table is found by its name.

A card is taken by its heading and put down exactly where the hand lets it go
(Zain, 2026-10-23, after first choosing reordering within the grid). Until a
card is moved, the cards stand in the grid above. The first move puts every
card where it stood, and from then on nothing is arranged for anybody: a gap
left behind stays a gap, a wider window moves nothing, and a table the schema
gains later is put below the rest. The places and sizes are kept in the file and
every move or resize is one step of history; undoing the first brings the
flowing layout back. This
follows the rule that a hand's placement is never undone.

## 4. What the schema is, and what the rows are

The schema stays the only description of the tables. Practice rows are kept
against the relation's own identity (`RelationId`) and each value against the
column's (`ColumnIdentity`), so a rename or a new column order keeps every
value, and nothing done to a row ever reaches the schema. The page is built
from `schema_preview` every time it is read: no table, column or key is
described twice.

NULL is kept apart from the empty string. An empty cell means NULL except in a
column of text, where it is the empty string; *Set to NULL* (and Ctrl+0) gives
NULL anywhere.

## 5. Relation to ADR-012

ADR-012 §11 and §46 describe exactly this: project-owned sample data whose
editing is an ordinary Application change, taking part in dirty state, save
and undo, and whose values survive a change of structure by stable
association (§86). Every practice change is one step of the Editor's history.

The DataSource port ADR-012 §2 puts between the workspace and its rows is
**not** built yet: with one source, the page reads the project and changes it
through the Editor. The port is introduced when a second source (imported or
live data) arrives.

## 6. What is deliberately not here

No SQL is written or shown. The schema's lines are not drawn. There is no
Close beside Show Schema: on this page it would do exactly what Show Schema
does. The schema is
never changed to suit the rows, and is put back on screen exactly as it was
by *Show Schema* or the switch.
