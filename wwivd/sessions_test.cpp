/**************************************************************************/
/*                                                                        */
/*                          WWIV BBS Software                             */
/*             Copyright (C)2026, WWIV Software Services                  */
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
#include "wwivd/sessions.h"

#include "core/os.h"
#include "gtest/gtest.h"
#include <atomic>
#include <chrono>
#include <stop_token>

#ifndef _WIN32
#include "core/test/file_helper.h"
#include "sdk/config.h"
#include "sdk/wwivd_config.h"
#include "wwivd/node_manager.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "wwivd/wwivd.h"
#include <cerrno>
#include <csignal>
#include <filesystem>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#endif

using namespace std::chrono;
using namespace std::chrono_literals;
using namespace wwiv::core;
using namespace wwiv::wwivd;

namespace {

// Waits up to 10s for pred, so a test fails instead of hanging.
template <typename Pred> bool eventually(Pred pred) {
  const auto end = steady_clock::now() + 10s;
  while (!pred()) {
    if (steady_clock::now() > end) {
      return false;
    }
    wwiv::os::sleep_for(5ms);
  }
  return true;
}

} // namespace

TEST(SessionsTest, StopWaitsForThreads) {
  std::atomic<bool> ran{false};
  Sessions sessions;
  ASSERT_TRUE(sessions.start([&](const std::stop_token& st) {
    while (!st.stop_requested()) {
      wwiv::os::sleep_for(1ms);
    }
    ran = true;
  }));
  EXPECT_EQ(1u, sessions.running());
  sessions.stop();
  EXPECT_TRUE(ran);
  EXPECT_EQ(0u, sessions.running());
}

TEST(SessionsTest, StartAfterStopIsRefused) {
  Sessions sessions;
  sessions.stop();
  auto ran = false;
  EXPECT_FALSE(sessions.start([&] { ran = true; }));
  EXPECT_TRUE(sessions.stopping());
  sessions.stop();
  EXPECT_FALSE(ran);
}

TEST(SessionsTest, FinishedThreadsArePruned) {
  Sessions sessions;
  for (auto i = 0; i < 5; i++) {
    ASSERT_TRUE(sessions.start([] {}));
  }
  EXPECT_TRUE(eventually([&] { return sessions.running() == 0; }));
}

TEST(SessionsTest, ExceptionInThreadIsCaught) {
  Sessions sessions;
  ASSERT_TRUE(sessions.start([] { throw std::runtime_error("boom"); }));
  EXPECT_TRUE(eventually([&] { return sessions.running() == 0; }));
}

TEST(SessionsTest, SleepUnlessStopped) {
  std::stop_source src;
  EXPECT_TRUE(sleep_unless_stopped(src.get_token(), 1ms));
  src.request_stop();
  const auto start = steady_clock::now();
  EXPECT_FALSE(sleep_unless_stopped(src.get_token(), 10s));
  EXPECT_LT(steady_clock::now() - start, 1s);
}

#ifndef _WIN32

namespace {

class SocketPair {
public:
  SocketPair() { EXPECT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, fds_)); }
  ~SocketPair() {
    if (fds_[1] != -1) {
      close(fds_[1]);
    }
  }
  /** Ours; ownership goes to the caller. */
  SocketHandle take_ours() { return SocketHandle(std::exchange(fds_[0], -1)); }
  [[nodiscard]] int theirs() const { return fds_[1]; }

private:
  int fds_[2]{-1, -1};
};

} // namespace

// A thread blocked reading a caller's socket is woken when stop() shuts it
// down, as wwivd's matrix logon and telnet proxy are.
TEST(SessionsTest, StopShutsDownWatchedSockets) {
  SocketPair sp;
  Sessions sessions;
  auto sock = sessions.watch(sp.take_ours());
  const auto fd = sock.get();
  std::atomic<ssize_t> got{-2};
  ASSERT_TRUE(sessions.start([&] {
    char c;
    got = recv(fd, &c, 1, 0);
  }));
  sessions.stop();
  EXPECT_EQ(0, got);
}

// A caller turned away (BUSY, blocked, quit at the matrix) ends its thread
// with its socket still watched, so the handler un-watches it as it is
// destroyed. That must not deadlock with start() pruning finished threads.
TEST(SessionsTest, ThreadsOwningWatchedSocketsCanBePruned) {
  Sessions sessions;
  for (auto i = 0; i < 200; i++) {
    SocketPair sp;
    ASSERT_TRUE(sessions.start([sock = sessions.watch(sp.take_ours())] {}));
  }
  EXPECT_TRUE(eventually([&] { return sessions.running() == 0; }));
}

TEST(SessionsTest, WatchAfterStopShutsDownAtOnce) {
  SocketPair sp;
  Sessions sessions;
  sessions.stop();
  auto sock = sessions.watch(sp.take_ours());
  char c;
  EXPECT_EQ(0, recv(sock.get(), &c, 1, 0));
}

// Once released (handed to a child), stop() leaves the socket alone.
TEST(SessionsTest, ReleasedSocketIsNotShutDown) {
  SocketPair sp;
  Sessions sessions;
  auto sock = sessions.watch(sp.take_ours()).release();
  sessions.stop();
  ASSERT_EQ(1, send(sp.theirs(), "x", 1, 0));
  char c;
  EXPECT_EQ(1, recv(sock.get(), &c, 1, 0));
}

TEST(SessionsTest, WatchedSocketStopsBeingWatchedWhenClosed) {
  SocketPair sp;
  Sessions sessions;
  { auto sock = sessions.watch(sp.take_ours()); }
  // The closed number may be reused by now; stop() must not touch it.
  SocketPair reused;
  sessions.stop();
  auto mine = reused.take_ours();
  ASSERT_EQ(1, send(reused.theirs(), "x", 1, 0));
  char c;
  EXPECT_EQ(1, recv(mine.get(), &c, 1, 0));
}

namespace {

// Runs cmd as wwivd runs a node, on a sessions thread, and waits until the
// child is registered.
void start_child(Sessions& sessions, const std::string& cmd, std::atomic<bool>& done) {
  ASSERT_TRUE(sessions.start([&sessions, cmd, &done] {
    wwiv::core::test::FileHelper helper;
    const wwiv::sdk::Config config(helper.TempDir(), wwiv::sdk::config_t{});
    NodeManager nodes(config, ConnectionType::TELNET);
    const wwiv::sdk::wwivd_config_t wc{};
    ExecCommandAndWait(wc, nodes, cmd, "[test] ", 1, SocketHandle{}, {}, &sessions);
    done = true;
  }));
  ASSERT_TRUE(eventually([&] { return sessions.children() == 1; }));
}

// The shell starts a grandchild (as a BBS starts a door, or sh runs the
// BBS without exec'ing it) and writes its pid to pid_file before waiting.
std::string with_grandchild(const std::string& prefix, const std::filesystem::path& pid_file) {
  return fmt::format("{}sleep 30 & echo $! > '{}'; wait", prefix, pid_file.string());
}

pid_t read_pid(const std::filesystem::path& pid_file) {
  pid_t pid = -1;
  EXPECT_TRUE(eventually([&] {
    TextFile tf(pid_file, "rt");
    pid = wwiv::strings::to_number<pid_t>(tf.ReadFileIntoString());
    return pid > 0;
  }));
  return pid;
}

bool gone(pid_t pid) { return kill(pid, 0) == -1 && errno == ESRCH; }

} // namespace

// The hangup goes to the child's whole process group, so it reaches what
// the child started too.
TEST(SessionsTest, StopHangsUpChildren) {
  wwiv::core::test::FileHelper helper;
  const auto pid_file = helper.TempDir() / "pid";
  Sessions sessions(10s);
  std::atomic<bool> done{false};
  start_child(sessions, with_grandchild("", pid_file), done);
  const auto grandchild = read_pid(pid_file);
  const auto start = steady_clock::now();
  sessions.stop();
  EXPECT_TRUE(done);
  EXPECT_LT(steady_clock::now() - start, 5s);
  EXPECT_EQ(0u, sessions.children());
  EXPECT_TRUE(eventually([&] { return gone(grandchild); }));
}

// sh doesn't always exec the BBS, and the hangup kills the shell at once.
// Shutdown must still wait for the BBS behind it to finish logging off.
TEST(SessionsTest, StopWaitsForTheGroupAfterTheShellExits) {
  wwiv::core::test::FileHelper helper;
  const auto done_file = helper.TempDir() / "logged_off";
  Sessions sessions(10s);
  std::atomic<bool> done{false};
  // The subshell stands in for the BBS: on SIGHUP it takes a moment to log
  // off. The outer shell just dies.
  start_child(sessions,
              fmt::format("(trap 'sleep 0.5; echo x > \"{}\"; exit 0' HUP; "
                          "while true; do sleep 0.05; done) & wait",
                          done_file.string()),
              done);
  // Let the subshell set its trap.
  wwiv::os::sleep_for(200ms);
  sessions.stop();
  EXPECT_TRUE(done);
  EXPECT_TRUE(std::filesystem::exists(done_file));
}

TEST(SessionsTest, StopKillsChildrenThatIgnoreHangup) {
  wwiv::core::test::FileHelper helper;
  const auto pid_file = helper.TempDir() / "pid";
  Sessions sessions(200ms);
  std::atomic<bool> done{false};
  start_child(sessions, with_grandchild("trap '' HUP; ", pid_file), done);
  const auto grandchild = read_pid(pid_file);
  const auto start = steady_clock::now();
  sessions.stop();
  EXPECT_TRUE(done);
  EXPECT_LT(steady_clock::now() - start, 5s);
  EXPECT_TRUE(eventually([&] { return gone(grandchild); }));
}

#endif // _WIN32
