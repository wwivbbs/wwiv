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
    "wfndfile_unix.cpp"
  )
endif()

if(WIN32)

  target_sources(core PRIVATE
    "file_win32.cpp"
    "os_win.cpp"
    "pipe.cpp"
    "pipe_win32.cpp"
    "wfndfile_win32.cpp"
  )
endif()

if(OS2) 
  target_link_libraries(core libcx)
  target_sources(core PRIVATE
    "file_os2.cpp"
    "os_os2.cpp"
    "pipe.cpp"
    "pipe_os2.cpp"
    "wfndfile_os2.cpp"
  )
endif()


configure_file(version_internal.h.in version_internal.h @ONLY)

#target_compile_options(core PRIVATE  /fsanitize=address)
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

# Tests
if (WWIV_BUILD_TESTS)

  set_max_warnings()

  add_library(core_fixtures 
    file_helper.cpp 
  )
  target_link_libraries(core_fixtures core)
  add_executable(core_tests
    "core_test_main.cpp"
    "clock_test.cpp"
    "cp437_test.cpp"
    "crc32_test.cpp"
    "command_line_test.cpp"
    "datetime_test.cpp"
    "datafile_test.cpp"
    "eventbus_test.cpp"
    "fake_clock_test.cpp"
    "findfiles_test.cpp"
    "file_test.cpp"
    "inifile_test.cpp"
    "ip_address_test.cpp"
    "log_test.cpp"
    "md5_test.cpp"
    "os_test.cpp"
    "scope_exit_test.cpp"
    "semaphore_file_test.cpp"
    "stl_test.cpp"
    "strings_test.cpp"
    "textfile_test.cpp"
    "transaction_test.cpp"
    "uuid_test.cpp"
    "parser/ast_test.cpp"
    "parser/lexer_test.cpp"
  )

  target_link_libraries(core_tests core_fixtures core GTest::gtest)
  gtest_discover_tests(core_tests)

  if(WIN32)
    target_sources(core_tests PRIVATE
    "pipe_test.cpp"
    )
  endif()

  if(OS2)
    target_sources(core_tests PRIVATE
    "pipe_test.cpp"
    )
    target_link_libraries(core_tests libcx)
  endif()


endif()