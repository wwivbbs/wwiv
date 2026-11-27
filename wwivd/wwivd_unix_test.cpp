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
#include "core/net.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/test/file_helper.h"
#include "core/textfile.h"
#include "fmt/format.h"
#include "sdk/config.h"
#include "sdk/wwivd_config.h"
#include "wwivd/node_manager.h"
#include "wwivd/wwivd.h"

#include "gtest/gtest.h"
#include <cerrno>
#include <fcntl.h>
#include <string>
#include <sys/socket.h>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::wwivd;

#ifdef __linux__

namespace {
bool fd_is_open(int fd) { return fcntl(fd, F_GETFD) != -1 || errno != EBADF; }
} // namespace

// The child must inherit only the caller's socket, not other callers'
// sockets (which are close-on-exec, as wwivd's accepted sockets are), and
// wwivd's copy of the caller's socket must be closed once the child runs.
TEST(ExecCommandAndWaitTest, ChildInheritsOnlyItsOwnSocket) {
  test::FileHelper helper;
  const Config config(helper.TempDir(), config_t{});
  NodeManager nodes(config, ConnectionType::TELNET);

  int mine[2]{-1, -1};
  int other[2]{-1, -1};
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, mine));
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM | SOCK_CLOEXEC, 0, other));

  const auto out = helper.TempDir() / "fds.txt";
  // $$ is the shell's pid; record which of the two fds it has open.
  const auto cmd = fmt::format(
      "for f in {} {}; do if [ -e /proc/$$/fd/$f ]; then echo $f; fi; done > '{}'", mine[0],
      other[0], out.string());

  const wwivd_config_t wc{};
  EXPECT_TRUE(ExecCommandAndWait(wc, nodes, cmd, "[test] ", 1, SocketHandle(mine[0])));

  // Check before opening anything else, which could reuse the fd number.
  // Our copy of the caller's socket is gone; other callers' are untouched.
  EXPECT_FALSE(fd_is_open(mine[0]));
  EXPECT_TRUE(fd_is_open(other[0]));

  TextFile tf(out, "rt");
  ASSERT_TRUE(tf.IsOpen());
  EXPECT_EQ(StrCat(mine[0], "\n"), tf.ReadFileIntoString());

  close(mine[1]);
  close(other[0]);
  close(other[1]);
}

// The child starts in working_dir, and wwivd's own directory is left alone
// (other threads are launching children from it at the same time).
TEST(ExecCommandAndWaitTest, ChildStartsInWorkingDir) {
  test::FileHelper helper;
  const Config config(helper.TempDir(), config_t{});
  NodeManager nodes(config, ConnectionType::TELNET);
  // The quote checks the directory is passed to the shell safely.
  ASSERT_TRUE(helper.Mkdir("it's here"));
  const auto dir = helper.Dir("it's here");
  const auto out = helper.TempDir() / "pwd.txt";
  const auto before = File::current_directory();

  const wwivd_config_t wc{};
  EXPECT_TRUE(ExecCommandAndWait(wc, nodes, fmt::format("pwd -P > '{}'", out.string()), "[test] ",
                                 1, SocketHandle{}, dir));
  EXPECT_EQ(before, File::current_directory());

  TextFile tf(out, "rt");
  ASSERT_TRUE(tf.IsOpen());
  EXPECT_EQ(StrCat(std::filesystem::canonical(dir).string(), "\n"), tf.ReadFileIntoString());
}

TEST(ExecCommandAndWaitTest, MissingWorkingDirFails) {
  test::FileHelper helper;
  const Config config(helper.TempDir(), config_t{});
  NodeManager nodes(config, ConnectionType::TELNET);

  const wwivd_config_t wc{};
  EXPECT_FALSE(ExecCommandAndWait(wc, nodes, "true", "[test] ", 1, SocketHandle{},
                                  helper.TempDir() / "missing"));
}

#endif // __linux__
