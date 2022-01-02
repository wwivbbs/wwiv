/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2016-2022, WWIV Software Services         */
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
#ifndef INCLUDED_CORE_NET_H
#define INCLUDED_CORE_NET_H

#include <atomic>
#include <functional>
#include <map>
#include <string>

#if defined( _WIN32 )

typedef int socklen_t;

#if defined(_WIN64)
typedef unsigned __int64 SOCKET;
#else
typedef unsigned int SOCKET;
#endif

#ifndef INVALID_SOCKET
#define INVALID_SOCKET (SOCKET)(~0)
#endif

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

namespace wwiv::core {

bool InitializeSockets();

bool GetRemotePeerAddress(SOCKET socket, std::string& ip);
bool GetRemotePeerHostname(SOCKET socket, std::string& hostname);

SOCKET CreateListenSocket(int port);

/**
 * Returns true if address is contained in the DNSRBL rbl_address.
 */
bool on_dns_dbl(const std::string& address, const std::string& rbl_address);

/**
 * Gets the DNS country code using rbl_address
 */
int get_dns_cc(const std::string& address, const std::string& rbl_address);

/** Sets the socket to blocking mode. */
bool SetBlockingMode(SOCKET sock);

/** 
 * Once a socket is accepted from the remote system.  Return
 * the socket and also the port that it was accepted from.
 */
struct accepted_socket_t {
  SOCKET client_socket;
  int port;
};

/**
 * Handles select over a set of sockets.
 */
class SocketSet final {
public:
  SocketSet();
  explicit SocketSet(int timeout_seconds);
  ~SocketSet();

  typedef std::function<void(accepted_socket_t)> socketset_accept_fn;

  /** 
   * Adds a port that the SocketSet will listen to and the function (fn)
   * that will be invoked on the current thread when a connection is
   * accepted.  If fn should be handled asynchronously, then it should
   * dispatch to a worker thread.
   */
  bool add(int port, const socketset_accept_fn& fn, const std::string& description);

  /** 
   * Runs the select/accept/execute loop until exit_signal is true.
   * returning false on error or true of signaled to exit.
   */
  bool Run(std::atomic<bool>& exit_signal);

private:
  /** Runs the select/accept/execute loops once, returning false on error. */
  bool RunOnce();

  std::map<SOCKET, int> socket_port_map_;
  std::map<SOCKET, socketset_accept_fn> socket_fn_map_;
  const int timeout_seconds_;
};

} // namespace

#endif
