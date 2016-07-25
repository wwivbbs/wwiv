# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (NETWORK2)
include_directories(..)

set(NETWORK_MAIN 
	network2.cpp
	email.cpp
	post.cpp
	subs.cpp
	)

add_executable(network2 ${NETWORK_MAIN})
target_link_libraries(network2 networkb_lib core sdk)

