# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (INITLIB)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(..)

add_definitions(-DNOT_BBS)

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
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_library(initlib ${COMMON_SOURCES} ${PLATFORM_SOURCES})
