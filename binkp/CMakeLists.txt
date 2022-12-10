# CMake for WWIV 5

set(SOURCES 
 binkp.cpp
 binkp_commands.cpp
 binkp_config.cpp
 cram.cpp
 file_manager.cpp
 net_log.cpp
 ppp_config.cpp
 remote.cpp
 transfer_file.cpp
 wfile_transfer_file.cpp
)

add_library(binkp_lib ${SOURCES})
target_link_libraries(binkp_lib fmt::fmt-header-only)
set_max_warnings(binkp_lib)

# Tests
if (WWIV_BUILD_TESTS)
    add_executable(binkp_tests 
        binkp_test.cpp
        binkp_config_test.cpp
        cram_test.cpp
        fake_connection.cpp
        file_manager_test.cpp
        transfer_file_test.cpp
        net_log_test.cpp
        ppp_config_test.cpp
        binkp_test_main.cpp
    )
    set_max_warnings(binkp_tests)
    target_link_libraries(binkp_tests binkp_lib core core_fixtures GTest::gmock GTest::gtest sdk)
    
    gtest_discover_tests(binkp_tests)

endif()
