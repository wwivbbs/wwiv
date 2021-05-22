# CMake for WWIV

add_library(core
  "clock.cpp"
  "cp437.cpp"
  "crc32.cpp"
  "command_line.cpp"
  "connection.cpp"
  "datetime.cpp"
  "eventbus.cpp"
  "fake_clock.cpp"
  "file.cpp"
  "file_lock.cpp"
  "findfiles.cpp"
  "graphs.cpp"
  "http_server.cpp"
  "inifile.cpp"
  "ip_address.cpp"
  "jsonfile.cpp"
  "log.cpp"
  "md5.cpp"
  "net.cpp"
  "os.cpp"
  "pipe.cpp"
  "semaphore_file.cpp"
  "socket_connection.cpp"
  "socket_exceptions.cpp"
  "strcasestr.cpp"
  "strings.cpp"
  "textfile.cpp"
  "uuid.cpp"
  "version.cpp"
  "parser/ast.cpp"
  "parser/lexer.cpp"
  "parser/token.cpp"
  )

if(UNIX) 
  target_sources(core PRIVATE
    "file_unix.cpp"
    "os_unix.cpp"
    "pipe_unix.cpp"
    "wfndfile_unix.cpp"
  )
endif()

if(WIN32)
  include(FindWWIVCurses)

  target_sources(core PRIVATE
    "file_win32.cpp"
    "os_win.cpp"
    "pipe_win32.cpp"
    "wfndfile_win32.cpp"
  )
endif()

if(OS2) 
  target_link_libraries(core libcx)
  target_sources(core PRIVATE
    "file_os2.cpp"
    "os_os2.cpp"
    "pipe_os2.cpp"
    "wfndfile_os2.cpp"
  )
endif()

set_max_warnings()

configure_file(version_internal.h.in version_internal.h @ONLY)

#target_compile_options(core PRIVATE  /fsanitize=address)
target_link_libraries(core fmt::fmt-header-only)
target_include_directories(core PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(core PUBLIC ../deps/cereal/include)

if (UNIX)
  if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    # using regular Clang or AppleClang
  	target_link_libraries(core c++fs)
  else()
  	target_link_libraries(core stdc++fs)
  endif()
endif()

