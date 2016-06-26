# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (CORE)
include_directories(..)

set(COMMON_SOURCES
  command_line.cpp
  inifile.cpp
  file.cpp
  file_unix.cpp
  graphs.cpp
  log.cpp
  md5.cpp
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
  add_definitions ("-Wall")
endif()

add_library(core ${COMMON_SOURCES} ${PLATFORM_SOURCES})
