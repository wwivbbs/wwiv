# This is needed to put it in to the root CMakeCache.txt
# but only works if it's been built... sigh 
  add_library(CL345_LIB STATIC IMPORTED GLOBAL)
if (WIN32) 
  set_property(
  TARGET 
    CL345_LIB PROPERTY 
  IMPORTED_LOCATION 
    ${CMAKE_CURRENT_SOURCE_DIR}/cl32.lib)
  set (CL32_DLL ${CMAKE_CURRENT_SOURCE_DIR}/cl32.dll PARENT_SCOPE)
  message(STATUS "Setting CL32_DLL: ${CL32_DLL}")
else()
  set (LIBCL_A ${CMAKE_CURRENT_SOURCE_DIR}/libcl.a)
  add_custom_command(
    OUTPUT ${LIBCL_A}
    COMMAND cmake -E environment
    COMMAND env
    COMMAND make
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
message (DEBUG "CL: ${cl345_lib}:${LIBCL_A}")
