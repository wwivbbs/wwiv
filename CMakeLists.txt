# CMake for WWIV
cmake_minimum_required(VERSION 3.9 FATAL_ERROR)
project(wwiv)

############################################################################### 
#
# Build variables to come from Jenkins, environment, etc.
# 

set(WWIV_RELEASE "5.5" CACHE STRING "WWIV Release Major Version to 2 digits")
set(WWIV_BUILD_NUMBER "development" CACHE STRING "WWIV Build Number")
set(WWIV_FULL_RELEASE "${WWIV_RELEASE}.${WWIV_BUILD_NUMBER}" CACHE STRING "WWIV Release Version to 3 digits")
set(WWIV_ARCH "x86" CACHE STRING "x86 or x64")
set(NUM_BITS "32" CACHE STRING "32 or 64")

set(WWIV_INSTALL_SRC "${CMAKE_SOURCE_DIR}/install" CACHE STRING "By default this is: ${CMAKE_SOURCE_DIR}/install")
# TODO move this under the built root.
set(WWIV_RELEASE_DIR "${CMAKE_BINARY_DIR}/release" CACHE STRING "By default this is: ${CMAKE_BINARY_DIR}/release")
#set(MY_CACHE_VARIABLE "VALUE" CACHE STRING "Description")
# set WWIV_RELEASE_ARCHIVE_FILE=%WORKSPACE%\wwiv-win-%ARCH%-%WWIV_RELEASE%.%BUILD_NUMBER%.zip

message(STATUS "WWIV Build Number: ${WWIV_FULL_RELEASE} : ${WWIV_BUILD_NUMBER}")

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

function(zip output_file input_files working_dir)
  # message(STATUS "zip: ${output_file} : ${input_files}")
  add_custom_command(
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${output_file}" --format=zip -- ${input_files}
    WORKING_DIRECTORY "${working_dir}"
    OUTPUT  "${output_file}"
    DEPENDS ${input_files}
    COMMENT "Creating ZIP file: ${output_file}."
    )
endfunction()

function(create_datafile_archive arc dir)
  message(STATUS "create_datafile_archive: dir: ${dir}: ${WWIV_RELEASE_DIR}/${arc}.zip")
  file(GLOB_RECURSE DATA_FILES "${dir}/*")
  zip("${WWIV_RELEASE_DIR}/${arc}.zip" "${DATA_FILES}" "${dir}/")
  add_custom_target("${arc}_archive" ALL DEPENDS "${WWIV_RELEASE_DIR}/${arc}.zip")
endfunction()

create_datafile_archive("data" "${WWIV_INSTALL_SRC}/data")
create_datafile_archive("inifiles" "${WWIV_INSTALL_SRC}/inifiles")
create_datafile_archive("gfiles" "${WWIV_INSTALL_SRC}/gfiles")
create_datafile_archive("scripts" "${WWIV_INSTALL_SRC}/scripts")
create_datafile_archive("zip-city" "${WWIV_INSTALL_SRC}/zip-city")
create_datafile_archive("regions" "${WWIV_INSTALL_SRC}/regions")
if (UNIX)
  create_datafile_archive("unix" "${WWIV_INSTALL_SRC}/platform/unix")
endif()
