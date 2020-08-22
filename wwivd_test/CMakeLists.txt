include(GoogleTest)
include_directories(${GTEST_INCLUDE_DIRS})

set(test_sources
  wwivd_non_http_test.cpp
)
list(APPEND test_sources wwivd_test_main.cpp)

set_max_warnings()
add_executable(wwivd_tests ${test_sources})
target_link_libraries(wwivd_tests wwivd_lib core_fixtures core sdk gtest)
gtest_discover_tests(wwivd_tests)
