# CMake for WWIV 5

set(NETWORK_MAIN networkt.cpp)


add_executable(networkt ${NETWORK_MAIN})
set_max_warnings(networkt)
target_link_libraries(networkt binkp_lib net_core core sdk)

