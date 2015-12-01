# CMake for WWIV 5.0
cmake_minimum_required (VERSION 3.0)
project (WWIV5All)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(Common)
include(FindASan)

add_subdirectory(deps/gtest-1.7.0)
add_subdirectory(core)
add_subdirectory(core_test)
add_subdirectory(bbs)
add_subdirectory(bbs_test)
add_subdirectory(fix)
add_subdirectory(init)
add_subdirectory(initlib)
add_subdirectory(netutil)
add_subdirectory(network)
add_subdirectory(networkb)
add_subdirectory(networkb_test)
add_subdirectory(nodemgr)
add_subdirectory(sdk)
add_subdirectory(sdk_test)
