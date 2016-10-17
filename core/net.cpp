/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2016 WWIV Software Services             */
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
#include "WS2tcpip.h"
// Really windows?
typedef int socklen_t;
#else

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>

typedef int HANDLE;
typedef int SOCKET;
constexpr int SOCKET_ERROR = -1;
#define closesocket(s) close(s)
#endif  // _WIN32

using std::string;

namespace wwiv {
namespace core {

bool GetRemotePeerAddress(SOCKET socket, std::string& ip) {
  sockaddr_in addr = {};
  socklen_t nAddrSize = sizeof(sockaddr);

  int result = getpeername(socket, reinterpret_cast<sockaddr *>(&addr), &nAddrSize);
  if (result == -1) {
    return false;
  }

  char buf[255];
  ip = inet_ntop(addr.sin_family, &addr.sin_addr, buf, sizeof(buf));
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


}  // namespace os
}  // namespace wwiv
