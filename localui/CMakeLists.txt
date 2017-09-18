# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (localui)
include_directories(..)
Include(FindWWIVCurses)

set(COMMON_SOURCES
  colors.cpp
  curses_io.cpp
  curses_win.cpp
  input.cpp
  listbox.cpp
  stdio_win.cpp
  )

add_library(localui ${COMMON_SOURCES} ${PLATFORM_SOURCES})
target_link_libraries(localui ${CURSES_LIBRARIES})
