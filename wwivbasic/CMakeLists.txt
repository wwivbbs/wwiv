# CMake for wwivbasic, the ANTLR-based BASIC interpreter (WWIV_WWIVBASIC).
#
# Targets:
#   wwivbasic_interpreter  the interpreter library; embedders use the headers
#                          in this directory (wwivbasic/interpreter.h, ...)
#   basicrun               runs one .bas file from the command line
#   wwivbasic_tests        unit tests, plus golden tests run through basicrun
#                          (with WWIV_BUILD_TESTS)

find_package(antlr4-runtime CONFIG REQUIRED)

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

option(WWIVBASIC_GENERATE_PARSER "Regenerate the wwivbasic parser when the grammar changes (needs Java)" ON)
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
    "wwivbasic/BasicLexer.g4 or BasicParser.g4 changed since the parser in "
    "wwivbasic/generated/ was generated, and Java (11 or newer) wasn't found to "
    "regenerate it. Install Java, or restore the grammar files.")
else()
  message(STATUS "wwivbasic: using the checked-in parser in generated/")
endif()

# vcpkg provides antlr4_shared on dynamic triplets (Windows default) and
# antlr4_static elsewhere.
if(TARGET antlr4_shared)
  set(WWIVBASIC_ANTLR_LIB antlr4_shared)
  set(WWIVBASIC_ANTLR_DEFINITIONS "")
else()
  set(WWIVBASIC_ANTLR_LIB antlr4_static)
  set(WWIVBASIC_ANTLR_DEFINITIONS ANTLR4CPP_STATIC)
endif()

# On Windows, vcpkg copies antlr4-runtime.dll next to the programs in the build
# directory, but the release zip needs it too, or bbs.exe won't start (#1777).
if(WIN32 AND WWIV_INSTALL AND TARGET antlr4_shared)
  install(FILES "$<TARGET_FILE:antlr4_shared>" DESTINATION .)
endif()

# The generated lexer and parser, in their own target so they build without
# warnings. Their headers are SYSTEM includes so their warnings don't show up
# in our code either.
add_library(wwivbasic_parser OBJECT ${WWIVBASIC_GENERATED_SOURCES})
target_include_directories(wwivbasic_parser SYSTEM PUBLIC ${ANTLR4_INCLUDE_DIR}
                           "${WWIVBASIC_GENERATED_DIR}")
target_compile_definitions(wwivbasic_parser PUBLIC ${WWIVBASIC_ANTLR_DEFINITIONS})
target_link_libraries(wwivbasic_parser PUBLIC ${WWIVBASIC_ANTLR_LIB})
if(MSVC)
  target_compile_options(wwivbasic_parser PRIVATE /W0)
else()
  target_compile_options(wwivbasic_parser PRIVATE -w)
endif()

add_library(wwivbasic_interpreter
            "interpreter.cpp"
            "stdio_console.cpp"
            "value.cpp"
            "internal/context.cpp"
            "internal/executor.cpp"
            "internal/function_def_visitor.cpp"
            "internal/utils.cpp"
            "stdlib/collections.cpp"
            "stdlib/common.cpp"
            "stdlib/numbers.cpp"
            "stdlib/strings.cpp"
)
target_link_libraries(wwivbasic_interpreter PRIVATE wwivbasic_parser PUBLIC core fmt::fmt-header-only)
target_compile_definitions(wwivbasic_interpreter PRIVATE _CRT_SECURE_NO_WARNINGS)
set_max_warnings(wwivbasic_interpreter)

add_executable(basicrun
               "basicrun/basicrun.cpp"
               "basicrun/wwiv_stub.cpp")
target_link_libraries(basicrun wwivbasic_interpreter)
set_max_warnings(basicrun)

if(WWIV_BUILD_TESTS)
  add_executable(wwivbasic_tests
                 "debug_test.cpp"
                 "embedding_test.cpp"
                 "interpreter_test.cpp"
                 "value_test.cpp"
                 "internal/utils_test.cpp"
                 "stdlib/strings_test.cpp"
  )
  # The interpreter tests use internals, which need the ANTLR headers.
  target_link_libraries(wwivbasic_tests PRIVATE GTest::gtest_main wwivbasic_interpreter
                        wwivbasic_parser)
  set_max_warnings(wwivbasic_tests)
  gtest_discover_tests(wwivbasic_tests)

  # Golden tests: each tests/golden/NAME.bas is run with basicrun and its stdout
  # compared with NAME.expected; NAME.input, if present, is its stdin. See
  # tests/run_golden.cmake. Golden tests without a NAME.input file get this
  # empty file as stdin.
  set(WWIVBASIC_EMPTY_INPUT "${CMAKE_CURRENT_BINARY_DIR}/golden_empty.input")
  file(WRITE "${WWIVBASIC_EMPTY_INPUT}" "")
  file(GLOB WWIVBASIC_GOLDEN_SCRIPTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/*.bas")
  foreach(script ${WWIVBASIC_GOLDEN_SCRIPTS})
    get_filename_component(name "${script}" NAME_WE)
    get_filename_component(dir "${script}" DIRECTORY)
    add_test(NAME wwivbasic.golden.${name}
             COMMAND ${CMAKE_COMMAND}
                     -DBASICRUN=$<TARGET_FILE:basicrun>
                     -DSCRIPT=${script}
                     -DEXPECTED=${dir}/${name}.expected
                     -DEMPTY_INPUT=${WWIVBASIC_EMPTY_INPUT}
                     -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_golden.cmake)
  endforeach()

  # basicrun accepts relative script paths that start with "./".
  add_test(NAME wwivbasic.basicrun.dot_relative_path
           COMMAND basicrun ./for.bas
           WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden)
  set_tests_properties(wwivbasic.basicrun.dot_relative_path PROPERTIES
                       PASS_REGULAR_EXPRESSION "^1\n2\n")
endif()
