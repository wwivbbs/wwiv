# CMake for WWIVfsed

set_max_warnings()

if(UNIX) 
  find_package (Threads)
  set(FORKPTY_LIB util)
endif()

add_executable(wwivfsed wwivfsed.cpp fsedconfig.cpp)
target_link_libraries(wwivfsed 
	common 
	core
	fsed
	sdk
  ${CMAKE_THREAD_LIBS_INIT} 
  ${FORKPTY_LIB}
	)
