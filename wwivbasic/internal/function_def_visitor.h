#pragma once

#include "wwivbasic/internal/antlr.h"
#include "wwivbasic/internal/context.h"

#include <string>

namespace wwivbasic {

// Collects procedure definitions (DEF ... ENDDEF) before execution so that
// functions can be called before they are defined in the source.
class FunctionDefVisitor : public BasicParserBaseVisitor {
public:
  FunctionDefVisitor(Context& ec, std::string filename = {})
      : ec_(ec), filename_(std::move(filename)) {}

  std::any visitProcedureDefinition(BasicParser::ProcedureDefinitionContext* context) override;
  std::any visitModuleDefinition(BasicParser::ModuleDefinitionContext* context) override;

private:
  Context& ec_;
  std::string filename_;
};

} // namespace wwivbasic
