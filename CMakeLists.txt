cmake_minimum_required(VERSION 3.21)
project(ERDFlow VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Qt6 6.9 REQUIRED COMPONENTS Widgets Svg)
include(CTest)

add_library(erdflow_warnings INTERFACE)
if(MSVC)
    target_compile_options(erdflow_warnings INTERFACE /W4 /permissive- /utf-8)
else()
    target_compile_options(erdflow_warnings INTERFACE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
endif()
option(ERDFLOW_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
if(ERDFLOW_SANITIZERS AND NOT MSVC)
    target_compile_options(erdflow_warnings INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(erdflow_warnings INTERFACE -fsanitize=address,undefined)
endif()

add_library(erdflow_domain domain/model.cpp domain/schema_preview.cpp domain/diagram_from_schema.cpp)
target_include_directories(erdflow_domain PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(erdflow_domain PRIVATE erdflow_warnings)

add_library(erdflow_application application/editor.cpp application/history.cpp)
target_link_libraries(erdflow_application PUBLIC erdflow_domain PRIVATE erdflow_warnings)

add_library(erdflow_infrastructure infrastructure/project_store.cpp)
target_link_libraries(erdflow_infrastructure PUBLIC erdflow_application Qt6::Core PRIVATE erdflow_warnings)


# The canvas draws the diagram, so it is also what draws a picture of it.
# Qt6::Svg brings the generator that writes one as SVG.
add_library(erdflow_canvas app/desktop/diagram_view.cpp app/desktop/theme.cpp app/desktop/picture_export.cpp
                          app/desktop/document_export.cpp)
target_link_libraries(erdflow_canvas PUBLIC erdflow_application Qt6::Widgets Qt6::Svg PRIVATE erdflow_warnings)

add_library(erdflow_desktop app/desktop/main_window.cpp app/desktop/schema_view.cpp app/desktop/schema_router.cpp app/desktop/schema_explorer.cpp app/desktop/schema_facts.cpp
                            app/desktop/notice.cpp
                            app/desktop/home_page.cpp app/desktop/home_live_demo.cpp app/desktop/home_demo_scenes.cpp app/desktop/home_demo_canvas.cpp app/desktop/conceptual_examples.cpp app/desktop/relational_examples.cpp app/desktop/app_top_bar.cpp app/desktop/home_sidebar.cpp app/desktop/home_learning_panel.cpp app/desktop/start_route_card.cpp app/desktop/project_details_form.cpp app/desktop/welcome_flow_illustration.cpp app/desktop/ribbon.cpp app/desktop/icons.cpp
                            app/desktop/symbols.cpp app/desktop/symbol_picker.cpp
                            app/desktop/export_dialog.cpp app/desktop/search_bar.cpp)
target_link_libraries(erdflow_desktop PUBLIC erdflow_canvas Qt6::Svg PRIVATE erdflow_warnings)
# The Relational Schema's router is compiled optimised even in a Debug build.
# Routing every line is the costliest thing a schema drag does, and unoptimised
# the same search, finding the same routes, is some fourteen times slower;
# only the router is optimised, so the rest of the schema is still stepped
# through as written (2026-10-06). Not under MSVC, whose Debug /RTC1 cannot be
# combined with /O2; Release is untouched.
set_source_files_properties(app/desktop/schema_router.cpp PROPERTIES
    COMPILE_OPTIONS "$<$<AND:$<CONFIG:Debug>,$<NOT:$<CXX_COMPILER_ID:MSVC>>>:-O2>")
# The icon set is compiled in rather than read from disk, so a built ERDFlow is
# one file that cannot be started without its own artwork. Qt6::Svg brings the
# renderer that turns those files into icons.
qt_add_resources(erdflow_desktop "erdflow_icons"
    PREFIX "/erdflow/icons"
    BASE "assets/icons"
    FILES
        assets/icons/select.svg assets/icons/entity.svg assets/icons/attribute.svg
        assets/icons/relationship.svg assets/icons/isa.svg assets/icons/connect.svg
        assets/icons/note.svg assets/icons/comment.svg assets/icons/symbols.svg assets/icons/picture.svg assets/icons/pan.svg
        assets/icons/zoom.svg assets/icons/new-project.svg assets/icons/open.svg
        assets/icons/save.svg assets/icons/import.svg assets/icons/export.svg
        assets/icons/undo.svg assets/icons/redo.svg assets/icons/duplicate.svg
        assets/icons/delete.svg assets/icons/search.svg assets/icons/conceptual.svg
        assets/icons/schema.svg assets/icons/physical.svg assets/icons/sql.svg
        assets/icons/data.svg assets/icons/explorer.svg assets/icons/properties.svg
        assets/icons/validate.svg assets/icons/theme.svg assets/icons/settings.svg
        assets/icons/full-view.svg assets/icons/close.svg
        assets/icons/panel-left.svg assets/icons/panel-right.svg assets/icons/side-panels.svg
)

qt_add_resources(erdflow_desktop "erdflow_icons_on_dark"
    PREFIX "/erdflow/icons-on-dark"
    BASE "assets/icons-on-dark"
    FILES
        assets/icons-on-dark/select.svg assets/icons-on-dark/entity.svg assets/icons-on-dark/attribute.svg
        assets/icons-on-dark/relationship.svg assets/icons-on-dark/isa.svg assets/icons-on-dark/connect.svg
        assets/icons-on-dark/note.svg assets/icons-on-dark/comment.svg assets/icons-on-dark/symbols.svg assets/icons-on-dark/picture.svg assets/icons-on-dark/pan.svg
        assets/icons-on-dark/zoom.svg assets/icons-on-dark/new-project.svg assets/icons-on-dark/open.svg
        assets/icons-on-dark/save.svg assets/icons-on-dark/import.svg assets/icons-on-dark/export.svg
        assets/icons-on-dark/undo.svg assets/icons-on-dark/redo.svg assets/icons-on-dark/duplicate.svg
        assets/icons-on-dark/delete.svg assets/icons-on-dark/search.svg assets/icons-on-dark/conceptual.svg
        assets/icons-on-dark/schema.svg assets/icons-on-dark/physical.svg assets/icons-on-dark/sql.svg
        assets/icons-on-dark/data.svg assets/icons-on-dark/explorer.svg assets/icons-on-dark/properties.svg
        assets/icons-on-dark/validate.svg assets/icons-on-dark/theme.svg assets/icons-on-dark/settings.svg
        assets/icons-on-dark/full-view.svg assets/icons-on-dark/close.svg
        assets/icons-on-dark/panel-left.svg assets/icons-on-dark/panel-right.svg assets/icons-on-dark/side-panels.svg
)


# Marks drawn on data rather than on commands: the primary key's golden key.
# Kept apart from the icon sets, so it is the same key whichever set is chosen.
qt_add_resources(erdflow_desktop "erdflow_marks"
    PREFIX "/erdflow/marks"
    BASE "assets/marks"
    FILES
        assets/marks/primary-key.svg
)

# The line-art set: Lucide for the ordinary commands, ERDFlow's own for the
# database shapes Lucide has no icon for. Every file names its colour as the
# surrounding text's, so one set of files serves every palette. Lucide is ISC
# licensed; the notice travels with the icons.
qt_add_resources(erdflow_desktop "erdflow_icons_outline"
    PREFIX "/erdflow/icons-outline"
    BASE "assets/icons-outline"
    FILES
        assets/icons-outline/key.svg
        assets/icons-outline/select.svg assets/icons-outline/entity.svg
        assets/icons-outline/attribute.svg assets/icons-outline/relationship.svg
        assets/icons-outline/schema-relationships.svg
        assets/icons-outline/database.svg assets/icons-outline/file-text.svg
        assets/icons-outline/shield.svg assets/icons-outline/link.svg assets/icons-outline/zap.svg
        assets/icons-outline/ban.svg assets/icons-outline/circle-dashed.svg
        assets/icons-outline/fingerprint.svg
        assets/icons-outline/isa.svg assets/icons-outline/connect.svg
        assets/icons-outline/note.svg assets/icons-outline/symbols.svg assets/icons-outline/picture.svg
        assets/icons-outline/export.svg
        assets/icons-outline/pan.svg assets/icons-outline/zoom.svg
        assets/icons-outline/new-project.svg assets/icons-outline/open.svg
        assets/icons-outline/save.svg assets/icons-outline/undo.svg
        assets/icons-outline/redo.svg assets/icons-outline/duplicate.svg
        assets/icons-outline/delete.svg assets/icons-outline/search.svg
        assets/icons-outline/properties.svg assets/icons-outline/validate.svg
        assets/icons-outline/theme.svg assets/icons-outline/settings.svg
        assets/icons-outline/full-view.svg assets/icons-outline/close.svg
        assets/icons-outline/panel-left.svg assets/icons-outline/panel-right.svg assets/icons-outline/side-panels.svg
        # The Home screen's own: its sidebar, its learning panel and its bar.
        # Lucide throughout, so the page reads as one family with the rest.
        assets/icons-outline/house.svg assets/icons-outline/recent.svg
        assets/icons-outline/examples.svg assets/icons-outline/templates.svg
        assets/icons-outline/import.svg assets/icons-outline/help.svg
        assets/icons-outline/tutorials.svg assets/icons-outline/conceptual.svg
        assets/icons-outline/relational.svg assets/icons-outline/sql.svg
        assets/icons-outline/flow.svg assets/icons-outline/chevron-right.svg
        assets/icons-outline/chevron-down.svg assets/icons-outline/appearance.svg
        assets/icons-outline/sparkles.svg
        # The schema's Arrange, in the header of a schema drawn by hand.
        assets/icons-outline/layers.svg
        # The ribbon's tabs and the commands on the Design, Export, Import,
        # View and Help rows, named for what they stand for so another icon set
        # can draw its own under the same names.
        assets/icons-outline/view.svg assets/icons-outline/background.svg
        assets/icons-outline/icon-set.svg assets/icons-outline/notation.svg
        assets/icons-outline/lines.svg assets/icons-outline/project-file.svg
        assets/icons-outline/pdf-document.svg assets/icons-outline/data-dictionary.svg
        assets/icons-outline/html-report.svg assets/icons-outline/csv-listing.svg
        assets/icons-outline/svg-picture.svg assets/icons-outline/pdf-page.svg
        assets/icons-outline/more-pictures.svg assets/icons-outline/copy-picture.svg
        assets/icons-outline/project-picture.svg assets/icons-outline/other-tool.svg
        assets/icons-outline/actual-size.svg assets/icons-outline/zoom-in.svg
        assets/icons-outline/zoom-out.svg assets/icons-outline/grid.svg
        assets/icons-outline/align-to-grid.svg assets/icons-outline/canvas-controls.svg
        assets/icons-outline/comment.svg assets/icons-outline/history.svg
        assets/icons-outline/guide.svg assets/icons-outline/about.svg
        # The File, Home and Settings tabs (Settings is the Home screen's own
        # settings.svg, House its house.svg), Home's Insert, and the raft's
        # panel button when neither panel is showing.
        assets/icons-outline/file.svg assets/icons-outline/insert.svg
        assets/icons-outline/no-panels.svg
)

add_executable(erdflow app/desktop/main.cpp)
target_link_libraries(erdflow PRIVATE erdflow_desktop erdflow_infrastructure erdflow_warnings)

if(BUILD_TESTING)
    add_executable(editor_benchmark tests/editor_benchmark.cpp)
    target_link_libraries(editor_benchmark PRIVATE erdflow_canvas erdflow_infrastructure erdflow_warnings)

    add_executable(core_tests tests/core_tests.cpp)
    target_link_libraries(core_tests PRIVATE erdflow_application erdflow_warnings)
    add_test(NAME core COMMAND core_tests)

    add_executable(persistence_tests tests/persistence_tests.cpp)
    target_link_libraries(persistence_tests PRIVATE erdflow_infrastructure erdflow_warnings)
    add_test(NAME persistence COMMAND persistence_tests)

    add_executable(canvas_tests tests/canvas_tests.cpp)
    target_link_libraries(canvas_tests PRIVATE erdflow_canvas erdflow_warnings)
    add_test(NAME canvas COMMAND canvas_tests)

    add_executable(theme_tests tests/theme_tests.cpp)
    target_link_libraries(theme_tests PRIVATE erdflow_canvas erdflow_warnings)
    add_test(NAME theme COMMAND theme_tests)
    add_executable(desktop_tests tests/desktop_tests.cpp)
    target_link_libraries(desktop_tests PRIVATE erdflow_desktop erdflow_infrastructure erdflow_warnings)
    # The desktop suite runs as three parts, each a process with its own
    # timeout, rather than as one that had grown to within a few seconds of
    # its limit in Debug (2026-10-09). Together they are the whole suite;
    # run with no part named, desktop_tests still runs all of it.
    add_test(NAME desktop_export COMMAND desktop_tests --export-state)
    add_test(NAME desktop_relational COMMAND desktop_tests --relational-part)
    add_test(NAME desktop_window COMMAND desktop_tests --window-part)
    add_test(NAME desktop_smoke COMMAND erdflow --example --smoke-test)
    # Home and both workspaces photographed at the 1440 x 1080 reference and
    # held to pictures kept per platform, plus an audit of keyboard reach,
    # focus and contrast on Home (ADR-022 section 9.12). A platform with no
    # pictures of its own runs the audit and says the pictures were skipped.
    add_executable(visual_tests tests/visual_tests.cpp)
    target_link_libraries(visual_tests PRIVATE erdflow_desktop erdflow_infrastructure erdflow_warnings)
    target_compile_definitions(visual_tests PRIVATE
        ERDFLOW_VISUAL_BASELINES="${CMAKE_SOURCE_DIR}/tests/visual"
        ERDFLOW_VISUAL_OUTPUT="${CMAKE_BINARY_DIR}/visual-output")
    add_test(NAME visual COMMAND visual_tests)
    set(erdflow_offscreen_test_environment "QT_QPA_PLATFORM=offscreen")
    # On Windows the offscreen platform reads fonts only from QT_QPA_FONTDIR or
    # Qt's own lib/fonts, which Qt no longer ships, so it would draw no letters.
    if(WIN32)
        list(APPEND erdflow_offscreen_test_environment "QT_QPA_FONTDIR=C:/Windows/Fonts")
    endif()
    set_tests_properties(canvas theme desktop_export desktop_relational desktop_window desktop_smoke visual
                         PROPERTIES ENVIRONMENT "${erdflow_offscreen_test_environment}" TIMEOUT 30)
    # The desktop suite drives the whole window through both workspaces and
    # outgrew 30 s in Debug with the Schema Explorer's tests (Zain, 2026-09-29).
    # Each of its parts keeps the limit the whole suite had.
    set_tests_properties(desktop_export desktop_relational desktop_window PROPERTIES TIMEOUT 60)
endif()
