# CMake for WWIV 5

set(NETWORK_MAIN network3.cpp)

add_executable(network3 ${NETWORK_MAIN})
target_link_libraries(network3 networkb_lib net_core core sdk)

