# ADR-021 — Relational Identity and Schema Entry Paths

> **Settled work.** Do not change, replace or re-style anything in this
> document to suit something new you have been asked to build. If what you are
> building genuinely contradicts what is written here, stop and ask Zain, who
> owns this project: say what you want to change, what the application will
> look like afterwards, and whether it is a gain or a loss. He decides.
> Full rule: [CLAUDE.md](../../CLAUDE.md).

**Status:** Accepted

**Amended:** 2026-10-01 -- §5b, how a column that is both keys is marked (see
the note there).

**Date:** 2026-09-22
**Project:** ERDFlow
**Decision Scope:** Relational object identity, provenance, schema entry paths, legacy migration

---

## 1. Context

A project may begin in three ways: by drawing a Conceptual ERD, by going
straight to the Relational Schema, or by importing an existing SQL definition.
All three must arrive at **one** schema workspace backed by **one** relational
model. There must not be a second schema engine for hand-made schemas.

The implementation before this decision could not support that. Every
schema-side structure was keyed by the `ElementRef` of the conceptual element
it was derived from:

```text
Entity E-1
    │
    └──── schema state effectively keyed by E-1
```

That works only while every relation is generated. A relation made by hand has
no conceptual element, and therefore no key.

It also deviated from ADR-008 §3, which already said:

> Do not reuse `EntityId` as `RelationId`, even when both objects have the same
> visible name.

So this decision does not introduce a new architecture. It restores one
accepted in August that the derived-only implementation had short-circuited.

---

## 2. Decision

**A relational object owns its identity. Origin records where it came from; it
does not define what it is.**

```text
Conceptual Entity            Relation
EntityId: E-1     ──────▶    RelationId: R-8
                  origin     Origin: E-1
```

```text
Identity ≠ Origin
```

`RelationId` answers *which relation is this*. `origin` answers *where did it
come from, if anywhere*.

### 2.1 The name is `RelationId`, not `TableId`

ERDFlow's levels are distinct and `Table` belongs to the one below:

```text
Conceptual → Relational Schema → Physical/Table Design → SQL → Data
Entity     → Relation          → Table
```

`TableId` is reserved for Phase 29. The user interface still says **Table**,
**Column**, **Type** and **Constraints**; internal type names and the words a
reader sees are allowed to differ.

> **Clarified by [ADR-022](ADR-022-FOUR-VISIBLE-WORKSPACES-AND-AZURE-THEME.md),
> 2026-09-23.** This reservation stands and its reasoning is unchanged:
> `Relation` and `Table` remain distinct internal concepts and the lineage
> `Entity → Relation → Table` is intact. What has changed is only that `Table`
> will surface *inside* the Relational Design workspace rather than in a
> visible workspace of its own.

### 2.2 Identity is derived, and derived from origin alone

A generated relation's identity is worked out from the element it came from by
a fixed mixing function, not drawn from the id generator. The same project
opened twice therefore yields the same relations, which is what lets layout,
typed names and added columns be kept against them.

**It depends on the origin alone, never on the conversion rule.** An identity
that moved when a decision was answered differently would orphan everything
kept against it the moment somebody chose another ISA strategy. The rule is
provenance; it is never identity.

### 2.3 Provenance is a record, not a bare identity

```text
origin? → { source: ElementRef, rule: ConversionRule }
```

ADR-008 §20–21 anticipated a mapping kind and a conversion-rule identity while
deferring the exact list. `ConversionRule` names only the rules the conversion
already implements — entity, many-to-many bridge, associative bridge,
multivalued, subtype — because reconciliation will need to explain *how* a
relation was produced, not only which element produced it.

---

## 3. Scope of this round (Step A)

Identity only. Behaviour is unchanged and must be proved unchanged.

Delivered:

- `RelationId` and `ForeignKeyId`, with `Provenance` and `ConversionRule`.
- Eight structures re-keyed off conceptual references: the schema layout's
  places, widths, heights and lines; the schema's added columns, key names and
  counting keys; and the conversion's typed table names.
- Validation asks the question forwards — what identities could this project
  produce — because a derived identity cannot be turned back into its origin.
- Format version 26, with older files migrated deterministically on load.

Deliberately **not** in this round: relations made by hand, schema-first
behaviour, SQL import, three-way reconciliation, and Schema → Conceptual
propagation.

---

## 4. Migration

A file written before version 26 keyed the schema's own state by the
conceptual element. On load, each such key is put through the same derivation
the conversion uses. The same old file therefore always produces the same
relations, which is what makes the migration testable rather than merely
plausible.

A line written before 26 named the link it was drawn for. That name cannot be
recovered from a key, so downgrading a document drops its lines; migration
forwards keeps them.

---

## 5. Consequences

**Good.** The identity model now matches ADR-008. `SchemaColumn` already had
its own identity, so only relations and foreign keys were borrowing, and both
now own one. Import and schema-first become producers of relations rather than
separate engines. The whole schema view, its layout, its editing commands and
its validation survived unchanged.

**Costs.** A format bump, and a derived identity cannot be inverted — so
validation and error reporting can no longer always name the conceptual
element behind a piece of schema state. Reports that used to point at an
element now point at nothing, which is the honest answer once a relation may
have no element at all.

**Deferred, and important.** The conversion still regenerates unconditionally,
exactly as before. Stored relations and regenerated ones cannot yet disagree,
because nothing is stored that the conversion does not produce. The moment a
hand edit can survive a regeneration, ADR-010 §12's rule applies — *detect,
explain, the user decides* — and that is Phase 25/26 work, not a side effect of
this one.

---

## 5a. Keys go both ways (Zain, 2026-09-24)

Settled when the conversion was changed to use the key an entity already has.
The rule is: **use the existing meaningful key first, and generate a fallback
only when no suitable key exists.**

- **Conceptual to relational (built).** An entity's key attributes are its
  relation's primary key; several make a composite key. No `EntityNameID` is
  generated beside them. A foreign key references that key, all of its
  columns. A fallback key is generated only for an entity with no key
  attribute, and the user is told in a notice.
- **One fact on both sides (built).** For a mapped attribute, being drawn as a
  key and being a member of the primary key are the same fact, changed together
  from either side. A key is never silently duplicated.
- **Relational to conceptual (not built; binding on Step B and schema-first).**
  When a Conceptual ERD is produced from relations, a relation's primary key
  becomes the entity's key attribute, and a composite key makes all of its
  mapped columns parts of the identifier. No second conceptual key such as
  `StudentID` is created beside an existing `ID`.
- **The exception.** A primary key that is purely technical, generated with no
  conceptual meaning, is not pushed into the Conceptual ERD unless the user
  chooses to. The conversion's own fallback keys are that kind of key, and
  already stay on the schema alone.

---

## 5b. Bridge keys: the conceptual identifier first (Zain, 2026-09-24)

This applies §5a to the table a many-to-many or associative relationship
becomes. It is built and accepted as it stands. It must not be redesigned, and
the default must not be turned back into the participant keys.

**The order a bridge's primary key is chosen in:**

1. **A conceptual identifier the user defined.** This is a key attribute drawn
   on the relationship, or a column added to the bridge on the schema and made
   its primary key. That key *is* the bridge's primary key. The participants'
   foreign keys stay ordinary foreign keys beside it and are not made part of
   it.
2. **Otherwise, a separate key of its own (the default).** The conversion
   generates `<Relationship>ID` (for example `EnrolledID`). It can be renamed,
   and hovering it explains it, like every other generated key.
3. **Otherwise, and only where the user chose it, the participant keys.** The
   participants' foreign keys together form one composite primary key:
   `StudentID + CourseID`. Where a participant's own key has several columns,
   all of them take part. This is never the default and never happens without
   an answer.

**Rules that follow from that order:**

- **A later identifier wins over an earlier answer.** If the participant keys
  were chosen and a key attribute is drawn on the relationship afterwards, the
  drawn key becomes the primary key. The earlier answer is set aside, not
  erased. It applies again if that identifier is taken off.
- **Nothing offers to replace a real identifier.** The participant-key question
  is shown only on a bridge with no identifier of its own. Where one exists,
  the question is not shown at all.
- **The question is answered where the table is.** On the schema, the bridge
  shows *"Enrolled has no defined identifier. Bridge primary key:"* with the
  answers *Separate key (default)* and *Use participant keys (composite PK)*.
  An answer is an ordinary edit (`Editor::set_bridge_key`, kept in
  `ConversionDecisions::bridge_key`). It can be undone and is saved with the
  project (format version 28). It is removed together with the relationship.
  A file from before version 28 opens with every bridge on the default.

**How an FK that is also a key looks.** A foreign-key row keeps its own
marker: `FK`, in green, in the key gutter. This holds when the column is part
of a composite primary key too. The gutter never shows a merged `PK FK`
marker. That the column belongs to the primary key is shown in its
Constraints cell (`PK, NOT NULL`). The orange `PK` marker and the golden key
glyph belong to key columns that are not foreign keys.

```text
PK visual  →  orange PK, golden key            (a key that is not a foreign key)
FK visual  →  green FK, always                  (PK membership shown in Constraints)
```

> **Amended 2026-10-01 (recorded 2026-10-02).** The rule above that the
> gutter never shows both marks is superseded for a column that is both a
> primary key and a foreign key. At Zain's request, such a column now carries
> both roles visibly: in the key gutter, the golden key and the orange `PK`,
> then the green `FK`, each in its own colour, side by side -- separate marks,
> not one merged badge -- and the Schema Explorer marks it `PK FK` with both
> icons. `FK` is still always green and `PK` orange, and a key column that is
> not a foreign key, or a foreign key that is not a key, is marked as above.
> The text above is kept as the rule that held from 2026-09-24 to 2026-10-01.
> See *A column that is both keys wears both marks* in
> `docs/IMPLEMENTATION_STATUS.md`.

**Unchanged by this: the lines.** A generated relationship line still runs from
the exact primary-key row it references to the exact foreign-key row. Automatic
routing picks whichever left or right sides give the cleanest route. A line
shaped by hand keeps its shape, and manual control over connectors stays.

**Open conflict, awaiting Zain (2026-09-24).** The conversion honours a key
attribute drawn on the relationship, as rule 1 says. But the model's existing
validation rule `attribute.key.relationship` refuses to let a relationship own
a key attribute ("A relationship-owned attribute cannot be an entity
identifier"). As a result, drawing a key on Enrolled, by kind, by the
Identifier tick or by the schema's primary-key toggle, is refused. Today a
bridge can only get a user-defined key through a column added on the schema
and made its primary key. The two rules cannot both stand as written, and
which one gives way is Zain's decision.

---

## 5c. Step B, and the way back to the diagram (Zain, 2026-09-27)

Built, with Zain's answers to the questions it raised. Recorded here rather
than folded into §3 and §5a, which say what was true when they were written.

- **A project can start from its schema.** `SchemaOverrides::standalone` marks
  it; its relations (`Relation`) and foreign keys (`SchemaForeignKey`) have
  identities issued by the generator and no origin. Such a project holds no
  conceptual elements. Format version 30. Home's *Relational Schema* card is
  enabled for it, which is the condition §6 set: relations with no origin;
  tables, columns, keys and foreign keys created and edited; layout and a
  save/load round trip; a project with no Conceptual ERD at all.
- **A foreign key is drawn by hand**, from a column's key gutter onto the
  primary-key row it points at, or from the column's menu. It points at a
  table's only primary key column (or a unique one), and takes that key's
  type, which follows the key from then on.
- **Converting to a Conceptual ERD hands the project to the diagram.** Zain
  chose this over an unlinked copy and over a separate new project: one
  model, convertible both ways. Afterwards the project is one begun as a
  diagram, its schema derived again, and edits on either side reach the
  other. §5a is the rule for keys in this direction, and holds: a relation's
  primary key becomes the entity's key attribute and nothing is invented
  beside it. A join table becomes a many-to-many relationship, and converts
  back to the same bridge. What the schema said is carried across so the
  derived schema reads the same; where the rules cannot say it exactly, the
  conversion says so in words.
- **A table drawn by hand names its key for itself.** It starts with an
  `int` primary key called after the table, `StudentID` rather than `ID`, the
  rule the derivation already used for a key it generates for an entity. A
  key still called by the table's old name follows a rename in the same edit;
  one named by hand keeps its name. Zain asked for this over the plain `ID`
  the first version gave every table.
- **A foreign key can be renamed on any schema** (format version 31). A typed
  name is kept and no longer follows its key, as a typed table name no longer
  follows its entity. Zain chose this over the old refusal, and it is what
  lets a converted schema keep names such as `ManagerID`.

Still not built: a table made by hand in a project begun as a diagram (a
relation with no origin beside derived ones), reconciliation when the two
disagree (Phase 25), SQL import, and SQL generation from the schema.

---

## 6. Relationship to other decisions

- **ADR-001** — distinct typed identity per level. This implements it for the
  relational level.
- **ADR-008** — cross-level mapping and provenance. This implements §3 and
  takes up §20–21's deferred provenance record.
- **ADR-010** — the two levels may differ, and a difference is recorded rather
  than refused. Unchanged, but it may no longer assume which level came first.
- **ADR-016** — project organisation and the start experience. The three entry
  doors become implementable once Step B lands.
- **ADR-022** — the Home screen's `Relational Design First` route stays visible
  but disabled until Step B is built and tested (ADR-022 §5). Step B is done
  when relations can exist with no origin; tables, columns, keys and foreign
  keys can be created and edited; layout and project persistence and a
  save/load round trip work; and a project works with no Conceptual ERD at
  all. Only then is the route enabled.
