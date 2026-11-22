#include "function_def_visitor.h"
#include "utils.h"
#include "core/stl.h"

#include <string>
#include <vector>

namespace wwivbasic {

using namespace wwiv::stl;

std::any
FunctionDefVisitor::visitProcedureDefinition(BasicParser::ProcedureDefinitionContext* context) {
  std::vector<std::string> params;
  if (auto* list = context->parameterDefinitionList()) {
    for (auto* id : list->id()) {
      params.push_back(id->ID()->getText());
    }
  }
  const auto name = context->procedureName()->getText();
  if (ec_.module->has_fn(name)) {
    ec_.errors.push_back(fmt::format("{}:{}: error: function '{}' is already defined", filename_,
                                     context->getStart()->getLine(), name));
  }
  ec_.module->functions.insert_or_assign(name, BasicFunction(name, context, params));
  return {};
}

std::any FunctionDefVisitor::visitModuleDefinition(BasicParser::ModuleDefinitionContext* context) {
  const auto name = remove_quotes(context->STRING()->getText());
  if (!contains(ec_.modules, name)) {
    ec_.add_module(name);
  }
  ec_.module = &ec_.modules.at(name);
  return {};
}

} // namespace wwivbasic
