# CMake for WWIV 5

set(SOURCES 
 net_cmdline.cpp
 netdat.cpp
)

add_library(net_core ${SOURCES})
set_max_warnings(net_core)
target_link_libraries(net_core fmt::fmt-header-only)

## Tests
if (WWIV_BUILD_TESTS)

  set(test_sources
    netdat_test.cpp
    net_cmdline_test.cpp
    net_core_test_main.cpp
  )

  add_executable(net_core_tests ${test_sources})
  set_max_warnings(net_core_tests)
  target_link_libraries(net_core_tests net_core core_fixtures GTest::gtest sdk)

  gtest_discover_tests(net_core_tests)

endif()
