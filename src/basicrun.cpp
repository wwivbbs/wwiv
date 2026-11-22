// Runs a WWIVbasic script from the command line.
//
//   basicrun [--check] [--show_parsetree] [--trace] script.bas
//
// Script output goes to stdout; errors go to stderr as
// "file:line[:column]: error: message". Exits 1 on any error.
//
// The wwiv, wwiv.io and wwiv.data modules are stand-ins (see wwiv_stub.h);
// script input such as GETS and YN is read from stdin, one line per call.

#include "wwivbasic/interpreter.h"
#include "wwivbasic/stdio_console.h"
#include "wwiv_stub.h"
#include "core/command_line.h"
#include "core/log.h"
#include "fmt/format.h"

#include <cstdio>
#include <iostream>
#include <string>

using namespace wwivbasic;
using namespace wwiv::core;

int main(int argc, char* argv[]) {
  LoggerConfig config;
  // Log to the console only; don't leave basicrun.log files next to scripts.
  config.register_file_destinations = false;
  Logger::Init(argc, argv, config);

  CommandLine cmdline(argc, argv, {});
  cmdline.add_argument(BooleanCommandLineArgument(
    "show_parsetree", 't', "Display the parse tree before executing", false));
  cmdline.add_argument(
      BooleanCommandLineArgument("check", 'c', "Only parse the script; do not run it", false));
  cmdline.add_argument(
      BooleanCommandLineArgument("trace", 'v', "Log execution details to stderr", false));
  if (!cmdline.Parse()) {
    return 2;
  }

  if (cmdline.remaining().empty()) {
    fmt::print("{}", cmdline.GetHelp());
    return 2;
  }

  const std::string filename = cmdline.remaining().front();
  StdioConsole console;
  Interpreter interp(console);
  interp.set_trace(cmdline.barg("trace"));
  // Stand-in wwiv.* modules; input for GETS, GETKEY, YN and NY comes from stdin.
  WwivStub wwiv_stub(interp, console);

  RunOptions options;
  options.execute = !cmdline.barg("check") && !cmdline.barg("show_parsetree");
  const auto result = interp.run_file(filename, options);

  if (cmdline.barg("show_parsetree") && result.ok()) {
    fmt::print("{}\n", interp.parse_tree(filename).value_or(""));
  }

  std::cout.flush();
  for (const auto& err : result.errors) {
    fmt::print(stderr, "{}\n", err);
  }
  return result.ok() ? 0 : 1;
}
