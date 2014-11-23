#include "networkb/binkp.h"
#include "networkb/connection.h"

#include <chrono>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <map>
#include <string>

#include "core/strings.h"

using std::chrono::seconds;
using std::chrono::system_clock;
using std::clog;
using std::cout;
using std::endl;
using std::map;
using std::string;
using std::unique_ptr;
using wwiv::net::Connection;
using wwiv::strings::StringPrintf;

namespace wwiv {
namespace net {

TransferFile::TransferFile(const string& filename, const string& full_pathname, long timestamp)
  : filename_(filename), full_pathname_(full_pathname), timestamp_(timestamp) {}

TransferFile::~TransferFile() {}

const string TransferFile::as_packet_data(int offset) const {
  return StringPrintf("%s %u %d", filename_, timestamp_, offset);
}

InMemoryTransferFile::InMemoryTransferFile(const std::string& filename, const std::string& contents, long timestamp)
  : TransferFile(filename, filename, system_clock::to_time_t(system_clock::now())), contents_(contents) {}
InMemoryTransferFile::~InMemoryTransferFile() {}

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

bool BinkP::process_command(int16_t length) {
  const uint8_t command_id = conn_->read_uint8(seconds(3));
  unique_ptr<char[]> data(new char[length]);
  conn_->receive(data.get(), length - 1,seconds(3));
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
  } break;
  case M_GOT: {
    clog << "M_GOT: " << s << endl;
  } break;
  default: {
    clog << "UNHANDLED COMMAND: " << command_id_to_name(command_id) 
         << " data: " << s << endl;
  } break;
  }
  return true;
}

bool BinkP::process_data(int16_t length) {
  unique_ptr<char[]> data(new char[length]);
  int length_received = conn_->receive(data.get(), length - 1, seconds(3));
  string s(data.get(), length - 1);
  clog << "len: " << length_received << "; data: " << s << endl;
  return true;
}

bool BinkP::process_one_frame() {
  uint16_t header = conn_->read_uint16(seconds(2));
  if (header & 0x8000) {
    return process_command(header & 0x7fff);
  } else {
    return process_data(header & 0x7fff);
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

bool BinkP::send_data_packet(const char* data, std::size_t size) {
  // for now assume everything fits within a single frame.
  uint16_t packet_length = size & 0x7fff;
  std::unique_ptr<char[]> packet(new char[packet_length + 2]);
  uint8_t b0 = ((packet_length & 0x7f00) >> 8);
  uint8_t b1 = packet_length & 0x00ff;
  char *p = packet.get();
  *p++ = b0;
  *p++ = b1;
  memcpy(p, data, size);

  conn_->send(packet.get(), packet_length + 2, seconds(3));
  clog << "Sending: data: packet_length: " << (int) packet_length
       << "; size: " << size
       << "; data: " << string(packet.get(), size + 2) << endl;
  return true;
}

BinkState BinkP::ConnInit() {
  clog << "ConnInit" << endl;
  try {
    while (true) {
      process_one_frame();
    }
  } catch (wwiv::net::timeout_error e) {
    clog << e.what() << endl;
  }
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
    process_one_frame();
  }
  return BinkState::WAIT_OK;
}

BinkState BinkP::WaitOk() {
  clog << "WaitOk" << endl;
  if (ok_received) {
    return BinkState::UNKNOWN;
  }
  while (!ok_received) {
    process_one_frame();
  }
  return BinkState::UNKNOWN;
}

bool BinkP::SendFilePacket(TransferFile* file) {
  files_to_send_[file->filename()] = unique_ptr<TransferFile>(file);
  return true;
}

// TODO(rushfan): Remove this.
BinkState BinkP::SendDummyFile() {
  clog << "SendDummyFile" << endl;
  // Send a file 5 chars long with timestamp of around Sat Nov 22 15:23:00 PST 2014
  send_command_packet(M_FILE, "test.txt 5 1165408993 0"); 

  try {
    process_one_frame();
  } catch (wwiv::net::timeout_error e) {
    clog << "process_one_frame: " << e.what() << endl;
  }

  send_data_packet("ABCDE", 5);
  send_command_packet(M_EOB, "");
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
    while (true) {
      process_one_frame();
    }
  } catch (wwiv::net::socket_error e) {
    clog << e.what() << std::endl;
  }
}

  
}  // namespace net
} // namespace wwiv
