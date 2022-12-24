# CMake for WWIV
Include(FindWWIVCurses)

set(SOURCES 
 commands.cpp
 common.cpp
 fsed.cpp
 line.cpp
 model.cpp
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

if (WWIV_BUILD_TESTS)
  add_executable(fsed_tests
    "fsed_test_main.cpp"
    "model_test.cpp"
    "fsed_model_test.cpp"
  )

  include(GoogleTest)
  target_link_libraries(fsed_tests fsed core_fixtures core common sdk GTest::gtest)
  gtest_discover_tests(fsed_tests EXTRA_ARGS "--wwiv_testdata=${CMAKE_CURRENT_SOURCE_DIR}/testdata")
  
endif()