# ERDFlow 0.1.0

The first public release of ERDFlow, a desktop editor for designing databases:
draw the conceptual model as an entity-relationship diagram, and work out its
relational schema from it, or draw the schema directly.

## What it does

- **Conceptual diagrams.** Entities, relationships, attributes,
  generalisation and specialisation, drawn in Crow's Foot or one of three other
  notations. Lines can be drawn straight, curved or at right angles.
- **Relational design.** A schema worked out from the diagram, or a project
  that starts from its schema and is drawn table by table. Tables, columns,
  keys and constraints are edited in place, and the lines between tables run
  from row to row.
- **Find your way.** Search and filter both workspaces by kind, with the
  Explorer and Properties panels following the selection. Alignment guides
  appear while a table is carried.
- **Illustrate.** Text, symbols, pictures and Dr.icons (seventeen
  illustrations) can be put on a diagram or a schema.
- **Export.** Pictures in PNG, SVG, PDF and other formats, with the project
  carried inside SVG and PNG, and documentation listings of the model.
- **Projects are files.** Each project is one `.erdx` file, saved wherever you
  choose.

Not in this release: generating SQL, the Data workspace, and multi-page
diagrams.

## Downloads

| System | File | Requires |
| --- | --- | --- |
| Windows, 64-bit | `ERDFlow-0.1.0-windows-x86_64-setup.exe` | Windows 10 (version 1903 or later) or Windows 11. Installs for the current user; no administrator rights needed. |
| macOS, Apple silicon | `ERDFlow-0.1.0-macos-arm64.dmg` | macOS 13 or later. Open the disk image and drag ERDFlow to Applications. |
| macOS, Intel | `ERDFlow-0.1.0-macos-x86_64.dmg` | macOS 13 or later. |
| Linux, 64-bit | `ERDFlow-0.1.0-linux-x86_64.AppImage` | A distribution with glibc 2.39 or later, such as Ubuntu 24.04, Debian 13 or Fedora 40. Make it executable and run it. On Wayland it runs through XWayland. |

To check a download, compare its SHA-256 with `SHA256SUMS`:

    shasum -a 256 -c SHA256SUMS --ignore-missing

## Licences

ERDFlow is free software under the MIT licence. It uses Qt 6.11.2 under the
GNU LGPL version 3; the Qt source code, and the source of the other
components whose licences call for it, are published with this release (see
`SOURCES.md`). Help → Licences in ERDFlow shows where each package keeps its
licence texts and notices.

## Known limitations

- `.erdx` files are not yet associated with ERDFlow by the installers; open
  them from ERDFlow's own Open command.
- The interface is in English.
