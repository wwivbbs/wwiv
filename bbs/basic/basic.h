/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_BASIC_H
#define INCLUDED_BBS_BASIC_H

#include "bbs/basic/util.h"
#include <string>

namespace wwiv::common {
class Input;
class Output;
}

namespace wwiv::sdk {
class Config;
class User;
} // namespace wwiv::sdk

namespace wwiv::bbs::basic {

class Basic {
public:
  Basic(common::Input& i, common::Output& o, const sdk::Config& config,
        common::Context* ctx);

  bool RunScript(const std::string& script_name);
  bool RunScript(const std::string& module, const std::string& text);
  [[nodiscard]] mb_interpreter_t* bas() const noexcept { return bas_; }

private:
  bool RegisterDefaultNamespaces();
  static mb_interpreter_t* SetupBasicInterpreter();

  common::Input& bin_;
  common::Output& bout_;
  const sdk::Config& config_;
  const common::Context* ctx_;
  std::unique_ptr<BasicScriptState> script_userdata_;

  mb_interpreter_t* bas_;
};

bool RunBasicScript(const std::string& script_name);

} // namespace wwiv::bbs

#endif
