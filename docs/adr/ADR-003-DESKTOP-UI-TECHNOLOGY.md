# ADR-003 — Desktop UI Technology

**Status:** Proposed  
**Date:** 2026-08-31  
**Project:** ERDFlow  
**Decision Scope:** Primary desktop UI framework and UI programming model

---

## 1. Context

ERDFlow is intended to be a professional, desktop-first visual database
design environment.

Its UI must eventually support:

```text
Main application shell
Ribbon / menu / toolbar
Explorer tree
Properties inspector
Conceptual ERD canvas
Relational Schema workspace
Physical/Table Design workspace
SQL workspace
Live Data Grid
Dialogs and import/review flows
Undo/redo
Keyboard shortcuts
Large desktop layouts
High-DPI displays
macOS
Linux
Windows
```

Later possibilities include:

```text
touch/stylus input
sketch recognition
VS Code extension
future Rust core
AI-assisted workflows
```

The desktop technology therefore needs to support a deep productivity
application rather than only a simple form or web-style dashboard.

---

## 2. Decision

ERDFlow will use:

> **Qt 6 with C++20 as the primary desktop UI technology.**

For the main desktop shell and normal productivity UI, ERDFlow will use:

> **Qt Widgets as the default UI layer.**

This includes likely use of mature desktop components such as:

```text
QMainWindow
QDockWidget
QTreeView
QTableView
QAbstractItemModel
QMenu
QToolBar
QStatusBar
QDialog
```

The exact Conceptual Canvas implementation is **not** locked by this ADR.

`QGraphicsScene` / `QGraphicsView` is a strong candidate for the first
canvas implementation, but that choice should be validated during the
Canvas phase rather than silently bundled into the general desktop decision.

Qt Quick / QML is not selected as the primary application UI technology.

It remains available for isolated future surfaces where its strengths are
clearly useful.

---

## 3. Why Qt 6

Qt 6 fits ERDFlow's core requirements unusually well:

- native desktop application model,
- mature C++ integration,
- macOS/Linux/Windows support,
- established desktop widgets,
- model/view architecture,
- table/tree controls,
- dockable desktop layouts,
- graphics capabilities,
- high-DPI support,
- accessibility infrastructure,
- internationalization support,
- CMake integration,
- long-term desktop ecosystem.

ERDFlow's existing architecture also already assumes C++20 for the desktop
application.

Qt therefore does not introduce a second primary implementation language
for the first product milestone.

---

## 4. Why Qt Widgets as the Default

Qt's own current UI guidance distinguishes the two major Qt UI technologies.

Qt Quick is optimized for fluid, dynamic interfaces and has stronger
touch/gesture ergonomics.

Qt Widgets provides a broad set of mature, traditional desktop controls and
is explicitly positioned for complex desktop applications.

That distinction maps well to ERDFlow.

ERDFlow's primary interface is expected to contain:

```text
dockable panels
property editors
tree views
table views
menus
toolbars
desktop dialogs
keyboard-heavy workflows
large information-dense workspaces
```

This is a strong match for Qt Widgets.

---

## 5. Product Fit

ERDFlow is closer in interaction style to:

```text
IDE
database modeling tool
CAD/design environment
desktop engineering application
```

than to:

```text
mobile app
consumer touch-first application
simple animated dashboard
```

The UI should prioritize:

- density,
- precision,
- keyboard/mouse efficiency,
- dockable workspace behavior,
- large-table interaction,
- predictable desktop conventions.

Qt Widgets aligns with those priorities.

---

## 6. Primary Desktop Shape

The intended main shell is conceptually:

```text
┌────────────────────────────────────────────────────────────┐
│ File  Home  Insert  Design  View  Convert  Export  Help  │
├────────────────────────────────────────────────────────────┤
│ Toolbar / Ribbon                                           │
├───────────────┬──────────────────────────┬─────────────────┤
│ Explorer      │                          │ Properties      │
│               │      Main Workspace      │                 │
│               │                          │                 │
├───────────────┴──────────────────────────┴─────────────────┤
│ Conceptual | Schema | Table Design | SQL | Data           │
├────────────────────────────────────────────────────────────┤
│ Status                                                   │
└────────────────────────────────────────────────────────────┘
```

Qt Widgets can express this naturally through desktop window, dock,
view, toolbar, and model/view classes.

---

## 7. Qt Widgets Does Not Become the Architecture

Choosing Qt Widgets does not change ADR-002.

The dependency direction remains:

```text
Presentation
     ↓
Application
     ↓
Domain
```

Qt is a Presentation technology.

It must not become the Domain.

---

## 8. Domain Independence

The following are allowed in Presentation:

```cpp
QString
QWidget
QMainWindow
QGraphicsItem
QTableView
QTreeView
QAction
QDockWidget
```

The Domain should instead use framework-independent values such as:

```cpp
std::string
std::vector
std::optional
std::chrono
```

plus ERDFlow-owned domain types.

Example:

```text
Qt EntityGraphicsItem
        ↓ references
EntityId
        ↓
Domain Entity
```

not:

```text
QGraphicsRectItem = Entity
```

---

## 9. Model/View Is a Major Advantage

ERDFlow will contain several data-heavy views:

```text
Explorer
Schema tables
Physical columns
Validation lists
Live Data Grid
Search results
```

Qt's model/view framework is useful because the view does not need to own
the underlying data.

Conceptually:

```text
ERDFlow Data / View Adapter
            ↓
QAbstractItemModel
            ↓
QTreeView / QTableView
```

This matches ERDFlow's architectural rule that presentation should not
become the source of truth.

---

## 10. Explorer Direction

The Explorer will likely use:

```text
QTreeView
      ↓
Qt model adapter
      ↓
Application/Domain read model
```

The Explorer should not maintain a duplicate independent database model.

---

## 11. Live Data Grid Direction

The Data Workspace will likely use:

```text
QTableView
      ↓
Qt Table Model Adapter
      ↓
Data Source abstraction
```

For local/smaller data, sorting/filtering may use Qt proxy models.

For future large/remote data, query behavior can be delegated through the
Data Source rather than loading everything into the view.

---

## 12. Properties Panel Direction

Properties may use normal desktop widgets such as:

```text
QLineEdit
QComboBox
QCheckBox
QSpinBox
QTextEdit
custom editors
```

but changes must still flow through Application commands.

Example:

```text
QLineEdit edited
      ↓
RenameEntityCommand
      ↓
Application
      ↓
Domain
```

---

## 13. Main Window Direction

`QMainWindow` is a strong fit for the initial shell because ERDFlow needs:

- central workspace,
- menus,
- toolbars,
- status bar,
- dockable side panels.

Likely first shell:

```text
QMainWindow
├── Explorer QDockWidget
├── Central Workspace
├── Properties QDockWidget
├── Toolbar/Menu
└── StatusBar
```

Exact widget composition remains an implementation detail.

---

## 14. Ribbon Decision

ERDFlow's product vision uses an Office-like ribbon/toolbar experience.

Qt does not require ERDFlow to adopt a third-party ribbon framework.

The first implementation should use standard Qt controls to build the
necessary command surface.

Do not introduce a third-party ribbon dependency until the actual shell
shows that it is necessary.

This avoids adding licensing and maintenance risk prematurely.

---

## 15. Canvas Technology Is Separate

This ADR chooses the overall desktop UI stack.

It does **not** permanently choose the rendering implementation of the ERD
canvas.

The canvas has specialized requirements:

```text
many custom 2D objects
connectors
selection
dragging
zoom
pan
hit testing
labels
large scenes
possible future stylus input
```

Qt Widgets includes the Graphics View framework, specifically designed for
managing and interacting with many custom 2D graphical items.

Therefore:

> `QGraphicsScene` + `QGraphicsView` is the leading initial canvas candidate.

But it should be benchmarked/prototyped before becoming a separate locked
decision.

---

## 16. Why Not Lock Graphics View Yet

The roadmap intentionally keeps the exact Canvas technology open.

Before locking it, the prototype should test:

```text
entity creation
attribute creation
relationship diamond
connectors
label placement
M:M notation
selection
dragging
zoom
pan
hundreds/thousands of items
```

If it meets ERDFlow's needs, we can record that decision separately.

If not, the Domain/Application architecture remains unaffected.

---

## 17. Qt Quick / QML

Qt Quick remains a valid Qt technology.

It provides strengths such as:

- fluid animation,
- declarative UI,
- touch/gesture-oriented interaction,
- custom visual effects,
- modern dynamic interfaces.

Those strengths may become valuable later for:

```text
touch/stylus surfaces
animated onboarding
specialized visual panels
presentation-style experiences
```

However, they do not outweigh the desktop-productivity advantages of
Widgets for ERDFlow's primary shell today.

---

## 18. Why Not QML as the Primary UI

Making QML the primary application layer would introduce:

```text
C++
+
QML
+
JavaScript-like QML expressions
```

as a major everyday development model from the beginning.

That is not inherently bad.

But ERDFlow's first goal is a dense native engineering tool.

Qt Widgets already supplies the controls and interaction model needed for
that goal with direct C++ integration.

Therefore the additional QML layer is not justified for the primary shell
at this stage.

---

## 19. Mixed Widgets + QML

ERDFlow may later embed isolated Qt Quick/QML surfaces if a measured product
need justifies them.

Examples:

```text
stylus sketch input
special animation
interactive onboarding
special visual effect
```

Rule:

> Mixed UI technology is an exception, not the default.

Do not create two parallel UI stacks for ordinary controls.

---

## 20. Cross-Platform Requirement

The desktop application targets:

```text
macOS
Linux
Windows
```

The same main UI codebase should be used across all three where practical.

Platform-specific behavior should be isolated.

Examples that may need special handling later:

```text
menus
file associations
system theme behavior
window integration
packaging
signing
native dialogs
```

These differences must not leak into the Domain.

---

## 21. Native Look and Feel

ERDFlow should feel like a serious desktop application on each platform.

This does not mean every pixel must exactly match each OS.

It means:

- normal desktop behavior,
- correct keyboard/mouse interaction,
- expected window behavior,
- appropriate menus/dialogs,
- good typography,
- native-quality performance.

ERDFlow may apply its own coherent visual identity while preserving
platform conventions.

---

## 22. Styling

The chosen ERDFlow visual direction includes a dark professional interface.

Qt styling may use:

- Qt palettes,
- stylesheets where appropriate,
- custom widgets/delegates,
- controlled custom painting.

Do not build a large custom styling framework before the shell exists.

Performance-sensitive canvas rendering should not rely on excessive
stylesheet tricks.

---

## 23. Branding vs Platform UI

ERDFlow's visual identity may include:

```text
ERDFlow wordmark
dark navy surfaces
blue accent
stage-specific visual colors
```

Branding must not compromise:

```text
readability
accessibility
platform usability
state clarity
```

The logo/marketing aesthetic does not require every control in the
application to glow or behave like a marketing illustration.

---

## 24. Accessibility

Qt provides accessibility infrastructure, but ERDFlow must still design
for accessible interaction.

Later reviews should include:

- keyboard navigation,
- focus visibility,
- meaningful labels,
- contrast,
- scalable text,
- high-DPI,
- screen-reader behavior where practical.

Accessibility is not solved merely by choosing Qt.

---

## 25. High-DPI

ERDFlow is a visual design application and must behave correctly on modern
high-DPI displays.

The implementation should avoid hard-coded pixel assumptions where possible.

Canvas coordinates and semantic model measurements should not be confused
with raw physical screen pixels.

---

## 26. Input Model

Initial priority:

```text
mouse
trackpad
keyboard
```

Future:

```text
touch
stylus
sketch recognition
```

Qt Widgets has more limited touch-oriented ergonomics than Qt Quick, which is
a known trade-off.

Because stylus recognition is deferred, this does not outweigh Widgets'
benefits for V1.

If stylus becomes a first-class product feature later, an isolated Qt Quick
or specialized input surface can be evaluated.

---

## 27. Keyboard-First Productivity

ERDFlow should support professional keyboard workflows.

Examples later may include:

```text
Delete
Undo / Redo
Copy / Paste
Zoom
Tool selection
Search
Rename
Save
Open
Navigation
```

Qt's desktop event/action system is suitable for this style of application.

---

## 28. Command Integration

Qt actions should call Application commands.

Example:

```text
QAction("Delete")
      ↓
DeleteEntityCommand
      ↓
Application
      ↓
Domain
```

Do not hide business behavior directly inside `QAction` callbacks.

---

## 29. Undo Integration

Qt's Undo Framework may be used for desktop UI integration.

But ADR-002 remains in force:

```text
ERDFlow semantic command
      ↓
Qt undo adapter
      ↓
QUndoStack
```

The reusable core should not require `QUndoCommand`.

---

## 30. Build System

Qt 6 will be integrated through CMake.

Conceptually:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
```

Additional Qt modules are added only when actually required.

Avoid linking every Qt module by default.

---

## 31. Initial Qt Modules

Expected initial requirement:

```text
Qt6::Core
Qt6::Gui
Qt6::Widgets
```

Possible later modules depend on implementation evidence.

Do not treat this list as permission to expose Qt into the Domain.

---

## 32. C++ Standard

Desktop implementation uses:

```text
C++20
```

This is separate from Qt's own minimum compiler requirements.

C++20 gives ERDFlow a consistent language baseline for:

- strong value types,
- modern standard library,
- concepts/utilities where useful,
- maintainable systems code.

---

## 33. Compiler Discipline

The project should compile with strong warnings.

Platform-specific warning flags will be defined in the build phase.

Warnings should be treated seriously rather than silenced globally.

Exact `-Werror`/equivalent policy for all external/production builds can be
decided later.

---

## 34. Licensing Consideration

Qt uses a dual licensing model with commercial and open-source options.

Open-source Qt includes modules available under LGPL/GPL terms, while some
components may have different availability/licensing.

Therefore:

> ERDFlow must not assume that "using Qt" automatically means every Qt
> module can be distributed under any desired application license.

Before distribution:

- review the exact Qt modules used,
- review their licenses,
- determine open-source vs commercial Qt strategy,
- satisfy the applicable obligations,
- avoid casually adding GPL-only components if they conflict with the
  intended ERDFlow distribution model.

This ADR selects the technology.

It does not provide legal advice or finalize ERDFlow's distribution license.

---

## 35. Avoid Licensing Lock-In by Dependency Discipline

Use only required Qt modules.

Do not introduce third-party Qt components merely for visual convenience
without checking:

```text
license
maintenance
cross-platform support
release quality
```

This is especially relevant to:

```text
ribbon libraries
diagram libraries
commercial widget suites
```

---

## 36. Alternative A — Qt Quick / QML as Primary UI

### Strengths

- fluid/declarative UI,
- strong animation,
- good touch/gesture model,
- highly customizable visuals.

### Weaknesses for Current ERDFlow

- adds QML as a major second UI language,
- less aligned with the first milestone's traditional information-dense
  desktop tooling,
- would add complexity where Widgets already solves most shell needs.

### Decision

Not selected as primary.

Available selectively later.

---

## 37. Alternative B — Electron

### Strengths

- huge web ecosystem,
- HTML/CSS UI flexibility,
- easy web developer recruitment,
- cross-platform.

### Weaknesses

- bundled browser/runtime footprint,
- JavaScript/web stack becomes central,
- weaker fit with ERDFlow's chosen C++ desktop direction,
- requires a larger bridge between native core and presentation,
- less aligned with the desired native desktop architecture.

### Decision

Rejected for the primary desktop app.

---

## 38. Alternative C — Tauri

### Strengths

- smaller webview-based desktop footprint than Electron,
- Rust-oriented backend,
- cross-platform,
- strong future relevance to Rust ecosystems.

### Weaknesses for Current ERDFlow

- web frontend remains the primary UI model,
- Rust would become necessary much earlier,
- ERDFlow currently needs to establish the product/domain before Rust
  migration is justified,
- complex diagram/editor UI would still be implemented through a web stack.

### Decision

Rejected for the first desktop architecture.

Could be reconsidered for another frontend in the future.

---

## 39. Alternative D — Flutter

### Strengths

- cross-platform,
- polished custom rendering,
- strong UI development model.

### Weaknesses for Current ERDFlow

- introduces Dart as another primary language,
- less natural alignment with the existing C++ systems/core plan,
- less direct fit for a classic information-dense engineering desktop
  application compared with Qt Widgets.

### Decision

Rejected for the primary desktop app.

---

## 40. Alternative E — Platform-Native UI Per OS

Examples:

```text
AppKit / Swift on macOS
WinUI on Windows
GTK on Linux
```

### Strengths

- deepest native integration.

### Weaknesses

- three major UI implementations,
- duplicated feature work,
- duplicated testing,
- slower product iteration,
- higher risk of behavioral divergence.

### Decision

Rejected.

---

## 41. Alternative F — wxWidgets

### Strengths

- C++,
- cross-platform,
- native desktop focus,
- established toolkit.

### Why Qt Is Preferred

Qt provides a broader integrated ecosystem for ERDFlow's expected needs,
including:

- mature model/view,
- graphics framework,
- rich desktop controls,
- strong CMake support,
- cross-platform APIs beyond widgets,
- future option of Qt Quick within the same framework.

### Decision

Not selected.

---

## 42. Why Not a Browser-First Architecture

ERDFlow may eventually have a web-related frontend or service.

That does not mean the desktop product should be implemented as a browser
application today.

The current priority is:

```text
strong native desktop experience
local-first use
deep modeling workflow
```

The architecture already keeps the Domain reusable enough for a future
different frontend.

---

## 43. Future VS Code Extension

The future VS Code extension is a separate frontend.

It should not force the desktop app to use web technologies.

Conceptually:

```text
Qt Desktop UI
       \
        → ERDFlow Core
       /
VS Code Extension
```

Both can share domain/application capabilities through appropriate
boundaries.

---

## 44. Future Rust

Choosing Qt/C++ for Presentation does not block future Rust core work.

The architecture is:

```text
Qt / C++ Presentation
       ↓
Application/Core boundary
       ↓
C++ core today
Rust modules later where justified
```

Rust does not need to render the desktop interface.

---

## 45. Performance Expectations

Qt is capable of building high-performance desktop applications, but
framework selection alone does not guarantee performance.

ERDFlow must still:

- update the canvas incrementally,
- avoid unnecessary scene rebuilds,
- avoid blocking the UI thread,
- use model/view correctly,
- page/chunk large data where necessary,
- profile actual bottlenecks.

SCALE.md remains authoritative for performance strategy.

---

## 46. Canvas Performance Prototype

Before locking the first Canvas implementation, create a prototype that
measures:

```text
create many nodes
create many connectors
drag nodes
zoom
pan
selection
label updates
connector updates
memory
```

The result should inform the Canvas-specific decision.

---

## 47. UI Testing

The architecture should maximize testing below the UI layer.

Qt-specific tests should focus on:

- widget behavior,
- interaction wiring,
- rendering-sensitive behavior where necessary.

Domain and Application behavior should remain testable without a GUI.

---

## 48. Qt Version Policy

ERDFlow targets Qt 6.

Do not target Qt 5 for compatibility.

The exact minimum Qt 6 minor version should be selected during the build
foundation based on:

- APIs actually required,
- supported operating systems,
- available developer environments,
- packaging requirements.

Avoid choosing the newest minor version solely because it exists.

---

## 49. Long-Term Support Consideration

Before a production release, evaluate whether ERDFlow should standardize on:

- an available Qt LTS release,
- another supported Qt 6 release appropriate for the distribution model.

That is a release engineering decision.

The architecture remains Qt 6 either way.

---

## 50. Initial Development Environment

For early development, the target should be:

```text
C++20
Qt 6
CMake
macOS development machine
```

while keeping source portable for:

```text
Linux
Windows
```

Cross-platform CI follows later in the roadmap.

---

## 51. UI State vs Project State

Qt may own ephemeral UI state such as:

```text
selected tab
hover state
open dialog
temporary drag
panel focus
```

Persistent project state belongs elsewhere.

Example:

```text
Entity position
```

may be project layout data.

Example:

```text
current hover highlight
```

is UI-only state.

---

## 52. No Business Logic in Custom Painting

Custom paint routines should render state.

They should not decide database semantics.

Wrong:

```text
paint()
    detects M:M
    creates junction relation
```

Correct:

```text
Domain relationship = M:M
       ↓
Presentation paint()
       ↓
draws M near each entity side
```

---

## 53. No Persistence in Widgets

Wrong:

```text
MainWindow::saveProject()
    manually serializes every Entity
```

Preferred:

```text
MainWindow
    ↓
SaveProject use case
    ↓
ProjectStore
```

The Qt shell initiates the operation.

It does not own persistence semantics.

---

## 54. No Import Logic in Dialogs

Import dialogs gather options and show previews.

They should not contain parser/domain logic.

```text
Dialog
  ↓
Application
  ↓
Importer adapter
  ↓
structured result
```

---

## 55. No Data Ownership in Table Widgets

Use model/view rather than treating a `QTableWidget` as the authoritative
database data structure.

For ERDFlow's serious data workflows, prefer:

```text
QTableView
+
QAbstractItemModel adapter
```

over storing all application truth inside UI table cells.

---

## 56. Initial Shell Recommendation

The first real GUI step should be deliberately small:

```text
QApplication
    ↓
QMainWindow
    ├── Explorer dock
    ├── empty central workspace
    ├── Properties dock
    ├── menu/toolbar
    └── status bar
```

Compile and run this before implementing Conceptual modeling.

---

## 57. Initial Canvas Recommendation

When the Canvas phase begins:

1. create a small `QGraphicsScene/QGraphicsView` prototype,
2. render generic rectangle/oval/diamond/connector objects,
3. test move/select/zoom/pan,
4. test with larger synthetic scenes,
5. then decide whether to adopt it formally.

Do not embed Domain rules into the prototype.

---

## 58. Consequences — Positive

This decision gives ERDFlow:

- a mature desktop toolkit,
- direct C++20 integration,
- strong table/tree support,
- a natural desktop shell,
- cross-platform source reuse,
- a mature model/view system,
- a viable first canvas candidate,
- future access to Qt Quick if needed,
- a straightforward relationship with the existing layered architecture.

---

## 59. Consequences — Costs

ERDFlow accepts:

- Qt framework dependency,
- Qt learning curve,
- deployment/packaging work,
- Qt licensing obligations that must be managed correctly,
- manual effort for very custom modern visuals,
- weaker touch-first ergonomics in Widgets than Qt Quick,
- some Qt-specific adapter code.

These costs are acceptable for the expected product shape.

---

## 60. Risks

### Risk 1 — Qt leaks into Domain

Mitigation:

```text
build/dependency boundaries
code review
Domain tests without Qt
```

### Risk 2 — UI becomes visually dated

Mitigation:

```text
careful custom styling
custom delegates/widgets where valuable
strong typography/layout
avoid default-looking prototype UI in final product
```

### Risk 3 — Over-customizing Widgets

Mitigation:

Do not rebuild every standard control from scratch.

Customize where product value exists.

### Risk 4 — Licensing mistakes

Mitigation:

Review actual modules and distribution obligations before release.

### Risk 5 — Graphics View fails future scale needs

Mitigation:

Canvas choice remains separate and must be prototyped/benchmarked.

---

## 61. Decision Invariants

### Invariant 1

ERDFlow desktop UI uses Qt 6.

### Invariant 2

C++20 is the primary desktop implementation language.

### Invariant 3

Qt Widgets is the default main-shell/productivity UI technology.

### Invariant 4

Qt Quick/QML is optional, not the default UI architecture.

### Invariant 5

The exact ERD canvas technology is not locked by this ADR.

### Invariant 6

Domain code does not depend on Qt UI types.

### Invariant 7

Data-heavy Qt UI uses model/view separation where appropriate.

### Invariant 8

Desktop UI code invokes Application behavior rather than implementing
database rules directly.

### Invariant 9

ERDFlow targets macOS, Linux, and Windows.

### Invariant 10

Qt module/licensing choices must be reviewed before distribution.

---

## 62. First Implementation Scope

When actual implementation begins, use only what is needed:

```text
Qt 6 Core
Qt 6 Gui
Qt 6 Widgets
C++20
CMake
```

Build:

```text
MainWindow
Explorer placeholder
Workspace placeholder
Properties placeholder
Status bar
Basic menu/toolbar
```

Then test.

Do not immediately build:

```text
QML system
custom theme engine
third-party ribbon system
plugin UI
AI panel
stylus system
```

---

## 63. References Considered

This decision is informed by the current Qt 6 documentation:

- Qt UI overview:
  `https://doc.qt.io/qt-6/topics-ui.html`

- Qt Widgets overview:
  `https://doc.qt.io/qt-6/qtwidgets-index.html`

- Qt Model/View Programming:
  `https://doc.qt.io/qt-6/model-view-programming.html`

- Qt Quick overview/types:
  `https://doc.qt.io/qt-6/qtquick-qmlmodule.html`

- Qt licensing:
  `https://doc.qt.io/qt-6/licensing.html`

- Qt open-source licensing obligations:
  `https://www.qt.io/development/open-source-lgpl-obligations`

The exact versions and licensing requirements must be rechecked when
preparing distribution.

---

## 64. Decision Outcome

ERDFlow adopts:

```text
Qt 6
+
C++20
+
Qt Widgets as the primary desktop UI
```

with:

```text
Qt Quick/QML
→ optional future specialized UI technology

Canvas implementation
→ evaluated separately during Canvas phase
```

This gives ERDFlow a strong desktop foundation without coupling the core
database model to the UI framework.

---

## 65. Final Principle

> Use Qt to build the desktop experience, not to define the database model.

ERDFlow's user interface may evolve significantly over time.

Its Domain meaning must remain stable underneath it.
