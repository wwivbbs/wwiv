# CMake for WWIV 5
include_directories(..)

set(NETWORKB_MAIN networkb.cpp)

set(SOURCES 
 binkp.cpp
 binkp_commands.cpp
 binkp_config.cpp
 cram.cpp
 file_manager.cpp
 net_log.cpp
 net_util.cpp
 packets.cpp
 ppp_config.cpp
 remote.cpp
 transfer_file.cpp
 wfile_transfer_file.cpp
)

add_library(networkb_lib ${SOURCES})
add_executable(networkb ${NETWORKB_MAIN})
target_link_libraries(networkb networkb_lib core sdk)
if(UNIX)
  if(CMAKE_SYSTEM_NAME MATCHES "SunOS.*")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lsocket -lnsl -lkstat -lssp")
  endif()
endif()
