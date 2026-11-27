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

#include "common/input_line_editor.h"
#include <string>

using namespace wwiv::common;
using CellKind = InputLineEditor::CellKind;

namespace {

// The field as drawn: '<' and '>' for the arrows, '.' for empty cells.
std::string field(const InputLineEditor& ed) {
  std::string s;
  for (auto i = 0; i < ed.width(); i++) {
    const auto c = ed.cell(i);
    switch (c.kind) {
    case CellKind::more_left: s.push_back('<'); break;
    case CellKind::more_right: s.push_back('>'); break;
    case CellKind::empty: s.push_back('.'); break;
    case CellKind::text: s.push_back(c.ch); break;
    }
  }
  return s;
}

void type(InputLineEditor& ed, const std::string& s) {
  for (const auto ch : s) {
    ed.type(static_cast<unsigned char>(ch));
  }
}

// The cursor must always be inside the field and never on an arrow.
void expect_cursor_ok(const InputLineEditor& ed) {
  const auto cell = ed.cursor_cell();
  ASSERT_GE(cell, 0);
  if (cell == ed.width()) {
    // Only a full field puts the cursor just past its last cell.
    ASSERT_EQ(0, ed.offset()) << field(ed);
    ASSERT_EQ(static_cast<int>(ed.text().size()), ed.width()) << field(ed);
    ASSERT_EQ(ed.pos(), ed.width()) << field(ed);
    return;
  }
  ASSERT_LT(cell, ed.width());
  const auto kind = ed.cell(cell).kind;
  EXPECT_NE(CellKind::more_left, kind) << field(ed);
  EXPECT_NE(CellKind::more_right, kind) << field(ed);
}

} // namespace

TEST(InputLineEditorTest, ArrowCharsAreCp437Guillemets) {
  EXPECT_EQ('\xAE', InputLineEditor::more_left_char);
  EXPECT_EQ('\xAF', InputLineEditor::more_right_char);
}

TEST(InputLineEditorTest, FitsWithoutScrolling) {
  InputLineEditor ed("hello", 20, 10, InputMode::MIXED, true);
  EXPECT_EQ("hello.....", field(ed));
  EXPECT_EQ(5, ed.cursor_cell());
  EXPECT_FALSE(ed.more_left());
  EXPECT_FALSE(ed.more_right());
}

TEST(InputLineEditorTest, LongOriginal_CursorAtEnd_ShowsLeftArrow) {
  InputLineEditor ed("abcdefghijklmnopqrstuvwxyz", 40, 10, InputMode::MIXED, true);
  // The cursor is in the last cell, just after the 'z'.
  EXPECT_EQ("<stuvwxyz.", field(ed));
  EXPECT_EQ(9, ed.cursor_cell());
  EXPECT_TRUE(ed.more_left());
  EXPECT_FALSE(ed.more_right());
}

TEST(InputLineEditorTest, Home_ShowsRightArrow) {
  InputLineEditor ed("abcdefghijklmnopqrstuvwxyz", 40, 10, InputMode::MIXED, true);
  ed.home();
  EXPECT_EQ("abcdefghi>", field(ed));
  EXPECT_EQ(0, ed.cursor_cell());
  ed.end();
  EXPECT_EQ("<stuvwxyz.", field(ed));
}

TEST(InputLineEditorTest, Middle_ShowsBothArrows) {
  InputLineEditor ed("abcdefghijklmnopqrstuvwxyz", 40, 10, InputMode::MIXED, true);
  ed.home();
  for (auto i = 0; i < 12; i++) {
    ed.right();
    expect_cursor_ok(ed);
  }
  EXPECT_EQ('m', ed.text()[ed.pos()]);
  EXPECT_TRUE(ed.more_left());
  EXPECT_TRUE(ed.more_right());
  const auto f = field(ed);
  EXPECT_EQ('<', f.front());
  EXPECT_EQ('>', f.back());
  EXPECT_EQ('m', f[ed.cursor_cell()]);
}

TEST(InputLineEditorTest, ScrollsInJumps) {
  InputLineEditor ed("", 100, 12, InputMode::MIXED, true);
  type(ed, "0123456789A");
  EXPECT_EQ(0, ed.offset());
  // Typing into the last cell scrolls by a third of the field (4).
  type(ed, "B");
  EXPECT_EQ(5, ed.offset());
  expect_cursor_ok(ed);
  const auto offset = ed.offset();
  type(ed, "CD");
  EXPECT_EQ(offset, ed.offset()); // no scroll for each key
}

TEST(InputLineEditorTest, CursorNeverOnArrow_TypingAndMoving) {
  InputLineEditor ed("", 200, 8, InputMode::MIXED, true);
  for (auto i = 0; i < 150; i++) {
    ed.type(static_cast<unsigned char>('a' + i % 26));
    expect_cursor_ok(ed);
  }
  for (auto i = 0; i < 160; i++) {
    ed.left();
    expect_cursor_ok(ed);
  }
  EXPECT_EQ(0, ed.pos());
  EXPECT_EQ(0, ed.offset());
  for (auto i = 0; i < 160; i++) {
    ed.right();
    expect_cursor_ok(ed);
  }
  EXPECT_EQ(150, ed.pos());
}

TEST(InputLineEditorTest, CursorNeverOnArrow_Deleting) {
  InputLineEditor ed(std::string(60, 'x'), 100, 10, InputMode::MIXED, true);
  ed.home();
  for (auto i = 0; i < 30; i++) {
    ed.right();
  }
  while (!ed.text().empty()) {
    if (ed.pos() > 0) {
      ed.backspace();
    } else {
      ed.del();
    }
    expect_cursor_ok(ed);
  }
  EXPECT_EQ("..........", field(ed));
}

TEST(InputLineEditorTest, Deleting_PullsHiddenTextBackIntoView) {
  InputLineEditor ed("abcdefghijklmnopqrstuvwxyz", 40, 10, InputMode::MIXED, true);
  for (auto i = 0; i < 20; i++) {
    ed.backspace();
  }
  EXPECT_EQ("abcdef", ed.text());
  EXPECT_EQ(0, ed.offset());
  EXPECT_EQ("abcdef....", field(ed));
}

TEST(InputLineEditorTest, MaxLength_StopsTyping) {
  InputLineEditor ed("", 5, 10, InputMode::MIXED, true);
  type(ed, "abcdefgh");
  EXPECT_EQ("abcde", ed.text());
}

TEST(InputLineEditorTest, MaxLength_OverwriteStillAllowedWhenFull) {
  InputLineEditor ed("abcde", 5, 10, InputMode::MIXED, false);
  ed.home();
  type(ed, "XY");
  EXPECT_EQ("XYcde", ed.text());
}

TEST(InputLineEditorTest, OriginalLongerThanMax_EditableButCantGrow) {
  const std::string orig(120, 'a');
  InputLineEditor ed(orig, 79, 78, InputMode::CMDLINE, true);
  EXPECT_EQ(120u, ed.text().size());
  type(ed, "b"); // insert: can't grow
  EXPECT_EQ(orig, ed.text());
  ed.backspace();
  ed.backspace();
  EXPECT_EQ(118u, ed.text().size());
  type(ed, "b"); // still over max_length
  EXPECT_EQ(118u, ed.text().size());
  ed.toggle_insert(); // CMDLINE ignores Insert
  EXPECT_TRUE(ed.insert());
}

TEST(InputLineEditorTest, Clear) {
  InputLineEditor ed("abcdefghijklmnopqrstuvwxyz", 40, 10, InputMode::MIXED, true);
  ed.clear();
  EXPECT_EQ("", ed.text());
  EXPECT_EQ(0, ed.offset());
  EXPECT_EQ("..........", field(ed));
}

TEST(InputLineEditorTest, InsertInMiddle) {
  InputLineEditor ed("abef", 10, 10, InputMode::MIXED, true);
  ed.left();
  ed.left();
  type(ed, "cd");
  EXPECT_EQ("abcdef", ed.text());
  EXPECT_EQ(4, ed.pos());
}

TEST(InputLineEditorTest, Upper) {
  InputLineEditor ed("", 10, 10, InputMode::UPPER, true);
  type(ed, "abc");
  EXPECT_EQ("ABC", ed.text());
}

TEST(InputLineEditorTest, Proper) {
  InputLineEditor ed("", 20, 20, InputMode::PROPER, true);
  type(ed, "jOHN smith");
  EXPECT_EQ("John Smith", ed.text());
}

TEST(InputLineEditorTest, Filename_Disallowed) {
  InputLineEditor ed("", 20, 20, InputMode::FILENAME, true);
  type(ed, "a/b:c*d.txt");
  EXPECT_EQ("abcd.txt", ed.text());
}

TEST(InputLineEditorTest, FullPathName_Disallowed) {
  InputLineEditor ed("", 20, 20, InputMode::FULL_PATH_NAME, true);
  type(ed, "c:/a<b>*c");
  EXPECT_EQ("c:/abc", ed.text());
}

TEST(InputLineEditorTest, Date_InsertsSeparators) {
  InputLineEditor ed("", 10, 10, InputMode::DATE, true);
  type(ed, "12252026");
  EXPECT_EQ("12/25/2026", ed.text());
  type(ed, "9");
  EXPECT_EQ("12/25/2026", ed.text());
}

TEST(InputLineEditorTest, Date_TypedSeparator) {
  InputLineEditor ed("", 10, 10, InputMode::DATE, true);
  type(ed, "12/25/2026");
  EXPECT_EQ("12/25/2026", ed.text());
}

TEST(InputLineEditorTest, Date_BackspaceRemovesSeparator) {
  InputLineEditor ed("", 10, 10, InputMode::DATE, true);
  type(ed, "123");
  EXPECT_EQ("12/3", ed.text());
  ed.backspace();
  EXPECT_EQ("12/", ed.text());
  // Erasing the separator also erases the digit before it.
  ed.backspace();
  EXPECT_EQ("1", ed.text());
}

TEST(InputLineEditorTest, Phone_InsertsSeparators) {
  InputLineEditor ed("", 12, 12, InputMode::PHONE, true);
  type(ed, "2065551212");
  EXPECT_EQ("206-555-1212", ed.text());
}

TEST(InputLineEditorTest, NarrowField_NoArrows) {
  InputLineEditor ed("abcdefgh", 20, 3, InputMode::MIXED, true);
  EXPECT_FALSE(ed.more_left());
  EXPECT_EQ("gh.", field(ed));
  expect_cursor_ok(ed);
}

// When text exactly fills the field, do not scroll the first character off
// to reserve a cursor cell after the text. That made 2- and 3-character
// fields (state, country) look like each new key overwrote the last.
TEST(InputLineEditorTest, ExactFit_TwoCharState) {
  InputLineEditor ed("", 2, 2, InputMode::UPPER, true);
  type(ed, "c");
  EXPECT_EQ("C", ed.text());
  EXPECT_EQ("C.", field(ed));
  EXPECT_EQ(1, ed.cursor_cell());
  EXPECT_EQ(0, ed.offset());
  type(ed, "a");
  EXPECT_EQ("CA", ed.text());
  EXPECT_EQ("CA", field(ed));
  // The cursor sits just past the full field, so Left visibly moves it.
  EXPECT_EQ(2, ed.cursor_cell());
  EXPECT_EQ(0, ed.offset());
  EXPECT_FALSE(ed.more_left());
  expect_cursor_ok(ed);
  ed.left();
  EXPECT_EQ(1, ed.cursor_cell());
  expect_cursor_ok(ed);
}

TEST(InputLineEditorTest, ExactFit_OverwriteLastChar) {
  InputLineEditor ed("", 2, 2, InputMode::UPPER, false);
  type(ed, "ca");
  EXPECT_EQ(2, ed.cursor_cell());
  // At the end of a full field there is nothing to overwrite.
  type(ed, "x");
  EXPECT_EQ("CA", ed.text());
  ed.left();
  EXPECT_EQ(1, ed.cursor_cell());
  type(ed, "x");
  EXPECT_EQ("CX", ed.text());
  EXPECT_EQ("CX", field(ed));
  EXPECT_EQ(2, ed.cursor_cell());
  EXPECT_EQ(0, ed.offset());
  expect_cursor_ok(ed);
}

TEST(InputLineEditorTest, ExactFit_ThreeCharCountry) {
  InputLineEditor ed("", 3, 3, InputMode::UPPER, true);
  type(ed, "usa");
  EXPECT_EQ("USA", ed.text());
  EXPECT_EQ("USA", field(ed));
  EXPECT_EQ(3, ed.cursor_cell());
  EXPECT_EQ(0, ed.offset());
  EXPECT_FALSE(ed.more_left());
  expect_cursor_ok(ed);
}

TEST(InputLineEditorTest, ExactFit_OriginalTextFillsField) {
  InputLineEditor ed("CA", 2, 2, InputMode::UPPER, true);
  EXPECT_EQ("CA", field(ed));
  EXPECT_EQ(2, ed.cursor_cell());
  EXPECT_EQ(0, ed.offset());
  expect_cursor_ok(ed);
}

TEST(InputLineEditorTest, ControlCharsIgnored) {
  InputLineEditor ed("", 20, 20, InputMode::MIXED, true);
  ed.type(7);
  ed.type(255);
  EXPECT_EQ("", ed.text());
}

TEST(InputLineEditorTest, RandomKeys_InvariantsHold) {
  // A fixed-seed LCG so failures reproduce.
  uint32_t seed = 12345;
  auto next = [&seed](uint32_t n) {
    seed = seed * 1103515245u + 12345u;
    return (seed >> 16) % n;
  };
  for (const auto width : {1, 2, 3, 4, 5, 10, 78}) {
    // 120: a viewport that scrolls. width: a full field can't grow
    // (state, country), so it must never scroll.
    for (const auto maxlen : {120, width}) {
      for (const auto insert : {true, false}) {
        InputLineEditor ed("", maxlen, width, InputMode::MIXED, insert);
        std::string model;
        for (auto i = 0; i < 5000; i++) {
          switch (next(8)) {
          case 0: ed.left(); break;
          case 1: ed.right(); break;
          case 2: ed.backspace(); break;
          case 3: ed.del(); break;
          case 4: if (next(20) == 0) { ed.home(); } break;
          case 5: if (next(20) == 0) { ed.end(); } break;
          default: ed.type(static_cast<unsigned char>('a' + next(26))); break;
          }
          ASSERT_LE(ed.text().size(), static_cast<size_t>(maxlen));
          ASSERT_GE(ed.pos(), 0);
          ASSERT_LE(ed.pos(), static_cast<int>(ed.text().size()));
          ASSERT_GE(ed.offset(), 0);
          ASSERT_LE(ed.offset(), ed.pos());
          expect_cursor_ok(ed);
          // Cells show the text that's scrolled into view.
          for (auto c = 0; c < ed.width(); c++) {
            const auto cell = ed.cell(c);
            if (cell.kind == CellKind::text) {
              ASSERT_EQ(ed.text()[ed.offset() + c], cell.ch);
            }
          }
          // Nothing stays hidden to the left once all the text fits with room
          // to spare. When size == width and the field can still grow, a slack
          // cell is reserved (see ExactFit_* for the can't-grow case).
          if (static_cast<int>(ed.text().size()) < width) {
            ASSERT_EQ(0, ed.offset()) << field(ed);
            ASSERT_FALSE(ed.more_left()) << field(ed);
          }
          // A field whose width equals its limit never scrolls.
          if (maxlen == width) {
            ASSERT_EQ(0, ed.offset()) << field(ed);
          }
        }
    }
    }
  }
}
