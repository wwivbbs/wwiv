# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (SDK)
include_directories(..)

set(COMMON_SOURCES
  config.cpp
  networks.cpp
  )

if(UNIX) 
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_library(sdk ${COMMON_SOURCES})
