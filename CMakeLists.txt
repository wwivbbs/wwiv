# CMake for WWIV
cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
project(wwiv)

############################################################################### 
#
# Build variables to come from Jenkins, environment, etc.
# 

set(WWIV_RELEASE "5.5" CACHE STRING "WWIV Release Major Version to 2 digits")
set(WWIV_FULL_RELEASE "5.5.0" CACHE STRING "WWIV Release Version to 3 digits")
set(WWIV_ARCH "x86" CACHE STRING "x86 or x64")
set(NUM_BITS "32" CACHE STRING "32 or 64")

set(WWIV_INSTALL_SRC "${CMAKE_SOURCE_DIR}/install" CACHE STRING "By default this is: ${CMAKE_SOURCE_DIR}/install")
# TODO move this under the built root.
set(WWIV_RELEASE_DIR "${CMAKE_SOURCE_DIR}/release" CACHE STRING "By default this is: ${CMAKE_SOURCE_DIR}/release")
#set(MY_CACHE_VARIABLE "VALUE" CACHE STRING "Description")
# set WWIV_RELEASE_ARCHIVE_FILE=%WORKSPACE%\wwiv-win-%ARCH%-%WWIV_RELEASE%.%BUILD_NUMBER%.zip


list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include_directories(${CMAKE_SOURCE_DIR})

include(Common)
include(FindASan)

MACRO_ENSURE_OUT_OF_SOURCE_BUILD()
ENSURE_MINIMUM_COMPILER_VERSIONS()

if (WWIV_BUILD_TESTS)
  # Workaround gtest really wanting to compile with /Mtd vs /MD
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  # Since we don't include the top level googletest CMakeLists.txt
  # explicitly set the version.
  set(GOOGLETEST_VERSION 1.9.0)

  # googlemock also includes googletest
  add_subdirectory(deps/googletest/googlemock)

  enable_testing()
  find_package(GTest REQUIRED)
  find_package(GMock REQUIRED)
  message(STATUS "Using GoogleTest from ${GTEST_INCLUDE_DIRS}")
endif (WWIV_BUILD_TESTS)

# Cryptlib
add_subdirectory(deps/cl342)

# FMT
add_subdirectory(deps/fmt EXCLUDE_FROM_ALL)

if(WIN32)
  add_subdirectory(deps/pdcurses EXCLUDE_FROM_ALL)
endif()

add_subdirectory(core)
add_subdirectory(bbs)
add_subdirectory(binkp)
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
add_subdirectory(networkt)
add_subdirectory(sdk)
add_subdirectory(wwivconfig)
add_subdirectory(wwivd)
add_subdirectory(wwivutil)

if (WWIV_BUILD_TESTS)
  message (STATUS "WWIV_BUILD_TESTS is ON")
  add_subdirectory(bbs_test)
  add_subdirectory(binkp_test)
  add_subdirectory(core_test)
  add_subdirectory(net_core_test)
  add_subdirectory(sdk_test)
  add_subdirectory(wwivd_test)
	
endif (WWIV_BUILD_TESTS)

# Create build.nfo
message(STATUS "Writing ${CMAKE_BINARY_DIR}/BUILD.NFO")
file(
  WRITE "${CMAKE_BINARY_DIR}/build.nfo"
  "Build URL $ENV{BUILD_URL}\n"
  "Build Version: $ENV{BUILD_NUMBER}\n\n"
)