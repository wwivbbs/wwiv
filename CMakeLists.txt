############################################################################### 
#
# CMake for WWIV

cmake_minimum_required(VERSION 3.13 FATAL_ERROR)
project(wwiv)


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
if (WWIV_SSH_CRYPTLIB AND NOT OS2)
add_subdirectory(deps/cl345)
endif()

# FMT
add_subdirectory(deps/fmt EXCLUDE_FROM_ALL)

if(WIN32 OR OS2)
  # We only use pdcurses on Win32
  add_definitions(-DPDC_WIDE)
  add_subdirectory(deps/pdcurses EXCLUDE_FROM_ALL)
endif()

add_subdirectory(bbs)
add_subdirectory(binkp)
add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(fsed)
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
add_subdirectory(wwivfsed)
add_subdirectory(wwivutil)

if (WWIV_BUILD_TESTS)
  message (STATUS "WWIV_BUILD_TESTS is ON")
  add_subdirectory(bbs_test)
  add_subdirectory(binkp_test)
  add_subdirectory(common_test)
  add_subdirectory(core_test)
  add_subdirectory(net_core_test)
  add_subdirectory(sdk_test)
  add_subdirectory(wwivd_test)
endif (WWIV_BUILD_TESTS)

if (WWIV_INSTALL)
  # Create build.nfo
  message(STATUS "Writing ${CMAKE_BINARY_DIR}/BUILD.NFO")
  file(
    WRITE "${CMAKE_BINARY_DIR}/build.nfo"
    "Build URL $ENV{BUILD_URL}\n"
    "Build Version: $ENV{BUILD_NUMBER}\n\n"
  )

  if(WWIV_ZIP_INSTALL_FILES)
    create_datafile_archive("data" "${WWIV_INSTALL_SRC}/data")
    create_datafile_archive("inifiles" "${WWIV_INSTALL_SRC}/inifiles")
    create_datafile_archive("gfiles" "${WWIV_INSTALL_SRC}/gfiles")
    create_datafile_archive("menus" "${WWIV_INSTALL_SRC}/menus")
    create_datafile_archive("scripts" "${WWIV_INSTALL_SRC}/scripts")
    create_datafile_archive("zip-city" "${WWIV_INSTALL_SRC}/zip-city")
    create_datafile_archive("regions" "${WWIV_INSTALL_SRC}/regions")
    if (UNIX)
      create_datafile_archive("unix" "${WWIV_INSTALL_SRC}/platform/unix")
    endif()
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
  install(TARGETS wwivfsed DESTINATION .)
  install(TARGETS wwivd DESTINATION .)
  install(TARGETS wwivutil DESTINATION .)

  if (UNIX)
    set(PLATFORM_DIR "${WWIV_INSTALL_SRC}/platform/unix")
    # Copy shell scripts, rest can be from unix.zip
    file(GLOB PLATFORM_FILES "${PLATFORM_DIR}/*.sh" "${PLATFORM_DIR}/*.bash")
    foreach(file ${PLATFORM_FILES})
      message(DEBUG "Installing ${file}")
      INSTALL(FILES "${file}" 
              PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
              DESTINATION .)
    endforeach()
  elseif(WIN32)
    set(PLATFORM_DIR "${WWIV_INSTALL_SRC}/platform/win32")
    file(GLOB PLATFORM_FILES "${PLATFORM_DIR}/*")
    foreach(file ${PLATFORM_FILES})
      message(DEBUG "Installing ${file}")
      INSTALL(FILES "${file}" DESTINATION .)
    endforeach()
  endif()

  file(GLOB DOCS_FILES "${WWIV_INSTALL_SRC}/docs/*")
  foreach(file ${DOCS_FILES})
    INSTALL(FILES "${file}" DESTINATION .)
  endforeach()
endif (WWIV_INSTALL)
