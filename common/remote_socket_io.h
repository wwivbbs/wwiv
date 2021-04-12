/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services            */
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
#ifndef INCLUDED_COMMON_REMOTE_SOCKET_IO_H
#define INCLUDED_COMMON_REMOTE_SOCKET_IO_H

// ReSharper disable once CppUnusedIncludeDirective
#include "core/net.h" // INVALID_SOCKET
#include "common/remote_io.h"
#include <atomic>
#include <cstdint>
#include <mutex>
#include <queue>
#include <thread>

#if defined( _WIN32 )
#define NOCRYPT // Disable include of wincrypt.h
#include <winsock2.h>
#else 

typedef int HANDLE;
typedef int SOCKET;
#endif // _WIN32

namespace wwiv::common {

class RemoteSocketIO final : public RemoteIO {
 public:
  static const uint8_t TELNET_OPTION_IAC = 255;
  static const uint8_t TELNET_OPTION_NOP = 241;
  static const uint8_t TELNET_OPTION_BRK = 243;

  static const uint8_t TELNET_OPTION_WILL = 251;
  static const uint8_t TELNET_OPTION_WONT = 252;
  static const uint8_t TELNET_OPTION_DO = 253;
  static const uint8_t TELNET_OPTION_DONT = 254;

  static const uint8_t TELNET_SB = 250;
  static const uint8_t TELNET_SE = 240;

  static const uint8_t TELNET_OPTION_BINARY = 0;
  static const uint8_t TELNET_OPTION_ECHO = 1;
  static const uint8_t TELNET_OPTION_RECONNECTION = 2;
  static const uint8_t TELNET_OPTION_SUPPRESSS_GA = 3;
  static const uint8_t TELNET_OPTION_TERMINAL_TYPE = 24;
  static const uint8_t TELNET_OPTION_WINDOW_SIZE = 31;
  static const uint8_t TELNET_OPTION_TERMINAL_SPEED = 32;
  static const uint8_t TELNET_OPTION_LINEMODE = 34;

  static bool Initialize();

  RemoteSocketIO(unsigned int socket_handle, bool telnet);
  ~RemoteSocketIO() override;

  bool open() override;
  void close(bool bIsTemporary) override;
  unsigned char getW() override;
  bool disconnect() override;
  void purgeIn() override;
  unsigned int put(unsigned char ch) override;
  unsigned int read(char *buffer, unsigned int count) override;
  unsigned int write(const char *buffer, unsigned int count, bool no_translation = false) override;
  bool connected() override;
  bool incoming() override;
  void StopThreads();
  void StartThreads();
  unsigned int GetHandle() const override;
  bool valid_socket() const { return (socket_ != INVALID_SOCKET); }

  // VisibleForTesting
  void AddStringToInputBuffer(int start, int end, const char* buffer);
  std::queue<char>& queue() { return queue_; }

  void set_binary_mode(bool b) override;
  std::optional<ScreenPos> screen_position() override;

private:
  void HandleTelnetIAC(unsigned char nCmd, unsigned char nParam);
  void InboundTelnetProc();

  std::queue<char> queue_;
  mutable std::mutex mu_;
  mutable std::mutex threads_started_mu_;
  SOCKET socket_{INVALID_SOCKET};
  std::thread read_thread_;
  std::atomic<bool> stop_;
  bool threads_started_{false};
  bool telnet_{true};
  bool skip_next_{false};
};


} // namespace wwiv::common

#endif
