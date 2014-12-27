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
#include "core/wfndfile.h"
#include "networkb/binkp_commands.h"
#include "networkb/binkp_config.h"
#include "networkb/callout.h"
#include "networkb/connection.h"
#include "networkb/socket_exceptions.h"
#include "networkb/transfer_file.h"
#include "networkb/wfile_transfer_file.h"

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

namespace wwiv {
namespace net {

#define LOG wwiv::core::Logger()

string expected_password_for(Callout* callout, int node) {
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
  vector<string> v = SplitString(network_list, " ");
  for (auto s : v) {
    StringTrim(&s);
    if (ends_with(s, StrCat("@", network_name)) && starts_with(s, "20000:20000/")) {
      s = s.substr(12);
      s = s.substr(0, s.find('/'));
      return stoi(s);
    }
  }
  return -1;
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
    const string s_node_net = StringPrintf("S%d.NET", destination_node_);
    const string search_path = StrCat(dir, File::pathSeparatorString, s_node_net);
    WFindFile fnd;
    bool found = fnd.open(search_path, 0);
    while (found) {
      result.push_back(new WFileTransferFile(fnd.GetFileName(), unique_ptr<File>(new File(network_directory_, fnd.GetFileName()))));
      found = fnd.next();
    }
    fnd.close();

    return result;
  }

private:
  const string network_directory_;
  const uint16_t destination_node_;
};

BinkP::BinkP(Connection* conn, BinkConfig* config, Callout* callout, BinkSide side,
        int expected_remote_node,
        received_transfer_file_factory_t& received_transfer_file_factory)
  : conn_(conn), config_(config), callout_(callout), side_(side),
    own_address_(config->node_number()), 
    expected_remote_node_(expected_remote_node), 
    error_received_(false),
    received_transfer_file_factory_(received_transfer_file_factory) {}

BinkP::~BinkP() {
  files_to_send_.clear();
}

bool BinkP::process_command(int16_t length, milliseconds d) {
  const uint8_t command_id = conn_->read_uint8(d);
  unique_ptr<char[]> data(new char[length]);
  conn_->receive(data.get(), length - 1, d);
  string s(data.get(), length - 1);
  switch (command_id) {
  case BinkpCommands::M_NUL: {
    LOG << "RECV:  M_NUL: " << s;
  } break;
  case BinkpCommands::M_ADR: {
    LOG << "RECV:  M_ADR: " << s;
    address_list_ = s;
  } break;
  case BinkpCommands::M_OK: {
    LOG << "RECV:  M_OK: " << s;
    ok_received_ = true;
  } break;
  case BinkpCommands::M_GET: {
    LOG << "RECV:  M_GET: " << s;
    HandleFileGetRequest(s);
  } break;
  case BinkpCommands::M_GOT: {
    LOG << "RECV:  M_GOT: " << s;
    HandleFileGotRequest(s);
  } break;
  case BinkpCommands::M_EOB: {
    LOG << "RECV:  M_EOB: " << s;
    eob_received_ = true;
  } break;
  case BinkpCommands::M_PWD: {
    LOG << "RECV:  M_PWD: " << s;
    remote_password_ = s;
  } break;
  case BinkpCommands::M_FILE: {
    LOG << "RECV:  M_FILE: " << s;
    HandleFileRequest(s);
  } break;
  case BinkpCommands::M_ERR: {
    LOG << "RECV:  M_ERR: " << s;
    error_received_ = true;
  } break;
  default: {
    LOG << "RECV:  ** UNHANDLED COMMAND: " << BinkpCommands::command_id_to_name(command_id) 
         << " data: " << s;
  } break;
  }
  return true;
}

bool BinkP::process_data(int16_t length, milliseconds d) {
  unique_ptr<char[]> data(new char[length]);
  int length_received = conn_->receive(data.get(), length, d);
  string s(data.get(), length);
  LOG << "RECV:  DATA PACKET; len: " << length_received << "; data: " << s.substr(3);
  return true;
}

bool BinkP::process_frames(milliseconds d) {
  return process_frames([&]() -> bool { return false; }, d);
}

bool BinkP::process_frames(std::function<bool()> predicate, milliseconds d) {
  try {
    while (!predicate()) {
      uint16_t header = conn_->read_uint16(d);
      if (header & 0x8000) {
        if (!process_command(header & 0x7fff, d)) {
          // false return value means an error occurred.
          return false;
        }
      } else {
        // Unexpected data frame.
        if (!process_data(header & 0x7fff, d)) {
          // false return value mean san error occurred.
          return false;
        }
      }
    }
  } catch (timeout_error ignored) {
    // LOG << "        timeout_error from process_frames";
  }
  return true;
}

bool BinkP::send_command_packet(uint8_t command_id, const string& data) {
  const std::size_t size = 3 + data.size(); /* header + command + data + null*/
  unique_ptr<char[]> packet(new char[size]);
  // Actual packet size parameter does not include the size parameter itself.
  // And for sending a commmand this will be 2 less than our actual packet size.
  uint16_t packet_length = (data.size() + sizeof(uint8_t)) | 0x8000;
  uint8_t b0 = ((packet_length & 0xff00) >> 8) | 0x80;
  uint8_t b1 = packet_length & 0x00ff;

  char *p = packet.get();
  *p++ = b0;
  *p++ = b1;
  *p++ = command_id;
  memcpy(p, data.data(), data.size());

  int sent = conn_->send(packet.get(), size, seconds(3));
  LOG << "SEND:  command: " << BinkpCommands::command_id_to_name(command_id) << ": "
//       << "; packet_length: " << (packet_length & 0x7fff)
//       << "; sent " << sent << "; data: " 
       << data;
  return true;
}

bool BinkP::send_data_packet(const char* data, std::size_t packet_length) {
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

BinkState BinkP::WaitConn() {
  LOG << "STATE: WaitConn";
  send_command_packet(BinkpCommands::M_NUL, "OPT wwivnet");
  send_command_packet(BinkpCommands::M_NUL, StrCat("SYS ", config_->system_name()));
  send_command_packet(BinkpCommands::M_NUL, "ZYZ Unknown Sysop");
  send_command_packet(BinkpCommands::M_NUL, "VER networkb/0.0 binkp/1.0");
  send_command_packet(BinkpCommands::M_NUL, "LOC Unknown");

  send_command_packet(BinkpCommands::M_NUL, StringPrintf("WWIV @%u.%s", config_->node_number(), config_->network_name().c_str()));
  send_command_packet(BinkpCommands::M_ADR, StringPrintf("20000:20000/%d@%s", config_->node_number(), config_->network_name().c_str()));

  // Try to process any inbound frames before leaving this state.
  process_frames(milliseconds(100));
  return (side_ == BinkSide::ORIGINATING) ?
      BinkState::SEND_PASSWORD : BinkState::WAIT_ADDR;
}

BinkState BinkP::SendPasswd() {
  LOG << "STATE: SendPasswd";
  const string password = expected_password_for(callout_, expected_remote_node_);
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

  int remote_node = node_number_from_address_list(address_list_, config_->network_name());
  LOG << "       remote node: " << remote_node;
  const string expected_password = expected_password_for(callout_, remote_node);
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
  if (side_ == BinkSide::ANSWERING) {
    return BinkState::WAIT_PWD;
  }

  const string expected_ftn = StringPrintf("20000:20000/%d@%s", expected_remote_node_,  config_->network_name().c_str());
  LOG << "       expected_ftn: " << expected_ftn;
  if (address_list_.find(expected_ftn) != string::npos) {
    return (side_ == BinkSide::ORIGINATING) ?
      BinkState::IF_SECURE : BinkState::WAIT_PWD;
  } else {
    send_command_packet(BinkpCommands::M_ERR, 
      StrCat("Unexpected Address: ", address_list_));
    // TODO(rushfan): add error state?
    return BinkState::UNKNOWN;
  }
}

BinkState BinkP::TransferFiles() {
  LOG << "STATE: TransferFiles";
  // Quickly let the inbound event loop percolate.
  process_frames(milliseconds(100));

  SendFiles file_sender(config_->network_dir(), expected_remote_node_);
  const auto list = file_sender.CreateTransferFileList();
  for (auto file : list) {
    SendFilePacket(file);
  }
  // Quickly let the inbound event loop percolate.
  for (int i=0; i < 5; i++) {
    process_frames(milliseconds(50));
  }

  // TODO(rushfan): Should this be in a new state?
  if (files_to_send_.empty()) {
    // All files are sent, let's let the remote know we are done.
    LOG << "       Sending EOB";
    // Kinda a hack, but trying to send a 3 byte packet was stalling on Windows.  Making it larger makes
    // it send (yes, even with TCP_NODELAY set).
    send_command_packet(BinkpCommands::M_EOB, "All files to send have been sent. Thank you.");
    process_frames(seconds(1));
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
  for (int count=1; count < 20; count++) {
    try {
      process_frames([&]() -> bool { return eob_received_; }, seconds(500));
      if (eob_received_) {
        return BinkState::DONE;
      }
      LOG << "       WaitEob: eob_received: " << boolalpha << eob_received_;
    } catch (timeout_error e) {
      LOG << "       WaitEob looping for more data. " << e.what();
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
  LOG << "       SendFilePacket: " << file->filename();
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
  LOG << "HandleFileRequest; request_line: " << request_line;
  string filename;
  long expected_length = 0;
  time_t timestamp = 0;
  long starting_offset = 0;
  if (!ParseFileRequestLine(request_line, &filename, &expected_length, &timestamp, &starting_offset)) {
    return false;
  }

  auto d = seconds(5);
  long bytes_received = starting_offset;
  bool done = false;

  try {
    unique_ptr<TransferFile> received_file(received_transfer_file_factory_(filename));
    while (!done) {
      LOG << "        loop";
      uint16_t header = conn_->read_uint16(d);
      uint16_t length = header & 0x7fff;
      if (header & 0x8000) {
        // Unexpected file packet.  Could be a M_GOT, M_SKIP, or M_GET
        // TOOD(rushfan): make process_command return the command.
        process_command(length, d);
      } else {
        unique_ptr<char[]> data(new char[length]);
        int length_received = conn_->receive(data.get(), length, d);
        LOG << "RECV:  DATA PACKET; len: " << length_received;
        received_file->WriteChunk(data.get(), length_received);
        bytes_received += length_received;
        if (bytes_received >= expected_length) {
          LOG << "        file finished; bytes_received: " << bytes_received;
          done = true;
          const string data_line = StringPrintf("%s %u %u", filename.c_str(), bytes_received, timestamp);
          send_command_packet(BinkpCommands::M_GOT, data_line);
          received_files_.push_back(std::move(received_file));
        }
      }
    }
  } catch (timeout_error e) {
    LOG << "        timeout_error: " << e.what();
  }
  LOG << "        " << " returning true";
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

void BinkP::Run() {
  LOG << "STATE: Run (Main Event Loop): ";
  try {
    BinkState state = (side_ == BinkSide::ORIGINATING) ? BinkState::CONN_INIT : BinkState::WAIT_CONN;
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
        done = true;
        break;
      }

      if (error_received_) {
        LOG << "STATE: Error Received.";
        done = true;
      }
      process_frames(milliseconds(100));
    }
  } catch (socket_error e) {
    LOG << "STATE: BinkP::RunOriginatingLoop() socket_error: " << e.what();
  }
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
