cmake_minimum_required(VERSION 2.8)
project (SDK_TEST)

include_directories(../deps/gtest-1.7.0/include)
include_directories(..)

set(test_sources
  config_test.cpp
  network_test.cpp
  sdk_helper.cpp
)

if(UNIX) 
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_executable(sdk_tests ${test_sources})
target_link_libraries(sdk_tests core core_fixtures gtest_main sdk)

