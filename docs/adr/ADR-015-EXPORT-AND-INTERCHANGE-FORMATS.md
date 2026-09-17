# ADR-015 — Export and Interchange Formats

**Status:** Accepted  
**Date:** 2026-09-17

## Context

ERDFlow must produce files that leave it. Two different needs were being
treated as one:

- a **picture**, which any system can open and no system can edit back into a
  model,
- a **model**, which only a documented format carries.

They do not have the same answer, and the second is constrained by a fact
about the notation. Chen conceptual notation has no interchange format. Every
portable format in the ecosystem — Mermaid ER, DBML, PlantUML, SQL DDL — is
relational, and none of them can express attribute ovals, ISA triangles,
composite, multivalued or derived attributes, or associative entities. A
conceptual diagram exported to any of them silently loses the notation
ERDFlow exists to draw.

## Decision

Export is tiered, and interchange begins at the relational level.

- **Pictures.** SVG is the picture to prefer and PNG the default raster,
  with JPEG, WebP, TIFF and PDF beside them, plus a multi-page PDF of a whole
  project and copy-as-picture to the clipboard. JPEG carries a warning where
  it is offered, because it smears the edges of text and lines.
- **The picture that is also a project.** SVG carries the whole `.erdx` in a
  `metadata` element and PNG in a `tEXt` chunk, both of which every other
  reader of those formats ignores. One file is then both the picture a
  recipient can open and the project ERDFlow reopens without loss. The
  payload is the same bytes `.erdx` holds at the same version, so one reader
  serves both. Stripping it must leave a valid picture, and a file ERDFlow
  did not write is still accepted as a picture. The lossy and niche formats
  carry no payload.
- **Models.** `.erdx` remains the lossless native format, and a JSON Schema
  for it is published so a third party can write a reader without reading
  ERDFlow's source.
- **Relational interchange.** `.sql`, Mermaid ER and DBML exist only once the
  Relational Schema workspace does, are one-way and lossy, and say so where
  they are offered. No conceptual-level text export is added.
- **Listings.** An HTML report, a Markdown data dictionary and CSV of
  entities and attributes read the model that already exists. They are
  listings, not interchange; nothing re-imports them.

Options decide whether an exported picture is usable, so resolution or DPI,
extent (whole diagram, selection or current view), background (transparent,
theme colour or white) and margins ship with the picture tier rather than
after it.

EPS, EMF and DOCX/PPTX embedding are named as deferred so their absence reads
as a decision. EMF in particular pastes into Office on Windows as editable
vector, but Qt's support is weak elsewhere, making it platform-specific or
nothing.

## Portability rules

An exported file carries nothing tying it to the machine that wrote it: UTF-8
with no byte-order mark, LF endings in text formats, no absolute paths or
machine or user names, images embedded rather than referenced, one file rather
than a directory bundle, and a stable key order so successive saves differ
only where the project differs and the format reads usefully in version
control.

Fonts are the exception SVG cannot carry cheaply. Text names a generic family
with a documented fallback chain, and an outline-text option trades selectable
text for identical rendering where a handoff must be pixel-exact.

## Consequences

The embedded payload is bounded by the same 8 MiB limit the project format
applies. A project holding a background picture and placed pictures can reach
it, so an export over the limit writes the picture without the payload and
says so plainly, rather than failing or writing a file that cannot be
reopened.

Export splits into two halves with different prerequisites. The picture half
needs nothing but a canvas that draws and may be pulled forward the way the
Phase 15 project format already was. The schema half cannot precede Phase 24,
because there is no schema to export until then.

ERDFlow does not claim to re-import what it did not write.

## What it is called

Export, and later Import. Not Download and Upload: those belong to a program
whose files live somewhere else, and ERDFlow's live on the reader's own disk.
Every tool this one sits beside — ERwin, ER/Studio, Workbench, DataGrip — says
Import and Export, and so does every document in this repository, so the
interface says it too.

## Final principle

> Give away a picture anyone can open, keep the model in a format that is
> documented, and never offer an export that quietly drops the notation the
> product exists to draw.
