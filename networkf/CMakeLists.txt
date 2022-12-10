# CMake for WWIV NetworkF


add_library(networkf_lib networkf.cpp)
set_max_warnings(networkf_lib)
target_link_libraries(networkf_lib binkp_lib net_core core sdk fmt::fmt-header-only)

add_executable(networkf networkf_main.cpp)
target_link_libraries(networkf networkf_lib)

## Tests
if (WWIV_BUILD_TESTS)

  set(networkf_test_sources
    networkf_test.cpp
    networkf_test_main.cpp
  )

  add_executable(networkf_tests ${networkf_test_sources})
  set_max_warnings(networkf_tests)
  target_link_libraries(networkf_tests net_core core_fixtures GTest::gtest sdk sdk_fixtures networkf_lib)

  gtest_discover_tests(networkf_tests)

endif()
