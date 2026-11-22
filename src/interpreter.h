#pragma once

#include "context.h"

#include <filesystem>
#include <string>
#include <vector>

namespace wwivbasic {

struct RunOptions {
  // When false, only parse (and check) the script.
  bool execute{true};
};

struct RunResult {
  bool ok() const { return errors.empty(); }
  // Errors formatted as "file:line[:column]: error: message".
  std::vector<std::string> errors;
};

// Parses the script at path and, unless options.execute is false, runs it in
// the given context.
RunResult run_file(Context& ctx, const std::filesystem::path& path, const RunOptions& options = {});

// Parses and runs script text. The filename is used in error messages.
RunResult run_source(Context& ctx, const std::string& filename, const std::string& text,
                     const RunOptions& options = {});

} // namespace wwivbasic
