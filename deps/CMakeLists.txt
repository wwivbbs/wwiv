# CMake targets for third-party code under deps/ that does not ship a usable
# CMakeLists.txt of its own. These are defined here, rather than inside the
# submodules, so the submodules stay unmodified.
#
# pdcurses and cl345 have their own CMakeLists.txt and are added from the top
# level CMakeLists.txt.

# My-Basic: the BASIC interpreter used by bbs/basic.
add_library(my_basic STATIC my_basic/core/my_basic.c)
# Code includes it as "deps/my_basic/core/my_basic.h", so expose the source root.
target_include_directories(my_basic PUBLIC "${PROJECT_SOURCE_DIR}")
