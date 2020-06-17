# CMake for WWIV 5

set(NETWORK_MAIN networkt.cpp)

set_max_warnings()

add_executable(networkt ${NETWORK_MAIN})
target_link_libraries(networkt binkp_lib net_core core sdk)

