# CMake for WWIV 5

add_library(network2_lib
	context.cpp
	email.cpp
	post.cpp
	subs.cpp
	)
set_max_warnings(network2_lib)
target_link_libraries(network2_lib binkp_lib net_core core sdk)

add_executable(network2 network2.cpp)
set_max_warnings(network2)
target_link_libraries(network2 network2_lib)

## Tests
if (WWIV_BUILD_TESTS)

  set(network2_test_sources
    post_test.cpp
    network2_test_main.cpp
  )

  add_executable(network2_tests ${network2_test_sources})
  set_max_warnings(network2_tests)
  target_link_libraries(network2_tests network2_lib core_fixtures GTest::gtest sdk_fixtures)

  gtest_discover_tests(network2_tests)

endif()
