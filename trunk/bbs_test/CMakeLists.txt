cmake_minimum_required(VERSION 2.8)
project (WWIV5)
include_directories(.)

include_directories(../deps/gtest-1.7.0/include)
include_directories(..)
include_directories(../bbs)
include_directories(../bbs/platform)

set(test_sources
  bbs_helper.cpp file_helper.cpp 
  printfile_test.cpp strings_test.cpp  stuffin_test.cpp  utility_test.cpp
  wfile_test.cpp wutil_test.cpp xfer_test.cpp
)

if(UNIX) 
  if(APPLE)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework Foundation")
  endif()
    SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()


add_executable(bbs_tests ${test_sources})
target_link_libraries(bbs_tests bbs_lib core gtest_main)
