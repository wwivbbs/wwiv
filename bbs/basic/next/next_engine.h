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

#include <functional>
#include <memory>
#include <string>

namespace wwivbasic {
class Interpreter;
}

namespace wwiv::common {
class Context;
class Input;
class Output;
} // namespace wwiv::common

namespace wwiv::sdk {
class Config;
}

namespace wwiv::bbs::basic {

// Runs scripts on wwivbasic, with a BbsConsole over bout/bin and the wwiv.*
// modules (wwiv_bindings.h). Built only with WWIV_WWIVBASIC. With bbs --debug,
// each script waits for a debugger to attach (wwivbasic/docs/DEBUGGER.md).
class FileBase;
class MessageBase;
class UserBase;

class NextEngine final {
public:
  // Called with each script's interpreter after the wwiv.* modules are added
  // and before the script runs. Tests use it to add an ASSERT module.
  using setup_fn = std::function<void(wwivbasic::Interpreter&)>;

  NextEngine(common::Input& in, common::Output& out, const sdk::Config& config,
             common::Context* ctx, setup_fn on_setup = {});
  ~NextEngine();
  NextEngine(const NextEngine&) = delete;
  NextEngine& operator=(const NextEngine&) = delete;

  // Runs the script scriptdir/script_name.
  bool RunScript(const std::string& script_name);
  // Runs script text as the module named module.
  bool RunScript(const std::string& module, const std::string& text);

  // What wwiv.msg reads. Without one, scripts have no wwiv.msg module.
  void set_message_base(std::unique_ptr<MessageBase> msgs);
  // What wwiv.user reads. Without one, scripts have no wwiv.user module.
  void set_user_base(std::unique_ptr<UserBase> users);
  // What wwiv.files reads. Without one, scripts have no wwiv.files module.
  void set_file_base(std::unique_ptr<FileBase> files);

private:
  // Runs text as the script scriptdir/name, with the module name module
  // (lower case). Errors are shown with paths relative to scriptdir.
  bool RunText(const std::string& name, const std::string& module, const std::string& text);
  // Shows one line of an error message in color 6, without interpreting it.
  void ShowError(const std::string& line);

  common::Input& in_;
  common::Output& out_;
  const sdk::Config& config_;
  common::Context* ctx_;
  setup_fn on_setup_;
  std::unique_ptr<MessageBase> msgs_;
  std::unique_ptr<UserBase> users_;
  std::unique_ptr<FileBase> files_;
};

} // namespace wwiv::bbs::basic

#endif
