cmake_minimum_required(VERSION 2.8)
project (WWIV5)

include_directories(../deps/googletest-master/googletest/include)
include_directories(..)
include_directories(../deps/cereal/include)

set(test_sources
  bbs_helper.cpp
  bputs_test.cpp
  bputch_test.cpp
  datetime_test.cpp
  make_abs_test.cpp
  new_bbslist_test.cpp
  pause_test.cpp
  printfile_test.cpp
  stuffin_test.cpp
  trashcan_test.cpp
  utility_test.cpp
  wutil_test.cpp
  xfer_test.cpp
)

if(UNIX) 
  if(APPLE)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework Foundation")
  endif()
endif()


add_executable(bbs_tests ${test_sources})
target_link_libraries(bbs_tests core bbs_lib core_fixtures gtest_main)
