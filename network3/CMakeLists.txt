# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (NETWORK3)
include_directories(..)

set(NETWORK_MAIN network3.cpp)

add_executable(network3 ${NETWORK_MAIN})
target_link_libraries(network3 networkb_lib core sdk)

