# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (CORE)
include_directories(..)

set(COMMON_SOURCES
  inifile.cpp
  file.cpp
  file_unix.cpp
  log.cpp
  os.cpp
  os_unix.cpp
  strings.cpp
  textfile.cpp
  version.cpp
  )

if(UNIX) 
  set(PLATFORM_SOURCES 
    wfndfile_unix.cpp
    )
endif()

add_library(core ${COMMON_SOURCES} ${PLATFORM_SOURCES})
