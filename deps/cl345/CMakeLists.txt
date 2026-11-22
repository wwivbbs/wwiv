# Cryptlib (used for SSH support in the BBS).
#
# Windows: uses the prebuilt cl32.dll / cl32.lib that live in this directory.
#          CL345_LIB is an imported SHARED library, so CMake knows about the
#          DLL and wwiv_copy_runtime_dlls() can copy it next to executables.
# Others:  builds libcl.a with cryptlib's own makefile.
#
# Configure with -DWWIV_SSH_CRYPTLIB=OFF to build without cryptlib (no SSH).

if (WIN32) 
  add_library(CL345_LIB SHARED IMPORTED GLOBAL)
  set_target_properties(CL345_LIB PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/cl32.dll"
    IMPORTED_IMPLIB   "${CMAKE_CURRENT_SOURCE_DIR}/cl32.lib"
  )
  # Used by the install rules in the top-level CMakeLists.txt
  set (CL32_DLL ${CMAKE_CURRENT_SOURCE_DIR}/cl32.dll PARENT_SCOPE)
  message(STATUS "Setting CL32_DLL: ${CMAKE_CURRENT_SOURCE_DIR}/cl32.dll")
else()
  add_library(CL345_LIB STATIC IMPORTED GLOBAL)
  set (LIBCL_A ${CMAKE_CURRENT_SOURCE_DIR}/libcl.a)
  add_custom_command(
    OUTPUT ${LIBCL_A}
    COMMAND cmake -E environment
    COMMAND env
    # The makefile's default CFLAGS plus -w: cryptlib is third-party code, so
    # don't bury WWIV's own warnings under its warnings.
    COMMAND make "CFLAGS=-c -D__UNIX__ -DNDEBUG -I. -w"
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Cryptlib makefile target."
    )
  add_custom_target(
    cl345_lib_builder
    DEPENDS ${LIBCL_A}
  )  
  add_dependencies(CL345_LIB cl345_lib_builder)
  set_property(TARGET CL345_LIB PROPERTY IMPORTED_LOCATION ${LIBCL_A})
endif()
set_property(
  TARGET CL345_LIB
  APPEND PROPERTY
      INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}"
)
message (DEBUG "CL: ${LIBCL_A}")
