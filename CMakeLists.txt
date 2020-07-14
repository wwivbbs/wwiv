############################################################################### 
#
# CMake for WWIV

cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
project(wwiv)

############################################################################### 
#
# Build variables to come from Jenkins, environment, etc.

set(WWIV_RELEASE "5.5.0" CACHE STRING "WWIV Release Major Version to 3 digits")
set(WWIV_BUILD_NUMBER "development" CACHE STRING "WWIV Build Number")
set(WWIV_FULL_RELEASE "${WWIV_RELEASE}.${WWIV_BUILD_NUMBER}" CACHE INTERNAL "WWIV Release Version, 4 digits with build number as 4th")
set(WWIV_ARCH "x86" CACHE STRING "x86 or x64")
if (WIN32)
  set(CPACK_PACKAGE_FILE_NAME "wwiv-win-${WWIV_ARCH}-${WWIV_FULL_RELEASE}")
endif()
if (LINUX)
  set(CPACK_PACKAGE_FILE_NAME "wwiv-linux-${WWIV_DISTRO}-${WWIV_FULL_RELEASE}")
endif()
set(WWIV_INSTALL_SRC "${CMAKE_SOURCE_DIR}/install" CACHE STRING "By default this is: ${CMAKE_SOURCE_DIR}/install")
set(WWIV_RELEASE_DIR "${CMAKE_BINARY_DIR}/release" CACHE STRING "By default this is: ${CMAKE_BINARY_DIR}/release")
#set(MY_CACHE_VARIABLE "VALUE" CACHE STRING "Description")
# set WWIV_RELEASE_ARCHIVE_FILE=%WORKSPACE%\wwiv-win-%ARCH%-%WWIV_RELEASE%.%BUILD_NUMBER%.zip
# Packaging support
set(CPACK_INCLUDE_TOPLEVEL_DIRECTORY OFF)
set(CPACK_PACKAGE_VENDOR "WWIV Software Services")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "WWIV Computer bulletin board system (BBS)")
set(CPACK_PACKAGE_VERSION_MAJOR "5")
set(CPACK_PACKAGE_VERSION_MINOR "5")
set(CPACK_PACKAGE_VERSION_PATCH "0")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README.md")
set(CPACK_SOURCE_GENERATOR "TGZ;ZIP")
include(CPack)

message(STATUS "WWIV Build Number: ${WWIV_FULL_RELEASE}")

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
  set(GOOGLETEST_VERSION 1.10.0)

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

create_datafile_archive("data" "${WWIV_INSTALL_SRC}/data")
create_datafile_archive("inifiles" "${WWIV_INSTALL_SRC}/inifiles")
create_datafile_archive("gfiles" "${WWIV_INSTALL_SRC}/gfiles")
create_datafile_archive("scripts" "${WWIV_INSTALL_SRC}/scripts")
create_datafile_archive("zip-city" "${WWIV_INSTALL_SRC}/zip-city")
create_datafile_archive("regions" "${WWIV_INSTALL_SRC}/regions")
if (UNIX)
  create_datafile_archive("unix" "${WWIV_INSTALL_SRC}/platform/unix")
endif()

install(TARGETS bbs DESTINATION .)
install(TARGETS network DESTINATION .)
install(TARGETS networkb DESTINATION .)
install(TARGETS networkc DESTINATION .)
install(TARGETS networkf DESTINATION .)
install(TARGETS networkt DESTINATION .)
install(TARGETS network1 DESTINATION .)
install(TARGETS network2 DESTINATION .)
install(TARGETS network3 DESTINATION .)
install(TARGETS wwivconfig DESTINATION .)
install(TARGETS wwivd DESTINATION .)
install(TARGETS wwivutil DESTINATION .)

if (UNIX)
  set(PLATFORM_DIR "${WWIV_INSTALL_SRC}/platform/unix")
endif()
if (WIN32)
  set(PLATFORM_DIR "${WWIV_INSTALL_SRC}/platform/win32")
endif()
file(GLOB PLATFORM_FILES "${PLATFORM_DIR}/*")
INSTALL(FILES ${PLATFORM_FILES} DESTINATION .)

file(GLOB DOCS_FILES "${WWIV_INSTALL_SRC}/docs/*")
install(FILES ${DOCS_FILES} DESTINATION .)
#install(DIRECTORY "${PLATFORM_DIR}" DESTINATION . FILES_MATCHING PATTERN "*")