# CMake for WWIV 5

set(SOURCES 
 net_cmdline.cpp
 netdat.cpp
)

set_max_warnings()

add_library(net_core ${SOURCES})
target_link_libraries(net_core fmt::fmt-header-only)
