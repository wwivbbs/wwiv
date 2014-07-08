cmake_minimum_required(VERSION 2.8)
project (WWIV5)
include_directories(.)

include_directories(../deps/gtest-1.7.0/include)
include_directories(..)
include_directories(../bbs)
include_directories(../bbs/platform)

set(fixture_sources
  file_helper.cpp 
)

set(test_sources
  wfile_test.cpp
)

if(UNIX) 
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_library(core_fixtures ${fixture_sources}) 

add_executable(core_tests ${test_sources})
target_link_libraries(core_tests core_fixtures core gtest_main)

