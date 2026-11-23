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
#include "bbs/basic/next/next_engine.h"

#include "common/context.h"
#include "common/output.h"
#include "core/log.h"
#include "sdk/config.h"
#include "wwivbasic/interpreter.h"

namespace wwiv::bbs::basic {

NextEngine::NextEngine(common::Input& in, common::Output& out, const sdk::Config& config,
                       common::Context* ctx, setup_fn on_setup)
    : in_(in), out_(out), config_(config), ctx_(ctx), on_setup_(std::move(on_setup)) {}

NextEngine::~NextEngine() = default;

bool NextEngine::RunScript(const std::string& script_name) {
  return RunScript(script_name, std::string{});
}

bool NextEngine::RunScript(const std::string& module, const std::string&) {
  if (!config_.scripting_enabled()) {
    out_.outstr("WWIVbasic scripting is not enabled on this system.");
    return false;
  }
  if (ctx_ && ctx_->session_context().debug_wwivbasic()) {
    // TODO(M6): the debugger (/debug/v1) for the new engine.
    LOG(WARNING) << "The WWIVbasic debugger isn't supported on the new engine yet; running '"
                 << module << "' without it.";
    out_.pl("|#6The debugger isn't supported on the new BASIC engine yet.");
  }
  LOG(ERROR) << "The new BASIC engine can't run scripts yet: '" << module << "'";
  out_.pl("|#6The new BASIC engine can't run scripts yet.");
  return false;
}

} // namespace wwiv::bbs::basic
