# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (NETWORKB)
include_directories(..)

set(NETWORKB_MAIN networkb.cpp)

set(SOURCES 
 binkp.cpp
 binkp_commands.cpp
 binkp_config.cpp
 callout.cpp
 connection.cpp
 contact.cpp
 net_log.cpp
 ppp_config.cpp
 socket_connection.cpp
 socket_exceptions.cpp
 transfer_file.cpp
 wfile_transfer_file.cpp
)

add_library(networkb_lib ${SOURCES})
add_executable(networkb ${NETWORKB_MAIN})
target_link_libraries(networkb networkb_lib core sdk)

