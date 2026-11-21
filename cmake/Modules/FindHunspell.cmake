# FindHunspell.cmake
# - Try to find the Hunspell spell checking library.
#
# Uses pkg-config (hunspell.pc) for hints when it is available, then
# searches the normal include and library paths (including vcpkg).
#
# Once done this will define
#  Hunspell_FOUND - System has Hunspell
#  Hunspell_INCLUDE_DIRS - The directory containing hunspell.hxx
#  Hunspell_LIBRARIES - The libraries needed to use Hunspell
#  Hunspell_VERSION - The version from pkg-config, if known
#  Hunspell::Hunspell - Imported target to link against
#
# Debian/Ubuntu: apt install libhunspell-dev hunspell-en-us
# vcpkg:         vcpkg install hunspell

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_HUNSPELL QUIET hunspell)
endif()

find_path(Hunspell_INCLUDE_DIR
  NAMES hunspell.hxx
  HINTS ${PC_HUNSPELL_INCLUDEDIR} ${PC_HUNSPELL_INCLUDE_DIRS}
  PATH_SUFFIXES hunspell)

find_library(Hunspell_LIBRARY
  NAMES hunspell-1.7 hunspell libhunspell-1.7 libhunspell hunspell-1.6
  HINTS ${PC_HUNSPELL_LIBDIR} ${PC_HUNSPELL_LIBRARY_DIRS})

set(Hunspell_VERSION ${PC_HUNSPELL_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Hunspell
  REQUIRED_VARS Hunspell_LIBRARY Hunspell_INCLUDE_DIR
  VERSION_VAR Hunspell_VERSION)

if(Hunspell_FOUND)
  set(Hunspell_INCLUDE_DIRS ${Hunspell_INCLUDE_DIR})
  set(Hunspell_LIBRARIES ${Hunspell_LIBRARY})
  if(NOT TARGET Hunspell::Hunspell)
    add_library(Hunspell::Hunspell UNKNOWN IMPORTED)
    set_target_properties(Hunspell::Hunspell PROPERTIES
      IMPORTED_LOCATION "${Hunspell_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${Hunspell_INCLUDE_DIR}")
  endif()
endif()

mark_as_advanced(Hunspell_INCLUDE_DIR Hunspell_LIBRARY)
