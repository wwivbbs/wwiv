# FindGMock.cmake
# - Try to find benchmark
#
# The following variables are optionally searched for defaults
#  GMOCK_ROOT_DIR:  Base directory where all googletest components are found
#
# Once done this will define
#  GMOCK_FOUND - System has GMOCK
#  GMOCK_INCLUDE_DIRS - The GMOCK include directories
#  GMOCK_LIBRARIES - The libraries needed to use GMOCKmessage("WWIV FindGMock")
include(FindPackageHandleStandardArgs)
# Make googletest happy.
set(GMOCK_ROOT_DIR 
    "${CMAKE_SOURCE_DIR}/deps/googletest/googlemock" 
    CACHE PATH 
    "Folder containing googlemock")

find_path(GMOCK_INCLUDE_DIR "gmock/gmock.h"
  PATHS ${GMOCK_ROOT_DIR}
  PATH_SUFFIXES include
  NO_DEFAULT_PATH)

set(GMOCK_LIBRARY gmock CACHE STRING "gmock target" FORCE)
set(GMOCK_MAIN_LIBRARY gmock_main CACHE STRING "gmock-main target" FORCE)

# handle the QUIETLY and REQUIRED arguments and set GMOCK_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(GMock DEFAULT_MSG
                                  GMOCK_INCLUDE_DIR GMOCK_LIBRARY GMOCK_MAIN_LIBRARY)

mark_as_advanced(GMOCK_INCLUDE_DIR GMOCK_LIBRARY GMOCK_MAIN_LIBRARY)
