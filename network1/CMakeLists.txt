# CMake for WWIV 5

set(NETWORK_MAIN network1.cpp)

add_executable(network1 ${NETWORK_MAIN})
set_max_warnings(network1)
target_link_libraries(network1 binkp_lib net_core core sdk)

