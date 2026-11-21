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
# On Windows it also sets these cache variables when Hunspell is a DLL
# (the default for vcpkg's x86-windows and x64-windows triplets):
#  Hunspell_DLL_RELEASE - The release DLL
#  Hunspell_DLL_DEBUG - The debug DLL (if there is a debug build)
#
# Debian/Ubuntu: apt install libhunspell-dev hunspell-en-us
# Windows:       vcpkg builds it from vcpkg.json (the "spellcheck" feature),
#                see WWIV_HUNSPELL_VCPKG in the top level CMakeLists.txt.

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_HUNSPELL QUIET hunspell)
endif()

find_path(Hunspell_INCLUDE_DIR
  NAMES hunspell.hxx
  HINTS ${PC_HUNSPELL_INCLUDEDIR} ${PC_HUNSPELL_INCLUDE_DIRS}
  PATH_SUFFIXES hunspell)

# hunspell-1.7.dll is for the import library that libtool makes with MSVC
# (hunspell-1.7.dll.lib).
set(_hunspell_names
  hunspell-1.7 hunspell libhunspell-1.7 libhunspell hunspell-1.7.dll hunspell-1.6)

if(DEFINED VCPKG_INSTALLED_DIR AND DEFINED VCPKG_TARGET_TRIPLET)
  # vcpkg installs release libraries in {triplet}/lib and debug libraries in
  # {triplet}/debug/lib. Look for each one explicitly so that a Debug build
  # links the debug library and a Release build the release one. Mixing them
  # breaks with MSVC since Hunspell passes std::string across the DLL.
  set(_hunspell_vcpkg "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")
  find_library(Hunspell_LIBRARY_RELEASE
    NAMES ${_hunspell_names}
    PATHS "${_hunspell_vcpkg}/lib"
    NO_DEFAULT_PATH)
  find_library(Hunspell_LIBRARY_DEBUG
    NAMES ${_hunspell_names}
    PATHS "${_hunspell_vcpkg}/debug/lib"
    NO_DEFAULT_PATH)
endif()

if(NOT Hunspell_LIBRARY_RELEASE)
  # Not from vcpkg, i.e. libhunspell-dev on Linux.
  find_library(Hunspell_LIBRARY_RELEASE
    NAMES ${_hunspell_names}
    HINTS ${PC_HUNSPELL_LIBDIR} ${PC_HUNSPELL_LIBRARY_DIRS})
endif()

include(SelectLibraryConfigurations)
select_library_configurations(Hunspell)

set(Hunspell_VERSION ${PC_HUNSPELL_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Hunspell
  REQUIRED_VARS Hunspell_LIBRARY Hunspell_INCLUDE_DIR
  VERSION_VAR Hunspell_VERSION)

# Finds the DLL that goes with the import library lib, it is in ../bin.
function(_hunspell_find_dll out lib)
  set(dll "")
  if(lib)
    get_filename_component(libdir "${lib}" DIRECTORY)
    file(GLOB dlls "${libdir}/../bin/*hunspell*.dll")
    if(dlls)
      list(GET dlls 0 dll)
      get_filename_component(dll "${dll}" ABSOLUTE)
    endif()
  endif()
  set(${out} "${dll}" PARENT_SCOPE)
endfunction()

if(Hunspell_FOUND)
  set(Hunspell_INCLUDE_DIRS ${Hunspell_INCLUDE_DIR})

  if(WIN32)
    _hunspell_find_dll(_dll_release "${Hunspell_LIBRARY_RELEASE}")
    _hunspell_find_dll(_dll_debug "${Hunspell_LIBRARY_DEBUG}")
    set(Hunspell_DLL_RELEASE "${_dll_release}" CACHE FILEPATH "Hunspell release DLL" FORCE)
    set(Hunspell_DLL_DEBUG "${_dll_debug}" CACHE FILEPATH "Hunspell debug DLL" FORCE)
    mark_as_advanced(Hunspell_DLL_RELEASE Hunspell_DLL_DEBUG)
  endif()

  if(NOT TARGET Hunspell::Hunspell)
    if(WIN32 AND Hunspell_DLL_RELEASE)
      add_library(Hunspell::Hunspell SHARED IMPORTED)
    else()
      add_library(Hunspell::Hunspell UNKNOWN IMPORTED)
      if(WIN32)
        # Static library: hunvisapi.h must not use __declspec(dllimport).
        set_target_properties(Hunspell::Hunspell PROPERTIES
          INTERFACE_COMPILE_DEFINITIONS HUNSPELL_STATIC)
      endif()
    endif()
    set_target_properties(Hunspell::Hunspell PROPERTIES
      INTERFACE_INCLUDE_DIRECTORIES "${Hunspell_INCLUDE_DIR}")

    # RELEASE is listed first, so it is also used for RelWithDebInfo and
    # MinSizeRel builds.
    foreach(_cfg RELEASE DEBUG)
      if(Hunspell_LIBRARY_${_cfg})
        set_property(TARGET Hunspell::Hunspell APPEND PROPERTY IMPORTED_CONFIGURATIONS ${_cfg})
        if(WIN32 AND Hunspell_DLL_${_cfg})
          set_target_properties(Hunspell::Hunspell PROPERTIES
            IMPORTED_IMPLIB_${_cfg} "${Hunspell_LIBRARY_${_cfg}}"
            IMPORTED_LOCATION_${_cfg} "${Hunspell_DLL_${_cfg}}")
        else()
          set_target_properties(Hunspell::Hunspell PROPERTIES
            IMPORTED_LOCATION_${_cfg} "${Hunspell_LIBRARY_${_cfg}}")
        endif()
      endif()
    endforeach()
  endif()
endif()

mark_as_advanced(Hunspell_INCLUDE_DIR Hunspell_LIBRARY_RELEASE Hunspell_LIBRARY_DEBUG)
