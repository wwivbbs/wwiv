# CMake for WWIV 5


set(SOURCES 
 binkp.cpp
 binkp_commands.cpp
 binkp_config.cpp
 cram.cpp
 file_manager.cpp
 net_log.cpp
 ppp_config.cpp
 remote.cpp
 transfer_file.cpp
 wfile_transfer_file.cpp
)
set_max_warnings()

add_library(binkp_lib ${SOURCES})
target_link_libraries(binkp_lib fmt::fmt-header-only)
