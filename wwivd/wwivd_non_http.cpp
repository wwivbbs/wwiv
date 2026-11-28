/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2017-2022, WWIV Software Services             */
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
#include "wwivd/wwivd_non_http.h"

#include "core/file.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/semaphore_file.h"
#include "core/socket_connection.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/format.h"
#include "sdk/ansi/makeansi.h"
#include "sdk/config.h"
#include "wwivd/connection_data.h"
#include "wwivd/node_manager.h"
#include "wwivd/wwivd.h"
#include <cctype>
#include <filesystem>
#include <memory>
#include <string>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

#ifdef WWIV_USE_PIPES
#include "core/pipe.h"
#endif

namespace wwiv::wwivd {

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

std::string to_string(const wwivd_matrix_entry_t& e) {
  std::ostringstream ss;
  ss << "[" << e.key << "] " << e.name << " (" << e.description << ")";
  return ss.str();
}

std::string to_string(const std::vector<wwivd_matrix_entry_t>& elements) {
  std::ostringstream ss;
  for (const auto& e : elements) {
    ss << "{" << to_string(e) << "}" << std::endl;
  }
  return ss.str();
}

std::string CreateCommandLine(const std::string& tmpl, std::map<char, std::string> params) {
  std::string out;

  for (auto it = tmpl.begin(); it != tmpl.end(); ++it) {
    if (*it == '@') {
      ++it;
      if (it == tmpl.end()) {
        out.push_back('@');
        break;
      }
      try {
        out.append(params.at(*it));
      } catch (const std::out_of_range&) {
        out.push_back(*it);
      }
    } else {
      out.push_back(*it);
    }
  }

  return out;
}

std::filesystem::path node_file(const Config& config, ConnectionType ct, int node_number) {
  if (ct == ConnectionType::BINKP) {
    return FilePath(config.datadir(), "binkpinuse");
  }
  return FilePath(config.datadir(), StrCat("nodeinuse.", node_number));
}

// HACK: Copied out of SocketConnection class
// TODO(rushfan): Make this available in core::net
bool SetNoDelayMode([[maybe_unused]] SOCKET sock, [[maybe_unused]] bool no_delay) {
#ifdef _WIN32
  int one = no_delay ? 1 : 0;
  return setsockopt(sock, IPPROTO_TCP, TCP_NODELAY, reinterpret_cast<char*>(&one), sizeof(one)) !=
         SOCKET_ERROR;

#else // _WIN32
  // TODO(rushfan): set TCP_NODELAY
  return true;
#endif // _WIN32
}

/** Sends all of data, retrying on short writes. Returns false on error. */
static bool send_all(SOCKET sock, const char* data, int size) {
  while (size > 0) {
    const auto n = send(sock, data, size, 0);
    if (n <= 0) {
      return false;
    }
    data += n;
    size -= static_cast<int>(n);
  }
  return true;
}

/**
 * Proxies the caller on sock to host_port until either side closes.
 * sock is owned by the caller of this function and is not closed here.
 */
static bool telnet_to(const std::string& host_port, int /*node_number*/, SOCKET sock) { 
  LOG(INFO) << "telnet_to: " << host_port;
  auto idx = host_port.find(':');
  std::string host = host_port;
  int port = 23;
  if (idx != std::string::npos) {
    host = host_port.substr(0, idx);
    port = to_number<int>(host_port.substr(idx + 1));
  }
  auto out = Connect(host, port);
  if (!out || !out->is_open()) {
    LOG(ERROR) << "Unable to connect to: host: " << host << "; port: " << port;
    return false;
  }
  SetBlockingMode(out->socket());
  SetNoDelayMode(out->socket(), false);
  const auto max_fd = std::max<int>(sock, out->socket());
  struct timeval ts {};
  VLOG(4) << "telnet_to: outside loop";
  char data[1024];
  while (out->is_open()) {
    VLOG(4) << "telnet_to: loop";
    // If we had more than 2 here, should move this out of the loop.
    fd_set sock_set;
    FD_ZERO(&sock_set);
    FD_SET(sock, &sock_set);
    FD_SET(out->socket(), &sock_set);
    // Some OSes change this to be the time remaining per call, so reset it
    // each time. ick
    ts.tv_sec = 5;
    ts.tv_usec = 0;
    VLOG(3) << "  ** max_fd: " << max_fd << "; out: " << out->socket() << "; in: " << sock;
    auto rc = select(max_fd + 1, &sock_set, nullptr, nullptr, &ts);
    if (rc < 0) {
      LOG(ERROR) << "select failed:" << max_fd;
      break;
    } else if (rc == 0) {
      // loop.
      VLOG(3) << "telnet_to: select timed out";
      continue;
    } 

    // We got one!
    if (FD_ISSET(out->socket(), &sock_set)) {
      VLOG(4) << "FD_ISSET: out";
      if (auto num_read = recv(out->socket(), data, sizeof(data), 0); num_read > 0) {
        if (!send_all(sock, data, static_cast<int>(num_read))) {
          VLOG(1) << "telnet_to: write to in failed";
          break;
        }
      } else {
        LOG(INFO) << "Remote session closed; read returned " << num_read;
        break;
      }
    }

    if (FD_ISSET(sock, &sock_set)) {
      VLOG(4) << "FD_ISSET: in";
      if (auto num_read = recv(sock, data, sizeof(data), 0); num_read > 0) {
        if (!send_all(out->socket(), data, static_cast<int>(num_read))) {
          VLOG(1) << "telnet_to: write to out failed";
          break;
        }
      } else {
        LOG(INFO) << "Caller closed the connection; read returned " << num_read;
        break;
      }
    }
  }
  VLOG(1) << "TELNET Loop done;";
  return true;
}

#if defined(WWIV_USE_PIPES)
/**
 * Copies data between the caller's socket and the BBS's named pipes until
 * either side goes away. On return both pipes are closed, so the BBS sees
 * that the caller hung up (and wwivd sees when the BBS exits).
 */
static void socket_pipe_loop(SOCKET sock, Pipe& data_pipe, Pipe& control_pipe) {
  LOG(INFO) << "Starting socket_pipe_loop; sock: " << sock;
  auto close_pipes = finally([&] {
    data_pipe.Close();
    control_pipe.Close();
  });
  if (!data_pipe.Create()) {
    LOG(ERROR) << "Failed to create pipe: " << data_pipe.name();
    return;
  }
  if (!control_pipe.Create()) {
    LOG(ERROR) << "Failed to create pipe: " << control_pipe.name();
  }
  if (!data_pipe.WaitForClient(std::chrono::seconds(10))) {
    LOG(ERROR) << "Failed to connect bbs end of data pipe: " << data_pipe.name();
    return;
  }
  if (!control_pipe.WaitForClient(std::chrono::seconds(5))) {
    LOG(WARNING) << "Failed to connect bbs end of control pipe: " << control_pipe.name();
  }

  char data[1024];
  while (true) {
    if (!data_pipe.IsOpen()) {
      VLOG(1) << "Data pipe was closed by the BBS; exiting.";
      return;
    }

    // BBS -> caller: drain everything the BBS has written so far.
    while (data_pipe.peek()) {
      const auto o = data_pipe.read(data, sizeof(data));
      if (!o || o.value() <= 0) {
        break;
      }
      if (!send_all(sock, data, o.value())) {
        LOG(INFO) << "socket_pipe_loop: write to caller failed; exiting.";
        return;
      }
    }

    // Caller -> BBS: wait briefly for input from the caller. This short
    // timeout is also how often output from the BBS is picked up.
    fd_set sock_set;
    FD_ZERO(&sock_set);
    FD_SET(sock, &sock_set);
    struct timeval ts {};
    ts.tv_sec = 0;
    ts.tv_usec = 20000; // 20ms
    const auto rc = select(static_cast<int>(sock) + 1, &sock_set, nullptr, nullptr, &ts);
    if (rc < 0) {
      LOG(ERROR) << "select failed; socket: " << sock << "; errno: " << errno;
      return;
    }
    if (rc > 0 && FD_ISSET(sock, &sock_set)) {
      const auto num_read = recv(sock, data, sizeof(data), 0);
      if (num_read <= 0) {
        LOG(INFO) << "Caller closed the connection; read returned " << num_read;
        return;
      }
      if (!data_pipe.write(data, static_cast<int>(num_read))) {
        LOG(ERROR) << "Failed to write to pipe; exiting.";
        return;
      }
    }
  }
}
#endif

/**
 * Runs raw_cmd for node_number. Takes ownership of sock (which may be
 * empty, e.g. in pipe mode); it is closed once the child has its own copy,
 * or when the @telnet: proxy finishes.
 */
static bool launch_cmd(const wwivd_config_t& wc, const std::string& raw_cmd,
                       const std::string& working_dir, const std::shared_ptr<NodeManager>& nodes,
                       int node_number, WatchedSocket sock, ConnectionType connection_type,
                       const std::string& remote_peer, Sessions* sessions) {

  const auto wwiv_pid = fmt::format("[{}] ", get_pid());
  nodes->set_node(node_number, connection_type, StrCat("Connected: ", remote_peer));

  const std::map<char, std::string> params = {
    {'N', std::to_string(node_number)}, 
    {'H', std::to_string(sock.get())},
    {'P', std::to_string(get_pid())},
};

  if (sock) {
    // Reset the socket back to blocking mode
    VLOG(2) << "Setting blocking mode.";
    if (!SetBlockingMode(sock.get())) {
      LOG(ERROR) << "Failed to reset the socket to blocking mode.";
    }
  }

  VLOG(2) << "raw_cmd: " << raw_cmd;
  auto at_exit = finally([=] { nodes->ReleaseNode(node_number); });
  if (starts_with(raw_cmd, "@telnet:")) {
    return telnet_to(raw_cmd.substr(8), node_number, sock.get());
  }
  const auto cmd = CreateCommandLine(raw_cmd, params);
  // From here the child has the socket, and shutdown hangs it up instead.
  return ExecCommandAndWait(wc, *nodes, cmd, wwiv_pid, node_number, sock.release(), working_dir,
                            sessions);
}

/** Launches bbs on node_number for the caller on sock, taking ownership of sock. */
static bool launch_node(const Config& config, const wwivd_config_t& wc,
                        const wwivd_matrix_entry_t& bbs, const std::shared_ptr<NodeManager>& nodes,
                        int node_number, WatchedSocket sock, ConnectionType connection_type,
                        const std::string& remote_peer, Sessions* sessions) {
  const auto& raw_cmd = connection_type == ConnectionType::SSH ? bbs.ssh_cmd : bbs.telnet_cmd;
  const auto root = config.root_directory();
  const auto working_dir =
      bbs.working_directory.empty() ? "" : FilePath(root, bbs.working_directory).string();

  const auto wwiv_pid = fmt::format("[{}] ", get_pid());
  VLOG(1) << wwiv_pid << ": launching node(" << node_number << ")";
  const auto sem_text = fmt::format("Created by pid: {}\nremote peer: {}", wwiv_pid, remote_peer);
  const auto sem_path = node_file(config, connection_type, node_number);

  try {
    const auto semaphore_file = SemaphoreFile::try_acquire(sem_path, sem_text, std::chrono::seconds(60));
    if (bbs.data_mode != wwivd_data_mode_t::pipe) {
      // The child gets the caller's socket directly.
      return launch_cmd(wc, raw_cmd, working_dir, nodes, node_number, std::move(sock),
                        connection_type, remote_peer, sessions);
    }

    // Pipe mode: wwivd keeps the socket and relays it over named pipes; the
    // child gets no socket.
#ifdef WWIV_USE_PIPES
    Pipe data_pipe(node_number, false);
    Pipe control_pipe(node_number, true);
#endif
#if defined(_WIN32) && defined(WWIV_USE_PIPES)
    VLOG(1) << "Spawning socket_pipe_loop. sock: " << sock.get();
    // Declared after the pipes so that it is joined before they are destroyed.
    std::jthread pipes_thread(socket_pipe_loop, sock.get(), std::ref(data_pipe),
                              std::ref(control_pipe));
#endif
    // wwivd keeps reading the socket here, so shutdown hangs the caller up
    // by shutting it down, which ends the relay and closes the pipes.
    auto result = launch_cmd(wc, raw_cmd, working_dir, nodes, node_number, WatchedSocket{},
                             connection_type, remote_peer, sessions);
    VLOG(1) << "after launch_cmd";
#if defined(WWIV_USE_PIPES)
#if defined(__OS2__)
    socket_pipe_loop(sock.get(), data_pipe, control_pipe);
#elif defined(_WIN32)
    // The BBS has exited, so its end of the pipe is closed and the loop ends.
    pipes_thread.join();
#endif
#endif
    // sock is closed when it goes out of scope.
    return result;
  } catch (const semaphore_not_acquired& e) {
    LOG(ERROR) << wwiv_pid << "Unable to create semaphore file: " << sem_path << "; errno: " << errno
               << "; what: " << e.what();
    return false;
  }
}

static ConnectionType connection_type_for(const wwivd_config_t& c, int port) {
  if (port == c.telnet_port) {
    return ConnectionType::TELNET;
  }
  if (port == c.binkp_port) {
    return ConnectionType::BINKP;
  }
  if (port == c.ssh_port) {
    return ConnectionType::SSH;
  }
  if (port == c.http_port) {
    return ConnectionType::HTTP;
  }
  LOG(WARNING) << "Defaulting to TELNET for unknown connection type for port: " << port;
  return ConnectionType::TELNET;
}

static bool check_ansi(SocketConnection& conn) {
  const auto d = 3s;
  conn.send("Checking for ANSI Graphics... ", d);
  conn.send("\x1b[6n", d);
  const auto res = conn.receive_upto(10, d);
  if (!res.empty() && res.front() == 27) {
    conn.send_line("ANSI detected.", d);
    return true;
  }
  conn.send_line("No ANSI detected.", d);
  return false;
}

std::string Color(int c, bool ansi, int& current) {
  if (!ansi) {
    return "";
  }
  auto s = ansi::makeansi(c, current);
  current = c;
  return s;
}

ConnectionHandler::ConnectionHandler(ConnectionData d, accepted_socket_t a)
    : data(std::move(d)), port_(a.port) {
  SocketHandle sock(a.client_socket);
  sock_ = data.sessions ? data.sessions->watch(std::move(sock))
                        : WatchedSocket(nullptr, std::move(sock));
}

// ReSharper disable once CppMemberFunctionMayBeConst
std::optional<wwivd_matrix_entry_t> ConnectionHandler::DoMatrixLogon(const wwivd_config_t& c) {

  SocketConnection conn(sock_.get(), SocketConnection::ExitMode::LEAVE_SOCKET_OPEN);
  if (c.bbses.empty()) {
    // This should be checked before calling this method.
    throw std::runtime_error("c.bbses.empty()");
  }
  if (c.bbses.size() == 1) {
    // Only have 1 entry, that's the one to display.
    return c.bbses.front();
  }

  const auto ansi = check_ansi(conn);
  // This caller's current color, starting from the terminal's default.
  auto curatr = 7;
  const auto color = [&](int c) { return Color(c, ansi, curatr); };
  const auto d = 1s;
  for (auto tries = 0; tries < 3; tries++) {
    conn.send_line(StrCat(color(10), "Matrix Logon Menu"), d);
    conn.send_line("\r\n", d);
    for (const auto& b : c.bbses) {
      // Skip ones that require ANSI.
      if (b.require_ansi && !ansi) {
        continue;
      }
      std::ostringstream ss;
      ss << color(14) << b.key << color(3) << ") " << color(10) << b.name;
      if (!b.description.empty()) {
        ss << color(11) << "  (" << b.description << ")";
      }
      conn.send_line(ss.str(), d);
    }
    std::ostringstream ss;
    ss << color(14) << '!' << color(3) << ") " << color(11) << " Logoff/Quit.";
    conn.send_line(ss.str(), d);

    conn.send_line("\r\n", d);
    conn.send(StrCat(color(3), "Enter Selection: "), d);
    const auto key_str = conn.receive_upto(1, std::chrono::seconds(15));
    // dump left overs
    conn.receive_upto(1024, std::chrono::milliseconds(1));
    if (key_str.empty()) {
      continue;
    }
    const auto key = key_str.front();

    for (const auto& b : c.bbses) {
      if (std::toupper(b.key) == std::toupper(key)) {
        conn.send_line("\r\n", d);
        return b;
      }
    }
    // Hangup
    if (key == '!') {
      return std::nullopt;
    }
  }

  // Out of tries. The caller closes the socket.
  return std::nullopt;
}


// Can throw
// ReSharper disable once CppMemberFunctionMayBeConst
ConnectionHandler::BlockedConnectionResult ConnectionHandler::CheckForBlockedConnection() {
  const auto sock = sock_.get();
  VLOG(4) << "ConnectionHandler::CheckForBlockedConnection; (1): " << sock;

  VLOG(4) << "ConnectionHandler::CheckForBlockedConnection; (2): " << sock;
  // We fail open when we can't get the remote peer
  const auto o = GetRemotePeerAddress(sock);
  if (!o) {
    LOG(ERROR) << "CheckForBlockedConnection: Allowing connections we can't determine the remote peer.";
    return BlockedConnectionResult(BlockedConnectionAction::ALLOW, "???");
  }
  const auto remote_peer = o.value();

  VLOG(4) << "ConnectionHandler::CheckForBlockedConnection; (3): " << sock;
  const auto& b = data.c->blocking;
  // Check for always allowed addresses
  if (b.use_goodip_txt && data.good_ips_) {
    if (data.good_ips_->IsAlwaysAllowed(remote_peer)) {
      LOG(INFO) << "Allowing connection for goodip.txt always-allowed peer: " << remote_peer;
      return BlockedConnectionResult(BlockedConnectionAction::ALLOW, remote_peer);
    }
  }

  VLOG(4) << "ConnectionHandler::CheckForBlockedConnection; (4): " << sock;
  // Check for always blocked addresses
  if (b.use_badip_txt && data.bad_ips_) {
    if (data.bad_ips_->IsBlocked(remote_peer)) {
      // We have a connection from a blocked country
      LOG(INFO) << "Denying connection attempt from badip.txt blocked peer: " << remote_peer;
      return BlockedConnectionResult(BlockedConnectionAction::DENY, remote_peer);
    }
  }

  VLOG(4) << "ConnectionHandler::CheckForBlockedConnection; (5a): " << sock;
  if (is_rfc1918_private_address(remote_peer)) {
    LOG(INFO) << "Allowing connection for peer from RFC1918 address space: " << remote_peer;
    return BlockedConnectionResult(BlockedConnectionAction::ALLOW, remote_peer);
  }

  VLOG(4) << "ConnectionHandler::CheckForBlockedConnection; (5b): " << sock;
  // Check for country blocking if we have a DNS cc server defined.
  if (b.use_dns_cc && !b.dns_cc_server.empty() && !ends_with(b.dns_cc_server, "nerd.dk")) {
    // TODO(rushfan): HACK to disable using nerd.dk which is down.
    const auto cc = get_dns_cc(remote_peer, b.dns_cc_server);
    LOG(INFO) << "Validating country code for connection on port: " << port_
              << "; from peer: " << remote_peer << "; country code: " << cc;
    if (contains(data.c->blocking.block_cc_countries, cc)) {
      // We have a connection from a blocked country
      LOG(INFO) << "Denying connection attempt from country " << cc << " for peer: " << remote_peer;
      return BlockedConnectionResult(BlockedConnectionAction::DENY, remote_peer);
    }
  }

  VLOG(4) << "ConnectionHandler::CheckForBlockedConnection; (6): " << sock;
  // Not blocked address nor blocked country. See if it's a new
  // connection, and if so, should we block it now.
  if (b.auto_blocklist && data.auto_blocker_) {
    if (!data.auto_blocker_->Connection(remote_peer)) {
      // We have a newly blocked address.
      LOG(INFO) << "Denying connection attempt from AutoBlocker: " << remote_peer;
      return BlockedConnectionResult(BlockedConnectionAction::DENY, remote_peer);
    }
  }

  // Nothing left to check, let the connection through.
  LOG(INFO) << "Allowing connection for peer: " << remote_peer;
  return BlockedConnectionResult(BlockedConnectionAction::ALLOW, remote_peer);
}

void ConnectionHandler::HandleBinkPConnection() {
  // sock_ is closed when this handler is destroyed unless it's handed to
  // the child, so the early returns below don't close it themselves.
  try {
    const auto result = CheckForBlockedConnection();
    SocketConnection conn(sock_.get(), SocketConnection::ExitMode::LEAVE_SOCKET_OPEN);
    if (result.action == BlockedConnectionAction::DENY) {
      VLOG(1) << " BINKP BUSY (Blocked): " << result.remote_peer;
      conn.send_line("BUSY (Blocked)\r\n", 10s);
      return;
    }
    if (!data.concurrent_connections_->aquire(result.remote_peer)) {
      LOG(INFO) << " BINKP BUSY (Concurrent Limit Reached): " << result.remote_peer;
      conn.send_line("BUSY (Concurrent Limit Reached)\r\n", 10s);
      return;
    }
    auto at_exit = finally([&] { data.concurrent_connections_->release(result.remote_peer); });

    auto& nodemgr = data.nodes->at("BINKP");
    auto node = -1;
    if (!nodemgr->AcquireNode(node, result.remote_peer)) {
      LOG(INFO) << " BINKP BUSY (No Available Nodes): " << result.remote_peer;
      conn.send_line("BUSY (No Available Nodes)\r\n", 10s);
      return;
    }
    launch_cmd(*data.c, data.c->binkp_cmd, "", nodemgr, 0, std::move(sock_), ConnectionType::BINKP,
               result.remote_peer, data.sessions);
  } catch (const std::exception& e) {
    LOG(ERROR) << "HandleBinkPConnection: Handled Uncaught Exception: " << e.what();
  }
}

// ReSharper disable once CppMemberFunctionMayBeConst
ConnectionHandler::MailerModeResult ConnectionHandler::DoMailerMode() {
  SocketConnection conn(sock_.get(), SocketConnection::ExitMode::RESET_TO_BLOCKING);

  const auto text = fmt::format(
      "CONNECT 2400\r\nWWIV - Server {}\r\nPress <ESC> twice for the BBS...\r\n", full_version());
  VLOG(1) << "In DoMailerMode.";
  conn.send_line(text, 10s);

  const auto end = system_clock::now() + 10s;
  auto num_escapes = 0;
  while (system_clock::now() < end && num_escapes < 2) {
    conn.send(".", 1s);
    if (auto received = conn.receive_upto(1, 1s); !received.empty() && received.front() == 27) {
      ++num_escapes;
    }
  }

  conn.send("\r\n\r\n", 1s);

  return num_escapes > 1 ? MailerModeResult::ALLOW : MailerModeResult::DENY;
}

void ConnectionHandler::HandleConnection() {
  // sock_ is closed when this handler is destroyed unless it's handed to
  // the child, so the early returns below don't close it themselves.
  VLOG(4) << "ConnectionHandler::HandleConnection; sock: " << sock_.get();
  try {
    SocketConnection conn(sock_.get(), SocketConnection::ExitMode::LEAVE_SOCKET_OPEN);
    const auto result = CheckForBlockedConnection();
    if (result.action == BlockedConnectionAction::DENY) {
      VLOG(1) << "HandleConnection: BUSY (Blocked): " << result.remote_peer;
      conn.send_line("BUSY (Blocked)\r\n", 10s);
      return;
    }
    VLOG(4) << "After block check";
    if (!data.concurrent_connections_->aquire(result.remote_peer)) {
      LOG(INFO) << " BUSY (Concurrent Limit Reached): " << result.remote_peer;
      conn.send_line("BUSY (Concurrent Limit Reached)\r\n", 10s);
      return;
    }
    VLOG(4) << "After concurrent check";
    auto at_exit = finally([&] { data.concurrent_connections_->release(result.remote_peer); });
    const auto connection_type = connection_type_for(*data.c, port_);

    if (data.c->blocking.mailer_mode && connection_type == ConnectionType::TELNET) {
      VLOG(4) << "doing mailer mode check";
      if (const auto mailer_result = DoMailerMode(); mailer_result == MailerModeResult::DENY) {
        LOG(INFO) << "DENY (from MailerMode, didn't press ESC twice)";
        return;
      }
      LOG(INFO) << "ACCEPT (From MailerMode)";
    }

    if (data.c->bbses.empty()) {
      // If no bbses are defined, bail early and let someone know.
      LOG(ERROR) << "No BBSes defined in WWIVconfig for the Matrix.";
      conn.send_line("No BBSes defined in WWIVconfig for the Matrix.  Please tell the SysOp.",
                     std::chrono::seconds(1));
      return;
    }

    std::optional<wwivd_matrix_entry_t> bbs;
    if (connection_type == ConnectionType::TELNET) {
      VLOG(4) << "connection_type: ConnectionType::TELNET";
      bbs = DoMatrixLogon(*data.c);
    } else if (connection_type == ConnectionType::SSH) {
      VLOG(4) << "connection_type: ConnectionType::SSH";
      bbs = data.c->bbses.front();
    }
    if (!bbs) {
      VLOG(1) << "No BBS selected (caller quit or timed out); closing connection.";
      return;
    }

    VLOG(2) << "BBS is: " << bbs->name;

    if (!contains(*data.nodes, bbs->name)) {
      LOG(ERROR) << "Can't find node manager for bbs: " << bbs->name;
      conn.send_line(StrCat("Can't find config for bbs: ", bbs->name), std::chrono::seconds(1));
      return;
    }
    auto& nodemgr = data.nodes->at(bbs->name);

    // Telnet or SSH connection.  Find open node number and launch the child.
    auto node = -1;
    if (nodemgr->AcquireNode(node, result.remote_peer)) {
      launch_node(*data.config, *data.c, *bbs, nodemgr, node, std::move(sock_), connection_type,
                  result.remote_peer, data.sessions);
      VLOG(1) << "Exiting HandleConnection (launch_node)";
    } else {
      LOG(INFO) << "Sending BUSY. No available node to handle connection.";
      conn.send_line("BUSY (No Available Nodes)\r\n", 10s);
      VLOG(1) << "Exiting HandleConnection: BUSY (No Available Nodes)";
    }
  } catch (const std::exception& e) {
    LOG(ERROR) << "Handled Uncaught Exception: " << e.what();
  } catch (...) {
    LOG(ERROR) << "Handled Uncaught Exception: !!!";
  }
}

} // namespace wwiv
