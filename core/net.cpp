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
#include "core/net.h"

#ifdef _WIN32

#pragma comment(lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#endif  // _WIN32

#include "core/log.h"
#include "core/socket_exceptions.h"
#include "core/strings.h"

using std::string;
using namespace wwiv::strings;

namespace wwiv {
namespace core {

bool InitializeSockets() {
#ifdef _WIN32
  WSADATA wsadata;
  int result = WSAStartup(MAKEWORD(2, 2), &wsadata);
  if (result != 0) {
    LOG(ERROR) << "WSAStartup failed with error: " << result;
    return false;
  }
#endif  // _WIN32
  return true;
}

bool GetRemotePeerAddress(SOCKET socket, std::string& ip) {
  sockaddr_in addr = {};
  socklen_t nAddrSize = sizeof(sockaddr);

  int result = getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &nAddrSize);
  if (result == -1) {
    return false;
  }

#ifdef _WIN32
  // inet_ntop is not available on Windows XP, but is deprecated.
  ip = inet_ntoa(addr.sin_addr);
#else
  char buf[255];
  ip = inet_ntop(addr.sin_family, &addr.sin_addr, buf, sizeof(buf));
#endif
  return true;
}

bool GetRemotePeerHostname(SOCKET socket, std::string& hostname) {
  sockaddr_in addr = {};
  socklen_t nAddrSize = sizeof(sockaddr);

  char host[1024];
  char service[81];

  int result = getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &nAddrSize);
  if (result == -1) {
    return false;
  }
  // pretend sa is full of good information about the host and port...

  result = getnameinfo(
    reinterpret_cast<sockaddr *>(&addr), sizeof(addr),
    host, sizeof(host),
    service, sizeof(service), 0);

  if (result != 0) {
    return false;
  }

  hostname = host;
  return true;
}

SOCKET CreateListenSocket(int port) {
  struct sockaddr_in my_addr;
  SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    throw socket_error("Unable to create socket [socket]");
  }
  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&optval), sizeof(optval)) == -1) {
    throw socket_error("Unable to create socket [SO_REUSEADDR]");
  }
  if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&optval), sizeof(optval)) == -1) {
    throw socket_error("Unable to create socket [SO_KEEPALIVE]");
  }
  // Try to set nodelay.
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&optval), sizeof(optval));

  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(static_cast<decltype(my_addr.sin_port)>(port));
  memset(&(my_addr.sin_zero), 0, 8);
  my_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, (sockaddr*)&my_addr, sizeof(my_addr)) == -1) {
    const string msg = StrCat(
      "Error binding to socket, make sure nothing else is listening on port: ", 
      port, "; errno: ", errno);
    throw socket_error(msg);
  }
  if (listen(sock, 10) == -1) {
    throw socket_error(StrCat("Error listening. errno: ", errno));
  }

  return sock;
}

}  // namespace os
}  // namespace wwiv
