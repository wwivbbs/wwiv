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

#include <filesystem>
#include <sys/socket.h>
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
