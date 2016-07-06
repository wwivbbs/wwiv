# CMake for WWIV
cmake_minimum_required (VERSION 2.6)
project (WWIV5All)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(Common)
include(FindASan)

add_subdirectory(deps/cl342)
add_subdirectory(deps/googletest-master/googletest)
add_subdirectory(core)
add_subdirectory(core_test)
add_subdirectory(bbs)
add_subdirectory(bbs_test)
add_subdirectory(init)
add_subdirectory(localui)
add_subdirectory(network)
add_subdirectory(network3)
add_subdirectory(networkb)
add_subdirectory(networkb_test)
add_subdirectory(nodemgr)
add_subdirectory(sdk)
add_subdirectory(sdk_test)
add_subdirectory(wwivd)
add_subdirectory(wwivutil)

