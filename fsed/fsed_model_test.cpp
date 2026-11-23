/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2020-2022, WWIV Software Services               */
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
#include "bbs/bbs_helper.h"
#include "core/stl.h"
#include "fsed/model.h"
#include "fsed/view_rows.h"
#include "fsed/newline.h"

#include "gtest/gtest.h"
#include <algorithm>
#include <memory>
#include <string>

using namespace wwiv::stl;

class FakeView : public wwiv::fsed::editor_viewport_t {
public:
  // Inherited via editor_viewport_t
  [[nodiscard]] int max_view_lines() const override { return 10; }
  [[nodiscard]] int max_view_columns() const override { return 20; }
  [[nodiscard]] int top_line() const override { return top_line_; }
  void set_top_line(int l) override { top_line_ = l; }
  void gotoxy(const wwiv::fsed::FsedModel& ed) override { 
    x_ = ed.cx;
    y_ = ed.cy;
  }

private:
  int top_line_{0};
  int x_{0};
  int y_{0};
};

class FsedModelWithViewTest : public ::testing::Test {
protected:
  FsedModelWithViewTest() { 
    view = std::make_shared<FakeView>();
    ed.set_view(view);
  }

  void add(const std::string& s) {
    for (const auto& c : s) {
      if (c == '\n') {
        ed.enter();
        continue;
      } 
      ed.add(c);
    }
  }

  void add_test_lines(int n) {
    for (int i = 0; i < n; i++) {
      add(fmt::format("Test-{}\n", i));
    }
  }

  wwiv::fsed::FsedModel ed{255};
  std::shared_ptr<FakeView> view;
};

TEST_F(FsedModelWithViewTest, AddSingle_Character) {
  ed.add('H');
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(1, ed.cx);
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(1, wwiv::stl::ssize(ed));
}

TEST_F(FsedModelWithViewTest, AddSingleline) {
  const std::string line = "Hello World";
  add(line);
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(ssize(line), ed.cx);
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(1, wwiv::stl::ssize(ed));
}

TEST_F(FsedModelWithViewTest, AddTwolines) {
  const std::string line = "Hello\nWorld";
  add(line);
  EXPECT_EQ(ssize(std::string("World")), ed.cx);
  EXPECT_EQ(1, ed.curli);
  EXPECT_EQ(2, wwiv::stl::ssize(ed));
  EXPECT_EQ(1, ed.cy);
}

TEST_F(FsedModelWithViewTest, SixLines) {
  const std::string line = "Hello\nWorld\nThis\nIs\nA\nTest\n";
  add(line);
  EXPECT_EQ(6, ed.curli);
  EXPECT_EQ(7, wwiv::stl::ssize(ed));
  EXPECT_EQ(6, ed.cy);
}

TEST_F(FsedModelWithViewTest, PageUp_NoScroll) {
  const std::string line = "Hello\nWorld\nThis\nIs\nA\nTest\n";
  add(line);
  ed.cursor_pgup();
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(7, wwiv::stl::ssize(ed));
  EXPECT_EQ(0, ed.cy);
}

TEST_F(FsedModelWithViewTest, PageUp_Scroll) {
  add_test_lines(100);
  EXPECT_EQ(100, ed.curli);
  EXPECT_EQ(10, ed.cy);

  // Scroll up
  ed.cursor_pgup();
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(90, ed.curli);
}

TEST_F(FsedModelWithViewTest, BackSpace_Color) {
  ed.curline().set_wwiv_color(2);
  add("a");
  // Still 2
  EXPECT_EQ(2, ed.curline().wwiv_color());
  ed.bs();
  // Still 2 after a backspace
  EXPECT_EQ(2, ed.curline().wwiv_color());
  ed.bs();
  // Now 0 since we went to the end.
  EXPECT_EQ(0, ed.curline().wwiv_color());
}


// Visible characters of a line, ignoring heart-color codes.
static std::string line_text(const wwiv::fsed::line_t& ln) {
  std::string out;
  for (const auto& c : ln.cells()) {
    out.push_back(c.ch);
  }
  return out;
}

// Join the visible text of every editor line (no WWIV wrap markers).
static std::string joined_text(const wwiv::fsed::FsedModel& ed) {
  std::string out;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    out.append(line_text(ed.line(i)));
  }
  return out;
}

// #1343: wrapping a long paragraph must insert a new line rather than
// writing into the text that already sits below the cursor.
TEST_F(FsedModelWithViewTest, WordWrap_DoesNotOverwriteLinesBelow) {
  ed.set_max_line_len(20);
  bool saw_invalidate_to_eof = false;
  ed.add_callback([&](wwiv::fsed::FsedModel& e, wwiv::fsed::editor_range_t r) {
    // Capture whether any redraw covered the lines that were shifted down.
    if (r.start.line == 0 && r.end.line == wwiv::stl::size_int(e) - 1) {
      saw_invalidate_to_eof = true;
    }
  });

  // Line 0 is blank; line 1 is quoted text the wrap must not clobber.
  add("\nQUOTED TEXT");
  ed.cursor_up();
  EXPECT_EQ(0, ed.curli);

  add("12345 12345 12345 XXXYY");

  // The quoted line must still be present and unchanged, after the paragraph.
  bool found = false;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    if (line_text(ed.line(i)) == "QUOTED TEXT") {
      found = true;
      EXPECT_GT(i, 0);
    }
  }
  EXPECT_TRUE(found);
  EXPECT_GE(wwiv::stl::size_int(ed), 3);

  // #1343 display: the wrap must invalidate through EOF so the view
  // redraws the lines that were shifted down, not just the new wrap line.
  EXPECT_TRUE(saw_invalidate_to_eof);
}

// #1343: a long word with no spaces takes the character-wrap path, which
// used to do "++curli" without insert_line() and overwrote the next line.
TEST_F(FsedModelWithViewTest, CharacterWrap_DoesNotOverwriteLinesBelow) {
  ed.set_max_line_len(20);
  add("\nQUOTED TEXT");
  ed.cursor_up();
  add("abcdefghijklmnopqrstuvwxyz");

  bool found = false;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    if (line_text(ed.line(i)) == "QUOTED TEXT") {
      found = true;
      EXPECT_GT(i, 0);
    }
  }
  EXPECT_TRUE(found);
  // 26 letters at width 20 should be two lines plus the quoted line.
  EXPECT_GE(wwiv::stl::size_int(ed), 3);
  EXPECT_EQ("abcdefghijklmnopqrst", line_text(ed.line(0)));
  EXPECT_EQ("uvwxyz", line_text(ed.line(1)));
  EXPECT_TRUE(ed.line(0).wrapped());
}

// #1344: inserting in the middle of a full, hard-broken line must wrap
// onto a new line instead of growing past the screen width (and must
// not overwrite the next unwrapped paragraph).
TEST_F(FsedModelWithViewTest, MidLineInsert_WrapsWithoutClobberingNextParagraph) {
  ed.set_max_line_len(20);
  // 19 characters so typing them does not itself trigger a wrap.
  add("12345 12345 12345 1");
  EXPECT_EQ(19, wwiv::stl::size_int(ed.curline()));
  ed.enter();
  add("NEXT PARAGRAPH");
  ed.cursor_up();
  ed.cursor_home();
  // Insert at the start of the full line.
  add("ZZ ");

  bool found = false;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    if (line_text(ed.line(i)) == "NEXT PARAGRAPH") {
      found = true;
    }
  }
  EXPECT_TRUE(found);
  EXPECT_LE(wwiv::stl::size_int(ed.line(0)), 20);
  EXPECT_TRUE(ed.line(0).wrapped());
  // The characters we shifted off the first line must still exist.
  const auto all = joined_text(ed);
  EXPECT_NE(std::string::npos, all.find("ZZ 12345 12345 12345 1"));
  EXPECT_NE(std::string::npos, all.find("NEXT PARAGRAPH"));
}

// #1344: inserting into an already soft-wrapped paragraph must keep every
// character (with spaces intact) and must not exceed the max width.  Overflow
// reflows into the continuation lines of the same paragraph.
TEST_F(FsedModelWithViewTest, MidLineInsert_ReflowsSoftWrappedParagraph) {
  ed.set_max_line_len(20);
  add("12345 12345 12345 XXXYY");
  ASSERT_GE(wwiv::stl::size_int(ed), 2);
  ASSERT_TRUE(ed.line(0).wrapped());
  const auto before = joined_text(ed);

  // Go back to the start of the paragraph and insert a short prefix.
  while (ed.curli > 0) {
    ed.cursor_up();
  }
  ed.cursor_home();
  add("ZZ ");

  const auto after = joined_text(ed);
  EXPECT_NE(std::string::npos, after.find("ZZ "));
  // Every original character should still be present, in order, after the prefix.
  EXPECT_EQ(std::string("ZZ ") + before, after);
  // No line in the paragraph should exceed the max width.
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    EXPECT_LE(wwiv::stl::size_int(ed.line(i)), 20) << "line " << i;
  }
}


// Backspace at column 0 of a line that does not fit entirely on the line
// above must pull back through the last space in the remaining width, not
// a single character (the old "space = expr > 0" precedence bug).
TEST_F(FsedModelWithViewTest, BackspaceJoin_PullsWholeWord) {
  ed.set_max_line_len(20);
  add("AAAA \nBBB CCC DDD EEE");
  ed.cursor_home();
  EXPECT_EQ(1, ed.curli);
  EXPECT_EQ(0, ed.cx);

  ed.bs();

  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(5, ed.cx);
  EXPECT_EQ("AAAA BBB CCC DDD", line_text(ed.line(0)));
  EXPECT_EQ(" EEE", line_text(ed.line(1)));
}

// When the next line fits on the previous one, backspace at column 0
// joins them into a single line.
TEST_F(FsedModelWithViewTest, BackspaceJoin_FitsEntirely) {
  ed.set_max_line_len(20);
  add("AAA\nBBB");
  ed.cursor_home();
  ed.bs();

  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(4, ed.cx);
  EXPECT_EQ(1, wwiv::stl::size_int(ed));
  EXPECT_EQ("AAA BBB", line_text(ed.line(0)));
}

// Backspace on the top visible row must scroll the viewport instead of
// driving cy negative.
TEST_F(FsedModelWithViewTest, Backspace_AtTopOfViewport_DoesNotGoNegative) {
  add_test_lines(100);
  ed.cursor_pgup();
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(90, ed.curli);
  ed.cursor_home();

  ed.bs();

  EXPECT_GE(ed.cy, 0);
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(89, ed.curli);
  EXPECT_EQ(89, view->top_line());
}

TEST_F(FsedModelWithViewTest, CursorLeftRight_WrapAcrossLines) {
  add("Hello\nWorld");
  EXPECT_EQ(1, ed.curli);
  ed.cursor_home();
  EXPECT_EQ(0, ed.cx);

  ed.cursor_left();
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(5, ed.cx);
  EXPECT_EQ(0, ed.cy);

  ed.cursor_right();
  EXPECT_EQ(1, ed.curli);
  EXPECT_EQ(0, ed.cx);
  EXPECT_EQ(1, ed.cy);

  // Left at the start of the document is a no-op.
  ed.cursor_up();
  ed.cursor_home();
  ed.cursor_left();
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(0, ed.cx);

  // Right at the end of the last line is a no-op.
  ed.cursor_down();
  ed.cursor_end();
  const auto end_cx = ed.cx;
  ed.cursor_right();
  EXPECT_EQ(1, ed.curli);
  EXPECT_EQ(end_cx, ed.cx);
}

TEST_F(FsedModelWithViewTest, CursorLeft_AtTopOfViewport_Scrolls) {
  add_test_lines(100);
  ed.cursor_pgup();
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(90, ed.curli);
  ed.cursor_home();

  ed.cursor_left();

  EXPECT_GE(ed.cy, 0);
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(89, ed.curli);
  EXPECT_EQ(89, view->top_line());
  EXPECT_EQ(wwiv::stl::size_int(ed.line(89)), ed.cx);
}


// Reproduce the test-video case: a full-width quote line, a hard break, then
// a soft-wrapped paragraph.  Inserting at column 0 of the quote must wrap at
// a word boundary (not "Maryla"/"nd.") and must not glue overflow onto the
// next paragraph ("nd.Trying", "hisextend", "umpsover").
TEST_F(FsedModelWithViewTest, InsertAtStartOfFullQuote_WrapsAtWordBoundary) {
  const std::string quote =
      "U> I find it funny how many people on here are in or are from Maryland.";
  const std::string para =
      "Trying this again with some FSED changes I'm working on. I need to make this "
      "extend past a couple lines, so sorry about the noise. The quick brown fox jumps "
      "over the lazy dog.";
  ed.set_max_line_len(static_cast<int>(quote.size()));
  add(quote);
  ed.enter();
  add(para);

  while (ed.curli > 0) {
    ed.cursor_up();
  }
  ed.cursor_home();
  add("line 3");

  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    const auto t = line_text(ed.line(i));
    EXPECT_LE(wwiv::stl::size_int(ed.line(i)), static_cast<int>(quote.size())) << t;
    if (t.size() >= 6) {
      EXPECT_NE("Maryla", t.substr(t.size() - 6)) << t;
    }
    EXPECT_NE("nd.", t);
  }
  const auto all = joined_text(ed);
  EXPECT_NE(std::string::npos, all.find("Maryland."));
  EXPECT_NE(std::string::npos, all.find("line 3"));
  EXPECT_NE(std::string::npos, all.find("Trying this again"));
  EXPECT_EQ(std::string::npos, all.find("hisextend"));
  EXPECT_EQ(std::string::npos, all.find("umpsover"));
  // joined_text concatenates hard-broken lines with no separator, so
  // "Maryland." + "Trying" looks mashed in 'all'.  Check the actual lines.
  bool found_trying = false;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    const auto t = line_text(ed.line(i));
    EXPECT_EQ(std::string::npos, t.find("nd.Trying")) << t;
    EXPECT_EQ(std::string::npos, t.find("Maryland.Trying")) << t;
    if (t.find("Trying this again") != std::string::npos) {
      found_trying = true;
      EXPECT_EQ(std::string::npos, t.find("Maryland.")) << t;
    }
  }
  EXPECT_TRUE(found_trying);
}

TEST_F(FsedModelWithViewTest, EnterMidLine_DoesNotConcatenateOntoNextParagraph) {
  const std::string quote =
      "U> I find it funny how many people on here are in or are from Maryland.";
  ed.set_max_line_len(static_cast<int>(quote.size()));
  add(quote);
  ed.enter();
  add("Trying this again");
  while (ed.curli > 0) {
    ed.cursor_up();
  }
  ed.cursor_end();
  for (int i = 0; i < 9; i++) {
    ed.cursor_left(); // just before "Maryland."
  }
  ed.enter();

  const auto all = joined_text(ed);
  EXPECT_NE(std::string::npos, all.find("Trying this again"));
  bool found_trying = false;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    const auto t = line_text(ed.line(i));
    EXPECT_EQ(std::string::npos, t.find("nd.Trying")) << t;
    EXPECT_EQ(std::string::npos, t.find("Maryland.Trying")) << t;
    if (t.find("Trying this again") != std::string::npos) {
      found_trying = true;
      EXPECT_EQ(std::string::npos, t.find("Maryland.")) << t;
    }
  }
  EXPECT_TRUE(found_trying);
  bool found_para = false;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    EXPECT_LE(wwiv::stl::size_int(ed.line(i)), static_cast<int>(quote.size()));
    if (line_text(ed.line(i)).find("Trying this again") == 0) {
      found_para = true;
    }
  }
  EXPECT_TRUE(found_para);
}

TEST_F(FsedModelWithViewTest, BackspaceJoin_PreservesSpaceAfterMaryland) {
  ed.set_max_line_len(wwiv::fsed::editor_wrap_columns(80));
  add("are from Maryland.\nTrying this");
  ed.cursor_home();
  ed.bs();

  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ("are from Maryland. Trying this", line_text(ed.line(0)));
  EXPECT_EQ(std::string::npos, joined_text(ed).find("Maryland.Trying"));
}

// After a wrap, deleting one character must not swallow a neighbor
// (the test video lost the "g" from "Trying" / "gthis").
TEST_F(FsedModelWithViewTest, DeleteAfterWrap_DoesNotLoseCharacters) {
  ed.set_max_line_len(20);
  add("12345 12345 12345 XXXYY");
  ASSERT_GE(wwiv::stl::size_int(ed), 2);
  ASSERT_TRUE(ed.line(0).wrapped());
  const auto before = joined_text(ed);

  while (ed.curli > 1) {
    ed.cursor_up();
  }
  if (ed.curli == 0) {
    ed.cursor_down();
  }
  ed.cursor_home();
  ASSERT_GT(wwiv::stl::size_int(ed.curline()), 0);
  const auto continuation = line_text(ed.curline());
  const char removed = continuation.front();
  const auto rest = continuation.substr(1);
  ed.del();

  const auto after = joined_text(ed);
  EXPECT_EQ(before.size() - 1, after.size());
  const auto at = before.rfind(std::string(1, removed) + rest);
  ASSERT_NE(std::string::npos, at);
  EXPECT_EQ(before.substr(0, at) + before.substr(at + 1), after);
}


// The old wrap heuristic required the last space to be in the second half
// of the line, otherwise it character-wrapped.  A space in the first half
// plus a long last word must still wrap at that space, not mid-word.
TEST_F(FsedModelWithViewTest, WrapsAtLastSpaceEvenIfInFirstHalf) {
  ed.set_max_line_len(20);
  add("1234567 123456789012345");
  EXPECT_EQ("1234567 ", line_text(ed.line(0)));
  EXPECT_TRUE(ed.line(0).wrapped());
  EXPECT_EQ("1234567 123456789012345", joined_text(ed));
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    EXPECT_LE(wwiv::stl::size_int(ed.line(i)), 20) << line_text(ed.line(i));
  }
}


// Frank's video A: typing at the end of a soft-wrapped paragraph must flow
// into the next continuation, not leave a dangling overflow line.  The BBS
// default max_line_len_ is 79; runtime wraps at screen width minus 1
// (79 on an 80-column terminal) so a full-width write cannot autowrap
// onto the status bar.
// "Alaska... I" is 74 characters, so " am g" reaches 79 and must wrap.
TEST_F(FsedModelWithViewTest, ParagraphReflow_OverflowJoinsContinuation) {
  const std::string line0 =
      "Alaska and Spain were my last two commands while I was still in uniform. I";
  const std::string line1 =
      "retired from the Navy in Laurel, but I continued doing the same job for the";
  const std::string line2 = "next thirty years.";
  ASSERT_EQ(74, static_cast<int>(line0.size()));
  ASSERT_EQ(75, static_cast<int>(line1.size()));

  ed.set_max_line_len(79);
  std::vector<wwiv::fsed::line_t> lines;
  lines.emplace_back(true, line0);
  lines.emplace_back(true, line1);
  lines.emplace_back(false, line2);
  ed.set_lines(std::move(lines));
  ed.curli = 0;
  ed.cy = 0;
  ed.cx = wwiv::stl::size_int(ed.line(0));
  add(" am g");

  const auto all = joined_text(ed);
  EXPECT_EQ(
      "Alaska and Spain were my last two commands while I was still in uniform. I am g retired from the Navy in Laurel, but I continued doing the same job for the next thirty years.",
      all);
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    EXPECT_LE(wwiv::stl::size_int(ed.line(i)), 79) << line_text(ed.line(i));
  }
  // Overflow must sit on the continuation, not a stranded line above "retired".
  bool retired_with_overflow = false;
  int retired_line = -1;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    const auto t = line_text(ed.line(i));
    if (t.find("retired") != std::string::npos) {
      retired_line = i;
      if (t.find("g retired") != std::string::npos || t.find("am g") != std::string::npos) {
        retired_with_overflow = true;
      }
    }
  }
  EXPECT_TRUE(retired_with_overflow) << all;
  EXPECT_GT(retired_line, 0);
  EXPECT_TRUE(ed.line(0).wrapped());
  EXPECT_GE(wwiv::stl::size_int(ed.line(0)), 70);
  // A new hard-broken overflow line would bump this to 4.
  EXPECT_LE(wwiv::stl::size_int(ed), 3);
}

// Deleting in a soft-wrapped paragraph must pull words back up.
TEST_F(FsedModelWithViewTest, ParagraphReflow_DeletePullsWordsUp) {
  ed.set_max_line_len(20);
  add("12345 12345 12345 12345 12345");
  ASSERT_GE(wwiv::stl::size_int(ed), 2);
  ASSERT_TRUE(ed.line(0).wrapped());
  const int lines_before = wwiv::stl::size_int(ed);

  while (ed.curli > 0) {
    ed.cursor_up();
  }
  ed.cursor_home();
  // Remove the first word and its trailing space so the next word can move up.
  for (int i = 0; i < 6; i++) {
    ed.del();
  }

  const auto all = joined_text(ed);
  EXPECT_EQ("12345 12345 12345 12345", all);
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    EXPECT_LE(wwiv::stl::size_int(ed.line(i)), 20) << line_text(ed.line(i));
  }
  EXPECT_LE(wwiv::stl::size_int(ed), lines_before);
  EXPECT_EQ("12345 12345 12345 ", line_text(ed.line(0)));
}

// Video B: inserting at column 0 of a full-width line must wrap rather than
// leave text past max_line_len.  Runtime wrap is screen width minus 1 (79).
TEST_F(FsedModelWithViewTest, MidLineInsert_OnFullWidthLine_WrapsAtWidthMinusOne) {
  const std::string retired =
      "retired from the Navy in Laurel, but I continued doing the same job for the";
  const std::string full = retired + " 123"; // 79 characters
  ASSERT_EQ(79, static_cast<int>(full.size()));

  ed.set_max_line_len(wwiv::fsed::editor_wrap_columns(80));
  std::vector<wwiv::fsed::line_t> lines;
  lines.emplace_back(false, full);
  ed.set_lines(std::move(lines));
  ed.curli = 0;
  ed.cy = 0;
  ed.cx = 0;
  add("I ");

  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    EXPECT_LE(wwiv::stl::size_int(ed.line(i)), 79) << line_text(ed.line(i));
  }
  EXPECT_TRUE(ed.line(0).wrapped());
  EXPECT_GE(wwiv::stl::size_int(ed), 2);
  const auto all = joined_text(ed);
  EXPECT_NE(std::string::npos, all.find("I "));
  EXPECT_NE(std::string::npos, all.find(retired));
  EXPECT_NE(std::string::npos, all.find(" 123"));
  EXPECT_EQ("I " + full, all);
}

// Video C: jumping to the end of a long message must keep cy inside the
// viewport (not on the status-bar row).
TEST_F(FsedModelWithViewTest, CursorEof_StaysInsideViewport) {
  add_test_lines(100);
  // Each line is added with a trailing enter(), so the last line is empty
  // and curli is size-1 (100), not 99.
  ed.cursor_eof();

  EXPECT_EQ(wwiv::stl::size_int(ed) - 1, ed.curli);
  EXPECT_GE(ed.cy, 0);
  EXPECT_LE(ed.cy, view->max_view_lines());
  EXPECT_EQ(ed.curli, view->top_line() + ed.cy);
  EXPECT_EQ(wwiv::stl::size_int(ed.curline()), ed.cx);
}

TEST_F(FsedModelWithViewTest, EnterAtBottom_StaysInsideViewport) {
  add_test_lines(15);
  ed.cursor_eof();
  EXPECT_LE(ed.cy, view->max_view_lines());
  ed.enter();
  EXPECT_GE(ed.cy, 0);
  EXPECT_LE(ed.cy, view->max_view_lines());
  EXPECT_EQ(ed.curli, view->top_line() + ed.cy);
}

// Soft-wrap marker must stay a flag, not a visible cell, when saving/loading.
TEST_F(FsedModelWithViewTest, ToLines_SoftWrapMarkerIsNotText) {
  ed.set_max_line_len(20);
  add("12345 12345 12345 XXXYY");
  ASSERT_TRUE(ed.line(0).wrapped());
  const auto saved = ed.to_lines(true);
  ASSERT_GE(saved.size(), 2u);
  EXPECT_EQ('\x1', saved[0].back());
  EXPECT_EQ(std::string::npos, saved[0].substr(0, saved[0].size() - 1).find('\x1'));

  wwiv::fsed::line_t reloaded(true, saved[0]);
  EXPECT_TRUE(reloaded.wrapped());
  const auto t = line_text(reloaded);
  EXPECT_EQ(std::string::npos, t.find('\x1'));
  EXPECT_EQ(saved[0].substr(0, saved[0].size() - 1), t);
}


static void expect_viewport_consistent(const wwiv::fsed::FsedModel& ed, const FakeView& view,
                                       const char* where) {
  EXPECT_GE(ed.cy, 0) << where;
  EXPECT_LE(ed.cy, view.max_view_lines()) << where;
  EXPECT_GE(view.top_line(), 0) << where;
  EXPECT_EQ(ed.curli, view.top_line() + ed.cy) << where;
}

// Enter on the last visible row must scroll top_line, not grow cy past the
// viewport (the paste path: each CR is enter()).
TEST_F(FsedModelWithViewTest, EnterAtBottom_ScrollsTopLine) {
  const int vis = view->max_view_lines(); // max cy; visible rows = vis+1
  for (int i = 0; i < vis; i++) {
    ed.enter();
    expect_viewport_consistent(ed, *view, "filling");
  }
  EXPECT_EQ(0, view->top_line());
  EXPECT_EQ(vis, ed.cy);
  EXPECT_EQ(vis, ed.curli);

  ed.enter();
  expect_viewport_consistent(ed, *view, "first scroll");
  EXPECT_EQ(1, view->top_line());
  EXPECT_EQ(vis, ed.cy);
  EXPECT_EQ(vis + 1, ed.curli);

  for (int i = 0; i < 5; i++) {
    ed.enter();
    expect_viewport_consistent(ed, *view, "more enters");
  }
  EXPECT_EQ(6, view->top_line());
}

// Simulate a web-terminal paste: a long burst of characters and CRs, including
// blank lines and lines longer than the viewport width.
TEST_F(FsedModelWithViewTest, PasteBurst_LongDocument_ViewportConsistent) {
  ed.set_max_line_len(wwiv::fsed::editor_wrap_columns(view->max_view_columns()));
  std::string burst;
  burst += "# Title\n\n";
  for (int i = 0; i < 120; i++) {
    if (i % 7 == 0) {
      burst += "\n";
      continue;
    }
    if (i % 5 == 0) {
      burst += "This is a long pasted line that will wrap past the viewport width for sure!\n";
      continue;
    }
    burst += fmt::format("Line number {} of the pasted document.\n", i);
  }
  add(burst);
  expect_viewport_consistent(ed, *view, "after paste");
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    EXPECT_LE(wwiv::stl::size_int(ed.line(i)), ed.max_line_len()) << line_text(ed.line(i));
  }
  EXPECT_GT(view->top_line(), 0);
  EXPECT_GT(ed.curli, view->max_view_lines());

  // Scroll to the top, then back to the bottom.
  while (ed.curli > 0) {
    ed.cursor_up();
    expect_viewport_consistent(ed, *view, "scrolling up");
  }
  EXPECT_EQ(0, view->top_line());
  EXPECT_EQ(0, ed.cy);

  while (ed.curli < wwiv::stl::size_int(ed) - 1) {
    ed.cursor_down();
    expect_viewport_consistent(ed, *view, "scrolling down");
  }
  EXPECT_EQ(ed.curli, view->top_line() + ed.cy);
  EXPECT_LE(ed.cy, view->max_view_lines());
}

TEST_F(FsedModelWithViewTest, ScrollUpDown_ShortThenLong) {
  add_test_lines(40);
  ed.cursor_eof();
  expect_viewport_consistent(ed, *view, "eof");
  const int bottom_top = view->top_line();
  EXPECT_GT(bottom_top, 0);

  for (int i = 0; i < 15; i++) {
    ed.cursor_up();
    expect_viewport_consistent(ed, *view, "up");
  }
  for (int i = 0; i < 30; i++) {
    ed.cursor_down();
    expect_viewport_consistent(ed, *view, "down");
  }
  expect_viewport_consistent(ed, *view, "done");
}


// Apply a paste the same way the FSED key loop does: CR and a lone LF
// start a new line; LF after CR is ignored (Windows CRLF).
static void paste_keys(wwiv::fsed::FsedModel& ed, const std::string& s) {
  bool last_was_cr = false;
  for (const unsigned char ch : s) {
    if (wwiv::fsed::consume_line_break(static_cast<int>(ch), last_was_cr)) {
      ed.enter();
      continue;
    }
    if (ch >= 32) {
      ed.add(static_cast<char>(ch));
    }
  }
}

static int max_line_cells(const wwiv::fsed::FsedModel& ed) {
  int m = 0;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    m = std::max(m, wwiv::stl::size_int(ed.line(i)));
  }
  return m;
}

TEST_F(FsedModelWithViewTest, Paste_CrLf_LongListItemsWrapAndDoNotDuplicate) {
  ed.set_max_line_len(wwiv::fsed::editor_wrap_columns(80));
  const std::string burst =
      "Ordinary paragraph that has plenty of spaces and wraps cleanly onto the next editor row.\r\n"
      "\r\n"
      "1. **Build + tokenizer**  CMake/FindHunspell, `SpellDictionary` + stub, `spell_text` and tests. No UI, no config schema.\r\n"
      "2. **Config + FSED UI** \xE2\x80\x94 `spell_config_t`, menuset field, sidecar + word lists, defaults **P**.\r\n"
      "A digit applies that suggestion; then reflow. Ignore (I) skips this occurrence; 'G' ignores that word for the rest of the pass. 'A' adds the word to the user's personal dictionary.\r\n"
      "5. **Scope:** FSED only for v1, or should the line editor get a later hook?\r\n";
  paste_keys(ed, burst);

  EXPECT_LE(max_line_cells(ed), ed.max_line_len());
  bool found_item1 = false;
  bool found_scope = false;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    const auto t = line_text(ed.line(i));
    if (t.find("1. **Build + tokenizer**") == 0) {
      found_item1 = true;
      EXPECT_LE(wwiv::stl::size_int(ed.line(i)), ed.max_line_len()) << t;
      // The rest of the list item must continue on the next soft-wrapped line,
      // not be cut off in the model.
      EXPECT_TRUE(ed.line(i).wrapped());
      ASSERT_LT(i + 1, wwiv::stl::size_int(ed));
      EXPECT_NE(std::string::npos, line_text(ed.line(i + 1)).find("No UI, no config schema."));
    }
    if (t.find("5. **Scope:**") == 0) {
      found_scope = true;
    }
    if (i > 0 && !t.empty()) {
      EXPECT_NE(t, line_text(ed.line(i - 1))) << "consecutive duplicate line at " << i;
    }
  }
  EXPECT_TRUE(found_item1);
  EXPECT_TRUE(found_scope);
  expect_viewport_consistent(ed, *view, "after crlf paste");
}

TEST_F(FsedModelWithViewTest, Paste_UnixLf_KeepsHardBreaksAndWraps) {
  ed.set_max_line_len(wwiv::fsed::editor_wrap_columns(80));
  const std::string burst =
      "1. **Build + tokenizer**  CMake/FindHunspell, `SpellDictionary` + stub, `spell_text` and tests.\n"
      "5. **Scope:** FSED only for v1, or should the line editor get a later hook?\n";
  paste_keys(ed, burst);

  EXPECT_LE(max_line_cells(ed), ed.max_line_len());
  bool found_item1 = false;
  bool found_scope = false;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    const auto t = line_text(ed.line(i));
    if (t.find("1. **Build + tokenizer**") == 0) {
      found_item1 = true;
    }
    if (t.find("5. **Scope:**") == 0) {
      found_scope = true;
      // A lone LF must start a new paragraph, not join this onto item 1.
      EXPECT_EQ(std::string::npos, t.find("spell_text"));
    }
  }
  EXPECT_TRUE(found_item1);
  EXPECT_TRUE(found_scope);
}

TEST_F(FsedModelWithViewTest, WrapOntoNextRow_NewLineHasOnlyOverflow) {
  ed.set_max_line_len(wwiv::fsed::editor_wrap_columns(80));
  const std::string line = "5. **Scope:** FSED only for v1, or should the line editor get a later hook?";
  ASSERT_LT(static_cast<int>(line.size()), ed.max_line_len());
  paste_keys(ed, line);
  ASSERT_EQ(1, wwiv::stl::size_int(ed)); // may be 2 if trailing empty? no newline
  // Type enough to wrap the last word onto a new row.
  paste_keys(ed, " extra words that force a wrap onto the next editor row now");
  EXPECT_LE(max_line_cells(ed), ed.max_line_len());
  EXPECT_GE(wwiv::stl::size_int(ed), 2);
  EXPECT_TRUE(ed.line(0).wrapped());
  const auto last = line_text(ed.line(wwiv::stl::size_int(ed) - 1));
  // The new row is only the overflow; it must not still contain the whole
  // original line (the stale-tail bug).
  EXPECT_EQ(std::string::npos, last.find("5. **Scope:**"));
  EXPECT_NE(std::string::npos, joined_text(ed).find("later hook?"));
  EXPECT_NE(std::string::npos, joined_text(ed).find("extra words that force a wrap"));
}

TEST_F(FsedModelWithViewTest, Paste_PipeCodesStayInTheLine) {
  ed.set_max_line_len(wwiv::fsed::editor_wrap_columns(80));
  paste_keys(ed, "strip heart and pipe (`|#n`, `|nn`) codes; skip quoted lines.\r\n");
  bool found = false;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    const auto t = line_text(ed.line(i));
    if (t.find("|#n") != std::string::npos || t.find("|nn") != std::string::npos) {
      found = true;
    }
    EXPECT_LE(wwiv::stl::size_int(ed.line(i)), ed.max_line_len()) << t;
  }
  EXPECT_TRUE(found);
}


// Exact last block of the spellcheck plan (Unix newlines;
// item 4 uses typographic quotes).  Wrapped at 79 this is about 12 lines,
// but a sysop paste of the whole plan is 173 lines and hits
// GetMaxMessageLinesAllowed() (120).  Refusing to reflow at that cap
// left these items wider than the screen.
static const char kOpenQuestionsBlock[] =
    "## Open questions for Rushfan\n"
    "\n"
    "1. **License:** is optional, dynamically linked Hunspell acceptable for an Apache-2.0 tree? Static vcpkg linking on Windows may be harder. Alternative engines?\n"
    "2. **User storage:** agree on the `data/spell/users/{usernum}.json` sidecar, or prefer burning `res_byte`/`res_char` in `userrec` (without changing the 1024-byte layout)?\n"
    "3. **Dictionaries:** require the sysop to install them, or ship `en_US` under `install/dict`?\n"
    "4. **Save behavior:** auto-run the walk on Save when enabled (as above), or prompt \xE2\x80\x9CSpell check before saving?\xE2\x80\x9D first?\n"
    "5. **Scope:** FSED only for v1, or should the line editor get a later hook?\n";

static void expect_open_questions_wrapped(const wwiv::fsed::FsedModel& ed,
                                          const char* where) {
  EXPECT_LE(max_line_cells(ed), ed.max_line_len()) << where;
  const auto all = joined_text(ed);
  EXPECT_NE(std::string::npos, all.find("Apache-2.0 tree?")) << where;
  EXPECT_NE(std::string::npos, all.find("or prefer burning `res_byte`")) << where;
  EXPECT_NE(std::string::npos, all.find("`install/dict`?")) << where;
  EXPECT_NE(std::string::npos, all.find("4. **Save behavior:**")) << where;
  EXPECT_NE(std::string::npos, all.find("5. **Scope:** FSED only for v1")) << where;

  bool saw_apache = false;
  for (int i = 0; i < wwiv::stl::size_int(ed); i++) {
    const auto t = line_text(ed.line(i));
    EXPECT_LE(wwiv::stl::size_int(ed.line(i)), ed.max_line_len()) << where << " :" << t;
    // Mid-word terminal autowrap of unwrapped item 1 split at column 80:
    // "... an Apach" / "e-2.0 tree?...".  Word wrap keeps Apache-2.0 intact.
    EXPECT_NE(0u, t.find("e-2.0 tree?")) << where << " :" << t;
    EXPECT_FALSE(t.size() >= 5 && t.compare(t.size() - 5, 5, "Apach") == 0) << where << " :" << t;
    if (t.find("Apache-2.0") != std::string::npos) {
      saw_apache = true;
    }
    if (t.find("2. **User storage:**") == 0) {
      EXPECT_TRUE(ed.line(i).wrapped()) << where;
      ASSERT_LT(i + 1, wwiv::stl::size_int(ed)) << where;
      EXPECT_NE(std::string::npos, line_text(ed.line(i + 1)).find("or prefer burning"))
          << where;
    }
  }
  EXPECT_TRUE(saw_apache) << where;
}

static std::string crlf_copy(const std::string& s) {
  std::string out;
  out.reserve(s.size() + 8);
  for (char c : s) {
    if (c == '\n') {
      out += "\r\n";
    } else {
      out += c;
    }
  }
  return out;
}

TEST_F(FsedModelWithViewTest, Paste_OpenQuestionsBlock_WrapsAtWordBoundaries) {
  ed.set_max_line_len(wwiv::fsed::editor_wrap_columns(80));
  paste_keys(ed, kOpenQuestionsBlock);
  expect_open_questions_wrapped(ed, "unix lf, under maxli");
  expect_viewport_consistent(ed, *view, "unix lf, under maxli");
}

TEST_F(FsedModelWithViewTest, Paste_OpenQuestionsBlock_CrLf_WrapsAtWordBoundaries) {
  ed.set_max_line_len(wwiv::fsed::editor_wrap_columns(80));
  paste_keys(ed, crlf_copy(kOpenQuestionsBlock));
  expect_open_questions_wrapped(ed, "crlf, under maxli");
}

// Sysop cap is 120 lines.  Fill almost to that cap, then paste the exact
// final block.  The message must never grow past maxli, and no line may be
// left wider than the screen (refusing to reflow used to leave 159/170-
// character lines that the terminal autowrapped mid-word at column 80).
// Text that does not fit is dropped, as with any other key at the limit.
static void expect_capped_at_maxli(wwiv::fsed::FsedModel& ed, const char* where) {
  EXPECT_LE(wwiv::stl::size_int(ed), ed.maxli()) << where;
  EXPECT_LE(max_line_cells(ed), ed.max_line_len()) << where;
  const auto lines = ed.to_lines(true);
  EXPECT_LE(wwiv::stl::size_int(lines), ed.maxli()) << where;
  const auto all = joined_text(ed);
  EXPECT_NE(std::string::npos, all.find("## Open questions for Rushfan")) << where;
  // The start of item 1 fits; the rest of the block is past the limit.
  EXPECT_NE(std::string::npos, all.find("1. **License:**")) << where;
  EXPECT_EQ(std::string::npos, all.find("5. **Scope:**")) << where;
}

static std::string filler_then(const std::string& tail, int n) {
  std::string burst;
  for (int i = 0; i < n; i++) {
    burst += "Filler line.\n";
  }
  return burst + tail;
}

TEST_F(FsedModelWithViewTest, Paste_OpenQuestionsBlock_AtSysopMaxli_StopsAtLimit) {
  wwiv::fsed::FsedModel ed120{120};
  auto v = std::make_shared<FakeView>();
  ed120.set_view(v);
  ed120.set_max_line_len(wwiv::fsed::editor_wrap_columns(80));
  // 117 short lines plus the heading/blank of the block leaves one line
  // for item 1, which needs two once wrapped.
  paste_keys(ed120, filler_then(kOpenQuestionsBlock, 117));
  expect_capped_at_maxli(ed120, "unix lf, at sysop maxli");
  expect_viewport_consistent(ed120, *v, "unix lf, at sysop maxli");
}

TEST_F(FsedModelWithViewTest, Paste_OpenQuestionsBlock_AtSysopMaxli_CrLf_StopsAtLimit) {
  wwiv::fsed::FsedModel ed120{120};
  auto v = std::make_shared<FakeView>();
  ed120.set_view(v);
  ed120.set_max_line_len(wwiv::fsed::editor_wrap_columns(80));
  paste_keys(ed120, filler_then(crlf_copy(kOpenQuestionsBlock), 117));
  expect_capped_at_maxli(ed120, "crlf, at sysop maxli");
}

// A single wrapped paragraph must not grow past maxli either, and the
// character that would need the extra line is rejected, not kept.
TEST_F(FsedModelWithViewTest, Wrap_AtMaxli_RejectsCharacterInsteadOfGrowing) {
  wwiv::fsed::FsedModel small{2};
  auto v = std::make_shared<FakeView>();
  small.set_view(v);
  small.set_max_line_len(10);
  for (const char c : std::string("aaaa bbbb cccc dddd")) {
    small.add(c);
  }
  ASSERT_EQ(2, wwiv::stl::size_int(small));
  // Filling the second line is fine; one more character needs a third line.
  EXPECT_NE(wwiv::fsed::editor_add_result_t::error, small.add('x'));
  ASSERT_EQ(2, wwiv::stl::size_int(small));
  const auto before = joined_text(small);
  const auto cx = small.cx;
  const auto curli = small.curli;
  EXPECT_EQ(wwiv::fsed::editor_add_result_t::error, small.add('y'));
  EXPECT_EQ(2, wwiv::stl::size_int(small));
  EXPECT_LE(max_line_cells(small), small.max_line_len());
  EXPECT_EQ(before, joined_text(small));
  EXPECT_EQ(curli, small.curli);
  EXPECT_EQ(cx, small.cx);
}

TEST_F(FsedModelWithViewTest, Enter_AtMaxli_DoesNotAddALine) {
  wwiv::fsed::FsedModel small{2};
  auto v = std::make_shared<FakeView>();
  small.set_view(v);
  small.add('a');
  EXPECT_TRUE(small.enter());
  small.add('b');
  EXPECT_FALSE(small.enter());
  EXPECT_EQ(2, wwiv::stl::size_int(small));
  EXPECT_EQ(1, small.curli);
  EXPECT_EQ(1, small.cx);
}
