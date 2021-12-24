# CMake for WWIV 5

set_max_warnings()

add_library(networkf_lib networkf.cpp)
target_link_libraries(networkf_lib binkp_lib net_core core sdk fmt::fmt-header-only)

add_executable(networkf networkf_main.cpp)
target_link_libraries(networkf networkf_lib)

