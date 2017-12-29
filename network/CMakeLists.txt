# CMake for WWIV 5
include_directories(..)

set(NETWORK_MAIN network.cpp)

add_executable(network ${NETWORK_MAIN})
target_link_libraries(network networkb_lib core sdk)

