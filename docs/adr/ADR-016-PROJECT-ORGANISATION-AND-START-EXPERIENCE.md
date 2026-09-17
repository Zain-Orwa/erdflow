# ADR-016 — Project Organisation and Start Experience

**Status:** Accepted  
**Date:** 2026-09-17

## Context

ERDFlow could open and save one project at a time and had no way to start from
anything but an empty canvas or the built-in example. Nothing in the plan
covered how a user finds earlier work, starts from something, or keeps several
database designs apart.

Two things are easily confused and are kept distinct here. **Pages** divide one
database design across several sheets sharing one conceptual model, inside a
single `.erdx`; they are ADR-005's concern and Phase 16's. **Projects** are
separate database designs in separate files. A folder holds projects; a project
holds pages.

## Decision

### A folder is a real folder

A folder is a real directory and a project is a real `.erdx` file inside it.
ERDFlow keeps no library database and owns no location. The filesystem is the
only record of where anything is, which means Finder and ERDFlow cannot
disagree, renaming a file outside ERDFlow simply works, and projects sync
through iCloud, Dropbox or git, travel on a USB stick, and survive ERDFlow
being uninstalled with no export step.

ERDFlow offers a home folder, *ERDFlow Projects* in the user's documents, and
points new projects at it without requiring it. A project may live there, in
folders the user makes, or loose under its own name anywhere.

Any index over projects added later is a cache derived from the folders,
rebuildable by reading them again, and never the record of what exists.

### A template is a project

A template is an ordinary `.erdx`. There is no template format, no placeholder
syntax and no second reader to keep correct. *New from template* copies the
file and leaves the copy untitled and unsaved, so a template cannot be
overwritten by the work started from it. Built-in templates are embedded in
the binary the way the icon sets are, so ERDFlow has no content it can be
started without; *Save as template* writes one into a templates folder inside
the home folder, and anything found there joins the gallery.

Two kinds ship: worked schemas to open and adapt, and genuinely empty starting
frames carrying a title block and a chosen notation. The existing university
example becomes the first worked schema rather than remaining a separate
feature.

### The start screen

ERDFlow opens on a start screen laid out the way document applications are: a
collapsible band of template cards across the top, Blank first and always,
each card carrying a thumbnail of the diagram it contains and a two-line
caption of name and notation; below it the user's projects, newest first and
grouped under date headings, with sort, folder filter, grid or list, search
over names, and Browse.

The screen is skippable, and a file passed on the command line or `--example`
bypasses it.

Thumbnails are rendered into a cache keyed by file path and modification time,
so a folder draws quickly after its first visit and a changed project is
redrawn. Deleting the cache costs a redraw and nothing else. Storing a
thumbnail inside the `.erdx` would let it travel with the file, but that is a
format change and is not worth making before the cache has proved too slow.

## Constraint from intended identity

ERDFlow will have accounts, several people working in one project, and several
AI agents working across accounts and projects. None of that is built, and
none of it may be designed away.

The start screen therefore shows no owner column in V1 because there are no
accounts yet, not because projects have no owners. The project list, the start
screen and the project file are each designed to gain an owner and a
collaborator without their structure changing. Searching project *names* is a
directory listing and is free; searching project *contents* is the derived
index above, never a second source of truth.

## Consequences

Because folders are real, a project can be moved or deleted outside ERDFlow. A
recent entry whose file has gone says so and offers to forget it, rather than
failing when clicked. This is ordinary rather than an error, and is the price
of having no second record.

This work has no prerequisite beyond the Phase 15 project format and belongs
to the Phase 17 conceptual editor milestone: a tool a user cannot find their
work in is not public-quality.

## Final principle

> The filesystem is the truth about where projects are, a template is just a
> project, and nothing decided here may make accounts, collaborators or agents
> a rewrite to add.
