include(GoogleTest)
enable_testing()

include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIR})

set(test_sources
  binkp_test.cpp
  binkp_config_test.cpp
  cram_test.cpp
  fake_connection.cpp
  file_manager_test.cpp
  transfer_file_test.cpp
  net_log_test.cpp
  ppp_config_test.cpp
  binkp_test_main.cpp
)

set_max_warnings()

add_executable(binkp_tests ${test_sources})
target_link_libraries(binkp_tests binkp_lib core core_fixtures gmock gtest sdk)
 
gtest_discover_tests(binkp_tests)

