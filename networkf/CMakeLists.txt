# CMake for WWIV 5

set(NETWORK_MAIN networkf.cpp)

set_max_warnings()

add_executable(networkf ${NETWORK_MAIN})
target_link_libraries(networkf binkp_lib net_core core sdk)

