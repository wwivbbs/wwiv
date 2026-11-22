#pragma once
#include "BasicParserBaseVisitor.h"
#include "antlr4-runtime.h"
#include "context.h"

#include <string>
#include <vector>

namespace wwivbasic {

// Executes a parse tree. Expression visitors return a std::any holding a
// Value; statement visitors return an empty std::any.
class ExecutionVisitor : public BasicParserBaseVisitor {
public:
  ExecutionVisitor(Context& ec, std::string filename = {})
      : ec_(ec), filename_(std::move(filename)) {}

  // Calls a function (BASIC or native) by name with already-evaluated
  // arguments.
  Value call(const std::string& name, const std::vector<Value>& params);

  // True once an END statement has run.
  bool ended() const { return flow_ == Flow::END; }

  std::any visitMain(BasicParser::MainContext* context) override;
  std::any visitProcedureCall(BasicParser::ProcedureCallContext* context) override;
  std::any visitCallChain(BasicParser::CallChainContext* ctx) override;
  std::any visitParameterList(BasicParser::ParameterListContext* context) override;
  std::any visitProcedureDefinition(BasicParser::ProcedureDefinitionContext* context) override;
  std::any visitImportModule(BasicParser::ImportModuleContext* context) override;
  std::any visitModuleDefinition(BasicParser::ModuleDefinitionContext* context) override;
  std::any visitStatements(BasicParser::StatementsContext* context) override;
  std::any visitStatement(BasicParser::StatementContext* context) override;

  // Expressions
  std::any visitProcCall(BasicParser::ProcCallContext* context) override;
  std::any visitParens(BasicParser::ParensContext* context) override;
  std::any visitIdent(BasicParser::IdentContext* context) override;
  std::any visitInt(BasicParser::IntContext* context) override;
  std::any visitReal(BasicParser::RealContext* context) override;
  std::any visitString(BasicParser::StringContext* context) override;
  std::any visitBoolean(BasicParser::BooleanContext* context) override;
  std::any visitNegate(BasicParser::NegateContext* context) override;
  std::any visitMulDiv(BasicParser::MulDivContext* context) override;
  std::any visitAddSub(BasicParser::AddSubContext* context) override;
  std::any visitRelation(BasicParser::RelationContext* context) override;
  std::any visitNot(BasicParser::NotContext* context) override;
  std::any visitAnd(BasicParser::AndContext* context) override;
  std::any visitOr(BasicParser::OrContext* context) override;
  std::any visitRvalue(BasicParser::RvalueContext* ctx) override;

  // Statements
  std::any visitAssignmentStatement(BasicParser::AssignmentStatementContext* context) override;
  std::any visitIfStatement(BasicParser::IfStatementContext* context) override;
  std::any visitForStatement(BasicParser::ForStatementContext* ctx) override;
  std::any visitForInStatement(BasicParser::ForInStatementContext* ctx) override;
  std::any visitWhileStatement(BasicParser::WhileStatementContext* ctx) override;
  std::any visitDoUntilStatement(BasicParser::DoUntilStatementContext* ctx) override;
  std::any visitPrintStatement(BasicParser::PrintStatementContext* ctx) override;
  std::any visitReturnStatement(BasicParser::ReturnStatementContext* context) override;
  std::any visitExitStatement(BasicParser::ExitStatementContext* context) override;
  std::any visitEndStatement(BasicParser::EndStatementContext* context) override;

private:
  // How control leaves a block of statements.
  enum class Flow { NORMAL, EXIT_LOOP, RETURN, END };

  Value eval(BasicParser::ExprContext* e);
  // Runs a loop body; returns false if the loop must stop (EXIT/RETURN/END).
  bool run_loop_body(BasicParser::StatementsContext* body);
  // A dotted name split into the variable it starts with and the members
  // after it. `variable` is "x", or "m.x" for module m's variable x.
  struct NamePath {
    std::string variable;
    std::vector<std::string> members;
  };
  // Resolves a dotted name: the longest prefix that names a module, then
  // that module's variable; otherwise the first part is a variable.
  NamePath resolve_name(const std::string& name) const;
  // The value of a path's variable, or an error if there is none.
  Value path_variable(const NamePath& path, const std::string& name);
  // Members and methods of values (obj.name, obj.name = v, obj.name(args)).
  // No values have members yet; these raise errors. See docs/PLAN.md (M3).
  Value get_member(const Value& object, const std::string& member);
  void set_member(const Value& object, const std::string& member, const Value& value);
  Value call_method(const Value& object, const std::string& method,
                    const std::vector<Value>& params);
  // Loads, defines and runs IMPORT "file.bas" (once per file).
  void import_file(const std::string& name);

  Context& ec_;
  std::string filename_;
  Flow flow_{Flow::NORMAL};
  Value return_value_;
  int call_depth_{0};
};

} // namespace wwivbasic
