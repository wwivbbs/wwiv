#pragma once
#ifndef __INCLUDED_NETWORKB_CONNECTION_H__
#define __INCLUDED_NETWORKB_CONNECTION_H__

#include <cstdint>
#include <exception>
#include <string>

#ifdef _WIN32
#include <WinSock2.h>
#endif  // _WIN32

namespace wwiv {
namespace net {

using std::string;

class Connection
{
public:
  Connection(const string& host, int port);
  virtual ~Connection();

  int receive(void* data, int size);
  int send(const uint8_t* data, int size);

  uint16_t read_uint16();
  uint8_t read_uint8();

private:
  const string host_;
  const int port_;
  SOCKET sock_;
};

struct socket_error : public std::runtime_error {
  socket_error(const string& message) : runtime_error(message) {}
};

struct connection_error : public socket_error {
  connection_error(const string& host, int port);
};

}  // namespace net
} // namespace wwiv

#endif  // __INCLUDED_NETWORKB_CONNECTION_H__