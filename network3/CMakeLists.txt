# CMake for WWIV 5

set(NETWORK_MAIN network3.cpp)

add_executable(network3 ${NETWORK_MAIN})
set_max_warnings(network3)
target_link_libraries(network3 binkp_lib net_core core sdk)

