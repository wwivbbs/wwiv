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

#include "core/strings.h"
#include "networkb/connection.h"
#include "networkb/socket_exceptions.h"
#include "networkb/transfer_file.h"

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using std::unique_ptr;
using std::vector;
using wwiv::net::Connection;
using wwiv::strings::SplitString;
using wwiv::strings::StringPrintf;

namespace wwiv {
namespace net {

  BinkP::BinkP(Connection* conn, BinkSide side, int own_address,
	       int  expected_remote_address)
    : conn_(conn), side_(side), own_address_(own_address), 
      expected_remote_address_(expected_remote_address) {}

BinkP::~BinkP() {
  files_to_send_.clear();
}

string BinkP::command_id_to_name(int command_id) const {
  static const map<int, string> map = {
    {M_NUL, "M_NUL"},
    {M_ADR, "M_ADR"},
    {M_PWD, "M_PWD"},
    {M_FILE, "M_FILE"},
    {M_OK, "M_OK"},
    {M_EOB, "M_EOB"},
    {M_GOT, "M_GOT"},
    {M_ERR, "M_ERR"},
    {M_BSY, "M_BSY"},
    {M_GET, "M_GET"},
    {M_SKIP, "M_SKIP"},
  };
  return map.at(command_id);
}

bool BinkP::process_command(int16_t length, std::chrono::milliseconds d) {
  const uint8_t command_id = conn_->read_uint8(d);
  clog << "        process_command: command_id: " << std::dec << command_id << endl;
  unique_ptr<char[]> data(new char[length]);
  conn_->receive(data.get(), length - 1, d);
  string s(data.get(), length - 1);
  switch (command_id) {
  case M_NUL: {
    clog << "RECV:  M_NUL: " << s << endl;
  } break;
  case M_ADR: {
    clog << "RECV:  M_ADR: " << s << endl;
    // TODO(rushfan): tokenize into addresses
    address_list_ = s;
  } break;
  case M_OK: {
    clog << "RECV:  M_OK: " << s << endl;
    ok_received_ = true;
  } break;
  case M_GET: {
    clog << "RECV:  M_GET: " << s << endl;
    HandleFileGetRequest(s);
  } break;
  case M_GOT: {
    clog << "RECV:  M_GOT: " << s << endl;
    HandleFileGotRequest(s);
  } break;
  case M_EOB: {
    clog << "RECV:  M_EOB: " << s << endl;
    eob_received_ = true;
  } break;
  case M_PWD: {
    clog << "RECV:  M_PWD: " << s << endl;
    remote_password_ = s;
  } break;
  case M_FILE: {
    clog << "RECV:  M_FILE: " << s << endl;
    HandleFileRequest(s);
  } break;
  default: {
    clog << "RECV:  ** UNHANDLED COMMAND: " << command_id_to_name(command_id) 
         << " data: " << s << endl;
  } break;
  }
  return true;
}

bool BinkP::process_data(int16_t length, std::chrono::milliseconds d) {
  unique_ptr<char[]> data(new char[length]);
  int length_received = conn_->receive(data.get(), length, d);
  string s(data.get(), length);
  clog << "RECV:  DATA PACKET; len: " << length_received << "; data: " << s.substr(3) << endl;
  return true;
}

bool BinkP::process_frames(std::chrono::milliseconds d) {
  return process_frames([&]() -> bool { return false; }, d);
}

bool BinkP::process_frames(std::function<bool()> predicate, std::chrono::milliseconds d) {
  try {
    while (!predicate()) {
      // clog << "        process_frames loop" << endl;
      uint16_t header = conn_->read_uint16(d);
      if (header & 0x8000) {
        if (!process_command(header & 0x7fff, d)) {
	  // false return value mean san error occurred.
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
    // clog << "        timeout_error from process_frames" << endl;
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
  clog << "SEND:  command: " << command_id_to_name(command_id)
       << "; packet_length: " << (packet_length & 0x7fff)
       << "; data: " << data << endl;
  return true;
}

bool BinkP::send_data_packet(const char* data, std::size_t packet_length) {
  // for now assume everything fits within a single frame.
  std::unique_ptr<char[]> packet(new char[packet_length + 2]);
  packet_length &= 0x7fff;
  uint8_t b0 = ((packet_length & 0xff00) >> 8);
  uint8_t b1 = packet_length & 0x00ff;
  char *p = packet.get();
  *p++ = b0;
  *p++ = b1;
  memcpy(p, data, packet_length);

  conn_->send(packet.get(), packet_length + 2, seconds(10));
  clog << "SEND:  data packet: packet_length: " << (int) packet_length << endl;
  return true;
}

BinkState BinkP::ConnInit() {
  clog << "STATE: ConnInit" << endl;
  process_frames(seconds(2));
  return BinkState::WAIT_CONN;
}

BinkState BinkP::WaitConn() {
  clog << "STATE: WaitConn" << endl;
  send_command_packet(M_NUL, "OPT wwivnet");
  send_command_packet(M_NUL, "SYS NETWORKB test app");
  send_command_packet(M_NUL, "ZYZ Unknown Sysop");
  send_command_packet(M_NUL, "VER networkb/0.0 binkp/1.0");
  send_command_packet(M_NUL, "LOC San Francisco, CA");
  send_command_packet(M_NUL, "WWIV @2.wwivnet");
  send_command_packet(M_ADR, StringPrintf("20000:20000/%d@wwivnet", own_address_));

  // Try to process any inbound frames before leaving this state.
  process_frames(milliseconds(100));
  return (side_ == BinkSide::ORIGINATING) ?
    BinkState::SEND_PASSWORD : BinkState::WAIT_ADDR;
}

BinkState BinkP::SendPasswd() {
  clog << "STATE: SendPasswd" << endl;
  send_command_packet(M_PWD, "-");
  return BinkState::WAIT_ADDR;
}

BinkState BinkP::WaitAddr() {
  clog << "STATE: WaitAddr" << endl;
  auto predicate = [&]() -> bool { return !address_list_.empty(); };
  for (int i=0; i < 30; i++) {
    process_frames(predicate, seconds(1));
    if (!address_list_.empty()) {
      return BinkState::AUTH_REMOTE;
    }
  }
  return BinkState::AUTH_REMOTE;
}

BinkState BinkP::PasswordAck() {
  clog << "STATE: PasswordAck" << endl;
  if (remote_password_ == "-") {
    // Passwords match, send OK.
    send_command_packet(M_OK, "Passwords match; insecure session");
    return BinkState::WAIT_OK;
  }

  // Passwords do not match, send error.
  send_command_packet(M_ERR,
      StringPrintf("Password doen't match.  Received '%s' expected '%s'."
		   "-", "-"));
  return BinkState::DONE;
}

BinkState BinkP::WaitPwd() {
  clog << "STATE: WaitPwd" << endl;
  auto predicate = [&]() -> bool { return !remote_password_.empty(); };
  for (int i=0; i < 30; i++) {
    process_frames(predicate, seconds(1));
    if (predicate()) {
      return BinkState::PASSWORD_ACK;
    }
  }
  return BinkState::PASSWORD_ACK;
}

BinkState BinkP::WaitOk() {
  // TODO(rushfan): add proper timeout to wait for OK.
  clog << "STATE: WaitOk" << endl;
  for (int i=0; i < 30; i++) {
    process_frames([&]() -> bool { return ok_received_; }, seconds(1));
    if (ok_received_) {
      return BinkState::TRANSFER_FILES;
    }
  }

  clog << "       after WaitOk: M_OK never received." << endl;
  send_command_packet(M_ERR, "M_OK never received. Timeed out waiting for it.");
  return BinkState::DONE;
}

BinkState BinkP::IfSecure() {
  clog << "STATE: IfSecure" << endl;
  // Wait for OK if we sent a password.
  // Log an unsecure session of there is no password.
  return BinkState::WAIT_OK;
}

BinkState BinkP::AuthRemote() {
  clog << "STATE: AuthRemote" << endl;
  // Check that the address matches who we thought we called.
  clog << "       address line = " << address_list_ << endl;
  const string expected_ftn = StringPrintf("20000:20000/%d@wwivnet",
					   expected_remote_address_);
  if (address_list_.find(expected_ftn) != string::npos) {
    return (side_ == BinkSide::ORIGINATING) ?
      BinkState::IF_SECURE : BinkState::WAIT_PWD;
  } else {
    send_command_packet(M_ERR, wwiv::strings::StrCat("Unexpected Address: ",
						     address_list_));
    // TODO(rushfan): add error state?
    return BinkState::UNKNOWN;
  }
}

BinkState BinkP::TransferFiles() {
  clog << "STATE: TransferFiles" << endl;
  // Quickly let the inbound event loop percolate.
  process_frames(seconds(1));
  // HACK
  SendDummyFile("a.txt", 'a', 40);
  //SendDummyFile("b.txt", 'b', 50000);
  // Quickly let the inbound event loop percolate.
  process_frames(seconds(1));

  // TODO(rushfan): Should this be in a new state?
  if (files_to_send_.empty()) {
    // All files are sent, let's let the remote know we are done.
    send_command_packet(M_EOB, "");
  }
  return BinkState::WAIT_EOB;
}

BinkState BinkP::Unknown() {
  clog << "STATE: Unknown" << endl;
  int count = 0;
  auto predicate = [&]() -> bool { return count++ > 4; };
  process_frames(predicate, seconds(3));
  return BinkState::DONE;
}

BinkState BinkP::WaitEob() {
  clog << "STATE: WaitEob: eob_received: " << std::boolalpha << eob_received_ << endl;
  auto predicate = [&]() -> bool { return eob_received_; };
  for (int count=1; count < 4; count++) {
    try {
      count++;
      process_frames(predicate, seconds(3));
      if (eob_received_) {
        return BinkState::DONE;
      }
      process_frames(milliseconds(100));
    } catch (timeout_error e) {
      clog << "       WaitEob looping for more data. " << e.what() << std::endl;
    }
  }
  return BinkState::DONE;
}

bool BinkP::SendFilePacket(TransferFile* file) {
  string filename(file->filename());
  clog << "       SendFilePacket: " << filename << endl;
  files_to_send_[filename] = unique_ptr<TransferFile>(file);
  send_command_packet(M_FILE, file->as_packet_data(0));
  process_frames(seconds(2));

  // file* may not be viable anymore if it was already send.
  auto iter = files_to_send_.find(filename);
  if (iter != std::end(files_to_send_)) {
    // We have the file still to send.
    SendFileData(file);
  }
  return true;
}

bool BinkP::SendFileData(TransferFile* file) {
  clog << "       SendFilePacket: " << file->filename() << endl;
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
  clog << "HandleFileRequest; request_line: " << request_line << endl;
  string filename;
  long expected_length = 0;
  time_t timestamp = 0;
  long starting_offset = 0;
  if (!ParseFileRequestLine(request_line, &filename, &expected_length, &timestamp,
			    &starting_offset)) {
    return false;
  }

  auto d = seconds(5);
  bool done = false;
  long bytes_received = starting_offset;

  // HACK
  string contents;

  try {
    while (!done) {
      clog << "        loop" << endl;
      uint16_t header = conn_->read_uint16(d);
      uint16_t length = header & 0x7fff;
      if (header & 0x8000) {
	// Unexpected file packet.  Could be a M_GOT, M_SKIP, or M_GET
	// TOOD(rushfan): make process_command return the command.
	process_command(length, d);
      } else {
	unique_ptr<char[]> data(new char[length]);
	int length_received = conn_->receive(data.get(), length, d);
	clog << "RECV:  DATA PACKET; len: " << length_received << endl;
	string chunk(data.get(), length_received);
	contents += chunk;
	bytes_received += length_received;
	if (bytes_received >= expected_length) {
	  clog << "        file finished; bytes_received: " << bytes_received << endl;
	  done = true;
	  const string data_line = StringPrintf("%s %u %u", filename.c_str(),
						bytes_received, timestamp);
	  send_command_packet(M_GOT, data_line);
	}
      }
    }
  } catch (timeout_error e) {
    clog << "        timeout_error: " << e.what() << endl;
  }
  clog << "        " << " returning true" << endl;
  // clog << "        contents: " << contents;
  return true;
}

bool BinkP::HandleFileGetRequest(const string& request_line) {
  clog << "       HandleFileGetRequest: request_line: [" << request_line << "]" << endl; 
  vector<string> s = SplitString(request_line, " ");
  const string filename = s.at(0);
  long length = std::stol(s.at(1));
  time_t timestamp = std::stoul(s.at(2));
  long offset = 0;
  if (s.size() >= 4) {
    offset = std::stol(s.at(3));
  }

  auto iter = files_to_send_.find(filename);
  if (iter == std::end(files_to_send_)) {
    clog << "File not found: " << filename;
    return false;
  }
  return SendFileData(iter->second.get());
  // File was sent but wait until we receive M_GOT before we remove it from the list.
}

bool BinkP::HandleFileGotRequest(const string& request_line) {
  clog << "       HandleFileGotRequest: request_line: [" << request_line << "]" << endl; 
  vector<string> s = SplitString(request_line, " ");
  const string filename = s.at(0);
  long length = std::stol(s.at(1));

  auto iter = files_to_send_.find(filename);
  if (iter == std::end(files_to_send_)) {
    clog << "File not found: " << filename;
    return false;
  }

  TransferFile* file = iter->second.get();
  file = nullptr;
  files_to_send_.erase(iter);
  return true;
}

// TODO(rushfan): Remove this.
BinkState BinkP::SendDummyFile(const std::string& filename, char fill, size_t size) {
  clog << "       SendDummyFile: " << filename << endl;
  bool result = SendFilePacket(new InMemoryTransferFile(filename, string(size, fill)));
  return BinkState::UNKNOWN;
}

void BinkP::Run() {
  clog << "STATE: Run (Main Event Loop): " << endl;
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
	clog << "STATE: Done." << endl;
	done = true;
	break;
      }
      process_frames(milliseconds(100));
    }
  } catch (socket_error e) {
    clog << "STATE: BinkP::RunOriginatingLoop() socket_error: " << e.what() << std::endl;
  }
}

bool ParseFileRequestLine(const std::string& request_line, 
			  std::string* filename,
			  long* length,
			  time_t* timestamp,
			  long* offset) {
  vector<string> s = SplitString(request_line, " ");
  if (s.size() < 3) {
    clog << "ERROR: INVALID request_line: "<< request_line
         << "; had < 3 parts.  # parts: " << s.size() << endl;
    return false;
  }
  *filename = s.at(0);
  *length = std::stol(s.at(1));
  *timestamp = std::stoul(s.at(2));
  *offset = 0;
  if (s.size() >= 4) {
    *offset = std::stol(s.at(3));
  }
  return true;
}
  
}  // namespace net
} // namespace wwiv
