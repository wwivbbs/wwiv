# CMake for WWIV Daemon
include_directories(../deps/cereal/include)

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

set_max_warnings()

add_library(wwivd_lib ${WWIVD_SOURCES})
target_link_libraries(wwivd_lib sdk core sdk ${CMAKE_THREAD_LIBS_INIT})
add_executable(wwivd ${WWIVD_MAIN})
target_link_libraries(wwivd wwivd_lib)

## Tests
if (WWIV_BUILD_TESTS)

include(GoogleTest)
include_directories(${GTEST_INCLUDE_DIRS})

set(test_sources
  wwivd_non_http_test.cpp
)
list(APPEND test_sources wwivd_test_main.cpp)

set_max_warnings()
add_executable(wwivd_tests ${test_sources})
target_link_libraries(wwivd_tests wwivd_lib core_fixtures core sdk gtest)
gtest_discover_tests(wwivd_tests)

endif()


