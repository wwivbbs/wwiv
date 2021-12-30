# CMake for WWIV 5

set(SOURCES 
 net_cmdline.cpp
 netdat.cpp
)

set_max_warnings()

add_library(net_core ${SOURCES})
target_link_libraries(net_core fmt::fmt-header-only)

## Tests
if (WWIV_BUILD_TESTS)

  set(test_sources
    netdat_test.cpp
    net_cmdline_test.cpp
    net_core_test_main.cpp
  )
  set_max_warnings()

  add_executable(net_core_tests ${test_sources})
  target_link_libraries(net_core_tests net_core core_fixtures GTest::gtest sdk)

  gtest_discover_tests(net_core_tests)

endif()
