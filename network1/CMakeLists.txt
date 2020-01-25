# CMake for WWIV 5

set(NETWORK_MAIN network1.cpp)
set_max_warnings()

add_executable(network1 ${NETWORK_MAIN})
target_link_libraries(network1 networkb_lib net_core core sdk)

