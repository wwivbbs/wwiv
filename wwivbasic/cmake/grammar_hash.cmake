# Computes a hash of the grammar files and the ANTLR tool version, used to tell
# whether the checked-in parser in generated/ is up to date. Line endings are
# normalized so a CRLF checkout on Windows hashes the same as an LF one.
function(wwivbasic_grammar_hash source_dir jar out_var)
  get_filename_component(jar_name "${jar}" NAME)
  set(all "${jar_name}\n")
  foreach(grammar BasicLexer.g4 BasicParser.g4)
    file(READ "${source_dir}/${grammar}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(APPEND all "${grammar}\n${text}")
  endforeach()
  string(SHA256 hash "${all}")
  set(${out_var} "${hash}" PARENT_SCOPE)
endfunction()
