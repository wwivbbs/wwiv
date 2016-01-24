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
#endif  // _WIN32

#include "bbs/wiot.h"

#include <iostream>
#include <memory>
#include <process.h>

#include "sdk/user.h"
#include "bbs/platform/platformfcns.h"
#include "core/strings.h"
#include "core/file.h"
#include "core/os.h"
#include "core/wwivport.h"
#include "core/wwivassert.h"


using std::clog;
using std::endl;
using std::string;
using std::unique_ptr;
using namespace wwiv::strings;


WIOTelnet::WIOTelnet(int socket_handle)
  : socket_(static_cast<SOCKET>(socket_handle)), threads_started_(false) {
  // assigning the value to a static causes this only to be
  // initialized once.
  static bool once = WIOTelnet::Initialize();
  if (socket_handle == 0) {
    // This means we don't have a real socket handle, for example running in local mode.
    // so we set it to INVALID_SOCKET and don't initialize anything.
    socket_ = INVALID_SOCKET;
    return;
  }
  else if (socket_handle == INVALID_SOCKET) {
    // Also exit early if we have an invalid handle.
    return;
  }

  // Make sure our signal event is not set to the "signaled" state
  stop_event_ = CreateEvent(nullptr, true, false, nullptr);
  clog << "Created Stop Event: " << GetLastErrorText() << endl;
}

unsigned int WIOTelnet::GetHandle() const {
  return static_cast<unsigned int>(socket_);
}

unsigned int WIOTelnet::GetDoorHandle() const {
  return GetHandle();
}

bool WIOTelnet::open() {
  if (socket_ == INVALID_SOCKET) {
    // We can not open an invalid socket.
    return false;
  }
  StartThreads();

  SOCKADDR_IN addr;
  int nAddrSize = sizeof(SOCKADDR);

  getpeername(socket_, reinterpret_cast<SOCKADDR *>(&addr), &nAddrSize);

  const string address = inet_ntoa(addr.sin_addr);
  SetRemoteName("Internet TELNET Session");
  SetRemoteAddress(address);

  { 
    unsigned char s[3] = {
        WIOTelnet::TELNET_OPTION_IAC,
        WIOTelnet::TELNET_OPTION_DONT,
        WIOTelnet::TELNET_OPTION_ECHO
    };
    write(reinterpret_cast<char*>(s), 3, true);
  }
  { 
    unsigned char s[3] = {
        WIOTelnet::TELNET_OPTION_IAC,
        WIOTelnet::TELNET_OPTION_WILL,
        WIOTelnet::TELNET_OPTION_ECHO
    };
    write(reinterpret_cast<char*>(s), 3, true);
  }
  { 
    unsigned char s[3] = { 
        WIOTelnet::TELNET_OPTION_IAC,
        WIOTelnet::TELNET_OPTION_DONT,
        WIOTelnet::TELNET_OPTION_LINEMODE 
    };
    write(reinterpret_cast<char*>(s), 3, true);
  }

  return true;
}

void WIOTelnet::close(bool temporary) {
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

unsigned int WIOTelnet::put(unsigned char ch) {
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

unsigned char WIOTelnet::getW() {
  if (!valid_socket()) { return 0; }
  char ch = 0;
  std::lock_guard<std::mutex> lock(mu_);
  if (!queue_.empty()) {
    ch = queue_.front();
    queue_.pop();
  }
  return static_cast<unsigned char>(ch);
}

bool WIOTelnet::dtr(bool raise) {
  // Early return on invalid sockets.
  if (!valid_socket()) { return false; }

  if (!raise) {
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
  }
  return true;
}

void WIOTelnet::purgeIn() {
  // Early return on invalid sockets.
  if (!valid_socket()) { return; }

  std::lock_guard<std::mutex> lock(mu_);
  while (!queue_.empty()) {
    queue_.pop();
  }
}

unsigned int WIOTelnet::read(char *buffer, unsigned int count) {
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

unsigned int WIOTelnet::write(const char *buffer, unsigned int count, bool bNoTranslation) {
  // Early return on invalid sockets.
  if (!valid_socket()) { return 0; }

  unique_ptr<char[]> tmp_buffer(new char[count * 2 + 100]);
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
    ::Sleep(0);
  }
}

bool WIOTelnet::carrier() {
  return valid_socket();
}

bool WIOTelnet::incoming() {
  // Early return on invalid sockets.
  if (!valid_socket()) { return false; }

  std::lock_guard<std::mutex> lock(mu_);
  bool bRet = (queue_.size() > 0);
  return bRet;
}

void WIOTelnet::StopThreads() {
  if (!threads_started_) {
    return;
  }
  if (!SetEvent(stop_event_)) {
    const string error_text = GetLastErrorText();
    clog << "WIOTelnet::StopThreads: Error with SetEvent " << GetLastError() 
         << " - '" << error_text << "'" << endl;
  }
  ::Sleep(0);

  // Wait for read thread to exit.
  read_thread_.join();
  threads_started_ = false;
}

void WIOTelnet::StartThreads() {
  if (threads_started_) {
    return;
  }

  if (!ResetEvent(stop_event_)) {
    const string error_text = GetLastErrorText();
    clog << "WIOTelnet::StartThreads: Error with ResetEvent " << GetLastError()
         << " - '" << error_text << "'" << endl;
  }

  read_thread_ = std::thread([this]() { InboundTelnetProc(this); });
  threads_started_ = true;
}

WIOTelnet::~WIOTelnet() {
  StopThreads();
  CloseHandle(stop_event_);
  stop_event_ = INVALID_HANDLE_VALUE;
  WSACleanup();
}

// Static Class Members.

bool WIOTelnet::Initialize() {
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
}

void WIOTelnet::InboundTelnetProc(void* pTelnetVoid) {
  WIOTelnet* pTelnet = static_cast<WIOTelnet*>(pTelnetVoid);
  WSAEVENT hEvent = WSACreateEvent();
  WSANETWORKEVENTS events;
  char szBuffer[4096];
  bool bDone = false;
  SOCKET socket_ = static_cast<SOCKET>(pTelnet->socket_);
  int nRet = WSAEventSelect(socket_, hEvent, FD_READ | FD_CLOSE);
  HANDLE hArray[2];
  hArray[0] = hEvent;
  hArray[1] = pTelnet->stop_event_;

  if (nRet == SOCKET_ERROR) {
    bDone = true;
  }
  while (!bDone) {
    DWORD dwWaitRet = WSAWaitForMultipleEvents(2, hArray, false, 10000, false);
    if (dwWaitRet == (WSA_WAIT_EVENT_0 + 1)) {
      if (!ResetEvent(pTelnet->stop_event_)) {
        const string error_text = GetLastErrorText();
        clog << "WIOTelnet::InboundTelnetProc: Error with ResetEvent " 
             << GetLastError() << " - '" << error_text << "'" << endl;
      }

      bDone = true;
      break;
    }
    nRet = WSAEnumNetworkEvents(socket_, hEvent, &events);
    if (nRet == SOCKET_ERROR) {
      bDone = true;
      break;
    }
    if (events.lNetworkEvents & FD_READ) {
      memset(szBuffer, 0, 4096);
      nRet = recv(socket_, szBuffer, sizeof(szBuffer), 0);
      if (nRet == SOCKET_ERROR) {
        if (WSAGetLastError() != WSAEWOULDBLOCK) {
          // Error or the socket was closed.
          socket_ = INVALID_SOCKET;
          bDone = true;
          break;
        }
      } else if (nRet == 0) {
        socket_ = INVALID_SOCKET;
        bDone = true;
        break;
      }
      szBuffer[ nRet ] = '\0';

      // Add the data to the input buffer
      int nNumSleeps = 0;
      while ((pTelnet->queue_.size() > 32678) && (nNumSleeps++ <= 10) && !bDone) {
        ::Sleep(100);
      }

      pTelnet->AddStringToInputBuffer(0, nRet, szBuffer);
    } else if (events.lNetworkEvents & FD_CLOSE) {
      bDone = true;
      socket_ = INVALID_SOCKET;
      break;
    }
  }

  if (socket_ == INVALID_SOCKET) {
    closesocket(socket_);
    socket_ = INVALID_SOCKET;
    pTelnet->socket_ = INVALID_SOCKET;
  }

  WSACloseEvent(hEvent);
}

void WIOTelnet::HandleTelnetIAC(unsigned char nCmd, unsigned char nParam) {
  // We should probably start responding to the DO and DONT options....
  ::OutputDebugString("HandleTelnetIAC: ");

  switch (nCmd) {
  case TELNET_OPTION_NOP: {
    ::OutputDebugString("TELNET_OPTION_NOP\n");
  }
  break;
  case TELNET_OPTION_BRK: {
    ::OutputDebugString("TELNET_OPTION_BRK\n");
  }
  break;
  case TELNET_OPTION_WILL: {
    const string s = StringPrintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_WILL", nParam);
    ::OutputDebugString(s.c_str());
  }
  break;
  case TELNET_OPTION_WONT: {
    const string s = StringPrintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_WONT", nParam);
    ::OutputDebugString(s.c_str());
  }
  break;
  case TELNET_OPTION_DO: {
    const string do_s = StringPrintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_DO", nParam);
    ::OutputDebugString(do_s.c_str());
    switch (nParam) {
    case TELNET_OPTION_SUPPRESSS_GA: {
      const string will_s = StringPrintf("%c%c%c", TELNET_OPTION_IAC, TELNET_OPTION_WILL, TELNET_OPTION_SUPPRESSS_GA);
      write(will_s.c_str(), 3, true);
      ::OutputDebugString("Sent TELNET IAC WILL SUPPRESSS GA\r\n");
    }
    break;
    }
  }
  break;
  case TELNET_OPTION_DONT: {
    const string dont_s = StringPrintf("[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_DONT", nParam);
    ::OutputDebugString(dont_s.c_str());
  }
  break;
  }
}

void WIOTelnet::AddStringToInputBuffer(int nStart, int nEnd, char *buffer) {
  WWIV_ASSERT(buffer);
  std::lock_guard<std::mutex> lock(mu_);

  bool bBinaryMode = GetBinaryMode();
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
