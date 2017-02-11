# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (CORE)
include_directories(..)

set(COMMON_SOURCES
  crc32.cpp
  command_line.cpp
  connection.cpp
  file.cpp
  file_unix.cpp
  findfiles.cpp
  graphs.cpp
  inifile.cpp
  log.cpp
  md5.cpp
  net.cpp
  os.cpp
  os_unix.cpp
  socket_connection.cpp
  socket_exceptions.cpp
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
