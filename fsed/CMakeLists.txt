# CMake for WWIV
Include(FindWWIVCurses)

set(SOURCES 
 commands.cpp
 common.cpp
 fsed.cpp
 line.cpp
 model.cpp
 spell.cpp
 spell_text.cpp
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
  find_package(Hunspell QUIET)
  if (Hunspell_FOUND)
    message(STATUS "WWIV_HUNSPELL is ON (found Hunspell ${Hunspell_VERSION})")
    target_link_libraries(fsed Hunspell::Hunspell)
    target_compile_definitions(fsed PRIVATE WWIV_HAS_HUNSPELL)
  else()
    message(STATUS "WWIV_HUNSPELL is ON, but Hunspell was not found; FSED spell check is disabled")
  endif()
else()
  message(STATUS "WWIV_HUNSPELL is OFF")
endif()

if (WWIV_BUILD_TESTS)
  add_executable(fsed_tests
    "spell_test.cpp"
    "spell_text_test.cpp"
    "fsed_test_main.cpp"
    "model_test.cpp"
    "fsed_model_test.cpp"
  )

  include(GoogleTest)
  target_link_libraries(fsed_tests fsed core_fixtures core common sdk GTest::gtest)
  gtest_discover_tests(fsed_tests EXTRA_ARGS "--wwiv_testdata=${CMAKE_CURRENT_SOURCE_DIR}/testdata")
  
endif()