# Runs one golden test: executes BASICRUN on SCRIPT and compares stdout with
# EXPECTED. Line endings are normalized so tests pass on Windows and Unix.
#
# Usage:
#   cmake -DBASICRUN=<exe> -DSCRIPT=<file.bas> -DEXPECTED=<file.expected>
#         -DEMPTY_INPUT=<empty file> -P run_golden.cmake
#
# If the expected file's first line is "#exit <n>", the script must exit with
# code <n> (default 0) and that line is not part of the expected output.

foreach(var BASICRUN SCRIPT EXPECTED EMPTY_INPUT)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "run_golden.cmake: ${var} is not set")
  endif()
endforeach()

get_filename_component(script_dir "${SCRIPT}" DIRECTORY)
get_filename_component(script_name "${SCRIPT}" NAME_WE)

# Script input (GETS, YN, ...) comes from NAME.input if it exists; otherwise
# stdin is empty, so a test never waits on the terminal.
set(input_file "${script_dir}/${script_name}.input")
if(NOT EXISTS "${input_file}")
  set(input_file "${EMPTY_INPUT}")
endif()

execute_process(
  COMMAND "${BASICRUN}" "${SCRIPT}"
  WORKING_DIRECTORY "${script_dir}"
  INPUT_FILE "${input_file}"
  OUTPUT_VARIABLE actual
  ERROR_VARIABLE stderr
  RESULT_VARIABLE exit_code
)

file(READ "${EXPECTED}" expected)
string(REPLACE "\r\n" "\n" expected "${expected}")
string(REPLACE "\r\n" "\n" actual "${actual}")

set(want_exit 0)
if(expected MATCHES "^#exit ([0-9]+)\n")
  set(want_exit "${CMAKE_MATCH_1}")
  string(REGEX REPLACE "^#exit [0-9]+\n" "" expected "${expected}")
endif()

set(failed FALSE)
if(NOT "${exit_code}" STREQUAL "${want_exit}")
  message("Exit code: got ${exit_code}, want ${want_exit}")
  set(failed TRUE)
endif()
if(NOT "${actual}" STREQUAL "${expected}")
  message("Output mismatch for ${SCRIPT}")
  message("----- expected -----\n${expected}")
  message("----- actual -------\n${actual}")
  set(failed TRUE)
endif()
if(failed)
  if(stderr)
    message("----- stderr -------\n${stderr}")
  endif()
  message(FATAL_ERROR "Golden test failed: ${SCRIPT}")
endif()
