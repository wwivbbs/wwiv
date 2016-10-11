# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (SDK)
include_directories(..)
include_directories(../deps/cereal/include)

set(COMMON_SOURCES
  bbslist.cpp
  binkp.cpp
  callout.cpp
  config.cpp
  connect.cpp
  contact.cpp
  datetime.cpp
  fido/fido_packets.cpp
  msgapi/email_wwiv.cpp
  msgapi/message_api.cpp
  msgapi/message_api_wwiv.cpp
  msgapi/message_area_wwiv.cpp
  msgapi/message_utils_wwiv.cpp
  msgapi/message_wwiv.cpp
  msgapi/type2_text.cpp
  names.cpp
  networks.cpp
  phone_numbers.cpp
  ssm.cpp
  status.cpp
  subxtr.cpp
  user.cpp
  usermanager.cpp
  )

if(UNIX) 
  add_definitions ("-Wall")
endif()

add_library(sdk ${COMMON_SOURCES})
target_link_libraries(sdk core)
