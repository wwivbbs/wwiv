cmake_minimum_required(VERSION 3.20)

# wwivbasic builds on its own, or inside another project (WWIV) with
# add_subdirectory. On its own it builds basicrun and the tests, finds vcpkg
# itself and uses the copy of WWIV's core/ library in core/. Inside another
# project it builds only the interpreter library (wwivbasic_interpreter) and
# uses that project's vcpkg and its `core` target, which must be added (and
# "core/..." headers be on the include path) before this directory.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(WWIVBASIC_STANDALONE ON)
else()
  set(WWIVBASIC_STANDALONE OFF)
endif()

# Dependencies come from vcpkg (see vcpkg.json, which pins the versions with
# builtin-baseline). When building on its own, vcpkg is found from, in order:
#   1. -DCMAKE_TOOLCHAIN_FILE=...
#   2. the VCPKG_ROOT environment variable
#   3. the vcpkg submodule of a WWIV checkout next to this one (../wwiv/vcpkg)
if(WWIVBASIC_STANDALONE AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  if(DEFINED ENV{VCPKG_ROOT})
    set(WWIVBASIC_VCPKG_ROOT "$ENV{VCPKG_ROOT}")
  elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../wwiv/vcpkg/scripts/buildsystems/vcpkg.cmake")
    get_filename_component(WWIVBASIC_VCPKG_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../wwiv/vcpkg" ABSOLUTE)
  else()
    message(FATAL_ERROR
      "vcpkg not found. Set VCPKG_ROOT to a vcpkg checkout (for example the vcpkg "
      "submodule of WWIV), or check out WWIV next to this directory as ../wwiv.")
  endif()
  message(STATUS "Using vcpkg from ${WWIVBASIC_VCPKG_ROOT}")
  set(CMAKE_TOOLCHAIN_FILE "${WWIVBASIC_VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
    CACHE FILEPATH "Vcpkg toolchain file")
endif()

project(wwivbasic LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(WWIVBASIC_BUILD_TOOLS "Build basicrun" ${WWIVBASIC_STANDALONE})
option(WWIVBASIC_BUILD_TESTS "Build the unit and golden tests (needs basicrun)"
       ${WWIVBASIC_STANDALONE})
if(WWIVBASIC_BUILD_TESTS AND NOT WWIVBASIC_BUILD_TOOLS)
  message(FATAL_ERROR "WWIVBASIC_BUILD_TESTS needs WWIVBASIC_BUILD_TOOLS (the golden tests run basicrun)")
endif()

find_package(antlr4-runtime REQUIRED)
find_package(fmt CONFIG REQUIRED)

add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/src")

# WWIV's core/ library: the embedder's when there is one, otherwise our copy.
if(TARGET core)
  message(STATUS "wwivbasic: using the existing core target")
else()
  include_directories("${CMAKE_CURRENT_SOURCE_DIR}")
  add_subdirectory(core)
endif()

# The lexer and parser are generated from BasicLexer.g4 and BasicParser.g4 by
# the ANTLR tool and checked in under generated/, so building doesn't need
# Java. When Java is available, editing a grammar file regenerates them; commit
# the result along with the grammar change. generated/grammar.sha256 records
# which grammar they were generated from.
set(ANTLR4_JAR_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/antlr/antlr-4.13.2-complete.jar")
set(WWIVBASIC_GENERATED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/generated")
set(WWIVBASIC_GENERATED_SOURCES
    "${WWIVBASIC_GENERATED_DIR}/BasicLexer.cpp"
    "${WWIVBASIC_GENERATED_DIR}/BasicParser.cpp"
    "${WWIVBASIC_GENERATED_DIR}/BasicParserBaseVisitor.cpp"
    "${WWIVBASIC_GENERATED_DIR}/BasicParserVisitor.cpp")
set(WWIVBASIC_GENERATED_FILES
    ${WWIVBASIC_GENERATED_SOURCES}
    "${WWIVBASIC_GENERATED_DIR}/BasicLexer.h"
    "${WWIVBASIC_GENERATED_DIR}/BasicParser.h"
    "${WWIVBASIC_GENERATED_DIR}/BasicParserBaseVisitor.h"
    "${WWIVBASIC_GENERATED_DIR}/BasicParserVisitor.h"
    "${WWIVBASIC_GENERATED_DIR}/grammar.sha256")

option(WWIVBASIC_GENERATE_PARSER "Regenerate the parser when the grammar changes (needs Java)" ON)
if(WWIVBASIC_GENERATE_PARSER)
  find_package(Java 11 COMPONENTS Runtime)
endif()

include("${CMAKE_CURRENT_SOURCE_DIR}/cmake/grammar_hash.cmake")
wwivbasic_grammar_hash("${CMAKE_CURRENT_SOURCE_DIR}" "${ANTLR4_JAR_LOCATION}" WWIVBASIC_GRAMMAR_HASH)
set(WWIVBASIC_CHECKED_IN_HASH "")
if(EXISTS "${WWIVBASIC_GENERATED_DIR}/grammar.sha256")
  file(STRINGS "${WWIVBASIC_GENERATED_DIR}/grammar.sha256" WWIVBASIC_CHECKED_IN_HASH LIMIT_COUNT 1)
endif()
set(WWIVBASIC_GENERATE_COMMAND
    "${CMAKE_COMMAND}" "-DJAVA=${Java_JAVA_EXECUTABLE}" "-DJAR=${ANTLR4_JAR_LOCATION}"
    "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_parser.cmake")

if(Java_FOUND)
  if(NOT WWIVBASIC_GRAMMAR_HASH STREQUAL WWIVBASIC_CHECKED_IN_HASH)
    message(STATUS "wwivbasic: the grammar changed; regenerating the parser")
    execute_process(COMMAND ${WWIVBASIC_GENERATE_COMMAND} COMMAND_ERROR_IS_FATAL ANY)
  endif()
  # Regenerate on the next build whenever a grammar file is edited.
  add_custom_command(
    OUTPUT ${WWIVBASIC_GENERATED_FILES}
    COMMAND ${WWIVBASIC_GENERATE_COMMAND}
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/BasicLexer.g4"
            "${CMAKE_CURRENT_SOURCE_DIR}/BasicParser.g4"
            "${ANTLR4_JAR_LOCATION}"
            "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_parser.cmake"
    COMMENT "Generating the wwivbasic lexer and parser"
    VERBATIM)
elseif(NOT WWIVBASIC_GRAMMAR_HASH STREQUAL WWIVBASIC_CHECKED_IN_HASH)
  message(FATAL_ERROR
    "BasicLexer.g4 or BasicParser.g4 changed since the parser in generated/ was "
    "generated, and Java (11 or newer) wasn't found to regenerate it. Install Java, "
    "or restore the grammar files.")
else()
  message(STATUS "wwivbasic: using the checked-in parser in generated/")
endif()

# vcpkg provides antlr4_shared on dynamic triplets (Windows default) and
# antlr4_static elsewhere.
if(TARGET antlr4_shared)
  set(WWIVBASIC_ANTLR_LIB antlr4_shared)
else()
  set(WWIVBASIC_ANTLR_LIB antlr4_static)
  add_definitions(-DANTLR4CPP_STATIC)
endif()

# Compiler warnings for our own code (src/). Warnings don't fail the build.
# The generated ANTLR sources and the vendored core/ library are left alone.
if(MSVC)
  set(WWIVBASIC_WARNINGS /W4)
  set(WWIVBASIC_NO_WARNINGS /W0)
else()
  set(WWIVBASIC_WARNINGS -Wall -Wextra)
  set(WWIVBASIC_NO_WARNINGS -w)
endif()

# The generated lexer and parser, in their own target so they build without
# our warning flags. Their headers are SYSTEM includes so their warnings don't
# show up in our code either.
add_library(wwivbasic_parser OBJECT ${WWIVBASIC_GENERATED_SOURCES})
target_include_directories(wwivbasic_parser SYSTEM PUBLIC ${ANTLR4_INCLUDE_DIR}
                           "${WWIVBASIC_GENERATED_DIR}")
target_link_libraries(wwivbasic_parser PUBLIC ${WWIVBASIC_ANTLR_LIB})
target_compile_options(wwivbasic_parser PRIVATE ${WWIVBASIC_NO_WARNINGS})

add_library(wwivbasic_interpreter
            "src/context.cpp"
            "src/executor.cpp"
            "src/function_def_visitor.cpp"
            "src/interpreter.cpp"
            "src/stdio_console.cpp"
            "src/utils.cpp"
            "src/value.cpp"
            "src/stdlib/collections.cpp"
            "src/stdlib/common.cpp"
            "src/stdlib/numbers.cpp"
            "src/stdlib/strings.cpp"
)
# Public headers (the embedding API) are in include/wwivbasic/.
target_include_directories(wwivbasic_interpreter PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_link_libraries(wwivbasic_interpreter wwivbasic_parser fmt::fmt core)
target_compile_definitions(wwivbasic_interpreter PUBLIC _CRT_SECURE_NO_WARNINGS)
# The public headers need C++20 or newer; this makes targets that link the
# interpreter build with C++23 even if the embedder defaults to an older
# standard (WWIV uses C++17).
target_compile_features(wwivbasic_interpreter PUBLIC cxx_std_23)
target_compile_options(wwivbasic_interpreter PRIVATE ${WWIVBASIC_WARNINGS})

if(WWIVBASIC_BUILD_TOOLS)
  add_executable(basicrun
                 "src/basicrun.cpp"
                 "src/wwiv_stub.cpp")
  target_link_libraries(basicrun wwivbasic_interpreter)
  target_compile_options(basicrun PRIVATE ${WWIVBASIC_WARNINGS})
endif()

if(WWIVBASIC_BUILD_TESTS)
  enable_testing()
  find_package(GTest CONFIG REQUIRED)

  add_executable(wwivbasic_tests
                 "src/utils_test.cpp"
                 "src/stdlib/strings_test.cpp"
                 "src/interpreter_test.cpp"
                 "src/embedding_test.cpp"
                 "src/value_test.cpp"
  )

  target_link_libraries(wwivbasic_tests PRIVATE GTest::gtest_main wwivbasic_interpreter)
  target_compile_options(wwivbasic_tests PRIVATE ${WWIVBASIC_WARNINGS})

  # Unit tests
  include(GoogleTest)
  gtest_discover_tests(wwivbasic_tests)

  # Golden tests: each tests/golden/NAME.bas is run with basicrun and its stdout
  # compared with NAME.expected; NAME.input, if present, is its stdin. See
  # tests/run_golden.cmake.
  # Golden tests without a NAME.input file get this empty file as stdin.
  set(WWIVBASIC_EMPTY_INPUT "${CMAKE_CURRENT_BINARY_DIR}/golden_empty.input")
  file(WRITE "${WWIVBASIC_EMPTY_INPUT}" "")
  file(GLOB WWIVBASIC_GOLDEN_SCRIPTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/*.bas")
  foreach(script ${WWIVBASIC_GOLDEN_SCRIPTS})
    get_filename_component(name "${script}" NAME_WE)
    get_filename_component(dir "${script}" DIRECTORY)
    add_test(NAME golden.${name}
             COMMAND ${CMAKE_COMMAND}
                     -DBASICRUN=$<TARGET_FILE:basicrun>
                     -DSCRIPT=${script}
                     -DEXPECTED=${dir}/${name}.expected
                     -DEMPTY_INPUT=${WWIVBASIC_EMPTY_INPUT}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_golden.cmake)
  endforeach()

  # basicrun accepts relative script paths that start with "./".
  add_test(NAME basicrun.dot_relative_path
           COMMAND basicrun ./for.bas
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden)
  set_tests_properties(basicrun.dot_relative_path PROPERTIES PASS_REGULAR_EXPRESSION "^1\n2\n")
endif()
