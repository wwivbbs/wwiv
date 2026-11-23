# Regenerates the lexer and parser in generated/ from BasicLexer.g4 and
# BasicParser.g4, then records the grammar's hash in generated/grammar.sha256.
#
#   cmake -DJAVA=<java> -DJAR=<antlr jar> -DSOURCE_DIR=<repo root> -P generate_parser.cmake
#
# ANTLR runs from the repo root with relative paths so the "Generated from"
# comment in each file doesn't depend on where the repo is checked out.

include("${CMAKE_CURRENT_LIST_DIR}/grammar_hash.cmake")

# The build runs this whenever a grammar file looks newer than the generated
# files, which on a fresh checkout depends only on the order git wrote them. If
# the grammar matches what generated/ was made from, just mark the files as up
# to date instead of rewriting them (ANTLR's output on Windows differs, e.g. in
# line endings, which would show up as local changes).
wwivbasic_grammar_hash("${SOURCE_DIR}" "${JAR}" hash)
set(hash_file "${SOURCE_DIR}/generated/grammar.sha256")
if(EXISTS "${hash_file}")
  file(STRINGS "${hash_file}" checked_in_hash LIMIT_COUNT 1)
  if(checked_in_hash STREQUAL hash)
    file(GLOB generated_files "${SOURCE_DIR}/generated/*")
    file(TOUCH_NOCREATE ${generated_files})
    return()
  endif()
endif()

execute_process(
  COMMAND "${JAVA}" -jar "${JAR}" -Dlanguage=Cpp -package wwivbasic -visitor -no-listener
          -Werror -o generated BasicLexer.g4 BasicParser.g4
  WORKING_DIRECTORY "${SOURCE_DIR}"
  RESULT_VARIABLE result)
if(NOT result EQUAL 0)
  message(FATAL_ERROR "ANTLR failed to generate the parser (exit code ${result})")
endif()

file(WRITE "${hash_file}" "${hash}\n")
