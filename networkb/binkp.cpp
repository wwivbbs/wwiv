/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
#include "networkb/binkp.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <functional>
#include <iostream>
#include <memory>
#include <map>
#include <string>
#include <vector>

#include "core/crc32.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/version.h"
#include "networkb/binkp_commands.h"
#include "networkb/binkp_config.h"
#include "core/connection.h"
#include "networkb/cram.h"
#include "networkb/file_manager.h"
#include "networkb/net_log.h"
#include "core/socket_exceptions.h"
#include "networkb/transfer_file.h"
#include "networkb/wfile_transfer_file.h"
#include "sdk/callout.h"
#include "sdk/contact.h"
#include "core/datetime.h"
#include "sdk/filenames.h"
#include "sdk/fido/fido_address.h"

using std::boolalpha;
using std::end;
using std::endl;
using std::function;
using std::map;
using std::max;
using std::min;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

namespace wwiv {
namespace net {

static int System(const string& bbsdir, const string& cmd) {
  const auto path = FilePath(bbsdir, cmd);

  auto err = system(path.c_str());
  LOG(INFO) << "       executed: '" << path << "' with an error code: " << err;
  return err;
}

string expected_password_for(const net_call_out_rec* con) {
  if (con != nullptr && !con->session_password.empty()) {
    // If the password is not empty string.
    return con->session_password;
  }
  // return default password of "-"
  return "-";
}

BinkP::BinkP(wwiv::core::Connection* conn, BinkConfig* config, BinkSide side,
        const std::string& expected_remote_node,
        received_transfer_file_factory_t& received_transfer_file_factory)
  : conn_(conn),
    config_(config), 
    side_(side),
    expected_remote_node_(expected_remote_node), 
    error_received_(false),
    received_transfer_file_factory_(received_transfer_file_factory),
    bytes_sent_(0),
    bytes_received_(0),
    remote_(config, side_ == BinkSide::ANSWERING, expected_remote_node) {
  if (side_ == BinkSide::ORIGINATING) {
    crc_ = config_->crc();
  }
}

BinkP::~BinkP() {
  files_to_send_.clear();
}

bool BinkP::process_opt(const std::string& opt) {
  LOG(INFO) << "OPT line: '" << opt << "'";

  const auto opts = SplitString(opt, " ");
  for (auto s : opts) {
    if (starts_with(s, "CRAM")) {
      LOG(INFO) << "       CRAM Requested by Remote Side.";
      // CRAM Support http://ftsc.org/docs/fts-1027.001
      auto last_dash = s.find_last_of('-');
      if (last_dash != string::npos) {
        // we really have CRAM-MD5
        auto challenge = s.substr(last_dash + 1);
        VLOG(1) << "        challenge: '" << challenge << "'";
        cram_.set_challenge_data(challenge);
        if (config_->cram_md5()) {
          auth_type_ = AuthType::CRAM_MD5;
        }
        else {
          LOG(INFO) << "       CRAM-MD5 disabled in net.ini; Using plain text passwords.";
        }
      }
    }
    else if (s == "CRC") {
      if (config_->crc()) {
        LOG(INFO) << "       Enabling CRC support";
        // If we support crc. Let's use it at receiving time now.
        crc_ = true;
      }
      else {
        LOG(INFO) << "       Not enabling CRC support (disabled in net.ini).";
      }
    }
    else {
      LOG(INFO) << "       Unknown OPT: '" << s << "'";
    }
  }
  return true;
}

bool BinkP::process_command(int16_t length, duration<double> d) {
  if (!conn_->is_open()) {
    LOG(INFO) << "       process_frames(returning false, connection is not open.)";
    return false;
  }
  const uint8_t command_id = conn_->read_uint8(d);
  VLOG(3) << "       process_frames(command_id: '" << static_cast<int>(command_id) << "')";

  // In the header, SIZE should be set to the total length of "Data string"
  // plus one for the octet to store the command number.
  // So we read one less than the original length as that
  // included the command number.  If the length is 1, we shouldn't read 0
  // and just let the log_line and command data be the empty string.  Argus likes
  // to not put data after M_EOB and M_OK commands.
  string s;
  if (length > 1) {
    s = conn_->receive(length - 1, d);
  }

  string log_line = s;
  if (command_id == BinkpCommands::M_PWD) {
	  log_line = string(12, '*');
  }
  VLOG(1) << "RECV:  " << BinkpCommands::command_id_to_name(command_id)
      << ": " << log_line;
  switch (command_id) {
  case BinkpCommands::M_NUL: {
    // TODO(rushfan): process these.
    if (starts_with(s, "OPT")) {
      s = s.substr(3);
      StringTrimBegin(&s);

      process_opt(s);
    } else if (starts_with(s, "SYS ")) {
      LOG(INFO) << "Remote Side: " << s.substr(4);
      remote_.set_system_name(s.substr(4));
    } else if (starts_with(s, "VER ")) {
      LOG(INFO) << "Remote Side: " << s.substr(4);
      remote_.set_version(s.substr(4));
    }
  } break;
  case BinkpCommands::M_ADR: {
    remote_.set_address_list(s);
    file_manager_.reset(new FileManager(config_->config().root_directory(), remote_.network()));
  } break;
  case BinkpCommands::M_OK: {
    ok_received_ = true;
  } break;
  case BinkpCommands::M_GET: {
    HandleFileGetRequest(s);
  } break;
  case BinkpCommands::M_GOT: {
    HandleFileGotRequest(s);
  } break;
  case BinkpCommands::M_EOB: {
    eob_received_ = true;
  } break;
  case BinkpCommands::M_PWD: {
    HandlePassword(s);
  } break;
  case BinkpCommands::M_FILE: {
    HandleFileRequest(s);
  } break;
  case BinkpCommands::M_ERR: {
    LOG(ERROR) << "RECV:  M_ERR: " << s;
    error_received_ = true;
  } break;
  default: {
    LOG(ERROR) << "       ** Unhandled Command: " << BinkpCommands::command_id_to_name(command_id) << ": " << s;
  } break;
  }
  return true;
}

bool BinkP::process_data(int16_t length, duration<double> d) {
  if (!conn_->is_open()) {
    return false;
  }
  string s = conn_->receive(length, d);
  LOG_IF(length != static_cast<int16_t>(s.size()), ERROR) 
      << "RECV:  DATA PACKET; ** unexpected size** len: " 
      << s.size()
      << "; expected: " << length
      << " duration:" << wwiv::core::to_string(d);
  if (!current_receive_file_) {
    LOG(ERROR) << "ERROR: Received M_DATA with no current file.";
    return false;
  }
  current_receive_file_->WriteChunk(s);
  if (current_receive_file_->length() >= current_receive_file_->expected_length()) {
    LOG(INFO) << "       file finished; bytes_received: " << current_receive_file_->length();

    string data_line = StringPrintf("%s %u %u",
        current_receive_file_->filename().c_str(),
        current_receive_file_->length(),
        current_receive_file_->timestamp());

    auto crc = current_receive_file_->crc();
    // If we want to use CRCs and we don't have a zero CRC.
    if (crc_ && crc != 0) {
      data_line += StringPrintf(" %08X", current_receive_file_->crc());
    }

    // Increment the nubmer of bytes received.
    bytes_received_ += current_receive_file_->length();

    // Close the current file, add the name to the list of received files.
    current_receive_file_->Close();

    // If we have a crc; check it.
    if (crc_ && crc != 0) {
      auto dir = config_->network_dir(remote_.network_name());
      File received_file(dir, current_receive_file_->filename());
      auto file_crc = crc32file(received_file.full_pathname());
      if (file_crc == 0) {
        LOG(ERROR) << "Error calculating CRC32 of: " << current_receive_file_->filename();
      } else {
        if (file_crc != current_receive_file_->crc()) {
          // TODO(rushfan): Once we're sure this works, make it mark the file bad.
          LOG(ERROR) << "Wrong CRC32 of: " << current_receive_file_->filename()
            << "; expected: " << std::hex << current_receive_file_->crc()
            << "; actual: " << std::hex << file_crc;
        }
      }
    }

    file_manager_->ReceiveFile(current_receive_file_->filename());

    // Delete the reference to this file and signal the other side we received it.
    current_receive_file_.release();
    send_command_packet(BinkpCommands::M_GOT, data_line);
  } else {
//    VLOG(1) << "       file still transferring; bytes_received: " << current_receive_file_->length()
//        << " and: " << current_receive_file_->expected_length() << " bytes expected.";
  }

  return true;
}

bool BinkP::process_frames(duration<double> d) {
  VLOG(3) << "       process_frames(" << wwiv::core::to_string(d) << ")";
  return process_frames([&]() -> bool { return false; }, d);
}

bool BinkP::process_frames(function<bool()> predicate, duration<double> d) {
  if (!conn_->is_open()) {
    return false;
  }
  try {
    while (!predicate()) {
      VLOG(3) << "       process_frames(pred)";
      uint16_t header = conn_->read_uint16(d);
      if (header & 0x8000) {
        if (!process_command(header & 0x7fff, d)) {
          // false return value means an error occurred.
          return false;
        }
      } else {
        // process data frame.
        // note: always use a timeout of 10s to process data since dropping bytes
        // causes real problems.
        if (!process_data(header & 0x7fff, seconds(10))) {
          // false return value mean san error occurred.
          return false;
        }
      }
    }
  } catch (const timeout_error& e) {
    VLOG(3) << "timeout in process_frames: " << e.what();
  }
  return true;
}

bool BinkP::send_command_packet(uint8_t command_id, const string& data) {
  if (!conn_->is_open()) {
    return false;
  }
  const std::string::size_type size = 3 + data.size(); /* header + command + data + null*/
  unique_ptr<char[]> packet(new char[size]);
  // Actual packet size parameter does not include the size parameter itself.
  // And for sending a commmand this will be 2 less than our actual packet size.
  uint16_t packet_length = static_cast<uint16_t>(data.size() + sizeof(uint8_t)) | 0x8000;
  uint8_t b0 = ((packet_length & 0xff00) >> 8) | 0x80;
  uint8_t b1 = packet_length & 0x00ff;

  char *p = packet.get();
  *p++ = b0;
  *p++ = b1;
  *p++ = command_id;
  memcpy(p, data.data(), data.size());

  conn_->send(packet.get(), size, seconds(3));
  if (command_id != BinkpCommands::M_PWD) {
    LOG(INFO) << "SEND:  " << BinkpCommands::command_id_to_name(command_id)
         << ": " << data;
  } else {
    // Mask the password.
    LOG(INFO) << "SEND:  " << BinkpCommands::command_id_to_name(command_id) 
        << ": " << string(8, '*');
  }
  return true;
}

bool BinkP::send_data_packet(const char* data, std::size_t packet_length) {
  if (!conn_->is_open()) {
    return false;
  }
  // for now assume everything fits within a single frame.
  unique_ptr<char[]> packet(new char[packet_length + 2]);
  packet_length &= 0x7fff;
  uint8_t b0 = ((packet_length & 0xff00) >> 8);
  uint8_t b1 = packet_length & 0x00ff;
  char *p = packet.get();
  *p++ = b0;
  *p++ = b1;
  memcpy(p, data, packet_length);

  conn_->send(packet.get(), packet_length + 2, seconds(10));
  VLOG(3) << "SEND:  data packet: packet_length: " << (int) packet_length;
  return true;
}

BinkState BinkP::ConnInit() {
  VLOG(1) << "STATE: ConnInit";
  process_frames(seconds(2));
  return BinkState::WAIT_CONN;
}

static string wwiv_version_string() {
  return StrCat(wwiv_version, beta_version);
}

static string wwiv_version_string_with_date() {
  return StrCat(wwiv_version, beta_version, " (", wwiv_date, ")");
}

BinkState BinkP::WaitConn() {
  VLOG(1) << "STATE: WaitConn";
  // Send initial CRAM command.
  if (side_ == BinkSide::ANSWERING) {
    if (config_->cram_md5()) {
      cram_.GenerateChallengeData();
      const string opt_cram = StrCat("OPT CRAM-MD5-", cram_.challenge_data());
      send_command_packet(BinkpCommands::M_NUL, opt_cram);
    }
  }
  send_command_packet(BinkpCommands::M_NUL, StrCat("WWIVVER ", wwiv_version_string_with_date()));
  send_command_packet(BinkpCommands::M_NUL, StrCat("SYS ", config_->system_name()));
  const string sysop_name_packet = StrCat("ZYZ ", config_->sysop_name());
  send_command_packet(BinkpCommands::M_NUL, sysop_name_packet);
  const string version_packet = StrCat("VER networkb/", wwiv_version_string(), " binkp/1.0");
  send_command_packet(BinkpCommands::M_NUL, version_packet);
  send_command_packet(BinkpCommands::M_NUL, "LOC Unknown");
  if (config_->crc()) {
    send_command_packet(BinkpCommands::M_NUL, "OPT CRC");
  }

  string network_addresses;
  if (side_ == BinkSide::ANSWERING) {
    // Present all addresses on answering side.
    for (const auto net : config_->networks().networks()) {    
      if (net.type == network_type_t::wwivnet) {
        string lower_case_network_name = net.name;
        StringLowerCase(&lower_case_network_name);		  
        send_command_packet(BinkpCommands::M_NUL,
            StringPrintf("WWIV @%u.%s", net.sysnum, lower_case_network_name.c_str()));
        if (!network_addresses.empty()) {
          network_addresses.push_back(' ');
        }
		    network_addresses += StringPrintf("20000:20000/%d@%s", net.sysnum, lower_case_network_name.c_str());
      } else if (config_->config().is_5xx_or_later()) {
        if (!network_addresses.empty()) {
          network_addresses.push_back(' ');
        }
        try {
          FidoAddress address(net.fido.fido_address);
          network_addresses += address.as_string();
        }
        catch (const bad_fidonet_address&) {
          LOG(WARNING) << "Bad FTN Address: '" << net.fido.fido_address
            << "' for network: '" << net.name << "'.";
        }
      }
    }
  } else {
    // Sending side: 
    const auto net = config_->networks()[config_->callout_network_name()];
    if (net.type == network_type_t::wwivnet) {
      // Present single primary WWIVnet address.
      send_command_packet(BinkpCommands::M_NUL,
        StringPrintf("WWIV @%u.%s", config_->callout_node_number(), config_->callout_network_name().c_str()));
      const string lower_case_network_name = ToStringLowerCase(net.name);
      network_addresses = StringPrintf("20000:20000/%d@%s", net.sysnum, lower_case_network_name.c_str());
    } else if (config_->config().is_5xx_or_later()) {
      try {
        // Present single FTN address.
        FidoAddress address(net.fido.fido_address);
        network_addresses = address.as_string();
      } catch (const bad_fidonet_address& e) {
          LOG(WARNING) << "Bad FTN Address: '" << net.fido.fido_address
            << "' for network: '" << net.name << "'.";
          // We should terminate here, so rethrow the exception after
          // letting the user know the network name that is borked.
          throw e;
      }

    }
  }
  send_command_packet(BinkpCommands::M_ADR, network_addresses);

  // Try to process any inbound frames before leaving this state.
  process_frames(milliseconds(100));
  return (side_ == BinkSide::ORIGINATING) ?
      BinkState::SEND_PASSWORD : BinkState::WAIT_ADDR;
}

BinkState BinkP::SendPasswd() {
  // This is on the sending side.
  const string network_name(remote_.network_name());
  VLOG(1) << "STATE: SendPasswd for network '" << network_name << "' for node: " << expected_remote_node_;
  Callout* callout = config_->callouts().at(network_name).get();
  string password = expected_password_for(callout, expected_remote_node_);
  VLOG(1) << "       sending password packet";
  switch (auth_type_) {
  case AuthType::CRAM_MD5:
  {
    string hashed_password = cram_.CreateHashedSecret(cram_.challenge_data(), password);
    string hashed_password_command = StrCat("CRAM-MD5-", hashed_password);
    send_command_packet(BinkpCommands::M_PWD, hashed_password_command);
  } break;
  case AuthType::PLAIN_TEXT:
    send_command_packet(BinkpCommands::M_PWD, password);
    break;
  }
  return BinkState::WAIT_ADDR;
}

BinkState BinkP::WaitAddr() {
  VLOG(1) << "STATE: WaitAddr";
  auto predicate = [&]() -> bool { return !remote_.address_list().empty(); };
  for (int i=0; i < 10; i++) {
    process_frames(predicate, seconds(1));
    if (!remote_.address_list().empty()) {
      return BinkState::AUTH_REMOTE;
    }
  }
  return BinkState::AUTH_REMOTE;
}

BinkState BinkP::PasswordAck() {
  // This is only on the answering side.
  // This should only happen in the originating side.
  if (side_ != BinkSide::ANSWERING) {
    LOG(ERROR) << "**** ERROR: WaitPwd Called on ORIGINATING side";
  }

  string expected_password;

  const string network_name = remote_.network_name();
  auto callout = config_->callouts().at(network_name).get();
  if (remote_.network().type == network_type_t::wwivnet) {
    // TODO(rushfan): we need to use the network name we matched not the one that config thinks.
    auto remote_node = remote_.wwivnet_node();
    LOG(INFO) << "       remote node: " << remote_node;

    expected_password = expected_password_for(callout, remote_node);
  } else if (remote_.network().type == network_type_t::ftn) {
    auto remote_node = remote_.ftn_address();
    expected_password = expected_password_for(callout, remote_node);
  }

  VLOG(1) << "STATE: PasswordAck";
  if (auth_type_ == AuthType::PLAIN_TEXT) {
    // VLOG(1) << "       PLAIN_TEXT expected_password = '" << expected_password << "'";
    if (remote_password_ == expected_password) {
      // Passwords match, send OK.
      send_command_packet(BinkpCommands::M_OK, "Passwords match; insecure session");
      // No need to wait for OK since we are the answering side, just move straight to
      // transfer files.
      return BinkState::TRANSFER_FILES;
    }
  } else if (auth_type_ == AuthType::CRAM_MD5) {
    VLOG(1) << "       CRAM_MD5 expected_password = '" << expected_password << "'";
    VLOG(1) << "       received password: " << remote_password_;
    if (cram_.ValidatePassword(cram_.challenge_data(), expected_password, remote_password_)) {
      // Passwords match, send OK.
      send_command_packet(BinkpCommands::M_OK, "Passwords match; secure session.");
      // No need to wait for OK since we are the answering side, just move straight to
      // transfer files.
      return BinkState::TRANSFER_FILES;
    }
  }

  // Passwords do not match, send error.
  send_command_packet(BinkpCommands::M_ERR, "Incorrect password received.  Please check your configuration.");
  // Log it if we're in debug logging mode.
  VLOG(1) << "Password doesn't match.  Received '" << remote_password_
          << "' expected '", expected_password, "'.";
  return BinkState::DONE;
}

BinkState BinkP::WaitPwd() {
  // This should only happen in the originating side.
  if (side_ != BinkSide::ANSWERING) {
    LOG(ERROR) << "**** ERROR: WaitPwd Called on ORIGINATING side";
  }
  VLOG(1) << "STATE: WaitPwd";
  auto predicate = [&]() -> bool { return !remote_password_.empty(); };
  for (int i=0; i < 30; i++) {
    process_frames(predicate, seconds(1));
    if (predicate()) {
      break;
    }
  }

  return BinkState::PASSWORD_ACK;
}

BinkState BinkP::WaitOk() {
  // TODO(rushfan): add proper timeout to wait for OK.
  VLOG(1) << "STATE: WaitOk";
  for (int i=0; i < 30; i++) {
    process_frames([&]() -> bool { return ok_received_; }, seconds(1));
    if (ok_received_) {
      return BinkState::TRANSFER_FILES;
    }
  }

  LOG(ERROR) << "       after WaitOk: M_OK never received.";
  send_command_packet(BinkpCommands::M_ERR, "M_OK never received. Timed out waiting for it.");
  return BinkState::DONE;
}

BinkState BinkP::IfSecure() {
  VLOG(1) << "STATE: IfSecure";
  // Wait for OK if we sent a password.
  // Log an unsecure session of there is no password.
  return BinkState::WAIT_OK;
}

BinkState BinkP::AuthRemote() {
  VLOG(1) << "STATE: AuthRemote";
  // Check that the address matches who we thought we called.
  VLOG(1) << "       remote address_list: " << remote_.address_list();
  const string network_name(remote_.network_name());
  if (side_ == BinkSide::ANSWERING) {
    if (!contains(config_->callouts(), network_name)) {
      // We don't have a callout.net entry for this caller. Fail the connection
      send_command_packet(BinkpCommands::M_ERR, 
          StrCat("Error (NETWORKB-0003): Unable to find callout.net for: ", network_name));
      return BinkState::FATAL_ERROR;
    }

    const net_call_out_rec* callout_record = nullptr;
    if (remote_.network().type == network_type_t::wwivnet) {
      auto caller = remote_.wwivnet_node();
      LOG(INFO) << "       remote_network_name: " << network_name << "; caller_node: " << caller;
      callout_record = config_->callouts().at(network_name)->net_call_out_for(caller);
      if (callout_record == nullptr) {
        // We don't have a callout.net entry for this caller. Fail the connection
        send_command_packet(BinkpCommands::M_ERR,
          StrCat("Error (NETWORKB-0002): Unable to find caller in WWIVnet callout.net. caller: ", caller));
        return BinkState::FATAL_ERROR;
      }
    } else {
      auto caller = remote_.ftn_address();
      LOG(INFO) << "       remote_network_name: " << network_name << "; caller_address: " << caller;
      callout_record = config_->callouts().at(network_name)->net_call_out_for(caller);
      if (callout_record == nullptr) {
        // We don't have a callout.net entry for this caller. Fail the connection
        send_command_packet(BinkpCommands::M_ERR,
          StrCat("Error (NETWORKB-0002): Unable to find FTN address in remote list. caller: ", caller));
        return BinkState::FATAL_ERROR;
      }
    }
    return BinkState::WAIT_PWD;
  }

  VLOG(1) << "       expected_ftn: '" << expected_remote_node_ << "'";
  if (remote_.address_list().find(expected_remote_node_) != string::npos) {
    return (side_ == BinkSide::ORIGINATING) ?
      BinkState::IF_SECURE : BinkState::WAIT_PWD;
  } else {
    send_command_packet(BinkpCommands::M_ERR,
      StrCat("Error (NETWORKB-0001): Unexpected Addresses: '", remote_.address_list(),
        "'; expected: '", expected_remote_node_, "'"));
    return BinkState::FATAL_ERROR;
  }
}

BinkState BinkP::TransferFiles() {
  if (remote_.network().type == network_type_t::wwivnet) {
    VLOG(1) << "STATE: TransferFiles to node: " << remote_.wwivnet_node();
  } else {
    VLOG(1) << "STATE: TransferFiles to node: " << remote_.ftn_address();
  }
  // Quickly let the inbound event loop percolate.
  process_frames(milliseconds(500));
  const auto list = file_manager_->CreateTransferFileList(remote_);
  for (auto file : list) {
    SendFilePacket(file);
  }

  VLOG(1) << "STATE: After SendFilePacket for all files.";
  // Quickly let the inbound event loop percolate.
  for (int i=0; i < 5; i++) {
    process_frames(milliseconds(500));
  }

  // TODO(rushfan): Should this be in a new state?
  if (files_to_send_.empty()) {
    // All files are sent, let's let the remote know we are done.
    VLOG(1) << "       Sending EOB";
    // Kinda a hack, but trying to send a 3 byte packet was stalling on Windows.  Making it larger makes
    // it send (yes, even with TCP_NODELAY set).
    send_command_packet(BinkpCommands::M_EOB, "All files to send have been sent. Thank you.");
    process_frames(seconds(1));
  } else {
    VLOG(1) << "       files_to_send_ is not empty, Not sending EOB";
    string files;
    for (const auto& f : files_to_send_) {
      files += f.first;
      files += " ";
    }
    VLOG(2) << "Files: " << files;
  }
  return BinkState::WAIT_EOB;
}

BinkState BinkP::Unknown() {
  VLOG(1) << "STATE: Unknown";
  int count = 0;
  auto predicate = [&]() -> bool { return count++ > 4; };
  process_frames(predicate, seconds(3));
  return BinkState::DONE;
}

BinkState BinkP::FatalError() {
  LOG(ERROR) << "STATE: FatalError";
  int count = 0;
  auto predicate = [&]() -> bool { return count++ > 4; };
  process_frames(predicate, seconds(3));
  return BinkState::DONE;
}

BinkState BinkP::WaitEob() {
  VLOG(1) << "STATE: WaitEob: ENTERING eob_received: " << boolalpha << eob_received_;
  if (eob_received_) {
    // If we've already received an EOB, don't process_frames or do anything else. 
    // We're done.
    return BinkState::DONE;
  }

  const int eob_retries = 12;
  const int eob_wait_seconds = 5;
  for (int count=1; count < eob_retries; count++) {
    // Loop for up to one minute swaiting for an EOB before exiting.
    try {
      process_frames([&]() -> bool { return eob_received_; }, seconds(eob_wait_seconds));
      if (eob_received_) {
        return BinkState::DONE;
      }
      LOG(INFO) << "       WaitEob: still waiting for M_EOB to be received. will wait up to "
          << (eob_retries * eob_wait_seconds) << " seconds.";
    } catch (const timeout_error& e) {
      LOG(ERROR) << "       WaitEob: ERROR while waiting for more data: " << e.what();
    }
  }
  return BinkState::DONE;
}

bool BinkP::SendFilePacket(TransferFile* file) {
  const string filename(file->filename());
  LOG(INFO) << "       SendFilePacket: " << filename;
  files_to_send_[filename] = unique_ptr<TransferFile>(file);
  send_command_packet(BinkpCommands::M_FILE, file->as_packet_data(0));
  process_frames(seconds(2));

  // file* may not be viable anymore if it was already send.
  if (contains(files_to_send_, filename)) {
    // We have the file still to send.
    SendFileData(file);
  }
  return true;
}

bool BinkP::SendFileData(TransferFile* file) {
  LOG(INFO) << "       SendFileData: " << file->filename();
  long file_length = file->file_size();
  const int chunk_size = 16384; // This is 1<<14.  The max per spec is (1 << 15) - 1
  unique_ptr<char[]> chunk(new char[chunk_size]);
  for (long start = 0; start < file_length; start+=chunk_size) {
    int size = min<int>(chunk_size, file_length - start);
    if (!file->GetChunk(chunk.get(), start, size)) {
      // Bad chunk. Abort
    }
    send_data_packet(chunk.get(), size);
    // sending multichunk files was not reliable.  check after each frame if we have
    // an inbound command.
    process_frames(seconds(1));
  }
  return true;
}

bool BinkP::HandlePassword(const string& password_line) {
  VLOG(1) << "        HandlePassword: ";
  VLOG(2) << "        password_line: " << password_line;
  if (!starts_with(password_line, "CRAM")) {
    LOG(INFO) << "        HandlePassword: Received Plain text password";
    auth_type_ = AuthType::PLAIN_TEXT;
    remote_password_ = password_line;
    return true;
  }

  static const string CRAM_MD5_PREFIX = "CRAM-MD5-";
  if (!starts_with(password_line, CRAM_MD5_PREFIX)) {
    send_command_packet(BinkpCommands::M_ERR,
        "CRAM authentication required, no common hash function");
    return false;
  }
  string hashed_password = password_line.substr(CRAM_MD5_PREFIX.size());
  LOG(INFO) << "        HandlePassword: Received CRAM-MD5 hashed password";
  auth_type_ = AuthType::CRAM_MD5;
  remote_password_ = hashed_password;
  return true;
}

// M_FILE received.
bool BinkP::HandleFileRequest(const string& request_line) {
  VLOG(1) << "       HandleFileRequest; request_line: " << request_line;
  ReceiveFile* old_file = current_receive_file_.release();
  if (old_file != nullptr) {
    LOG(ERROR) << "** ERROR: Got HandleFileRequest while still having an open receive file!";
  }
  string filename;
  long expected_length;
  time_t timestamp;
  long starting_offset = 0;
  uint32_t crc = 0;
  if (!ParseFileRequestLine(request_line,
      &filename, 
      &expected_length,
      &timestamp, 
      &starting_offset,
      &crc)) {
    return false;
  }
  const auto net = remote_.network_name();
  auto *p = new ReceiveFile(received_transfer_file_factory_(net, filename),
    filename,
    expected_length,
    timestamp,
    crc);
  current_receive_file_.reset(p);
  return true;
}

bool BinkP::HandleFileGetRequest(const string& request_line) {
  LOG(INFO) << "       HandleFileGetRequest: request_line: [" << request_line << "]"; 
  vector<string> s = SplitString(request_line, " ");
  const auto filename = s.at(0);
  auto length = to_number<long>(s.at(1));
  auto timestamp = to_number<time_t>(s.at(2));
  long offset = 0;
  if (s.size() >= 4) {
    offset = to_number<long>(s.at(3));
  }

  auto iter = files_to_send_.find(filename);
  if (iter == end(files_to_send_)) {
    LOG(ERROR) << "File not found: " << filename;
    return false;
  }
  return SendFileData(iter->second.get());
  // File was sent but wait until we receive M_GOT before we remove it from the list.
}

bool BinkP::HandleFileGotRequest(const string& request_line) {
  LOG(INFO) << "       HandleFileGotRequest: request_line: [" << request_line << "]"; 
  vector<string> s = SplitString(request_line, " ");
  const string filename = s.at(0);
  int length = to_number<int>(s.at(1));

  auto iter = files_to_send_.find(filename);
  if (iter == end(files_to_send_)) {
    LOG(ERROR) << "File not found: " << filename;
    return false;
  }

  TransferFile* file = iter->second.get();
  // Increment the number of bytes sent.
  // Also don't increment with -1 if there's an error with the file.
  bytes_sent_ += max(0, file->file_size());
  
  if (length != file->file_size()) {
    LOG(ERROR) << "NON-FATAL ERROR: Size didn't match M_GOT. Please log a bug. M_GOT: "
      << length << "; file_size: " << file->file_size();
  }

  // This is a file that we sent.
  if (!file->Delete()) {
    LOG(ERROR) << "       *** UNABLE TO DELETE FILE: " << file->filename(); 
  }
  files_to_send_.erase(iter);
  return true;
}

void BinkP::Run() {
  VLOG(1) << "STATE: Run(): side:" << static_cast<int>(side_);
  BinkState state = (side_ == BinkSide::ORIGINATING) ? BinkState::CONN_INIT : BinkState::WAIT_CONN;
  auto start_time = system_clock::now();
  try {
    bool done = false;
    while (!done) {
      switch (state) {
      case BinkState::CONN_INIT:
        state = ConnInit();
        break;
      case BinkState::WAIT_CONN:
        state = WaitConn();
        break;
      case BinkState::SEND_PASSWORD:
        state = SendPasswd();
        break;
      case BinkState::WAIT_ADDR:
        state = WaitAddr();
        break;
      case BinkState::AUTH_REMOTE:
        state = AuthRemote();
        break;
      case BinkState::IF_SECURE:
        state = IfSecure();
        break;
      case BinkState::WAIT_PWD:
        state = WaitPwd();
        break;
      case BinkState::PASSWORD_ACK:
        state = PasswordAck();
        break;
      case BinkState::WAIT_OK:
        state = WaitOk();
        break;
      case BinkState::TRANSFER_FILES:
        state = TransferFiles();
        break;
      case BinkState::WAIT_EOB:
        state = WaitEob();
        break;
      case BinkState::UNKNOWN:
        state = Unknown();
        break;
      case BinkState::FATAL_ERROR:
        state = FatalError();
        break;
      case BinkState::DONE:
        VLOG(1) << "STATE: Done.";
        conn_->close();
        done = true;
        break;
      }

      if (error_received_) {
        LOG(ERROR) << "STATE: Error Received.";
        done = true;
      }
      process_frames(milliseconds(100));
    }
  } catch (const socket_closed_error& e) {
    // The other end closed the socket before we did.
    LOG(INFO) << "       connection was closed by the other side. details: " << e.what();
  } catch (const socket_error& e) {
    LOG(ERROR) << "STATE: BinkP::RunOriginatingLoop() socket_error: " << e.what();
  }

  auto end_time = system_clock::now();
  if (remote_.network().type == network_type_t::wwivnet) {
    // Handle WWIVnet inbound files.
    if (file_manager_) {
      // file_manager_ is null in some tests (BinkpTest).
      file_manager_->rename_wwivnet_pending_files();
      if (!config_->skip_net()) {
        process_network_files();
      }
    }

    // Log to net.log
    auto sec = duration_cast<seconds>(end_time - start_time);
    // Update WWIVnet net.log and contact.net for WWIVnet connections.
    NetworkSide network_log_side = (side_ == BinkSide::ORIGINATING) ? NetworkSide::TO : NetworkSide::FROM;
    NetworkLog net_log(config_->gfiles_directory());
    net_log.Log(system_clock::to_time_t(start_time), 
      network_log_side,
      remote_.wwivnet_node(), 
      bytes_sent_, 
      bytes_received_, 
      sec, remote_.network_name());

    // Update contact.net
    Contact c(config_->network(remote_.network_name()), true);
    if (error_received_) {
      c.add_failure(remote_.wwivnet_node(), system_clock::to_time_t(start_time));
    } else {
      c.add_connect(remote_.wwivnet_node(), system_clock::to_time_t(start_time),
        bytes_sent_, bytes_received_);
    }
  } else {
    // Handle WWIVnet inbound files.
    if (file_manager_) {
      // file_manager_ is null in some tests (BinkpTest).
      file_manager_->rename_ftn_pending_files();
      if (!config_->skip_net()) {
        const int network_number = config_->networks().network_number(remote_.network_name());
        // TODO(rushfan): May need binary path one day
        System(config_->config().root_directory(),
               StrCat("networkc .", network_number, " --v=", config_->verbose()));
      }
    }


  }
}

static bool checkup2(const time_t tFileTime, string dir, string filename) {
  File file(dir, filename);

  if (file.Open(File::modeReadOnly)) {
    time_t tNewFileTime = file.last_write_time();
    file.Close();
    return (tNewFileTime > (tFileTime + 2));
  }
  return true;
}

static bool need_network3(const string& dir, int network_version) {
  if (!File::Exists(dir, BBSLIST_NET)) {
    return false;
  }
  if (!File::Exists(dir, CONNECT_NET)) {
    return false;
  }
  if (!File::Exists(dir, CALLOUT_NET)) {
    return false;
  }

  if (network_version != wwiv_net_version) {
    // always need network3 if the versions do not match.
    LOG(INFO) << "Need to run network3 since current network_version: "
      << network_version << " != our network_version: " << wwiv_net_version;
    return true;
  }
  File bbsdataNet(dir, BBSDATA_NET);
  if (!bbsdataNet.Open(File::modeReadOnly)) {
    return false;
  }

  time_t bbsdata_time = bbsdataNet.last_write_time();
  bbsdataNet.Close();

  return checkup2(bbsdata_time, dir, BBSLIST_NET)
    || checkup2(bbsdata_time, dir, CONNECT_NET)
    || checkup2(bbsdata_time, dir, CALLOUT_NET);
}


void BinkP::process_network_files() const {
  const string network_name = remote_.network_name();
  VLOG(1) << "STATE: process_network_files for network: " << network_name;
  int network_number = config_->networks().network_number(network_name);
  if (network_number == wwiv::sdk::Networks::npos) {
    return;
  }
  // TODO(rushfan): May need binary path one day
  System(config_->config().root_directory(),
         StrCat("networkc .", network_number, " --v=", config_->verbose()));
}

bool ParseFileRequestLine(const string& request_line,
        string* filename,
        long* length,
        time_t* timestamp,
        long* offset,
        uint32_t* crc) {
  vector<string> s = SplitString(request_line, " ");
  if (s.size() < 3) {
    LOG(ERROR) << "ERROR: INVALID request_line: "<< request_line
         << "; had < 3 parts.  # parts: " << s.size();
    return false;
  }
  *filename = s.at(0);
  *length = to_number<long>(s.at(1));
  *timestamp = to_number<time_t>(s.at(2));
  *offset = 0;
  if (s.size() >= 4) {
    *offset = to_number<long>(s.at(3));
  }
  if (s.size() >= 5) {
    *crc = to_number<uint32_t>(s.at(4), 16);
  }
  return true;
}
  
}  // namespace net
} // namespace wwiv
