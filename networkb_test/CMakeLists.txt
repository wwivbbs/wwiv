cmake_minimum_required(VERSION 2.8)
project (networkb_test)

include_directories(../deps/gtest-1.7.0/include)
include_directories(..)

set(test_sources
  binkp_test.cpp
  binkp_config_test.cpp
  fake_connection.cpp
  transfer_file_test.cpp
)

if(UNIX) 
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_executable(networkb_tests ${test_sources})
target_link_libraries(networkb_tests networkb_lib core core_fixtures gtest_main)

