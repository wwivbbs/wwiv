# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (INIT)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(BBS_SOURCE_DIR  ../bbs)
include_directories(platform)
include_directories(${BBS_SOURCE_DIR})
include_directories(${BBS_SOURCE_DIR}/platform)

add_definitions(-DINIT -DNOT_BBS)


set(COMMON_SOURCES
  autosel.cpp compile.cpp convert.cpp
  init.cpp init1.cpp init2.cpp init3.cpp
  initsession.cpp statusmgr.cpp
  )

if(UNIX) 
  include_directories(platform/unix)
  set(PLATFORM_SOURCES 
	platform/unix/wfile.cpp 
	platform/unix/wlocal_io.cpp
	platform/unix/filestuff.cpp
	platform/unix/utility2.cpp 
	platform/unix/wfndfile.cpp 
	platform/unix/wshare.cpp 
	)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_executable(init ${COMMON_SOURCES} ${PLATFORM_SOURCES})
