set (CMAKE_SYSTEM_NAME linux-djgpp)

set (DJGPP TRUE)

# specify the cross compiler
set (CMAKE_C_COMPILER i686-pc-msdosdjgpp-gcc)
set (CMAKE_CXX_COMPILER i686-pc-msdosdjgpp-g++)

# where is the target environment
set (CMAKE_FIND_ROOT_PATH /usr/lib/gcc/i686-pc-msdosdjgpp)

# search for programs in the build host directories
set (CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# for libraries and headers in the target directories
set (CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set (CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# DOSBox 0.74-3's CPU core has no CMOV, which the DJGPP cross compiler emits
# by default (-march=pentiumpro), so an unmodified build dies with an invalid
# opcode. Target i386, which also keeps the game runnable on period hardware.
set (CMAKE_C_FLAGS "-march=i386 -I/usr/i686-pc-msdosdjgpp/sys-include/")
set (CMAKE_CXX_FLAGS "-march=i386 -I/usr/i686-pc-msdosdjgpp/sys-include/")
