# CMake for WWIV 5
include_directories(..)

set(SOURCES 
 net_cmdline.cpp
)

set_max_warnings()

add_library(net_core ${SOURCES})
