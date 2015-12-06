# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (SDK)
include_directories(..)

set(COMMON_SOURCES
  config.cpp
  message_utils_wwiv.cpp
  msgapi.cpp
  msgapi_wwiv.cpp
  networks.cpp
  phone_numbers.cpp
  )

add_library(sdk ${COMMON_SOURCES})
target_link_libraries(sdk core)
