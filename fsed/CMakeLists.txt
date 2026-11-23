# CMake for WWIV
Include(FindWWIVCurses)

set(SOURCES 
 commands.cpp
 common.cpp
 fsed.cpp
 line.cpp
 model.cpp
 spell.cpp
 spell_config.cpp
 spell_text.cpp
 spell_ui.cpp
 spell_walk.cpp
 view.cpp
)

add_library(
  fsed 
  ${SOURCES} 
) 

target_link_libraries(
  fsed 
  fmt::fmt-header-only
  local_io 
  common
  core 
  sdk 
)


set_max_warnings(fsed)
target_include_directories(fsed PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# Optional spell check using Hunspell. When it is off or Hunspell is not
# found, spell.cpp builds a stub that reports spell check as not available.
if (WWIV_HUNSPELL)
  find_package(Hunspell 1.7 QUIET)
  if (Hunspell_FOUND)
    message(STATUS "WWIV_HUNSPELL is ON (found Hunspell ${Hunspell_VERSION})")
    target_link_libraries(fsed Hunspell::Hunspell)
    target_compile_definitions(fsed PRIVATE WWIV_HAS_HUNSPELL)
    if (WIN32 AND WWIV_INSTALL AND Hunspell_DLL_RELEASE)
      # vcpkg copies the DLL next to the programs in the build directory;
      # the release zip needs it too. Debug builds use the debug DLL.
      if (Hunspell_DLL_DEBUG)
        install(FILES "$<IF:$<CONFIG:Debug>,${Hunspell_DLL_DEBUG},${Hunspell_DLL_RELEASE}>"
                DESTINATION .)
      else()
        install(FILES "${Hunspell_DLL_RELEASE}" DESTINATION .)
      endif()
    endif()
  else()
    if (WIN32)
      message(STATUS "WWIV_HUNSPELL is ON, but Hunspell was not found; FSED spell check is disabled "
                     "(is WWIV_HUNSPELL_VCPKG ON?)")
    else()
      message(STATUS "WWIV_HUNSPELL is ON, but Hunspell was not found; FSED spell check is disabled "
                     "(apt install libhunspell-dev)")
    endif()
  endif()
else()
  message(STATUS "WWIV_HUNSPELL is OFF")
endif()

if (WWIV_BUILD_TESTS)
  add_executable(fsed_tests
    "spell_config_test.cpp"
    "spell_test.cpp"
    "spell_text_test.cpp"
    "spell_walk_test.cpp"
    "fsed_test_main.cpp"
    "model_test.cpp"
    "fsed_model_test.cpp"
    "view_rows_test.cpp"
  )

  include(GoogleTest)
  target_link_libraries(fsed_tests fsed core_fixtures core common sdk GTest::gtest)
  gtest_discover_tests(fsed_tests EXTRA_ARGS "--wwiv_testdata=${CMAKE_CURRENT_SOURCE_DIR}/testdata")
  
endif()