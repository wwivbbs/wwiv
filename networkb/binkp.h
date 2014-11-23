#pragma once
#ifndef __INCLUDED_NETWORKB_BINKP_H__
#define __INCLUDED_NETWORKB_BINKP_H__

#include <cstdint>
#include <string>
#include <vector>

namespace wwiv {
namespace net {
  
static const int M_NUL  = 0;
static const int M_ADR  = 1;
static const int M_PWD  = 2;
static const int M_FILE = 3;
static const int M_OK   = 4;
static const int M_EOB  = 5;
static const int M_GOT  = 6;
static const int M_ERR  = 7;
static const int M_BSY  = 8;
static const int M_GET  = 9;
static const int M_SKIP = 10;

class Connection;

enum class BinkState {
  UNKNOWN
};

class BinkP {
public:
  explicit BinkP(Connection* conn);
  virtual ~BinkP();

  void Run();

private:
  bool process_one_frame();
  std::string command_id_to_name(int command_id) const;

  bool process_command(int16_t length);
  bool process_data(int16_t length);

  bool send_command_packet(uint8_t command_id, const std::string& data);
  bool send_data_packet(const char* data, std::size_t size);

  BinkState ConnInit();
  BinkState WaitConn();
  BinkState SendPasswd();
  BinkState WaitAddr();
  BinkState WaitOk();
  BinkState SendDummyFile();
  Connection* conn_;
  std::string address_list;
  //std::vector<std::string> addresses;
  bool ok_received;

};


}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_BINKP_H__