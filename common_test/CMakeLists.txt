include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIR})
include(GoogleTest)

set(test_sources
  "acs_test.cpp"
  "input_range_test.cpp"
  "common_test_main.cpp"
  "menu_data_util_test.cpp"
  "pipe_expr_test.cpp"
  "remote_socket_io_test.cpp"
)

set_max_warnings()

set(fixture_sources
  common_helper.cpp 
)

add_library(common_fixtures ${fixture_sources})
target_link_libraries(common_fixtures common core core_fixtures)

add_executable(common_tests ${test_sources})
target_link_libraries(common_tests common_fixtures common gmock gtest)
gtest_discover_tests(common_tests)
