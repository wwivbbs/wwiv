# CMake for WWIV 5
include_directories(..)

set(NETWORK_MAIN networkf.cpp)

add_executable(networkf ${NETWORK_MAIN})
target_link_libraries(networkf networkb_lib net_core core sdk)

