# CMake for WWIV 5

set(SOURCES 
 net_cmdline.cpp
)

set_max_warnings()

add_library(net_core ${SOURCES})
