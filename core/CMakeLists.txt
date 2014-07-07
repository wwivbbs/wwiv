# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (CORE)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(..)

set(COMMON_SOURCES
  wstringutils.cpp
  platform/wutil.cpp
  )

if(UNIX) 
  set(PLATFORM_SOURCES 
	platform/unix/wfile.cpp 
	platform/unix/wfndfile.cpp 
	)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_library(core ${COMMON_SOURCES} ${PLATFORM_SOURCES})
