include_directories(${GTEST_INCLUDE_DIRS})
include_directories(${GMOCK_INCLUDE_DIR})
include_directories(../deps/cereal/include)
include(GoogleTest)

set(test_sources
  "net/callout_test.cpp"
  "chains_test.cpp"
  "config_test.cpp"
  "net/contact_test.cpp"
  "datetime_test.cpp"
  "instance_message_test.cpp"
  "msgapi/email_test.cpp"
  "fido/fido_util_test.cpp"
  "fido/flo_test.cpp"
  "net/ftn_msgdupe_test.cpp"
  "msgapi/msgapi_test.cpp"
  "names_test.cpp"
  "net/network_test.cpp"
  "msgapi/parsed_message_test.cpp"
  "phone_numbers_test.cpp"
  "qscan_test.cpp"
  "sdk_helper.cpp"
  "subxtr_test.cpp"
  "msgapi/type2_text_test.cpp"
  "user_test.cpp"
  "acs/ar_test.cpp"
  "acs/expr_test.cpp"
  "acs/value_test.cpp"
  "ansi/ansi_test.cpp"
  "ansi/framebuffer_test.cpp"
  "ansi/makeansi_test.cpp"
  "files/allow_test.cpp"
  "files/dirs_test.cpp"
  "files/diz_test.cpp"
  "files/files_test.cpp"
  "files/files_ext_test.cpp"
  "files/tic_test.cpp"
  "fido/fido_address_test.cpp"
  "fido/nodelist_test.cpp"
  "net/callouts_test.cpp"
  "net/packets_test.cpp"
)
list(APPEND test_sources sdk_test_main.cpp)

set_max_warnings()

add_executable(sdk_tests ${test_sources})

target_link_libraries(sdk_tests core core_fixtures gtest sdk)

gtest_discover_tests(sdk_tests)
