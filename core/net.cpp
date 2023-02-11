/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2016-2022, WWIV Software Services          */
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

// Put these before the windows and os/2 includes. This was barfing on Win64
#include "core/log.h"
#include "core/scope_exit.h"
#include "core/socket_exceptions.h"
#include "core/strings.h"

#ifdef _WIN32

#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif 

#ifndef NOMINMAX
#define NOMINMAX
#endif 

#ifndef NOCRYPT
#define NOCRYPT // Disable include of wincrypt.h
#endif 

#include <winsock2.h>
#include <MSWSock.h>
#include <WS2tcpip.h>

#else

#include <fcntl.h>
#include <unistd.h>

#endif // _WIN32

#ifdef __OS2__
#include <libcx/net.h>
#endif  // __OS2__

using namespace wwiv::strings;

namespace wwiv::core {

bool InitializeSockets() {
#ifdef _WIN32
  WSADATA wsadata;
  const auto result = WSAStartup(MAKEWORD(2, 2), &wsadata);
  if (result != 0) {
    LOG(ERROR) << "WSAStartup failed with error: " << result;
    return false;
  }
#endif // _WIN32
  return true;
}

std::optional<std::string> GetRemotePeerAddress(SOCKET socket) {
  sockaddr_in addr{};
  socklen_t nAddrSize = sizeof(sockaddr);

  const auto result = getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &nAddrSize);
  if (result == -1) {
    return std::nullopt;
  }

  char buf[255];
  std::string ip = inet_ntop(addr.sin_family, &addr.sin_addr, buf, sizeof(buf));
  return ip;
}

std::optional<std::string> GetRemotePeerHostname(SOCKET socket) {
  sockaddr_in addr{};
  socklen_t nAddrSize = sizeof(sockaddr);

  char host[1024];
  char service[81];

  if (getpeername(socket, reinterpret_cast<sockaddr*>(&addr), &nAddrSize) == -1) {
    return std::nullopt;
  }

  // pretend sa is full of good information about the host and port...
  if (getnameinfo(reinterpret_cast<sockaddr*>(&addr), sizeof(addr), host, sizeof(host), service,
                  sizeof(service), 0) != 0) {
    return std::nullopt;
  }

  return std::string(host);
}

SOCKET CreateListenSocket(int port) {
  struct sockaddr_in my_addr{};
  memset(&my_addr, 0, sizeof(sockaddr_in));

  const auto sock = socket(AF_INET, SOCK_STREAM, 0);
  if (sock == INVALID_SOCKET) {
    throw socket_error("Unable to create socket [socket]");
  }
  int optval = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&optval),
                 sizeof(optval)) == -1) {
    throw socket_error("Unable to create socket [SO_REUSEADDR]");
  }
  if (setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&optval),
                 sizeof(optval)) == -1) {
    throw socket_error("Unable to create socket [SO_KEEPALIVE]");
  }
  // Try to set nodelay.
  setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&optval), sizeof(optval));

  my_addr.sin_family = AF_INET;
  my_addr.sin_port = htons(static_cast<decltype(my_addr.sin_port)>(port));
  memset(&(my_addr.sin_zero), 0, 8);
  my_addr.sin_addr.s_addr = INADDR_ANY;

  if (bind(sock, reinterpret_cast<sockaddr*>(&my_addr), sizeof(my_addr)) == -1) {
    const auto msg =
        StrCat("Error binding to socket, make sure nothing else is listening on port: ", port,
               "; errno: ", errno);
    throw socket_error(msg);
  }
  if (listen(sock, 10) == -1) {
    throw socket_error(StrCat("Error listening. errno: ", errno));
  }

  return sock;
}

bool is_rfc1918_private_address(const std::string& ip) {
  // RFC 1918 defined private address space:
  // 10.0.0.0/8 IP addresses: 10.0.0.0 – 10.255.255.255
  // 172.16.0.0/12 IP addresses: 172.16.0.0 – 172.31.255.255
  // 192.168.0.0/16 IP addresses: 192.168.0.0 – 192.168.255.255
  auto parts = SplitString(ip, ".");
  if (parts.size() != 4) {
    // WTF
    return false;
  }
  std::vector<int> o;
  for (const auto& part : parts) {
    o.push_back(to_number<int>(part));
  }
  
  if (o.size() != 4) {
    // WTF
    return false;
  }
  if (o.at(0) == 10) {
    return true;
  }
  if (o.at(0) == 172 && o.at(1) >= 16) {
    return true;
  }
  if (o.at(0) == 192 && o.at(1) == 168) {
    return true;
  }
  return false;
}

/**
 * DBSRBL uses a special format for the IP addresses, so this function
 * maps the FQDN a.b.c.d to d.c.b.a.rbl_address so it may be used to
 * lookup against a DNSRBL.
 */
static std::string dns_rbl_name(const std::string& address, const std::string& rbl_address) {
  std::string out;
  auto v = SplitString(address, ".");
  for (auto it = v.rbegin(); it != v.rend(); ++it) {
    out.append(*it);
    out.push_back('.');
  }
  out.append(rbl_address);
  return out;
}

bool on_dns_dbl(const std::string& address, const std::string& rbl_address) {
  const auto s = dns_rbl_name(address, rbl_address);
  struct addrinfo* res = nullptr;
  const auto result = getaddrinfo(s.c_str(), nullptr, nullptr, &res);
  if (result != 0) {
    return false;
  }
  freeaddrinfo(res);
  return true;
}

int get_dns_cc(const std::string& address, const std::string& rbl_address) {
  const auto s = dns_rbl_name(address, rbl_address);
  struct addrinfo* res = nullptr;
  const auto result = getaddrinfo(s.c_str(), nullptr, nullptr, &res);
  if (result != 0) {
    return 0;
  }

  auto at_exit = finally([res] { freeaddrinfo(res); });
  if (res->ai_family == AF_INET) {
    const auto ipv4 = reinterpret_cast<struct sockaddr_in*>(res->ai_addr);
    const uint32_t b = htonl(ipv4->sin_addr.s_addr) & 0x0000ffff;
    return b;
  }
  return 0;
}

bool SetBlockingMode(SOCKET sock) {
  if (sock == INVALID_SOCKET) {
    return false;
  }

#ifdef _WIN32
  u_long blocking = 1;
  return ioctlsocket(sock, FIONBIO, &blocking) == NO_ERROR;
#else  // _WIN32
  int flags = fcntl(sock, F_GETFL, 0 /* ignored */);
  flags &= ~O_NONBLOCK;
  return fcntl(sock, F_SETFL, flags) != -1;
#endif // _WIN32
}

SocketSet::SocketSet()
  : SocketSet(2) {
};

SocketSet::SocketSet(int timeout_seconds)
  : timeout_seconds_(timeout_seconds) {
};

SocketSet::~SocketSet() = default;

bool SocketSet::add(int port, const socketset_accept_fn& fn, const std::string& description) {
  auto s = CreateListenSocket(port);
  if (s == INVALID_SOCKET) {
    return false;
  }
  LOG(INFO) << "Listening to " << description << " on port: " << port;
  socket_fn_map_.emplace(s, fn);
  socket_port_map_.emplace(s, port);
  return true;
}

bool SocketSet::Run(std::atomic<bool>& exit_signal) {
  while (true) {
    if (exit_signal.load() == true) {
      return true;
    }
    if (!RunOnce()) {
      return false;
    }
  }
}

bool SocketSet::RunOnce() {
  SOCKET max_fd = 0;
  fd_set fds{};
  FD_ZERO(&fds);
  for (const auto& e : socket_fn_map_) {
    if (e.first > max_fd) {
      max_fd = e.first;
    }
    FD_SET(e.first, &fds);
  }

  if (max_fd == INVALID_SOCKET) {
    LOG(ERROR) << "Nothing to do!";
    return false;
  }

  VLOG(3) << "About to call select. (" << max_fd << "); timeout: "
	  << timeout_seconds_;
  int status;
  if (timeout_seconds_ > 0) {
    timeval timeout{};
    timeout.tv_usec = 0;
    timeout.tv_sec = timeout_seconds_;
    status = select(max_fd + 1, &fds, nullptr, nullptr, &timeout);
  } else {
    status = select(max_fd + 1, &fds, nullptr, nullptr, nullptr);
  }

  VLOG(3) << "After select; status: " << status << "; errno: " << errno;
  if (status < 0 && errno == EINTR) {
    LOG(ERROR) << "Caught signal calling select";
    // return true so we can check for exit signal.
    return true;
  }
  if (status < 0) {
    LOG(ERROR) << "Error calling select; errno: [" << errno << "]";
    // return false here since we know this wasn't a signal.
    return false;
  }
  if (status == 0) {
    // Timeout expired.  Keep on trucking.
    VLOG(4) << "timeout expired on select";
    return true;
  }
  VLOG(4) << "After status checks for: " << status;

  for (const auto& e : socket_fn_map_) {
    VLOG(4) << "Checking Socket map for: " << e.first;
    if (FD_ISSET(e.first, &fds)) {
      VLOG(4) << "FD Set: " << e.first;
      socklen_t addr_size = sizeof(sockaddr_in);
      struct sockaddr_in saddr{};
      const auto client_sock = accept(e.first, reinterpret_cast<sockaddr*>(&saddr), &addr_size);

#ifdef _WIN32
      auto newvalue = SO_SYNCHRONOUS_NONALERT;
      setsockopt(client_sock, SOL_SOCKET, SO_OPENTYPE, reinterpret_cast<char*>(&newvalue),
                 sizeof(newvalue));
#endif
      VLOG(4) << "Calling e.second";
      e.second({client_sock, socket_port_map_.at(e.first)});
    }
  }
  return true;
}

} // namespace wwiv
