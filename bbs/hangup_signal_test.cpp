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
#include "bbs/hangup_signal.h"

#include "gtest/gtest.h"

#ifdef __unix__
#include "core/scope_exit.h"
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

using namespace wwiv::bbs;
using namespace wwiv::core;

class HangupSignalTest : public ::testing::TestWithParam<int> {};

// The signal shuts the caller's socket down, so the BBS's reader sees end
// of file, as it does when the caller hangs up.
TEST_P(HangupSignalTest, SignalShutsDownSocket) {
  int fds[2]{-1, -1};
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
  auto close_fds = finally([&] {
    close(fds[0]);
    close(fds[1]);
  });
  HangupOnSignal(fds[0]);
  auto stop = finally(StopHangupOnSignal);

  ASSERT_EQ(0, raise(GetParam()));

  // The process is still here, and the caller's end sees the hangup.
  char c;
  EXPECT_EQ(0, recv(fds[1], &c, 1, 0));
  EXPECT_EQ(0, recv(fds[0], &c, 1, 0));
}

INSTANTIATE_TEST_SUITE_P(Signals, HangupSignalTest, ::testing::Values(SIGHUP, SIGTERM));

// Once the caller has been hung up, a second signal (wwivd's SIGHUP after
// systemd's SIGTERM) must neither end the BBS mid-logoff nor touch a socket
// that reused the number.
TEST(HangupSignalTest, SignalsAfterStopDoNothing) {
  int fds[2]{-1, -1};
  ASSERT_EQ(0, socketpair(AF_UNIX, SOCK_STREAM, 0, fds));
  auto close_fds = finally([&] {
    close(fds[0]);
    close(fds[1]);
  });
  HangupOnSignal(fds[0]);
  StopHangupOnSignal();

  ASSERT_EQ(0, raise(SIGHUP));
  ASSERT_EQ(0, raise(SIGTERM));

  ASSERT_EQ(1, send(fds[1], "x", 1, 0));
  char c;
  EXPECT_EQ(1, recv(fds[0], &c, 1, 0));
}

#endif // __unix__
