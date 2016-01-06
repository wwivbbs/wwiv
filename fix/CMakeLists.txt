# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (FIX)
include_directories(..)

set(COMMON_SOURCES
  fix.cpp
  fix_config.cpp
  log.cpp
  dirs.cpp
  users.cpp
  )

add_executable(fix ${COMMON_SOURCES})
target_link_libraries(fix core sdk)
