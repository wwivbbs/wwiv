cmake_minimum_required(VERSION 2.8)
project (core_test)

include_directories(../deps/gtest-1.7.0/include)
include_directories(..)

set(fixture_sources
  file_helper.cpp 
)

set(test_sources
  inifile_test.cpp
  scope_exit_test.cpp
  stl_test.cpp
  strings_test.cpp
  transaction_test.cpp
  datafile_test.cpp
  file_test.cpp
  textfile_test.cpp
)

add_library(core_fixtures ${fixture_sources}) 

add_executable(core_tests ${test_sources})
target_link_libraries(core_fixtures core)
target_link_libraries(core_tests core_fixtures core gtest_main)

