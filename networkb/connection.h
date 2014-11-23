#pragma once
#ifndef __INCLUDED_NETWORKB_CONNECTION_H__
#define __INCLUDED_NETWORKB_CONNECTION_H__

#include <chrono>
#include <cstdint>
#include <exception>
#include <string>

#ifdef _WIN32
#include <WinSock2.h>
#endif  // _WIN32

namespace wwiv {
namespace net {

class Connection
{
public:
  Connection(const std::string& host, int port);
  virtual ~Connection();

  int receive(void* data, int size, std::chrono::milliseconds d);
  int send(const void* data, int size, std::chrono::milliseconds d);

  uint16_t read_uint16(std::chrono::milliseconds d);
  uint8_t read_uint8(std::chrono::milliseconds d);

  bool send_uint8(uint8_t data, std::chrono::milliseconds d);
  bool send_uint16(uint16_t data, std::chrono::milliseconds d);

private:
  const std::string host_;
  const int port_;
  SOCKET sock_;
};

struct socket_error : public std::runtime_error {
  socket_error(const std::string& message) : runtime_error(message) {}
};

struct timeout_error : public socket_error {
  timeout_error(const std::string& message) : socket_error(message) {}
};

struct connection_error : public socket_error {
  connection_error(const std::string& host, int port);
};

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_CONNECTION_H__