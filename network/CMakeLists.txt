# CMake for WWIV 5

set(NETWORK_MAIN network.cpp)
set_max_warnings()

add_executable(network ${NETWORK_MAIN})
target_link_libraries(network binkp_lib net_core core sdk)

