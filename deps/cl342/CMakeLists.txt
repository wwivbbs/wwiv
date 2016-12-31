cmake_minimum_required (VERSION 2.6)
project (CL342)

# Awful looking solution hacked up from:
# http://stackoverflow.com/questions/7876753
add_custom_target(build_cryptlib_lib
  ALL
  COMMAND ${CMAKE_MAKE_PROGRAM}
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  COMMENT "Cryptlib makefile target."
  )
set (cryptlib_lib ${CMAKE_CURRENT_SOURCE_DIR}/libcl.a)

add_library(cl342_lib STATIC IMPORTED)
set_property(TARGET cl342_lib APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(cl342_lib PROPERTIES IMPORTED_LOCATION_NOCONFIG "${cryptlib_lib}")
add_dependencies(cl342_lib build_cryptlib_lib)

# This is needed to put it in to the root CMakeCache.txt
# but only works if it's been built... sigh
find_library(CL342_LIB libcl.a PATHS .) 
