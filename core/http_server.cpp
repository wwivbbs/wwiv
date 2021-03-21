/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2016-2021, WWIV Software Services          */
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

#include "core/datetime.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/format.h"
#include <string>
#include <vector>

using namespace wwiv::strings;

namespace wwiv::core {

// Subset from https://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html#sec6.1.1
std::map<int, std::string> CreateHttpStatusMap() {
  std::map<int, std::string> m = {
      {200, "OK"},
      {204, "No Content"},
      {301, "Moved Permanently"},
      {302, "Found"},
      {304, "Not Modified"},
      {307, "Temporary Redirect"},
      {400, "Bad Request"},
      {401, "Unauthorized"},
      {403, "Forbidden"},
      {404, "Not Found"},
      {405, "Method Not Allowed"},
      {406, "Not Acceptable"},
      {408, "Request Time-out"},
      {412, "Precondition Failed"},
      {500, "Internal Server Error"},
      {501, "Not Implemented"},
      {503, "Service Unavailable"}
  };
  return m;
}

HttpServer::HttpServer(std::unique_ptr<SocketConnection> conn)
  : conn_(std::move(conn)) {
}

HttpServer::~HttpServer() = default;

bool HttpServer::add(HttpMethod method, const std::string& root, HttpHandler* handler) {
  if (method != HttpMethod::GET) {
    return false;
  }
  get_.emplace(root, handler);
  return true;
}


static std::string current_time_as_string() {
  const auto dt = DateTime::now();
  return dt.to_string();
};

void HttpServer::SendResponse(const HttpResponse& r) {
  static const auto statuses = CreateHttpStatusMap();
  const auto d = std::chrono::seconds(1);
  conn_->send_line(StrCat("HTTP/1.1 ", r.status, " ", statuses.at(r.status)), d);
  conn_->send_line(StrCat("Date: ", current_time_as_string()), d);
  conn_->send_line(fmt::format("Server: wwivd/{}", full_version()), d);
  if (!r.text.empty()) {
    const auto content_length = r.text.size();
    conn_->send_line(StrCat("Content-Length: ", content_length), d);
    conn_->send("\r\n", d);
    conn_->send(r.text, d);
  } else {
    conn_->send_line("Connection: close", d);
    conn_->send("\r\n", d);
  }
}

static std::vector<std::string> read_lines(SocketConnection* conn) {
  std::vector<std::string> lines;
  while (true) {
    auto s = conn->read_line(1024, std::chrono::milliseconds(10));
    if (!s.empty()) {
      lines.push_back(s);
    } else {
      break;
    }
  }
  return lines;
}

bool HttpServer::Run() {
  const auto inital_requestline = conn_->read_line(1024, std::chrono::milliseconds(10));
  if (inital_requestline.empty()) {
    return false;
  }
  auto cmd_parts = SplitString(inital_requestline, " ");
  if (cmd_parts.size() < 3) {
    return false;
  }
  const auto& path = cmd_parts.at(1);
  const auto headers = read_lines(conn_.get());
  if (const auto & cmd = cmd_parts.at(0); cmd == "GET") {
    for (const auto& e : get_) {
      if (starts_with(path, e.first)) {
        const auto r = e.second->Handle(HttpMethod::GET, path, headers);
        SendResponse(r);
        return true;
      }
    }
    SendResponse(HttpResponse(404));
    return true;
  }
  SendResponse(HttpResponse(405));
  return false;
}


}
