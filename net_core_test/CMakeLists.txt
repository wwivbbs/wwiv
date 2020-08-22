include(GoogleTest)
enable_testing()

include_directories(${GTEST_INCLUDE_DIRS})

set(test_sources
  net_cmdline_test.cpp
  net_core_test_main.cpp
)
set_max_warnings()

add_executable(net_core_tests ${test_sources})
target_link_libraries(net_core_tests net_core core_fixtures gtest sdk)

gtest_discover_tests(net_core_tests)
