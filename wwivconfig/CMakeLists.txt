# CMake for WWIV 5
Include(FindWWIVCurses)

set(COMMON_SOURCES
  "archivers.cpp"
  "autoval.cpp"
  "colors.cpp"
  "convert.cpp"
  "editors.cpp"
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
  "wwivd_ui.cpp"
  )

add_library(wwivconfig_lib ${COMMON_SOURCES})
target_link_libraries(wwivconfig_lib localui common core sdk ${CURSES_LIBRARIES})

if (WWIV_BUILD_TESTS)

  set(wwivconfig_test_sources
    convert_test.cpp
    wwivconfig_test_main.cpp
  )

  add_executable(wwivconfig_tests ${wwivconfig_test_sources})
  set_max_warnings(wwivconfig_tests)
  target_link_libraries(wwivconfig_tests 
      ${CURSES_LIBRARIES} 
      core 
      core_fixtures
      GTest::gtest 
      localui common 
      sdk 
      sdk_fixtures
      wwivconfig_lib 
  )

  gtest_discover_tests(wwivconfig_tests)

endif()

add_executable(wwivconfig "wwivconfig.cpp")
set_max_warnings(wwivconfig)
target_link_libraries(wwivconfig wwivconfig_lib localui common core sdk ${CURSES_LIBRARIES})
