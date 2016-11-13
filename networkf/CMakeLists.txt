# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (NETWORKF)
include_directories(..)

set(NETWORK_MAIN networkf.cpp)

add_executable(networkf ${NETWORK_MAIN})
target_link_libraries(networkf networkb_lib core sdk)

