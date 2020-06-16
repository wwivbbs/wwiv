# CMake for WWIV 5

set(NETWORK_MAIN networkc.cpp)

set_max_warnings()

add_executable(networkc ${NETWORK_MAIN})
target_link_libraries(networkc binkp_lib net_core core sdk)

