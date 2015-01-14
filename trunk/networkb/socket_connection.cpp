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
#include <netinet/in.h>
#include <sys/socket.h>

#define NO_ERROR 0
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(x) ::close(x)
#endif  // _WIN32

#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "networkb/socket_exceptions.h"

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
    LOG << "WSAStartup failed with error: " << result;
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

SocketConnection::SocketConnection(SOCKET sock, const string& host, int port)
  : sock_(sock), host_(host), port_(port), open_(true) {}

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
        LOG << "Unable to put socket into nonblocking mode.";
        closesocket(s);
        s = INVALID_SOCKET;
        continue;
      }
      if (!SetNoDelayMode(s)) {
        LOG << "Unable to put socket into nodelay mode.";
        closesocket(s);
        s = INVALID_SOCKET;
        continue;
      }
      return unique_ptr<SocketConnection>(new SocketConnection(s, host, port));
    }
  }
  throw connection_error(host, port);
}

unique_ptr<SocketConnection> Accept(int port) {
  static bool initialized = InitializeSockets();
  if (!initialized) {
    throw socket_error("Unable to initialize sockets.");
  }

  SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);

  sockaddr_in saddr;
  saddr.sin_addr.s_addr = INADDR_ANY;
  saddr.sin_family = PF_INET;
  saddr.sin_port = htons(port);
  int ret = bind(sock, reinterpret_cast<const struct sockaddr *>(&saddr), sizeof(sockaddr_in));
  if (ret == SOCKET_ERROR) {
    throw socket_error("Unable to bind to socket.");
  }
  ret = listen(sock, 1);
  if (ret == SOCKET_ERROR) {
    throw socket_error("Unable to listen to socket.");
  }
 
  socklen_t addr_length = sizeof(sockaddr_in);
  SOCKET s = accept(sock, reinterpret_cast<struct sockaddr *>(&saddr), &addr_length);

  if (!SetNonBlockingMode(s)) {
    LOG << "Unable to put socket into nonblocking mode.";
    closesocket(s);
    s = INVALID_SOCKET;
    throw socket_error("Unable to set nonblocking mode on the socket.");
  }
  if (!SetNoDelayMode(s)) {
    LOG << "Unable to put socket into nodelay mode.";
    closesocket(s);
    s = INVALID_SOCKET;
    throw socket_error("Unable to set nodelay mode on the socket.");
  }
  return unique_ptr<SocketConnection>(new SocketConnection(s, "", port));
}

SocketConnection::~SocketConnection() {
  LOG << "       SocketConnection::~SocketConnection()";
  if (sock_ != INVALID_SOCKET) {
    closesocket(sock_);
    sock_ = INVALID_SOCKET;
  }
}

template<typename TYPE, std::size_t SIZE = sizeof(TYPE)>
static int read_TYPE(const SOCKET sock, TYPE* data, const milliseconds d, std::size_t size = SIZE) {
  auto end = system_clock::now() + d;
  char *p = reinterpret_cast<char*>(data);
  std::size_t total_read = 0;
  int remaining = size;
  while (true) {
    if (system_clock::now() > end) {
      throw timeout_error("timeout error reading from socket.");
    }
    int result = ::recv(sock, p, remaining, 0);
    if (result == SOCKET_ERROR) {
      if (WouldSocketBlock()) {
        sleep_for(SLEEP_MS);
        continue;
      }
    }
    if (result == 0) {
      return 0;
    }
    total_read += result;
    if (total_read < size) {
      p += result;
      remaining -= result;
      continue;
    }
    return result;
  }
  throw socket_error("unknown error reading from socket");
}

int SocketConnection::receive(void* data, const int size, milliseconds d) {
  int num_read = read_TYPE<void, 0>(sock_, data, d, size);
  if (open_ && num_read == 0) {
    throw socket_error(StringPrintf("size error reading from socket. was %d expected %u", 0, size));
  }
  return num_read;
}

int SocketConnection::send(const void* data, int size, milliseconds d) {
  int sent = ::send(sock_, reinterpret_cast<const char*>(data), size, 0);
  if (open_ && sent != size) {
    LOG << "ERROR: send != packet size.  size: " << size << "; sent: " << sent;
  }
  return size;
}

uint16_t SocketConnection::read_uint16(milliseconds d) {
  uint16_t data = 0;
  int num_read = read_TYPE<uint16_t>(sock_, &data, d);
  if (open_ && num_read == 0) {
    throw socket_error(StringPrintf("size error reading from socket. was %d expected %u", 0, sizeof(uint16_t)));
  }
  return ntohs(data);
}

uint8_t SocketConnection::read_uint8(milliseconds d) {
  uint8_t data = 0;
  int num_read = read_TYPE<uint8_t>(sock_, &data, d);
  if (open_ && num_read == 0) {
    throw socket_error(StringPrintf("size error reading from socket. was %d expected %u", 0, sizeof(uint8_t)));
  }
  return data;
}

bool SocketConnection::close() {
  open_ = false;
  closesocket(sock_);
  return true;
}


}  // namespace net
} // namespace wwiv
