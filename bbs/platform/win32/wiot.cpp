/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include "bbs/platform/win32/wiot.h"

#include <iostream>
#include <process.h>

#include "bbs/wuser.h"
#include "bbs/platform/platformfcns.h"
#include "bbs/fcns.h"
#include "core/strings.h"
#include "core/file.h"
#include "core/wwivport.h"
#include "core/wwivassert.h"

#pragma comment(lib, "Ws2_32.lib")

WIOTelnet::WIOTelnet(unsigned int nHandle) : socket_(static_cast<SOCKET>(nHandle)), threads_started_(false) {
  WIOTelnet::InitializeWinsock();
  if (!DuplicateHandle(GetCurrentProcess(), reinterpret_cast<HANDLE>(socket_),
                       GetCurrentProcess(), reinterpret_cast<HANDLE*>(&duplicate_socket_),
                       0, TRUE, DUPLICATE_SAME_ACCESS)) {
    std::cout << "Error creating duplicate socket: " << GetLastError() << "\r\n\n";
  }
  if (socket_ != 0 && socket_ != INVALID_SOCKET) {
    // Make sure our signal event is not set to the "signaled" state
    stop_event_ = CreateEvent(nullptr, true, false, nullptr);
    std::cout << "Created Stop Event: " << GetLastErrorText() << std::endl;
  } else {
    const char *message = "**ERROR: UNABLE to create STOP EVENT FOR WIOTelnet";
    sysoplog(message);
    std::cout << message << std::endl;
  }
}

bool WIOTelnet::setup(char, int, int, unsigned long) {
  return true;
}

unsigned int WIOTelnet::GetHandle() const {
  return static_cast<unsigned int>(socket_);
}

unsigned int WIOTelnet::GetDoorHandle() const {
  return static_cast<unsigned int>(duplicate_socket_);
}

unsigned int WIOTelnet::open() {
  StartThreads();

  SOCKADDR_IN addr;
  int nAddrSize = sizeof(SOCKADDR);

  getpeername(socket_, reinterpret_cast<SOCKADDR *>(&addr), &nAddrSize);

  const std::string address = inet_ntoa(addr.sin_addr);
  SetRemoteName("Internet TELNET Session");
  SetRemoteAddress(address);

  char szTempTelnet[ 21 ];
  snprintf(szTempTelnet, sizeof(szTempTelnet), "%c%c%c", WIOTelnet::TELNET_OPTION_IAC, WIOTelnet::TELNET_OPTION_DONT,
           WIOTelnet::TELNET_OPTION_ECHO);
  write(szTempTelnet, 3, true);
  snprintf(szTempTelnet, sizeof(szTempTelnet), "%c%c%c", WIOTelnet::TELNET_OPTION_IAC, WIOTelnet::TELNET_OPTION_WILL,
           WIOTelnet::TELNET_OPTION_ECHO);
  write(szTempTelnet, 3, true);
  snprintf(szTempTelnet, sizeof(szTempTelnet), "%c%c%c", WIOTelnet::TELNET_OPTION_IAC, WIOTelnet::TELNET_OPTION_DONT,
           WIOTelnet::TELNET_OPTION_LINEMODE);
  write(szTempTelnet, 3, true);

  return 0;
}

void WIOTelnet::close(bool bIsTemporary) {
  if (!bIsTemporary) {
    // this will stop the threads
    closesocket(socket_);
  } else {
    StopThreads();
  }
}

unsigned int WIOTelnet::putW(unsigned char ch) {
  if (socket_ == INVALID_SOCKET) {
#ifdef _DEBUG
    std::cout << "pw(INVALID-SOCKET) ";
#endif // _DEBUG
    return 0;
  }
  char szBuffer[5];
  szBuffer[0] = static_cast<char>(ch);
  szBuffer[1] = '\0';
  if (ch == TELNET_OPTION_IAC) {
    szBuffer[1] = static_cast<char>(ch);
    szBuffer[2] = '\0';
  }

  for (;;) {
    int nRet = send(socket_, szBuffer, strlen(szBuffer), 0);
    if (nRet == SOCKET_ERROR) {
      if (WSAGetLastError() != WSAEWOULDBLOCK) {
        if (WSAGetLastError() != WSAENOTSOCK) {
          std::cout << "DEBUG: ERROR on send from putW() [" << WSAGetLastError() << "] [#" << static_cast<int>
                    (ch) << "]" << std::endl;
        }
        return 0;
      }
    } else {
      return nRet;
    }
    WWIV_Delay(0);
  }
}

unsigned char WIOTelnet::getW() {
  char ch = 0;
  WaitForSingleObject(mu_, INFINITE);
  if (!queue_.empty()) {
    ch = queue_.front();
    queue_.pop();
  }
  ReleaseMutex(mu_);
  return static_cast<unsigned char>(ch);
}

bool WIOTelnet::dtr(bool raise) {
  if (!raise) {
    closesocket(socket_);
    closesocket(duplicate_socket_);
    socket_ = INVALID_SOCKET;
    duplicate_socket_ = INVALID_SOCKET;
  }
  return true;
}

void WIOTelnet::flushOut() {
  // NOP - We don't wait for output
}

void WIOTelnet::purgeOut() {
  // NOP - We don't wait for output
}

void WIOTelnet::purgeIn() {
  // Implementing this is new as of 2003-03-31, so if this causes problems,
  // then we may need to remove it [Rushfan]
  WaitForSingleObject(mu_, INFINITE);
  while (!queue_.empty()) {
    queue_.pop();
  }
  ReleaseMutex(mu_);
}

unsigned int WIOTelnet::put(unsigned char ch) {
  return putW(ch);
}

char WIOTelnet::peek() {
  char ch = 0;

  WaitForSingleObject(mu_, INFINITE);
  if (!queue_.empty()) {
    ch = queue_.front();
  }
  ReleaseMutex(mu_);
  return ch;
}

unsigned int WIOTelnet::read(char *buffer, unsigned int count) {
  unsigned int nRet = 0;
  char * pszTemp = buffer;

  WaitForSingleObject(mu_, INFINITE);
  while ((!queue_.empty()) && (nRet <= count)) {
    char ch = queue_.front();
    queue_.pop();
    *pszTemp++ = ch;
    nRet++;
  }
  *pszTemp++ = '\0';
  ReleaseMutex(mu_);
  return nRet;
}

unsigned int WIOTelnet::write(const char *buffer, unsigned int count, bool bNoTranslation) {
  int nRet;
  char * pszBuffer = reinterpret_cast<char*>(malloc(count * 2 + 100));
  ZeroMemory(pszBuffer, (count * 2) + 100);
  int nCount = count;

  if (!bNoTranslation && (memchr(buffer, CHAR_TELNET_OPTION_IAC, count) != nullptr)) {
    // If there is a #255 then excape the #255's
    const char * p = buffer;
    char * p2 = pszBuffer;
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
    memcpy(pszBuffer, buffer, count);
  }

  for (;;) {
    nRet = send(socket_, pszBuffer, nCount, 0);
    if (nRet == SOCKET_ERROR) {
      if (WSAGetLastError() != WSAEWOULDBLOCK) {
        if (WSAGetLastError() != WSAENOTSOCK) {
          std::cout << "DEBUG: in write(), expected to send " << count << " character(s), actually sent " << nRet << std::endl;
        }
        free(pszBuffer);
        return 0;
      }
    } else {
      free(pszBuffer);
      return nRet;
    }
    WWIV_Delay(0);
  }
}

bool WIOTelnet::carrier() {
  return (socket_ != INVALID_SOCKET);
}

bool WIOTelnet::incoming() {
  WaitForSingleObject(mu_, INFINITE);
  bool bRet = (queue_.size() > 0);
  ReleaseMutex(mu_);
  return bRet;
}

void WIOTelnet::StopThreads() {
  if (!threads_started_) {
    return;
  }
  if (!SetEvent(stop_event_)) {
    const std::string error_text = GetLastErrorText();
    std::cout << "WIOTelnet::StopThreads: Error with SetEvent " << GetLastError() 
              << " - '" << error_text << "'" << std::endl;
  }
  WWIV_Delay(0);

  // Wait for read thread to exit.
  DWORD dwRes = WaitForSingleObject(read_thread_, 5000);
  switch (dwRes) {
  case WAIT_OBJECT_0:
    // Thread Ended
    break;
  case WAIT_TIMEOUT:
    // The exit code of 123 doesn't mean anything, and isn't used anywhere.
    ::TerminateThread(read_thread_, 123);
    std::cout << "WIOTelnet::StopThreads: Terminated the read thread" << std::endl;
    break;
  default:
    break;
  }
  threads_started_ = false;
}

void WIOTelnet::StartThreads() {
  if (threads_started_) {
    return;
  }

  if (!ResetEvent(stop_event_)) {
    const std::string error_text = GetLastErrorText();
    std::cout << "WIOTelnet::StartThreads: Error with ResetEvent " << GetLastError()
              << " - '" << error_text << "'" << std::endl;
  }

  mu_ = ::CreateMutex(nullptr, false, "WWIV Input Buffer");
  read_thread_ = reinterpret_cast<HANDLE>(_beginthread(WIOTelnet::InboundTelnetProc, 0, static_cast< void * >(this)));
  threads_started_ = true;
}

WIOTelnet::~WIOTelnet() {
  StopThreads();
  CloseHandle(stop_event_);
  stop_event_ = INVALID_HANDLE_VALUE;
  WSACleanup();
}

//
// Static Class Members.
//

void WIOTelnet::InitializeWinsock() {
  WSADATA wsaData;
  int err = WSAStartup(0x0101, &wsaData);

  if (err != 0) {
    switch (err) {
    case WSASYSNOTREADY:
      std::cout << "Error from WSAStartup: WSASYSNOTREADY";
      break;
    case WSAVERNOTSUPPORTED:
      std::cout << "Error from WSAStartup: WSAVERNOTSUPPORTED";
      break;
    case WSAEINPROGRESS:
      std::cout << "Error from WSAStartup: WSAEINPROGRESS";
      break;
    case WSAEPROCLIM:
      std::cout << "Error from WSAStartup: WSAEPROCLIM";
      break;
    case WSAEFAULT:
      std::cout << "Error from WSAStartup: WSAEFAULT";
      break;
    default:
      std::cout << "Error from WSAStartup: ** unknown error code **";
      break;
    }
  }
}

void WIOTelnet::InboundTelnetProc(LPVOID pTelnetVoid) {
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
        const std::string error_text = GetLastErrorText();
        std::cout << "WIOTelnet::InboundTelnetProc: Error with ResetEvent " 
                  << GetLastError() << " - '" << error_text << "'" << std::endl;
      }

      bDone = true;
      break;
    }
    int nRet = WSAEnumNetworkEvents(socket_, hEvent, &events);
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
    closesocket(pTelnet->duplicate_socket_);
    socket_ = INVALID_SOCKET;
    pTelnet->socket_ = INVALID_SOCKET;
    pTelnet->duplicate_socket_ = INVALID_SOCKET;
  }

  WSACloseEvent(hEvent);
}

void WIOTelnet::HandleTelnetIAC(unsigned char nCmd, unsigned char nParam) {
  //
  // We should probably start responding to the DO and DONT options....
  //
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
    char szBuffer[ 255 ];
    _snprintf(szBuffer, sizeof(szBuffer), "[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_WILL", nParam);
    ::OutputDebugString(szBuffer);
  }
  break;
  case TELNET_OPTION_WONT: {
    char szBuffer[ 255 ];
    _snprintf(szBuffer, sizeof(szBuffer), "[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_WONT", nParam);
    ::OutputDebugString(szBuffer);
  }
  break;
  case TELNET_OPTION_DO: {
    char szBuffer[ 255 ];
    _snprintf(szBuffer, sizeof(szBuffer), "[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_DO", nParam);
    ::OutputDebugString(szBuffer);
    switch (nParam) {
    case TELNET_OPTION_SUPPRESSS_GA: {
      char szTelnetOptionBuffer[ 255 ];
      _snprintf(szTelnetOptionBuffer, sizeof(szTelnetOptionBuffer), "%c%c%c", TELNET_OPTION_IAC, TELNET_OPTION_WILL,
                TELNET_OPTION_SUPPRESSS_GA);
      write(szTelnetOptionBuffer, 3, true);
      ::OutputDebugString("Sent TELNET IAC WILL SUPPRESSS GA\r\n");
    }
    break;
    }
  }
  break;
  case TELNET_OPTION_DONT: {
    char szBuffer[ 255 ];
    _snprintf(szBuffer, sizeof(szBuffer), "[Command: %s] [Option: {%d}]\n", "TELNET_OPTION_DONT", nParam);
    ::OutputDebugString(szBuffer);
  }
  break;
  }
}

void WIOTelnet::AddStringToInputBuffer(int nStart, int nEnd, char *pszBuffer) {
  WWIV_ASSERT(pszBuffer);
  WaitForSingleObject(mu_, INFINITE);

  char szBuffer[4096];
  strncpy(szBuffer, pszBuffer, sizeof(szBuffer));
  memcpy(szBuffer, pszBuffer, nEnd);
  bool bBinaryMode = GetBinaryMode();
  for (int i = nStart; i < nEnd; i++) {
    if ((static_cast<unsigned char>(szBuffer[i]) == 255)) {
      if ((i + 1) < nEnd  && static_cast<unsigned char>(szBuffer[i + 1]) == 255) {
        AddCharToInputBuffer(szBuffer[i + 1]);
        i++;
      } else if ((i + 2) < nEnd) {
        HandleTelnetIAC(szBuffer[i + 1], szBuffer[i + 2]);
        i += 2;
      } else {
        ::OutputDebugString("WHAT THE HECK?!?!?!? 255 w/o any options or anything\r\n");
      }
    } else if (bBinaryMode || szBuffer[i] != '\0') {
      // I think the nulls in the input buffer were being bad... RF20020906
      // This fixed the problem of telnetting with CRT to a linux machine and then telnetting from
      // that linux box to the bbs... Hopefully this will fix the Win9x built-in telnet client as
      // well as TetraTERM
      AddCharToInputBuffer(szBuffer[i]);
    } else {
      ::OutputDebugString("szBuffer had a null\r\n");
    }
  }
  ReleaseMutex(mu_);
}

void WIOTelnet::AddCharToInputBuffer(char ch) {
  queue_.push(ch);
}
