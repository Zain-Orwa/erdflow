# ADR-018 — Search Narrows the Diagram

**Status:** Accepted  
**Date:** 2026-09-17

## Context

Phase 42 lists search as "entity search, attribute search, relation search".
That is the shape search takes in a list: a box, a set of hits, and a way to
walk from one to the next.

A diagram is not a list. On a drawing, the question is rarely "where is
Student?" — it is "show me Student and stop showing me everything else", or
"show me only the entities", or "what does this actually touch?". Walking from
hit to hit answers none of those, because the thing in the way is the rest of
the diagram.

## Decision

Search **narrows what the diagram shows**. It is a filter, not a cursor.

- **What to look for** is matched against names, without regard to case.
- **Which kind** — entities, attributes, relationships, hierarchies, or all of
  them. A kind chosen with nothing typed asks for every element of that kind,
  which is how "show me only the entities" is asked for.
- **What was found** keeps its full strength and wears a ring. Everything else
  either fades or goes, which is the reader's choice.
- **What was found is brought to the middle of the view**, because being found
  should bring the answer to the reader rather than leave them to hunt for it.

## Fading is the default, hiding is offered

Fading keeps the diagram's shape, so a match can be seen in its place rather
than floating alone, and no line is ever left hanging from a shape that has
gone. That is what a reader usually wants, so it is the default.

Hiding is offered beside it, because sometimes a clean view is wanted more than
the context — a picture to hand on, or a diagram too crowded to read through.
Its cost is stated where it is offered: a line goes with whichever of its ends
goes, so what is left can be less than what matched.

## "What it touches" is one step, not the whole graph

A match on its own is often not enough to understand: a relationship with its
participants missing says nothing. So the reader may ask for what a match
touches, and that means **one step**: what belongs to it, the relationships and
hierarchies it takes part in, and the far side of those.

One step rather than every reachable thing, because following the joins to
their end fetches most of a well-joined diagram and leaves the setting doing
nothing. A relationship or a hierarchy pulled in this way brings its own far
side with it, since either says nothing alone — that is the same rule, not an
extra step.

## It is a way of looking, not an edit

A search changes nothing in the model. It is not saved with the document, does
not pass through the undo history, and does not dirty the project — the same
rule the grid and the comment switch already follow. Closing the search puts
the whole diagram back, so a filter is never left on behind a bar nobody can
see.

## Consequences

The search bar lives above the canvas rather than in the toolbar or over the
Explorer: it is next to the thing it filters, and it costs the toolbar no room
when it is not in use.

Filtering is done by the projection, not by the model, so it costs nothing to
keep correct as the model changes: an edit reapplies the filter to whatever is
now on the diagram.

This is a pull-forward of Phase 42, which depends on nothing later.

## Final principle

> On a list, search finds one thing among many. On a diagram, the many are the
> problem: search says what to keep and what to let recede.
