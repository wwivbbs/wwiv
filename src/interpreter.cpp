#include "interpreter.h"

#include "executor.h"
#include "function_def_visitor.h"
#include "fmt/format.h"

namespace wwivbasic {

static RunResult run_loaded(Context& ctx, const std::filesystem::path& path,
                            const RunOptions& options) {
  RunResult result;
  const auto tree = ctx.parseTree(path);
  if (!tree || !ctx.errors.empty()) {
    result.errors = ctx.errors;
    if (result.errors.empty()) {
      result.errors.push_back(fmt::format("{}: error: unable to parse", path.string()));
    }
    return result;
  }

  const auto filename = path.string();
  FunctionDefVisitor fd(ctx, filename);
  fd.visit(tree.value());
  if (!ctx.errors.empty()) {
    result.errors = ctx.errors;
    return result;
  }

  if (!options.execute) {
    return result;
  }

  ExecutionVisitor v(ctx, filename);
  try {
    v.visit(tree.value());
  } catch (const BasicError& e) {
    if (e.line > 0) {
      result.errors.push_back(fmt::format("{}:{}: error: {}", e.file, e.line, e.what()));
    } else {
      result.errors.push_back(fmt::format("{}: error: {}", filename, e.what()));
    }
  }
  return result;
}

RunResult run_file(Context& ctx, const std::filesystem::path& path, const RunOptions& options) {
  if (!ctx.add_source(path)) {
    RunResult result;
    result.errors = ctx.errors;
    return result;
  }
  return run_loaded(ctx, path, options);
}

RunResult run_source(Context& ctx, const std::string& filename, const std::string& text,
                     const RunOptions& options) {
  ctx.add_source(filename, text);
  return run_loaded(ctx, filename, options);
}

} // namespace wwivbasic
