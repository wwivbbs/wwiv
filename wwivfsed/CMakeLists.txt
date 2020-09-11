# CMake for WWIVfsed

set_max_warnings()

add_executable(wwivfsed wwivfsed.cpp)
target_link_libraries(wwivfsed common core fsed sdk)
