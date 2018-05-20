include_directories(../deps/googletest/googletest/include)
include_directories(..)


set(test_sources
  wwivd_non_http_test.cpp
)

if(UNIX) 
  add_definitions ("-Wall")
endif()

if(NOT UNIX)
  list(APPEND test_sources wwivd_test_main.cpp)
endif()
add_executable(wwivd_tests ${test_sources})

if(UNIX)
  target_link_libraries(wwivd_tests wwivd_lib core core_fixtures sdk gtest_main)
else()
  target_link_libraries(wwivd_tests wwivd_lib core_fixtures core sdk gtest)
endif()

add_test(wwivd_tests wwivd_tests)
