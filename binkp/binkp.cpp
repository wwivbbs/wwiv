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
#include "binkp/binkp.h"

#include "binkp/binkp_commands.h"
#include "binkp/binkp_config.h"
#include "binkp/cram.h"
#include "binkp/file_manager.h"
#include "binkp/net_log.h"
#include "binkp/transfer_file.h"
#include "core/connection.h"
#include "core/crc32.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/os.h"
#include "core/socket_exceptions.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/printf.h"
#include "sdk/fido/fido_address.h"
#include "sdk/net/callout.h"
#include "sdk/net/contact.h"
#include <algorithm>
#include <chrono>
#include <cstring>
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

using namespace std::chrono;
using namespace wwiv::core;
using namespace wwiv::net;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

namespace wwiv::net {

static int System(const std::string& bbsdir, const std::string& cmd) {
  const auto path = FilePath(bbsdir, cmd).string();

  const auto err = system(path.c_str());
  VLOG(1) << "       executed: '" << path << "' with an error code: " << err;
  return err;
}

std::string expected_password_for(const net_call_out_rec* con) {
  if (con != nullptr && !con->session_password.empty()) {
    // If the password is not empty string.
    return con->session_password;
  }
  // return default password of "-"
  return "-";
}

BinkP::BinkP(Connection* conn, BinkConfig* config, BinkSide side,
             const std::string& expected_remote_node,
             received_transfer_file_factory_t& received_transfer_file_factory)
    : config_(config), conn_(conn), side_(side), expected_remote_node_(expected_remote_node),
      received_transfer_file_factory_(received_transfer_file_factory),
      remote_(config, side_ == BinkSide::ANSWERING, expected_remote_node) {
  if (side_ == BinkSide::ORIGINATING) {
    crc_ = config_->crc();
  }
}

BinkP::~BinkP() { files_to_send_.clear(); }

bool BinkP::process_opt(const std::string& opt) {
  VLOG(1) << "OPT line: '" << opt << "'";

  const auto opts = SplitString(opt, " ");
  for (const auto& s : opts) {
    if (starts_with(s, "CRAM")) {
      LOG(INFO) << "       CRAM Requested by Remote Side.";
      if (const auto last_dash = s.find_last_of('-'); last_dash != std::string::npos) {
        // we really have CRAM-MD5
        auto challenge = s.substr(last_dash + 1);
        VLOG(1) << "        challenge: '" << challenge << "'";
        cram_.set_challenge_data(challenge);
        if (config_->cram_md5()) {
          auth_type_ = AuthType::CRAM_MD5;
        } else {
          LOG(INFO) << "       CRAM-MD5 disabled in net.ini; Using plain text passwords.";
        }
      }
    } else if (s == "CRC") {
      if (config_->crc()) {
        LOG(INFO) << "       Enabling CRC support";
        // If we support crc. Let's use it at receiving time now.
        crc_ = true;
      } else {
        LOG(INFO) << "       Not enabling CRC support (disabled in net.ini).";
      }
    } else {
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
  const auto command_id = conn_->read_uint8(d);
  VLOG(3) << "       process_frames(command_id: '" << static_cast<int>(command_id) << "')";

  // In the header, SIZE should be set to the total length of "Data string"
  // plus one for the octet to store the command number.
  // So we read one less than the original length as that
  // included the command number.  If the length is 1, we shouldn't read 0
  // and just let the log_line and command data be the empty string.  Argus likes
  // to not put data after M_EOB and M_OK commands.
  std::string s;
  if (length > 1) {
    s = conn_->receive(length - 1, d);
  }

  std::string log_line = s;
  if (command_id == BinkpCommands::M_PWD) {
    log_line = std::string(12, '*');
  }
  LOG(INFO) << "RECV:  " << BinkpCommands::command_id_to_name(command_id) << ": " << log_line;
  switch (command_id) {
  case BinkpCommands::M_NUL: {
    // TODO(rushfan): process these.
    if (starts_with(s, "OPT")) {
      s = s.substr(3);
      StringTrimBegin(&s);

      process_opt(s);
    } else if (starts_with(s, "SYS ")) {
      remote_.set_system_name(s.substr(4));
    } else if (starts_with(s, "VER ")) {
      remote_.set_version(s.substr(4));
    }
  } break;
  case BinkpCommands::M_ADR: {
    remote_.set_address_list(s);
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
    LOG(ERROR) << "       ** Unhandled Command: " << BinkpCommands::command_id_to_name(command_id)
               << ": " << s;
  } break;
  }
  return true;
}

bool BinkP::process_data(int16_t length, duration<double> d) {
  if (!conn_->is_open()) {
    return false;
  }
  const auto s = conn_->receive(length, d);
  LOG_IF(length != static_cast<int16_t>(s.size()), ERROR)
      << "RECV:  DATA PACKET; ** unexpected size** len: " << s.size() << "; expected: " << length
      << " duration:" << wwiv::core::to_string(d);
  if (!current_receive_file_) {
    LOG(ERROR) << "ERROR: Received M_DATA with no current file.";
    return false;
  }
  current_receive_file_->WriteChunk(s);
  if (current_receive_file_->length() >= current_receive_file_->expected_length()) {
    LOG(INFO) << "       file finished; bytes_received: " << current_receive_file_->length();

    auto data_line =
        fmt::format("{} {} {}", current_receive_file_->filename(), current_receive_file_->length(),
                    current_receive_file_->timestamp());

    const auto crc = current_receive_file_->crc();
    // If we want to use CRCs and we don't have a zero CRC.
    if (crc_ && crc != 0) {
      data_line += fmt::sprintf(" %08X", current_receive_file_->crc());
    }

    // Increment the number of bytes received.
    bytes_received_ += current_receive_file_->length();

    // Close the current file, add the name to the list of received files.
    if (!current_receive_file_->Close()) {
      LOG(ERROR) << "Failed to close file: " << current_receive_file_->filename();
    }

    // If we have a crc; check it.
    if (crc_ && crc != 0) {
      const auto rdir = config_->receive_dir(remote_.network_name());
      const auto dir = File::absolute(config_->config().root_directory(), rdir);
      const File received_file(FilePath(dir, current_receive_file_->filename()));
      if (const auto file_crc = crc32file(received_file.path()); file_crc == 0) {
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
    current_receive_file_.reset();
    send_command_packet(BinkpCommands::M_GOT, data_line);
  } else {
    //    VLOG(3) << "       file still transferring; bytes_received: " <<
    //    current_receive_file_->length()
    //        << " and: " << current_receive_file_->expected_length() << " bytes expected.";
  }

  return true;
}

bool BinkP::process_frames(duration<double> d) {
  VLOG(3) << "       process_frames(" << wwiv::core::to_string(d) << ")";
  return process_frames([&]() -> bool { return false; }, d);
}

bool BinkP::process_frames(const std::function<bool()>& predicate, duration<double> d) {
  if (!conn_->is_open()) {
    return false;
  }
  try {
    while (!predicate()) {
      VLOG(3) << "       process_frames(pred)";
      if (const auto header = conn_->read_uint16(d); header & 0x8000) {
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

bool BinkP::send_command_packet(uint8_t command_id, const std::string& data) {
  if (!conn_->is_open()) {
    return false;
  }
  const auto size = 3 + size_int(data); /* header + command + data + null*/
  const auto packet = std::make_unique<uint8_t[]>(size);
  // Actual packet size parameter does not include the size parameter itself.
  // And for sending a command this will be 2 less than our actual packet size.
  const auto packet_length = static_cast<uint16_t>(data.size() + sizeof(uint8_t)) | 0x8000;
  const uint8_t b0 = ((packet_length & 0xff00) >> 8) | 0x80;
  const uint8_t b1 = packet_length & 0x00ff;

  auto* p = packet.get();
  *p++ = b0;
  *p++ = b1;
  *p++ = command_id;
  memcpy(p, data.data(), data.size());

  conn_->send(packet.get(), size, seconds(3));
  if (command_id != BinkpCommands::M_PWD) {
    LOG(INFO) << "SEND:  " << BinkpCommands::command_id_to_name(command_id) << ": " << data;
  } else {
    // Mask the password.
    LOG(INFO) << "SEND:  " << BinkpCommands::command_id_to_name(command_id) << ": "
              << std::string(8, '*');
  }
  return true;
}

bool BinkP::send_data_packet(const char* data, int packet_length) {
  if (!conn_->is_open()) {
    return false;
  }
  // for now assume everything fits within a single frame.
  const std::unique_ptr<char[]> packet(new char[packet_length + 2]);
  packet_length &= 0x7fff;
  uint8_t b0 = ((packet_length & 0xff00) >> 8);
  uint8_t b1 = packet_length & 0x00ff;
  auto* p = packet.get();
  *p++ = b0;
  *p++ = b1;
  memcpy(p, data, packet_length);

  conn_->send(packet.get(), packet_length + 2, seconds(10));
  VLOG(3) << "SEND:  data packet: packet_length: " << packet_length;
  return true;
}

BinkState BinkP::ConnInit() {
  VLOG(1) << "STATE: ConnInit";
  process_frames(seconds(2));
  return BinkState::WAIT_CONN;
}

static std::string wwiv_version_string() { return full_version(); }

static std::string wwiv_version_string_with_date() {
  return fmt::format("{} ({})", full_version(), wwiv_compile_datetime());
}

BinkState BinkP::WaitConn() {
  VLOG(1) << "STATE: WaitConn";
  // Send initial CRAM command.
  if (side_ == BinkSide::ANSWERING) {
    if (config_->cram_md5()) {
      cram_.GenerateChallengeData();
      const auto opt_cram = StrCat("OPT CRAM-MD5-", cram_.challenge_data());
      send_command_packet(BinkpCommands::M_NUL, opt_cram);
    }
  }
  send_command_packet(BinkpCommands::M_NUL, StrCat("WWIVVER ", wwiv_version_string_with_date()));
  send_command_packet(BinkpCommands::M_NUL, StrCat("SYS ", config_->system_name()));
  const auto sysop_name_packet = StrCat("ZYZ ", config_->sysop_name());
  send_command_packet(BinkpCommands::M_NUL, sysop_name_packet);
  const auto version_packet = StrCat("VER networkb/", wwiv_version_string(), " binkp/1.0");
  send_command_packet(BinkpCommands::M_NUL, version_packet);
  send_command_packet(BinkpCommands::M_NUL, "LOC Unknown");
  if (config_->crc()) {
    send_command_packet(BinkpCommands::M_NUL, "OPT CRC");
  }

  std::string network_addresses;
  if (side_ == BinkSide::ANSWERING) {
    // Present all addresses on answering side.
    for (const auto& net : config_->networks().networks()) {
      if (net.type == network_type_t::wwivnet) {
        std::string lower_case_network_name = net.name;
        StringLowerCase(&lower_case_network_name);
        send_command_packet(BinkpCommands::M_NUL, fmt::format("WWIV @{}.{}", net.sysnum,
                                                               lower_case_network_name));
        if (!network_addresses.empty()) {
          network_addresses.push_back(' ');
        }
        network_addresses += fmt::format("20000:20000/{}@{}", net.sysnum, lower_case_network_name);
      } else if (config_->config().is_5xx_or_later()) {
        if (!network_addresses.empty()) {
          network_addresses.push_back(' ');
        }
        try {
          FidoAddress address(net.fido.fido_address);
          network_addresses += address.as_string(true);
        } catch (const bad_fidonet_address&) {
          LOG(WARNING) << "Bad FTN Address: '" << net.fido.fido_address << "' for network: '"
                       << net.name << "'.";
        }
      }
    }
  } else {
    // Sending side:
    const auto& net = config_->networks()[config_->callout_network_name()];
    if (net.type == network_type_t::wwivnet) {
      // Present single primary WWIVnet address.
      send_command_packet(BinkpCommands::M_NUL,
                          fmt::format("WWIV @{}.{}", config_->callout_node_number(),
                                      config_->callout_network_name()));
      const auto lower_case_network_name = ToStringLowerCase(net.name);
      network_addresses = fmt::format("20000:20000/{}@{}", net.sysnum, lower_case_network_name);
    } else if (config_->config().is_5xx_or_later()) {
      // We don't support FTN on 4.x
      try {
        // Present single FTN address.
        const FidoAddress address(net.fido.fido_address);
        network_addresses = address.as_string(true);
      } catch (const bad_fidonet_address& e) {
        LOG(WARNING) << "Bad FTN Address: '" << net.fido.fido_address << "' for network: '"
                     << net.name << "'." << "; exception: " << e.what();
        // We should terminate here, so rethrow the exception after
        // letting the user know the network name that is borked.
        throw;
      }
    }
  }
  send_command_packet(BinkpCommands::M_ADR, network_addresses);

  // Try to process any inbound frames before leaving this state.
  process_frames(milliseconds(100));
  return BinkState::WAIT_ADDR;
}

BinkState BinkP::SendPasswd() {
  // This is on the sending side.
  VLOG(1) << "STATE: SendPasswd for network '" << remote_.network_name()
          << "' for node: " << expected_remote_node_;
  const auto* callout = at(config_->callouts(), remote_.domain()).get();
  const auto password = expected_password_for(callout, expected_remote_node_);
  VLOG(1) << "       sending password packet for node: " << expected_remote_node_;
  switch (auth_type_) {
  case AuthType::CRAM_MD5: {
    const auto hashed_password = cram_.CreateHashedSecret(cram_.challenge_data(), password);
    const auto hashed_password_command = StrCat("CRAM-MD5-", hashed_password);
    send_command_packet(BinkpCommands::M_PWD, hashed_password_command);
  } break;
  case AuthType::PLAIN_TEXT:
    send_command_packet(BinkpCommands::M_PWD, password);
    break;
  }
  return BinkState::AUTH_REMOTE;
}

BinkState BinkP::WaitAddr() {
  VLOG(1) << "STATE: WaitAddr";
  auto predicate = [&]() -> bool { return !remote_.address_list().empty(); };
  for (auto i = 0; i < 10; i++) {
    process_frames(predicate, seconds(1));
    if (!remote_.address_list().empty()) {
      break;
    }
  }
  return side_ == BinkSide::ORIGINATING ? BinkState::SEND_PASSWORD : BinkState::AUTH_REMOTE;
}

bool BinkP::CheckPassword(const FidoAddress& address) {
  VLOG(1) << "       CheckPassword: " << address;
  const auto* callout = at(config_->callouts(), address.domain()).get();
  const auto* ncn = callout->net_call_out_for(address.as_string(true));
  const auto expected_password = expected_password_for(ncn);
  if (auth_type_ == AuthType::PLAIN_TEXT) {
    VLOG(2) << "       PLAIN_TEXT expected_password = '" << expected_password << "'";
    if (remote_password_ == expected_password) {
      return true;
    }
  } else if (auth_type_ == AuthType::CRAM_MD5) {
    VLOG(1) << "       CRAM_MD5 expected_password = '" << expected_password << "'";
    VLOG(1) << "       received password: " << remote_password_;
    if (cram_.ValidatePassword(cram_.challenge_data(), expected_password, remote_password_)) {
      return true;
    }
  }
  VLOG(1) << "Password doesn't match.  Received '" << remote_password_ << "' expected '"
          << expected_password << "'.";
  return false;
}

BinkState BinkP::PasswordAck() {
  VLOG(1) << "STATE: PasswordAck";
  // This is only on the answering side.
  // This should only happen in the originating side.
  if (side_ != BinkSide::ANSWERING) {
    LOG(ERROR) << "**** ERROR: WaitPwd Called on ORIGINATING side";
  }

  if (remote_.ftn_addresses().empty()) {
    LOG(ERROR) << "Unable to parse addresses.";
    send_command_packet(BinkpCommands::M_ERR, "Unable to find common address to validate password.");
    return BinkState::DONE;
  }

  for (const auto& a : remote_.ftn_addresses()) {
    VLOG(1) << "       Checking password for address: " << a;
    if (!CheckPassword(a)) {
      // Passwords do not match, send error.
      send_command_packet(BinkpCommands::M_ERR,
                          "Incorrect password received.  Please check your configuration.");
      return BinkState::DONE;
    }
  }

  if (auth_type_ == AuthType::CRAM_MD5) {
    send_command_packet(BinkpCommands::M_OK, "Passwords match; secure session.");
    // No need to wait for OK since we are the answering side.
    return BinkState::TRANSFER_FILES;
  }
  // auth_type_ == AuthType::PLAIN_TEXT
  // VLOG(3) << "       PLAIN_TEXT expected_password = '" << expected_password << "'";
  // No need to wait for OK since we are the answering side.
  send_command_packet(BinkpCommands::M_OK, "Passwords match; insecure session");
  return BinkState::TRANSFER_FILES;
}

BinkState BinkP::WaitPwd() {
  // This should only happen in the originating side.
  if (side_ != BinkSide::ANSWERING) {
    LOG(ERROR) << "**** ERROR: WaitPwd Called on ORIGINATING side";
  }
  VLOG(1) << "STATE: WaitPwd";
  auto predicate = [&]() -> bool { return !remote_password_.empty(); };
  for (auto i = 0; i < 30; i++) {
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
  for (int i = 0; i < 30; i++) {
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
  // Log an insecure session of there is no password.
  return BinkState::WAIT_OK;
}

BinkState BinkP::AuthRemoteAnswering() {
  VLOG(1) << "       AuthRemoteAnswering";

  std::set<FidoAddress> known_addresses;
  for (const auto& kv : config_->address_pw_map) {
    known_addresses.insert(kv.first);
  }

  const auto addrs = ftn_addresses_from_address_list(remote_.address_list(), known_addresses);
  VLOG(1) << "       addrs.size: " << addrs.size();
  if (addrs.empty()) {
    send_command_packet(
        BinkpCommands::M_ERR,
        StrCat("Error (NETWORKB-0004): Unable to find common nodes in: ", remote_.address_list()));
    return BinkState::FATAL_ERROR;
  }
  if (addrs.size() == 1) {
    const auto a = *addrs.begin();
    // We only have one address, so set the domain or wwivnet node depending on
    // the type of network.
    if (a.zone() == 20000) {
      remote_.set_wwivnet_node(a.node(), a.domain());
    } else {
      remote_.set_domain(a.domain());
    }
  }

  remote_.set_ftn_addresses(addrs);
  return BinkState::WAIT_PWD;
}

BinkState BinkP::AuthRemoteCalling() {
  VLOG(1) << "       AuthRemoteCalling";
  VLOG(1) << "       expected_ftn: '" << expected_remote_node_ << "'";
  if (remote_.address_list().find(expected_remote_node_) != std::string::npos) {
    return side_ == BinkSide::ORIGINATING ? BinkState::IF_SECURE : BinkState::WAIT_PWD;
  }
  send_command_packet(
      BinkpCommands::M_ERR,
      fmt::format("Error (NETWORKB-0001): Unexpected Addresses: '{}'; expected: '{}'",
                  remote_.address_list(), expected_remote_node_));
  return BinkState::FATAL_ERROR;
}


// Check that the address matches who we thought we called.
BinkState BinkP::AuthRemote() {
  VLOG(1) << "STATE: AuthRemote";
  VLOG(1) << "       remote address_list: " << remote_.address_list();
  const auto result = side_ == BinkSide::ANSWERING ?  AuthRemoteAnswering() : AuthRemoteCalling();
  if (result != BinkState::FATAL_ERROR) {
    const auto rdir = config_->receive_dir(remote_.network().name);
    VLOG(1) << "       Creating file manager for network: " << remote_.network().name << "; dir: " << rdir;
    file_manager_ = std::make_unique<FileManager>(config_->config(), remote_.network(), rdir);
  }
  return result;
}

BinkState BinkP::TransferFiles() {
  if (remote_.network().type == network_type_t::wwivnet) {
    VLOG(1) << "STATE: TransferFiles to node: " << remote_.wwivnet_node() << " :" << remote_.network_name();
  } else {
    VLOG(1) << "STATE: TransferFiles to node: " << remote_.ftn_address() << " :" << remote_.network_name();
  }
  VLOG(1) << "       receive dir: " << config_->receive_dir(remote_.network_name());

  // Quickly let the inbound event loop percolate.
  process_frames(milliseconds(500));
  const auto list = file_manager_->CreateTransferFileList(remote_);
  for (auto* file : list) {
    SendFilePacket(file);
  }

  VLOG(1) << "STATE: After SendFilePacket for all files.";
  // Quickly let the inbound event loop percolate.
  for (auto i = 0; i < 5; i++) {
    process_frames(milliseconds(500));
  }

  // TODO(rushfan): Should this be in a new state?
  if (files_to_send_.empty()) {
    // All files are sent, let's let the remote know we are done.
    VLOG(1) << "       Sending EOB";
    // Kinda a hack, but trying to send a 3 byte packet was stalling on Windows.  Making it larger
    // makes it send (yes, even with TCP_NODELAY set).
    send_command_packet(BinkpCommands::M_EOB, "All files to send have been sent. Thank you.");
    process_frames(seconds(1));
  } else {
    VLOG(1) << "       files_to_send_ is not empty, Not sending EOB";
    std::ostringstream files;
    for (const auto& f : files_to_send_) {
      files << f.first << " ";
    }
    VLOG(2) << "Files: " << files.str();
  }
  return BinkState::WAIT_EOB;
}

BinkState BinkP::Unknown() {
  VLOG(1) << "STATE: Unknown";
  auto count = 0;
  auto predicate = [&]() -> bool { return count++ > 4; };
  process_frames(predicate, seconds(3));
  return BinkState::DONE;
}

BinkState BinkP::FatalError() {
  LOG(ERROR) << "STATE: FatalError";
  auto count = 0;
  auto predicate = [&]() -> bool { return count++ > 4; };
  process_frames(predicate, seconds(3));
  return BinkState::DONE;
}

BinkState BinkP::WaitEob() {
  VLOG(1) << "STATE: WaitEob: ENTERING eob_received: " << std::boolalpha << eob_received_;
  if (eob_received_) {
    // If we've already received an EOB, don't process_frames or do anything else.
    // We're done.
    return BinkState::DONE;
  }

  const auto eob_retries = 12;
  const auto eob_wait_seconds = 5;
  for (auto count = 1; count < eob_retries; count++) {
    // Loop for up to one minute waiting for an EOB before exiting.
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
  const auto filename(file->filename());
  VLOG(1) << "       SendFilePacket: " << filename;
  files_to_send_[filename] = std::unique_ptr<TransferFile>(file);
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
  VLOG(1) << "       SendFileData: " << file->filename();
  const auto file_length = file->file_size();
  const auto chunk_size = 16384; // This is 1<<14.  The max per spec is (1 << 15) - 1
  const auto chunk = std::make_unique<char[]>(chunk_size);
  for (long start = 0; start < file_length; start += chunk_size) {
    const auto size = std::min<int>(chunk_size, file_length - start);
    if (!file->GetChunk(chunk.get(), start, size)) {
      // Bad chunk. Abort
    }
    send_data_packet(chunk.get(), size);
    // sending multi-chunk files was not reliable.  check after each frame if we have
    // an inbound command.
    process_frames(seconds(1));
  }
  return true;
}

bool BinkP::HandlePassword(const std::string& password_line) {
  VLOG(1) << "        HandlePassword: ";
  VLOG(2) << "        password_line: " << password_line;
  if (!starts_with(password_line, "CRAM")) {
    LOG(INFO) << "        HandlePassword: Received Plain text password";
    auth_type_ = AuthType::PLAIN_TEXT;
    remote_password_ = password_line;
    return true;
  }

  static const std::string CRAM_MD5_PREFIX = "CRAM-MD5-";
  if (!starts_with(password_line, CRAM_MD5_PREFIX)) {
    send_command_packet(BinkpCommands::M_ERR,
                        "CRAM authentication required, no common hash function");
    return false;
  }
  const auto hashed_password = password_line.substr(CRAM_MD5_PREFIX.size());
  LOG(INFO) << "        HandlePassword: Received CRAM-MD5 hashed password";
  auth_type_ = AuthType::CRAM_MD5;
  remote_password_ = hashed_password;
  return true;
}

// M_FILE received.
bool BinkP::HandleFileRequest(const std::string& request_line) {
  VLOG(1) << "       HandleFileRequest; request_line: " << request_line;
  auto* old_file = current_receive_file_.release();
  if (old_file != nullptr) {
    LOG(ERROR) << "** ERROR: Got HandleFileRequest while still having an open receive file!";
  }
  std::string filename;
  long expected_length;
  time_t timestamp;
  long starting_offset = 0;
  uint32_t crc = 0;
  if (!ParseFileRequestLine(request_line, &filename, &expected_length, &timestamp, &starting_offset,
                            &crc)) {
    return false;
  }
  const auto net = remote_.network_name();
  auto* p = new ReceiveFile(received_transfer_file_factory_(net, filename), filename,
                            expected_length, timestamp, crc);
  current_receive_file_.reset(p);
  return true;
}

bool BinkP::HandleFileGetRequest(const std::string& request_line) {
  LOG(INFO) << "       HandleFileGetRequest: request_line: [" << request_line << "]";
  const auto s = SplitString(request_line, " ");
  const auto& filename = s.at(0);
  //const auto length = to_number<long>(s.at(1));
  //const auto timestamp = to_number<time_t>(s.at(2));
  long offset = 0;
  if (s.size() >= 4) {
    offset = to_number<long>(s.at(3));
  }
  if (offset != 0) {
    LOG(WARNING) << "offset specified in FileGetRequest.  We don't support offset != 0";
  }

  const auto iter = files_to_send_.find(filename);
  if (iter == end(files_to_send_)) {
    LOG(ERROR) << "File not found: " << filename;
    return false;
  }
  return SendFileData(iter->second.get());
  // File was sent but wait until we receive M_GOT before we remove it from the list.
}

bool BinkP::HandleFileGotRequest(const std::string& request_line) {
  LOG(INFO) << "       HandleFileGotRequest: request_line: [" << request_line << "]";
  const auto s = SplitString(request_line, " ");
  const auto& filename = s.at(0);
  const auto length = to_number<int>(s.at(1));

  const auto iter = files_to_send_.find(filename);
  if (iter == end(files_to_send_)) {
    LOG(ERROR) << "File not found: " << filename;
    return false;
  }

  auto* file = iter->second.get();
  // Increment the number of bytes sent.
  // Also don't increment with -1 if there's an error with the file.
  bytes_sent_ += std::max(0, file->file_size());

  if (length != file->file_size()) {
    LOG(ERROR) << "NON-FATAL ERROR: Size didn't match M_GOT. Please log a bug. M_GOT: " << length
               << "; file_size: " << file->file_size();
  }

  // This is a file that we sent.
  if (!file->Delete()) {
    LOG(ERROR) << "       *** UNABLE TO DELETE FILE: " << file->filename();
  }
  files_to_send_.erase(iter);
  return true;
}

void BinkP::Run(const wwiv::core::CommandLine& cmdline) {
  const auto now = DateTime::now();
  config_->session_identifier(fmt::format("in-{}", now.to_time_t()));
  LOG(INFO) << "session id:  " << config_->session_identifier();

  VLOG(1) << "STATE: Run(): side:" << static_cast<int>(side_);
  auto state = (side_ == BinkSide::ORIGINATING) ? BinkState::CONN_INIT : BinkState::WAIT_CONN;
  const auto start_time = system_clock::now();
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
    LOG(INFO) << "       connection was closed by the other side.";
    VLOG(1)   << "       details: " << e.what();
  } catch (const socket_error& e) {
    LOG(ERROR) << "STATE: BinkP::RunOriginatingLoop() socket_error: " << e.what();
  }

  const auto network_log_side = side_ == BinkSide::ORIGINATING ? NetworkSide::TO : NetworkSide::FROM;
  NetworkLog net_log(config_->gfiles_directory());
  const auto end_time = system_clock::now();
  const auto log_seconds = duration_cast<seconds>(end_time - start_time);
  if (remote_.network().type == network_type_t::wwivnet) {
    // Handle WWIVnet inbound files.
    if (file_manager_) {
      // file_manager_ is null in some tests (BinkpTest).
      file_manager_->rename_wwivnet_pending_files();
      if (!config_->skip_net()) {
        process_network_files(cmdline);
      }
    }

    // Log to net.log
    // Update WWIVnet net.log and contact.net for WWIVnet connections.
    net_log.Log(system_clock::to_time_t(start_time), network_log_side, remote_.wwivnet_node(),
                bytes_sent_, bytes_received_, log_seconds, remote_.network_name());

    // Update contact.net
    Contact c(config_->network(remote_.network_name()), true);
    const auto dt = DateTime::from_time_t(system_clock::to_time_t(start_time));
    if (error_received_) {
      c.add_failure(remote_.wwivnet_node(), dt);
    } else {
      c.add_connect(remote_.wwivnet_node(), dt, bytes_sent_, bytes_received_);
    }
  } else {

    // Handle FTN inbound files.
    if (file_manager_) {
      // file_manager_ is null in some tests (BinkpTest).
      file_manager_->rename_ftn_pending_files(remote_);
      if (!config_->skip_net()) {
        const auto network_number = config_->networks().network_number(remote_.network_name());
        System(cmdline.bindir(), StrCat("networkc .", network_number, " --v=", config_->verbose()));
      }
    }

    // Log to net.log using fake outbound node (32675)
    net_log.Log(system_clock::to_time_t(start_time), network_log_side, FTN_FAKE_OUTBOUND_NODE,
                bytes_sent_, bytes_received_, log_seconds, remote_.network_name());

  }

  // Cleanup receive directory if it's not empty.
  const auto rdir = config_->receive_dir(remote_.network_name());
  if (!rdir.empty()) {
    if (!File::Remove(rdir)) {
      LOG(ERROR) << "Unable to remove directory: " << rdir;
    }
  }

}

void BinkP::process_network_files(const wwiv::core::CommandLine& cmdline) const {
  const auto network_name = remote_.network_name();
  VLOG(1) << "STATE: process_network_files for network: " << network_name;
  const auto network_number = config_->networks().network_number(network_name);
  if (network_number == wwiv::sdk::Networks::npos) {
    return;
  }
  System(cmdline.bindir(), StrCat("networkc .", network_number, " --v=", config_->verbose()));
}

bool ParseFileRequestLine(const std::string& request_line, std::string* filename, long* length,
                          time_t* timestamp, long* offset, uint32_t* crc) {
  auto s = SplitString(request_line, " ");
  if (s.size() < 3) {
    LOG(ERROR) << "ERROR: INVALID request_line: " << request_line
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

} // namespace wwiv
