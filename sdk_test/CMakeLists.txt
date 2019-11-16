include_directories(${GTEST_INCLUDE_DIRS})

set(test_sources
  callout_test.cpp
  chains_test.cpp
  config_test.cpp
  contact_test.cpp
  datetime_test.cpp
  email_test.cpp
  fido_util_test.cpp
  flo_test.cpp
  ftn_msgdupe_test.cpp
  msgapi_test.cpp
  names_test.cpp
  network_test.cpp
  parsed_message_test.cpp
  phone_numbers_test.cpp
  qscan_test.cpp
  sdk_helper.cpp
  subxtr_test.cpp
  type2_text_test.cpp
  user_test.cpp
  ansi/ansi_test.cpp
  ansi/framebuffer_test.cpp
  ansi/makeansi_test.cpp
  files/allow_test.cpp
  fido/fido_address_test.cpp
  fido/nodelist_test.cpp
  net/callouts_test.cpp
  net/packets_test.cpp
)
if (WIN32)
  list(APPEND test_sources sdk_test_main.cpp)
endif()

add_executable(sdk_tests ${test_sources})

if (UNIX)
target_link_libraries(sdk_tests core core_fixtures gtest_main sdk)
elseif(WIN32)
target_link_libraries(sdk_tests core core_fixtures gtest sdk)
endif()

add_test(sdk_tests, sdk_tests)
