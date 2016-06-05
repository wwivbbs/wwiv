# CMake for WWIV 5.0 NodeMgr
cmake_minimum_required (VERSION 2.6)
project (wwivd)
include_directories(..)

set(WWIVD_SOURCES wwivd.cpp)

add_executable(wwivd ${WWIVD_SOURCES})
target_link_libraries(wwivd sdk core)
