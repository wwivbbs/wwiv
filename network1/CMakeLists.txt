# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (NETWORK1)
include_directories(..)

set(NETWORK_MAIN network1.cpp)

add_executable(network1 ${NETWORK_MAIN})
target_link_libraries(network1 networkb_lib core sdk)

