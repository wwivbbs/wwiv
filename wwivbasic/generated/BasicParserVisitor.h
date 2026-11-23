
// Generated from BasicParser.g4 by ANTLR 4.13.2

#pragma once


#include "antlr4-runtime.h"
#include "BasicParser.h"


namespace wwivbasic {

/**
 * This class defines an abstract visitor for a parse tree
 * produced by BasicParser.
 */
class  BasicParserVisitor : public antlr4::tree::AbstractParseTreeVisitor {
public:

  /**
   * Visit parse trees produced by BasicParser.
   */
    virtual std::any visitMain(BasicParser::MainContext *context) = 0;

    virtual std::any visitEos(BasicParser::EosContext *context) = 0;

    virtual std::any visitProcedureCall(BasicParser::ProcedureCallContext *context) = 0;

    virtual std::any visitParameterList(BasicParser::ParameterListContext *context) = 0;

    virtual std::any visitProcedureDefinition(BasicParser::ProcedureDefinitionContext *context) = 0;

    virtual std::any visitParameterDefinitionList(BasicParser::ParameterDefinitionListContext *context) = 0;

    virtual std::any visitImportModule(BasicParser::ImportModuleContext *context) = 0;

    virtual std::any visitModuleDefinition(BasicParser::ModuleDefinitionContext *context) = 0;

    virtual std::any visitStatements(BasicParser::StatementsContext *context) = 0;

    virtual std::any visitStatement(BasicParser::StatementContext *context) = 0;

    virtual std::any visitEmptyStatement(BasicParser::EmptyStatementContext *context) = 0;

    virtual std::any visitIdent(BasicParser::IdentContext *context) = 0;

    virtual std::any visitOr(BasicParser::OrContext *context) = 0;

    virtual std::any visitMulDiv(BasicParser::MulDivContext *context) = 0;

    virtual std::any visitAddSub(BasicParser::AddSubContext *context) = 0;

    virtual std::any visitParens(BasicParser::ParensContext *context) = 0;

    virtual std::any visitString(BasicParser::StringContext *context) = 0;

    virtual std::any visitInt(BasicParser::IntContext *context) = 0;

    virtual std::any visitNot(BasicParser::NotContext *context) = 0;

    virtual std::any visitRelation(BasicParser::RelationContext *context) = 0;

    virtual std::any visitProcCall(BasicParser::ProcCallContext *context) = 0;

    virtual std::any visitAnd(BasicParser::AndContext *context) = 0;

    virtual std::any visitReal(BasicParser::RealContext *context) = 0;

    virtual std::any visitNegate(BasicParser::NegateContext *context) = 0;

    virtual std::any visitBoolean(BasicParser::BooleanContext *context) = 0;

    virtual std::any visitBooleanExpr(BasicParser::BooleanExprContext *context) = 0;

    virtual std::any visitAssignmentStatement(BasicParser::AssignmentStatementContext *context) = 0;

    virtual std::any visitIfStatement(BasicParser::IfStatementContext *context) = 0;

    virtual std::any visitForStatement(BasicParser::ForStatementContext *context) = 0;

    virtual std::any visitForInStatement(BasicParser::ForInStatementContext *context) = 0;

    virtual std::any visitWhileStatement(BasicParser::WhileStatementContext *context) = 0;

    virtual std::any visitDoUntilStatement(BasicParser::DoUntilStatementContext *context) = 0;

    virtual std::any visitPrintStatement(BasicParser::PrintStatementContext *context) = 0;

    virtual std::any visitPrintItem(BasicParser::PrintItemContext *context) = 0;

    virtual std::any visitReturnStatement(BasicParser::ReturnStatementContext *context) = 0;

    virtual std::any visitExitStatement(BasicParser::ExitStatementContext *context) = 0;

    virtual std::any visitEndStatement(BasicParser::EndStatementContext *context) = 0;

    virtual std::any visitLvalue(BasicParser::LvalueContext *context) = 0;

    virtual std::any visitId(BasicParser::IdContext *context) = 0;

    virtual std::any visitRvalue(BasicParser::RvalueContext *context) = 0;

    virtual std::any visitProcedureName(BasicParser::ProcedureNameContext *context) = 0;

    virtual std::any visitQualifiedName(BasicParser::QualifiedNameContext *context) = 0;

    virtual std::any visitCallChain(BasicParser::CallChainContext *context) = 0;

    virtual std::any visitMemberSuffix(BasicParser::MemberSuffixContext *context) = 0;

    virtual std::any visitMemberName(BasicParser::MemberNameContext *context) = 0;


};

}  // namespace wwivbasic
