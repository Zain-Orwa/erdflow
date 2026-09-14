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

add_library(erdflow_domain domain/model.cpp)
target_include_directories(erdflow_domain PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(erdflow_domain PRIVATE erdflow_warnings)

add_library(erdflow_application application/editor.cpp)
target_link_libraries(erdflow_application PUBLIC erdflow_domain PRIVATE erdflow_warnings)

add_library(erdflow_infrastructure infrastructure/project_store.cpp)
target_link_libraries(erdflow_infrastructure PUBLIC erdflow_application Qt6::Core PRIVATE erdflow_warnings)


add_library(erdflow_canvas app/desktop/diagram_view.cpp app/desktop/theme.cpp)
target_link_libraries(erdflow_canvas PUBLIC erdflow_application Qt6::Widgets PRIVATE erdflow_warnings)

add_library(erdflow_desktop app/desktop/main_window.cpp app/desktop/icons.cpp)
target_link_libraries(erdflow_desktop PUBLIC erdflow_canvas Qt6::Svg PRIVATE erdflow_warnings)
# The icon set is compiled in rather than read from disk, so a built ERDFlow is
# one file that cannot be started without its own artwork. Qt6::Svg brings the
# renderer that turns those files into icons.
qt_add_resources(erdflow_desktop "erdflow_icons"
    PREFIX "/erdflow/icons"
    BASE "assets/icons"
    FILES
        assets/icons/select.svg assets/icons/entity.svg assets/icons/attribute.svg
        assets/icons/relationship.svg assets/icons/isa.svg assets/icons/connect.svg
        assets/icons/note.svg assets/icons/comment.svg assets/icons/pan.svg
        assets/icons/zoom.svg assets/icons/new-project.svg assets/icons/open.svg
        assets/icons/save.svg assets/icons/import.svg assets/icons/export.svg
        assets/icons/undo.svg assets/icons/redo.svg assets/icons/duplicate.svg
        assets/icons/delete.svg assets/icons/search.svg assets/icons/conceptual.svg
        assets/icons/schema.svg assets/icons/physical.svg assets/icons/sql.svg
        assets/icons/data.svg assets/icons/explorer.svg assets/icons/properties.svg
        assets/icons/validate.svg assets/icons/theme.svg assets/icons/settings.svg
)

qt_add_resources(erdflow_desktop "erdflow_icons_on_dark"
    PREFIX "/erdflow/icons-on-dark"
    BASE "assets/icons-on-dark"
    FILES
        assets/icons-on-dark/select.svg assets/icons-on-dark/entity.svg assets/icons-on-dark/attribute.svg
        assets/icons-on-dark/relationship.svg assets/icons-on-dark/isa.svg assets/icons-on-dark/connect.svg
        assets/icons-on-dark/note.svg assets/icons-on-dark/comment.svg assets/icons-on-dark/pan.svg
        assets/icons-on-dark/zoom.svg assets/icons-on-dark/new-project.svg assets/icons-on-dark/open.svg
        assets/icons-on-dark/save.svg assets/icons-on-dark/import.svg assets/icons-on-dark/export.svg
        assets/icons-on-dark/undo.svg assets/icons-on-dark/redo.svg assets/icons-on-dark/duplicate.svg
        assets/icons-on-dark/delete.svg assets/icons-on-dark/search.svg assets/icons-on-dark/conceptual.svg
        assets/icons-on-dark/schema.svg assets/icons-on-dark/physical.svg assets/icons-on-dark/sql.svg
        assets/icons-on-dark/data.svg assets/icons-on-dark/explorer.svg assets/icons-on-dark/properties.svg
        assets/icons-on-dark/validate.svg assets/icons-on-dark/theme.svg assets/icons-on-dark/settings.svg
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
    add_test(NAME desktop COMMAND desktop_tests)
    add_test(NAME desktop_smoke COMMAND erdflow --example --smoke-test)
    set_tests_properties(canvas theme desktop desktop_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)
endif()
