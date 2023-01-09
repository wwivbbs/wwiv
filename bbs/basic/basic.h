/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_BASIC_BASIC_H
#define INCLUDED_BBS_BASIC_BASIC_H

#include "bbs/basic/util.h"
#include "bbs/basic/debug_state.h"
#include "bbs/basic/debugger.h"
#include <mutex>
#include <optional>
#include <string>
#include <thread>

namespace httplib {
class Client;
class ContentReader;
struct Response;
struct Request;
class Server;
} // namespace httplib

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
  ~Basic();

  bool RunScript(const std::string& script_name);
  bool RunScript(const std::string& module, const std::string& text);
  bool StartDebugger(int port);
  bool WaitForDebuggerToAttach();
  [[nodiscard]] mb_interpreter_t* bas() const noexcept { return bas_; }

  // Debugger support
  void AttachDebugger(const httplib::Request& req, httplib::Response& res,
                      const httplib::ContentReader& content_reader);
  void DetachDebugger(const httplib::Request& req, httplib::Response& res,
                      const httplib::ContentReader& content_reader);
  void DetachDebuggerImpl();
  bool debugger_attached() const;
  Debugger* debugger();

  DebugState& current_debug_state();

private:
  bool RegisterDefaultNamespaces();
  mb_interpreter_t* SetupBasicInterpreter();
  static bool LoadBasicFile(mb_interpreter_t* bas, const std::string& script_name);

  common::Input& bin_;
  common::Output& bout_;
  const sdk::Config& config_;
  const common::Context* ctx_;
  std::unique_ptr<BasicScriptState> script_userdata_;

  mb_interpreter_t* bas_;

  std::thread srv_thread_;
  std::unique_ptr<httplib::Server> svr_;

  // Guard everything the debugger touches
  mutable std::mutex mu_;
  bool debugger_attached_{false};
  std::unique_ptr<DebugState> debug_state_;
  std::unique_ptr<Debugger> debugger_;
  std::string initial_module_;
  std::string initial_module_source_;
};

bool RunBasicScript(const std::string& script_name);

} // namespace wwiv::bbs

#endif
