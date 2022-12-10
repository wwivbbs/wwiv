#
# FindWWIVCurses.
#
# On Windows, WWIV uses PDCurses. On all other platforms, use ncurses.

if(UNIX)
  # message("FindWWIVCurses: Using NCurses")
  set(CURSES_NEED_NCURSES TRUE)
  set(CURSES_NEED_WIDE TRUE)
  find_package(Curses REQUIRED)
elseif(WIN32 OR OS2)
  # message("FindWWIVCurses: Using PDCurses")
  include_directories(${PDCURSES_INCLUDE_DIRS})
  # message("FindWWIVCurses: PDCURSES_INCLUDE_DIRS: ${PDCURSES_INCLUDE_DIRS}")
  message(VERBOSE "FindWWIVCurses: CURSES_LIBRARIES: ${CURSES_LIBRARIES}")
endif()
# message("FindWWIVCurses: Curses Library: ${CURSES_LIBRARIES}") 
