/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2019, WWIV Software Services             */
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
#ifndef __INCLUDED_NETWORKB_FAKE_CONNECTION_H__
#define __INCLUDED_NETWORKB_FAKE_CONNECTION_H__

#include <chrono>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <queue>

#include "core/connection.h"

class FakeBinkpPacket {
public:
  FakeBinkpPacket(const void* data, int size);
  FakeBinkpPacket(const FakeBinkpPacket& o);
  ~FakeBinkpPacket();

  bool is_command() const { return is_command_; }
  uint8_t command() const { return command_; }
  uint16_t header() const { return header_; }
  std::string data() const { return data_; }

  std::string debug_string() const;

private:
  bool is_command_;
  uint8_t command_;
  uint16_t header_;
  std::string data_;
};

class FakeConnection : public wwiv::core::Connection
{
public:
  // Connection implementation.
  FakeConnection();
  virtual ~FakeConnection();

  int receive(void* data, int size, std::chrono::duration<double> d) override;
  std::string receive(int size, std::chrono::duration<double> d) override;
  int send(const void* data, int size, std::chrono::duration<double> d) override;
  int send(const std::string& s, std::chrono::duration<double> d) override;

  uint16_t read_uint16(std::chrono::duration<double> d) override;
  uint8_t read_uint8(std::chrono::duration<double> d) override;
  bool is_open() const override;
  bool close() override;

  bool has_sent_packets() const;
  FakeBinkpPacket GetNextPacket();
  void ReplyCommand(int8_t command_id, const std::string& data);

  // GUARDED_BY(mu_)
  std::queue<FakeBinkpPacket> receive_queue_;
  // GUARDED_BY(mu_)
  std::queue<FakeBinkpPacket> send_queue_;
private:
  mutable std::mutex mu_;
  bool open_;
};

#endif  // __INCLUDED_NETWORKB_FAKE_CONNECTION_H__
