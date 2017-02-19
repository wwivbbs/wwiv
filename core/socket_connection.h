/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
#pragma once
#ifndef __INCLUDED_NETWORKB_SOCKET_CONNECTION_H__
#define __INCLUDED_NETWORKB_SOCKET_CONNECTION_H__

#include <chrono>
#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>
#include <string>

#include "core/net.h"
#include "core/connection.h"

#ifdef _WIN32
#include <WinSock2.h>

#else  // _WIN32
#endif  // _WIN32

namespace wwiv {
namespace core {

class SocketConnection;

std::unique_ptr<SocketConnection> Connect(const std::string& host, int port);

class SocketConnection : public Connection
{
public:
  explicit SocketConnection(SOCKET sock);
  SocketConnection(SOCKET sock, bool close_socket);
  virtual ~SocketConnection();

  int receive(void* data, int size, std::chrono::duration<double> d) override;
  std::string receive(int size, std::chrono::duration<double> d) override;
  std::string read_line(int max_size, std::chrono::duration<double> d);
  int send(const void* data, int size, std::chrono::duration<double> d) override;
  int send(const std::string& s, std::chrono::duration<double> d) override;
  /** Sends a line s and \r\n */
  int send_line(const std::string& s, std::chrono::duration<double> d);

  uint16_t read_uint16(std::chrono::duration<double> d) override;
  uint8_t read_uint8(std::chrono::duration<double> d) override;

  bool is_open() const { return open_; }
  bool close() override;

private:
  SOCKET sock_;
  bool open_;
  bool close_socket_ = true;
};


}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_SOCKET_CONNECTION_H__
