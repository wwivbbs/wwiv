# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (NETWORK)
include_directories(..)

set(NETWORK_MAIN network.cpp)

add_executable(network ${NETWORK_MAIN})
target_link_libraries(network networkb_lib core sdk)

