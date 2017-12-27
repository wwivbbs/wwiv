# Findbenchmark.cmake
# - Try to find benchmark
#
# The following variables are optionally searched for defaults
#  benchmark_ROOT_DIR:  Base directory where all benchmark components are found
#
# Once done this will define
#  benchmark_FOUND - System has benchmark
#  benchmark_INCLUDE_DIRS - The benchmark include directories
#  benchmark_LIBRARIES - The libraries needed to use benchmark
message("WWIV Findbenchmark")
set(benchmark_ROOT_DIR "${CMAKE_SOURCE_DIR}/deps/benchmark" CACHE PATH "Folder containing benchmark")

find_path(benchmark_INCLUDE_DIRS "benchmark/benchmark.h"
  PATHS ${benchmark_ROOT_DIR}
  PATH_SUFFIXES include
  NO_DEFAULT_PATH)

set(benchmark_LIBRARY benchmark CACHE STRING "benchmark target" FORCE)

include(FindPackageHandleStandardArgs)
# handle the QUIETLY and REQUIRED arguments and set benchmark_FOUND to TRUE
# if all listed variables are TRUE
find_package_handle_standard_args(benchmark FOUND_VAR benchmark_FOUND
  REQUIRED_VARS benchmark_LIBRARY
  benchmark_INCLUDE_DIRS)

if(benchmark_FOUND)
  set(benchmark_LIBRARIES ${benchmark_LIBRARY})
endif()
message("benchmark_INCLUDE_DIRS ${benchmark_INCLUDE_DIRS}")

mark_as_advanced(benchmark_INCLUDE_DIR benchmark_LIBRARY)