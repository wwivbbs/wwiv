cmake_minimum_required(VERSION 2.8)
project (networkb_test)

include_directories(../deps/gtest-1.7.0/include)
include_directories(..)

set(test_sources
  binkp_test.cpp
  binkp_config_test.cpp
  callout_test.cpp
  fake_connection.cpp
  transfer_file_test.cpp
  net_log_test.cpp
  ppp_config_test.cpp
)

add_executable(networkb_tests ${test_sources})
target_link_libraries(networkb_tests networkb_lib core core_fixtures gtest_main sdk)

