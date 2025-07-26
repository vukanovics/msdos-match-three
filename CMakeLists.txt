cmake_minimum_required(VERSION 3.5)

project(mtchthre VERSION 1.0 LANGUAGES C)

set(CMAKE_SYSTEM_NAME linux-djgpp)
set (DJGPP TRUE)
set(CMAKE_C_COMPILER i686-pc-msdosdjgpp-gcc)
set(CMAKE_C_FLAGS "-I/usr/i686-pc-msdosdjgpp/sys-include/ -Wall -Wextra")

# search for programs in the build host directories
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

set (CMAKE_C_FLAGS -I${CMAKE_CURRENT_LIST_DIR}/djgpp/i586-pc-msdosdjgpp/sys-include/)
set (CMAKE_CXX_FLAGS -I${CMAKE_CURRENT_LIST_DIR}/djgpp/i586-pc-msdosdjgpp/sys-include/)

add_subdirectory(src)
