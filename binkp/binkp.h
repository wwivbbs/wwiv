/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#ifndef INCLUDED_BINKP_BINKP_H
#define INCLUDED_BINKP_BINKP_H

#include "core/command_line.h"
#include "core/connection.h"
#include "binkp/cram.h"
#include "binkp/file_manager.h"
#include "binkp/receive_file.h"
#include "binkp/remote.h"
#include "sdk/net/callout.h"
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <string>

namespace wwiv::net {
  
class BinkConfig;
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
  FATAL_ERROR,
  DONE
};

enum class BinkSide {
  ORIGINATING,
  ANSWERING
};

enum class AuthType {
  PLAIN_TEXT,
  CRAM_MD5
};

// Main protocol driver class for BinkP derived sessions.
class BinkP {
public:
  typedef std::function<TransferFile*(
      const std::string& network_name, const std::string& filename)>
      received_transfer_file_factory_t;
  BinkP(wwiv::core::Connection* conn, BinkConfig* config, BinkSide side,
        const std::string& expected_remote_node,
        received_transfer_file_factory_t& received_transfer_file_factory);
  virtual ~BinkP();

  void Run(const wwiv::core::CommandLine& cmdline);

private:
  // Process frames until we time out waiting for a new frame.
  bool process_frames(std::chrono::duration<double> d);
  // Process frames until predicate is satisfied (returns true) or we time out waiting
  // for a new frame.
  bool process_frames(const std::function<bool()>& predicate, std::chrono::duration<double> d);
 
  bool process_opt(const std::string& opt);
  bool process_command(int16_t length, std::chrono::duration<double> d);
  bool process_data(int16_t length, std::chrono::duration<double> d);

  bool send_command_packet(uint8_t command_id, const std::string& data);
  bool send_data_packet(const char* data, int size);

  void process_network_files(const wwiv::core::CommandLine& cmdline) const;

  BinkState ConnInit();
  BinkState WaitConn();
  BinkState SendPasswd();
  BinkState WaitAddr();
  BinkState WaitOk();
  BinkState WaitPwd();
  BinkState PasswordAck();
  BinkState IfSecure();
  BinkState AuthRemote();
  BinkState AuthRemoteAnswering();
  BinkState AuthRemoteCalling();
  BinkState TransferFiles();
  BinkState WaitEob();
  BinkState Unknown();
  BinkState FatalError();
  bool SendFilePacket(TransferFile* file);
  bool SendFileData(TransferFile* file);
  bool HandleFileGetRequest(const std::string& request_line);
  bool HandleFileGotRequest(const std::string& request_line);
  bool HandlePassword(const std::string& password_line);
  bool HandleFileRequest(const std::string& request_line);

  bool CheckPassword(const sdk::fido::FidoAddress& address);

  BinkConfig* config_ = nullptr;
  wwiv::core::Connection* conn_ = nullptr;
  bool ok_received_ = false;
  bool eob_received_ = false;
  std::map<std::string, std::unique_ptr<TransferFile>> files_to_send_;
  BinkSide side_;
  const std::string expected_remote_node_;
  std::string remote_password_;
  bool error_received_ = false;
  received_transfer_file_factory_t received_transfer_file_factory_;
  std::unique_ptr<ReceiveFile> current_receive_file_;
  unsigned int bytes_received_ = 0;
  unsigned int bytes_sent_ = 0;

  // Handles CRAM-MD5 authentication
  Cram cram_;
  // Auth type used.
  AuthType auth_type_ = AuthType::PLAIN_TEXT;
  bool crc_ = false;

  std::unique_ptr<FileManager> file_manager_;
  Remote remote_;
};

// Parses a M_FILE request line into it's parts.
// TODO(rushfan): Support the 5th CRC parameter in FRL-1022
// See  http://www.filegate.net/ftsc/FRL-1022.001
bool ParseFileRequestLine(const std::string& request_line, 
			  std::string* filename,
			  long* length,
			  time_t* timestamp,
			  long* offset,
        uint32_t* crc);

// Returns just the expected password for a node (node) contained in the
// callout.net file used by the wwiv::sdk::Callout class.
std::string expected_password_for(const sdk::net::net_call_out_rec* con);

template <typename N>
std::string expected_password_for(const wwiv::sdk::Callout* callout, N node) {
  return expected_password_for(callout->net_call_out_for(node));
}

}  // namespace

#endif
