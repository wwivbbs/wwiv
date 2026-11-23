/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                 Copyright (C)2026, WWIV Software Services              */
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
/*                                                                        */
/**************************************************************************/
#include "gtest/gtest.h"

#include "bbs/exec_socket.h"
#include "bbs/wwivfoss_inband.h"
#include "common/remote_io.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <functional>
#include <string>

using namespace std::chrono;
using namespace std::string_literals;
using namespace wwiv::bbs;

namespace {

// A caller: hands the pump scripted input once, captures what it writes,
// and can hang up at a given time.
class FakeCallerIO final : public wwiv::common::RemoteIO {
public:
  bool open() override { return true; }
  void close(bool) override {}
  unsigned char getW() override { return 0; }
  bool disconnect() override { return true; }
  void purgeIn() override {}
  unsigned int put(unsigned char ch) override {
    output.push_back(static_cast<char>(ch));
    return 1;
  }
  unsigned int write(const char* buffer, unsigned int count, bool) override {
    output.append(buffer, count);
    return count;
  }
  unsigned int read(char* buffer, unsigned int count) override {
    const auto n = std::min<size_t>(count, input.size());
    input.copy(buffer, n);
    input.erase(0, n);
    return static_cast<unsigned int>(n);
  }
  bool connected() override { return !hangup_at || steady_clock::now() < *hangup_at; }
  bool incoming() override { return !input.empty(); }
  [[nodiscard]] unsigned int GetHandle() const override { return 0; }

  std::string input;
  std::string output;
  std::optional<steady_clock::time_point> hangup_at;
};

// Reads from sock until want returns true for what was received, or it
// times out. Returns everything received.
std::string read_until(int sock, const std::function<bool(const std::string&)>& want,
                       milliseconds timeout = milliseconds(5000)) {
  std::string got;
  const auto deadline = steady_clock::now() + timeout;
  char buf[256];
  while (!want(got) && steady_clock::now() < deadline) {
    if (const auto n = recv(sock, buf, sizeof(buf), MSG_DONTWAIT); n > 0) {
      got.append(buf, n);
    } else {
      usleep(10000);
    }
  }
  return got;
}

bool contains(const std::string& s, const std::string& what) {
  return s.find(what) != std::string::npos;
}

// Runs door() in a child process connected to the ExecSocket, pumps it
// against io, and returns the pump result. What the door received from the
// BBS is returned in door_received.
struct PumpRun {
  pump_socket_result_t result;
  std::string door_received;
  milliseconds elapsed;
};

PumpRun run_door(ExecSocket& es, FakeCallerIO& io,
                 const std::function<std::string(int sock)>& door) {
  int fds[2];
  EXPECT_EQ(0, pipe(fds));
  const auto port = es.port();
  const auto pid = fork();
  if (pid == 0) {
    ::close(fds[0]);
    const auto sock = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<uint16_t>(port));
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
      _exit(2);
    }
    const auto received = door(sock);
    (void)!write(fds[1], received.data(), received.size());
    ::close(fds[1]);
    _exit(0);
  }
  ::close(fds[1]);
  PumpRun r{};
  const auto sock = es.accept();
  EXPECT_TRUE(sock.has_value());
  const auto start = steady_clock::now();
  r.result = es.pump_socket(pid, sock.value(), io);
  r.elapsed = duration_cast<milliseconds>(steady_clock::now() - start);
  if (r.result != pump_socket_result_t::process_exit) {
    kill(pid, SIGKILL);
  }
  waitpid(pid, nullptr, 0);
  char buf[1024];
  for (ssize_t n; (n = ::read(fds[0], buf, sizeof(buf))) > 0;) {
    r.door_received.append(buf, n);
  }
  ::close(fds[0]);
  return r;
}

wwivfoss_options_t fast_options() {
  wwivfoss_options_t o{};
  o.heartbeat = seconds(0);
  o.door_hangup_grace = milliseconds(500);
  o.caller_hangup_grace = milliseconds(2000);
  return o;
}

} // namespace

TEST(ExecSocketWWIVFossTest, DataBothWays) {
  ExecSocket es(0);
  ASSERT_TRUE(es.listening());
  es.enable_wwivfoss(fast_options());
  FakeCallerIO io;
  io.input = "a\xFF" "b"s;

  const auto r = run_door(es, io, [](int sock) {
    const auto got = read_until(sock, [](const auto& s) { return contains(s, "b"); });
    const auto out = "hi\xFF\xFF" "!"s;
    send(sock, out.data(), out.size(), 0);
    usleep(300000);
    return got;
  });

  EXPECT_EQ(pump_socket_result_t::process_exit, r.result);
  // DTR up first, then the caller's data with FF doubled.
  EXPECT_EQ("\xFF\x03" "a\xFF\xFF" "b"s, r.door_received);
  EXPECT_EQ("hi\xFF!"s, io.output);
}

TEST(ExecSocketWWIVFossTest, DoorLowersDtr_ReturnsCallerAfterGrace) {
  ExecSocket es(0);
  ASSERT_TRUE(es.listening());
  es.enable_wwivfoss(fast_options());
  FakeCallerIO io;

  const auto r = run_door(es, io, [](int sock) {
    const auto out = "bye\xFF\x01"s;
    send(sock, out.data(), out.size(), 0);
    sleep(30); // Never exits on its own.
    return std::string();
  });

  EXPECT_EQ(pump_socket_result_t::socket_error, r.result);
  EXPECT_EQ("bye", io.output);
  EXPECT_TRUE(io.connected());
  EXPECT_GE(r.elapsed.count(), 400);
  EXPECT_LT(r.elapsed.count(), 5000);
}

TEST(ExecSocketWWIVFossTest, DoorLowersDtr_ExitsWithinGrace) {
  ExecSocket es(0);
  ASSERT_TRUE(es.listening());
  es.enable_wwivfoss(fast_options());
  FakeCallerIO io;

  const auto r = run_door(es, io, [](int sock) {
    const auto out = "\xFF\x01"s;
    send(sock, out.data(), out.size(), 0);
    usleep(100000);
    return std::string();
  });

  EXPECT_EQ(pump_socket_result_t::process_exit, r.result);
}

TEST(ExecSocketWWIVFossTest, CallerHangup_SendsDtrLow) {
  ExecSocket es(0);
  ASSERT_TRUE(es.listening());
  es.enable_wwivfoss(fast_options());
  FakeCallerIO io;
  io.hangup_at = steady_clock::now() + milliseconds(300);

  const auto r = run_door(es, io, [](int sock) {
    // Exit as soon as the BBS reports the caller is gone.
    return read_until(sock, [](const auto& s) { return contains(s, "\xFF\x01"s); });
  });

  EXPECT_EQ(pump_socket_result_t::process_exit, r.result);
  EXPECT_EQ("\xFF\x03\xFF\x01"s, r.door_received);
}

TEST(ExecSocketWWIVFossTest, Heartbeat) {
  ExecSocket es(0);
  ASSERT_TRUE(es.listening());
  auto opts = fast_options();
  opts.heartbeat = seconds(1);
  es.enable_wwivfoss(opts);
  FakeCallerIO io;

  const auto r = run_door(es, io, [](int sock) {
    return read_until(sock, [](const auto&) { return false; }, milliseconds(2500));
  });

  EXPECT_EQ(pump_socket_result_t::process_exit, r.result);
  // The initial FF 03 plus two heartbeats.
  EXPECT_EQ("\xFF\x03\xFF\x03\xFF\x03"s, r.door_received);
}
