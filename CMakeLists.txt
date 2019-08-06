# CMake for WWIV
cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
project(wwiv)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(Common)
include(FindASan)

MACRO_ENSURE_OUT_OF_SOURCE_BUILD()

if (WWIV_BUILD_TESTS)
  # Workaround gtest really wanting to compile with /Mtd vs /MD
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  # googlemock also includes googletest
  add_subdirectory(deps/googletest/googlemock)

  enable_testing()
  find_package(GTest REQUIRED)
  find_package(GMock REQUIRED)
  message("Using GoogleTest from ${GTEST_INCLUDE_DIRS}")
endif (WWIV_BUILD_TESTS)

# Cryptlib
add_subdirectory(deps/cl342)

# FMT
add_subdirectory(deps/fmt EXCLUDE_FROM_ALL)

if(WIN32)
  add_subdirectory(deps/pdcurses)
endif()

add_subdirectory(core)
add_subdirectory(bbs)
add_subdirectory(local_io)
add_subdirectory(localui)
add_subdirectory(net_core)
add_subdirectory(network)
add_subdirectory(network1)
add_subdirectory(network2)
add_subdirectory(network3)
add_subdirectory(networkb)
add_subdirectory(networkc)
add_subdirectory(networkf)
add_subdirectory(sdk)
add_subdirectory(wwivconfig)
add_subdirectory(wwivd)
add_subdirectory(wwivutil)

if (WWIV_BUILD_TESTS)
  message (STATUS "WWIV_BUILD_TESTS is ON")
  add_subdirectory(bbs_test)
  add_subdirectory(core_test)
  add_subdirectory(net_core_test)
  add_subdirectory(networkb_test)
  add_subdirectory(sdk_test)
  add_subdirectory(wwivd_test)
	
endif (WWIV_BUILD_TESTS)
