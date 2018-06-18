# CMake for WWIV 5
include_directories(..)

set(WWIVUTIL_MAIN wwivutil.cpp)

set(COMMAND_SOURCES
  command.cpp
  config/config.cpp
  fido/fido.cpp
  fido/dump_fido_packet.cpp
  fido/dump_nodelist.cpp
  files/files.cpp
  fix/dirs.cpp
  fix/fix.cpp
  fix/users.cpp
  messages/messages.cpp
  net/dump_bbsdata.cpp
  net/dump_callout.cpp
  net/dump_connect.cpp
  net/dump_contact.cpp
  net/dump_packet.cpp
  net/net.cpp
  net/req.cpp
  status/status.cpp
  util.cpp
  )

add_executable(wwivutil ${WWIVUTIL_MAIN} ${COMMAND_SOURCES})
target_link_libraries(wwivutil core networkb_lib sdk)
