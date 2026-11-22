#include "executor.h"
#include "function_def_visitor.h"
#include "stdlib/collections.h"
#include "utils.h"
#include "BasicLexer.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/format.h"

#include <filesystem>

namespace wwivbasic {

using namespace wwiv::stl;
using namespace wwiv::strings;

// Deep enough for any reasonable script, shallow enough not to overflow the
// C++ stack on runaway recursion.
static constexpr int kMaxCallDepth = 1000;

Value ExecutionVisitor::eval(BasicParser::ExprContext* e) {
  return Value(visit(e));
}

std::any ExecutionVisitor::visitMain(BasicParser::MainContext* context) {
  // When starting, reset the module to the root.
  ec_.module = ec_.root;
  flow_ = Flow::NORMAL;
  for (auto* child : context->children) {
    try {
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
    return fn->cpp_fn(params);
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

std::any ExecutionVisitor::visitProcedureCall(BasicParser::ProcedureCallContext* ctx) {
  const auto fn_name = ctx->procedureName()->getText();
  std::vector<Value> params;
  if (ctx->parameterList()) {
    params = std::any_cast<std::vector<Value>>(visit(ctx->parameterList()));
  }
  // l(i) reads item i of list l, unless a function named l exists.
  if (!ec_.try_find_function(fn_name)) {
    if (auto v = ec_.var(fn_name); v && v->value().is_list()) {
      if (params.size() != 1) {
        throw BasicError(fmt::format("{}(): a list index takes 1 argument, got {}", fn_name,
                                     params.size()));
      }
      return stdlib::list_get(v->value(), params.front(), fn_name + "()");
    }
  }
  return call(fn_name, params);
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
  if (context->ID()) {
    name = context->ID()->getText();
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

std::any ExecutionVisitor::visitStatement(BasicParser::StatementContext* context) {
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
  return visit(context->procedureCall());
}

std::any ExecutionVisitor::visitParens(BasicParser::ParensContext* context) {
  return visit(context->expr());
}

std::any ExecutionVisitor::visitIdent(BasicParser::IdentContext* context) {
  return visit(context->rvalue());
}

std::any ExecutionVisitor::visitRvalue(BasicParser::RvalueContext* context) {
  if (auto v = ec_.var(context->getText())) {
    return v->value();
  }
  // Like most BASICs, an unset variable reads as empty.
  return Value();
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
  return Value(-eval(context->expr()).toInt());
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
  ec_.upsert(name, value);
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
  visit(body);
  if (flow_ == Flow::EXIT_LOOP) {
    flow_ = Flow::NORMAL;
    return false;
  }
  return flow_ == Flow::NORMAL;
}

std::any ExecutionVisitor::visitForStatement(BasicParser::ForStatementContext* ctx) {
  const auto varname = ctx->ID(0)->getText();
  if (ctx->ID().size() > 1 && !iequals(ctx->ID(1)->getText(), varname)) {
    throw BasicError(
        fmt::format("NEXT {} does not match FOR {}", ctx->ID(1)->getText(), varname));
  }
  const auto start = eval(ctx->expr(0)).toInt();
  const auto end = eval(ctx->expr(1)).toInt();
  const auto step = ctx->STEP() ? eval(ctx->expr(2)).toInt() : 1;
  if (step == 0) {
    throw BasicError("FOR loop STEP must not be 0");
  }

  ec_.upsert(varname, Value(start));
  while (true) {
    const auto current = ec_.var(varname).value_or(Var(varname, Value(0))).value().toInt();
    if ((step > 0 && current > end) || (step < 0 && current < end)) {
      break;
    }
    if (!run_loop_body(ctx->statements())) {
      break;
    }
    const auto next = ec_.var(varname).value_or(Var(varname, Value(0))).value().toInt() + step;
    ec_.upsert(varname, Value(next));
  }
  return {};
}

std::any ExecutionVisitor::visitForInStatement(BasicParser::ForInStatementContext* ctx) {
  const auto varname = ctx->ID(0)->getText();
  if (ctx->ID().size() > 1 && !iequals(ctx->ID(1)->getText(), varname)) {
    throw BasicError(
        fmt::format("NEXT {} does not match FOR {}", ctx->ID(1)->getText(), varname));
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
