/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
/**************************************************************************/
#include "core/socket_connection.h"

#include <cerrno>
#include <chrono>
#include <cstring>
#include <thread>
#ifdef _WIN32
#include <WS2tcpip.h>
#include <WinSock2.h>
#pragma comment(lib, "Ws2_32.lib")
#pragma comment(lib, "Mswsock.lib")
#pragma comment(lib, "AdvApi32.lib")
#else // _WIN32
#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#endif // _WIN32

#ifdef __OS2__
#include <libcx/net.h>
#endif  // __OS2__

#include "scope_exit.h"
#include "stl.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/socket_exceptions.h"
#include "core/strings.h"
#include "fmt/printf.h"

using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::system_clock;
using std::chrono::time_point;
using wwiv::os::sleep_for;
using namespace wwiv::strings;

namespace wwiv::core {

namespace {

const auto SLEEP_MS = milliseconds(100);

bool SetBlockingMode(SOCKET sock, bool blocking_mode) {
  if (sock == INVALID_SOCKET) {
    return false;
  }

#ifdef _WIN32
  u_long nonblocking = blocking_mode ? 0 : 1;
  return ioctlsocket(sock, FIONBIO, &nonblocking) == NO_ERROR;
#else  // _WIN32
  int flags = fcntl(sock, F_GETFL, 0 /* ignored */);
  return fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1;
#endif // _WIN32
}

bool SetNoDelayMode(SOCKET sock, bool no_delay) {
#ifdef _WIN32
  int one = no_delay ? 1 : 0;
  return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&one), sizeof(one)) !=
         SOCKET_ERROR;

#else // _WIN32
  // TODO(rushfan): set TCP_NODELAY
  return true;

#endif // _WIN32
}

bool WouldSocketBlock() {
#ifdef _WIN32
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else  // _WIN32
  return errno == EWOULDBLOCK;
#endif // _WIN32
}

std::string GetLastErrorText() {
#if defined ( _WIN32 )
  char* error_text{nullptr};
  ScopeExit on_exit([&error_text] {LocalFree(error_text);});
  const auto last_error = GetLastError();
  FormatMessage(
    FORMAT_MESSAGE_ALLOCATE_BUFFER |
    FORMAT_MESSAGE_FROM_SYSTEM |
    FORMAT_MESSAGE_IGNORE_INSERTS,
    nullptr,
    last_error,
    MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), // Default language
    LPTSTR(&error_text),
    0,
    nullptr
  );
  return fmt::format("last_error: {} {}", last_error, error_text);
#else
  return fmt::format("errno: {} {}", errno, strerror(errno));
#endif
}

} // namespace

SocketConnection::SocketConnection(SOCKET sock)
  : SocketConnection(sock, ExitMode::CLOSE_SOCKET) {
}

SocketConnection::SocketConnection(SOCKET sock, ExitMode exit_mode)
  : sock_(sock), open_(true), exit_mode_(exit_mode) {
  static auto initialized = InitializeSockets();
  if (!initialized) {
    throw socket_error("Unable to initialize sockets.");
  }

  if (!SetBlockingMode(sock_, false)) {
    LOG(ERROR) << "SocketConnection: Unable to put socket into nonblocking mode.";
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
    throw socket_error("SocketConnection: Unable to set nonblocking mode on the socket.");
  }
  if (!SetNoDelayMode(sock_, true)) {
    LOG(ERROR) << "SocketConnection: Unable to put socket into nodelay mode.";
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
    throw socket_error("SocketConnection: Unable to set nodelay mode on the socket.");
  }
}

std::unique_ptr<SocketConnection> Connect(const std::string& host, int port) {
  static auto initialized = InitializeSockets();
  if (!initialized) {
    throw socket_error("SocketConnection::Connect Unable to initialize sockets.");
  }

  struct addrinfo hints{};
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype = SOCK_STREAM;
  hints.ai_protocol = IPPROTO_IP;

  const auto port_string = std::to_string(port);
  struct addrinfo* address = nullptr;
  const auto result_addrinfo = getaddrinfo(host.c_str(), port_string.c_str(), &hints, &address);
  if (result_addrinfo != 0) {
    LOG(ERROR) << "ERROR calling getaddrinfo: " << result_addrinfo;
    // TODO(rushfan): Throw connection error here?
  }
  for (auto* res = address; res != nullptr; res = res->ai_next) {
    auto s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
      continue;
    }
    const auto result = connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen));
    if (result == SOCKET_ERROR) {
      closesocket(s);
    } else {
      // success;
      freeaddrinfo(address);
      try {
        return std::make_unique<SocketConnection>(s);
      } catch (const socket_error&) {
      }
    }
  }
  throw connection_error(host, port);
}

/*
 ** Leaving here in case I need it again
// From: http://stackoverflow.com/questions/2493136/how-can-i-obtain-the-ipv4-address-of-the-client
// get sockaddr, IPv4 or IPv6:
static void *get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}
*/

SocketConnection::~SocketConnection() {

  if (exit_mode_ == ExitMode::LEAVE_SOCKET_OPEN || sock_ == INVALID_SOCKET) {
    // Bail out of here, either we ignore the socket or it is invalid.
    return;
  }

  if (exit_mode_ == ExitMode::RESET_TO_BLOCKING && sock_ != INVALID_SOCKET) {
    SetBlockingMode(sock_, true);
    SetNoDelayMode(sock_, false);
  } else if (exit_mode_ == ExitMode::CLOSE_SOCKET && sock_ != INVALID_SOCKET) {
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
  }
}

template <typename TYPE, std::size_t SIZE = sizeof(TYPE)>
static int read_TYPE(const SOCKET sock, TYPE* data, const duration<double> d, bool throw_on_timeout,
                     std::size_t size = SIZE) {
  const auto end = system_clock::now() + d;
  auto* p = reinterpret_cast<char*>(data);
  auto total_read = 0;
  auto remaining = static_cast<int>(size);
  bool first = true;
  while (true) {
    if (!first && system_clock::now() > end) {
      if (throw_on_timeout) {
        throw timeout_error("timeout error reading from socket.");
      }
      return total_read;
    }
    first = false;
    const auto result = recv(sock, p, remaining, 0);
    if (result == SOCKET_ERROR) {
      const auto saved_errno = errno;
      const auto saved_errno_text = GetLastErrorText();
      if (WouldSocketBlock()) {
        sleep_for(SLEEP_MS);
        continue;
      }
      if (saved_errno != ECONNRESET) {
        // This happens normally as the other side disconnects.
        LOG(ERROR) << "Got Socket Error on recv: " << saved_errno_text;
      }
      return total_read;
    }
    if (result <= 0) {
      return total_read;
    }
    total_read += result;
    if (total_read < static_cast<int>(size)) {
      p += result;
      remaining -= result;
      continue;
    }
    return total_read;
  }
}

int SocketConnection::receive(void* data, const int size, duration<double> d) {
  const auto num_read = read_TYPE<void, 0>(sock_, data, d, true, size);
  if (open_ && num_read == 0) {
    throw socket_closed_error(fmt::sprintf("receive: got zero read from socket. expected: ", size));
  }
  return num_read;
}

int SocketConnection::receive_upto(void* data, const int size, duration<double> d) {
  return read_TYPE<void, 0>(sock_, data, d, false, size);
}

std::string SocketConnection::receive(int size, duration<double> d) {
  const auto data = std::make_unique<char[]>(size);
  const auto num_read = receive(data.get(), size, d);
  return std::string(data.get(), num_read);
}

std::string SocketConnection::receive_upto(int size, duration<double> d) {
  const auto data = std::make_unique<char[]>(size);
  const auto num_read = receive_upto(data.get(), size, d);
  return std::string(data.get(), num_read);
}

std::string SocketConnection::read_line(int max_size, duration<double> d) {
  std::string s;
  auto size = 0;
  try {
    while (true) {
      char data = 0;
      const auto num_read = read_TYPE<char>(sock_, &data, d, true);
      if (!open_) {
        throw socket_closed_error("read_line: socket not open");
      }
      if (num_read == 0) {
        break;
      }
      s.push_back(data);
      size++;
      if (data == '\n') {
        break;
      }
      if (size > max_size) {
        break;
      }
    }
  } catch (const timeout_error&) {
  }
  return s;
}

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif  // MSG_NOSIGNAL 

int SocketConnection::send(const void* data, int size, duration<double>) {
  const auto sent = ::send(sock_, static_cast<const char*>(data), size, MSG_NOSIGNAL);
  if (open_ && sent != size) {
    if (sent == -1) {
      throw socket_closed_error(
          StrCat("Socket Closed; errno: ", strerror(errno)));
    }
    LOG(ERROR) << "ERROR: send != packet size.  size: " << size << "; sent: " << sent;
  }
  return size;
}

int SocketConnection::send(const std::string& s, duration<double> d) {
  return send(s.data(), stl::size_int(s), d);
}

int SocketConnection::send_line(const std::string& s, duration<double> d) {
  return send(s + "\r\n", d);
}

uint16_t SocketConnection::read_uint16(duration<double> d) {
  uint16_t data = 0;
  const auto num_read = read_TYPE<uint16_t>(sock_, &data, d, true);
  if (open_ && num_read == 0) {
    throw socket_closed_error(
        StrCat("read_uint16: got zero read from socket. expected: ", sizeof(uint16_t)));
  }
  return ntohs(data);
}

uint8_t SocketConnection::read_uint8(duration<double> d) {
  uint8_t data = 0;
  const auto num_read = read_TYPE<uint8_t>(sock_, &data, d, true);
  if (open_ && num_read == 0) {
    throw socket_closed_error(
        StrCat("read_uint8: got zero read from socket. expected: ", sizeof(uint8_t)));
  }
  return data;
}

bool SocketConnection::close() {
  if (open_) {
    open_ = false;
    closesocket(sock_);
  }
  return true;
}

} // namespace wwiv
