# CMake for WWIV
Include(FindWWIVCurses)

set(COMMON_SOURCES 
 "bgetch.cpp"
 "checka.cpp"
 "com.cpp"
 "context.cpp"
 "datetime.cpp"
 "full_screen.cpp"
 "input.cpp"
 "input_range.cpp"
 "iobase.cpp"
 "language.cpp"
 "macro_context.cpp"
 "menus/menu_data_util.cpp"
 "menus/menu_generator.cpp"
 "message_editor_data.cpp"
 "null_remote_io.cpp"
 "output.cpp"
 "pause.cpp"
 "pipe_expr.cpp"
 "printfile.cpp"
 "quote.cpp"
 "remote_io.cpp"
 "remote_socket_io.cpp"
 "value/bbsvalueprovider.cpp"
 "value/uservalueprovider.cpp"
 "workspace.cpp"
)

add_library(
  common 
  ${COMMON_SOURCES} 
) 

target_link_libraries(
  common 
  fmt::fmt-header-only
  local_io 
  core 
  sdk 
)

if (OS2 OR WIN32)
  target_sources(common PRIVATE "remote_pipe_io.cpp")
endif()

set_max_warnings(common)

target_include_directories(common PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Tests

if (WWIV_BUILD_TESTS)

  set(common_test_sources
    "acs_test.cpp"
    "input_range_test.cpp"
    "common_test_main.cpp"
    "menu_data_util_test.cpp"
    "pipe_expr_test.cpp"
    "remote_socket_io_test.cpp"
  )

  set(common_fixture_sources
    common_helper.cpp 
  )

  add_library(common_fixtures ${common_fixture_sources})
  target_link_libraries(common_fixtures common core core_fixtures)

  add_executable(common_tests ${common_test_sources})
  target_link_libraries(common_tests common_fixtures common GTest::gmock GTest::gtest)
  gtest_discover_tests(common_tests)

endif()
