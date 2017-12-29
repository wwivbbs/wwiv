# CMake for WWIV
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
  include(FindWWIVCurses)

  set(PLATFORM_SOURCES 
    file_win32.cpp
    os_win.cpp
    wfndfile_win32.cpp
    )

  if(MSVC)
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_WINSOCK_DEPRECATED_NO_WARNINGS)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
      string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
  endif()

endif()


add_library(core ${COMMON_SOURCES} ${PLATFORM_SOURCES})
