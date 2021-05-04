include_directories(${GTEST_INCLUDE_DIRS})
include(GoogleTest)

set(fixture_sources
  file_helper.cpp 
)

set(test_sources
  clock_test.cpp
  cp437_test.cpp
  crc32_test.cpp
  command_line_test.cpp
  datetime_test.cpp
  datafile_test.cpp
  eventbus_test.cpp
  fake_clock_test.cpp
  findfiles_test.cpp
  file_test.cpp
  inifile_test.cpp
  ip_address_test.cpp
  log_test.cpp
  md5_test.cpp
  os_test.cpp
  scope_exit_test.cpp
  semaphore_file_test.cpp
  stl_test.cpp
  strings_test.cpp
  textfile_test.cpp
  transaction_test.cpp
  uuid_test.cpp
  parser/ast_test.cpp
  parser/lexer_test.cpp
)
list(APPEND test_sources core_test_main.cpp)

set_max_warnings()

add_library(core_fixtures ${fixture_sources})
target_link_libraries(core_fixtures core)
add_executable(core_tests ${test_sources})
target_link_libraries(core_tests core_fixtures core gtest)
gtest_discover_tests(core_tests)

if(OS2)
  target_link_libraries(core_tests libcx)
endif()

