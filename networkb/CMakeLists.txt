# CMake for WWIV 5

set(NETWORKB_MAIN networkb.cpp)

add_executable(networkb ${NETWORKB_MAIN})
target_link_libraries(networkb binkp_lib net_core core sdk)
if(UNIX)
  if(CMAKE_SYSTEM_NAME MATCHES "SunOS.*")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lsocket -lnsl -lkstat -lssp")
  endif()
endif()
