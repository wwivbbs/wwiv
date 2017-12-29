# CMake for WWIV 5
include_directories(..)

set(NETWORK_MAIN networkc.cpp)

add_executable(networkc ${NETWORK_MAIN})
target_link_libraries(networkc networkb_lib core sdk)

