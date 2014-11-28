#pragma once
#ifndef __INCLUDED_NETWORKB_FAKE_CONNECTION_H__
#define __INCLUDED_NETWORKB_FAKE_CONNECTION_H__

#include <chrono>
#include <cstdint>
#include <string>
#include <queue>

#include "networkb/connection.h"

class FakeBinkpPacket {
public:
  FakeBinkpPacket(const void* data, int size);
  ~FakeBinkpPacket();

  bool is_command() const { return is_command_; }
  uint8_t command() const { return command_; }
  uint16_t header() const { return header_; }
  std::string data() const { return data_; }

private:
  bool is_command_;
  uint8_t command_;
  uint16_t header_;
  std::string data_;
};

class FakeConnection : public wwiv::net::Connection
{
public:
  // Connection implementation.
  FakeConnection();
  virtual ~FakeConnection();

  virtual int receive(void* data, int size, std::chrono::milliseconds d) override;
  virtual int send(const void* data, int size, std::chrono::milliseconds d) override;
  virtual uint16_t read_uint16(std::chrono::milliseconds d) override;
  virtual uint8_t read_uint8(std::chrono::milliseconds d) override;

  std::queue<FakeBinkpPacket> receive_queue_;
  std::queue<FakeBinkpPacket> send_queue_;
};

#endif  // __INCLUDED_NETWORKB_FAKE_CONNECTION_H__
