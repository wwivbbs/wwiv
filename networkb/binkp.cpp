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

#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/os.h"
#include "core/version.h"
#include "core/wfndfile.h"
#include "networkb/binkp_commands.h"
#include "networkb/binkp_config.h"
#include "networkb/callout.h"
#include "networkb/connection.h"
#include "networkb/socket_exceptions.h"
#include "networkb/transfer_file.h"
#include "networkb/wfile_transfer_file.h"
#include "sdk/filenames.h"

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::boolalpha;
using std::endl;
using std::map;
using std::stoi;
using std::stol;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::net;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::os;

namespace wwiv {
namespace net {

string expected_password_for(const Callout* callout, int node) {
  const net_call_out_rec* con = callout->node_config_for(node);
  string password("-");  // default password
  if (con != nullptr) {
    const char *p = con->password;
    if (p && *p) {
      // If the password is null nullptr and not empty string.
      password.assign(p);
    } else {
      LOG << "       No password found for node: " << node << " using default password of '-'";
    }
  }
  return password;
}

int node_number_from_address_list(const string& network_list, const string& network_name) {
  LOG << "       node_number_from_address_list: '" << network_list << "'; network_name: " << network_name;
  vector<string> v = SplitString(network_list, " ");
  for (auto s : v) {
    StringTrim(&s);
    LOG << "       node_number_from_address_list(s): '" << s << "'";
    if (ends_with(s, StrCat("@", network_name)) && starts_with(s, "20000:20000/")) {
      s = s.substr(12);
      s = s.substr(0, s.find('/'));
      return stoi(s);
    }
  }
  return -1;
}

std::string network_name_from_single_address(const std::string& network_list) {
  vector<string> v = SplitString(network_list, " ");
  if (v.empty()) {
    return "";
  }
  string s = v.front();
  string::size_type index = s.find_last_of("@");
  if (index == string::npos) {
    // default to wwivnet
    return "wwivnet";
  }
  return s.substr(index+1);
}

class SendFiles {
public:
  SendFiles(const string& network_directory, uint16_t destination_node) 
    : network_directory_(network_directory), destination_node_(destination_node) {}
  virtual ~SendFiles() {}

  vector<TransferFile*> CreateTransferFileList() {
    vector<TransferFile*> result;
    string dir = network_directory_;
    File::EnsureTrailingSlash(&dir);
    const string s_node_net = StringPrintf("s%d.net", destination_node_);
    const string search_path = StrCat(dir, s_node_net);
    LOG << "       CreateTransferFileList: search_path: " << search_path;
    if (File::Exists(search_path)) {
      File file (search_path);
      const string basename = file.GetName();
      result.push_back(new WFileTransferFile(basename, unique_ptr<File>(new File(network_directory_, basename))));
      LOG << "       CreateTransferFileList: found file: " << basename;
    }

    return result;
  }

private:
  const string network_directory_;
  const uint16_t destination_node_;
};

BinkP::BinkP(Connection* conn, BinkConfig* config, std::map<const string, Callout>& callouts, BinkSide side,
        int expected_remote_node,
        received_transfer_file_factory_t& received_transfer_file_factory)
  : conn_(conn),
    config_(config), 
    callouts_(callouts),
    side_(side),
    expected_remote_node_(expected_remote_node), 
    error_received_(false),
    received_transfer_file_factory_(received_transfer_file_factory) {}

BinkP::~BinkP() {
  files_to_send_.clear();
}

bool BinkP::process_command(int16_t length, milliseconds d) {
  if (!conn_->is_open()) {
    return false;
  }
  const uint8_t command_id = conn_->read_uint8(d);
  unique_ptr<char[]> data(new char[length]);
  conn_->receive(data.get(), length - 1, d);
  string s(data.get(), length - 1);

  LOG << "RECV:  " << BinkpCommands::command_id_to_name(command_id) << ": " << s;
  switch (command_id) {
  case BinkpCommands::M_NUL: {
    // TODO(rushfan): process these.
  } break;
  case BinkpCommands::M_ADR: {
    address_list_ = s;
    // address list is always lower cased compared.
    StringLowerCase(&address_list_);
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
    remote_password_ = s;
  } break;
  case BinkpCommands::M_FILE: {
    HandleFileRequest(s);
  } break;
  case BinkpCommands::M_ERR: {
    error_received_ = true;
  } break;
  default: {
    LOG << "       ** Unhandled Command: " << BinkpCommands::command_id_to_name(command_id) << ": " << s;
  } break;
  }
  return true;
}

bool BinkP::process_data(int16_t length, milliseconds d) {
  if (!conn_->is_open()) {
    return false;
  }
  unique_ptr<char[]> data(new char[length]);
  int length_received = conn_->receive(data.get(), length, d);
  string s(data.get(), length);
  LOG << "RECV:  DATA PACKET; len: " << length_received << "; expected: " << length << " duration:" << d.count();
  if (!current_receive_file_) {
    LOG << "ERROR: Received M_DATA with no current file.";
    return false;
  }
  current_receive_file_->WriteChunk(data.get(), length_received);
  if (current_receive_file_->length() >= current_receive_file_->expected_length()) {
    LOG << "       file finished; bytes_received: " << current_receive_file_->length();

    const string data_line = StringPrintf("%s %u %u",
        current_receive_file_->filename().c_str(),
        current_receive_file_->length(),
        current_receive_file_->timestamp());

    current_receive_file_->Close();
    received_files_.push_back(current_receive_file_->filename());
    current_receive_file_.release();
    send_command_packet(BinkpCommands::M_GOT, data_line);
  } else {
    LOG << "       file still transferring; bytes_received: " << current_receive_file_->length()
        << " and: " << current_receive_file_->expected_length() << " bytes expected.";
  }

  return true;
}

bool BinkP::process_frames(milliseconds d) {
  return process_frames([&]() -> bool { return false; }, d);
}

bool BinkP::process_frames(std::function<bool()> predicate, milliseconds d) {
  if (!conn_->is_open()) {
    return false;
  }
  try {
    while (!predicate()) {
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
  } catch (timeout_error& e) {
  }
  return true;
}

bool BinkP::send_command_packet(uint8_t command_id, const string& data) {
  if (!conn_->is_open()) {
    return false;
  }
  const std::size_t size = 3 + data.size(); /* header + command + data + null*/
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

  int sent = conn_->send(packet.get(), size, seconds(3));
  if (command_id != BinkpCommands::M_PWD) {
    LOG << "SEND:  command: " << BinkpCommands::command_id_to_name(command_id)
         << ": " << data;
  } else {
    // Mask the password.
    LOG << "SEND:  command: " << BinkpCommands::command_id_to_name(command_id) 
        << ": " << std::string(data.size(), '*');
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
  LOG << "SEND:  data packet: packet_length: " << (int) packet_length;
  return true;
}

BinkState BinkP::ConnInit() {
  LOG << "STATE: ConnInit";
  process_frames(seconds(2));
  return BinkState::WAIT_CONN;
}

static string wwiv_version_string() {
  return StrCat(wwiv_version, beta_version, " (", wwiv_date, ")");

}

BinkState BinkP::WaitConn() {
  LOG << "STATE: WaitConn";
  send_command_packet(BinkpCommands::M_NUL, "OPT wwivnet");
  send_command_packet(BinkpCommands::M_NUL, StrCat("WWIVVER ", wwiv_version_string()));
  send_command_packet(BinkpCommands::M_NUL, StrCat("SYS ", config_->system_name()));
  send_command_packet(BinkpCommands::M_NUL, "ZYZ Unknown Sysop");
  send_command_packet(BinkpCommands::M_NUL, "VER networkb/0.0 binkp/1.0");
  send_command_packet(BinkpCommands::M_NUL, "LOC Unknown");

  string network_addresses;
  if (side_ == BinkSide::ANSWERING) {
    // Present all addresses on answering side.
    for (const auto net : config_->networks().networks()) {
      string lower_case_network_name(net.name);
      StringLowerCase(&lower_case_network_name);
      send_command_packet(BinkpCommands::M_NUL,
          StringPrintf("WWIV @%u.%s", net.sysnum, lower_case_network_name.c_str()));
      if (!network_addresses.empty()) {
        network_addresses.append(" ");
      }
      network_addresses += StringPrintf("20000:20000/%d@%s", net.sysnum, lower_case_network_name.c_str());
    }
  } else {
    // Present single primary address.
    send_command_packet(BinkpCommands::M_NUL,
        StringPrintf("WWIV @%u.%s", config_->callout_node_number(), config_->callout_network_name().c_str()));
    const auto net = config_->networks()[config_->callout_network_name()];
    string lower_case_network_name(net.name);
    StringLowerCase(&lower_case_network_name);
    network_addresses = StringPrintf("20000:20000/%d@%s", net.sysnum, lower_case_network_name.c_str());
  }
  send_command_packet(BinkpCommands::M_ADR, network_addresses);

  // Try to process any inbound frames before leaving this state.
  process_frames(milliseconds(100));
  return (side_ == BinkSide::ORIGINATING) ?
      BinkState::SEND_PASSWORD : BinkState::WAIT_ADDR;
}

BinkState BinkP::SendPasswd() {
  // This is on the sending side.
  const string network_name(remote_network_name());
  LOG << "STATE: SendPasswd for network '" << network_name << "' for node: " << expected_remote_node_;
  Callout callout = callouts_.at(network_name);
  const string password = expected_password_for(&callout, expected_remote_node_);
  LOG << "       sending password packet";
  send_command_packet(BinkpCommands::M_PWD, password);
  return BinkState::WAIT_ADDR;
}

BinkState BinkP::WaitAddr() {
  LOG << "STATE: WaitAddr";
  auto predicate = [&]() -> bool { return !address_list_.empty(); };
  for (int i=0; i < 10; i++) {
    process_frames(predicate, seconds(1));
    if (!address_list_.empty()) {
      return BinkState::AUTH_REMOTE;
    }
  }
  return BinkState::AUTH_REMOTE;
}

BinkState BinkP::PasswordAck() {
  // This is only on the answering side.
  // This should only happen in the originating side.
  if (side_ != BinkSide::ANSWERING) {
    LOG << "**** ERROR: WaitPwd Called on ORIGINATING side";
  }

  // TODO(rushfan): we need to use the network name we matched not the one that config thinks.
  const string network_name = remote_network_name();
  int remote_node = node_number_from_address_list(address_list_, network_name);
  LOG << "       remote node: " << remote_node;

  Callout callout(callouts_.at(network_name));
  const string expected_password = expected_password_for(&callout, remote_node);
  LOG << "STATE: PasswordAck";
  LOG << "       expected_password = '" << expected_password << "'";
  if (remote_password_ == expected_password) {
    // Passwords match, send OK.
    send_command_packet(BinkpCommands::M_OK, "Passwords match; insecure session");
    // No need to wait for OK since we are the answering side, just move straight to
    // transfer files.
    return BinkState::TRANSFER_FILES;
  }

  // Passwords do not match, send error.
  send_command_packet(BinkpCommands::M_ERR,
      StringPrintf("Password doen't match.  Received '%s' expected '%s'.",
       remote_password_.c_str(), expected_password.c_str()));
  return BinkState::DONE;
}

BinkState BinkP::WaitPwd() {
  // This should only happen in the originating side.
  if (side_ != BinkSide::ANSWERING) {
    LOG << "**** ERROR: WaitPwd Called on ORIGINATING side";
  }
  LOG << "STATE: WaitPwd";
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
  LOG << "STATE: WaitOk";
  for (int i=0; i < 30; i++) {
    process_frames([&]() -> bool { return ok_received_; }, seconds(1));
    if (ok_received_) {
      return BinkState::TRANSFER_FILES;
    }
  }

  LOG << "       after WaitOk: M_OK never received.";
  send_command_packet(BinkpCommands::M_ERR, "M_OK never received. Timeed out waiting for it.");
  return BinkState::DONE;
}

BinkState BinkP::IfSecure() {
  LOG << "STATE: IfSecure";
  // Wait for OK if we sent a password.
  // Log an unsecure session of there is no password.
  return BinkState::WAIT_OK;
}

BinkState BinkP::AuthRemote() {
  LOG << "STATE: AuthRemote";
  // Check that the address matches who we thought we called.
  LOG << "       remote address_list: " << address_list_;
  const string network_name(remote_network_name());
  if (side_ == BinkSide::ANSWERING) {
    int caller_node = node_number_from_address_list(address_list_, network_name);
    LOG << "       remote_network_name: " << network_name << "; caller_node: " << caller_node;
    if (callouts_.find(network_name) == end(callouts_)) {
      // We don't have a callout.net entry for this caller. Fail the connection
      send_command_packet(BinkpCommands::M_ERR, 
          StrCat("Error (NETWORKB-0003): Unable to find callout.net for: ", network_name));
      // TODO(rushfan): Do we need a real error state (same TODO as below)
      return BinkState::UNKNOWN;
    }
    const net_call_out_rec* callout_record = callouts_.at(network_name).node_config_for(caller_node);
    if (callout_record == nullptr) {
      // We don't have a callout.net entry for this caller. Fail the connection
      send_command_packet(BinkpCommands::M_ERR, 
          StrCat("Error (NETWORKB-0002): Unexpected Address: ", address_list_));
      // TODO(rushfan): Do we need a real error state (same TODO as below)
      return BinkState::UNKNOWN;
    }
    return BinkState::WAIT_PWD;
  }

  const string expected_ftn = StringPrintf("20000:20000/%d@%s", expected_remote_node_, network_name.c_str());
  LOG << "       expected_ftn: " << expected_ftn;
  if (address_list_.find(expected_ftn) != string::npos) {
    return (side_ == BinkSide::ORIGINATING) ?
      BinkState::IF_SECURE : BinkState::WAIT_PWD;
  } else {
    send_command_packet(BinkpCommands::M_ERR, 
      StrCat("Error (NETWORKB-0001): Unexpected Address: ", address_list_));
    // TODO(rushfan): add error state?
    return BinkState::UNKNOWN;
  }
}

BinkState BinkP::TransferFiles() {
  // This is only valid if we are the SENDER.
  // TODO(rushfan): make this a method on the class.
  int remote_node = expected_remote_node_;
  const string network_name(remote_network_name());
  if (side_ == BinkSide::ANSWERING) {
    remote_node = node_number_from_address_list(address_list_, network_name);
  }

  LOG << "STATE: TransferFiles to node: " << remote_node;
  // Quickly let the inbound event loop percolate.
  process_frames(milliseconds(500));
  SendFiles file_sender(config_->network_dir(network_name), remote_node);
  const auto list = file_sender.CreateTransferFileList();
  for (auto file : list) {
    SendFilePacket(file);
  }

  LOG << "STATE: After SendFilePacket for all files.";
  // Quickly let the inbound event loop percolate.
  for (int i=0; i < 5; i++) {
    process_frames(milliseconds(500));
  }

  // TODO(rushfan): Should this be in a new state?
  if (files_to_send_.empty()) {
    // All files are sent, let's let the remote know we are done.
    LOG << "       Sending EOB";
    // Kinda a hack, but trying to send a 3 byte packet was stalling on Windows.  Making it larger makes
    // it send (yes, even with TCP_NODELAY set).
    send_command_packet(BinkpCommands::M_EOB, "All files to send have been sent. Thank you.");
    process_frames(seconds(1));
  } else {
    LOG << "       files_to_send_ is not empty, Not sending EOB";
  }
  return BinkState::WAIT_EOB;
}

BinkState BinkP::Unknown() {
  LOG << "STATE: Unknown";
  int count = 0;
  auto predicate = [&]() -> bool { return count++ > 4; };
  process_frames(predicate, seconds(3));
  return BinkState::DONE;
}

BinkState BinkP::WaitEob() {
  LOG << "STATE: WaitEob: ENTERING eob_received: " << boolalpha << eob_received_;
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
      LOG << "       WaitEob: still waiting for M_EOB to be received. will wait up to "
          << (eob_retries * eob_wait_seconds) << " seconds.";
    } catch (timeout_error& e) {
      LOG << "       WaitEob: ERROR while waiting for more data: " << e.what();
    }
  }
  return BinkState::DONE;
}

bool BinkP::SendFilePacket(TransferFile* file) {
  const string filename(file->filename());
  LOG << "       SendFilePacket: " << filename;
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
  LOG << "       SendFileData: " << file->filename();
  long file_length = file->file_size();
  const int chunk_size = 16384; // This is 1<<14.  The max per spec is (1 << 15) - 1
  long start = 0;
  unique_ptr<char[]> chunk(new char[chunk_size]);
  for (long start = 0; start < file_length; start+=chunk_size) {
    int size = std::min<int>(chunk_size, file_length - start);
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

// M_FILE received.
bool BinkP::HandleFileRequest(const string& request_line) {
  LOG << "       HandleFileRequest; request_line: " << request_line;
  ReceiveFile* old_file = current_receive_file_.release();
  if (old_file != nullptr) {
    LOG << "** ERROR: Got HandleFileRequest while still having an open receive file!";
  }
  string filename;
  long expected_length;
  time_t timestamp;
  long starting_offset = 0;
  if (!ParseFileRequestLine(request_line,
      &filename, 
      &expected_length,
      &timestamp, 
      &starting_offset)) {
    return false;
  }
  const auto net = remote_network_name();
  auto *p = new ReceiveFile(received_transfer_file_factory_(net, filename),
    filename,
    expected_length,
    timestamp);
  current_receive_file_.reset(p);
  return true;
}

bool BinkP::HandleFileGetRequest(const string& request_line) {
  LOG << "       HandleFileGetRequest: request_line: [" << request_line << "]"; 
  vector<string> s = SplitString(request_line, " ");
  const string filename = s.at(0);
  long length = stol(s.at(1));
  time_t timestamp = std::stoul(s.at(2));
  long offset = 0;
  if (s.size() >= 4) {
    offset = stol(s.at(3));
  }

  auto iter = files_to_send_.find(filename);
  if (iter == std::end(files_to_send_)) {
    LOG << "File not found: " << filename;
    return false;
  }
  return SendFileData(iter->second.get());
  // File was sent but wait until we receive M_GOT before we remove it from the list.
}

bool BinkP::HandleFileGotRequest(const string& request_line) {
  LOG << "       HandleFileGotRequest: request_line: [" << request_line << "]"; 
  vector<string> s = SplitString(request_line, " ");
  const string filename = s.at(0);
  long length = stol(s.at(1));

  auto iter = files_to_send_.find(filename);
  if (iter == std::end(files_to_send_)) {
    LOG << "File not found: " << filename;
    return false;
  }

  TransferFile* file = iter->second.get();
  if (!file->Delete()) {
    LOG << "       *** UNABLE TO DELETE FILE: " << file->filename(); 
  }
  files_to_send_.erase(iter);
  return true;
}

static void rename_pend(const string& directory, const string& filename) {
  File pend_file(directory, filename);
  if (!pend_file.Exists()) {
    LOG << " pending file does not exist: " << pend_file;
    return;
  }
  const string pend_filename(pend_file.full_pathname());
  const string num = filename.substr(1);
  const string prefix = (atoi(num.c_str())) ? "1" : "0";

  for (int i = 0; i < 1000; i++) {
    const string new_filename = StringPrintf("%sp%s-0-%u.net", directory.c_str(), prefix.c_str(), i);
    // LOG << new_filename;
    if (File::Rename(pend_filename, new_filename)) {
      LOG << "renamed file to: " << new_filename;
      return;
    }
  }
  LOG << "all attempts failed to rename_pend";
}

void BinkP::Run() {
  LOG << "STATE: Run (Main Event Loop): side_:" << static_cast<int>(side_);
  BinkState state = (side_ == BinkSide::ORIGINATING) ? BinkState::CONN_INIT : BinkState::WAIT_CONN;
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
      case BinkState::DONE:
        LOG << "STATE: Done.";
        conn_->close();
        done = true;
        break;
      }

      if (error_received_) {
        LOG << "STATE: Error Received.";
        done = true;
      }
      process_frames(milliseconds(100));
    }
  } catch (socket_closed_error&) {
    // The other end closed the socket before we did.
    LOG << "       connection was closed by the other side.";
  } catch (socket_error& e) {
    LOG << "STATE: BinkP::RunOriginatingLoop() socket_error: " << e.what()
        << "\nStacktrace:\n" << stacktrace();
  }
  LOG << "Before rename pending_files";
  rename_pending_files();
  if (!config_->skip_net()) {
    process_network_files();
  }
}

void BinkP::rename_pending_files() const {
  LOG << "STATE: rename_pending_files";
  for (const auto& file : received_files_) {
    if (!config_->networks().contains(remote_network_name())) {
      // unknown network. log and skip.
      LOG << "ERROR: unknown network name (not in config.dat): " << remote_network_name();
      continue;
    }
    const auto dir = config_->networks()[remote_network_name()].dir;
    LOG << "       renaming_pending_file: dir: " << dir << "; file: " << file;
    rename_pend(dir, file);
  }
}

static int System(const string& cmd) {
  LOG << "       executing:: " << cmd;
  return system(cmd.c_str());
}

void BinkP::process_network_files() const {
  const string network_name = remote_network_name();
  LOG << "STATE: process_network_files for network: " << network_name;
  int network_number = config_->networks().network_number(network_name);
  if (network_number == wwiv::sdk::Networks::npos) {
    return;
  }
  const auto dir = config_->networks()[remote_network_name()].dir;
  if (File::ExistsWildcard(StrCat(dir, "p*.net"))) {
    System(StrCat("network1 .", network_number));
    if (File::Exists(StrCat(dir, LOCAL_NET))) {
      System(StrCat("network2 .", network_number));
    }
  }
  // TODO(rushfan): check timestamps on network files to see if we need
  // to run network3 .# Y.  Also look for UPD files (BBSLIST.UPD and CONNECT.UPD).
}

const std::string BinkP::remote_network_name() const {
  string remote_network_name(config_->callout_network_name());
  if (side_ == BinkSide::ANSWERING) {
    //TODO(rushfan): Ensure that we only have one address on receiving end.
    remote_network_name = network_name_from_single_address(address_list_);
  }
  LOG << "       remote_network_name(): " << remote_network_name;
  return remote_network_name;
}

bool ParseFileRequestLine(const std::string& request_line, 
        std::string* filename,
        long* length,
        time_t* timestamp,
        long* offset) {
  vector<string> s = SplitString(request_line, " ");
  if (s.size() < 3) {
    LOG << "ERROR: INVALID request_line: "<< request_line
         << "; had < 3 parts.  # parts: " << s.size();
    return false;
  }
  *filename = s.at(0);
  *length = stol(s.at(1));
  *timestamp = stoul(s.at(2));
  *offset = 0;
  if (s.size() >= 4) {
    *offset = stol(s.at(3));
  }
  return true;
}
  
}  // namespace net
} // namespace wwiv
