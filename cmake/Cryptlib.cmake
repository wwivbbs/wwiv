# Cryptlib (used for SSH support in the BBS), from the deps/cryptlib
# submodule (https://github.com/wwivbbs/cryptlib, a fork of
# https://github.com/cryptlib/cryptlib).
#
# Defines the imported target "cryptlib":
#   Windows: the prebuilt cl32.dll / cl32.lib that ship with cryptlib. It is a
#            SHARED imported library, so wwiv_copy_runtime_dlls() can copy the
#            DLL next to executables. Also sets CL32_DLL for the install rules.
#   Others:  libcl.a, built with cryptlib's own makefile. The makefile builds
#            in its source directory, so the submodule is marked
#            "ignore = dirty" in .gitmodules.
#
# Configure with -DWWIV_SSH_CRYPTLIB=OFF to build without cryptlib (no SSH).

set(WWIV_CRYPTLIB_DIR "${PROJECT_SOURCE_DIR}/deps/cryptlib")

if (WIN32)
  add_library(cryptlib SHARED IMPORTED GLOBAL)
  set_target_properties(cryptlib PROPERTIES
    IMPORTED_LOCATION "${WWIV_CRYPTLIB_DIR}/cl32.dll"
    IMPORTED_IMPLIB   "${WWIV_CRYPTLIB_DIR}/cl32.lib"
  )
  # Used by the install rules in the top-level CMakeLists.txt
  set(CL32_DLL "${WWIV_CRYPTLIB_DIR}/cl32.dll")
  message(STATUS "Setting CL32_DLL: ${CL32_DLL}")
else()
  add_library(cryptlib STATIC IMPORTED GLOBAL)
  set(LIBCL_A "${WWIV_CRYPTLIB_DIR}/libcl.a")
  add_custom_command(
    OUTPUT "${LIBCL_A}"
    # XCFLAGS is added to the makefile's own CFLAGS. -w: cryptlib is
    # third-party code, so don't bury WWIV's own warnings under its warnings.
    COMMAND make "XCFLAGS=-w -DUSE_SSH_CTR -DUSE_DES"
    WORKING_DIRECTORY "${WWIV_CRYPTLIB_DIR}"
    COMMENT "Building cryptlib with its makefile"
  )
  add_custom_target(cryptlib_builder DEPENDS "${LIBCL_A}")
  add_dependencies(cryptlib cryptlib_builder)
  set_target_properties(cryptlib PROPERTIES IMPORTED_LOCATION "${LIBCL_A}")

  # Libraries libcl.a itself needs, so they come after it on the link line
  # (GNU ld only searches libraries that follow the one that needs them).
  # See tools/getlibs.sh: on Linux that's -lresolv -lpthread (-ldl), and
  # tools/ccopts.sh builds with the system zlib whenever
  # /usr/include/zlib.h exists.
  set(_cryptlib_deps)
  find_package(Threads REQUIRED)
  list(APPEND _cryptlib_deps Threads::Threads ${CMAKE_DL_LIBS})
  find_library(WWIV_RESOLV_LIBRARY resolv)
  if (WWIV_RESOLV_LIBRARY)
    list(APPEND _cryptlib_deps "${WWIV_RESOLV_LIBRARY}")
  endif()
  if (EXISTS "/usr/include/zlib.h")
    find_package(ZLIB REQUIRED)
    list(APPEND _cryptlib_deps ZLIB::ZLIB)
  endif()
  set_target_properties(cryptlib PROPERTIES INTERFACE_LINK_LIBRARIES "${_cryptlib_deps}")
endif()

set_target_properties(cryptlib PROPERTIES
  INTERFACE_INCLUDE_DIRECTORIES "${WWIV_CRYPTLIB_DIR}"
)
