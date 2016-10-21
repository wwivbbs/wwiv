# CMake for WWIV 5
cmake_minimum_required (VERSION 2.6)
project (INIT)
include_directories(..)


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
  sysop_account.cpp
  user_editor.cpp 
  utility.cpp 
  )

if(UNIX) 
  find_package(Curses REQUIRED)
endif()

add_executable(init ${COMMON_SOURCES})
target_link_libraries(init localui core sdk ${CURSES_LIBRARIES})
