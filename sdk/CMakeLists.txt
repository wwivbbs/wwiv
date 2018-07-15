include_directories(..)
include_directories(../deps/cereal/include)

set(COMMON_SOURCES
  bbslist.cpp
  binkp.cpp
  callout.cpp
  config.cpp
  connect.cpp
  contact.cpp
  ftn_msgdupe.cpp
  fido/fido_address.cpp
  fido/fido_callout.cpp
  fido/fido_packets.cpp
  fido/fido_util.cpp
  fido/nodelist.cpp
  msgapi/email_wwiv.cpp
  msgapi/message_api.cpp
  msgapi/message_api_wwiv.cpp
  msgapi/message_area_wwiv.cpp
  msgapi/message_wwiv.cpp
  msgapi/parsed_message.cpp
  msgapi/type2_text.cpp
  net/packets.cpp
  names.cpp
  networks.cpp
  phone_numbers.cpp
  qscan.cpp
  ssm.cpp
  status.cpp
  subscribers.cpp
  subxtr.cpp
  user.cpp
  usermanager.cpp
  wwivd_config.cpp
  )

if(CMAKE_COMPILER_IS_GNUCC OR CMAKE_COMPILER_IS_GNUCXX) 
   # TODO also add  -Wno-long-long -pedantic
   set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
 endif()

add_library(sdk ${COMMON_SOURCES})
target_link_libraries(sdk core)
