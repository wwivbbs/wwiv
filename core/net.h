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
#include <chrono>
#include <functional>
#include <map>
#include <optional>
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

std::optional<std::string> GetRemotePeerAddress(SOCKET socket);
std::optional<std::string> GetRemotePeerHostname(SOCKET socket);

SOCKET CreateListenSocket(int port);

/**
 * Returns true if address is a private IP address as defined in RFC1918.
 */
bool is_rfc1918_private_address(const std::string& address);

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
 * Owns a socket and closes it exactly once.
 *
 * Move-only. Use release() when handing the socket to something else that
 * will close it, and reset() to close it early (for example right after a
 * child process has been spawned with its own copy).
 */
class SocketHandle final {
public:
  SocketHandle() noexcept = default;
  explicit SocketHandle(SOCKET sock) noexcept : sock_(sock) {}
  ~SocketHandle() { reset(); }

  SocketHandle(const SocketHandle&) = delete;
  SocketHandle& operator=(const SocketHandle&) = delete;
  SocketHandle(SocketHandle&& o) noexcept : sock_(o.release()) {}
  SocketHandle& operator=(SocketHandle&& o) noexcept {
    if (this != &o) {
      reset(o.release());
    }
    return *this;
  }

  [[nodiscard]] SOCKET get() const noexcept { return sock_; }
  [[nodiscard]] bool valid() const noexcept { return sock_ != INVALID_SOCKET; }
  explicit operator bool() const noexcept { return valid(); }

  /** Gives up ownership without closing; returns the socket. */
  [[nodiscard]] SOCKET release() noexcept {
    const auto s = sock_;
    sock_ = INVALID_SOCKET;
    return s;
  }

  /** Closes the current socket (if any) and takes ownership of sock. */
  void reset(SOCKET sock = INVALID_SOCKET) noexcept;

private:
  SOCKET sock_{INVALID_SOCKET};
};

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
  /**
   * timeout is how long each select waits, so also how long Run takes to
   * notice exit_signal. Zero waits forever (exit_signal is then only seen
   * after a connection).
   */
  explicit SocketSet(std::chrono::milliseconds timeout);
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
  const std::chrono::milliseconds timeout_;
};

} // namespace

#endif
