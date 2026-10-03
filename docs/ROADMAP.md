# ERDFlow Remaining Roadmap

Source of truth for unfinished product work.

Always continue from the first applicable item that is not DONE.

Do not skip ahead to SQL before Conceptual and Schema work is complete.

Optimization is intentionally last.

---

## Conceptual first

### 1. Chen / Min–max entity-end label positioning

Status: PENDING / VERIFY IMPLEMENTATION

Make sure Chen and Min–max cardinality/ordinality labels are positioned close to the entity end of the relationship connector.

Requirements already defined:

- label follows the final connector segment touching the entity;
- label stays close to the entity;
- label stays slightly outside/above the connector;
- label must not cover Total-participation double lines;
- recursive relationships must position each label near its corresponding entity end;
- Crow’s Foot must remain unchanged;
- Bachman must remain unchanged.

If already implemented, verify it and mark DONE.

Do not redesign cardinality.

---

### 2. Conceptual Min–max display mode

Status: PENDING

The underlying cardinality and ordinality system already exists.

Do NOT redesign it.

Add only a presentation choice for Conceptual Min–max notation:

A. Show cardinality only

or

B. Show cardinality + ordinality together

This affects display only.

It must not change the relationship model.

---

### 3. Rename Regular Entity to Strong Entity

Status: PENDING

Change the user-facing Conceptual terminology:

```
Regular
→
Strong
```

Final entity terminology:

```
Strong
Weak
```

Do not change entity semantics.

Do not redesign Strong/Weak behavior.

This is a terminology correction only.

---

### 4. Conceptual free-text object

Status: PENDING

Add a lightweight borderless text object that can be placed freely on the Conceptual canvas.

This is separate from:

- card notes;
- review comments;
- other existing note types.

The purpose is simple explanatory text directly on the diagram.

---

### 5. Conceptual composite-key ordering

Status: PENDING

Review and finish explicit semantic ordering for Conceptual composite identifiers.

Generated Schema must not depend accidentally on AttributeId ordering when a Conceptual entity has a composite key.

The Conceptual model should have a deliberate and stable composite-key order.

---

### 6. Conceptual relationship-owned key issue

Status: PENDING / KNOWN ISSUE

Resolve the existing:

```
drawn_keys_are_the_primary_key
```

failure correctly.

Do not change the expected baseline simply to make the test pass.

Determine the correct domain rule for relationship-owned attributes and entity identifiers and fix the actual issue.

---

### 7. Conceptual Check Model

Status: PENDING

Provide validation/checking for Conceptual diagrams.

The checker should identify actual modeling problems without modifying the model automatically.

Exact validation rules can be designed when this task begins.

---

### 8. Conceptual relationship naming quality

Status: PENDING

Improve relationship naming where ERDFlow creates or converts relationships.

Avoid meaningless generic names where the source model provides enough information for a better name.

Do not solve Schema naming separately before the Conceptual side is understood.

---

### 9. Remaining Conceptual UI / notation polish

Status: PENDING

After the functional Conceptual tasks above are complete, perform the remaining small Conceptual visual/information cleanup.

This is for genuine remaining polish only.

Do not use this item to redesign completed features.

---

## Schema / Relational Design second

### 10. Schema column reordering interaction

Status: PENDING

Task 4A is already complete.

Task 4B is already complete.

Do NOT redo them.

What remains is the actual user-facing interaction for changing column display order inside a Schema table.

Example:

```
ID
Name
Email
```

must be movable to:

```
ID
Email
Name
```

without moving the whole table.

The existing semantic PK/FK order must remain safe.

---

### 11. Hand-drawn composite foreign keys

Status: PENDING

Extend manual Schema connection support so a legitimate composite foreign key can be created safely.

Reuse existing Stage 5 safety rules.

Do not fake a composite FK as unrelated individual relationships.

---

### 12. Schema Check Model

Status: PENDING

Provide Schema validation/checking.

It should identify modeling/design errors without silently changing the Schema.

Exact rules should be defined when this task begins.

---

### 13. Schema comments / free text

Status: PENDING

Decide and implement the appropriate Schema-side equivalent of diagram comments/free text where needed.

Reuse shared infrastructure where appropriate.

Do not redesign Conceptual notes.

---

### 14. Discriminator column rename/edit support

Status: PENDING

Finish safe user-facing handling of generated discriminator columns.

Renaming/editing must preserve generated identity and conversion behavior.

---

### 15. Schema relationship/generated naming quality

Status: PENDING

Improve generated Schema naming where conversion currently creates awkward names.

Examples previously observed include output similar to:

```
Has
Qualified Ins
Works Ons
Qualified InID
```

Preserve stable IDs.

Do not change conversion semantics just to improve text.

---

### 16. Schema-first to Conceptual workflow

Status: PENDING / PRODUCT DECISION

Define and implement the supported workflow for a Schema-first project that later wants a Conceptual model.

This must respect the existing identity separation:

```
EntityId != TableId
AttributeId != ColumnId
```

Do not collapse Conceptual and Schema identities.

---

### 17. Schema / Relational Design ribbon

Status: PRODUCT DECISION

Decide whether Schema/Relational Design should use a ribbon structure comparable to Conceptual:

```
File
Home
Insert
Design
Export
Import
View
Help
```

Do not implement until the decision is made.

---

### 18. Conceptual + Schema Templates foundation

Status: PENDING

Examples are already implemented.

Do NOT confuse Examples with Templates.

Create the actual reusable Template system after the core Conceptual and Schema editing behavior above is complete.

The Template system should eventually support:

- built-in templates;
- Conceptual templates;
- Schema templates;
- creating a new project from a template.

---

### 19. User-created templates

Status: PENDING

After the Template foundation exists, support:

```
Save as Template
```

and reusable user-created templates.

---

### 20. Template gallery and thumbnails

Status: PENDING

Provide the UI for browsing templates.

This can include:

- gallery;
- categories if needed;
- thumbnails/previews;
- choosing a template when creating a project.

Do this after the Template model/foundation exists.

---

### 21. Floating controller — Explorer / Properties / Both

Status: PENDING / VERIFY IMPLEMENTATION

Verify the previously specified floating-controller addition.

The bottom of the existing floating controller should contain:

```
Explorer
Properties
Both Panels
```

Explorer:
toggle left Explorer.

Properties:
toggle right Properties.

Both:
if both visible → hide both.
if either hidden → show both.

This is UI-only state.

It must not:

- dirty the project;
- create Undo;
- change selection;
- modify diagram/model data.

If already implemented and verified, mark DONE.

---

## Only after Conceptual + Schema: SQL

IMPORTANT:

DO NOT START ANY SQL TASK UNTIL ITEMS 1–21 THAT ARE REQUIRED FOR THE CORE CONCEPTUAL/SCHEMA WORK HAVE BEEN COMPLETED OR DELIBERATELY DEFERRED.

---

### 22. Physical Schema readiness for SQL generation

Status: PENDING

Before generating SQL, determine what information the Physical Schema still needs for accurate DDL.

Do not generate incomplete SQL and pretend it is complete.

---

### 23. Database dialect strategy

Status: PENDING

Choose the initial SQL dialect and define how future dialects will be supported.

Do this before relying on dialect-specific SQL generation.

---

### 24. SQL DDL generation

Status: PENDING

Generate SQL DDL from the trusted Schema / Physical Schema model.

Do not generate SQL directly from visual objects.

---

### 25. SQL preview

Status: PENDING

Provide a user-visible SQL preview generated from the current Schema.

---

### 26. SQL file export

Status: PENDING

Support exporting generated SQL as a .sql file.

---

### 27. Physical SQL constraints

Status: PENDING

Add/model the physical features necessary for accurate SQL where still missing.

This may include, where required:

- DEFAULT;
- CHECK;
- explicit constraint names;
- indexes;
- ON DELETE;
- ON UPDATE;
- computed/generated columns.

Implement only based on the actual physical-model requirements determined earlier.

---

### 28. SQL import / parser

Status: PENDING

Allow SQL DDL to be parsed into ERDFlow's Schema model.

Do not implement this as visual-shape parsing.

Import into the actual domain model.

---

### 29. SQL round-trip / reconciliation strategy

Status: PENDING

Define how ERDFlow handles:

```
Schema
→ SQL
→ changed SQL
→ Schema
```

including conflicts and model synchronization.

Do this only after generation and import both exist.

---

## After SQL: export / import

### 30. Mermaid ER export

Status: PENDING

Export ERDFlow model information to Mermaid ER format.

---

### 31. DBML export

Status: PENDING

Export ERDFlow model information to DBML.

---

### 32. Additional model import/export formats

Status: PENDING

Add other formats only when there is a clear product need.

Do not invent formats merely to expand the feature list.

---

## General product work

### 33. Home Examples / Templates navigation behavior

Status: PRODUCT DECISION

Finalize how Home presents:

```
Examples
Templates
Recent projects
New project flows
```

without changing the completed examples themselves.

---

### 34. Data Workspace

Status: PENDING / FUTURE

Implement the planned Data Workspace after the modeling and SQL foundations are stable.

Purpose includes working with table data/sample data through the modeled Schema.

---

### 35. Multiple pages / larger project organization

Status: PENDING / FUTURE

Support larger projects with multiple diagram pages/workspaces if still required by the product architecture.

---

### 36. Autosave

Status: PENDING

Add safe autosave behavior.

It must not compromise explicit project save/version behavior.

---

### 37. Crash/session recovery

Status: PENDING

Allow recovery of unsaved work after an abnormal application exit where feasible.

Do this after autosave/persistence behavior is stable.

---

### 38. Remaining icons and visual polish

Status: PENDING

Perform final application-wide icon and visual consistency work.

Do not use this stage to change established UX behavior.

---

### 39. Accessibility and usability polish

Status: PENDING

Review keyboard access, readable states, contrast, focus behavior, and other usability/accessibility details after the core workflows stabilize.

---

### 40. Visual snapshot baseline update

Status: DEFERRED

Do NOT retake visual baselines while intentional UI/rendering changes are still ongoing.

Only update expected snapshots after the related visual behavior is considered stable and reviewed.

---

### 41. Clean git landing / commit strategy

Status: PENDING

Once the large uncommitted implementation state is stable:

- review the working tree;
- separate intentional changes from noise;
- organize commits safely;
- do not lose any completed work.

Do not reset the repository blindly.

---

## Quality / evaluation

### 42. Rating / evaluation system

Status: PENDING — LATE STAGE

Build the evaluation/rating system before optimization.

Its purpose is to measure where ERDFlow actually needs improvement.

Possible measurements may include:

- responsiveness;
- rendering performance;
- large-diagram performance;
- conversion correctness;
- test performance;
- workflow quality/usability.

Exact metrics should be defined when this stage begins.

---

## Final item

### 43. Optimization

Status: DEFERRED — LAST

Optimization is intentionally the final roadmap item.

Do NOT optimize the system before the rating/evaluation system exists.

Use actual measurements from Item 42 to determine:

- what is slow;
- what is expensive;
- what is unnecessary;
- what should be optimized.

Do not perform speculative optimization.

---

## Permanent roadmap rule

Keep all 43 entries in:

```
docs/ROADMAP.md
```

forever unless we explicitly decide to change the roadmap.

When a task finishes:

```
PENDING
→
DONE
```

Do not delete it.

Do not renumber later items simply because an earlier item is complete.

A future session must be able to say:

"Open docs/ROADMAP.md and continue from the first unfinished item."

and know exactly where ERDFlow development should continue.
