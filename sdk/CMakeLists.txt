
set(COMMON_SOURCES
  "arword.cpp"
  "bbslist.cpp"
  "chains.cpp"
  "config.cpp"
  "config430.cpp"
  "gfiles.cpp"
  "instance.cpp"
  "instance_message.cpp"
  "names.cpp"
  "phone_numbers.cpp"
  "qscan.cpp"
  "ssm.cpp"
  "status.cpp"
  "subxtr.cpp"
  "qwk_config.cpp"
  "user.cpp"
  "usermanager.cpp"
  "wwivd_config.cpp"
  "acs/acs.cpp"
  "acs/eval.cpp"
  "acs/expr.cpp"
  "ansi/ansi.cpp"
  "ansi/framebuffer.cpp"
  "ansi/makeansi.cpp"
  "ansi/localio_screen.cpp"
  "ansi/vscreen.cpp"
  "conf/conf.cpp"
  "fido/backbone.cpp"
  "fido/fido_address.cpp"
  "fido/fido_callout.cpp"
  "fido/fido_directories.cpp"
  "fido/fido_packets.cpp"
  "fido/fido_util.cpp"
  "fido/flo_file.cpp"
  "fido/nodelist.cpp"
  "files/allow.cpp"
  "files/arc.cpp"
  "files/dirs.cpp"
  "files/diz.cpp"
  "files/file_record.cpp"
  "files/files.cpp"
  "files/files_ext.cpp"
  "files/tic.cpp"
  "menus/menu.cpp"
  "menus/menu_set.cpp"
  "msgapi/email_wwiv.cpp"
  "msgapi/message_api.cpp"
  "msgapi/message_api_wwiv.cpp"
  "msgapi/message_area_wwiv.cpp"
  "msgapi/message_wwiv.cpp"
  "msgapi/parsed_message.cpp"
  "msgapi/type2_text.cpp"
  "net/binkp.cpp"
  "net/callout.cpp"
  "net/connect.cpp"
  "net/contact.cpp"
  "net/ftn_msgdupe.cpp"
  "net/callouts.cpp"
  "net/net.cpp"
  "net/packets.cpp"
  "net/networks.cpp"
  "net/subscribers.cpp"
  "value/value.cpp"
  "value/valueprovider.cpp"
   )

add_library(sdk ${COMMON_SOURCES})
set_max_warnings(sdk)
target_link_libraries(sdk PRIVATE local_io)
target_link_libraries(sdk PUBLIC core fmt::fmt-header-only)

## Tests
if (WWIV_BUILD_TESTS)

add_library(sdk_fixtures 
  "sdk_helper.cpp"
)
target_link_libraries(sdk_fixtures PUBLIC core fmt::fmt-header-only)

set(test_sources
  "bbslist_test.cpp"
  "chains_test.cpp"
  "config_test.cpp"
  "datetime_test.cpp"
  "instance_message_test.cpp"
  "names_test.cpp"
  "phone_numbers_test.cpp"
  "qscan_test.cpp"
  "subxtr_test.cpp"
  "user_test.cpp"

  "acs/ar_test.cpp"
  "acs/expr_test.cpp"
  "acs/value_test.cpp"
  "ansi/ansi_test.cpp"
  "ansi/framebuffer_test.cpp"
  "ansi/makeansi_test.cpp"
  "fido/fido_address_test.cpp"
  "fido/fido_packets_test.cpp"
  "fido/fido_util_test.cpp"
  "fido/flo_test.cpp"
  "fido/nodelist_test.cpp"
  "fido/test/ftn_directories_test_helper_test.cpp"
  "files/allow_test.cpp"
  "files/dirs_test.cpp"
  "files/diz_test.cpp"
  "files/files_ext_test.cpp"
  "files/files_test.cpp"
  "files/tic_test.cpp"
  "msgapi/email_test.cpp"
  "msgapi/msgapi_test.cpp"
  "msgapi/parsed_message_test.cpp"
  "msgapi/type2_text_test.cpp"
  "net/callout_test.cpp"
  "net/callouts_test.cpp"
  "net/contact_test.cpp"
  "net/ftn_msgdupe_test.cpp"
  "net/network_test.cpp"
  "net/packets_test.cpp"
)
list(APPEND test_sources sdk_test_main.cpp)


add_executable(sdk_tests ${test_sources})
set_max_warnings(sdk_tests)
target_link_libraries(sdk_tests core core_fixtures GTest::gtest sdk sdk_fixtures)

gtest_discover_tests(sdk_tests)


endif()