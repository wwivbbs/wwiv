#include "wwivbasic/internal/executor.h"
#include "wwivbasic/internal/function_def_visitor.h"
#include "wwivbasic/stdlib/collections.h"
#include "wwivbasic/object.h"
#include "wwivbasic/internal/utils.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"

#include <filesystem>

namespace wwivbasic {

using namespace wwiv::stl;
using namespace wwiv::strings;

// Deep enough for any reasonable script, shallow enough not to overflow the
// C++ stack on runaway recursion. Each script call takes several KB of C++
// stack in a debug build, and the Windows main thread has only 1 MB.
static constexpr int kMaxCallDepth = 100;

// Runs native code (a native function, or an object's getter, setter or
// method), turning C++ exceptions into script errors so a bug or a failed
// lookup in the embedder's code doesn't end the host program.
template <class F> static auto guard_native(const std::string& what, F&& f) {
  try {
    return f();
  } catch (const BasicError&) {
    throw;
  } catch (const StopScript&) {
    throw;
  } catch (const EndScript&) {
    throw;
  } catch (const std::exception& e) {
    throw BasicError(fmt::format("{}: {}", what, e.what()));
  } catch (...) {
    throw BasicError(fmt::format("{}: unknown C++ exception", what));
  }
}

Value ExecutionVisitor::eval(BasicParser::ExprContext* e) {
  return Value(visit(e));
}

std::any ExecutionVisitor::visitMain(BasicParser::MainContext* context) {
  // When starting, reset the module to the root.
  ec_.module = ec_.root;
  flow_ = Flow::NORMAL;
  for (auto* child : context->children) {
    try {
      // A top-level IMPORT isn't a statement, but a debugger can stop on it.
      if (auto* import = dynamic_cast<BasicParser::ImportModuleContext*>(child)) {
        ec_.check_stop();
        before_statement(import);
      }
      visit(child);
    } catch (BasicError& e) {
      // Top-level IMPORT/MODULE are not statements, so give their errors a
      // location here.
      if (auto* rule = dynamic_cast<antlr4::ParserRuleContext*>(child); rule && e.line == 0) {
        e.line = rule->getStart()->getLine();
        e.file = filename_;
      }
      throw;
    }
    if (flow_ == Flow::END) {
      break;
    }
    // RETURN or EXIT at the top level just stop the current statement.
    flow_ = Flow::NORMAL;
  }
  return {};
}

Value ExecutionVisitor::call(const std::string& name, const std::vector<Value>& params) {
  auto [owner, fn] = ec_.find_function(name);
  ec_.trace_log("CALL {} ({} args)", name, params.size());

  if (fn->type == BasicFunction::Type::NATIVE) {
    return guard_native(fn->name, [&] { return fn->cpp_fn(params); });
  }

  if (params.size() != fn->params.size()) {
    throw BasicError(fmt::format("{}: expected {} argument(s), got {}", name, fn->params.size(),
                                 params.size()));
  }
  if (call_depth_ >= kMaxCallDepth) {
    throw BasicError(fmt::format("{}: too many nested calls (limit {})", name, kMaxCallDepth));
  }

  Scope fnscope(fn->name);
  for (size_t i = 0; i < params.size(); i++) {
    fnscope.local_vars.insert_or_assign(fn->params.at(i), Var(fn->params.at(i), params.at(i)));
  }

  // Run the body in the module that defines the function.
  auto* saved_module = ec_.module;
  ec_.module = owner;
  owner->scopes.push_back(fnscope);
  ++call_depth_;

  // Errors in the body are reported against the file that defines it.
  auto saved_filename = filename_;
  if (!fn->filename.empty()) {
    filename_ = fn->filename;
  }

  struct Restore {
    ExecutionVisitor* v;
    Module* owner;
    Module* saved;
    std::string saved_filename;
    ~Restore() {
      owner->scopes.pop_back();
      v->ec_.module = saved;
      v->filename_ = saved_filename;
      --v->call_depth_;
    }
  } restore{this, owner, saved_module, saved_filename};
  Context::FramePush frame(ec_, {fn->name, filename_, 0, owner, owner->scopes.size() - 1});

  return_value_ = Value();
  visit(static_cast<BasicParser::ProcedureDefinitionContext*>(fn->def_fn)->statements());

  Value result;
  if (flow_ == Flow::RETURN) {
    result = return_value_;
    flow_ = Flow::NORMAL;
  } else if (flow_ == Flow::EXIT_LOOP) {
    // EXIT outside of a loop leaves the function.
    flow_ = Flow::NORMAL;
  }
  ec_.trace_log("{} RETURNED '{}'", name, result);
  return result;
}

// Dotted names are resolved at runtime. The longest prefix that names a
// module is a module path: wwiv.io.outstr() calls module wwiv.io's OUTSTR, and
// m.x is module m's variable x. Otherwise the first part is a variable. Any
// parts after the variable are members of its value (area.name, m.obj.name,
// obj.method()).

ExecutionVisitor::NamePath ExecutionVisitor::resolve_name(const std::string& name) const {
  const auto parts = SplitString(name, ".");
  NamePath path;
  size_t first_member = 1;
  path.variable = parts.front();
  for (auto k = parts.size() - 1; k >= 1; k--) {
    std::string prefix = parts.front();
    for (size_t i = 1; i < k; i++) {
      prefix += "." + parts[i];
    }
    if (contains(ec_.modules, prefix)) {
      path.variable = prefix + "." + parts[k];
      first_member = k + 1;
      break;
    }
  }
  path.members.assign(parts.begin() + static_cast<std::ptrdiff_t>(first_member), parts.end());
  return path;
}

Value ExecutionVisitor::path_variable(const NamePath& path, const std::string& name) {
  auto v = ec_.var(path.variable);
  if (!v) {
    throw BasicError(fmt::format("Unknown variable or module '{}' in '{}'", path.variable, name));
  }
  return v->value();
}

// Members and methods come from the object's ClassBinding (wwivbasic/object.h).
// Other values have none.

namespace {

// The object behind a value, checked: a value built by hand (not with wrap)
// could hold no object.
const ObjectData& checked_object(const Value& object, std::string_view what) {
  const auto data = object.object(what);
  if (!data || !data->self || !data->cls) {
    throw BasicError(fmt::format("{}: the object is missing (null)", what));
  }
  return *data;
}

} // namespace

Value ExecutionVisitor::get_member(const Value& object, const std::string& member) {
  if (!object.is_object()) {
    throw BasicError(fmt::format("Cannot read member '{}' of {}", member, object.a_type_name()));
  }
  const auto& data = checked_object(object, member);
  const auto& cls = *data.cls;
  const auto it = cls.properties.find(member);
  if (it == cls.properties.end()) {
    if (cls.methods.contains(member)) {
      throw BasicError(fmt::format("{}.{} is a method; call it with ()", cls.name, member));
    }
    throw BasicError(fmt::format("{} has no member '{}'", cls.name, member));
  }
  return guard_native(cls.name + "." + member, [&] { return it->second.get(data.self.get()); });
}

void ExecutionVisitor::set_member(const Value& object, const std::string& member,
                                  const Value& value) {
  if (!object.is_object()) {
    throw BasicError(fmt::format("Cannot set member '{}' of {}", member, object.a_type_name()));
  }
  const auto& data = checked_object(object, member);
  const auto& cls = *data.cls;
  const auto it = cls.properties.find(member);
  if (it == cls.properties.end()) {
    throw BasicError(fmt::format("{} has no member '{}'", cls.name, member));
  }
  if (!it->second.set) {
    throw BasicError(fmt::format("{}.{} is read-only", cls.name, member));
  }
  const auto what = cls.name + "." + member;
  guard_native(what, [&] {
    try {
      it->second.set(data.self.get(), value);
    } catch (const ArgumentError& e) {
      // Type errors from converting the value don't know the property's name.
      throw BasicError(fmt::format("{}: {}", what, e.what()));
    }
    return 0;
  });
}

Value ExecutionVisitor::call_method(const Value& object, const std::string& method,
                                    const std::vector<Value>& params) {
  if (!object.is_object()) {
    throw BasicError(fmt::format("Cannot call method '{}' on {}", method, object.a_type_name()));
  }
  const auto& data = checked_object(object, method);
  const auto& cls = *data.cls;
  const auto it = cls.methods.find(method);
  if (it == cls.methods.end()) {
    if (cls.properties.contains(method)) {
      throw BasicError(fmt::format("{}.{} is a property, not a method; use it without ()",
                                   cls.name, method));
    }
    throw BasicError(fmt::format("{} has no method '{}'", cls.name, method));
  }
  return guard_native(cls.name + "." + method,
                      [&] { return it->second(data.self.get(), params); });
}

std::any ExecutionVisitor::visitProcedureCall(BasicParser::ProcedureCallContext* ctx) {
  const auto fn_name = ctx->qualifiedName()->getText();
  std::vector<Value> params;
  if (ctx->parameterList()) {
    params = std::any_cast<std::vector<Value>>(visit(ctx->parameterList()));
  }
  if (ec_.try_find_function(fn_name)) {
    return call(fn_name, params);
  }
  const auto path = resolve_name(fn_name);
  if (path.members.empty()) {
    // l(i) or m.l(i) reads item i of a list.
    if (auto v = ec_.var(path.variable); v && v->value().is_list()) {
      if (params.size() != 1) {
        throw BasicError(fmt::format("{}(): a list index takes 1 argument, got {}", fn_name,
                                     params.size()));
      }
      return stdlib::list_get(v->value(), params.front(), fn_name + "()");
    }
    // Not a function or a list: report the unknown function or module.
    return call(fn_name, params);
  }
  // obj.a.method(args)
  auto object = path_variable(path, fn_name);
  for (size_t i = 0; i + 1 < path.members.size(); i++) {
    object = get_member(object, path.members[i]);
  }
  return call_method(object, path.members.back(), params);
}

std::any ExecutionVisitor::visitCallChain(BasicParser::CallChainContext* ctx) {
  auto value = Value(visit(ctx->procedureCall()));
  for (auto* suffix : ctx->memberSuffix()) {
    const auto name = suffix->memberName()->getText();
    if (suffix->LPAREN()) {
      std::vector<Value> params;
      if (suffix->parameterList()) {
        params = std::any_cast<std::vector<Value>>(visit(suffix->parameterList()));
      }
      value = call_method(value, name, params);
    } else {
      value = get_member(value, name);
    }
  }
  return value;
}

std::any ExecutionVisitor::visitParameterList(BasicParser::ParameterListContext* context) {
  std::vector<Value> result;
  for (auto* expr : context->expr()) {
    result.push_back(eval(expr));
  }
  return result;
}

std::any ExecutionVisitor::visitProcedureDefinition(BasicParser::ProcedureDefinitionContext*) {
  // Procedure definitions were collected by FunctionDefVisitor.
  return {};
}

std::any ExecutionVisitor::visitModuleDefinition(BasicParser::ModuleDefinitionContext*) {
  // Handled by FunctionDefVisitor.
  return {};
}

std::any ExecutionVisitor::visitImportModule(BasicParser::ImportModuleContext* context) {
  std::string name;
  if (context->qualifiedName()) {
    name = context->qualifiedName()->getText();
  } else {
    name = remove_quotes(context->STRING()->getText());
    if (!starts_with(name, "@")) {
      import_file(name);
      return {};
    }
    name = name.substr(1);
  }
  if (!contains(ec_.modules, name)) {
    throw BasicError(fmt::format("Unknown module: '@{}'", name));
  }
  ec_.trace_log("IMPORT @{}", name);
  ec_.module->imported_modules.emplace(name);
  return {};
}

void ExecutionVisitor::import_file(const std::string& name) {
  // Like WWIV's scripts directory, paths are relative to the importing script.
  const auto base = std::filesystem::path(filename_).parent_path();
  const auto path = (base / name).lexically_normal();
  if (ec_.import_root) {
    // Rejects "../x.bas", absolute paths and other drives.
    const auto rel = path.lexically_relative(*ec_.import_root);
    if (rel.empty() || *rel.begin() == "..") {
      throw BasicError(fmt::format(
          "Can't import '{}': it is outside the directory scripts can import from", name));
    }
  }
  // Each file is only loaded once, which also stops import cycles. The entry
  // script is registered by run_file/run_source, so a cycle back to it stops
  // here too.
  if (!ec_.imported_files.insert(Context::import_key(path)).second) {
    return;
  }
  ec_.trace_log("IMPORT \"{}\"", path.string());

  const auto errors_before = ec_.errors.size();
  // Reports every error added since errors_before.
  auto import_error = [&]() {
    std::string msg = fmt::format("Unable to import '{}'", name);
    for (auto i = errors_before; i < ec_.errors.size(); i++) {
      msg += "\n  " + ec_.errors[i];
    }
    return BasicError(msg);
  };

  ec_.add_source(path);
  const auto tree = ec_.parseTree(path);
  if (!tree || ec_.errors.size() != errors_before) {
    throw import_error();
  }

  // Imported functions are defined in the importing module (or the module a
  // MODULE statement in the file names), and its top-level code runs now.
  auto* saved_module = ec_.module;
  struct RestoreModule {
    Context& ec;
    Module* saved;
    ~RestoreModule() { ec.module = saved; }
  } restore{ec_, saved_module};

  FunctionDefVisitor fd(ec_, path.string());
  fd.visit(tree.value());
  if (ec_.errors.size() != errors_before) {
    throw import_error();
  }
  ec_.module = saved_module;

  ExecutionVisitor v(ec_, path.string());
  v.call_depth_ = call_depth_;
  RunningExecutor running(ec_, &v);
  Context::FramePush frame(ec_, {"", path.string(), 0, ec_.module, 0});
  v.visit(tree.value());
  if (v.ended()) {
    flow_ = Flow::END;
  }
}

std::any ExecutionVisitor::visitStatements(BasicParser::StatementsContext* context) {
  for (auto* stmt : context->statement()) {
    visit(stmt);
    if (flow_ != Flow::NORMAL) {
      break;
    }
  }
  return {};
}

void ExecutionVisitor::before_statement(antlr4::ParserRuleContext* context) {
  if (!ec_.frames.empty()) {
    const auto* start = context->getStart();
    auto& frame = ec_.frames.back();
    frame.line = static_cast<int>(start->getLine());
    // Top-level code runs in whichever module is current (MODULE can change it).
    if (frame.function.empty()) {
      frame.module = ec_.module;
    }
    if (ec_.statement_hook) {
      const DebugLocation loc{frame.file, frame.line,
                              static_cast<int>(start->getCharPositionInLine()) + 1,
                              static_cast<int>(ec_.frames.size()) - 1};
      if (ec_.statement_hook(loc)) {
        throw StopScript{};
      }
    }
  }
}

std::any ExecutionVisitor::visitStatement(BasicParser::StatementContext* context) {
  ec_.check_stop();
  before_statement(context);
  try {
    return visitChildren(context);
  } catch (BasicError& e) {
    // Record the innermost statement that failed.
    if (e.line == 0) {
      e.line = context->getStart()->getLine();
      e.file = filename_;
    }
    throw;
  }
}

// Expressions

std::any ExecutionVisitor::visitProcCall(BasicParser::ProcCallContext* context) {
  return visit(context->callChain());
}

std::any ExecutionVisitor::visitParens(BasicParser::ParensContext* context) {
  return visit(context->expr());
}

std::any ExecutionVisitor::visitIdent(BasicParser::IdentContext* context) {
  return visit(context->rvalue());
}

std::any ExecutionVisitor::visitRvalue(BasicParser::RvalueContext* context) {
  const auto name = context->getText();
  const auto path = resolve_name(name);
  if (!path.members.empty()) {
    // obj.a.b or m.obj.a
    auto value = path_variable(path, name);
    for (const auto& member : path.members) {
      value = get_member(value, member);
    }
    return value;
  }
  if (auto v = ec_.var(path.variable)) {
    return v->value();
  }
  // Like most BASICs, an unset variable reads as empty.
  return Value();
}

std::any ExecutionVisitor::visitReal(BasicParser::RealContext* context) {
  // REAL tokens are only digits and a '.', so parse_number fails only when
  // the value is too large.
  const auto text = context->getText();
  const auto d = Value::parse_number(text);
  if (!d) {
    throw BasicError(fmt::format("Number out of range: {}", text));
  }
  // Like my_basic, a whole number such as 2.0 is an INTEGER.
  return Value::number(*d);
}

std::any ExecutionVisitor::visitInt(BasicParser::IntContext* context) {
  const auto text = context->getText();
  try {
    return Value(std::stoi(text));
  } catch (const std::out_of_range&) {
    throw BasicError(fmt::format("Number out of range: {}", text));
  }
}

std::any ExecutionVisitor::visitString(BasicParser::StringContext* context) {
  return Value(remove_quotes(context->getText()));
}

std::any ExecutionVisitor::visitBoolean(BasicParser::BooleanContext* context) {
  return Value(context->booleanExpr()->TRUE() != nullptr);
}

std::any ExecutionVisitor::visitNegate(BasicParser::NegateContext* context) {
  const auto v = eval(context->expr());
  if (v.value_type() == Value::Type::REAL) {
    // Value::number, so -0.0 prints 0 like my_basic.
    return Value::number(-v.toDouble());
  }
  return Value::number(-static_cast<double>(v.toInt()));
}

std::any ExecutionVisitor::visitMulDiv(BasicParser::MulDivContext* context) {
  const auto left = eval(context->expr(0));
  const auto right = eval(context->expr(1));
  switch (context->op->getType()) {
  case BasicLexer::STAR:
    return left * right;
  case BasicLexer::SLASH:
    return left / right;
  case BasicLexer::MOD:
    return left % right;
  }
  throw BasicError(fmt::format("Unknown operator: {}", context->op->getText()));
}

std::any ExecutionVisitor::visitAddSub(BasicParser::AddSubContext* context) {
  const auto left = eval(context->expr(0));
  const auto right = eval(context->expr(1));
  switch (context->op->getType()) {
  case BasicLexer::PLUS:
    return left + right;
  case BasicLexer::MINUS:
    return left - right;
  }
  throw BasicError(fmt::format("Unknown operator: {}", context->op->getText()));
}

std::any ExecutionVisitor::visitRelation(BasicParser::RelationContext* context) {
  const auto left = eval(context->expr(0));
  const auto right = eval(context->expr(1));
  switch (context->op->getType()) {
  case BasicLexer::GT:
    return Value(left > right);
  case BasicLexer::GE:
    return Value(left == right || left > right);
  case BasicLexer::LT:
    return Value(left < right);
  case BasicLexer::LE:
    return Value(left == right || left < right);
  case BasicLexer::NE:
    return Value(left != right);
  case BasicLexer::EQ:
    return Value(left == right);
  }
  throw BasicError(fmt::format("Unknown operator: {}", context->op->getText()));
}

std::any ExecutionVisitor::visitNot(BasicParser::NotContext* context) {
  return Value(!eval(context->expr()).toBool());
}

std::any ExecutionVisitor::visitAnd(BasicParser::AndContext* context) {
  // Short-circuit: the right side is not evaluated if the left is false.
  return Value(eval(context->expr(0)).toBool() && eval(context->expr(1)).toBool());
}

std::any ExecutionVisitor::visitOr(BasicParser::OrContext* context) {
  return Value(eval(context->expr(0)).toBool() || eval(context->expr(1)).toBool());
}

// Statements

std::any
ExecutionVisitor::visitAssignmentStatement(BasicParser::AssignmentStatementContext* context) {
  const auto name = context->lvalue()->getText();
  const auto value = eval(context->expr());
  ec_.trace_log("ASSIGN {} = '{}'", name, value);
  const auto path = resolve_name(name);
  if (!path.members.empty()) {
    // obj.a.b = value
    auto object = path_variable(path, name);
    for (size_t i = 0; i + 1 < path.members.size(); i++) {
      object = get_member(object, path.members[i]);
    }
    set_member(object, path.members.back(), value);
    return {};
  }
  ec_.upsert(path.variable, value);
  return {};
}

std::any ExecutionVisitor::visitIfStatement(BasicParser::IfStatementContext* context) {
  // expr(i) guards statements(i); a trailing extra statements() is the ELSE.
  const auto conditions = context->expr();
  for (size_t i = 0; i < conditions.size(); i++) {
    if (eval(conditions[i]).toBool()) {
      return visit(context->statements(i));
    }
  }
  if (context->ELSE()) {
    return visit(context->statements().back());
  }
  return {};
}

bool ExecutionVisitor::run_loop_body(BasicParser::StatementsContext* body) {
  // Checked on every iteration, so even a loop with an empty body can stop.
  ec_.check_stop();
  visit(body);
  if (flow_ == Flow::EXIT_LOOP) {
    flow_ = Flow::NORMAL;
    return false;
  }
  return flow_ == Flow::NORMAL;
}

std::any ExecutionVisitor::visitForStatement(BasicParser::ForStatementContext* ctx) {
  const auto varname = ctx->qualifiedName(0)->getText();
  if (ctx->qualifiedName().size() > 1 && !iequals(ctx->qualifiedName(1)->getText(), varname)) {
    throw BasicError(
        fmt::format("NEXT {} does not match FOR {}", ctx->qualifiedName(1)->getText(), varname));
  }
  // Computed in doubles so REAL bounds and steps work (FOR x = 0 TO 1 STEP
  // 0.25); Value::number keeps the loop variable an INTEGER when it's whole.
  const auto start = eval(ctx->expr(0)).toDouble();
  const auto end = eval(ctx->expr(1)).toDouble();
  const auto step = ctx->STEP() ? eval(ctx->expr(2)).toDouble() : 1.0;
  if (step == 0) {
    throw BasicError("FOR loop STEP must not be 0");
  }

  ec_.upsert(varname, Value::number(start));
  while (true) {
    const auto current = ec_.var(varname).value_or(Var(varname, Value(0))).value().toDouble();
    if ((step > 0 && current > end) || (step < 0 && current < end)) {
      break;
    }
    if (!run_loop_body(ctx->statements())) {
      break;
    }
    // The body may change the loop variable, so read it again.
    const auto next = ec_.var(varname).value_or(Var(varname, Value(0))).value().toDouble() + step;
    ec_.upsert(varname, Value::number(next));
  }
  return {};
}

std::any ExecutionVisitor::visitForInStatement(BasicParser::ForInStatementContext* ctx) {
  const auto varname = ctx->qualifiedName(0)->getText();
  if (ctx->qualifiedName().size() > 1 && !iequals(ctx->qualifiedName(1)->getText(), varname)) {
    throw BasicError(
        fmt::format("NEXT {} does not match FOR {}", ctx->qualifiedName(1)->getText(), varname));
  }
  const auto list = eval(ctx->expr()).list("FOR ... IN");
  // Indexing (rather than iterators) keeps this safe if the body changes the
  // list: items added during the loop are visited, removed ones are not.
  for (size_t i = 0; i < list->items.size(); i++) {
    ec_.upsert(varname, list->items[i]);
    if (!run_loop_body(ctx->statements())) {
      break;
    }
  }
  return {};
}

std::any ExecutionVisitor::visitWhileStatement(BasicParser::WhileStatementContext* ctx) {
  while (eval(ctx->expr()).toBool()) {
    if (!run_loop_body(ctx->statements())) {
      break;
    }
  }
  return {};
}

std::any ExecutionVisitor::visitDoUntilStatement(BasicParser::DoUntilStatementContext* ctx) {
  do {
    if (!run_loop_body(ctx->statements())) {
      break;
    }
  } while (!eval(ctx->expr()).toBool());
  return {};
}

std::any ExecutionVisitor::visitPrintStatement(BasicParser::PrintStatementContext* ctx) {
  std::string line;
  for (auto* item : ctx->printItem()) {
    line.append(eval(item->expr()).toString());
  }
  ec_.console.write(line);
  if (!ctx->trailing) {
    ec_.console.newline();
  }
  return {};
}

std::any ExecutionVisitor::visitReturnStatement(BasicParser::ReturnStatementContext* context) {
  return_value_ = context->expr() ? eval(context->expr()) : Value();
  flow_ = Flow::RETURN;
  return {};
}

std::any ExecutionVisitor::visitExitStatement(BasicParser::ExitStatementContext*) {
  flow_ = Flow::EXIT_LOOP;
  return {};
}

std::any ExecutionVisitor::visitEndStatement(BasicParser::EndStatementContext*) {
  flow_ = Flow::END;
  return {};
}

} // namespace wwivbasic
