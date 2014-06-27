# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (INITLIB)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(..)
include_directories(platform)

add_definitions(-DNOT_BBS)

set(COMMON_SOURCES
  input.cpp
  platform/curses_io.cpp
  )

if(UNIX) 
  find_package(Curses REQUIRED)
  message("Curses Library: ${CURSES_LIBRARIES}") 

  include_directories(platform/unix)
  set(PLATFORM_SOURCES 
	platform/unix/filestuff.cpp
	platform/unix/stringstuff.cpp
	platform/unix/utility2.cpp 
	platform/unix/wfile.cpp 
	platform/unix/wfndfile.cpp 
	)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_library(initlib ${COMMON_SOURCES} ${PLATFORM_SOURCES})
