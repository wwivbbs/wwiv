# FindGTest.cmake
# - Try to find benchmark
#
# The following variables are optionally searched for defaults
#  GTEST_ROOT_DIR:  Base directory where all googletest components are found
#
# Once done this will define
#  GTEST_FOUND - System has GTest
#  GTEST_INCLUDE_DIRS - The GTest include directories
#  GTEST_LIBRARIES - The libraries needed to use GTest
message("WWIV FindGTest")

# TODO(rushfan): Test on Linux too.
set(GTEST_ROOT_DIR
    "${CMAKE_SOURCE_DIR}/deps/googletest/googletest"
    CACHE PATH
    "Folder containing GTest")

include(FindPackageHandleStandardArgs)

find_path(GTEST_INCLUDE_DIRS "gtest/gtest.h"
  PATHS ${GTEST_ROOT_DIR}
  PATH_SUFFIXES include
  NO_DEFAULT_PATH)

set(GTEST_LIBRARY gtest CACHE STRING "gtest target" FORCE)
set(GTEST_MAIN_LIBRARY gtest_main CACHE STRING "gtest-main target" FORCE)
# Used by cctz and others.
set(GTEST_BOTH_LIBRARIES gtest gtest_main)

# handle the QUIETLY and REQUIRED arguments and set GTEST_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(GTest DEFAULT_MSG
                                  GTEST_INCLUDE_DIRS GTEST_LIBRARY GTEST_MAIN_LIBRARY)

mark_as_advanced(GTEST_INCLUDE_DIRS GTEST_LIBRARY GTEST_MAIN_LIBRARY)
