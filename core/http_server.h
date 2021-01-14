/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2017-2021, WWIV Software Services         */
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
#ifndef INCLUDED_CORE_HTTP_SERVER_H
#define INCLUDED_CORE_HTTP_SERVER_H

#include "core/net.h"
#include "core/socket_connection.h"
#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#ifdef DELETE
#undef DELETE
#endif  // DELETE

namespace wwiv::core {

/** List of HTTP Methods */
enum class HttpMethod {
  OPTIONS,
  GET,
  HEAD,
  POST,
  PUT,
  DELETE,
  TRACE,
  CONNECT
};

/**
 * Encapsulates an HTTP Response to send to a client.
 */
class HttpResponse {
public:
  explicit HttpResponse(const int s)
    : status(s) {
  };

  HttpResponse(const int s, std::string t)
    : status(s), text(std::move(t)) {
  }

  HttpResponse(int s, std::map<std::string, std::string>& h, std::string t)
    : status(s), headers(h), text(std::move(t)) {
  }

  int status;
  std::map<std::string, std::string> headers;
  std::string text;
};

class HttpHandler {
public:
  virtual ~HttpHandler() = default;
  virtual HttpResponse Handle(HttpMethod method, const std::string& path,
                              std::vector<std::string> headers) = 0;
};

/**
 * Simple HTTP 1.1 Server that can handle GET requests. 
 */
class HttpServer final {
public:
  explicit HttpServer(std::unique_ptr<SocketConnection> conn);
  ~HttpServer();
  /** Adds a handler (handler) for method method and URL path root {root). */
  bool add(HttpMethod method, const std::string& root, HttpHandler* handler);
  /** Sends an HTTP Response and then terminates the connection. */
  void SendResponse(const HttpResponse& r);
  /** Runs the Http Server. It must already have all of the handlers needed added to it. */
  bool Run();

private:
  std::unique_ptr<SocketConnection> conn_;
  std::map<std::string, HttpHandler*> get_;
};


}

#endif
