# ERDFlow — Setup Milestone 01
## C++20 + Qt 6 + Qt Widgets + CMake + VS Code IntelliSense

**Project:** ERDFlow  
**Repository:** `~/Developer/erdflow`  
**GitHub:** `Zain-Orwa/erdflow`  
**Branch:** `main`  
**Platform used for this milestone:** Apple Silicon macOS  
**Current stack:** C++20 + Qt 6 + Qt Widgets + CMake

---

# 1. Why this milestone matters

Before drawing entities, relationships, schemas, SQL, or data grids, ERDFlow first needs a reliable desktop application foundation.

The goal of this setup milestone is:

```text
Source Code
    ↓
CMake understands the project
    ↓
Compiler understands C++20
    ↓
CMake finds Qt
    ↓
Qt Widgets are linked
    ↓
ERDFlow executable is produced
    ↓
macOS launches the application window
```

At the end of this milestone we have proven:

- the repository builds,
- C++20 is enabled,
- Qt 6 is found,
- Qt Widgets can be used,
- `MainWindow` is separated from `main.cpp`,
- the application launches,
- VS Code can understand the Qt headers,
- we are ready to start building the real desktop shell.

This is the foundation underneath everything that comes later.

---

# 2. Big mental model

Think of building ERDFlow like building a workshop.

```text
C++              = language we use to build things
Qt               = toolbox of ready-made desktop components
Qt Widgets       = desktop UI tools inside that toolbox
CMake            = build planner / project organizer
Compiler         = machine that turns C++ into executable code
VS Code          = editor
compile_commands = instructions that help VS Code understand the real build
ERDFlow          = the application we are building
```

A useful analogy:

```text
Architectural drawing    → CMakeLists.txt
Building materials       → C++ + Qt
Construction workers     → Compiler + linker
Finished building        → erdflow executable
Rooms/furniture          → MainWindow, Explorer, Canvas, Properties...
```

---

# 3. Current project flow

ERDFlow's product flow is:

```text
Conceptual ERD
      ↓
Relational Schema
      ↓
Physical / Table Design
      ↓
SQL
      ↓
Data
```

The product philosophy is:

> Draw once. Progressively refine.

But none of those features should be placed directly into `main.cpp`.

We are building a clean application shell first.

---

# 4. Current architecture direction

The architecture is:

```text
Presentation (Qt)
      ↓
Application
      ↓
Domain

Infrastructure / adapters implement external ports.
```

Very important:

```text
Qt UI ≠ Domain model
```

For example:

```text
QGraphicsItem ≠ Entity
QTreeView      ≠ Project model
QTableView     ≠ Database rows themselves
```

Qt presents information.

The Domain will eventually contain the real database meaning.

---

# 5. What is CMake?

CMake is a **build-system generator**.

It does not normally compile our C++ code itself.

Instead, CMake reads:

```text
CMakeLists.txt
```

and prepares the real build instructions for the platform.

On our machine the idea is:

```text
CMakeLists.txt
      ↓
CMake
      ↓
Build files
      ↓
Apple Clang compiler + linker
      ↓
erdflow executable
```

## Analogy

Imagine we tell a construction manager:

> Build a house using these rooms, these materials, and these rules.

That instruction sheet is similar to `CMakeLists.txt`.

CMake organizes the construction process.

---

# 6. Why ERDFlow needs CMake

ERDFlow is intended to be cross-platform.

We do not want to maintain completely different manual compiler commands for:

```text
macOS
Linux
Windows
```

Instead, we describe the project once:

```text
CMakeLists.txt
```

Then CMake creates the appropriate build configuration for the platform.

This is especially useful later when ERDFlow grows into many source files and modules.

---

# 7. Our current CMakeLists.txt

The current project configuration is:

```cmake
cmake_minimum_required(VERSION 3.21)

project(ERDFlow
    VERSION 0.1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(erdflow
    app/desktop/main.cpp
    app/desktop/main_window.cpp
    app/desktop/main_window.hpp
)

target_link_libraries(erdflow
    PRIVATE
        Qt6::Widgets
)
```

Now let us understand every part.

---

# 8. `cmake_minimum_required`

```cmake
cmake_minimum_required(VERSION 3.21)
```

## WHAT

Defines the minimum CMake version the project expects.

## WHY

CMake features and behavior evolve.

This prevents somebody from trying to configure ERDFlow using a CMake version that is too old.

## HOW

CMake checks its version before continuing.

Analogy:

```text
"This application requires macOS X or newer."
```

Same idea, but for the build tool.

---

# 9. `project(...)`

```cmake
project(ERDFlow
    VERSION 0.1.0
    LANGUAGES CXX
)
```

## WHAT

Defines the CMake project.

## WHY

CMake needs to know the project name, version, and programming language.

## HOW

We tell CMake:

```text
Name     = ERDFlow
Version  = 0.1.0
Language = C++
```

`CXX` is CMake's name for C++.

---

# 10. C++20 configuration

```cmake
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
```

## WHAT

Configures the C++ language standard.

## WHY

ERDFlow has deliberately chosen C++20.

We want predictable language behavior across machines.

## HOW

### First line

```cmake
set(CMAKE_CXX_STANDARD 20)
```

means:

> Compile ERDFlow as C++20.

### Second line

```cmake
set(CMAKE_CXX_STANDARD_REQUIRED ON)
```

means:

> Do not silently fall back to an older C++ version.

### Third line

```cmake
set(CMAKE_CXX_EXTENSIONS OFF)
```

means:

> Prefer standard C++ rather than compiler-specific language extensions.

This improves portability.

---

# 11. What is Qt?

Qt is a large cross-platform application framework.

For ERDFlow, Qt provides desktop functionality such as:

```text
windows
buttons
menus
dock panels
tree views
table views
dialogs
graphics
events
keyboard input
mouse input
layout systems
model/view architecture
```

Instead of directly programming separately against:

```text
Windows Win32
macOS Cocoa
Linux window systems
```

we write Qt code.

Qt handles much of the platform-specific work.

---

# 12. Qt cross-platform mental model

```text
ERDFlow code
      ↓
Qt API
      ↓
Platform-specific Qt implementation
      ↓
Operating System
```

For example:

```text
                    ┌─ Windows
ERDFlow → Qt →      ├─ macOS
                    └─ Linux
```

This is one major reason Qt fits ERDFlow.

---

# 13. What is Qt Widgets?

Qt contains different technologies.

ERDFlow currently uses:

```text
Qt 6
└── Qt Widgets
```

Qt Widgets is especially useful for desktop productivity applications.

Examples:

```text
QMainWindow
QDockWidget
QTreeView
QTableView
QDialog
QMenu
QToolBar
QStatusBar
```

ERDFlow is closer to:

```text
IDE
database designer
engineering tool
desktop modeling application
```

than to a simple mobile-style application.

That is why Qt Widgets is a strong fit.

---

# 14. How CMake finds Qt

This line is important:

```cmake
find_package(Qt6 REQUIRED COMPONENTS Widgets)
```

Break it down:

```text
find_package
    ↓
Find an installed external package

Qt6
    ↓
The package we want

REQUIRED
    ↓
Configuration must fail if Qt cannot be found

COMPONENTS Widgets
    ↓
We specifically need the Qt Widgets module
```

Mental model:

```text
CMake
  │
  ├── "Do you have Qt 6?"
  │
  └── "Do you have the Widgets component?"
```

If Qt cannot be found, CMake cannot correctly configure this project.

---

# 15. Where Qt came from on our machine

Qt 6 is installed on the Apple Silicon macOS development machine using Homebrew.

Current environment:

```text
Qt       6.11.1
CMake    4.3.4
Compiler Apple Clang 21
```

The exact original Homebrew installation command is not reproduced here because the important confirmed state for this milestone is that Qt is installed and CMake successfully finds it.

---

# 16. Creating the ERDFlow executable target

```cmake
add_executable(erdflow
    app/desktop/main.cpp
    app/desktop/main_window.cpp
    app/desktop/main_window.hpp
)
```

## WHAT

Defines an executable program named:

```text
erdflow
```

## WHY

CMake needs to know which files belong to the application.

## HOW

We give CMake the source files.

Mental model:

```text
main.cpp
       \
main_window.cpp ──→ build target: erdflow
       /
main_window.hpp
```

The result is something we can launch:

```bash
./build/erdflow
```

---

# 17. Linking Qt Widgets

```cmake
target_link_libraries(erdflow
    PRIVATE
        Qt6::Widgets
)
```

This is one of the most important setup lines.

## WHAT

Links Qt Widgets to the `erdflow` executable.

## WHY

Writing:

```cpp
#include <QMainWindow>
```

is not enough by itself.

The compiler may understand the declaration, but the final executable must also be connected to Qt's implementation.

## HOW

CMake connects:

```text
erdflow
   +
Qt6::Widgets
```

during the build.

---

# 18. Include vs link

This is a very important beginner distinction.

## Include

```cpp
#include <QMainWindow>
```

roughly means:

> Let this source file see the declarations for `QMainWindow`.

## Link

```cmake
target_link_libraries(erdflow PRIVATE Qt6::Widgets)
```

roughly means:

> Connect our program to the compiled Qt Widgets implementation.

Analogy:

```text
Header/include = instruction manual
Library/link    = actual machine
```

Having only the manual is not enough.

Having the machine without knowing its controls is also not enough.

We need both.

---

# 19. Current source structure

At this milestone:

```text
erdflow/
│
├── CMakeLists.txt
│
├── app/
│   └── desktop/
│       ├── main.cpp
│       ├── main_window.hpp
│       └── main_window.cpp
│
├── build/
│   └── ...
│
└── .vscode/
    └── settings.json
```

Each file has a different responsibility.

---

# 20. `main.cpp`

`main.cpp` is the application entry point.

Its current responsibility is conceptually:

```text
start application
      ↓
create QApplication
      ↓
create MainWindow
      ↓
show MainWindow
      ↓
enter Qt event loop
```

The important design decision is:

> `main.cpp` should stay small.

It should start ERDFlow, not become the place where all UI and database logic is written.

---

# 21. `QApplication`

A Qt Widgets application needs a `QApplication`.

Mental model:

```text
QApplication
    ↓
runs the desktop application's event system
```

It helps process things such as:

```text
mouse clicks
keyboard presses
window events
menus
timers
focus
closing
opening
```

Without the application event loop, the window would not behave like an interactive desktop application.

---

# 22. The event loop

Eventually `main.cpp` reaches:

```cpp
app.exec();
```

Conceptually:

```text
Start ERDFlow
    ↓
Create UI
    ↓
Show MainWindow
    ↓
app.exec()
    ↓
┌─────────────────────────┐
│ Wait for user event     │
│ Handle event            │
│ Wait for next event     │
│ Handle event            │
│ ...                     │
└─────────────────────────┘
```

For example:

```text
User clicks Explorer
        ↓
Qt receives mouse event
        ↓
Qt dispatches event
        ↓
ERDFlow reacts
```

---

# 23. Why we separated MainWindow from `main.cpp`

Originally it would be easy to put everything into one file.

But ERDFlow will become large.

So we separated:

```text
main.cpp
    ↓
starts application

main_window.hpp
    ↓
declares MainWindow

main_window.cpp
    ↓
defines MainWindow behavior
```

This makes the project easier to understand and grow.

---

# 24. Header file vs implementation file

## Header

```text
main_window.hpp
```

answers:

> What is `MainWindow`?

## Source file

```text
main_window.cpp
```

answers:

> How does `MainWindow` work?

Analogy:

```text
Restaurant menu      = header
Kitchen implementation = .cpp
```

The menu tells you what exists.

The kitchen performs the work.

---

# 25. Current `main_window.hpp`

```cpp
#pragma once

#include <QMainWindow>

class MainWindow : public QMainWindow
{
public:
    explicit MainWindow(QWidget *parent = nullptr);
};
```

---

# 26. `#pragma once`

```cpp
#pragma once
```

## WHAT

Prevents the same header from being included repeatedly in one compilation unit.

## WHY

Repeated class declarations from the same header can create problems.

## HOW

The compiler remembers that it has already processed this header.

Mental shortcut:

> Read this header once.

---

# 27. Local header vs library header

We discussed this distinction.

Qt/library header:

```cpp
#include <QMainWindow>
```

Project-local header:

```cpp
#include "main_window.hpp"
```

Useful beginner rule:

```text
<...>   library/system header
"..."   local project header
```

This is a convention and search-path distinction that keeps intent clear.

---

# 28. Our `MainWindow` class

```cpp
class MainWindow : public QMainWindow
```

This means:

```text
Our MainWindow
      ↓ inherits from
Qt QMainWindow
```

We do not create a desktop window completely from zero.

We reuse Qt's existing main-window behavior and extend it for ERDFlow.

Analogy:

```text
QMainWindow
= ready-made car chassis

MainWindow
= our ERDFlow car built on that chassis
```

---

# 29. Inheritance here

```cpp
class MainWindow : public QMainWindow
```

`public QMainWindow` means our class publicly inherits from Qt's `QMainWindow`.

So our `MainWindow` can use functionality provided by `QMainWindow`, such as:

```text
window title
window sizing
dock areas
menu bar
toolbars
status bar
central widget area
```

This becomes very important for ERDFlow's desktop shell.

---

# 30. MainWindow as our application board

A useful mental picture:

```text
┌───────────────────────────────────────────────┐
│                 ERDFlow MainWindow            │
│                                               │
│  Explorer     Future workspace    Properties │
│                                               │
│                                               │
│                 Status                        │
└───────────────────────────────────────────────┘
```

`MainWindow` is the main outer desktop shell.

It is similar to a board/frame where we arrange UI components.

Qt's layout/docking systems handle the real placement.

---

# 31. Constructor declaration

Inside the header:

```cpp
explicit MainWindow(QWidget *parent = nullptr);
```

This is the **constructor declaration**.

A constructor is a special member function that runs when an object of the class is created.

Example idea:

```cpp
MainWindow window;
```

causes the `MainWindow` constructor to run.

---

# 32. What `QWidget *parent` means

```cpp
QWidget *parent
```

is a pointer to a Qt widget that could own this widget/window.

Qt uses parent-child relationships extensively.

Example:

```text
Parent Widget
    ├── Child A
    ├── Child B
    └── Child C
```

For our main window, the default is:

```cpp
nullptr
```

meaning:

> no parent widget was provided.

Therefore it normally behaves as a top-level window.

---

# 33. What `nullptr` means

```cpp
nullptr
```

means:

> This pointer currently points to no object.

Modern C++ prefers `nullptr` over older forms such as:

```cpp
NULL
0
```

because `nullptr` is specifically designed to represent a null pointer.

Mental model:

```text
pointer → object
```

versus:

```text
pointer → nothing
```

The second case is `nullptr`.

---

# 34. What top-level means

A top-level widget/window is not embedded inside another widget.

For ERDFlow:

```text
MainWindow ← top-level application window
    │
    ├── Explorer
    ├── Canvas
    └── Properties
```

It is the main outer window visible to the operating system.

---

# 35. Current `main_window.cpp`

```cpp
#include "main_window.hpp"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle("ERDFlow");
    resize(1200, 800);
}
```

---

# 36. `MainWindow::MainWindow`

```cpp
MainWindow::MainWindow(QWidget *parent)
```

This is the **constructor definition**.

The first `MainWindow` identifies the class scope.

The second `MainWindow` is the constructor name.

`::` is the **scope resolution operator**.

Read:

```text
MainWindow::MainWindow
```

as:

> the `MainWindow` constructor belonging to the `MainWindow` class.

---

# 37. Base-class constructor

```cpp
: QMainWindow(parent)
```

This is a constructor initializer list.

Before our constructor body runs, it initializes the base class.

Flow:

```text
Create MainWindow
      ↓
QMainWindow(parent)
      ↓
MainWindow constructor body
```

Why?

Because our `MainWindow` **is built on top of** `QMainWindow`.

The base part must exist first.

---

# 38. Window title

```cpp
setWindowTitle("ERDFlow");
```

This changes the visible title of the application window.

Conceptually:

```text
┌─────────────────────────────┐
│ ERDFlow                     │ ← title
├─────────────────────────────┤
│                             │
│                             │
└─────────────────────────────┘
```

Because `MainWindow` inherits from `QMainWindow`, we can use this functionality.

---

# 39. Starting window size

```cpp
resize(1200, 800);
```

This requests an initial size:

```text
width  = 1200 pixels
height = 800 pixels
```

Visual:

```text
            1200 px
    ◄──────────────────►

    ┌──────────────────┐
    │                  │
    │                  │
    │                  │ 800 px
    │                  │
    │                  │
    └──────────────────┘
```

It is a starting size, not a permanent fixed size.

The user can still resize the window.

---

# 40. How the operating system is involved

Our class is not the operating system window API itself.

We write:

```text
MainWindow
    ↓
QMainWindow
    ↓
Qt platform layer
    ↓
macOS
```

On another machine:

```text
MainWindow
    ↓
QMainWindow
    ↓
Qt platform layer
    ↓
Windows/Linux
```

This is how Qt helps ERDFlow stay cross-platform.

---

# 41. Building the project

The project has already successfully passed the build test.

The general workflow is:

```text
Configure
    ↓
Build
    ↓
Run
```

Conceptually:

```bash
cmake -S . -B build
cmake --build build
./build/erdflow
```

We use one command at a time during implementation so errors remain easy to locate.

---

# 42. What `-S .` and `-B build` mean

Example:

```bash
cmake -S . -B build
```

Breakdown:

```text
-S .
```

means:

> Source directory is the current directory.

```text
-B build
```

means:

> Put generated build files inside the `build` directory.

So:

```text
project source
     │
     │ CMake reads
     ▼
CMakeLists.txt

generated build material
     ↓
build/
```

This keeps generated files separate from source code.

---

# 43. Why use a separate `build/` directory?

Without it, generated build files could clutter the project source tree.

Preferred:

```text
erdflow/
├── app/
├── docs/
├── CMakeLists.txt
└── build/        ← generated
```

rather than mixing compiler-generated material with our actual source files.

This is called an **out-of-source build**.

---

# 44. Running ERDFlow

After building:

```bash
./build/erdflow
```

means:

```text
.
└── build
    └── erdflow
```

`./` means:

> start from the current directory.

Then macOS launches the compiled application process.

We already verified that a blank `QMainWindow` launches successfully.

---

# 45. The VS Code problem we encountered

After writing Qt code, VS Code showed an error similar to:

```text
cannot open source file "QWidget"
```

Important discovery:

> The actual CMake project was already capable of building Qt.

The problem was VS Code IntelliSense.

There are two different systems:

```text
Real build system
    ↓
CMake + compiler

Editor understanding
    ↓
VS Code IntelliSense
```

The editor can be confused even when the real compiler is correctly configured.

---

# 46. Why this distinction matters

A red underline in VS Code does **not automatically mean** the project cannot compile.

We must distinguish:

```text
Editor diagnostic
```

from:

```text
Compiler/build error
```

For ERDFlow, CMake knew where Qt lived.

VS Code did not yet know.

---

# 47. `compile_commands.json`

We enabled:

```text
compile_commands.json
```

This file is a **compilation database**.

It records the real compile command information CMake generated.

It can contain information such as:

```text
compiler
source file
include paths
compiler flags
language standard
Qt header paths
```

Conceptual example:

```text
main_window.cpp
      ↓
compile_commands.json says:
      ├── use Apple Clang
      ├── use C++20
      ├── search these include folders
      └── use these Qt paths
```

VS Code can read that information instead of guessing.

---

# 48. Command used to generate it

We ran:

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## WHAT

Reconfigured the project.

## WHY

We wanted CMake to produce the compilation database.

## HOW

This option:

```text
-DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

sets a CMake configuration variable.

Result:

```text
build/compile_commands.json
```

was generated.

---

# 49. The Vulkan message

During configuration we saw:

```text
Could NOT find WrapVulkanHeaders
```

while CMake still finished with:

```text
Configuring done
Generating done
Build files have been written...
```

For our current Qt Widgets application, this did not prevent successful configuration.

The important evidence was that CMake completed and generated the build files.

We therefore did not treat that non-blocking message as an ERDFlow setup failure.

---

# 50. Verifying the compile database

We checked:

```bash
ls build/compile_commands.json
```

and received:

```text
build/compile_commands.json
```

That proved the file existed.

This is a good development habit:

```text
Make change
    ↓
Verify expected result
```

rather than assuming a command succeeded.

---

# 51. VS Code workspace configuration

There was initially no:

```text
.vscode/
```

directory.

We created it:

```bash
mkdir -p .vscode
```

Then we created:

```text
.vscode/settings.json
```

with:

```json
{
    "C_Cpp.default.compileCommands": "${workspaceFolder}/build/compile_commands.json"
}
```

---

# 52. What JSON is

JSON means:

```text
JavaScript Object Notation
```

But it is widely used outside JavaScript.

It is a structured text format.

Example:

```json
{
    "name": "ERDFlow",
    "version": 1
}
```

Think of it like labeled configuration data:

```text
name    → ERDFlow
version → 1
```

---

# 53. Our VS Code JSON file

```json
{
    "C_Cpp.default.compileCommands": "${workspaceFolder}/build/compile_commands.json"
}
```

This is **not part of ERDFlow's runtime application**.

It configures the editor.

It tells Microsoft's C/C++ tooling:

> Use this CMake compilation database to understand this project.

---

# 54. `${workspaceFolder}`

In VS Code:

```text
${workspaceFolder}
```

means:

> the root folder currently opened as the workspace.

For us, conceptually:

```text
${workspaceFolder}
      ↓
~/Developer/erdflow
```

Therefore:

```text
${workspaceFolder}/build/compile_commands.json
```

points to the generated compilation database.

---

# 55. Why not hard-code the complete path?

We could have written something resembling:

```text
/Users/.../Developer/erdflow/build/compile_commands.json
```

but `${workspaceFolder}` is cleaner.

It makes the configuration relative to the project.

That is more portable.

---

# 56. Reloading VS Code

After creating the setting, we used:

```text
Cmd + Shift + P
```

then:

```text
Developer: Reload Window
```

Why?

VS Code needed to reload the project configuration.

After reloading, the false Qt header error disappeared.

That confirmed the editor configuration worked.

---

# 57. Complete connection diagram

Here is the complete setup we now have:

```text
                         ERDFlow SOURCE
                              │
                  ┌───────────┴───────────┐
                  │                       │
            CMakeLists.txt          C++ source files
                  │                       │
                  └───────────┬───────────┘
                              │
                              ▼
                            CMake
                              │
                  finds Qt 6 Widgets
                              │
                              ▼
                       Build configuration
                              │
                ┌─────────────┴─────────────┐
                │                           │
                ▼                           ▼
      compile_commands.json          Compiler / linker
                │                           │
                ▼                           ▼
        VS Code IntelliSense          erdflow executable
                                            │
                                            ▼
                                      macOS launches
                                            │
                                            ▼
                                        MainWindow
```

---

# 58. Build-time vs editor-time vs run-time

This is worth memorizing.

## Editor-time

```text
VS Code
IntelliSense
compile_commands.json
```

Helps **you** write and understand code.

## Build-time

```text
CMake
Apple Clang
Qt headers/libraries
```

Turns source code into the application.

## Run-time

```text
./build/erdflow
Qt
macOS
```

The compiled program is actually running.

Diagram:

```text
WRITE
  ↓
EDITOR
  ↓
BUILD
  ↓
EXECUTABLE
  ↓
RUN
```

---

# 59. What CMake does not do

CMake is not:

```text
the C++ language
the Qt framework
the compiler
the ERDFlow application
the VS Code editor
```

Its main role is organizing/configuring the build.

---

# 60. What Qt does not do

Qt is not:

```text
our database Domain
our Entity class
our conversion engine
CMake
the compiler
```

Qt is primarily the framework currently used for the desktop Presentation layer and related platform integration.

---

# 61. What VS Code does not do

VS Code is not the authority on whether our project compiles.

It is an editor.

Its IntelliSense system helps us, but the real build remains:

```text
CMake + compiler
```

That is why fixing `compile_commands.json` was an **editor setup fix**, not a change to ERDFlow's application architecture.

---

# 62. Why our setup supports ERDFlow's future

This foundation already gives us several important properties.

## Cross-platform direction

```text
C++20 + Qt 6 + CMake
```

can support:

```text
macOS
Linux
Windows
```

## Clean desktop shell

`QMainWindow` is designed for desktop application layouts.

## Future dock panels

`QMainWindow` supports dock areas, which is exactly what we need for things such as:

```text
Explorer
Properties
future utility panels
```

## Future data grid

Qt's model/view system provides:

```text
QTableView
QAbstractItemModel
```

which aligns with ERDFlow's DataSource architecture.

## Future Explorer

Qt gives us:

```text
QDockWidget
QTreeView
```

which is the next implementation step.

---

# 63. Why Qt should stay out of the Domain

Even though Qt is excellent for the UI, ERDFlow deliberately does not want:

```cpp
QGraphicsItem == Entity
```

or domain classes that exist only because the UI framework exists.

Preferred mental model:

```text
Domain Entity
    ↓ represented by
Qt visual item
```

not:

```text
Qt visual item
    =
Domain Entity
```

Why?

Because later the same domain information may be used by:

```text
desktop UI
SQL generation
validation
import
CLI
tests
future service
possible future Rust module
```

without needing a GUI.

---

# 64. Our current milestone boundary

We have completed the **foundation shell setup**.

Confirmed current state:

```text
✓ CMake project works
✓ C++20 configured
✓ Qt 6 Widgets found
✓ ERDFlow executable builds
✓ ERDFlow launches
✓ MainWindow separated from main.cpp
✓ Window title = ERDFlow
✓ Initial size = 1200 × 800
✓ compile_commands.json generated
✓ VS Code IntelliSense understands Qt headers
```

Not yet implemented:

```text
Explorer panel
```

This is intentional.

The next real UI step is:

```text
MainWindow
    ↓
LEFT Explorer
    ↓
QDockWidget
+
QTreeView
```

We stopped before executing that implementation.

---

# 65. What the next UI milestone will look like

Current:

```text
┌────────────────────────────────────────────┐
│ ERDFlow                                    │
├────────────────────────────────────────────┤
│                                            │
│                                            │
│              blank shell                   │
│                                            │
│                                            │
└────────────────────────────────────────────┘
```

Next:

```text
┌────────────────────────────────────────────┐
│ ERDFlow                                    │
├──────────────┬─────────────────────────────┤
│ Explorer     │                             │
│              │                             │
│ QTreeView    │       main workspace        │
│              │                             │
│              │                             │
└──────────────┴─────────────────────────────┘
```

But that belongs to the **next coding step**, not this setup milestone.

---

# 66. The development method we are using

For ERDFlow implementation:

```text
Understand
    ↓
Design
    ↓
Implement one small step
    ↓
Build
    ↓
Test
    ↓
Continue
```

And during learning:

```text
Assistant explains WHAT / WHY / HOW
            ↓
You write the C++ yourself
            ↓
We inspect it
            ↓
We build it
            ↓
We test visible behavior
```

This avoids copy-paste learning.

---

# 67. Quick command reference

## Configure

```bash
cmake -S . -B build
```

## Configure and generate compilation database

```bash
cmake -S . -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

## Build

```bash
cmake --build build
```

## Run

```bash
./build/erdflow
```

## Check compilation database

```bash
ls build/compile_commands.json
```

## Show source with syntax highlighting

```bash
ccat app/desktop/main_window.cpp
```

Our `ccat` command is an alias around `bat`, configured so terminal code can be read with syntax highlighting without opening the editor.

---

# 68. Five concepts to memorize

If you remember only five things from this milestone, remember these:

### 1.

```text
CMake = describes/configures how ERDFlow is built
```

### 2.

```text
Qt = cross-platform application framework
```

### 3.

```text
QMainWindow = Qt's desktop main-window class
MainWindow  = our ERDFlow class built from it
```

### 4.

```text
Header = declaration / what exists
.cpp   = definition / how it works
```

### 5.

```text
compile_commands.json
= bridge that lets VS Code understand the real CMake compiler configuration
```

---

# 69. Mini self-check

Try answering these without looking back.

### Q1
What is the difference between CMake and the compiler?

### Q2
Why does ERDFlow use Qt instead of writing separate macOS, Windows, and Linux window code?

### Q3
What does this mean?

```cpp
class MainWindow : public QMainWindow
```

### Q4
Why do we use:

```cpp
nullptr
```

for the default parent?

### Q5
What is the difference between:

```cpp
#include <QMainWindow>
```

and:

```cmake
target_link_libraries(erdflow PRIVATE Qt6::Widgets)
```

### Q6
Why was VS Code showing an error even though the project could build?

### Q7
What does `compile_commands.json` provide to VS Code?

### Q8
Why should `main.cpp` remain small?

### Q9
Why must Qt objects not become ERDFlow's authoritative Domain model?

### Q10
What is the very next implementation step?

Answer:

```text
Explorer on the LEFT
using QDockWidget + QTreeView
```

---

# 70. One-minute review

Before your next ERDFlow session, read this:

```text
ERDFlow is a C++20 desktop application.

CMake describes the build.

CMake finds Qt 6 and links Qt Widgets to our `erdflow` executable.

Qt provides cross-platform desktop UI components.

QApplication runs the Qt application/event loop.

QMainWindow is Qt's main desktop-window abstraction.

Our MainWindow inherits from QMainWindow and becomes ERDFlow's outer UI shell.

main_window.hpp declares the class.

main_window.cpp defines its behavior.

The current window is titled ERDFlow and starts at 1200 × 800.

The Domain must remain independent of Qt.

VS Code uses build/compile_commands.json so IntelliSense sees the same include paths and compiler configuration produced by CMake.

The foundation is working.

Next: Explorer = QDockWidget + QTreeView.
```

---

# 71. Milestone status

```text
MILESTONE 01 — DESKTOP FOUNDATION

[✓] Repository/project available
[✓] C++20
[✓] CMake
[✓] Qt 6
[✓] Qt Widgets
[✓] Executable target
[✓] Build
[✓] Run
[✓] QApplication
[✓] MainWindow
[✓] Header/source separation
[✓] VS Code Qt IntelliSense
[✓] compile_commands.json

NEXT
[ ] Explorer QDockWidget
[ ] Explorer QTreeView
```

**Milestone conclusion:**  
The development environment and minimum Qt desktop shell are operational. ERDFlow is now ready to move from setup into its first real desktop interface component.
