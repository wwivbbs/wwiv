/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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

#include <stdexcept>
#include <chrono>
#include <cstring>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <WinSock2.h>
#include <WS2tcpip.h>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#else  // _WIN32
#include <netdb.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

#endif  // _WIN32

#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/socket_exceptions.h"

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::system_clock;
using std::endl;
using std::string;
using std::unique_ptr;
using wwiv::os::sleep_for;
using namespace wwiv::strings;

namespace wwiv {
namespace core {

namespace {

static const auto SLEEP_MS = milliseconds(100);

static bool SetNonBlockingMode(SOCKET sock) {
  if (sock == INVALID_SOCKET) {
    return false;
  }

#ifdef _WIN32
  u_long nonblocking = 1;
  return ioctlsocket(sock, FIONBIO, &nonblocking) == NO_ERROR;
#else  // _WIN32
  int flags = fcntl(sock, F_GETFL, 0 /* ignored */);
  return fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1;
#endif  // _WIN32
}

static bool SetNoDelayMode(SOCKET sock) {
#ifdef _WIN32
      int one = 1;
      return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, (char*) &one, sizeof(one)) != SOCKET_ERROR;

#else  // _WIN32
  // TODO(rushfan): set TCP_NODELAY
  return true;

#endif  // _WIN32
}

static bool WouldSocketBlock() {
#ifdef _WIN32
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else  // _WIN32
  return errno == EWOULDBLOCK;
#endif  // _WIN32
}

}  // namespace

SocketConnection::SocketConnection(SOCKET sock) : SocketConnection(sock, true) {}

SocketConnection::SocketConnection(SOCKET sock, bool close_socket)
  : sock_(sock), open_(true), close_socket_(close_socket) {
  static bool initialized = InitializeSockets();
  if (!initialized) {
    throw socket_error("Unable to initialize sockets.");
  }

  if (!SetNonBlockingMode(sock_)) {
    LOG(ERROR) << "Unable to put socket into nonblocking mode.";
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
    throw socket_error("Unable to set nonblocking mode on the socket.");
  }
  if (!SetNoDelayMode(sock_)) {
    LOG(ERROR) << "Unable to put socket into nodelay mode.";
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
    throw socket_error("Unable to set nodelay mode on the socket.");
  }
}

unique_ptr<SocketConnection> Connect(const string& host, int port) {
  static bool initialized = InitializeSockets();
  if (!initialized) {
    throw socket_error("Unable to initialize sockets.");
  }

  struct addrinfo hints;
  memset(&hints, 0, sizeof(addrinfo));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype =  SOCK_STREAM;
  hints.ai_protocol = IPPROTO_IP;
  
  const string port_string = std::to_string(port);
  struct addrinfo* address = nullptr;
  int result_addrinfo = getaddrinfo(host.c_str(), port_string.c_str(), &hints, &address);
  if (result_addrinfo != 0) {
    LOG(ERROR) << "ERROR calling getaddrinfo: " << result_addrinfo;
    // TODO(rushfan): Throw connection error here?
  }
  for (struct addrinfo* res = address; res != nullptr; res = res->ai_next) {
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
      continue;
    }
    auto result = connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen));
    if (result == SOCKET_ERROR) {
      closesocket(s);
    } else {
      // success;
      freeaddrinfo(address);
      try {
        return std::make_unique<SocketConnection>(s);
      }
      catch (const socket_error&) {
      }
    }
  }
  throw connection_error(host, port);
}

/*
 ** Leaving here inc ase I need it again
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

  if (close_socket_ &&  sock_ != INVALID_SOCKET) {
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
  }
}

template<typename TYPE, std::size_t SIZE = sizeof(TYPE)>
static int read_TYPE(const SOCKET sock, TYPE* data, const duration<double> d, bool throw_on_timeout, std::size_t size = SIZE) {
  auto end = system_clock::now() + d;
  char *p = reinterpret_cast<char*>(data);
  std::size_t total_read = 0;
  int remaining = size;
  while (true) {
    if (system_clock::now() > end) {
      if (throw_on_timeout) {
        throw timeout_error("timeout error reading from socket.");
      }
      else {
        return total_read;
      }
    }
    int result = ::recv(sock, p, remaining, 0);
    if (result == SOCKET_ERROR) {
      if (WouldSocketBlock()) {
        sleep_for(SLEEP_MS);
        continue;
      }
    }
    if (result <= 0 && total_read == 0) {
      VLOG(3) << "result == 0 && total_read == 0";
      sleep_for(SLEEP_MS);
      continue;
    }
    if (result <= 0 && total_read > 0) {
      return total_read;
    }
    total_read += result;
    if (total_read < size) {
      p += result;
      remaining -= result;
      continue;
    }
    return total_read;
  }
  throw socket_error("unknown error reading from socket.");
}

int SocketConnection::receive(void* data, const int size, duration<double> d) {
  int num_read = read_TYPE<void, 0>(sock_, data, d, true, size);
  if (open_ && num_read == 0) {
    throw socket_closed_error(StringPrintf("receive: got zero read from socket. expected: ", size));
  }
  return num_read;
}

int SocketConnection::receive_upto(void* data, const int size, duration<double> d) {
  int num_read = read_TYPE<void, 0>(sock_, data, d, false, size);
  if (open_ && num_read == 0) {
    throw socket_closed_error(StringPrintf("receive_upto: got zero read from socket. expected: ", size));
  }
  return num_read;
}

string SocketConnection::receive(int size, duration<double> d) {
  std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
  int num_read = receive(data.get(), size, d);
  return string(data.get(), num_read);
}

string SocketConnection::receive_upto(int size, duration<double> d) {
  std::unique_ptr<char[]> data = std::make_unique<char[]>(size);
  int num_read = receive_upto(data.get(), size, d);
  return string(data.get(), num_read);
}

std::string SocketConnection::read_line(int max_size, std::chrono::duration<double> d) {
  string s;
  try {
    while (true) {
      char data = 0;
      int num_read = read_TYPE<char>(sock_, &data, d, true);
      if (!open_) {
        throw socket_closed_error("read_line: socket not open");
      }
      if (num_read == 0) {
        break;
      }
      s.push_back(data);
      if (data == '\n') {
        break;
      }
    }
  }
  catch (const timeout_error&) {
  }
  return s;
}

int SocketConnection::send(const void* data, int size, duration<double>) {
  int sent = ::send(sock_, reinterpret_cast<const char*>(data), size, 0);
  if (open_ && sent != size) {
    LOG(ERROR) << "ERROR: send != packet size.  size: " << size << "; sent: " << sent;
  }
  return size;
}

int SocketConnection::send(const std::string& s, std::chrono::duration<double> d) {
  return send(s.data(), s.size(), d);
}

int SocketConnection::send_line(const std::string& s, std::chrono::duration<double> d) {
  return send(s + "\r\n", d);
}

uint16_t SocketConnection::read_uint16(duration<double> d) {
  uint16_t data = 0;
  int num_read = read_TYPE<uint16_t>(sock_, &data, d, true);
  if (open_ && num_read == 0) {
    throw socket_closed_error(StrCat("read_uint16: got zero read from socket. expected: ", sizeof(uint16_t)));
  }
  return ntohs(data);
}

uint8_t SocketConnection::read_uint8(duration<double> d) {
  uint8_t data = 0;
  int num_read = read_TYPE<uint8_t>(sock_, &data, d, true);
  if (open_ && num_read == 0) {
    throw socket_closed_error(StrCat("read_uint8: got zero read from socket. expected: ", sizeof(uint8_t)));
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


}  // namespace net
} // namespace wwiv
