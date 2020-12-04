# CMake for WWIV 5
Include(FindWWIVCurses)

set(COMMON_SOURCES
  "colors.cpp"
  "curses_io.cpp"
  "curses_win.cpp"
  "edit_items.cpp"
  "input.cpp"
  "listbox.cpp"
  "stdio_win.cpp"
  )
set_max_warnings()

add_library(localui ${COMMON_SOURCES} ${PLATFORM_SOURCES})
target_link_libraries(localui ${CURSES_LIBRARIES} fmt::fmt-header-only)
