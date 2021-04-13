/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include "common/remote_socket_io.h"

#ifdef _WIN32
#pragma comment(lib, "Ws2_32.lib")
#include "WS2tcpip.h"
// Really windows?
typedef int socklen_t;
#else

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#endif  // _WIN32

#include "core/file.h"
#include "core/log.h"
#include "core/net.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include <iostream>
#include <memory>
#include <system_error>

namespace wwiv::common {

using std::chrono::milliseconds;
using wwiv::core::ScopeExit;
using wwiv::os::sleep_for;
using namespace wwiv::core;
using namespace wwiv::strings;

// N.B. mutex and yield are defines in Solaris.

struct socket_error final : std::runtime_error {
  explicit socket_error(const std::string& message) : std::runtime_error(message) {}
};

static bool socket_avail(SOCKET sock, int seconds) {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;

  const auto result = select(sock + 1, &fds, nullptr, nullptr, &tv);
  if (result == SOCKET_ERROR) {
    throw socket_error("Error on select for socket.");
  }
  return result == 1;
}

RemoteSocketIO::RemoteSocketIO(unsigned int socket_handle, bool telnet)
    : socket_(static_cast<SOCKET>(socket_handle)), telnet_(telnet) {
  // assigning the value to a static causes this only to be initialized once.
  [[maybe_unused]] static auto once = Initialize();

  // Make sure our signal event is not set to the "signaled" state.
  stop_.store(false);

  if (socket_handle == 0 || socket_handle == INVALID_SOCKET) {
    // This means we don't have a real socket handle, for example running in local mode.
    // so we set it to INVALID_SOCKET and don't initialize anything.
    socket_ = INVALID_SOCKET;
  }
}

unsigned int RemoteSocketIO::GetHandle() const { return static_cast<unsigned int>(socket_); }

bool RemoteSocketIO::open() {
  if (socket_ == INVALID_SOCKET) {
    // We can not open an invalid socket.
    return false;
  }
  StartThreads();

  GetRemotePeerAddress(socket_, remote_info().address);
  GetRemotePeerHostname(socket_, remote_info().address_name);
  if (telnet_) {
    {
      unsigned char s[3] = {RemoteSocketIO::TELNET_OPTION_IAC, RemoteSocketIO::TELNET_OPTION_DONT,
                            RemoteSocketIO::TELNET_OPTION_ECHO};
      write(reinterpret_cast<char*>(s), 3, true);
    }
    {
      unsigned char s[3] = {RemoteSocketIO::TELNET_OPTION_IAC, RemoteSocketIO::TELNET_OPTION_WILL,
                            RemoteSocketIO::TELNET_OPTION_ECHO};
      write(reinterpret_cast<char*>(s), 3, true);
    }
    {
      unsigned char s[3] = {RemoteSocketIO::TELNET_OPTION_IAC, RemoteSocketIO::TELNET_OPTION_WILL,
                            RemoteSocketIO::TELNET_OPTION_SUPPRESSS_GA};
      write(reinterpret_cast<char*>(s), 3, true);
    }
    {
      unsigned char s[3] = {RemoteSocketIO::TELNET_OPTION_IAC, RemoteSocketIO::TELNET_OPTION_DONT,
                            RemoteSocketIO::TELNET_OPTION_LINEMODE};
      write(reinterpret_cast<char*>(s), 3, true);
    }
  }

  return true;
}

void RemoteSocketIO::close(bool temporary) {
  if (socket_ == INVALID_SOCKET) {
    // Early return on invalid sockets.
    return;
  }
  if (!temporary) {
    // this will stop the threads
    closesocket(socket_);
  }
  StopThreads();
}

unsigned int RemoteSocketIO::put(unsigned char ch) {
  // Early return on invalid sockets.
  if (!valid_socket()) {
    return 0;
  }

  unsigned char szBuffer[3] = {ch, 0, 0};
  if (ch == TELNET_OPTION_IAC) {
    szBuffer[1] = ch;
  }

  const auto num_sent = send(socket_, reinterpret_cast<char*>(szBuffer), ssize(szBuffer), 0);
  if (num_sent == SOCKET_ERROR) {
    return 0;
  }
  return num_sent;
}

unsigned char RemoteSocketIO::getW() {
  if (!valid_socket()) {
    return 0;
  }
  char ch = 0;
  std::lock_guard<std::mutex> lock(mu_);
  if (!queue_.empty()) {
    ch = queue_.front();
    queue_.pop();
  }
  return static_cast<unsigned char>(ch);
}

bool RemoteSocketIO::disconnect() {
  // Early return on invalid sockets.
  if (!valid_socket()) {
    return false;
  }

  closesocket(socket_);
  socket_ = INVALID_SOCKET;
  return true;
}

void RemoteSocketIO::purgeIn() {
  // Early return on invalid sockets.
  if (!valid_socket()) {
    return;
  }

  std::lock_guard<std::mutex> lock(mu_);
  while (!queue_.empty()) {
    queue_.pop();
  }
}

unsigned int RemoteSocketIO::read(char* buffer, unsigned int count) {
  // Early return on invalid sockets.
  if (!valid_socket()) {
    return 0;
  }

  unsigned int num_read = 0;
  auto* temp = buffer;

  std::lock_guard<std::mutex> lock(mu_);
  while (!queue_.empty() && num_read <= count) {
    const auto ch = queue_.front();
    queue_.pop();
    *temp++ = ch;
    num_read++;
  }
  *temp = '\0';
  return num_read;
}

static const char CHAR_TELNET_OPTION_IAC = '\xFF';

unsigned int RemoteSocketIO::write(const char* buffer, unsigned int count, bool no_translation) {
  // Early return on invalid sockets.
  if (!valid_socket()) {
    return 0;
  }

  const auto tmp_buffer = std::make_unique<char[]>(count * 2 + 100);
  memset(tmp_buffer.get(), 0, count * 2 + 100);
  int nCount = count;

  if (no_translation || !memchr(buffer, CHAR_TELNET_OPTION_IAC, count)) {
    memcpy(tmp_buffer.get(), buffer, count);
  } else {
    // If there is a #255 then escape the #255's
    const auto* p = buffer;
    auto* p2 = tmp_buffer.get();
    for (unsigned int i = 0; i < count; i++) {
      if (*p == CHAR_TELNET_OPTION_IAC && !no_translation) {
        *p2++ = CHAR_TELNET_OPTION_IAC;
        *p2++ = CHAR_TELNET_OPTION_IAC;
        nCount++;
      } else {
        *p2++ = *p;
      }
      p++;
    }
    *p2 = '\0';
  }

  const auto num_sent = send(socket_, tmp_buffer.get(), nCount, 0);
  if (num_sent == SOCKET_ERROR) {
    return 0;
  }
  return num_sent;
}

bool RemoteSocketIO::connected() {
  const auto connected = valid_socket();
  if (!connected) {
    LOG(ERROR) << "!connected(); threads_started_ = " << std::boolalpha << threads_started_;
  }
  return connected;
}

bool RemoteSocketIO::incoming() {
  // Early return on invalid sockets.
  if (!valid_socket()) {
    return false;
  }

  std::lock_guard<std::mutex> lock(mu_);
  return !queue_.empty();
}

void RemoteSocketIO::StopThreads() {
  {
    std::lock_guard<std::mutex> lock(threads_started_mu_);
    if (!threads_started_) {
      return;
    }
    stop_.store(true);
    threads_started_ = false;
  }
  os::yield();

  // Wait for read thread to exit.
  if (!read_thread_.joinable()) {
    LOG(ERROR) << "read_thread_ is not JOINABLE.  Should not happen.";
  }
  try {
    if (read_thread_.joinable()) {
      read_thread_.join();
    }
  } catch (const std::system_error& e) {
    LOG(ERROR) << "Caught system_error with code: " << e.code() << "; meaning: " << e.what();
  }
}

void RemoteSocketIO::StartThreads() {
  {
    std::lock_guard<std::mutex> lock(threads_started_mu_);
    if (threads_started_) {
      return;
    }
    threads_started_ = true;
  }

  stop_.store(false);
  read_thread_ = std::thread(&RemoteSocketIO::InboundTelnetProc, this);
}

RemoteSocketIO::~RemoteSocketIO() {
  try {
    StopThreads();

#ifdef _WIN32
    WSACleanup();
#endif // _WIN32
  } catch (const std::exception& e) {
    std::cerr << e.what();
  }
}

// Static Class Members.

bool RemoteSocketIO::Initialize() {
#ifdef _WIN32
  WSADATA wsaData;
  const auto err = WSAStartup(0x0101, &wsaData);

  if (err != 0) {
    switch (err) {
    case WSASYSNOTREADY:
      LOG(ERROR) << "Error from WSAStartup: WSASYSNOTREADY";
      break;
    case WSAVERNOTSUPPORTED:
      LOG(ERROR) << "Error from WSAStartup: WSAVERNOTSUPPORTED";
      break;
    case WSAEINPROGRESS:
      LOG(ERROR) << "Error from WSAStartup: WSAEINPROGRESS";
      break;
    case WSAEPROCLIM:
      LOG(ERROR) << "Error from WSAStartup: WSAEPROCLIM";
      break;
    case WSAEFAULT:
      LOG(ERROR) << "Error from WSAStartup: WSAEFAULT";
      break;
    default:
      LOG(ERROR) << "Error from WSAStartup: ** unknown error code **";
      break;
    }
  }
  return err == 0;
#else
  return true;
#endif
}

void RemoteSocketIO::InboundTelnetProc() {
  constexpr size_t size = 4 * 1024;
  const auto data = std::make_unique<char[]>(size);
  try {
    while (true) {
      if (stop_.load()) {
        return;
      }
      if (!socket_avail(socket_, 1)) {
        continue;
      }
      const auto num_read = recv(socket_, data.get(), size, 0);
      if (num_read == SOCKET_ERROR) {
        // Got Socket error.
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        return;
      }
      if (num_read == 0) {
        // The other side has gracefully closed the socket.
        closesocket(socket_);
        socket_ = INVALID_SOCKET;
        return;
      }
      AddStringToInputBuffer(0, num_read, data.get());
    }
  } catch (const socket_error& e) {
    LOG(ERROR) << "InboundTelnetProc exiting. Caught socket_error: " << e.what();
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
  }
}

void RemoteSocketIO::set_binary_mode(bool b) {
  binary_mode_ = b;
  skip_next_ = false;
}

std::optional<ScreenPos> RemoteSocketIO::screen_position() { 
  // Clear inbound
  while (incoming()) {
    getW();
  }

  write("\x1b[6n", 4);

  const auto now = std::chrono::steady_clock::now();
  auto l = now + std::chrono::seconds(3);

  std::string dsr_response;
  while (std::chrono::steady_clock::now() < l && connected()) {
    auto ch = getW();
    if (ch == '\x1b') {
      l = std::chrono::steady_clock::now() + std::chrono::seconds(1);
      while (std::chrono::steady_clock::now() < l && connected()) {
        ch = getW();
        if (ch) {
          if (isdigit(ch) || ch == ';') {
            dsr_response.push_back(ch);
          }
          if (ch == 'R') {
            VLOG(1) << "RemoteSocketIO::screen_position(): DSR: " << dsr_response;
            if (auto idx = dsr_response.find(';'); idx != std::string::npos) {
              auto y = to_number<int>(dsr_response.substr(0, idx));
              auto x = to_number<int>(dsr_response.substr(idx+1));
              return {ScreenPos{x, y}};
            }
            return std::nullopt;
          }
        }
      }
      return std::nullopt;
    }
  }
  return std::nullopt;
}

void RemoteSocketIO::HandleTelnetIAC(unsigned char nCmd, unsigned char nParam) {
  // We should probably start responding to the DO and DONT options....
  switch (nCmd) {
  case TELNET_OPTION_NOP: {
    // TELNET_OPTION_NOP
  } break;
  case TELNET_OPTION_BRK: {
    // TELNET_OPTION_BRK;
  } break;
  case TELNET_OPTION_WILL: {
    // const string s = fmt::sprintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_WILL",
    // nParam);
    // ::OutputDebugString(s.c_str());
  } break;
  case TELNET_OPTION_WONT: {
    // const string s = fmt::sprintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_WONT",
    // nParam);
    // ::OutputDebugString(s.c_str());
  } break;
  case TELNET_OPTION_DO: {
    // const string do_s = fmt::sprintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_DO",
    // nParam);
    // ::OutputDebugString(do_s.c_str());
    switch (nParam) {
    case TELNET_OPTION_SUPPRESSS_GA: {
      char s[4];
      s[0] = TELNET_OPTION_IAC;
      s[1] = TELNET_OPTION_WILL;
      s[2] = TELNET_OPTION_SUPPRESSS_GA;
      s[3] = 0;
      write(s, 3, true);
      // Sent TELNET IAC WILL SUPPRESS GA
    } break;
    }
  } break;
  case TELNET_OPTION_DONT: {
    // const string dont_s = fmt::sprintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_DONT",
    // nParam);
    // ::OutputDebugString(dont_s.c_str());
  } break;
  }
}

void RemoteSocketIO::AddStringToInputBuffer(int start, int end, const char* buffer) {
  // Add the data to the input buffer
  for (auto num_sleeps = 0; num_sleeps < 10 && queue_.size() > 32678; ++num_sleeps) {
    sleep_for(milliseconds(100));
  }

  std::lock_guard<std::mutex> lock(mu_);

  if (binary_mode()) {
    for (auto i = start; i < end; i++) {
      const uint8_t c = buffer[i];
      if (skip_next_ && c == 0xff) {
        skip_next_ = false;
        continue;
      }
      if (c == 0xff) {
        // TODO(rushfan): If this causes problems, we can add a setting for this
        VLOG(2) << "Got an escaped 255 possibly";
        skip_next_ = true;
      } else {
        skip_next_ = false;
      }
      queue_.push(c);
    }
    return;
  }
  for (auto i = start; i < end; i++) {
    if (static_cast<unsigned char>(buffer[i]) == 255) {
      if ((i + 1) < end && static_cast<unsigned char>(buffer[i + 1]) == 255) {
        queue_.push(buffer[i + 1]);
        i++;
      } else if ((i + 2) < end) {
        HandleTelnetIAC(buffer[i + 1], buffer[i + 2]);
        i += 2;
      } else {
        // ::OutputDebugString("WHAT THE HECK?!?!?!? 255 w/o any options or anything\r\n");
      }
    } else if (buffer[i] != '\0') {
      // RF20020906: I think the nulls in the input buffer were being bad...
      // This fixed the problem with CRT to a linux machine and then telnet from
      // that linux box to the bbs... Hopefully this will fix the Win9x built-in
      // telnet client as well as TetraTERM.
      queue_.push(buffer[i]);
    }
  }
}

} // namespace wwiv::common
