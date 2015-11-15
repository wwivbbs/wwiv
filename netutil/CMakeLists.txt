# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (NETUTIL)
include_directories(..)

set(NETUTIL_MAIN netutil.cpp)

set(SOURCES 
 dump.cpp
 dump_callout.cpp
 dump_contact.cpp
)

add_executable(netutil ${NETUTIL_MAIN} ${SOURCES})
target_link_libraries(netutil core networkb_lib sdk)

