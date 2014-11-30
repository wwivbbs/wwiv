#pragma once
#ifndef __INCLUDED_NETWORKB_BINKP_H__
#define __INCLUDED_NETWORKB_BINKP_H__

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

namespace wwiv {
namespace net {
  
class BinkConfig;
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
  WAIT_PWD,
  PASSWORD_ACK,
  TRANSFER_FILES,
  WAIT_EOB,
  UNKNOWN,
  DONE
};

enum class BinkSide {
  ORIGINATING,
  ANSWERING
};

class BinkP {
public:
  // TODO(rushfan): should we use a unique_ptr for Connection and own the
  // connection?
  BinkP(Connection* conn,
        BinkConfig* config,
	      BinkSide side, 
	      int expected_remote_address);
  virtual ~BinkP();

  void Run();

private:
  // Process frames until we time out waiting for a new frame.
  bool process_frames(std::chrono::milliseconds d);
  // Process frames until predicate is satisfied (returns true) or we time out waiting
  // for a new frame.
  bool process_frames(std::function<bool()> predicate, std::chrono::milliseconds d);
 
  bool process_command(int16_t length, std::chrono::milliseconds d);
  bool process_data(int16_t length, std::chrono::milliseconds d);

  bool send_command_packet(uint8_t command_id, const std::string& data);
  bool send_data_packet(const char* data, std::size_t size);

  BinkState ConnInit();
  BinkState WaitConn();
  BinkState SendPasswd();
  BinkState WaitAddr();
  BinkState WaitOk();
  BinkState WaitPwd();
  BinkState PasswordAck();
  BinkState IfSecure();
  BinkState AuthRemote();
  BinkState TransferFiles();
  BinkState WaitEob();
  BinkState Unknown();
  bool SendFilePacket(TransferFile* file);
  bool SendFileData(TransferFile* file);
  bool HandleFileGetRequest(const std::string& request_line);
  bool HandleFileGotRequest(const std::string& request_line);
  bool HandleFileRequest(const std::string& request_line);
  BinkState SendDummyFile(const std::string& filename, char fill, std::size_t size);

  BinkConfig* config_;
  Connection* conn_;
  std::string address_list_;
  bool ok_received_;
  bool eob_received_;
  std::map<std::string, std::unique_ptr<TransferFile>> files_to_send_;
  BinkSide side_;
  const int own_address_;
  const int expected_remote_address_;
  std::string remote_password_;
  bool error_received_;
};

bool ParseFileRequestLine(const std::string& request_line, 
			  std::string* filename,
			  long* length,
			  time_t* timestamp,
			  long* offset);

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_BINKP_H__
