include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIR})
include(GoogleTest)

set(test_sources
  input_range_test.cpp
  common_test_main.cpp
)

set_max_warnings()
add_executable(common_tests ${test_sources})
target_link_libraries(common_tests common gmock gtest)
gtest_discover_tests(common_tests)
