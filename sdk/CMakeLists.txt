# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (SDK)
include_directories(..)

set(COMMON_SOURCES
  callout.cpp
  config.cpp
  contact.cpp
  datetime.cpp
  msgapi/message_api.cpp
  msgapi/message_api_wwiv.cpp
  msgapi/message_area_wwiv.cpp
  msgapi/message_utils_wwiv.cpp
  msgapi/message_wwiv.cpp
  msgapi/msgapi_wwiv.cpp
  names.cpp
  networks.cpp
  phone_numbers.cpp
  user.cpp
  usermanager.cpp
  )

if(UNIX) 
  add_definitions ("-Wall")
endif()

add_library(sdk ${COMMON_SOURCES})
target_link_libraries(sdk core)
