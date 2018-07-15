enable_testing()

include_directories(../deps/googletest/googletest/include)
include_directories(..)

set(test_sources
  binkp_test.cpp
  binkp_config_test.cpp
  cram_test.cpp
  fake_connection.cpp
  transfer_file_test.cpp
  net_log_test.cpp
  net_util_test.cpp
  ppp_config_test.cpp
  networkb_test_main.cpp
)

add_executable(networkb_tests ${test_sources})
target_link_libraries(networkb_tests networkb_lib core core_fixtures gtest sdk)

add_test(networkb_tests networkb_tests)

