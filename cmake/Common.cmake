
# Common CMake module for WWIV

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules/sanitizers)

message(STATUS "Loaded WWIV Common CMake Module.")

if(CMAKE_SYSTEM_NAME MATCHES "Linux")
  set(LINUX TRUE)
endif()

if (UNIX)
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++14 -frtti")
endif(UNIX)

if(MSVC)
  message("Using MSVC, Setting warnings to match UNIX.")
  add_definitions(/D_CRT_SECURE_NO_WARNINGS /D_CRT_NONSTDC_NO_DEPRECATE /D_WINSOCK_DEPRECATED_NO_WARNINGS )
  add_definitions(/DNOMINMAX /DWIN32_LEAN_AND_MEAN=1)
endif()

get_directory_property(defs COMPILE_DEFINITIONS) 

message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
message("COMPILE_DEFINITIONS: ${COMPILE_DEFINITIONS}; defs: ${def}")

