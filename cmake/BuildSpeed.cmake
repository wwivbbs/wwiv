#
# Build speed helpers for WWIV. Both are used only when the tool is installed,
# so a machine without them builds exactly as before.
#
#  * Compiler cache: ccache or sccache, used as the compiler launcher.
#  * Faster linker:  mold or lld instead of the default GNU ld (Linux only).
#
# Turn either off with -DWWIV_USE_COMPILER_CACHE=OFF / -DWWIV_USE_FAST_LINKER=OFF.

option(WWIV_USE_COMPILER_CACHE "Use ccache or sccache to compile when one is installed" ON)
option(WWIV_USE_FAST_LINKER "Link with mold or lld when one is installed (Linux only)" ON)

# find_program, but if the cached path no longer exists (the tool was
# uninstalled since the last configure), forget it and search again, so a stale
# path doesn't break every compile or link. find_program already searches again
# when the cached value is NOTFOUND, so a tool installed later is picked up.
macro(wwiv_find_program _var)
  if (${_var} AND NOT EXISTS "${${_var}}")
    unset(${_var} CACHE)
  endif()
  find_program(${_var} ${ARGN})
endmacro()

###############################################################################
#
# Compiler cache

function(wwiv_setup_compiler_cache)
  if (NOT WWIV_USE_COMPILER_CACHE)
    return()
  endif()
  # Respect a launcher chosen on the command line or via the environment.
  if (CMAKE_C_COMPILER_LAUNCHER OR CMAKE_CXX_COMPILER_LAUNCHER)
    message(STATUS "Compiler cache: using the compiler launcher already configured")
    return()
  endif()
  # Only the Ninja and Makefile generators support compiler launchers.
  if (NOT CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
    return()
  endif()

  if (MSVC)
    wwiv_find_program(WWIV_COMPILER_CACHE NAMES sccache ccache)
  else()
    wwiv_find_program(WWIV_COMPILER_CACHE NAMES ccache sccache)
  endif()
  if (NOT WWIV_COMPILER_CACHE)
    message(STATUS "Compiler cache: none found (install ccache or sccache to speed up rebuilds)")
    return()
  endif()

  if (MSVC)
    # ccache/sccache can't cache objects built with /Zi (debug info in a shared
    # PDB); they need /Z7 (debug info inside each object). Selecting that needs
    # policy CMP0141, which is set before project() in the top-level CMakeLists.
    cmake_policy(GET CMP0141 _wwiv_cmp0141)
    if (NOT _wwiv_cmp0141 STREQUAL "NEW")
      message(STATUS "Compiler cache: ${WWIV_COMPILER_CACHE} needs CMake 3.25+ with MSVC; not using it")
      return()
    endif()
    if (DEFINED CMAKE_MSVC_DEBUG_INFORMATION_FORMAT)
      # Keep the user's own setting. With ProgramDatabase (/Zi) the compiler
      # cache still runs but won't get cache hits.
      message(STATUS "Compiler cache: keeping CMAKE_MSVC_DEBUG_INFORMATION_FORMAT=${CMAKE_MSVC_DEBUG_INFORMATION_FORMAT}")
    else()
      set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<$<CONFIG:Debug,RelWithDebInfo>:Embedded>" PARENT_SCOPE)
    endif()
  endif()

  message(STATUS "Compiler cache: ${WWIV_COMPILER_CACHE}")
  set(CMAKE_C_COMPILER_LAUNCHER "${WWIV_COMPILER_CACHE}" PARENT_SCOPE)
  set(CMAKE_CXX_COMPILER_LAUNCHER "${WWIV_COMPILER_CACHE}" PARENT_SCOPE)
endfunction()

###############################################################################
#
# Faster linker

function(wwiv_setup_fast_linker)
  if (NOT WWIV_USE_FAST_LINKER)
    return()
  endif()
  if (NOT CMAKE_SYSTEM_NAME STREQUAL "Linux" OR NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    return()
  endif()
  # Respect a linker chosen on the command line.
  if (DEFINED CMAKE_LINKER_TYPE OR CMAKE_EXE_LINKER_FLAGS MATCHES "-fuse-ld=")
    message(STATUS "Linker: using the linker already configured")
    return()
  endif()
  if (CMAKE_VERSION VERSION_LESS 3.18)
    return()
  endif()

  include(CheckLinkerFlag)
  # check_linker_flag links a test program, so this only picks a linker the
  # compiler both accepts (-fuse-ld=mold needs GCC 12+) and can actually run.
  # check_linker_flag caches its result, so clear it and check again on every
  # configure; otherwise a linker installed or removed since the last configure
  # would go unnoticed. Look for the linker first so a machine without it
  # doesn't pay for a test link.
  foreach(_ld IN ITEMS mold lld)
    unset(WWIV_LINKER_SUPPORTS_${_ld} CACHE)
    wwiv_find_program(WWIV_LINKER_${_ld} NAMES ld.${_ld})
    if (NOT WWIV_LINKER_${_ld})
      continue()
    endif()
    check_linker_flag(CXX "-fuse-ld=${_ld}" WWIV_LINKER_SUPPORTS_${_ld})
    if (WWIV_LINKER_SUPPORTS_${_ld})
      message(STATUS "Linker: ${_ld}")
      add_link_options("-fuse-ld=${_ld}")
      return()
    endif()
  endforeach()
  message(STATUS "Linker: default (install mold or lld for faster links)")
endfunction()

wwiv_setup_compiler_cache()
wwiv_setup_fast_linker()
