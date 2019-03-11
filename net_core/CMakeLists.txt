# CMake for WWIV 5
include_directories(..)

set(SOURCES 
 net_cmdline.cpp
)

if(UNIX) 
  add_definitions ("-Wall")
endif()

if(WIN32)
    SET_WARNING_LEVEL_4()
endif()

add_library(net_core ${SOURCES})
