/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#ifndef INCLUDED_BBS_BASIC_NEXT_NEXT_ENGINE_H
#define INCLUDED_BBS_BASIC_NEXT_NEXT_ENGINE_H

#include "bbs/basic/script_engine.h"

#include <string>

namespace wwiv::bbs::basic {

// Runs scripts on wwivbasic, with a BbsConsole over bout/bin. Built only with
// WWIV_WWIVBASIC_NEXT.
//
// TODO(M4): the wwiv.* modules (see wwivbasic/docs/PLAN.md).
class NextEngine final : public ScriptEngine {
public:
  NextEngine(common::Input& in, common::Output& out, const sdk::Config& config,
             common::Context* ctx);
  ~NextEngine() override;

  bool RunScript(const std::string& script_name) override;
  bool RunScript(const std::string& module, const std::string& text) override;

private:
  // Runs text as the script scriptdir/name. Errors are shown with paths
  // relative to scriptdir.
  bool RunText(const std::string& name, const std::string& text);
  // Shows one line of an error message in color 6, without interpreting it.
  void ShowError(const std::string& line);

  common::Input& in_;
  common::Output& out_;
  const sdk::Config& config_;
  common::Context* ctx_;
};

} // namespace wwiv::bbs::basic

#endif
