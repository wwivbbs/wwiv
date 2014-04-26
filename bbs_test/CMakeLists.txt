cmake_minimum_required(VERSION 2.8)
project (WWIV5)
include_directories(.)

include_directories(../deps/gtest-1.7.0/include)
include_directories(../bbs)
include_directories(../bbs/platform)

set(test_sources
  strings_test.cpp  stuffin_test.cpp  xfer_test.cpp
  # TODO(rushfan): Add wfile tests
)

if(UNIX) 
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()


add_executable(bbs_tests ${test_sources})
target_link_libraries(bbs_tests bbs_lib gtest_main)
