#include "networkb/binkp.h"
#include "networkb/connection.h"

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <map>
#include <string>
#include <vector>

#include "core/strings.h"

using std::chrono::seconds;
using std::chrono::system_clock;
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

TransferFile::TransferFile(const string& filename, time_t timestamp)
  : filename_(filename), timestamp_(timestamp) {}

TransferFile::~TransferFile() {}

const string TransferFile::as_packet_data(int size, int offset) const {
  return StringPrintf("%s %u %u %d", filename_.c_str(), size, timestamp_, offset);
}

InMemoryTransferFile::InMemoryTransferFile(const std::string& filename, const std::string& contents)
  : TransferFile(filename, system_clock::to_time_t(system_clock::now())), 
    contents_(contents) {}

InMemoryTransferFile::~InMemoryTransferFile() {}

const string InMemoryTransferFile::as_packet_data(int offset) const {
  return TransferFile::as_packet_data(contents_.size(), offset);
}

bool InMemoryTransferFile::GetChunk(char* chunk, size_t start, size_t size) {
  if ((start + size) > contents_.size()) {
    clog << "ERROR InMemoryTransferFile::GetChunk (start + size) > contents_.size(): values["
         << (start + size) << ", " << contents_.size() << "]" << endl;
    return false;
  }
  memcpy(chunk, &contents_.data()[start], size);
  return true;
}

BinkP::BinkP(Connection* conn) : conn_(conn) {}
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
  unique_ptr<char[]> data(new char[length]);
  conn_->receive(data.get(), length - 1, d);
  string s(data.get(), length - 1);
  switch (command_id) {
  case M_NUL: {
    clog << "M_NUL: " << s << endl;
  } break;
  case M_ADR: {
    clog << "M_ADR: " << s << endl;
    // TODO(rushfan): tokenize into addresses
    address_list = s;
  } break;
  case M_OK: {
    clog << "M_OK: " << s << endl;
    ok_received = true;
  } break;
  case M_GET: {
    clog << "M_GET: " << s << endl;
    HandleFileGetRequest(s);
  } break;
  case M_GOT: {
    clog << "M_GOT: " << s << endl;
    HandleFileGotRequest(s);
  } break;
  default: {
    clog << "UNHANDLED COMMAND: " << command_id_to_name(command_id) 
         << " data: " << s << endl;
  } break;
  }
  return true;
}

bool BinkP::process_data(int16_t length, std::chrono::milliseconds d) {
  unique_ptr<char[]> data(new char[length]);
  int length_received = conn_->receive(data.get(), length - 1, d);
  string s(data.get(), length - 1);
  clog << "len: " << length_received << "; data: " << s << endl;
  return true;
}

bool BinkP::maybe_process_all_frames(std::chrono::milliseconds d) {
  try {
    while (true) {
      process_one_frame(d);
    }
  } catch (timeout_error ignores) {}
  return true;
}

bool BinkP::process_one_frame(std::chrono::milliseconds d) {
  uint16_t header = conn_->read_uint16(d);
  if (header & 0x8000) {
    return process_command(header & 0x7fff, d);
  } else {
    return process_data(header & 0x7fff, d);
  }
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

  conn_->send(packet.get(), size, seconds(3));
  clog << "Sending: command: " << command_id_to_name(command_id)
       << "; packet_length: " << (packet_length & 0x7fff)
       << "; data: " << string(packet.get(), size) << endl;
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
  clog << "Sending: data: packet_length: " << (int) packet_length << endl;
  return true;
}

BinkState BinkP::ConnInit() {
  clog << "ConnInit" << endl;
  maybe_process_all_frames(seconds(2));
  return BinkState::WAIT_CONN;
}

BinkState BinkP::WaitConn() {
  clog << "WaitConn" << endl;
  send_command_packet(M_NUL, "OPT wwivnet");
  send_command_packet(M_NUL, "SYS NETWORKB test app");
  send_command_packet(M_NUL, "ZYZ Unknown Sysop");
  send_command_packet(M_NUL, "VER networkb/0.0 binkp/1.0");
  send_command_packet(M_NUL, "LOC San Francisco, CA");
  send_command_packet(M_ADR, "20000:20000/2@wwivnet");
  return BinkState::SEND_PASSWORD;
}

BinkState BinkP::SendPasswd() {
  clog << "SendPasswd" << endl;
  send_command_packet(M_PWD, "-");
  return BinkState::WAIT_ADDR;
}

BinkState BinkP::WaitAddr() {
  clog << "WaitAddr" << endl;
  while (address_list.empty()) {
    process_one_frame(seconds(5));
  }
  return BinkState::WAIT_OK;
}

BinkState BinkP::WaitOk() {
  clog << "WaitOk" << endl;
  if (ok_received) {
    return BinkState::UNKNOWN;
  }
  while (!ok_received) {
    process_one_frame(seconds(5));
  }
  return BinkState::UNKNOWN;
}

bool BinkP::SendFilePacket(TransferFile* file) {
  string filename(file->filename());
  clog << "SendFilePacket: " << filename << endl;
  files_to_send_[filename] = unique_ptr<TransferFile>(file);
  send_command_packet(M_FILE, file->as_packet_data(0));

  maybe_process_all_frames(seconds(5));

  // file* may not be viable anymore if it was already send.
  auto iter = files_to_send_.find(filename);
  if (iter != std::end(files_to_send_)) {
    // We have the file still to send.
    SendFileData(file);
  }
  return true;
}

bool BinkP::SendFileData(TransferFile* file) {
  clog << "SendFilePacket: " << file->filename() << endl;
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
    maybe_process_all_frames(seconds(1));
  }
  return true;
}

bool BinkP::HandleFileGetRequest(const string& request_line) {
  clog << "HandleFileGetRequest: request_line: [" << request_line << "]" << endl; 
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
  clog << "HandleFileGotRequest: request_line: [" << request_line << "]" << endl; 
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
BinkState BinkP::SendDummyFile() {
  clog << "SendDummyFile" << endl;
  const string dummy_filename = "test.txt";
  bool result = SendFilePacket(new InMemoryTransferFile(dummy_filename, string(40000, 'A')));
  return BinkState::UNKNOWN;
}

void BinkP::Run() {
  BinkState state = BinkState::CONN_INIT;
  try {
    while (state != BinkState::UNKNOWN) {
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
        break;
      case BinkState::IF_SECURE:
        break;
      case BinkState::WAIT_OK:
        state = WaitOk();
        // HACK
        SendDummyFile();
        break;
      }
    }
    clog << "End of the line..." << endl;
    for (int count=1; count < 4; count++) {
      try {
        count++;
        maybe_process_all_frames(seconds(5));
      } catch (timeout_error e) {
        clog << "looping for more data. " << e.what() << std::endl;
      }
    }
  } catch (socket_error e) {
    clog << e.what() << std::endl;
  }
}
  
}  // namespace net
} // namespace wwiv
