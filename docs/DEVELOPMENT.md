# Developing ERDFlow

## Build and run

Requirements: a C++20 compiler, CMake 3.21 or newer, and Qt 6.9 or newer with
Core/Gui/Widgets and Svg development files. The UUID adapter uses Qt's UUIDv7 generator.
No Boost or separate UUID library is required.

From the repository root:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build --parallel
ctest --test-dir build --output-on-failure
./build/erdflow
./build/erdflow --example
./build/erdflow /path/to/project.erdx
```

If CMake cannot locate Qt, set `CMAKE_PREFIX_PATH` to your Qt installation when
configuring. For a Homebrew Qt installation this is commonly:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=/opt/homebrew/opt/qt
```

Release and diagnostics builds remain separate:

```sh
cmake -S . -B build/release -DCMAKE_BUILD_TYPE=Release
cmake --build build/release --parallel
ctest --test-dir build/release --output-on-failure

cmake -S . -B build/sanitized -DCMAKE_BUILD_TYPE=Debug -DERDFLOW_SANITIZERS=ON
cmake --build build/sanitized --parallel
ctest --test-dir build/sanitized --output-on-failure
```

The sanitizer option enables AddressSanitizer and UndefinedBehaviorSanitizer
with Clang/GCC. It does not instrument the prebuilt Qt frameworks. It is not
currently enabled for MSVC. LeakSanitizer support depends on the host platform.

Tests select `QT_QPA_PLATFORM=offscreen` for Qt UI executables. To inspect a
rendered example without starting a lasting window:

```sh
QT_QPA_PLATFORM=offscreen ./build/erdflow --example --screenshot /tmp/erdflow.png --smoke-test
QT_QPA_PLATFORM=offscreen ./build/erdflow --example --tab tabExport --screenshot /tmp/erdflow.png --smoke-test
QT_QPA_PLATFORM=offscreen ./build/release/editor_benchmark
```

`--search` opens the search bar already looking for something, which is the
quickest way to see what a search does to a diagram:

```sh
./build/erdflow --example --search Student
```

The bar is not shown until it is asked for, so without `--search` — or Ctrl+F,
or **Edit → Search…** — it is not on screen at all. That is deliberate: it costs
the window no room until it is wanted.

`--mode` opens in `basic` or `convertible`, which is the quickest way to see
what Convertible mode asks a model:

```sh
./build/erdflow --example --mode convertible
```

`--tab` brings a ribbon row to the front before the screenshot is taken, so a
row other than Home can be looked at without a person clicking the tab first.
The tabs are named `tabHome`, `tabInsert`, `tabDesign`, `tabExport`,
`tabView` and `tabHelp`.

`--smoke-test` marks the document saved before it quits. Some of the options
above are real edits — opening in Convertible mode is one — and a window with
unsaved work asks whether to save it on the way out, which nobody is there to
answer.

The screenshot option is development tooling: it grabs the window, chrome and
all. **Export** is the real thing: it writes the diagram as a picture, or the
project as a report, a data dictionary or a spreadsheet, by itself. Relational
interchange — `.sql`, Mermaid ER, DBML — is still planned and cannot precede the
Relational Schema workspace. A missing optional Vulkan
headers message does not prevent this Widgets/QPainter implementation from
configuring and building.

## Platform expectations

The current verification host is macOS arm64, Apple Clang 21.0.0, CMake 4.3.4,
and Qt 6.11.2. This is the tested environment, not a claim that every newer or
older compiler has been verified.

Linux requires a C++20 compiler and a compatible Qt 6.9+ development installation;
older system Qt packages may not satisfy the minimum. The commands above apply.
A normal launch needs a desktop session; tests can run offscreen.

Windows is expected to use a C++20-capable Visual Studio 2022 toolchain and a
matching Qt MSVC kit. With a multi-configuration generator:

```powershell
cmake -S . -B build -DCMAKE_PREFIX_PATH=C:/Qt/6.x.y/msvc2022_64
cmake --build build --config Debug --parallel
ctest --test-dir build -C Debug --output-on-failure
.\build\Debug\erdflow.exe --example
```

Replace the example Qt path with the actual 6.9+ installation. Qt DLLs must be
available on PATH for development. Windows/Linux builds, installers, signing,
notarization, and release dependency/license review have not been performed.
The project itself is MIT licensed; see [LICENSE](../LICENSE). The bundled
Lucide outline icons are ISC licensed; see
[LICENSE-lucide.txt](../assets/icons-outline/LICENSE-lucide.txt).

## Structure and ownership

```text
main.cpp (composition root)
  ├── QtIdGenerator + ErdxProjectStore (Infrastructure)
  ├── Editor (Application) → model/validation (Domain)
  └── MainWindow + Ribbon + DiagramView + theme/icons (Presentation)
```

Themes carry the application palette and the diagram colours together, so the
chrome and the canvas cannot disagree. `icons.cpp` supports three icon modes,
all reached through `glyph_icon`, so nothing in the window names an image file
of its own:

- **Outline**, the default. Single-weight line art held in `assets/icons-outline`
  and embedded under `:/erdflow/icons-outline`. Each file paints in
  `currentColor`, which Qt's SVG renderer does not resolve, so `glyph_icon`
  substitutes the theme's ink before handing the bytes to `QSvgRenderer`. One
  set of files therefore serves all nineteen palettes. A tool that is checked
  sits on a chip of the theme's accent, so the same file is rendered a second
  time in `readable_on(accent)` and kept as the icon's `QIcon::On` state.
- **Modern**, the coloured artwork in `assets/icons` and `assets/icons-on-dark`,
  embedded under `:/erdflow/icons` and `:/erdflow/icons-on-dark`. It carries its
  own colour, so the set comes in two inks and the panel colour picks which.
  `tools/generate-icons.py` generates both.
- **Painted**, glyphs drawn from the active theme by `draw()` with no files at
  all. It is also the fallback whenever an artwork file cannot be read, so a
  missing asset never leaves a button blank.

The user chooses the mode under **View → Icons**, and the choice is remembered.
CMake embeds all three sets with `qt_add_resources` and links Qt Svg.

Nineteen of the outline files come from [Lucide](https://lucide.dev) v1.46.0,
which is ISC licensed; the licence text travels with them in
`assets/icons-outline/LICENSE-lucide.txt`. Lucide has no icons for the Chen
shapes, so `entity`, `attribute`, `relationship`, `isa` and `connect` are
ERDFlow's own, drawn on the same 24-unit grid at the same 2-unit stroke weight
so the set reads as one family.

`symbols.cpp` holds the character table the Insert tab's gallery offers, as
eight named groups of named characters; `symbol_picker.cpp` is the gallery
itself. Some of the people are joined sequences rather than single characters,
so a new one is worth measuring against the picker's cell before it is added:
a font that does not join them draws two glyphs, and the grid elides what does
not fit. `desktop_tests.cpp` measures the whole table for exactly that. Nothing
about the gallery reaches the model: a character is text, and it lands in a
field through the same commands as anything else typed there.

A placed symbol is resized through `Editor::resize_symbols`, which refuses
anything that is not a symbol and clamps the box to `domain::min_symbol_size`
and `max_symbol_size`. The canvas draws the grips in `NodeItem::paint_plain_note`
and works out the hauled box in `Impl::sized_box`; the drag is previewed on the
item and committed once on release, the way a move and a bend are. Number
fields are not places a character can land: `text_target()` refuses a spin box's
inner line edit, so a symbol picked while the Size field has the keyboard goes
on the diagram rather than being typed into a figure.

Where a character lands is decided by `MainWindow::text_target()`, which asks
this window for its focus widget rather than asking the application for the
global one, so the answer survives the gallery being the window the desktop
calls active. Only a visible `QLineEdit` or `QPlainTextEdit` counts. Anything
else, including no focus at all, sends the character to the canvas through
`place_symbol()`, which creates a note marked `plain`. A plain note is drawn
as its character alone, grown to fill the box it is given; `paint_plain_note`
and the plain branch of `paint_shape` are the two places that draw it, and
both centre it on the character's ink rather than on the line it sits in,
because a line is mostly space and different characters use different parts
of it. The remembered caret exists because committing
a name rebuilds the properties panel: the replacement field reads from its
beginning, so the caret is restored before the character is inserted. The
picker refuses focus on every character button and does not take activation,
so choosing a character never moves the caret.

Two things follow from that rule and are easy to break. The canvas keeps its
inline rename box and hides it rather than destroying it, so `text_target()`
must reject an invisible widget or characters vanish into a box nobody can
see. And hiding a widget that holds the keyboard makes Qt hand the keyboard to
the next widget in the tab order, which is a field in the properties panel, so
`commit_inline_edit` gives it back to the view; without that, a character
picked after renaming on the canvas silently edits a name instead.

A character added to the table must be one the interface font can draw; the
desktop tests measure every one of them rather than trusting it.

`erdflow_domain` and `erdflow_application` have no Qt dependency. The desktop
receives Application interfaces; only the composition root assembles concrete
Infrastructure adapters. Graphics items contain stable references and display
state. Persistent edits go through `Editor`, then the projections synchronize.

Application commands prepare compact deltas, validate the candidate, and either
commit the complete edit/history or roll it back. The Application history is
authoritative; QAction adapts it directly without a second QUndoStack. Text
fields commit on focus loss, and a drag commits once on release. A separate
layout map keeps geometry distinct from semantic objects.

The initial `Project` directly owns the conceptual stores. Separate conceptual
model/page wrappers and other product stages will be introduced when they have
real behavior; the broad Domain document is the target model, not the current
C++ header specification. See [ADR-014](adr/ADR-014-CONCEPTUAL-EDITOR-FOUNDATION.md)
and the [ERDX format](ERDX_FORMAT.md).

## Working agreement

Read the Product, Roadmap, relevant ADRs, and implementation status before a new
feature. Keep the user's five requirements explicit: sustainable, scalable,
secure, maintainable, and usable. Explain substantive changes with **WHAT** is
changing, **WHY** it is needed, and **HOW** it is verified. Build one reviewable
dependency at a time; record deliberate sequencing changes.

Use strong warnings and test behavior at its lowest useful layer. Verify the
actual UI for integration changes, measure performance-sensitive work, update
docs to reflect implemented limits, and keep commits focused. Do not mark a
roadmap phase complete because interfaces or screenshots exist. Current work
remains uncommitted until it is recorded in Git.
