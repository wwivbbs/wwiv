#
# Common CMake module for WWIV

message(VERBOSE "WWIV Common CMake Module.")

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules)
list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/cmake/Modules/sanitizers)
set (CMAKE_CXX_STANDARD 17)
set (CMAKE_CXX_STANDARD_REQUIRED ON)

option(WWIV_BUILD_TESTS "Build WWIV test programs" ON)

macro(ENSURE_MINIMUM_COMPILER_VERSIONS)
  # Set minimum GCC version
  # See https://stackoverflow.com/questions/14933172/how-can-i-add-a-minimum-compiler-version-requisite
  if (CMAKE_COMPILER_IS_GNUCC AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 8.3)
      message(FATAL_ERROR "Require at least gcc-8.3; found: ${CMAKE_CXX_COMPILER_VERSION}")
  endif()

  if (MSVC)
    if (${MSVC_VERSION} LESS 1922)
      # See https://docs.microsoft.com/en-us/cpp/preprocessor/predefined-macros?view=vs-2019
      # for versions
      message(FATAL_ERROR "Require at least MSVC 2019 16.2 (1922); Found: ${MSVC_VERSION}")
    endif()
  endif()
endmacro(ENSURE_MINIMUM_COMPILER_VERSIONS)

if (UNIX)
  if (CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(LINUX TRUE)
  endif()

elseif (WIN32)

  if (MSVC)
    # Don't show warnings on using normal POSIX functions.  Maybe one day
    # We'll be using all C++ replacements for most things and can get rid
    # of this.
    add_definitions(/D_CRT_SECURE_NO_WARNINGS)
    add_definitions(/D_CRT_NONSTDC_NO_DEPRECATE)
    # Make Windows.h not so awful if included
    add_definitions(/D_WINSOCK_DEPRECATED_NO_WARNINGS)
    add_definitions(/DNOMINMAX)
    add_definitions(/DWIN32_LEAN_AND_MEAN=1)
    # Otherwise fmt will include windows.h and that breaks everything
    add_definitions(/DFMT_USE_WINDOWS_H=0)
    
    # Warning 26444 is too noisy to be useful for passing parameters to functions.
    # See https://developercommunity.visualstudio.com/content/problem/422153/warning-c26444-not-aligned-with-cppcoreguidelines.html
    add_definitions(/wd26444)

    # To silence cereal warnings that they know about already
    # bug: https://github.com/USCiLab/cereal/issues/456
    add_definitions(/D_SILENCE_CXX17_ITERATOR_BASE_CLASS_DEPRECATION_WARNING)
  endif(MSVC)
endif (UNIX)

if( NOT CMAKE_BUILD_TYPE )
  set( CMAKE_BUILD_TYPE "Debug" )
  message(STATUS "CMAKE_BUILD_TYPE not set; defaulting to Debug")
endif()
message(VERBOSE "CMAKE_BUILD_TYPE: ${CMAKE_BUILD_TYPE}")


IF(${CMAKE_BUILD_TYPE} STREQUAL Debug)
  message(VERBOSE "Defining _DEBUG macro for debug build")
  ADD_DEFINITIONS(-D_DEBUG)
ENDIF(${CMAKE_BUILD_TYPE} STREQUAL Debug)

macro(SET_MSVC_WARNING_LEVEL_4) 
  if(WIN32 AND MSVC)
    if(CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
      string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
      set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()
  endif()
endmacro()

macro(SET_MAX_WARNINGS)
  if(UNIX) 
    add_definitions ("-Wall")
  endif()
  if(WIN32)
    SET_MSVC_WARNING_LEVEL_4()
  endif()
endmacro()

MACRO(MACRO_ENSURE_OUT_OF_SOURCE_BUILD)
  STRING(COMPARE EQUAL "${${PROJECT_NAME}_SOURCE_DIR}"
    "${${PROJECT_NAME}_BINARY_DIR}" insource)
  GET_FILENAME_COMPONENT(PARENTDIR ${${PROJECT_NAME}_SOURCE_DIR} PATH)
  STRING(COMPARE EQUAL "${${PROJECT_NAME}_SOURCE_DIR}"
    "${PARENTDIR}" insourcesubdir)
  IF(insource OR insourcesubdir)
    MESSAGE(FATAL_ERROR 
    "${PROJECT_NAME} requires an out of source build.
     Please see https://github.com/wwivbbs/wwiv#out-of-source-build-warning
     This process created the file `CMakeCache.txt' and the directory `CMakeFiles'.
     Please delete them before re-running cmake."
    )
  ENDIF(insource OR insourcesubdir)
ENDMACRO(MACRO_ENSURE_OUT_OF_SOURCE_BUILD)

  
message(VERBOSE "CMAKE_CXX_FLAGS: ${CMAKE_CXX_FLAGS}")

function(zip output_file input_files working_dir)
  message(DEBUG "zip: ${output_file} : ${input_files}")
  add_custom_command(
    COMMAND ${CMAKE_COMMAND} -E tar "cf" "${output_file}" --format=zip -- ${input_files}
    WORKING_DIRECTORY "${working_dir}"
    OUTPUT  "${output_file}"
    DEPENDS ${input_files}
    COMMENT "Creating ZIP file: ${output_file}."
    )
endfunction()

function(create_datafile_archive arc dir)
  message(DEBUG "create_datafile_archive: dir: ${dir}: ${WWIV_RELEASE_DIR}/${arc}.zip")
  file(GLOB_RECURSE DATA_FILES "${dir}/*")
  zip("${WWIV_RELEASE_DIR}/${arc}.zip" "${DATA_FILES}" "${dir}/")
  add_custom_target("${arc}_archive" ALL DEPENDS "${WWIV_RELEASE_DIR}/${arc}.zip")
endfunction()

