#
# Common CMake module for WWIV
#

message(STATUS "Loaded WWIV Common CMake Module.")

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules/sanitizers)
set(CMAKE_VERBOSE_MAKEFILE ON)
set(CMAKE_COLOR_MAKEFILE   ON)

# Note: these only work in cmake 3.1 or later. We work around that later.
set (CMAKE_CXX_STANDARD 14)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

option(WWIV_BUILD_TESTS "Build WWIV test programs" ON)

if (UNIX)
  if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(LINUX TRUE)
  endif()

  if (CMAKE_COMPILER_IS_GNUCXX)
    list(APPEND CMAKE_CXX_FLAGS "-frtti")
  
    if (CMAKE_VERSION VERSION_LESS "3.1")
      set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++14")
    endif ()
  endif()

elseif (WIN32)

  if (MSVC)
    message("Using MSVC, Setting warnings to match UNIX.")
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    add_definitions(/D_CRT_NONSTDC_NO_DEPRECATE)
    add_definitions(/D_WINSOCK_DEPRECATED_NO_WARNINGS)
    add_definitions(/DNOMINMAX)
    add_definitions(/DWIN32_LEAN_AND_MEAN=1)

    # TODO(rushfan): See if this is still needed even with the latest googletest update.
    # Rushfan addition to fix under latest Visual Studio 2017.
    if (CMAKE_CXX_COMPILER_VERSION VERSION_GREATER 19.11)
      # VS 2017 (15.5) : Disable warnings from from gtest code, using deprecated
      # code related to TR1 
      add_definitions(-D_SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING)
      message("Add flag for gtest: _SILENCE_TR1_NAMESPACE_DEPRECATION_WARNING")
    endif()
  endif(MSVC)
endif (UNIX)

function(SET_WARNING_LEVEL_4)
  message(STATUS "Setting Warning Level 4")
  if(WIN32 AND MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
      string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
  endif()
endfunction()

message("CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")
