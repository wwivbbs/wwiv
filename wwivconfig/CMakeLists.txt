# CMake for WWIV 5
Include(FindWWIVCurses)

set(COMMON_SOURCES
  "archivers.cpp"
  "autoval.cpp"
  "convert.cpp"
  "editors.cpp"
  "languages.cpp"
  "levels.cpp"
  "menus.cpp"
  "networks.cpp"
  "new_user.cpp"
  "newinit.cpp"
  "paths.cpp"
  "protocols.cpp"
  "script_ui.cpp"
  "subacc.cpp"
  "subsdirs.cpp"
  "system_info.cpp"
  "sysop_account.cpp"
  "toggles.cpp"
  "user_editor.cpp"
  "utility.cpp"
  "wwivconfig.cpp"
  "wwivd_ui.cpp"
  )

set_max_warnings()

add_executable(wwivconfig ${COMMON_SOURCES})
target_link_libraries(wwivconfig localui core sdk ${CURSES_LIBRARIES})
