#include "networkb_test/fake_connection.h"

#include <stdexcept>
#include <chrono>
#include <cstring>
#include <memory>
#include <iostream>
#include <thread>

#ifndef _WIN32
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
    command_ = *p++;
  }
  std::ptrdiff_t remaining = size - (p - reinterpret_cast<const char*>(data));
  data_ = string(p, remaining);
}

FakeBinkpPacket::~FakeBinkpPacket() {}

static bool wait_for(std::function<bool()> predicate, std::chrono::milliseconds d) {
  auto now = std::chrono::steady_clock::now();
  auto end = now + d;
  while (!predicate() && std::chrono::steady_clock::now() < end) {
    std::this_thread::sleep_for(milliseconds(100));
  }
  return predicate();
}

uint16_t FakeConnection::read_uint16(std::chrono::milliseconds d) {
  auto predicate = [&]() { return !receive_queue_.empty(); };
  if (!wait_for(predicate, d)) {
    throw timeout_error("timedout on read_uint16");
  }
  return receive_queue_.front().header();
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
