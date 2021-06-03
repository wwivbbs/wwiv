/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                   Copyright (C)2021, WWIV Software Services            */
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
#include "bbs/exec_socket.h"

#include "core/file.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/format.h"
#include <chrono>
#include <string>

#ifdef _WIN32
#include <winsock2.h>
#include <WS2tcpip.h>
#include <afunix.h>
#include <MSWSock.h>
#elif defined(__unix__)
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/un.h>
#include <sys/wait.h>
#endif

namespace wwiv::bbs {

using namespace wwiv::core;
using namespace wwiv::strings;

static bool SetBlockingMode(SOCKET sock, bool blocking_mode) {
  if (sock == INVALID_SOCKET) {
    return false;
  }

#ifdef _WIN32
  u_long nonblocking = blocking_mode ? 0 : 1;
  return ioctlsocket(sock, FIONBIO, &nonblocking) == NO_ERROR;
#else // _WIN32
  int flags = fcntl(sock, F_GETFL, 0 /* ignored */);
  return fcntl(sock, F_SETFL, flags | O_NONBLOCK) != -1;
#endif // _WIN32
}

ExecSocket::ExecSocket(const std::filesystem::path& dir, exec_socket_type_t type)
    : dir_(dir), type_(type) {
  stop_.store(false);

  const int af = (type == exec_socket_type_t::unix_domain) ? AF_UNIX : AF_INET;
  server_socket_ = socket(af, SOCK_STREAM, 0);
  if (server_socket_ == -1) {
    LOG(ERROR) << "socket error";
    server_socket_ = -1;
    return;
  }

  if (type == exec_socket_type_t::unix_domain) {
    struct sockaddr_un addr {};
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    auto p = path();
    File::Remove(p);
    to_char_array(addr.sun_path, p.string());
    if (bind(server_socket_, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
      LOG(ERROR) << "bind error";
      server_socket_ = -1;
      return;
    }

  } else {
    // port
    struct sockaddr_in my_addr {};
    memset(&my_addr, 0, sizeof(sockaddr_in));

    int optval = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<char*>(&optval),
                   sizeof(optval)) == -1) {
      LOG(ERROR) << "Unable to create socket [SO_REUSEADDR]";
      server_socket_ = -1;
      return;
    }
    if (setsockopt(server_socket_, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&optval),
                   sizeof(optval)) == -1) {
      LOG(ERROR) << "Unable to create socket [SO_KEEPALIVE]";
      server_socket_ = -1;
      return;
    }
    // Try to set nodelay.
    setsockopt(server_socket_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&optval),
               sizeof(optval));

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(12345);
    memset(&(my_addr.sin_zero), 0, 8);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&my_addr), sizeof(my_addr)) == -1) {
      // throw socket_error(msg);
      LOG(ERROR) << "bind error";
      server_socket_ = -1;
      return;
    }

    struct sockaddr_in sin_port;
    socklen_t len = sizeof(sockaddr_in);
    if (getsockname(server_socket_, (struct sockaddr*)&sin_port, &len) == -1) {
      LOG(ERROR) << "getsockname error";
      return;
    }
    port_ = ntohs(sin_port.sin_port);
    VLOG(1) << "Bound to port: " << port_;
  }

  if (listen(server_socket_, 5) == -1) {
    LOG(ERROR) << "listen error";
    server_socket_ = -1;
    return;
  }
}

ExecSocket::~ExecSocket() {
  if (server_socket_ != -1) {
    closesocket(server_socket_);
  }
}


std::optional<int> ExecSocket::accept() {
  VLOG(1) << "ExecSocket::accept()";
  struct timeval tv;
  fd_set rfds;
  FD_ZERO(&rfds);
  FD_SET(server_socket_, &rfds);

  tv.tv_sec = 20; // 10 seconds
  tv.tv_usec = 0;
  
  if (const auto res = select(server_socket_ + 1, &rfds, nullptr, nullptr, &tv); res > 0) {
    if (client_socket_ = ::accept(server_socket_, nullptr, nullptr); client_socket_ > 0) {
      SetBlockingMode(client_socket_, false);
      VLOG(1) << "Accepted: ";
      return {client_socket_};
    }
  }
  LOG(ERROR) << "accept error";
  return std::nullopt;
}

[[nodiscard]] inline std::filesystem::path ExecSocket::path() const {
  return wwiv::core::FilePath(dir_, "wwivexec.sock");
}

std::string ExecSocket::z() const { 
  if (type_ == exec_socket_type_t::port) {
    return fmt::format("localhost:{}", port_);
  }
  return path().string();
}

// static 
bool ExecSocket::process_still_active(EXEC_SOCKET_HANDLE h) {
#ifdef _WIN32
  DWORD dwExitCode;
  if (GetExitCodeProcess(h, &dwExitCode) && dwExitCode != STILL_ACTIVE) {
    VLOG(1) << "Process exited. Ending pump_socket";
    return false;
  }
  return true;
#elif defined(__unix__)
  int status_code = 0;
  pid_t wp = waitpid(h, &status_code, WNOHANG);
  if (wp == -1 || wp > 0) {
    // -1 means error and >0 is the pid
    VLOG(2) << "waitpid returned: " << wp << "; errno: " << errno;
    if (WIFEXITED(status_code)) {
      VLOG(1) << "child exited with code: " << WEXITSTATUS(status_code);
      return false;
    }
    if (WIFSIGNALED(status_code)) {
      VLOG(1) << "child caught signal: " << WTERMSIG(status_code);
    } else {
      LOG(INFO) << "Raw status_code: " << status_code;
    }
    LOG(INFO) << "core dump? : " << WCOREDUMP(status_code);
  }
  return true;
#endif
}

pump_socket_result_t ExecSocket::pump_socket(EXEC_SOCKET_HANDLE hProcess, int sock, wwiv::common::RemoteIO& io) {
  static constexpr int check_process_every = 10;
  char buf[1024];
  int count = 0;
  while (!stop_.load()) {
    if (auto num_read = recv(sock, buf, sizeof(buf), 0); num_read > 0) {
      io.write(buf, num_read);
    } else if (num_read == 0) {
      VLOG(1) << "Exiting pump_socket: recv.";
      return pump_socket_result_t::socket_error;
    }

    if (!io.connected()) {
      VLOG(1) << "Exiting pump_socket: Caller Hung up.";
      return pump_socket_result_t::socket_error;
    }

    if (io.incoming()) {
      if (auto num_read = io.read(buf, sizeof(buf)); num_read > 0) {
        if (send(sock, buf, num_read, 0) == 0) {
          // TODO(rushfan): handle nonblocking error?
          VLOG(1) << "Exiting pump_socket; Write to socket failed";
          return pump_socket_result_t::socket_error;
        }
      }
    }

    if (++count >= check_process_every) {
      count = 0;
      if (!process_still_active(hProcess)) {
        return pump_socket_result_t::process_exit;
      }
    }
    wwiv::os::sleep_for(std::chrono::milliseconds(100));
  }
}

bool ExecSocket::stop_pump() { 
  stop_.store(true); 
  return true;
}

} // namespace wwiv::bbs
