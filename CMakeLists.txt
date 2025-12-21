cmake_minimum_required(VERSION 3.5)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

project(mtchthre VERSION 1.0 LANGUAGES C)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

set (CMAKE_EXPORT_COMPILE_COMMANDS true)

add_subdirectory(external)
add_subdirectory(src)
add_subdirectory(tests)
