cmake_minimum_required(VERSION 3.20)

# Dependencies come from vcpkg (see vcpkg.json, which pins the versions with
# builtin-baseline). vcpkg is found from, in order:
#   1. -DCMAKE_TOOLCHAIN_FILE=...
#   2. the VCPKG_ROOT environment variable
#   3. the vcpkg submodule of a WWIV checkout next to this one (../wwiv/vcpkg)
if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
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

find_package(antlr4-runtime REQUIRED)
find_package(antlr4-generator REQUIRED)
find_package(fmt CONFIG REQUIRED)
enable_testing()
find_package(GTest CONFIG REQUIRED)

include_directories(${CMAKE_SOURCE_DIR})
include_directories("${CMAKE_SOURCE_DIR}/src")
add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
add_subdirectory(core)

# The ANTLR tool used to generate the lexer and parser.
set(ANTLR4_JAR_LOCATION ${PROJECT_SOURCE_DIR}/antlr/antlr-4.13.2-complete.jar)

# vcpkg provides antlr4_shared on dynamic triplets (Windows default) and
# antlr4_static elsewhere.
if(TARGET antlr4_shared)
  set(WWIVBASIC_ANTLR_LIB antlr4_shared)
else()
  set(WWIVBASIC_ANTLR_LIB antlr4_static)
  add_definitions(-DANTLR4CPP_STATIC)
endif()

# using /MD flag for antlr4_runtime (for Visual C++ compilers only)
set(ANTLR4_WITH_STATIC_CRT OFF)

# generate lexer
antlr4_generate(wwivbasic_lexer
                ${CMAKE_CURRENT_SOURCE_DIR}/BasicLexer.g4
                LEXER
                FALSE
                FALSE
                "wwivbasic"
)

# generate parser
antlr4_generate(wwivbasic_parser
                ${CMAKE_CURRENT_SOURCE_DIR}/BasicParser.g4
                PARSER
                FALSE
                TRUE
                "wwivbasic"
                "${ANTLR4_TOKEN_FILES_wwivbasic_lexer}"
                "${ANTLR4_TOKEN_DIRECTORY_wwivbasic_lexer}"
)
 
add_library(wwivbasic_interpreter
            "src/context.cpp"
            "src/executor.cpp"
            "src/function_def_visitor.cpp"
            "src/interpreter.cpp"
            "src/utils.cpp"
            "src/value.cpp"
            "src/stdlib/common.cpp"
            "src/stdlib/numbers.cpp"
            "src/stdlib/strings.cpp"
            ${ANTLR4_SRC_FILES_wwivbasic_lexer} 
            ${ANTLR4_SRC_FILES_wwivbasic_parser}
)

add_executable(wwivbasic_tests
               "src/utils_test.cpp"
               "src/stdlib/strings_test.cpp"
               "src/interpreter_test.cpp"
)

target_link_libraries(wwivbasic_tests PRIVATE GTest::gtest_main wwivbasic_interpreter)


target_include_directories(wwivbasic_interpreter PUBLIC ${ANTLR4_INCLUDE_DIR} 
                           ${ANTLR4_INCLUDE_DIR_wwivbasic_lexer} 
                           ${ANTLR4_INCLUDE_DIR_wwivbasic_parser})
target_link_libraries(wwivbasic_interpreter ${WWIVBASIC_ANTLR_LIB} fmt::fmt core)
target_compile_definitions(wwivbasic_interpreter PUBLIC _CRT_SECURE_NO_WARNINGS)


add_executable(basicrun
               "src/basicrun.cpp")
target_link_libraries(basicrun wwivbasic_interpreter)

# Unit tests
include(GoogleTest)
gtest_discover_tests(wwivbasic_tests)

# Golden tests: each tests/golden/NAME.bas is run with basicrun and its stdout
# compared with NAME.expected. See tests/run_golden.cmake.
file(GLOB WWIVBASIC_GOLDEN_SCRIPTS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/*.bas")
foreach(script ${WWIVBASIC_GOLDEN_SCRIPTS})
  get_filename_component(name "${script}" NAME_WE)
  get_filename_component(dir "${script}" DIRECTORY)
  add_test(NAME golden.${name}
           COMMAND ${CMAKE_COMMAND}
                   -DBASICRUN=$<TARGET_FILE:basicrun>
                   -DSCRIPT=${script}
                   -DEXPECTED=${dir}/${name}.expected
                   -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_golden.cmake)
endforeach()

# basicrun accepts relative script paths that start with "./".
add_test(NAME basicrun.dot_relative_path
         COMMAND basicrun ./for.bas
         WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden)
set_tests_properties(basicrun.dot_relative_path PROPERTIES PASS_REGULAR_EXPRESSION "^1\n2\n")
