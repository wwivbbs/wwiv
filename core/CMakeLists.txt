# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (CORE)
include_directories(..)

set(COMMON_SOURCES
  crc32.cpp
  command_line.cpp
  connection.cpp
  file.cpp
  file_lock.cpp
  findfiles.cpp
  graphs.cpp
  http_server.cpp
  inifile.cpp
  log.cpp
  md5.cpp
  net.cpp
  os.cpp
  semaphore_file.cpp
  socket_connection.cpp
  socket_exceptions.cpp
  strings.cpp
  textfile.cpp
  version.cpp
  ../deps/easylogging/easylogging++.cc
  )

if(UNIX) 
  set(PLATFORM_SOURCES 
    file_unix.cpp
    os_unix.cpp
    wfndfile_unix.cpp
    )
  add_definitions ("-Wall")
endif()

if(WIN32)
  message("Including deps/pdcurses")
  include_directories(../deps/pdcurses)

  set(PLATFORM_SOURCES 
    file_win32.cpp
    os_win.cpp
    wfndfile_win32.cpp
    )

endif()

if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()

add_library(core ${COMMON_SOURCES} ${PLATFORM_SOURCES})
