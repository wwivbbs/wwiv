cmake_minimum_required(VERSION 2.8)
project (SDK_TEST)

include_directories(../deps/googletest-master/googletest/include)
include_directories(..)

set(test_sources
  callout_test.cpp
  config_test.cpp
  contact_test.cpp
  datetime_test.cpp
  msgapi_test.cpp
  names_test.cpp
  network_test.cpp
  phone_numbers_test.cpp
  sdk_helper.cpp
  subxtr_test.cpp
  user_test.cpp
  fido/fido_address_test.cpp
)

add_executable(sdk_tests ${test_sources})
target_link_libraries(sdk_tests core core_fixtures gtest_main sdk)

