# CMake for WWIV Daemon
cmake_minimum_required (VERSION 2.6)
project (wwivd)
include_directories(..)

set(WWIVD_SOURCES wwivd.cpp wwivd_unix.cpp)

add_executable(wwivd ${WWIVD_SOURCES})
target_link_libraries(wwivd sdk core)
