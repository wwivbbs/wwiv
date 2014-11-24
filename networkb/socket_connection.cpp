#include "networkb/socket_connection.h"

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
#include <sys/socket.h>

#define NO_ERROR 0
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(x) close(x)
#endif  // _WIN32

#include "core/strings.h"
#include "networkb/socket_exceptions.h"

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::chrono::time_point;
using std::chrono::duration;
using std::chrono::duration_cast;
using std::chrono::system_clock;
using std::string;
using std::this_thread::sleep_for;
using wwiv::strings::StringPrintf;

namespace wwiv {
namespace net {

namespace {

static const auto SLEEP_MS = milliseconds(100);;

bool InitializeSockets() {
#ifdef _WIN32
WSADATA wsadata;
int result = WSAStartup(MAKEWORD(2,2), &wsadata);
  if (result != 0) {
    std::clog << "WSAStartup failed with error: " << result << std::endl;
    return false;
  }
#endif  // _WIN32
  return true;
}

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

static bool WouldSocketBlock() {
#ifdef _WIN32
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else  // _WIN32
  return errno == EWOULDBLOCK;
#endif  // _WIN32
}

}  // namespace

SocketConnection::SocketConnection(const string& host, int port)
    : host_(host), port_(port) {
  static bool initialized = InitializeSockets();
  if (!initialized) {
    throw socket_error("Unable to initialize sockets.");
  }

  struct addrinfo hints;
  memset(&hints, 0, sizeof(addrinfo));
  hints.ai_family = PF_UNSPEC;
  hints.ai_socktype =  SOCK_STREAM;
  hints.ai_protocol = IPPROTO_IP;
  
  const string port_string = StringPrintf("%d", port);
  struct addrinfo* address = nullptr;
  int result = getaddrinfo(host.c_str(), port_string.c_str(), &hints, &address);
  for (struct addrinfo* res = address; res != nullptr; res = res->ai_next) {
    SOCKET s = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (s == INVALID_SOCKET) {
      continue;
    }
    int result = connect(s, res->ai_addr, static_cast<int>(res->ai_addrlen));
    if (result == SOCKET_ERROR) {
      closesocket(s);
      s = INVALID_SOCKET;
      continue;
    } else {
      // success;
      freeaddrinfo(address);
      if (!SetNonBlockingMode(s)) {
        std::clog << "Unable to put socket into nonblocking mode." << std::endl;
        closesocket(s);
        s = INVALID_SOCKET;
        continue;
      }
      sock_ = s;
      return;
    }
  }
}

SocketConnection::~SocketConnection() {
  if (sock_ != INVALID_SOCKET) {
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
  }
}

template<typename TYPE, std::size_t SIZE = sizeof(TYPE)>
static int read_TYPE(const SOCKET sock, TYPE* data, const milliseconds d, std::size_t size = SIZE) {
  auto end = system_clock::now() + d;
  while (true) {
    if (system_clock::now() > end) {
      throw timeout_error("timeout error reading from socket.");
    }
    int result = ::recv(sock, reinterpret_cast<char*>(data), size, 0);
    if (result == SOCKET_ERROR) {
      if (WouldSocketBlock()) {
        std::this_thread::sleep_for(SLEEP_MS);
        continue;
      }
    }
    if (result != size) {
      throw socket_error(StringPrintf("size error reading from socket. was %d expected %u", result, size));
    }
    return result;
  }
  throw socket_error("unknown error reading from socket");
}

int SocketConnection::receive(void* data, const int size, milliseconds d) {
  return read_TYPE<void, 0>(sock_, data, d, size);
}

int SocketConnection::send(const void* data, int size, milliseconds d) {
  return ::send(sock_, reinterpret_cast<const char*>(data), size, 0);
}

uint16_t SocketConnection::read_uint16(milliseconds d) {
  uint16_t data = 0;
  read_TYPE<uint16_t>(sock_, &data, d);
  return ntohs(data);
}

bool SocketConnection::send_uint16(uint16_t data, std::chrono::milliseconds d) {
  uint16_t netdata = htons(data);
  return send(&netdata, sizeof(uint16_t), d) == sizeof(uint16_t);
}

uint8_t SocketConnection::read_uint8(milliseconds d) {
  uint8_t data = 0;
  read_TYPE<uint8_t>(sock_, &data, d);
  return data;
}

bool SocketConnection::send_uint8(uint8_t data, std::chrono::milliseconds d) {
  return send(&data, sizeof(uint8_t), d) == sizeof(uint8_t);
}

}  // namespace net
} // namespace wwiv
