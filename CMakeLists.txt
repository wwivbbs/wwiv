# CMake for WWIV
cmake_minimum_required(VERSION 3.0 FATAL_ERROR)
project(wwiv)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(Common)
include(FindASan)

# Workaround gtest really wanting to compile with /Mtd vs /MD
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# googlemock also includes googletest
add_subdirectory(deps/googletest/googlemock)

enable_testing()
find_package(GTest REQUIRED)
find_package(GMock REQUIRED)
message("Using GoogleTest from ${GTEST_INCLUDE_DIRS}")

# Setup benchmark
#set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
#add_subdirectory(deps/benchmark)
#find_package(benchmark REQUIRED)

# cctz is needed by abseil-cpp
#add_subdirectory(deps/cctz) 
# abseil is the Google C++ libraries open sourced.
#add_subdirectory(deps/abseil-cpp)

# Cryptlib
add_subdirectory(deps/cl342)

if(WIN32)
add_subdirectory(deps/pdcurses)
endif()

add_subdirectory(core)
add_subdirectory(core_test)
add_subdirectory(bbs)
add_subdirectory(bbs_test)
add_subdirectory(init)
add_subdirectory(localui)
add_subdirectory(network)
add_subdirectory(network1)
add_subdirectory(network2)
add_subdirectory(network3)
add_subdirectory(networkb)
add_subdirectory(networkc)
add_subdirectory(networkf)
add_subdirectory(networkb_test)
add_subdirectory(sdk)
add_subdirectory(sdk_test)
add_subdirectory(wwivd)
add_subdirectory(wwivutil)

