/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2016-2017, WWIV Software Services          */
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
/*                                                                        */
/**************************************************************************/
#include "core/http_server.h"

#include <vector>

#include "core/strings.h"
#include "core/version.h"
#include "sdk/datetime.h"

using namespace wwiv::strings;

namespace wwiv {
namespace core {

HttpServer::HttpServer(SocketConnection& conn) : conn_(conn) {}
HttpServer::~HttpServer() {}

bool HttpServer::add(HttpMethod method, const std::string& root, HttpHandler* handler) {
  if (method != HttpMethod::GET) {
    return false;
  }
  get_.emplace(root, handler);
  return true;
}

void HttpServer::SendResponse(HttpResponse& r) {
  static const auto statuses = CreateHttpStatusMap();
  const auto d = std::chrono::seconds(1);
  conn_.send_line(StrCat("HTTP/1.1 ", r.status, " ", statuses.at(r.status)), d);
  conn_.send_line(StrCat("Date: ", wwiv::sdk::daten_to_wwivnet_time(time(nullptr))), d);
  conn_.send_line(StrCat("Server: wwivd/", wwiv_version, beta_version), d);
  if (!r.text.empty()) {
    auto content_length = r.text.size();
    conn_.send_line(StrCat("Content-Length: ", content_length), d);
    conn_.send("\r\n", d);
    conn_.send(r.text, d);
  }
  else {
    conn_.send_line("Connection: close", d);
    conn_.send("\r\n", d);
  }
}

static std::vector<std::string> read_lines(SocketConnection& conn) {
  std::vector<std::string> lines;
  while (true) {
    auto s = conn.read_line(1024, std::chrono::milliseconds(10));
    if (!s.empty()) {
      lines.push_back(s);
    }
    else {
      break;
    }
  }
  return lines;
}


bool HttpServer::Run() {
  const auto d = std::chrono::seconds(1);
  auto inital_requestline = conn_.read_line(1024, std::chrono::milliseconds(10));
  if (inital_requestline.empty()) {
    return false;
  }
  auto cmd_parts = SplitString(inital_requestline, " ");
  if (cmd_parts.size() < 3) {
    return false;
  }
  const auto& path = cmd_parts.at(1);
  const auto headers = read_lines(conn_);
  const auto& cmd = cmd_parts.at(0);
  if (cmd == "GET") {
    for (const auto& e : get_) {
      if (starts_with(path, e.first)) {
        auto r = e.second->Handle(HttpMethod::GET, path, headers);
        SendResponse(r);
        return true;
      }
    }
    HttpResponse r404(404);
    SendResponse(r404);
    return true;
  }
  HttpResponse r405(405);
  SendResponse(r405);
  return false;
}



}
}