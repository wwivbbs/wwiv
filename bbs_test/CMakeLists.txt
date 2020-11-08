
include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIR})
include_directories(../deps/cereal/include)
include(GoogleTest)

set(test_sources
  bbs_helper.cpp
  bputs_test.cpp
  bputch_test.cpp
  datetime_test.cpp
  dsz_test.cpp
  email_test.cpp
  input_test.cpp
  make_abs_test.cpp
  msgbase1_test.cpp
  new_bbslist_test.cpp
  pause_test.cpp
  printfile_test.cpp
  quote_test.cpp
  qwk_test.cpp
  stuffin_test.cpp
  trashcan_test.cpp
  utility_test.cpp
  wutil_test.cpp
  xfer_test.cpp
  basic/basic_test.cpp
  basic/util_test.cpp
  fsed/fsed_model_test.cpp
)

if(UNIX) 
  if(APPLE)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework Foundation")
  endif()
  if(CMAKE_SYSTEM_NAME MATCHES "SunOS.*")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lsocket -lnsl -lkstat -lssp")
  endif()
else()
  list(APPEND test_sources bbs_test_main.cpp)
endif()


add_executable(bbs_tests ${test_sources})
if(UNIX)
  target_link_libraries(bbs_tests core bbs_lib core_fixtures gtest_main)
else()
  target_link_libraries(bbs_tests core bbs_lib core_fixtures gtest)
endif()

gtest_discover_tests(bbs_tests)
