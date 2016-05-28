/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#ifdef _WIN32
// work around error using inet_ntoa on build machine.
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#pragma comment(lib, "Ws2_32.lib")
#include "WS2tcpip.h"
#else

#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

typedef int HANDLE;
typedef int SOCKET;
constexpr int SOCKET_ERROR = -1;
#define SOCKADDR_IN sockaddr_in
#define closesocket(s) close(s)
#endif  // _WIN32

#include "bbs/remote_socket_io.h"

#include <iostream>
#include <memory>
#include <process.h>

#include "sdk/user.h"
#include "bbs/platform/platformfcns.h"
#include "core/strings.h"
#include "core/file.h"
#include "core/os.h"
#include "core/scope_exit.h"
#include "core/wwivport.h"
#include "core/wwivassert.h"


using std::chrono::milliseconds;
using std::clog;
using std::endl;
using std::lock_guard;
using std::make_unique;
using std::mutex;
using std::string;
using std::thread;
using std::unique_ptr;
using wwiv::core::ScopeExit;
using wwiv::os::sleep_for;
using wwiv::os::yield;
using namespace wwiv::strings;

struct socket_error: public std::runtime_error {
  socket_error(const string& message): std::runtime_error(message) {}
};

static bool socket_avail(SOCKET sock, int seconds) {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(sock, &fds);

  timeval tv;
  tv.tv_sec = seconds;
  tv.tv_usec = 0;

  int result = select(sock + 1, &fds, 0, 0, &tv);
  if (result == SOCKET_ERROR) {
    throw socket_error("Error on select for socket.");
  }
  return result == 1;
}

RemoteSocketIO::RemoteSocketIO(int socket_handle, bool telnet)
  : socket_(static_cast<SOCKET>(socket_handle)), telnet_(telnet) {
  // assigning the value to a static causes this only to be
  // initialized once.
  static bool once = RemoteSocketIO::Initialize();

  // Make sure our signal event is not set to the "signaled" state.
  stop_.store(false);

  if (socket_handle == 0) {
    // This means we don't have a real socket handle, for example running in local mode.
    // so we set it to INVALID_SOCKET and don't initialize anything.
    socket_ = INVALID_SOCKET;
    return;
  }
  else if (socket_handle == INVALID_SOCKET) {
    // Also exit early if we have an invalid handle.
    socket_ = INVALID_SOCKET;
    return;
  }
}

unsigned int RemoteSocketIO::GetHandle() const {
  return static_cast<unsigned int>(socket_);
}

unsigned int RemoteSocketIO::GetDoorHandle() const {
  return GetHandle();
}

bool RemoteSocketIO::open() {
  if (socket_ == INVALID_SOCKET) {
    // We can not open an invalid socket.
    return false;
  }
  StartThreads();

  SOCKADDR_IN addr;
  int nAddrSize = sizeof(SOCKADDR);

  getpeername(socket_, reinterpret_cast<SOCKADDR *>(&addr), &nAddrSize);

  remote_info().cid_name = "Internet TELNET Session";
  const string address = inet_ntoa(addr.sin_addr);
  remote_info().address = address;
  if (telnet_) {
    { 
      unsigned char s[3] = {
          RemoteSocketIO::TELNET_OPTION_IAC,
          RemoteSocketIO::TELNET_OPTION_DONT,
          RemoteSocketIO::TELNET_OPTION_ECHO
      };
      write(reinterpret_cast<char*>(s), 3, true);
    }
    { 
      unsigned char s[3] = {
          RemoteSocketIO::TELNET_OPTION_IAC,
          RemoteSocketIO::TELNET_OPTION_WILL,
          RemoteSocketIO::TELNET_OPTION_ECHO
      };
      write(reinterpret_cast<char*>(s), 3, true);
    }
    { 
      unsigned char s[3] = { 
          RemoteSocketIO::TELNET_OPTION_IAC,
          RemoteSocketIO::TELNET_OPTION_DONT,
          RemoteSocketIO::TELNET_OPTION_LINEMODE 
      };
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
  } else {
    StopThreads();
  }
}

unsigned int RemoteSocketIO::put(unsigned char ch) {
  // Early return on invalid sockets.
  if (!valid_socket()) { return 0; }

  unsigned char szBuffer[3] = { ch, 0, 0 };
  if (ch == TELNET_OPTION_IAC) {
    szBuffer[1] = ch;
  }

  for (;;) {
    int nRet = send(socket_, reinterpret_cast<char*>(szBuffer),
        strlen(reinterpret_cast<char*>(szBuffer)), 0);
    if (nRet == SOCKET_ERROR) {
      if (WSAGetLastError() != WSAEWOULDBLOCK) {
        if (WSAGetLastError() != WSAENOTSOCK) {
          clog << "DEBUG: ERROR on send from put() [" << WSAGetLastError() << "] [#" 
               << static_cast<int> (ch) << "]" << endl;
        }
        return 0;
      }
    } else {
      return nRet;
    }
    ::Sleep(0);
  }
}

unsigned char RemoteSocketIO::getW() {
  if (!valid_socket()) { return 0; }
  char ch = 0;
  std::lock_guard<std::mutex> lock(mu_);
  if (!queue_.empty()) {
    ch = queue_.front();
    queue_.pop();
  }
  return static_cast<unsigned char>(ch);
}

bool RemoteSocketIO::dtr(bool raise) {
  // Early return on invalid sockets.
  if (!valid_socket()) { return false; }

  if (!raise) {
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
  }
  return true;
}

void RemoteSocketIO::purgeIn() {
  // Early return on invalid sockets.
  if (!valid_socket()) { return; }

  std::lock_guard<std::mutex> lock(mu_);
  while (!queue_.empty()) {
    queue_.pop();
  }
}

unsigned int RemoteSocketIO::read(char *buffer, unsigned int count) {
  // Early return on invalid sockets.
  if (!valid_socket()) { return 0; }

  unsigned int nRet = 0;
  char * temp = buffer;

  std::lock_guard<std::mutex> lock(mu_);
  while (!queue_.empty() && nRet <= count) {
    char ch = queue_.front();
    queue_.pop();
    *temp++ = ch;
    nRet++;
  }
  *temp++ = '\0';
  return nRet;
}

unsigned int RemoteSocketIO::write(const char *buffer, unsigned int count, bool bNoTranslation) {
  // Early return on invalid sockets.
  if (!valid_socket()) { return 0; }

  unique_ptr<char[]> tmp_buffer = make_unique<char[]>(count * 2 + 100);
  memset(tmp_buffer.get(), 0, count * 2 + 100);
  int nCount = count;

  if (!bNoTranslation && (memchr(buffer, CHAR_TELNET_OPTION_IAC, count) != nullptr)) {
    // If there is a #255 then excape the #255's
    const char* p = buffer;
    char* p2 = tmp_buffer.get();
    for (unsigned int i = 0; i < count; i++) {
      if (*p == CHAR_TELNET_OPTION_IAC && !bNoTranslation) {
        *p2++ = CHAR_TELNET_OPTION_IAC;
        *p2++ = CHAR_TELNET_OPTION_IAC;
        nCount++;
      } else {
        *p2++ = *p;
      }
      p++;
    }
    *p2++ = '\0';
  } else {
    memcpy(tmp_buffer.get(), buffer, count);
  }

  for (;;) {
    int nRet = send(socket_, tmp_buffer.get(), nCount, 0);
    if (nRet == SOCKET_ERROR) {
      if (WSAGetLastError() != WSAEWOULDBLOCK) {
        if (WSAGetLastError() != WSAENOTSOCK) {
          clog << "DEBUG: in write(), expected to send " << count << " character(s), actually sent " << nRet << endl;
        }
        return 0;
      }
    } else {
      return nRet;
    }
    yield();
  }
}

bool RemoteSocketIO::carrier() {
  return valid_socket();
}

bool RemoteSocketIO::incoming() {
  // Early return on invalid sockets.
  if (!valid_socket()) { return false; }

  lock_guard<mutex> lock(mu_);
  return !queue_.empty();
}

void RemoteSocketIO::StopThreads() {
  if (!threads_started_) {
    return;
  }
  stop_.store(true);
  yield();

  // Wait for read thread to exit.
  read_thread_.join();
  threads_started_ = false;
  if (socket_ == INVALID_SOCKET) {
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
  }
}

void RemoteSocketIO::StartThreads() {
  if (threads_started_) {
    return;
  }

  stop_.store(false);
  read_thread_ = thread(&RemoteSocketIO::InboundTelnetProc, this);
  threads_started_ = true;
}

RemoteSocketIO::~RemoteSocketIO() {
  StopThreads();

#ifdef _WIN32
  WSACleanup();
#endif  // _WIN32
}

// Static Class Members.

bool RemoteSocketIO::Initialize() {
#ifdef _WIN32
  WSADATA wsaData;
  int err = WSAStartup(0x0101, &wsaData);

  if (err != 0) {
    switch (err) {
    case WSASYSNOTREADY:
      clog << "Error from WSAStartup: WSASYSNOTREADY";
      break;
    case WSAVERNOTSUPPORTED:
      clog << "Error from WSAStartup: WSAVERNOTSUPPORTED";
      break;
    case WSAEINPROGRESS:
      clog << "Error from WSAStartup: WSAEINPROGRESS";
      break;
    case WSAEPROCLIM:
      clog << "Error from WSAStartup: WSAEPROCLIM";
      break;
    case WSAEFAULT:
      clog << "Error from WSAStartup: WSAEFAULT";
      break;
    default:
      clog << "Error from WSAStartup: ** unknown error code **";
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
  unique_ptr<char[]> data = make_unique<char[]>(size);

  while (true) {
    if (stop_.load()) {
      return;
    }
    if (!socket_avail(socket_, 1)) {
      continue;
    }
    int num_read = recv(socket_, data.get(), size, 0);
    if (num_read == SOCKET_ERROR) {
      return;
    }
    AddStringToInputBuffer(0, num_read, data.get());
  }
}

void RemoteSocketIO::HandleTelnetIAC(unsigned char nCmd, unsigned char nParam) {
  // We should probably start responding to the DO and DONT options....
  switch (nCmd) {
  case TELNET_OPTION_NOP: {
    // TELNET_OPTION_NOP
  }
  break;
  case TELNET_OPTION_BRK: {
    // TELNET_OPTION_BRK;
  }
  break;
  case TELNET_OPTION_WILL: {
    // const string s = StringPrintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_WILL", nParam);
    // ::OutputDebugString(s.c_str());
  }
  break;
  case TELNET_OPTION_WONT: {
    // const string s = StringPrintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_WONT", nParam);
    // ::OutputDebugString(s.c_str());
  }
  break;
  case TELNET_OPTION_DO: {
    // const string do_s = StringPrintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_DO", nParam);
    // ::OutputDebugString(do_s.c_str());
    switch (nParam) {
    case TELNET_OPTION_SUPPRESSS_GA: {
      const string will_s = StringPrintf("%c%c%c", TELNET_OPTION_IAC, TELNET_OPTION_WILL, TELNET_OPTION_SUPPRESSS_GA);
      write(will_s.c_str(), 3, true);
      // Sent TELNET IAC WILL SUPPRESSS GA
    }
    break;
    }
  }
  break;
  case TELNET_OPTION_DONT: {
    // const string dont_s = StringPrintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_DONT", nParam);
    // ::OutputDebugString(dont_s.c_str());
  }
  break;
  }
}

void RemoteSocketIO::AddStringToInputBuffer(int nStart, int nEnd, char *buffer) {
  WWIV_ASSERT(buffer);

  // Add the data to the input buffer
  for (int num_sleeps = 0; num_sleeps < 10 && queue_.size() > 32678; ++num_sleeps) {
    sleep_for(milliseconds(100));
  }

  lock_guard<mutex> lock(mu_);

  bool bBinaryMode = binary_mode();
  for (int i = nStart; i < nEnd; i++) {
    if ((static_cast<unsigned char>(buffer[i]) == 255)) {
      if ((i + 1) < nEnd  && static_cast<unsigned char>(buffer[i + 1]) == 255) {
        queue_.push(buffer[i + 1]);
        i++;
      } else if ((i + 2) < nEnd) {
        HandleTelnetIAC(buffer[i + 1], buffer[i + 2]);
        i += 2;
      } else {
        ::OutputDebugString("WHAT THE HECK?!?!?!? 255 w/o any options or anything\r\n");
      }
    } else if (bBinaryMode || buffer[i] != '\0') {
      // I think the nulls in the input buffer were being bad... RF20020906
      // This fixed the problem of telnetting with CRT to a linux machine and then telnetting from
      // that linux box to the bbs... Hopefully this will fix the Win9x built-in telnet client as
      // well as TetraTERM
      queue_.push(buffer[i]);
    } else {
      ::OutputDebugString("buffer had a null\r\n");
    }
  }
}
