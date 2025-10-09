cmake_minimum_required(VERSION 3.16)
project(glbox_lib)

# Header-only library
add_library(glbox INTERFACE)

target_include_directories(glbox INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}          # glbox/
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/glad/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/stb
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/imgui
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/imgui/backends
)
