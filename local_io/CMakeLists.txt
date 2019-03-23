# CMake for WWIV
include_directories(..)
Include(FindWWIVCurses)

set(COMMON_SOURCES
  local_io.cpp
  local_io_curses.cpp
  null_local_io.cpp
  stdio_local_io.cpp
  )

  if(WIN32)
  set(PLATFORM_SOURCES
		local_io_win32.cpp
	)
    SET_MSVC_WARNING_LEVEL_4()
endif()

add_library(local_io ${COMMON_SOURCES} ${PLATFORM_SOURCES})
target_link_libraries(local_io ${CURSES_LIBRARIES} localui )
