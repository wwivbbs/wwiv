# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (FIX)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(..)
set(BBS_SOURCE_DIR  ../bbs)

add_definitions(-DFIX -DNOT_BBS)

set(COMMON_SOURCES
  fix.cpp
  fix_config.cpp
  log.cpp
  dirs.cpp
  users.cpp
  )

set(BBS_SOURCES
  ${BBS_SOURCE_DIR}/datetime.cpp
  ${BBS_SOURCE_DIR}/memory.cpp
  ${BBS_SOURCE_DIR}/wuser.cpp
  )

if(UNIX) 
  set(PLATFORM_SOURCES 
	${BBS_SOURCE_DIR}/platform/unix/utility2.cpp 
	)
  if(APPLE)
    SET(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -framework CoreFoundation -framework Foundation")
    include_directories(platform/osx)
    message("Detected OS as APPLE.")
  endif()
endif()

add_executable(fix ${COMMON_SOURCES} ${BBS_SOURCES} ${PLATFORM_SOURCES})
target_link_libraries(fix core)
