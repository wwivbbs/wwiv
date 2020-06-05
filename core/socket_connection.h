/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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
#ifndef __INCLUDED_NETWORKB_SOCKET_CONNECTION_H__
#define __INCLUDED_NETWORKB_SOCKET_CONNECTION_H__

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include "core/connection.h"
#include "core/net.h" // SOCKET

#ifdef _WIN32
#include <WinSock2.h>

#else  // _WIN32
#endif  // _WIN32

namespace wwiv {
namespace core {

class SocketConnection;

std::unique_ptr<SocketConnection> Connect(const std::string& host, int port);

class SocketConnection : public Connection {
public:

  enum class ExitMode { LEAVE_SOCKET_OPEN, RESET_TO_BLOCKING, CLOSE_SOCKET };

  explicit SocketConnection(SOCKET sock);
  SocketConnection(SOCKET sock, ExitMode exit_mode);
  SocketConnection(const SocketConnection& other) = delete;
  SocketConnection& operator=(const SocketConnection& other) = delete;

  virtual ~SocketConnection();

  int receive(void* data, int size, std::chrono::duration<double> d) override;
  std::string receive(int size, std::chrono::duration<double> d) override;

  /** Receives up to size bytes and will return partial reads. */
  int receive_upto(void* data, int size, std::chrono::duration<double> d);
  std::string receive_upto(int size, std::chrono::duration<double> d);

  std::string read_line(int max_size, std::chrono::duration<double> d);
  int send(const void* data, int size, std::chrono::duration<double> d) override;
  int send(const std::string& s, std::chrono::duration<double> d) override;
  /** Sends a line s and \r\n */
  int send_line(const std::string& s, std::chrono::duration<double> d);

  uint16_t read_uint16(std::chrono::duration<double> d) override;
  uint8_t read_uint8(std::chrono::duration<double> d) override;

  bool is_open() const override { return open_; }
  bool close() override;

private:
  SOCKET sock_;
  bool open_;
  ExitMode exit_mode_ = ExitMode::LEAVE_SOCKET_OPEN;
};


} // namespace net
} // namespace wwiv

#endif  // __INCLUDED_NETWORKB_SOCKET_CONNECTION_H__
