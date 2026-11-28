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
#ifndef INCLUDED_BBS_BASIC_NEXT_DEBUG_SERVER_H
#define INCLUDED_BBS_BASIC_NEXT_DEBUG_SERVER_H

#include "bbs/basic/next/debug_session.h"

#include <memory>
#include <string>
#include <thread>

namespace httplib {
class Server;
}

namespace wwiv::bbs::basic {

// The script debugger's HTTP API (/debug/v1, used by wwivdbg) over a
// DebugSession. See docs/DEBUGGER.md in wwivbasic/ for the endpoints.
class DebugServer {
public:
  explicit DebugServer(DebugSession& session);
  // Stops the server and waits for its thread.
  ~DebugServer();
  DebugServer(const DebugServer&) = delete;
  DebugServer& operator=(const DebugServer&) = delete;

  // Listens on host:port in a thread of its own. False if it can't (the port
  // is in use, say).
  bool Start(const std::string& host, int port);

  // The state as GET /debug/v1/state returns it (a JSON object).
  static std::string StateJson(const DebugSession& session);

private:
  DebugSession& session_;
  std::unique_ptr<httplib::Server> svr_;
  std::thread thread_;
};

} // namespace wwiv::bbs::basic

#endif
