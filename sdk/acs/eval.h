/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
/*                Copyright (C)2020, WWIV Software Services               */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/**************************************************************************/
#ifndef __INCLUDED_SDK_ACS_EVAL_H__
#define __INCLUDED_SDK_ACS_EVAL_H__

#include "core/parser/ast.h"
#include "core/parser/lexer.h"
#include "sdk/acs/value.h"
#include "sdk/acs/valueprovider.h"
#include <any>
#include <iostream>
#include <unordered_map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace wwiv::sdk::acs {

/** 
 * Evaluation engine for evaluating expressions provides.
 */
class Eval : public wwiv::core::parser::AstVisitor {
public:
  explicit Eval(std::string expression);
  virtual ~Eval() = default;

  bool eval();
  bool add(const std::string& prefix, std::unique_ptr<ValueProvider>&& p);
  std::optional<Value> to_value(wwiv::core::parser::Factor* n);

  // Visitor implementation
  virtual void visit(wwiv::core::parser::AstNode*) override {}
  virtual void visit(wwiv::core::parser::Expression* n) override;
  virtual void visit(wwiv::core::parser::Factor* n) override;

private:
  std::string expression_;
  std::unordered_map<std::string, std::unique_ptr<ValueProvider>> providers_;
  std::unordered_map<int, Value> values_;
};  // class

} 

#endif // __INCLUDED_SDK_FILES_TIC_H__