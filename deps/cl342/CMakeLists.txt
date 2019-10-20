# This is needed to put it in to the root CMakeCache.txt
# but only works if it's been built... sigh 
  add_library(CL342_LIB STATIC IMPORTED GLOBAL)
if (WIN32) 
  set_property(
  TARGET 
    CL342_LIB PROPERTY 
  IMPORTED_LOCATION 
    ${CMAKE_CURRENT_SOURCE_DIR}/cl32.lib)
else()
  set (LIBCL_A ${CMAKE_CURRENT_SOURCE_DIR}/libcl.a)
  add_custom_command(
    OUTPUT 
      ${LIBCL_A}
    COMMAND 
      make
    WORKING_DIRECTORY 
      ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT 
      "Cryptlib makefile target."
    )
  add_custom_target(
    cl342_lib_builder
    DEPENDS ${LIBCL_A}
  )  
  add_dependencies(CL342_LIB cl342_lib_builder)
  set_property(TARGET CL342_LIB PROPERTY IMPORTED_LOCATION ${LIBCL_A})
endif()
set_property(
  TARGET CL342_LIB
  APPEND PROPERTY
      INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}"
)
message (STATUS "CL: ${cl342_lib}:${LIBCL_A}")
