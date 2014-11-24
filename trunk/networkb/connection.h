#pragma once
#ifndef __INCLUDED_NETWORKB_CONNECTION_H__
#define __INCLUDED_NETWORKB_CONNECTION_H__

#include <chrono>
#include <cstdint>
#include <string>

namespace wwiv {
namespace net {

class Connection
{
public:
  Connection();
  virtual ~Connection();

  virtual int receive(void* data, int size, std::chrono::milliseconds d) = 0;
  virtual int send(const void* data, int size, std::chrono::milliseconds d) = 0;

  virtual uint16_t read_uint16(std::chrono::milliseconds d) = 0;
  virtual uint8_t read_uint8(std::chrono::milliseconds d) = 0;

  virtual bool send_uint8(uint8_t data, std::chrono::milliseconds d) = 0;
  virtual bool send_uint16(uint16_t data, std::chrono::milliseconds d) = 0;
};

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_CONNECTION_H__
