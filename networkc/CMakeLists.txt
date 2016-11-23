# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (NETWORKC)
include_directories(..)

set(NETWORK_MAIN networkc.cpp)

add_executable(networkc ${NETWORK_MAIN})
target_link_libraries(networkc networkb_lib core sdk)

