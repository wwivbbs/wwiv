# CMake for WWIV 5
include_directories(..)
Include(FindWWIVCurses)


set(COMMON_SOURCES
  archivers.cpp
  autoval.cpp
  convert.cpp
  editors.cpp
  languages.cpp
  levels.cpp
  menus.cpp
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
  wwivconfig.cpp
  wwivd_ui.cpp
  )

add_executable(wwivconfig ${COMMON_SOURCES})
target_link_libraries(wwivconfig localui core sdk ${CURSES_LIBRARIES})
