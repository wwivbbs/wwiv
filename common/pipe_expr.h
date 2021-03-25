/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2020-2021, WWIV Software Services             */
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
#ifndef INCLUDED_COMMON_PIPE_EXPR_H
#define INCLUDED_COMMON_PIPE_EXPR_H

#include "common/context.h"
#include <string>
namespace wwiv::common {
class pipe_expr_token_t;

class PipeEval {
public:
  explicit PipeEval(Context& context);
  std::string eval(std::string);

private:
  std::string eval_variable(const std::string& var,
                            const std::vector<pipe_expr_token_t>& remaining);

  std::optional<pipe_expr_token_t> parse_variable(std::string::const_iterator& it,
                                                  const std::string::const_iterator& end);
  std::vector<pipe_expr_token_t> tokenize(std::string::const_iterator& it,
                                          const std::string::const_iterator& end);

  std::string eval_fn(const std::string& fn, const std::vector<pipe_expr_token_t>& args);
  std::string eval(std::vector<pipe_expr_token_t>& tokens);
  std::string evaluate_pipe_expression_string(const std::string& expr);
  // Not owned
  Context& context_;
  typedef std::function<std::string(Context&, const std::vector<pipe_expr_token_t>&)> fn_fn;
  std::map<std::string, fn_fn> fn_map_;
};

enum class pipe_fmt_align_t { left, mid, right };

struct pipe_fmt_mask {
  int len = 0;
  pipe_fmt_align_t align{pipe_fmt_align_t::left};
  char pad{' '};
};

pipe_fmt_mask parse_mask(const std::string& mask);

}
#endif
