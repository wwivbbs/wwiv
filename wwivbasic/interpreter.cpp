#include "wwivbasic/interpreter.h"

#include "wwivbasic/internal/context.h"

#include "wwivbasic/internal/executor.h"
#include "wwivbasic/internal/function_def_visitor.h"
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
  RunningExecutor running(ctx, &v);
  Context::FramePush frame(ctx, {"", filename, 0, ctx.root, 0});
  try {
    v.visit(tree.value());
  } catch (const EndScript&) {
    // END in a script function that native code called: the script is done.
  } catch (const StopScript&) {
    result.stopped = true;
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

void Interpreter::set_stop_check(std::function<bool()> stop) { ctx_->stop_check = std::move(stop); }

void Interpreter::set_import_root(const std::filesystem::path& dir) {
  auto root = dir.lexically_normal();
  // "scripts/" normalizes to "scripts/", whose last element is empty; drop it
  // so lexically_relative compares whole directory names.
  if (root.has_relative_path() && root.filename().empty()) {
    root = root.parent_path();
  }
  ctx_->import_root = root;
}

std::optional<RunResult> Interpreter::check_first_run(const std::string& name) {
  if (!ran_) {
    ran_ = true;
    return std::nullopt;
  }
  RunResult result;
  result.errors.push_back(fmt::format(
      "{}: error: this Interpreter has already run a script; use a new one", name));
  return result;
}

RunResult Interpreter::run_file(const std::filesystem::path& path, const RunOptions& options) {
  if (auto error = check_first_run(path.string())) {
    return *error;
  }
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
  if (auto error = check_first_run(filename)) {
    return *error;
  }
  ctx_->imported_files.insert(Context::import_key(filename));
  ctx_->add_source(filename, text);
  return run_loaded(*ctx_, filename, options);
}

Value Interpreter::call(const std::string& name, const std::vector<Value>& args) {
  auto* v = ctx_->executor;
  if (!v) {
    throw BasicError(
        fmt::format("{}: can't call a script function when no script is running", name));
  }
  auto result = v->call(name, args);
  if (v->ended()) {
    throw EndScript{};
  }
  return result;
}

void Interpreter::set_statement_hook(std::function<bool(const DebugLocation&)> hook) {
  ctx_->statement_hook = std::move(hook);
}

std::vector<DebugFrame> Interpreter::call_stack() const {
  std::vector<DebugFrame> result;
  for (auto it = ctx_->frames.rbegin(); it != ctx_->frames.rend(); ++it) {
    result.push_back({it->function, it->file, it->line});
  }
  return result;
}

std::vector<DebugVariable> Interpreter::variables() const { return ctx_->debug_variables(); }

std::optional<std::string> Interpreter::source(const std::string& file) const {
  if (const auto it = ctx_->sources.find(file); it != ctx_->sources.end()) {
    return it->second->text_;
  }
  return std::nullopt;
}

bool Interpreter::has_function(const std::string& name) const {
  return ctx_->executor && ctx_->try_find_function(name).has_value();
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
