# CMake for WWIV 5.0 NodeMgr
cmake_minimum_required (VERSION 2.6)
project (NODEMGR)
include_directories(..)

set(NODEMGR_SOURCES nodemgr.cpp)

add_executable(nodemgr ${NODEMGR_SOURCES})
target_link_libraries(nodemgr sdk core)
