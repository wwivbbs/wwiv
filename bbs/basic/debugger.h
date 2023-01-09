/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2023, WWIV Software Services                  */
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
#ifndef INCLUDED_BBS_BASIC_DEBUGGER_H
#define INCLUDED_BBS_BASIC_DEBUGGER_H

#include "bbs/basic/debug_state.h"
#include "bbs/basic/util.h"

#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

namespace httplib {
class Client;
class ContentReader;
struct Response;
struct Request;
class Server;
} // namespace httplib

namespace wwiv::bbs::basic {

class Debugger {
public:
  Debugger() = default;
  ~Debugger();
  DebugState& current_debug_state();
  bool attached() const;

  // Debugger Core
  bool StartServers(int port);
  bool WaitForDebuggerToAttach();

private:
  void Attach(const httplib::Request& req, httplib::Response& res,
              const httplib::ContentReader& content_reader);
  void Detach(const httplib::Request& req, httplib::Response& res,
              const httplib::ContentReader& content_reader);
  void DetachImpl();

  // HTTP action handlers
  std::string run(const httplib::Request&, httplib::Response& res);
  std::string stop(const httplib::Request&, httplib::Response& res);
  std::string stepover(const httplib::Request&, httplib::Response& res);
  std::string tracein(const httplib::Request&, httplib::Response& res);
  // HTTP Getters
  std::string state(const httplib::Request&, httplib::Response& res);
  std::string stack(const httplib::Request&, httplib::Response& res);
  std::string source(const httplib::Request&, httplib::Response& res);

  std::string status(const httplib::Request&, httplib::Response& res);
  std::string watch(const httplib::Request&, httplib::Response& res);
  std::string breakpoints(const httplib::Request&, httplib::Response& res);


private:
  mutable std::mutex mu_;
  DebugState debug_state_;
  bool attached_{false};
  std::thread srv_thread_;
  std::unique_ptr<httplib::Server> svr_;
};

} // namespace wwiv::bbs::basic

#endif
