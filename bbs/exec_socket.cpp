/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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

#include "bbs/wwivfoss_inband.h"

#include "core/file.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/strings.h"
#include "fmt/format.h"
#include <cerrno>
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

ExecSocket::ExecSocket(const std::filesystem::path& dir, uint16_t port, exec_socket_type_t type)
    : dir_(dir), type_(type) {
  stop_.store(false);

  const int af = (type == exec_socket_type_t::unix_domain) ? AF_UNIX : AF_INET;
  server_socket_ = socket(af, SOCK_STREAM, 0);
  if (server_socket_ == -1) {
    LOG(ERROR) << "socket error";
    server_socket_ = INVALID_SOCKET;
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
      server_socket_ = INVALID_SOCKET;
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
      server_socket_ = INVALID_SOCKET;
      return;
    }
    if (setsockopt(server_socket_, SOL_SOCKET, SO_KEEPALIVE, reinterpret_cast<char*>(&optval),
                   sizeof(optval)) == -1) {
      LOG(ERROR) << "Unable to create socket [SO_KEEPALIVE]";
      server_socket_ = INVALID_SOCKET;
      return;
    }
    // Try to set nodelay.
    setsockopt(server_socket_, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&optval),
               sizeof(optval));

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(port);
    memset(&(my_addr.sin_zero), 0, 8);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_socket_, reinterpret_cast<sockaddr*>(&my_addr), sizeof(my_addr)) == -1) {
      // throw socket_error(msg);
      LOG(ERROR) << "bind error";
      server_socket_ = INVALID_SOCKET;
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

  if (listen(server_socket_, 1) == -1) {
    LOG(ERROR) << "listen error";
    server_socket_ = INVALID_SOCKET;
    return;
  }
}

ExecSocket::~ExecSocket() {
  if (client_socket_ != INVALID_SOCKET && client_socket_ > 0) {
    closesocket(client_socket_);
  }
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
    return fmt::format("{}", port_);
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
  // WNOWAIT leaves the child to be reaped by the caller, so it still gets
  // the door's real exit status.
  siginfo_t info{};
  if (waitid(P_PID, static_cast<id_t>(h), &info, WEXITED | WNOHANG | WNOWAIT) == -1) {
    // ECHILD: already reaped (or not our child), so it isn't running.
    VLOG(2) << "waitid failed; errno: " << errno;
    return errno == EINTR;
  }
  if (info.si_pid == 0) {
    return true;
  }
  VLOG(1) << "child exited; code: " << info.si_code << "; status: " << info.si_status;
  return false;
#endif
}

pump_socket_result_t ExecSocket::pump_socket(EXEC_SOCKET_HANDLE hProcess, SOCKET sock, wwiv::common::RemoteIO& io) {
  if (wwivfoss_) {
    return pump_socket_wwivfoss(hProcess, sock, io);
  }
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
  return pump_socket_result_t::process_exit;
}

void ExecSocket::enable_wwivfoss(const wwivfoss_options_t& options) { wwivfoss_ = options; }

static bool would_block() {
#ifdef _WIN32
  return WSAGetLastError() == WSAEWOULDBLOCK;
#else
  return errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR;
#endif
}

// One non-blocking send of as much of data as the socket will take. Returns
// bytes sent (0 if the socket is full), or -1 on error.
static int send_some(SOCKET sock, const std::string& data) {
#ifdef MSG_NOSIGNAL
  constexpr int send_flags = MSG_NOSIGNAL;
#else
  constexpr int send_flags = 0;
#endif
  const auto n = send(sock, data.data(), static_cast<int>(data.size()), send_flags);
  if (n >= 0) {
    return static_cast<int>(n);
  }
  return would_block() ? 0 : -1;
}

// Waits up to timeout for the process to exit. Meanwhile keeps sending
// to_door and throws away whatever the door sends, so it can't get stuck
// writing to us.
static bool wait_for_exit(EXEC_SOCKET_HANDLE h, SOCKET sock, std::string& to_door,
                          std::chrono::milliseconds timeout) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  char buf[1024];
  for (;;) {
    if (!ExecSocket::process_still_active(h)) {
      return true;
    }
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    while (recv(sock, buf, sizeof(buf), 0) > 0) {
    }
    if (!to_door.empty()) {
      if (const auto n = send_some(sock, to_door); n > 0) {
        to_door.erase(0, static_cast<size_t>(n));
      } else if (n < 0) {
        to_door.clear();
      }
    }
    wwiv::os::sleep_for(std::chrono::milliseconds(50));
  }
}

pump_socket_result_t ExecSocket::pump_socket_wwivfoss(EXEC_SOCKET_HANDLE hProcess, SOCKET sock,
                                                      wwiv::common::RemoteIO& io) {
  using clock = std::chrono::steady_clock;
  // Reads per side per pass, and how much we'll queue for the door before
  // we stop reading from the caller.
  static constexpr int max_reads = 16;
  static constexpr size_t max_queued = 16 * 1024;
  const auto& opts = wwivfoss_.value();

  // Raise DTR first: after a hangup WWIVFOSS keeps the UART in loopback
  // until DTR comes back up, and a resident copy can't otherwise tell a new
  // connection has started.
  auto to_door = wwivfoss_line_state(WWIVFOSS_LINE_UP);
  auto last_sent = clock::now();
  auto last_process_check = clock::now();

  WWIVFossDecoder decoder;
  // DTR low only counts once the door has sent something, so a stale FF 01
  // can't kill it while the emulator is still booting.
  bool door_spoke = false;
  // Once the door has dropped DTR: when it's due to exit.
  std::optional<clock::time_point> door_hangup_deadline;
  char buf[1024];

  auto door_gone = [&](const char* why) {
    VLOG(1) << "Exiting pump_socket: door socket " << why;
    return wait_for_exit(hProcess, sock, to_door, opts.socket_closed_grace)
               ? pump_socket_result_t::process_exit
               : pump_socket_result_t::socket_error;
  };

  while (!stop_.load()) {
    bool busy = false;

    for (int i = 0; i < max_reads; i++) {
      const auto num_read = recv(sock, buf, sizeof(buf), 0);
      if (num_read < 0 && would_block()) {
        break;
      }
      if (num_read <= 0) {
        return door_gone(num_read == 0 ? "closed." : "failed.");
      }
      busy = true;
      if (const auto data = decoder.decode(buf, static_cast<size_t>(num_read)); !data.empty()) {
        door_spoke = true;
        io.write(data.data(), static_cast<int>(data.size()));
      }
      for (const auto state : decoder.take_line_states()) {
        // A drop is final even if DTR comes back up: doors pulse DTR to hang
        // up, then wait for a carrier loss that never comes.
        if (state & WWIVFOSS_LINE_DTR) {
          door_spoke = true;
        } else if (!door_spoke) {
          VLOG(1) << "Ignoring DTR low from a door that hasn't sent anything yet.";
        } else if (!door_hangup_deadline) {
          VLOG(1) << "Door lowered DTR; returning caller to the BBS.";
          door_hangup_deadline = clock::now() + opts.door_hangup_grace;
        }
      }
    }

    if (!io.connected()) {
      VLOG(1) << "Exiting pump_socket: Caller Hung up; lowering DTR to the door.";
      to_door += wwivfoss_line_state(WWIVFOSS_LINE_RTS);
      return wait_for_exit(hProcess, sock, to_door, opts.caller_hangup_grace)
                 ? pump_socket_result_t::process_exit
                 : pump_socket_result_t::socket_error;
    }

    for (int i = 0; i < max_reads && to_door.size() < max_queued && io.incoming(); i++) {
      const auto num_read = io.read(buf, sizeof(buf));
      if (num_read == 0) {
        break;
      }
      busy = true;
      // Keys typed while the door has DTR low are dropped so they don't run
      // as BBS commands once the caller is back.
      if (!door_hangup_deadline) {
        to_door += wwivfoss_escape(buf, num_read);
      }
    }

    // Any bytes reset WWIVFOSS's -H timer, so only send FF 03 when idle.
    if (opts.heartbeat.count() > 0 && to_door.empty() &&
        clock::now() - last_sent >= opts.heartbeat) {
      to_door = wwivfoss_line_state(WWIVFOSS_LINE_UP);
    }

    if (!to_door.empty()) {
      const auto n = send_some(sock, to_door);
      if (n < 0) {
        return door_gone("write failed.");
      }
      if (n > 0) {
        to_door.erase(0, static_cast<size_t>(n));
        last_sent = clock::now();
        busy = true;
      }
    }

    if (door_hangup_deadline || clock::now() - last_process_check >= std::chrono::seconds(1)) {
      last_process_check = clock::now();
      if (!process_still_active(hProcess)) {
        return pump_socket_result_t::process_exit;
      }
    }
    if (door_hangup_deadline && clock::now() >= door_hangup_deadline.value()) {
      LOG(INFO) << "Door lowered DTR but did not exit; terminating it.";
      return pump_socket_result_t::socket_error;
    }
    if (!busy) {
      wwiv::os::sleep_for(std::chrono::milliseconds(to_door.empty() ? 100 : 10));
    }
  }
  return pump_socket_result_t::process_exit;
}

bool ExecSocket::stop_pump() { 
  stop_.store(true); 
  return true;
}

} // namespace wwiv::bbs
