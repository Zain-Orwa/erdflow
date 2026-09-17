# ADR-017 — Review Comments

**Status:** Accepted  
**Date:** 2026-09-17

## Context

ERDFlow has two different things that could both be called a comment, and
conflating them would damage both.

- A **description** documents the model. Every element already carries one. It
  says what a Student *is*, it is part of the model's meaning, and it travels
  forward into the Relational Schema and out into generated SQL as a column or
  table comment. Product section 22 describes this, and Convertible mode adds a
  structured `Comment` field beside the logical type for the same purpose.
- A **review remark** is about the work rather than part of it. "Should this be
  weak?", "check the cardinality with Dana". It is written while reading a
  diagram, it is addressed to a person, and it must never reach the schema.

There is also already a **Note**, which is neither: a card placed on the canvas,
part of the drawing, moved and coloured and exported like any other element.

Three things, easily confused, with different lifetimes and different
destinations.

## Decision

A **comment** in ERDFlow is the review remark, and it is a first-class object of
its own — not an element, not a field.

- It is **pinned rather than placed.** It has no position, no colour and no
  transparency, because it is not on the canvas; it belongs to what it is
  pinned to.
- It can be pinned to **several things at once**, so one remark covers a whole
  area of a diagram and is written once rather than repeated.
- It can be pinned to an **element**, to a **connector** — a remark about a
  cardinality belongs on the line rather than on either shape it joins — or to
  a **range of text** inside an element's name or description, which is how a
  remark comes to be about one word rather than a whole thing.
- It is **shown by pointing at what it is pinned to**, so it costs the diagram
  no room until it is wanted.
- It can be **put away without being deleted**, one at a time, and every remark
  at once can be switched off.

## Where the two levels of hiding live

They are different in kind, so they are stored differently.

- **One comment hidden** is a property of that comment. A remark that has been
  dealt with is put away and stays put away; it travels with the document and
  passes through the undo history like any other edit.
- **Every comment hidden** is a property of how the diagram is being looked at,
  like the grid. It is not saved with the document and does not enter the
  history, because it says nothing about the work — only that the reader wants
  a clean diagram or a clean picture just now.

## The mark always shows

A commented element carries a small mark whether or not remarks are being
shown. Hiding is meant to quiet the diagram, not to lose what a reviewer said:
a remark nobody can see is a remark nobody can find. The mark is solid when
pointing at it would say something and hollow when every remark there has been
put away or the whole lot switched off.

## A comment never outlives what it is pinned to

Deleting an element, or cutting a line, unpins every comment pinned to it, and
a comment left pinned to nothing is deleted with it — in the same edit, so one
undo brings back the element, the line and the remark about them together. A
comment pinned to nothing is refused, because it could never be found again.

Editing text that a comment is pinned into does not refuse the edit and does
not drop the remark. The range is held inside the text the field now has: a
range that ran past the new end is shortened to reach it. The remark stays on
the field it was left on, which is what its reader wants, even when the exact
words it pointed at have gone. Ranges are counted in characters rather than in
bytes or in UTF-16 units, so they mean the same thing in the file, in the
domain and in the interface, none of which index text the same way.

## No author yet

A comment records no author, because ERDFlow has no accounts to name one. This
is not a statement that comments have no author. Accounts, several people in one
project, and several AI agents are all intended, and a review remark is exactly
the thing that will need to say who left it. The project format is versioned and
additive, so an author is added when accounts are, without disturbing anything
written now.

## Consequences

An inheritance link cannot carry a comment. It is anchored to its triangle
rather than being a connector, and has no identity of its own to pin to; a
remark about one goes on the triangle instead.

A comment is not exported as part of the model. It appears in neither the
relational exports, which do not exist yet, nor the schema. It is a remark about
the work, and the work is what leaves.

## Final principle

> A description says what the model means and travels with it. A note is part of
> the drawing. A comment is what somebody said about the work, pinned to the
> work, and never confused for either.
