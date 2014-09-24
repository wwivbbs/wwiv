# CMake for WWIV 5.0 NodeMgr
cmake_minimum_required (VERSION 2.6)
project (NODEMGR)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(..)

add_definitions(-DNOT_BBS)

set(NODEMGR_SOURCES nodemgr.cpp)

if(UNIX) 
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_executable(nodemgr ${NODEMGR_SOURCES})
target_link_libraries(nodemgr core)
