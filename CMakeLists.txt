# CMake for WWIV
cmake_minimum_required (VERSION 2.6)
project (WWIV5All)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake)

include(Common)
include(FindASan)

if(MSVC)
  message("Using MSVC, Setting warnings to match UNIX.")
  add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -D_WINSOCK_DEPRECATED_NO_WARNINGS)
endif()


add_subdirectory(deps/cl342)
# Workaround gtest really wanting to compile with /Mtd vs /MD
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
add_subdirectory(deps/googletest-master/googletest)

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

