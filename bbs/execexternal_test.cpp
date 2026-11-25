/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2026, WWIV Software Services               */
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
#include "bbs/execexternal.h"

#include "bbs/bbs_helper.h"
#include "bbs/exec.h"
#include "core/file.h"
#include "fmt/format.h"
#include "gtest/gtest.h"
#include "sdk/vardec.h"

#include <chrono>
#include <csignal>
#include <filesystem>
#include <fstream>
#include <functional>
#include <string>
#include <sys/socket.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using namespace wwiv::core;

class ExecExternalTest : public ::testing::Test {
protected:
  void SetUp() override {
    // Running a program changes the current directory to the helper's BBS
    // directory, which is deleted at teardown.
    cwd_ = std::filesystem::current_path();
    helper.SetUp();
    ASSERT_TRUE(helper.files().Mkdir("temp"));
    helper.sess().dirs().temp_directory(helper.files().DirName("temp"));
    a()->init_status_and_instances_for_test();
  }

  void TearDown() override { std::filesystem::current_path(cwd_); }

  [[nodiscard]] TestRemoteIO* remote() const {
    return static_cast<TestRemoteIO*>(helper.io()->remote_io());
  }

  BbsHelper helper;
  std::filesystem::path cwd_;
};

// #1737: the rest of the line that picked the program (here the LF of the
// caller's CR LF) must not be left queued for the program.
TEST_F(ExecExternalTest, PurgesQueuedCallerInput) {
  helper.io()->push_remote_keys("\n");
  ASSERT_TRUE(remote()->incoming());

  wwiv::bbs::CommandLine cl("true");
  EXPECT_EQ(0, ExecuteExternalProgram(cl, EFLAG_NOHUP));

  EXPECT_FALSE(remote()->incoming());
}

// @basic: scripts read the caller's input through the BBS, so they need the
// purge too.
TEST_F(ExecExternalTest, PurgesQueuedCallerInput_BasicScript) {
  helper.io()->push_remote_keys("\n");

  wwiv::bbs::CommandLine cl("@basic:no_such_script");
  [[maybe_unused]] auto _ = ExecuteExternalProgram(cl, EFLAG_NOHUP);

  EXPECT_FALSE(remote()->incoming());
}

// A program that reads the caller's connection itself (the default exec
// mode) must not get input that was pending before it started, whether
// queued in the BBS or still waiting on the connection.
TEST_F(ExecExternalTest, ProgramReadingConnectionGetsNoPendingInput) {
  int sv[2];
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, sv));
  remote()->handle = static_cast<unsigned int>(sv[0]);
  helper.io()->push_remote_keys("\r");
  ASSERT_EQ(1, write(sv[1], "\n", 1));

  // Reads one byte from the connection, giving up after a second.
  const auto got = FilePath(helper.files().DirName("temp"), "got");
  wwiv::bbs::CommandLine cl(
      fmt::format("timeout 1 head -c 1 <&{} > {}", sv[0], got.string()));
  [[maybe_unused]] auto _ = ExecuteExternalProgram(cl, EFLAG_NOHUP);

  ASSERT_TRUE(File::Exists(got));
  EXPECT_EQ(0u, std::filesystem::file_size(got));
  EXPECT_FALSE(remote()->incoming());
  close(sv[0]);
  close(sv[1]);
}

// #1735: socket doors (Listen Socket Port) when the caller hangs up.
class ExecExternalSocketDoorTest : public ExecExternalTest {
protected:
  void SetUp() override {
    ExecExternalTest::SetUp();
    a()->set_door_exit_grace_seconds(1);
    // The caller has already hung up when the door starts.
    remote()->is_connected = false;
  }

  [[nodiscard]] std::filesystem::path temp(const std::string& name) {
    return FilePath(helper.files().DirName("temp"), name);
  }

  // Runs script under bash as a socket door connected to %Z. Returns the
  // exit status and sets elapsed_.
  int RunDoor(const std::string& script) {
    wwiv::bbs::CommandLine cl(
        fmt::format("bash -c 'exec 3<>/dev/tcp/127.0.0.1/%Z; {}'", script));
    const auto start = std::chrono::steady_clock::now();
    const auto status = ExecuteExternalProgram(cl, EFLAG_NOHUP | EFLAG_LISTEN_SOCK);
    elapsed_ = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - start);
    return status;
  }

  std::chrono::milliseconds elapsed_{};
};

// Waits up to timeout for done() to return true.
static bool WaitFor(const std::function<bool()>& done,
                    std::chrono::milliseconds timeout = std::chrono::milliseconds(5000)) {
  const auto deadline = std::chrono::steady_clock::now() + timeout;
  while (!done()) {
    if (std::chrono::steady_clock::now() >= deadline) {
      return false;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
  }
  return true;
}

// The door sees its socket close, finishes up and exits; it isn't signalled.
TEST_F(ExecExternalSocketDoorTest, DoorExitsOnItsOwn) {
  const auto out = temp("out");
  const auto status = RunDoor(fmt::format("cat <&3 >/dev/null; echo saved > {}; exit 7",
                                          out.string()));

  ASSERT_TRUE(WIFEXITED(status)) << status;
  EXPECT_EQ(7, WEXITSTATUS(status));
  EXPECT_TRUE(File::Exists(out));
  EXPECT_LT(elapsed_.count(), 1000);
}

// A door that ignores the hangup is stopped after the grace period, along
// with what it started (here a background sleep standing in for an
// emulator under a launcher script).
TEST_F(ExecExternalSocketDoorTest, DoorIgnoresHangup_ProcessGroupStopped) {
  const auto kid_pid = temp("kid.pid");
  const auto status =
      RunDoor(fmt::format("sleep 30 & echo $! > {}; wait", kid_pid.string()));

  EXPECT_TRUE(WIFSIGNALED(status)) << status;
  EXPECT_GE(elapsed_.count(), 900);
  pid_t kid = 0;
  std::ifstream(kid_pid) >> kid;
  ASSERT_GT(kid, 0);
  EXPECT_TRUE(WaitFor([kid] { return kill(kid, 0) == -1 && errno == ESRCH; }))
      << "the door's child " << kid << " is still running";
}

// Once the door itself exits on SIGTERM, a process it left behind on
// purpose (like qemudoor.sh's copy-back) is not SIGKILLed.
TEST_F(ExecExternalSocketDoorTest, DoorExitsOnSigterm_LeftoverProcessFinishes) {
  const auto done = temp("done");
  [[maybe_unused]] const auto status = RunDoor(fmt::format(
      "(trap \"\" TERM; sleep 2; touch {}) & sleep 30", done.string()));

  EXPECT_TRUE(WaitFor([&done] { return File::Exists(done); }));
}

// The process group of the program that ran, as written to out by
// `ps -o pgid=`.
static pid_t ReadPgid(const std::filesystem::path& out) {
  pid_t pgid = 0;
  std::ifstream(out) >> pgid;
  return pgid;
}

// A socket door that bbs pumps gets its own process group, so it can be
// stopped along with whatever it started.
TEST_F(ExecExternalSocketDoorTest, DoorGetsItsOwnProcessGroup) {
  const auto out = temp("pgid");
  [[maybe_unused]] const auto status = RunDoor(fmt::format("ps -o pgid= -p $$ > {}", out.string()));

  const auto pgid = ReadPgid(out);
  ASSERT_GT(pgid, 0);
  EXPECT_NE(getpgrp(), pgid);
}

// In a local session there's no socket to pump, so the door is just a
// child: it stays in bbs's process group and keeps the terminal (in a
// background group it would be stopped on reading it, and never return).
TEST_F(ExecExternalSocketDoorTest, LocalSession_DoorStaysInBbsProcessGroup) {
  helper.sess().ok_modem_stuff(false);
  const auto out = temp("pgid");
  wwiv::bbs::CommandLine cl(fmt::format("ps -o pgid= -p $$ > {}", out.string()));
  [[maybe_unused]] const auto status =
      ExecuteExternalProgram(cl, EFLAG_NOHUP | EFLAG_LISTEN_SOCK);

  EXPECT_EQ(getpgrp(), ReadPgid(out));
}
