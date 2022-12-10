# CMake for WWIV Daemon

set(WWIVD_SOURCES 
	ips.cpp
	nets.cpp
    node_manager.cpp
    wwivd_http.cpp
    wwivd_non_http.cpp
    )

set(WWIVD_MAIN wwivd.cpp)

if (UNIX)
  find_package (Threads)
  list(APPEND WWIVD_SOURCES wwivd_unix.cpp)
  if(CMAKE_SYSTEM_NAME MATCHES "SunOS.*")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -lsocket -lnsl -lkstat -lssp")
  endif()
elseif(WIN32)
    list(APPEND WWIVD_SOURCES wwivd_win.cpp)
elseif(OS2)
  list(APPEND WWIVD_SOURCES wwivd_os2.cpp)
endif()

add_library(wwivd_lib ${WWIVD_SOURCES})
set_max_warnings(wwivd_lib)
target_link_libraries(wwivd_lib sdk core sdk ${CMAKE_THREAD_LIBS_INIT})
add_executable(wwivd ${WWIVD_MAIN})
target_link_libraries(wwivd wwivd_lib)

## Tests
if (WWIV_BUILD_TESTS)

  set(test_sources
    wwivd_non_http_test.cpp
  )
  list(APPEND test_sources wwivd_test_main.cpp)

  add_executable(wwivd_tests ${test_sources})
  set_max_warnings(wwivd_tests)
  target_link_libraries(wwivd_tests wwivd_lib core_fixtures core sdk GTest::gtest)
  gtest_discover_tests(wwivd_tests)

endif()


