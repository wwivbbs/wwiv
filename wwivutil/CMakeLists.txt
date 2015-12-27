# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (WWIVUTIL)
include_directories(..)

set(WWIVUTIL_MAIN wwivutil.cpp)

set(COMMAND_SOURCES
  command.cpp
  dump_callout.cpp
  dump_contact.cpp
  dump_packet.cpp
  messages.cpp
  net.cpp
  )

add_executable(wwivutil ${WWIVUTIL_MAIN} ${COMMAND_SOURCES})
target_link_libraries(wwivutil core sdk)
