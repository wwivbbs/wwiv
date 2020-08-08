# CMake for WWIV

set(COMMON_SOURCES
  clock.cpp
  crc32.cpp
  command_line.cpp
  connection.cpp
  datetime.cpp
  fake_clock.cpp
  file.cpp
  file_lock.cpp
  findfiles.cpp
  graphs.cpp
  http_server.cpp
  inifile.cpp
  ip_address.cpp
  log.cpp
  md5.cpp
  net.cpp
  os.cpp
  semaphore_file.cpp
  socket_connection.cpp
  socket_exceptions.cpp
  strcasestr.cpp
  strings.cpp
  textfile.cpp
  uuid.cpp
  version.cpp
  parser/ast.cpp
  parser/lexer.cpp
  parser/token.cpp
  )

if(UNIX) 
  set(PLATFORM_SOURCES 
    file_unix.cpp
    os_unix.cpp
    wfndfile_unix.cpp
    )
endif()

if(WIN32)
  include(FindWWIVCurses)

  set(PLATFORM_SOURCES 
    file_win32.cpp
    os_win.cpp
    wfndfile_win32.cpp
    )
endif()

set_max_warnings()

configure_file(version_internal.h.in version_internal.h @ONLY)

add_library(core ${COMMON_SOURCES} ${PLATFORM_SOURCES})
target_link_libraries(core fmt::fmt-header-only)
target_include_directories(core PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
if (UNIX)
if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # using regular Clang or AppleClang
	target_link_libraries(core c++fs)
else()
	target_link_libraries(core stdc++fs)
endif()
endif()

