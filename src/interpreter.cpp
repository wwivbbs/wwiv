#include "wwivbasic/interpreter.h"

#include "context.h"

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

Interpreter::Interpreter(Console& console) : ctx_(std::make_unique<Context>(console)) {}

Interpreter::~Interpreter() = default;

Module& Interpreter::add_module(const std::string& name) { return ctx_->add_module(name); }

Module& Interpreter::globals() { return *ctx_->root; }

void Interpreter::set_trace(bool trace) { ctx_->trace = trace; }

RunResult Interpreter::run_file(const std::filesystem::path& path, const RunOptions& options) {
  // The entry script counts as imported, so an import cycle back to it is
  // skipped instead of reloading it.
  ctx_->imported_files.insert(Context::import_key(path));
  if (!ctx_->add_source(path)) {
    RunResult result;
    result.errors = ctx_->errors;
    return result;
  }
  return run_loaded(*ctx_, path, options);
}

RunResult Interpreter::run_source(const std::string& filename, const std::string& text,
                                  const RunOptions& options) {
  ctx_->imported_files.insert(Context::import_key(filename));
  ctx_->add_source(filename, text);
  return run_loaded(*ctx_, filename, options);
}

std::optional<std::string> Interpreter::parse_tree(const std::filesystem::path& path) const {
  const auto tree = ctx_->parseTree(path);
  if (!tree) {
    return std::nullopt;
  }
  auto& su = ctx_->sources.at(path);
  return su->tree()->toStringTree(su->parser(), true);
}

} // namespace wwivbasic
