############################################################################### 
#
# CMake for WWIV

cmake_minimum_required(VERSION 3.15 FATAL_ERROR)

if(POLICY CMP0092)
  # MSVC warning flags are not in CMAKE_<LANG>_FLAGS by default.
  cmake_policy(SET CMP0092 NEW)
endif()

set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake
  CACHE STRING "Vcpkg toolchain file")

project(wwiv)


list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

# Set the current root as the Include directory for the compiler,
# since WWIV uses include paths like "core/foo.h"
include_directories(${CMAKE_SOURCE_DIR})

include(Common)
include(FindASan)
find_package(fmt CONFIG REQUIRED)
find_package(cereal CONFIG REQUIRED)

MACRO_ENSURE_OUT_OF_SOURCE_BUILD()
ENSURE_MINIMUM_COMPILER_VERSIONS()

if(WWIV_ASAN_ENABLED)
  string(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  message(STATUS "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}\n")
  add_compile_options(-fsanitize=address)
endif()

if (WWIV_BUILD_TESTS)
  message (STATUS "WWIV_BUILD_TESTS is ON")
  find_package(GTest CONFIG REQUIRED)

  # Workaround gtest really wanting to compile with /Mtd vs /MD
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  enable_testing()
  include(GoogleTest)
endif (WWIV_BUILD_TESTS)

# Cryptlib
if (WWIV_SSH_CRYPTLIB AND NOT OS2)
add_subdirectory(deps/cl345)
endif()


if(WIN32 OR OS2)
  # We only use pdcurses on Win32
  add_definitions(-DPDC_WIDE)
  add_subdirectory(deps/pdcurses EXCLUDE_FROM_ALL)
endif()

#if (NOT OS2)
set(WWIV_BUILD_WWIVD ON)
#endif()
add_subdirectory(bbs)
add_subdirectory(binkp)
add_subdirectory(common)
add_subdirectory(core)
add_subdirectory(fsed)
add_subdirectory(lnet)
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

if (WWIV_BUILD_WWIVD)
add_subdirectory(wwivd)
endif()

add_subdirectory(wwivfsed)
add_subdirectory(wwivutil)

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
  install(TARGETS lnet DESTINATION .)
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
if(WWIV_BUILD_WWIVD)
  install(TARGETS wwivd DESTINATION .)
endif()
  install(TARGETS wwivutil DESTINATION .)

  if (UNIX)
    set(PLATFORM_DIR "${WWIV_INSTALL_SRC}/platform/unix")
    # Copy shell scripts, rest can be from unix.zip
    file(GLOB PLATFORM_FILES "${PLATFORM_DIR}/*.sh" "${PLATFORM_DIR}/*.bash")
    foreach(file ${PLATFORM_FILES})
      message(DEBUG "Installing Platform Specific File: ${file}")
      INSTALL(FILES "${file}" 
              PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
              DESTINATION .)
    endforeach()
  elseif(WIN32)
    set(PLATFORM_DIR "${WWIV_INSTALL_SRC}/platform/win32")
    file(GLOB PLATFORM_FILES "${PLATFORM_DIR}/*")
    foreach(file ${PLATFORM_FILES})
      message(DEBUG "Installing Platform Specific File: ${file}")
      INSTALL(FILES "${file}" DESTINATION .)
    endforeach()
    INSTALL(FILES "${CL32_DLL}" DESTINATION .)

  elseif(OS2)
    set(PLATFORM_DIR "${WWIV_INSTALL_SRC}/platform/os2")
    file(GLOB PLATFORM_FILES "${PLATFORM_DIR}/*")
    foreach(file ${PLATFORM_FILES})
      message(DEBUG "Installing Platform Specific File: ${file}")
      INSTALL(FILES "${file}" DESTINATION .)
    endforeach()
  endif()

  file(GLOB DOCS_FILES "${WWIV_INSTALL_SRC}/docs/*")
  foreach(file ${DOCS_FILES})
    INSTALL(FILES "${file}" DESTINATION .)
  endforeach()
endif (WWIV_INSTALL)
