# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (CORE)
#set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(..)

set(COMMON_SOURCES
  inifile.cpp
  file.cpp
  file_unix.cpp
  log.cpp
  os.cpp
  strings.cpp
  textfile.cpp
  )

if(UNIX) 
  set(PLATFORM_SOURCES 
	wfndfile_unix.cpp 
	)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_library(core ${COMMON_SOURCES} ${PLATFORM_SOURCES})
