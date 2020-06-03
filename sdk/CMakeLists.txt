include_directories(../deps/cereal/include)

set(COMMON_SOURCES
  bbslist.cpp
  binkp.cpp
  callout.cpp
  chains.cpp
  config.cpp
  connect.cpp
  contact.cpp
  ftn_msgdupe.cpp
  ansi/ansi.cpp
  ansi/framebuffer.cpp
  ansi/makeansi.cpp
  ansi/localio_screen.cpp
  ansi/vscreen.cpp
  fido/fido_address.cpp
  fido/fido_callout.cpp
  fido/fido_packets.cpp
  fido/fido_util.cpp
  fido/nodelist.cpp
  files/allow.cpp
  files/file_record.cpp
  files/files.cpp
  files/files_ext.cpp
  msgapi/email_wwiv.cpp
  msgapi/message_api.cpp
  msgapi/message_api_wwiv.cpp
  msgapi/message_area_wwiv.cpp
  msgapi/message_wwiv.cpp
  msgapi/parsed_message.cpp
  msgapi/type2_text.cpp
  net/callouts.cpp
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

set_max_warnings()

add_library(sdk ${COMMON_SOURCES})
target_link_libraries(sdk core local_io)
