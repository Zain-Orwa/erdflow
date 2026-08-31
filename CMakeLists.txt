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
