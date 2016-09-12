# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (WWIVUTIL)
include_directories(..)

set(WWIVUTIL_MAIN wwivutil.cpp)

set(COMMAND_SOURCES
  command.cpp
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
  )

add_executable(wwivutil ${WWIVUTIL_MAIN} ${COMMAND_SOURCES})
target_link_libraries(wwivutil core networkb_lib sdk)
