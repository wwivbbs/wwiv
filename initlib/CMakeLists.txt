# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (INITLIB)
include_directories(..)

set(COMMON_SOURCES
  colors.cpp
  curses_io.cpp
  curses_win.cpp
  input.cpp
  listbox.cpp
  )

if(UNIX) 
  find_package(Curses REQUIRED)
  message("Curses Library: ${CURSES_LIBRARIES}") 

  include_directories(platform/unix)
  set(PLATFORM_SOURCES 
	platform/unix/filestuff.cpp
	)
endif()

add_library(initlib ${COMMON_SOURCES} ${PLATFORM_SOURCES} ${CURSES_LIBRARIES})
target_link_libraries(initlib ${CURSES_LIBRARIES})
