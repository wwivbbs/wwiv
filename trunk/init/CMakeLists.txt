# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (INIT)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(BBS_SOURCE_DIR  ../bbs)
include_directories(platform)
include_directories(${BBS_SOURCE_DIR})

add_definitions(-DINIT -DNOT_BBS)


set(COMMON_SOURCES
  convert.cpp
  init.cpp init1.cpp init2.cpp init3.cpp
  input.cpp
  user_editor.cpp regcode.cpp
  subacc.cpp platform/curses_io.cpp
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
#include_directories(${BBS_SOURCE_DIR}/platform)

add_executable(init ${COMMON_SOURCES} ${PLATFORM_SOURCES})
target_link_libraries(init ${CURSES_LIBRARIES})
