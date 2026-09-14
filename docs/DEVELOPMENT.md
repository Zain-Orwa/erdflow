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
QT_QPA_PLATFORM=offscreen ./build/release/editor_benchmark
```

The screenshot option is development tooling, not diagram export. Ordinary
model export is still planned. A missing optional Vulkan headers message does
not prevent this Widgets/QPainter implementation from configuring and building.

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
The project itself is MIT licensed; see [LICENSE](../LICENSE).

## Structure and ownership

```text
main.cpp (composition root)
  ├── QtIdGenerator + ErdxProjectStore (Infrastructure)
  ├── Editor (Application) → model/validation (Domain)
  └── MainWindow + DiagramView + theme/icons (Presentation)
```

Themes carry the application palette and the diagram colours together, so the
chrome and the canvas cannot disagree. `icons.cpp` supports two icon modes:
glyphs painted from the active theme, and SVG artwork embedded as Qt resources
from `assets/icons` and `assets/icons-on-dark`. The SVG mode picks a light/dark
variant using the panel colour and falls back to a painted glyph if artwork is
unavailable. `tools/generate-icons.py` generates both SVG sets; CMake embeds them
and links Qt Svg. The user chooses the mode under **View → Icons**.

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
