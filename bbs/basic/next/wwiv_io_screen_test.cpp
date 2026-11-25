/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "bbs/application.h"
#include "bbs/basic/next/next_engine.h"
#include "bbs/basic/next/type_ahead_test_util.h"
#include "bbs/bbs.h"
#include "bbs/bbs_helper.h"
#include "common/context.h"
#include "common/input.h"
#include "common/output.h"
#include "sdk/user.h"

#include <string>

using namespace wwiv::bbs::basic;
using namespace wwiv::sdk;
using testing::HasSubstr;
using testing::Not;

// The wwiv.io functions for full-screen UIs (new engine only).
class WwivIoScreenTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
    SetAnsi(true);
    type_ahead::HangUpWhenKeysRunOut(helper.context().session_context(), a()->sess());
  }

  void TearDown() override {
    type_ahead::StopTypingAhead();
    remote()->keys.clear();
  }

  void SetAnsi(bool on) {
    if (on) {
      helper.user()->set_flag(User::flag_ansi);
    } else {
      helper.user()->clear_flag(User::flag_ansi);
    }
  }

  TestRemoteIO* remote() { return static_cast<TestRemoteIO*>(helper.io()->remote_io()); }

  // Keys sent by the caller over the connection, where ANSI key sequences
  // (ESC [ A...) come from.
  void SendKeys(const std::string& keys) {
    a()->sess().incom(true);
    a()->sess().ok_modem_stuff(true);
    remote()->keys = keys;
  }

  // Runs text; returns what it printed (without color and cursor codes).
  std::string Run(const std::string& text, bool* ok = nullptr) {
    helper.io()->Clear();
    NextEngine engine(bin, bout, helper.config(), &helper.context());
    const auto r = engine.RunScript("test", text);
    if (ok) {
      *ok = r;
    }
    return helper.io()->captured();
  }

  BbsHelper helper{};
};

TEST_F(WwivIoScreenTest, OkAnsi) {
  EXPECT_EQ("TRUE\r\n", Run("print wwiv.io.okansi()\n"));
  SetAnsi(false);
  EXPECT_EQ("FALSE\r\n", Run("print wwiv.io.okansi()\n"));
}

TEST_F(WwivIoScreenTest, GotoXY) {
  Run("wwiv.io.gotoxy(10, 5)\n");
  // Row first in the ANSI sequence.
  EXPECT_THAT(helper.io()->rcaptured(), HasSubstr("\x1b[5;10H"));
}

TEST_F(WwivIoScreenTest, GotoXY_NoAnsi) {
  SetAnsi(false);
  Run("wwiv.io.gotoxy(10, 5)\n");
  EXPECT_THAT(helper.io()->rcaptured(), Not(HasSubstr("\x1b[")));
}

TEST_F(WwivIoScreenTest, GotoXY_StartsAtOne) {
  bool ok{true};
  const auto out = Run("wwiv.io.gotoxy(0, 1)\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("GOTOXY: x and y start at 1, got 0, 1"));
}

TEST_F(WwivIoScreenTest, ClrEol) {
  Run("wwiv.io.clreol()\n");
  EXPECT_THAT(helper.io()->rcaptured(), HasSubstr("\x1b[K"));
  SetAnsi(false);
  Run("wwiv.io.clreol()\n");
  EXPECT_THAT(helper.io()->rcaptured(), Not(HasSubstr("\x1b[K")));
}

TEST_F(WwivIoScreenTest, Ansic) {
  Run("wwiv.io.ansic(1)\nwwiv.io.outstr(\"x\")\n");
  // The color code comes before the text.
  const auto r = helper.io()->rcaptured();
  EXPECT_THAT(r, HasSubstr("\x1b["));
  EXPECT_LT(r.find("\x1b["), r.find('x'));
}

TEST_F(WwivIoScreenTest, Ansic_Range) {
  bool ok{true};
  const auto out = Run("wwiv.io.ansic(10)\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("ANSIC: color must be from 0 to 9, got 10"));
}

TEST_F(WwivIoScreenTest, Prt) {
  EXPECT_EQ("ab", Run("wwiv.io.prt(3, \"a\", \"b\")\n"));
  bool ok{true};
  Run("wwiv.io.prt(12, \"a\")\n", &ok);
  EXPECT_FALSE(ok);
}

TEST_F(WwivIoScreenTest, Pla) {
  EXPECT_EQ("line\r\nFALSE\r\n", Run("print wwiv.io.pla(\"line\")\n"));
}

TEST_F(WwivIoScreenTest, Onek) {
  // x isn't one of the keys; q is, and comes back in upper case.
  type_ahead::TypeAhead("xq");
  EXPECT_EQ("Q\r\n", Run("print wwiv.io.onek(\"yq\")\n"));
}

// Once the caller hangs up, ONEK stops waiting (returning the first key) and
// the script stops at the next statement.
TEST_F(WwivIoScreenTest, Onek_HangUp) {
  const auto out = Run("k = wwiv.io.onek(\"qy\")\nprint \"after\"\n");
  EXPECT_THAT(out, Not(HasSubstr("after")));
  EXPECT_TRUE(helper.sess().hangup());
}

TEST_F(WwivIoScreenTest, Onek_NoKeys) {
  bool ok{true};
  const auto out = Run("k = wwiv.io.onek(\"\")\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("ONEK: expected at least one key"));
}

TEST_F(WwivIoScreenTest, ReadKey) {
  // Up, Page Down, F1, a, Enter, Home. (F1 as ESC [ 11 ~: WWIV's key
  // parser doesn't map xterm's ESC O P yet.)
  SendKeys("\x1b[A\x1b[6~\x1b[11~a\r\x1b[H");
  EXPECT_EQ("UP/PGDN/F1/a/13/HOME\r\n", Run(R"(
a = wwiv.io.readkey()
b = wwiv.io.readkey()
c = wwiv.io.readkey()
d = wwiv.io.readkey()
e = ASC(wwiv.io.readkey())
f = wwiv.io.readkey()
print a; "/"; b; "/"; c; "/"; d; "/"; e; "/"; f
)"));
}

// Keys the BBS handles itself (Ctrl+T shows the time left) and broken escape
// sequences are skipped; they don't look like a hang-up.
TEST_F(WwivIoScreenTest, ReadKey_SkipsHandledKeys) {
  SendKeys("\x14\x1b[1111111111111111a");
  EXPECT_EQ("a\r\n", Run("print wwiv.io.readkey()\n"));
}

// Digits and "." come back as typed, not as the number pad's arrow keys
// (8 = up and so on), so a script can read a menu choice like "1".
TEST_F(WwivIoScreenTest, ReadKey_Digits) {
  SendKeys("0123456789.");
  EXPECT_EQ("0123456789.\r\n", Run(R"(
s = ""
for i = 1 to 11
  s = s + wwiv.io.readkey()
next
print s
)"));
}

// Once the caller hangs up, READKEY returns "" and the script stops.
TEST_F(WwivIoScreenTest, ReadKey_HangUp) {
  bool ok{true};
  const auto out = Run("k = wwiv.io.readkey()\nprint \"[\"; k; \"]\"\n", &ok);
  EXPECT_THAT(out, Not(HasSubstr("[]")));
}

TEST_F(WwivIoScreenTest, ScreenSize) {
  helper.user()->screen_width(100);
  helper.sess().num_screen_lines(30);
  EXPECT_EQ("100x30\r\n", Run("print wwiv.io.screen_columns(); \"x\"; wwiv.io.screen_rows()\n"));
}

TEST_F(WwivIoScreenTest, Colors) {
  EXPECT_EQ("[Hi there]/8/[Hi    ]\r\n", Run(R"(
s = "|#3Hi |15there"
print "["; wwiv.io.strip_colors(s); "]/"; wwiv.io.len_colors(s); "/[";
print wwiv.io.pad_colors("|#1Hi", 6); "]"
)"));
}

TEST_F(WwivIoScreenTest, PadColors_Limit) {
  bool ok{true};
  const auto out = Run("x = wwiv.io.pad_colors(\"\", 2000000000)\n", &ok);
  EXPECT_FALSE(ok);
  EXPECT_THAT(out, HasSubstr("PAD_COLORS: 2000000000 is more than 65535 characters"));
}

// PAUSING turns the user's pausing off for the script, and the user's
// setting comes back when the script ends.
TEST_F(WwivIoScreenTest, Pausing) {
  helper.user()->set_flag(User::pauseOnPage);
  EXPECT_EQ("TRUE/FALSE/FALSE\r\n", Run(R"(
a = wwiv.io.pausing(FALSE)
b = wwiv.io.pausing(FALSE)
print a; "/"; b; "/"; wwiv.io.pausing(TRUE)
)"));
  EXPECT_TRUE(helper.user()->pause());
  EXPECT_FALSE(helper.sess().disable_pause());

  // Left off at the end of the script: put back anyway.
  Run("x = wwiv.io.pausing(FALSE)\n");
  EXPECT_TRUE(helper.user()->pause());
  EXPECT_FALSE(helper.sess().disable_pause());
}

// PAUSING(TRUE) turns pausing on for a user who doesn't pause, and their
// setting comes back when the script ends.
TEST_F(WwivIoScreenTest, Pausing_TurnsOn) {
  helper.user()->clear_flag(User::pauseOnPage);
  helper.sess().disable_pause(true);
  EXPECT_EQ("FALSE/TRUE\r\n", Run(R"(
a = wwiv.io.pausing(TRUE)
print a; "/"; wwiv.io.pausing(TRUE)
)"));
  EXPECT_FALSE(helper.user()->pause());
  EXPECT_TRUE(helper.sess().disable_pause());
}

TEST_F(WwivIoScreenTest, Nsl) { EXPECT_EQ("TRUE\r\n", Run("print wwiv.io.nsl() >= 0\n")); }

// The example under "For full-screen UIs" in wwivbasic/docs/BINDINGS.md.
TEST_F(WwivIoScreenTest, DocExample) {
  SendKeys("aq");
  const auto out = Run(R"bas(
import "@wwiv.io"

If Not okansi() Then
  pl("This needs ANSI.")
  End
EndIf
x = pausing(FALSE)
cls()
gotoxy(1, 1)
prt(5, pad_colors("|#1Messages", screen_columns()))
gotoxy(1, screen_rows())
outstr("|#9Press Q to quit: ")
k = ""
While k <> "Q" And k <> "q"
  k = readkey()
  gotoxy(1, 3)
  clreol()
  outstr("You pressed: ", k)
Wend
)bas");
  EXPECT_THAT(out, HasSubstr("Messages"));
  EXPECT_THAT(out, HasSubstr("You pressed: a"));
  EXPECT_THAT(out, HasSubstr("You pressed: q"));
}

TEST_F(WwivIoScreenTest, DocExample_NoAnsi) {
  SetAnsi(false);
  const auto out = Run(R"bas(
import "@wwiv.io"

If Not okansi() Then
  pl("This needs ANSI.")
  End
EndIf
x = pausing(FALSE)
cls()
gotoxy(1, 1)
prt(5, pad_colors("|#1Messages", screen_columns()))
gotoxy(1, screen_rows())
outstr("|#9Press Q to quit: ")
k = ""
While k <> "Q" And k <> "q"
  k = readkey()
  gotoxy(1, 3)
  clreol()
  outstr("You pressed: ", k)
Wend
)bas");
  EXPECT_EQ("This needs ANSI.\r\n", out);
}
