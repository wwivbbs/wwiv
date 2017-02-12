# CMake for WWIV Daemon
cmake_minimum_required (VERSION 2.6)
project (wwivd)
include_directories(..)
include_directories(../deps/cereal/include)

set(WWIVD_SOURCES wwivd.cpp wwivd_unix.cpp)
if (UNIX)
  find_package (Threads)
endif()

add_executable(wwivd ${WWIVD_SOURCES})
target_link_libraries(wwivd sdk core ${CMAKE_THREAD_LIBS_INIT})
