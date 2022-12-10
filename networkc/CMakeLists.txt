# CMake for WWIV 5

set(NETWORK_MAIN networkc.cpp)

add_executable(networkc ${NETWORK_MAIN})
set_max_warnings(networkc)
target_link_libraries(networkc binkp_lib net_core core sdk)

