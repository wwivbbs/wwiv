# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (NETWORKB)
include_directories(..)

set(SOURCES 
 binkp.cpp
 connection.cpp
 networkb.cpp
 socket_connection.cpp
 socket_exceptions.cpp
 transfer_file.cpp
 wfile_transfer_file.cpp
)

if(UNIX) 
  SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()


add_executable(networkb ${SOURCES})
target_link_libraries(networkb core)

