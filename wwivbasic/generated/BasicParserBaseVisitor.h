
// Generated from BasicParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "BasicParserVisitor.h"


namespace wwivbasic {

/**
 * This class provides an empty implementation of BasicParserVisitor, which can be
 * extended to create a visitor which only needs to handle a subset of the available methods.
 */
class  BasicParserBaseVisitor : public BasicParserVisitor {
public:

  virtual std::any visitMain(BasicParser::MainContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEos(BasicParser::EosContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitProcedureCall(BasicParser::ProcedureCallContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParameterList(BasicParser::ParameterListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitProcedureDefinition(BasicParser::ProcedureDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParameterDefinitionList(BasicParser::ParameterDefinitionListContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitImportModule(BasicParser::ImportModuleContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitModuleDefinition(BasicParser::ModuleDefinitionContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStatements(BasicParser::StatementsContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitStatement(BasicParser::StatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEmptyStatement(BasicParser::EmptyStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIdent(BasicParser::IdentContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitOr(BasicParser::OrContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMulDiv(BasicParser::MulDivContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAddSub(BasicParser::AddSubContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitParens(BasicParser::ParensContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitString(BasicParser::StringContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitInt(BasicParser::IntContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNot(BasicParser::NotContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRelation(BasicParser::RelationContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitProcCall(BasicParser::ProcCallContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAnd(BasicParser::AndContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReal(BasicParser::RealContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitNegate(BasicParser::NegateContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBoolean(BasicParser::BooleanContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitBooleanExpr(BasicParser::BooleanExprContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitAssignmentStatement(BasicParser::AssignmentStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitIfStatement(BasicParser::IfStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForStatement(BasicParser::ForStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitForInStatement(BasicParser::ForInStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitWhileStatement(BasicParser::WhileStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitDoUntilStatement(BasicParser::DoUntilStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPrintStatement(BasicParser::PrintStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitPrintItem(BasicParser::PrintItemContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitReturnStatement(BasicParser::ReturnStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitExitStatement(BasicParser::ExitStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitEndStatement(BasicParser::EndStatementContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitLvalue(BasicParser::LvalueContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitId(BasicParser::IdContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitRvalue(BasicParser::RvalueContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitProcedureName(BasicParser::ProcedureNameContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitQualifiedName(BasicParser::QualifiedNameContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitCallChain(BasicParser::CallChainContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMemberSuffix(BasicParser::MemberSuffixContext *ctx) override {
    return visitChildren(ctx);
  }

  virtual std::any visitMemberName(BasicParser::MemberNameContext *ctx) override {
    return visitChildren(ctx);
  }


};

}  // namespace wwivbasic
