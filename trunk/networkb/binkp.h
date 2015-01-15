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

#include "networkb/callout.h"
#include "networkb/receive_file.h"

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

// Main protocol driver class for BinkP derived sessions.
class BinkP {
public:
  typedef std::function<TransferFile*(const std::string& network_name, const std::string& filename)>
      received_transfer_file_factory_t;
  // TODO(rushfan): should we use a unique_ptr for Connection and own the
  // connection?
  BinkP(Connection* conn,
        BinkConfig* config,
        std::map<const std::string, Callout>& callouts,
	      BinkSide side, 
	      int expected_remote_node,
        received_transfer_file_factory_t& received_transfer_file_factory);
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

  void rename_pending_files() const;
  void process_network_files() const;
  const std::string remote_network_name() const;

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

  BinkConfig* config_;
  std::map<const std::string, Callout> callouts_;
  Connection* conn_;
  std::string address_list_;
  bool ok_received_;
  bool eob_received_;
  std::map<std::string, std::unique_ptr<TransferFile>> files_to_send_;
  BinkSide side_;
  const int expected_remote_node_;
  std::string remote_password_;
  bool error_received_;
  received_transfer_file_factory_t received_transfer_file_factory_;
  std::vector<std::string> received_files_;
  std::unique_ptr<ReceiveFile> current_receive_file_;
};

// Parses a M_FILE request line into it's parts.
// TODO(rushfan): Support the 5th CRC parameter in FRL-1022
// See  http://www.filegate.net/ftsc/FRL-1022.001
bool ParseFileRequestLine(const std::string& request_line, 
			  std::string* filename,
			  long* length,
			  time_t* timestamp,
			  long* offset);

// Returns just the expected password for a node (node) contained in the
// CALLOUT.NET file used by the Callout class.
std::string expected_password_for(const Callout* callout, int node);

// Returns just the node number (such as "1") from a FTN address like
// (such as "20000:20000/1@wwivnet")
int node_number_from_address_list(const std::string& network_list, const std::string& network_name);

// Returns just the network name (such as "wwivnet") from a FTN address like
// (such as "20000:20000/1@wwivnet")
std::string network_name_from_single_address(const std::string& network_list);

}  // namespace net
}  // namespace wwiv

#endif  // __INCLUDED_NETWORKB_BINKP_H__
