# ERDFlow — how this project is to be worked on

**Read this before changing anything.** It applies to every file in the
repository: the C++ sources and headers, the tests, the documents under
`docs/`, the ADRs, the CMake files and the assets. Every source file carries a
short banner at the top pointing here.

---

## The rule

**Nothing already built is to be changed, modified, replaced, re-styled or
removed in the course of building something new.**

Adding to the project is ordinary work and needs no permission. The rule bites
when a new request would *contradict* what is already there — when satisfying
it means altering behaviour that already exists rather than extending it.

### When a change is unavoidable, stop and ask

Do not make the change and report it afterwards. Come back to **Zain, who owns
this project**, before touching it, and say three things:

1. **What you want to change.** Name the existing behaviour it touches, and
   where it lives.
2. **How the result will look.** Not the code — the *output*. What will somebody
   using ERDFlow see differently afterwards? If it changes what is drawn, say
   what is drawn now and what would be drawn instead.
3. **Whether it is a gain or a loss,** said plainly, including what is given up.
   If you think it is an improvement, say why. If you are not sure, say that.

Then wait. Zain decides. An agent does not get to decide on his behalf that a
replacement is an improvement.

### What this is not

It is not a rule against fixing a defect. If something is plainly broken —
it crashes, it computes the wrong answer, a test catches it — fix it, and say
what was wrong. The rule is about *deliberate* behaviour being swapped for
different behaviour without being asked.

---

## Why

Most of ERDFlow is settled work, arrived at deliberately and usually over
several rounds of Zain correcting an earlier attempt. Sizes, wording, layout,
notation, defaults and interaction that look arbitrary in the source are almost
always the outcome of a conversation an agent cannot see.

Some of it was learned the hard way. The conceptual diagram's element sizes were
enlarged twice, on reasonable-looking evidence, before it turned out the shapes
had never been the problem — it was the lettering and the weight of the lines.
Two rounds of work were spent undoing a change nobody had asked for.

An agent that rewrites settled work to suit whatever it has just been asked to
add is destroying decisions whose reasons are not in front of it. Ask.

---

## Things already settled

These have been decided and are not open unless Zain reopens them.

- **The permanent in-window top navigation is File / Home / Settings only.**
  Settings unfolds Design (Background, Icons, Lines), View (Grid, Align Grid,
  On-canvas Controls), and direct Help. File's existing Open & Save, Export,
  and Import controls stay on its tool row. The non-native old menu row stays
  hidden; native menus and the shared actions/shortcuts remain intact.
- **Crow's Foot is the default notation.** Not Chen. Four notations are offered;
  Crow's Foot is what a diagram is drawn in before anybody chooses.
- **Shapes are made for their names** (Zain, 2026-10-12, replacing the
  2026-10-11 rule that the defaults were the least size; least width
  2026-10-13). The defaults -- entity 133×77, relationship 171×99, attribute
  135×54, hierarchy 96×74 -- are the proportions a shape is made in, and the
  figures themselves are not to be changed. A new or renamed entity,
  attribute, relationship, generalization or specialization is made the size
  its name needs, with the existing lettering, proportions and height
  fitting: never narrower than the rendered width of "Attributes" and no
  wider than the rendered width of "Relationship", each measured on one line
  with the shape's own font and text room, separately for each kind of
  shape. So a short name such as "ID" makes a shape as wide as "Attributes"
  needs, in proportion to its default; "Relationship" is always written whole
  on one line; and a longer name makes it taller, written in up to three
  lines. Beyond three lines the last ends with an ellipsis and the full name
  is offered on hover; existing remark tooltips remain available. These
  limits apply when a shape is placed and when a rename refits it. A size
  given by hand is kept and saved, a renamed shape resized by hand keeps its
  size, and a diagram opens at exactly the sizes it was saved with. The
  curated examples and the template keep their own layouts.
- **The conceptual canvas and the Relational Schema are sized separately.** The
  canvas has `lettering_scale` and `connector_scale`; the schema measures every
  table from what it holds. A change to one must not reach into the other.
- **Nothing on the schema is read-only because it was derived.** Every value
  shown there can be changed there, and the change reaches whatever conceptual
  fact produced it. Where something cannot apply, it is still pressable and says
  why — never greyed out.
- **A hand's placement is never undone.** Anything dragged stays exactly where it
  was let go, including where the model thinks it wrong. Invalidity is reported,
  not prevented.
- **The schema generates the SQL.** Constraints shown there are database facts,
  not decoration, and `NULL` stays distinct from zero and from the empty string.
- **Vocabulary is that of database tools.** Export and Import, never Download
  and Upload.

---

## Working notes

The running plan is kept **outside** this repository, at
`/Users/zain/Developer/ERDFlow/notes/PLAN.md`. Read it at the start of a
session and update it as work lands. What is actually built is recorded in
`docs/IMPLEMENTATION_STATUS.md`.

Read the numbered documents in `docs/` and the ADRs before changing structure,
so that existing behaviour is understood before it is touched.

Both test suites must pass in Debug **and** Release before work is reported as
done.
