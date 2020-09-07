# CMake for WWIV
include_directories(../deps/cereal/include)
Include(FindWWIVCurses)

set(SOURCES 
 commands.cpp
 common.cpp
 fsed.cpp
 line.cpp
 model.cpp
 view.cpp
)

add_library(
  fsed 
  ${SOURCES} 
) 

target_link_libraries(
  fsed 
  fmt::fmt-header-only
  local_io 
  common
  core 
  sdk 
)

set_max_warnings()

target_include_directories(fsed PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
