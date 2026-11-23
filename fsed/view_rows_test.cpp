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
/*    either  express  or implied.  See  the  License for the specific   */
/*    language governing permissions and limitations under the License.   */
/**************************************************************************/
#include "fsed/view_rows.h"
#include "fsed/newline.h"
#include "local_io/keycodes.h"

#include "gtest/gtest.h"
#include <string>
#include <utility>
#include <vector>

using namespace wwiv::fsed;

// Layout matching FullScreenView for a 24-row screen with screen_length 23:
// header 1-4, top bar 5, edit 6-21, status bar 22, command 23.
TEST(ViewRows, RowForLineStaysInEditArea) {
  const int lines_start = 6;
  const int lines_end = 22;
  EXPECT_FALSE(row_in_edit_area(5, lines_start, lines_end));
  EXPECT_TRUE(row_in_edit_area(6, lines_start, lines_end));
  EXPECT_TRUE(row_in_edit_area(21, lines_start, lines_end));
  EXPECT_FALSE(row_in_edit_area(22, lines_start, lines_end));
  EXPECT_FALSE(row_in_edit_area(23, lines_start, lines_end));

  EXPECT_EQ(6, row_for_line(0, 0, lines_start, lines_end).value());
  EXPECT_EQ(21, row_for_line(15, 0, lines_start, lines_end).value());
  EXPECT_FALSE(row_for_line(16, 0, lines_start, lines_end).has_value());
  // After a scroll, line 0 is no longer visible.
  EXPECT_FALSE(row_for_line(0, 1, lines_start, lines_end).has_value());
  EXPECT_EQ(6, row_for_line(1, 1, lines_start, lines_end).value());
}

TEST(ViewRows, PlanPaintsNeverLeaveEditArea) {
  const int lines_start = 6;
  const int lines_end = 22;
  // Long document, scrolled, plus a short one that does not fill the window.
  for (const auto& [top, doc] : {std::pair{0, 3}, {0, 40}, {12, 40}, {24, 40}}) {
    const auto paints = plan_edit_area_paints(top, doc, lines_start, lines_end);
    ASSERT_EQ(lines_end - lines_start, static_cast<int>(paints.size()));
    for (const auto& p : paints) {
      EXPECT_GE(p.y, lines_start);
      EXPECT_LT(p.y, lines_end);
      EXPECT_FALSE(p.y >= lines_end) << "must never paint the status bar row";
      if (p.line >= 0) {
        EXPECT_EQ(p.line, top + (p.y - lines_start));
        EXPECT_LT(p.line, doc);
      }
    }
  }
}

// Fake screen buffer: apply the paint plan and assert no writes land on
// the status bar, command line, or header/top-bar rows.
TEST(ViewRows, FakeScreenBuffer_PasteScrollStaysInEditArea) {
  const int rows = 24;
  const int cols = 80;
  const int lines_start = 6;
  const int lines_end = 22; // status bar
  std::vector<std::string> screen(rows + 1, std::string(cols, ' '));

  auto paint_window = [&](int top, int doc_lines) {
    const auto paints = plan_edit_area_paints(top, doc_lines, lines_start, lines_end);
    for (const auto& p : paints) {
      ASSERT_GE(p.y, 1);
      ASSERT_LE(p.y, rows);
      ASSERT_GE(p.y, lines_start);
      ASSERT_LT(p.y, lines_end);
      if (p.line < 0) {
        screen[p.y].assign(cols, ' ');
      } else {
        auto label = std::string("L") + std::to_string(p.line);
        screen[p.y].assign(cols, ' ');
        screen[p.y].replace(0, label.size(), label);
      }
    }
  };

  // Simulate a 100-line paste that keeps the cursor on the last edit row.
  const int visible = lines_end - lines_start;
  int top = 0;
  const int doc = 100;
  for (int curli = 0; curli < doc; ++curli) {
    if (curli - top >= visible) {
      top = curli - (visible - 1);
    }
    paint_window(top, curli + 1);
    for (int y = 1; y <= 5; ++y) {
      // header / top bar must not be written by the edit-area painter
    }
    for (int y = lines_end; y <= rows; ++y) {
      EXPECT_EQ(std::string(cols, ' '), screen[y]) << "row " << y << " after line " << curli;
    }
  }
}

TEST(ViewRows, EditorWrapColumnsIsWidthMinusOne) {
  EXPECT_EQ(79, editor_wrap_columns(80));
  EXPECT_EQ(39, editor_wrap_columns(40));
  EXPECT_EQ(1, editor_wrap_columns(1));
  EXPECT_EQ(1, editor_wrap_columns(0));
}

// 25-line default accounts become a 24-row frame; a user who already set
// 24 (to match the web terminal) must also get 24, not 23.
TEST(ViewRows, FullscreenLengthForUserLines) {
  EXPECT_EQ(24, fullscreen_length_for_user_lines(25));
  EXPECT_EQ(24, fullscreen_length_for_user_lines(24));
  EXPECT_EQ(29, fullscreen_length_for_user_lines(30));
  EXPECT_EQ(24, fullscreen_length_for_user_lines(0));
  EXPECT_EQ(24, fullscreen_length_for_user_lines(15));
}

// Layout matching FullScreenView for a 24-row screen with screen_length 24
// (web terminal, or a user who set 24 lines): header 1-4, top bar 5,
// edit 6-22, status bar 23, command 24.
TEST(ViewRows, TwentyFourRowFrame_EditAreaEndsAtStatusBar) {
  const int lines_start = 6;
  const int lines_end = 23;
  EXPECT_TRUE(row_in_edit_area(22, lines_start, lines_end));
  EXPECT_FALSE(row_in_edit_area(23, lines_start, lines_end));
  EXPECT_FALSE(row_in_edit_area(24, lines_start, lines_end));
  const auto paints = plan_edit_area_paints(0, 40, lines_start, lines_end);
  ASSERT_EQ(17, static_cast<int>(paints.size()));
  EXPECT_EQ(6, paints.front().y);
  EXPECT_EQ(22, paints.back().y);
}

TEST(NewLine, CrAlwaysEnters) {
  bool last = false;
  EXPECT_TRUE(consume_line_break(wwiv::local::io::RETURN, last));
  EXPECT_TRUE(last);
  last = true;
  EXPECT_TRUE(consume_line_break(wwiv::local::io::RETURN, last));
  EXPECT_TRUE(last);
}

TEST(NewLine, LfEntersUnlessItFollowsCr) {
  bool last = false;
  EXPECT_TRUE(consume_line_break(wwiv::local::io::SOFTRETURN, last));
  EXPECT_FALSE(last);

  last = true;
  EXPECT_FALSE(consume_line_break(wwiv::local::io::SOFTRETURN, last));
  EXPECT_FALSE(last);
}

TEST(NewLine, CrLfIsASingleEnter) {
  bool last = false;
  EXPECT_TRUE(consume_line_break(wwiv::local::io::RETURN, last));
  EXPECT_FALSE(consume_line_break(wwiv::local::io::SOFTRETURN, last));
  EXPECT_TRUE(consume_line_break(wwiv::local::io::RETURN, last));
  EXPECT_FALSE(consume_line_break(wwiv::local::io::SOFTRETURN, last));
}

TEST(NewLine, OtherKeysClearCrLatch) {
  bool last = true;
  EXPECT_FALSE(consume_line_break('A', last));
  EXPECT_FALSE(last);
  EXPECT_TRUE(consume_line_break(wwiv::local::io::SOFTRETURN, last));
}

TEST(ViewRows, ClampScreenRowNeverPastLastLine) {
  EXPECT_EQ(1, clamp_screen_row(0, 24));
  EXPECT_EQ(1, clamp_screen_row(1, 24));
  EXPECT_EQ(24, clamp_screen_row(24, 24));
  EXPECT_EQ(24, clamp_screen_row(25, 24));
  EXPECT_EQ(24, clamp_screen_row(27, 24));
  EXPECT_EQ(1, clamp_screen_row(-3, 24));
}

// Layout reminder: header 1-4, top bar 5, edit 6-22, status 23, command 24.
TEST(ViewRows, HeaderOccupiesFirstFiveRowsOn24LineScreen) {
  const int num_header_lines = 4;
  const int lines_start = num_header_lines + 2; // 6
  EXPECT_EQ(6, lines_start);
  EXPECT_FALSE(row_in_edit_area(5, 6, 23));
  EXPECT_TRUE(row_in_edit_area(6, 6, 23));
}

// A one-line change repaints one row, not the whole window (#1729 review).
TEST(ViewRows, RangePaintsOnlyTheInvalidatedRow) {
  // Rows 6..22 show lines 10..26 of a 40-line document.
  const auto p = plan_range_paints(12, 12, false, 10, 40, 6, 23);
  ASSERT_EQ(1u, p.size());
  EXPECT_EQ(8, p[0].y);
  EXPECT_EQ(12, p[0].line);
}

TEST(ViewRows, RangePaintsAreClippedToTheViewport) {
  // Lines 5..12 changed, but only 10..12 are visible.
  const auto p = plan_range_paints(5, 12, false, 10, 40, 6, 23);
  ASSERT_EQ(3u, p.size());
  EXPECT_EQ(6, p.front().y);
  EXPECT_EQ(10, p.front().line);
  EXPECT_EQ(12, p.back().line);
  // Entirely off screen: nothing to paint.
  EXPECT_TRUE(plan_range_paints(30, 35, false, 10, 40, 6, 23).empty());
  EXPECT_TRUE(plan_range_paints(1, 3, false, 10, 40, 6, 23).empty());
}

TEST(ViewRows, RangeToEofClearsRowsPastTheDocument) {
  // A 5-line document after a delete: lines 3..4 repaint, rows for 5..16 clear.
  const auto p = plan_range_paints(3, 4, true, 0, 5, 6, 23);
  ASSERT_EQ(14u, p.size());
  EXPECT_EQ(9, p[0].y);
  EXPECT_EQ(3, p[0].line);
  EXPECT_EQ(4, p[1].line);
  for (size_t i = 2; i < p.size(); ++i) {
    EXPECT_EQ(-1, p[i].line);
  }
  EXPECT_EQ(22, p.back().y);
  for (const auto& x : p) {
    EXPECT_TRUE(row_in_edit_area(x.y, 6, 23));
  }
}

// Normal typing never triggers a full-frame repaint; a paste does, once.
TEST(ViewRows, PasteBurstOnlyFiresAfterAQueuedRun) {
  paste_burst_t b;
  // Typing: nothing queued on each pass.
  for (int i = 0; i < 50; ++i) {
    EXPECT_FALSE(b.drained(false));
  }
  // A key or two queued (fast typist) is not a paste.
  EXPECT_FALSE(b.drained(true));
  EXPECT_FALSE(b.drained(true));
  EXPECT_FALSE(b.drained(false));
  // A paste: many keys already waiting, then the queue drains.
  for (int i = 0; i < 200; ++i) {
    EXPECT_FALSE(b.drained(true));
  }
  EXPECT_TRUE(b.drained(false));
  // Only once.
  EXPECT_FALSE(b.drained(false));
}
