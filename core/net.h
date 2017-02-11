/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2016-2017, WWIV Software Services         */
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
#ifndef __INCLUDED_WWIV_CORE_NET_H__
#define __INCLUDED_WWIV_CORE_NET_H__
#pragma once

#include <string>

#if defined( _WIN32 )

#define NOCRYPT // Disable include of wincrypt.h
#include <winsock2.h>
#include <WS2tcpip.h>

// Really windows?
typedef int socklen_t;

#else 

#include <netdb.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/types.h>
#include <sys/socket.h>

typedef int HANDLE;
typedef int SOCKET;
constexpr int INVALID_SOCKET = -1;
constexpr int SOCKET_ERROR = -1;
constexpr int NO_ERROR = -1;
#define closesocket(s) ::close(s)
#endif // _WIN32

namespace wwiv {
namespace core {

bool InitializeSockets();

bool GetRemotePeerAddress(SOCKET socket, std::string& ip);
bool GetRemotePeerHostname(SOCKET socket, std::string& hostname);

SOCKET CreateListenSocket(int port);

}  // namespace core
}  // namespace wwiv

#endif  // __INCLUDED_WWIV_CORE_NET_H__
