# LNet for WWIV5

add_executable(lnet lnet.cpp lnet_main.cpp)
set_max_warnings(lnet)
target_link_libraries(lnet net_core core sdk local_io localui)

