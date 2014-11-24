#pragma once
#ifndef __INCLUDED_NETWORKB_BINKP_H__
#define __INCLUDED_NETWORKB_BINKP_H__

#include <chrono>
#include <cstdint>
#include <map>
#include <memory>
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
class TransferFile;

enum class BinkState {
  CONN_INIT,
  WAIT_CONN,
  SEND_PASSWORD,
  WAIT_ADDR,
  AUTH_REMOTE,
  IF_SECURE,
  WAIT_OK,
  UNKNOWN
};

class BinkP {
public:
  explicit BinkP(Connection* conn);
  virtual ~BinkP();

  void Run();

private:
  std::string command_id_to_name(int command_id) const;

  bool maybe_process_all_frames(std::chrono::milliseconds d);
  bool process_one_frame(std::chrono::milliseconds d);
  bool process_command(int16_t length, std::chrono::milliseconds d);
  bool process_data(int16_t length, std::chrono::milliseconds d);

  bool send_command_packet(uint8_t command_id, const std::string& data);
  bool send_data_packet(const char* data, std::size_t size);

  BinkState ConnInit();
  BinkState WaitConn();
  BinkState SendPasswd();
  BinkState WaitAddr();
  BinkState WaitOk();
  bool SendFilePacket(TransferFile* file);
  bool SendFileData(TransferFile* file);
  bool HandleFileGetRequest(const std::string& request_line);
  bool HandleFileGotRequest(const std::string& request_line);
  BinkState SendDummyFile();
  Connection* conn_;
  std::string address_list;
  bool ok_received;
  std::map<std::string, std::unique_ptr<TransferFile>> files_to_send_;
};


}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_BINKP_H__