#include "networkb_test/fake_connection.h"

#include <stdexcept>
#include <chrono>
#include <cstring>
#include <memory>
#include <iostream>
#include <thread>

#ifndef _WIN32
#include <unistd.h>  // for usleep
#define _sleep(x) usleep((x) * 1000)

#define NO_ERROR 0
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(x) close(x)

#endif  // _WIN32

#include "core/strings.h"
#include "networkb/socket_exceptions.h"

using std::chrono::milliseconds;
using std::chrono::seconds;
using std::clog;
using std::endl;
using std::string;
using std::unique_ptr;
using namespace wwiv::strings;
using namespace wwiv::net;


FakeBinkpPacket::FakeBinkpPacket(const void* data, int size) {
  const char *p = reinterpret_cast<const char*>(data);
  header_ = (*p++) << 8;
  header_ = header_ | *p++;
  is_command_ = (header_ | 0x8000) != 0;
  header_ &= 0x7fff;

  if (is_command_) {
    command_ = *p;
  }
  // size doesn't include the uint16_t header.
  data_ = string(p, size - 2);  
}

FakeBinkpPacket::~FakeBinkpPacket() {}

static bool wait_for(std::function<bool()> predicate, std::chrono::milliseconds d) {
  auto now = std::chrono::steady_clock::now();
  auto end = now + d;
  while (!predicate() && now < end) {
    now = std::chrono::steady_clock::now();
    _sleep(100);
    //std::this_thread::sleep_for(milliseconds(1));
  }
  return predicate();
}

FakeConnection::FakeConnection() {}
FakeConnection::~FakeConnection() {}


uint16_t FakeConnection::read_uint16(std::chrono::milliseconds d) {
  auto predicate = [&]() { return !receive_queue_.empty(); };
  if (!wait_for(predicate, d)) {
    throw timeout_error("timedout on read_uint16");
  }
  const auto& packet = receive_queue_.front();
  uint16_t header = packet.header();
  if (packet.is_command()) {
    header |= 0x8000;
  }
  return header;
}

uint8_t FakeConnection::read_uint8(std::chrono::milliseconds d) {
  auto predicate = [&]() { return !receive_queue_.empty(); };
  if (!wait_for(predicate, d)) {
    throw timeout_error("timedout on read_uint8");
  }

  const FakeBinkpPacket& front = receive_queue_.front();
  if (!front.is_command()) {
    throw std::logic_error("called read_uint8 on a data packet");
  }
  return front.command();
}

int FakeConnection::receive(void* data, int size, std::chrono::milliseconds d) {
  auto predicate = [&]() { return !receive_queue_.empty(); };
  if (!wait_for(predicate, d)) {
    throw timeout_error("timedout on receive");
  }

  const FakeBinkpPacket& front = receive_queue_.front();
  memcpy(data, front.data().data(), size);
  receive_queue_.pop();
  return size;
}

int FakeConnection::send(const void* data, int size, std::chrono::milliseconds d) {
  send_queue_.push(FakeBinkpPacket(data, size));
  return size;
}

FakeBinkpPacket FakeConnection::GetNextPacket() {
  if (send_queue_.empty()) {
    throw std::logic_error("GetNextPacket called on empty queue.");
  }
  FakeBinkpPacket packet = send_queue_.front();
  send_queue_.pop();
  return packet;
}

// Reply to the BinkP with a command.
void FakeConnection::ReplyCommand(int8_t command_id, const string& data) {
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

  receive_queue_.push(FakeBinkpPacket(packet.get(), size));
}
