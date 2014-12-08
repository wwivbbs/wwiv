# CMake for WWIV 5.0
cmake_minimum_required (VERSION 2.6)
project (INIT)
set(CMAKE_INCLUDE_CURRENT_DIR ON)
include_directories(..)

add_definitions(-DINIT -DNOT_BBS)


set(COMMON_SOURCES
  archivers.cpp
  autoval.cpp
  convert.cpp
  editors.cpp
  init.cpp 
  instance_settings.cpp
  languages.cpp
  levels.cpp
  networks.cpp
  newinit.cpp
  paths.cpp
  protocols.cpp
  regcode.cpp
  subacc.cpp 
  subsdirs.cpp 
  system_info.cpp
  user_editor.cpp 
  utility.cpp 
  )

if(UNIX) 
  find_package(Curses REQUIRED)
  message("Curses Library: ${CURSES_LIBRARIES}") 
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
endif()

add_executable(init ${COMMON_SOURCES})
target_link_libraries(init initlib core ${CURSES_LIBRARIES})
