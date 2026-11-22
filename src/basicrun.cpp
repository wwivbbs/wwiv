// Runs a WWIVbasic script from the command line.
//
//   basicrun [--check] [--show_parsetree] [--trace] script.bas
//
// Script output goes to stdout; errors go to stderr as
// "file:line[:column]: error: message". Exits 1 on any error.

#include "context.h"
#include "interpreter.h"
#include "core/command_line.h"
#include "core/log.h"
#include "fmt/format.h"

#include <cstdio>
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
  Context ctx;
  ctx.trace = cmdline.barg("trace");

  RunOptions options;
  options.execute = !cmdline.barg("check") && !cmdline.barg("show_parsetree");
  auto result = run_file(ctx, filename, options);

  if (cmdline.barg("show_parsetree") && result.ok()) {
    auto& su = ctx.sources.at(filename);
    fmt::print("{}\n", su->tree()->toStringTree(su->parser(), true));
  }

  for (const auto& err : result.errors) {
    fmt::print(stderr, "{}\n", err);
  }
  std::fflush(stdout);
  return result.ok() ? 0 : 1;
}
