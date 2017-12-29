#
# FindWWIVCurses.
#
# On Windows, WWIV uses PDCurses. On all other platforms, use ncurses.

if(UNIX)
  message("FindWWIVCurses: Using NCurses")
  set(CURSES_NEED_NCURSES TRUE)
  find_package(Curses REQUIRED)
elseif(WIN32)
  # message("FindWWIVCurses: Using PDCurses")
  include_directories(${PDCURSES_INCLUDE_DIRS})
  # message("FindWWIVCurses: PDCURSES_INCLUDE_DIRS: ${PDCURSES_INCLUDE_DIRS}")
  # message("FindWWIVCurses: CURSES_LIBRARIES: ${CURSES_LIBRARIES}")
endif()
# message("FindWWIVCurses: Curses Library: ${CURSES_LIBRARIES}") 
