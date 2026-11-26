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
#include "core/strings.h"
#include "sdk/ansi/ansi.h"
#include "sdk/ansi/framebuffer.h"
#include "sdk/user.h"
#include "wwivbasic/interpreter.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <vector>

using namespace wwiv::bbs::basic;
using namespace wwiv::sdk;
using namespace wwiv::sdk::ansi;
using testing::HasSubstr;
using testing::Not;
using testing::StartsWith;

namespace {

// The caller's terminal after some output: what's on each row, and in what
// colors.
struct Screen {
  // rows[0] is screen row 1, without trailing blanks.
  std::vector<std::string> rows;
  std::vector<std::vector<uint16_t>> cells;

  // Screen row y (from 1).
  [[nodiscard]] std::string row(int y) const {
    return y <= static_cast<int>(rows.size()) ? rows.at(static_cast<size_t>(y - 1)) : "";
  }
  // The attribute (background << 4 | foreground) at column x, row y (from 1).
  [[nodiscard]] int attr(int x, int y) const {
    return cells.at(static_cast<size_t>(y - 1)).at(static_cast<size_t>(x - 1)) >> 8;
  }
};

Screen ScreenOf(const std::string& output) {
  FrameBuffer fb(80);
  Ansi ansi(&fb, {}, 0x07);
  ansi.write(output);
  ansi.close();
  Screen s;
  for (int i = 0; i < fb.rows(); i++) {
    auto text = fb.row_as_text(i);
    std::replace(text.begin(), text.end(), '\0', ' ');
    s.rows.push_back(wwiv::strings::StringTrimEnd(text));
    s.cells.push_back(fb.row_char_and_attr(i));
  }
  return s;
}

int Count(const std::string& s, const std::string& what) {
  int n = 0;
  for (auto pos = s.find(what); pos != std::string::npos; pos = s.find(what, pos + 1)) {
    ++n;
  }
  return n;
}

} // namespace

// wwiv.screen: FULLSCREEN and LIGHTBAR (new engine only).
class WwivScreenTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    helper.config().scripting_enabled(true);
    helper.user()->set_flag(User::flag_ansi);
    helper.user()->screen_width(80);
    helper.sess().num_screen_lines(10);
    type_ahead::HangUpWhenKeysRunOut(helper.context().session_context(), a()->sess());
  }

  void TearDown() override {
    type_ahead::StopTypingAhead();
    remote()->keys.clear();
    bin.set_key_timeout(std::chrono::minutes(3));
    bin.set_sysop_key_timeout(std::chrono::minutes(10));
  }

  TestRemoteIO* remote() { return static_cast<TestRemoteIO*>(helper.io()->remote_io()); }

  // Keys sent by the caller over the connection (ESC [ B is Down).
  void SendKeys(const std::string& keys) {
    a()->sess().incom(true);
    a()->sess().ok_modem_stuff(true);
    remote()->keys = keys;
  }

  // Runs text; returns what it printed (without color and cursor codes), with
  // any errors. SNAPSHOT() in the script saves the screen as it is then (in
  // snapshots), and whether the user's pausing is on.
  std::string Run(const std::string& text) {
    helper.io()->Clear();
    raw.clear();
    NextEngine engine(bin, bout, helper.config(), &helper.context(),
                      [this](wwivbasic::Interpreter& interp) {
                        interp.globals().native_function("SNAPSHOT", [this]() {
                          raw += helper.io()->rcaptured();
                          snapshots.push_back(ScreenOf(raw));
                          pausing.push_back(helper.user()->pause());
                          return wwivbasic::Value();
                        });
                      });
    engine.RunScript("test", text);
    return helper.io()->captured();
  }

  // Everything sent to the caller, with ANSI codes. (Reading the test IO's
  // output empties it, so it's kept here.)
  std::string Raw() {
    raw += helper.io()->rcaptured();
    return raw;
  }

  BbsHelper helper{};
  std::vector<Screen> snapshots;
  std::vector<bool> pausing;
  std::string raw;
};

// FULLSCREEN

// A 10-row screen with 2 header lines: rows 1-2 are the header, 3 the top
// bar, 4-8 the content, 9 the bottom bar and 10 the command line.
TEST_F(WwivScreenTest, FullScreen_Layout) {
  const auto out = Run(R"(
s = wwiv.screen.fullscreen(2)
wwiv.io.gotoxy(1, 1)
wwiv.io.outstr("Header")
s.top_bar()
s.bottom_bar("Hi")
s.command_line("Cmd:")
snapshot()
size = STR(s.width) + " " + STR(s.height) + " " + STR(s.content_top) + " " + STR(s.content_height)
s.close()
print size
)");
  EXPECT_THAT(out, HasSubstr("80 10 4 5\r\n"));
  ASSERT_EQ(1u, snapshots.size());
  const auto& screen = snapshots.front();
  EXPECT_EQ("Header", screen.row(1));
  EXPECT_EQ("", screen.row(2));
  EXPECT_THAT(screen.row(3), StartsWith("\xc6\xcd\xcd"));
  EXPECT_EQ("", screen.row(4));
  EXPECT_THAT(screen.row(9), StartsWith("\xc6\xcd\xcd"));
  EXPECT_THAT(screen.row(9), HasSubstr(" Hi "));
  EXPECT_EQ("Cmd:", screen.row(10));
}

TEST_F(WwivScreenTest, FullScreen_ClearContent) {
  Run(R"(
s = wwiv.screen.fullscreen(1)
for y = s.content_top to s.content_top + s.content_height - 1
  wwiv.io.gotoxy(1, y)
  wwiv.io.outstr("x")
next
s.command_line("Cmd")
snapshot()
s.clear_content()
snapshot()
)");
  ASSERT_EQ(2u, snapshots.size());
  // Rows 3 to 7 are the content of a 10-row screen with 1 header line.
  EXPECT_EQ("x", snapshots[0].row(3));
  EXPECT_EQ("x", snapshots[0].row(7));
  EXPECT_EQ("", snapshots[1].row(3));
  EXPECT_EQ("", snapshots[1].row(7));
  EXPECT_EQ("Cmd", snapshots[1].row(10));
}

// Pausing is off while the screen is open, and comes back when it closes,
// with CLOSE or when the script ends.
TEST_F(WwivScreenTest, FullScreen_Pausing) {
  helper.user()->set_flag(User::pauseOnPage);
  Run("s = wwiv.screen.fullscreen(1)\nsnapshot()\ns.close()\nsnapshot()\n");
  EXPECT_EQ((std::vector<bool>{false, true}), pausing);

  Run("s = wwiv.screen.fullscreen(1)\n");
  EXPECT_TRUE(helper.user()->pause());
  EXPECT_FALSE(helper.sess().disable_pause());
}

TEST_F(WwivScreenTest, FullScreen_CloseClearsTheScreen) {
  Run(R"(
s = wwiv.screen.fullscreen(1)
s.top_bar()
s.close()
snapshot()
)");
  ASSERT_EQ(1u, snapshots.size());
  for (int y = 1; y <= 10; y++) {
    EXPECT_EQ("", snapshots.front().row(y)) << y;
  }
}

// When the script no longer holds the screen (here, the DEF that opened it
// returns), it closes: pausing comes back and another can open, but what's on
// the screen stays, except the command line.
TEST_F(WwivScreenTest, FullScreen_DroppedLeavesTheScreen) {
  helper.user()->set_flag(User::pauseOnPage);
  const auto out = Run(R"(
def Show()
  s = wwiv.screen.fullscreen(1)
  wwiv.io.gotoxy(1, 1)
  wwiv.io.outstr("Header")
  s.top_bar()
  s.command_line("Cmd")
  snapshot()
enddef
Show()
snapshot()
t = wwiv.screen.fullscreen(1)
print "reopened"
)");
  EXPECT_THAT(out, Not(HasSubstr("error")));
  EXPECT_THAT(out, HasSubstr("reopened"));
  EXPECT_EQ((std::vector<bool>{false, true}), pausing);
  ASSERT_EQ(2u, snapshots.size());
  const auto& after = snapshots[1];
  EXPECT_EQ("Header", after.row(1));
  EXPECT_THAT(after.row(2), StartsWith("\xc6\xcd\xcd"));
  EXPECT_EQ("", after.row(10));

  // And at the end of the script.
  Run("s = wwiv.screen.fullscreen(1)\nwwiv.io.outstr(\"Header\")\n");
  EXPECT_TRUE(helper.user()->pause());
  EXPECT_EQ("Header", ScreenOf(Raw()).row(1));
}

TEST_F(WwivScreenTest, FullScreen_OneAtATime) {
  auto out = Run("a = wwiv.screen.fullscreen(1)\nb = wwiv.screen.fullscreen(1)\n");
  EXPECT_THAT(out, HasSubstr("FULLSCREEN: a screen is already open; close it first"));

  // Once it's closed, another can open.
  out = Run(R"(
a = wwiv.screen.fullscreen(1)
a.close()
b = wwiv.screen.fullscreen(2)
b.close()
print "ok"
)");
  EXPECT_THAT(out, HasSubstr("ok\r\n"));
}

TEST_F(WwivScreenTest, FullScreen_Errors) {
  EXPECT_THAT(Run("s = wwiv.screen.fullscreen(7)\n"),
              HasSubstr("FULLSCREEN: 7 header lines leave no room for content on a 10-row "
                        "screen"));
  EXPECT_THAT(Run("s = wwiv.screen.fullscreen(-1)\n"), HasSubstr("FULLSCREEN: -1 header lines"));
  EXPECT_THAT(Run("s = wwiv.screen.fullscreen(1)\ns.close()\ns.top_bar()\n"),
              HasSubstr("screen.top_bar: the screen is closed"));

  helper.user()->clear_flag(User::flag_ansi);
  EXPECT_THAT(Run("s = wwiv.screen.fullscreen(1)\n"),
              HasSubstr("FULLSCREEN: needs ANSI; check wwiv.io.okansi() first"));
}

TEST_F(WwivScreenTest, FullScreen_ReadKey) {
  SendKeys("\x1b[Bq");
  const auto out = Run(R"(
s = wwiv.screen.fullscreen(1)
a = s.readkey()
b = s.readkey()
s.close()
print a; "/"; b
)");
  EXPECT_THAT(out, HasSubstr("DOWN/q\r\n"));
}

// LIGHTBAR

TEST_F(WwivScreenTest, Lightbar_Draw) {
  Run(R"(
l = wwiv.screen.lightbar(list("Alpha", "Bravo", "Charlie", "Delta"), 2, 3)
l.draw()
snapshot()
)");
  ASSERT_EQ(1u, snapshots.size());
  const auto& screen = snapshots.front();
  EXPECT_EQ("", screen.row(1));
  EXPECT_EQ("Alpha", screen.row(2));
  EXPECT_EQ("Bravo", screen.row(3));
  EXPECT_EQ("Charlie", screen.row(4));
  EXPECT_EQ("", screen.row(5));
  // The selected row is white on blue across the list's width (79 columns).
  EXPECT_EQ(0x1f, screen.attr(1, 2));
  EXPECT_EQ(0x1f, screen.attr(79, 2));
  EXPECT_NE(0x1f, screen.attr(1, 3));
}

TEST_F(WwivScreenTest, Lightbar_RunMovesAndReturnsTheFirstOtherKey) {
  // Down, Down, Up, q.
  SendKeys("\x1b[B\x1b[B\x1b[Aq");
  const auto out = Run(R"(
l = wwiv.screen.lightbar(list("Alpha", "Bravo", "Charlie"), 2, 3)
k = l.run()
snapshot()
print k; " "; l.selected
)");
  EXPECT_THAT(out, HasSubstr("q 1\r\n"));
  ASSERT_EQ(1u, snapshots.size());
  EXPECT_NE(0x1f, snapshots.front().attr(1, 2));
  EXPECT_EQ(0x1f, snapshots.front().attr(1, 3));
}

TEST_F(WwivScreenTest, Lightbar_Scrolls) {
  // Down x3, End, Home, Page Down, Page Up, each followed by x to report.
  SendKeys("\x1b[B\x1b[B\x1b[Bx\x1b[Fx\x1b[Hx\x1b[6~x\x1b[5~xq");
  const auto out = Run(R"(
l = wwiv.screen.lightbar(list("a0", "a1", "a2", "a3", "a4"), 1, 3)
s = ""
While l.run() = "x"
  s = s + STR(l.selected) + "/" + STR(l.top_item) + " "
  If l.selected = 3 And l.top_item = 1 Then
    snapshot()
  EndIf
Wend
print s
)");
  EXPECT_THAT(out, HasSubstr("3/1 4/2 0/0 3/2 0/0 \r\n"));
  ASSERT_EQ(1u, snapshots.size());
  EXPECT_EQ("a1", snapshots.front().row(1));
  EXPECT_EQ("a3", snapshots.front().row(3));
}

// RUN draws the whole list when it starts, since the screen may have been
// cleared since the last RUN.
TEST_F(WwivScreenTest, Lightbar_RunDrawsTheList) {
  SendKeys("xq");
  Run(R"(
l = wwiv.screen.lightbar(list("Alpha", "Bravo"), 1, 2)
k = l.run()
wwiv.io.cls()
k = l.run()
snapshot()
)");
  ASSERT_EQ(1u, snapshots.size());
  EXPECT_EQ("Alpha", snapshots.front().row(1));
  EXPECT_EQ("Bravo", snapshots.front().row(2));
}

// Moving within the window redraws only the rows that were and are selected.
TEST_F(WwivScreenTest, Lightbar_RedrawsOnlyWhatChanged) {
  SendKeys("\x1b[Bq");
  Run("l = wwiv.screen.lightbar(list(\"Alpha\", \"Bravo\", \"Charlie\"), 1, 3)\nk = l.run()\n");
  const auto output = Raw();
  EXPECT_EQ(2, Count(output, "Alpha"));
  EXPECT_EQ(2, Count(output, "Bravo"));
  EXPECT_EQ(1, Count(output, "Charlie"));
}

// The selected item is shown without its own colors, so the highlight shows;
// the others keep theirs. Items longer than the list is wide are cut.
TEST_F(WwivScreenTest, Lightbar_ItemColorsAndWidth) {
  Run(R"(
l = wwiv.screen.lightbar(list("|12Red", "|12Red", wwiv.io.pad_colors("", 100) + "x"), 1, 3)
l.draw()
snapshot()
)");
  ASSERT_EQ(1u, snapshots.size());
  const auto& screen = snapshots.front();
  EXPECT_EQ("Red", screen.row(1));
  EXPECT_EQ(0x1f, screen.attr(1, 1));
  EXPECT_EQ("Red", screen.row(2));
  EXPECT_EQ(0x0c, screen.attr(1, 2));
  EXPECT_EQ("", screen.row(3));
}

TEST_F(WwivScreenTest, Lightbar_Colors) {
  Run(R"(
l = wwiv.screen.lightbar(list("a", "b"), 1, 2)
l.color = "|14"
l.selected_color = "|20|15"
l.draw()
snapshot()
print l.color; " "; l.selected_color
)");
  ASSERT_EQ(1u, snapshots.size());
  EXPECT_EQ(0x4f, snapshots.front().attr(1, 1));
  EXPECT_EQ(0x0e, snapshots.front().attr(1, 2));
}

TEST_F(WwivScreenTest, Lightbar_SelectedAndSetItems) {
  const auto out = Run(R"(
l = wwiv.screen.lightbar(list("a", "b", "c", "d", "e"), 1, 2)
l.selected = 4
s = STR(l.selected) + "/" + STR(l.top_item)
l.set_items(list("x", "y"))
s = s + " " + STR(l.selected) + "/" + STR(l.top_item) + " " + STR(l.count)
l.set_items(list())
print s; " "; l.selected; " "; l.count
)");
  EXPECT_THAT(out, HasSubstr("4/3 1/0 2 -1 0\r\n"));
}

TEST_F(WwivScreenTest, Lightbar_Empty) {
  SendKeys("\x1b[Bq");
  const auto out = Run(R"(
l = wwiv.screen.lightbar(list(), 1, 3)
print l.run(); " "; l.selected
)");
  EXPECT_THAT(out, HasSubstr("q -1\r\n"));
}

// RUN("fn") calls fn(key, selected) for each key that isn't a movement key,
// until it returns FALSE.
TEST_F(WwivScreenTest, Lightbar_Callback) {
  // a, Down, b, q.
  SendKeys("a\x1b[Bbq");
  const auto out = Run(R"(
log = ""
def OnKey(key, selected)
  log = log + key + STR(selected) + ","
  return key <> "q"
enddef
l = wwiv.screen.lightbar(list("Alpha", "Bravo", "Charlie"), 1, 3)
k = l.run("OnKey")
print k; " "; log
)");
  EXPECT_THAT(out, HasSubstr("q a0,b1,q1,\r\n"));
}

// The callback can draw over the list; RUN draws it again afterwards.
TEST_F(WwivScreenTest, Lightbar_RedrawsAfterTheCallback) {
  SendKeys("aq");
  Run(R"(
def OnKey(key, selected)
  If key = "q" Then
    snapshot()
    return FALSE
  EndIf
  wwiv.io.gotoxy(1, 1)
  wwiv.io.outstr("Overwritten")
  return TRUE
enddef
l = wwiv.screen.lightbar(list("Alpha", "Bravo"), 1, 2)
k = l.run("OnKey")
)");
  ASSERT_EQ(1u, snapshots.size());
  EXPECT_EQ("Alpha", snapshots.front().row(1));
}

TEST_F(WwivScreenTest, Lightbar_EndInCallback) {
  SendKeys("ab");
  const auto out = Run(R"(
def OnKey(key, selected)
  print "["; key; "]"
  end
enddef
l = wwiv.screen.lightbar(list("Alpha"), 1, 1)
k = l.run("OnKey")
print "after"
)");
  EXPECT_THAT(out, HasSubstr("[a]"));
  EXPECT_THAT(out, Not(HasSubstr("[b]")));
  EXPECT_THAT(out, Not(HasSubstr("after")));
  EXPECT_THAT(out, Not(HasSubstr("error")));
}

// A misspelled callback is an error when RUN starts, not at the first key
// that isn't a movement key. (No keys are sent: reading one would hang up.)
TEST_F(WwivScreenTest, Lightbar_UnknownCallback) {
  const auto out = Run("l = wwiv.screen.lightbar(list(\"a\"), 1, 1)\nk = l.run(\"Nosuch\")\n");
  EXPECT_THAT(out, HasSubstr("lightbar.run: there is no function 'Nosuch'"));
  EXPECT_FALSE(a()->sess().hangup());
}

// While a FULLSCREEN is open, RUN reads keys through it, so an idle caller
// gets the screen's warning on the command line. With a one-minute timeout
// the warning is due at once.
TEST_F(WwivScreenTest, Lightbar_IdleWarningOnTheScreen) {
  bin.set_key_timeout(std::chrono::minutes(1));
  bin.set_sysop_key_timeout(std::chrono::minutes(1));
  SendKeys("q");
  auto out = Run(R"(
s = wwiv.screen.fullscreen(1)
l = wwiv.screen.lightbar(list("a"), s.content_top, 1)
k = l.run()
)");
  EXPECT_THAT(out, HasSubstr("Press space if you are still there."));

  // Without a screen there's no command line to show it on.
  SendKeys("q");
  out = Run("l = wwiv.screen.lightbar(list(\"a\"), 1, 1)\nk = l.run()\n");
  EXPECT_THAT(out, Not(HasSubstr("Press space")));
}

TEST_F(WwivScreenTest, Lightbar_RunFromItsOwnCallback) {
  SendKeys("a");
  const auto out = Run(R"(
def OnKey(key, selected)
  x = l.run()
  return FALSE
enddef
l = wwiv.screen.lightbar(list("Alpha"), 1, 1)
k = l.run("OnKey")
)");
  EXPECT_THAT(out, HasSubstr("lightbar.run: this lightbar is already running"));
}

// Once the caller hangs up, RUN returns "" and the script stops.
TEST_F(WwivScreenTest, Lightbar_HangUp) {
  const auto out = Run(R"(
l = wwiv.screen.lightbar(list("Alpha"), 1, 1)
k = l.run()
print "[" + k + "]"
)");
  EXPECT_THAT(out, Not(HasSubstr("[]")));
  EXPECT_TRUE(a()->sess().hangup());
}

TEST_F(WwivScreenTest, Lightbar_Errors) {
  EXPECT_THAT(Run("l = wwiv.screen.lightbar(list(\"a\"), 9, 3)\n"),
              HasSubstr("LIGHTBAR: rows 9 to 11 aren't all on the screen (1 to 10)"));
  EXPECT_THAT(Run("l = wwiv.screen.lightbar(list(\"a\"), 0, 1)\n"),
              HasSubstr("LIGHTBAR: rows 0 to 0 aren't all on the screen"));
  EXPECT_THAT(Run("l = wwiv.screen.lightbar(\"a\", 1, 1)\n"), HasSubstr("LIGHTBAR"));
  EXPECT_THAT(Run("l = wwiv.screen.lightbar(list(\"a\"), 1, 1)\nl.selected = 1\n"),
              HasSubstr("lightbar.selected: 1 isn't an item (there are 1)"));
  EXPECT_THAT(Run("l = wwiv.screen.lightbar(list(\"a\"), 1, 1)\nk = l.run(\"a\", \"b\")\n"),
              HasSubstr("lightbar.run: expected 0 or 1 arguments, got 2"));

  helper.user()->clear_flag(User::flag_ansi);
  EXPECT_THAT(Run("l = wwiv.screen.lightbar(list(\"a\"), 1, 1)\n"),
              HasSubstr("LIGHTBAR: needs ANSI; check wwiv.io.okansi() first"));
}

// The example under "wwiv.screen" in wwivbasic/docs/BINDINGS.md.
static const char kDocExample[] = R"bas(
import "@wwiv.io"
import "@wwiv.screen"

def OnKey(key, selected)
  If key = CHR(13) Then
    s.command_line("|#9You chose |#2" + get(items, selected))
    return TRUE
  EndIf
  return key <> "Q" And key <> "q"
enddef

If Not okansi() Then
  pl("This needs ANSI.")
  End
EndIf
items = list("Apples", "Bananas", "Cherries", "Dates", "Elderberries")
s = fullscreen(1)
gotoxy(1, 1)
outstr("|#5Fruit")
s.top_bar()
s.bottom_bar("")
s.command_line("|#9Enter chooses, Q quits")
l = lightbar(items, s.content_top, s.content_height)
k = l.run("OnKey")
s.close()
)bas";

TEST_F(WwivScreenTest, DocExample) {
  // Down, Enter, q.
  SendKeys("\x1b[B\rq");
  const auto out = Run(kDocExample);
  EXPECT_THAT(out, Not(HasSubstr("error")));
  EXPECT_THAT(out, HasSubstr("You chose Bananas"));
  // Q ended it, not the keys running out.
  EXPECT_FALSE(a()->sess().hangup());
}

TEST_F(WwivScreenTest, DocExample_NoAnsi) {
  helper.user()->clear_flag(User::flag_ansi);
  EXPECT_EQ("This needs ANSI.\r\n", Run(kDocExample));
}
