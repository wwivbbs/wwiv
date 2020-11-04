# CMake for WWIV 5

set(NETWORK_MAIN 
	network2.cpp
	context.cpp
	email.cpp
	post.cpp
	subs.cpp
	)
set_max_warnings()

add_executable(network2 ${NETWORK_MAIN})
target_link_libraries(network2 binkp_lib net_core core sdk)

