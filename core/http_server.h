/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2017, WWIV Software Services            */
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
#ifndef __INCLUDED_WWIV_CORE_HTTP_SERVER_H__
#define __INCLUDED_WWIV_CORE_HTTP_SERVER_H__
#pragma once

#include <functional>
#include <map>
#include <string>
#include <vector>

#include "core/net.h"
#include "core/socket_connection.h"

#ifdef DELETE
#undef DELETE
#endif  // DELETE

namespace wwiv {
namespace core {

// Subset from https://www.w3.org/Protocols/rfc2616/rfc2616-sec6.html#sec6.1.1
static std::map<int, std::string> CreateHttpStatusMap() {
  std::map<int, std::string> m = {
    { 200, "OK" },
    { 204, "No Content" },
    { 301, "Moved Permanently" },
    { 302, "Found" },
    { 304, "Not Modified" },
    { 307, "Temporary Redirect" },
    { 400, "Bad Request" },
    { 401, "Unauthorized" },
    { 403, "Forbidden" },
    { 404, "Not Found" },
    { 405, "Method Not Allowed" },
    { 406, "Not Acceptable" },
    { 408, "Request Time-out" },
    { 412, "Precondition Failed" },
    { 500, "Internal Server Error" },
    { 501, "Not Implemented" },
    { 503, "Service Unavailable" }
  };
  return m;
}

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
 * Encapsulates an HTTP Resposne to send to a client.
 */
class HttpResponse {
public:
  HttpResponse(int s) : status(s) {};
  HttpResponse(int s, const std::string& t) : status(s), text(t) {};
  HttpResponse(int s, std::map<std::string, std::string>& h, const std::string& t) : status(s), headers(h), text(t) {};

  int status;
  std::map<std::string, std::string> headers;
  std::string text;
};

class HttpHandler {
public:
  virtual HttpResponse Handle(HttpMethod method, const std::string& path, std::vector<std::string> headers) = 0;
};

/**
 * Simple HTTP 1.1 Server that can handle GET requests. 
 */
class HttpServer {
public:
  HttpServer(SocketConnection& conn);
  virtual ~HttpServer();
  /** Adds a handler (handler) for method method and URL path root {root). */
  bool add(HttpMethod method, const std::string& root, HttpHandler* handler);
  /** Sends an HTTP Response and then terminates the connection. */
  void SendResponse(HttpResponse& r);
  /** Runs the Http Server. It must already have all of the handlers needed added to it. */
  bool Run();

private:
  SocketConnection conn_;
  std::map<std::string, HttpHandler*> get_;
};


}  // namespace core
}  // namespace wwiv

#endif  // __INCLUDED_WWIV_CORE_HTTP_SERVER_H__
