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
#include "bbs_test/bbs_helper.h"
#include "core/stl.h"
#include "fsed/model.h"

#include "gtest/gtest.h"
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

class FsedModelTest : public ::testing::Test {
protected:
  FsedModelTest() { 
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

TEST_F(FsedModelTest, AddSingle_Character) {
  ed.add('H');
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(1, ed.cx);
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(1, wwiv::stl::ssize(ed));
}

TEST_F(FsedModelTest, AddSingleline) {
  const std::string line = "Hello World";
  add(line);
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(ssize(line), ed.cx);
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(1, wwiv::stl::ssize(ed));
}

TEST_F(FsedModelTest, AddTwolines) {
  const std::string line = "Hello\nWorld";
  add(line);
  EXPECT_EQ(ssize(std::string("World")), ed.cx);
  EXPECT_EQ(1, ed.curli);
  EXPECT_EQ(2, wwiv::stl::ssize(ed));
  EXPECT_EQ(1, ed.cy);
}

TEST_F(FsedModelTest, SixLines) {
  const std::string line = "Hello\nWorld\nThis\nIs\nA\nTest\n";
  add(line);
  EXPECT_EQ(6, ed.curli);
  EXPECT_EQ(7, wwiv::stl::ssize(ed));
  EXPECT_EQ(6, ed.cy);
}

TEST_F(FsedModelTest, PageUp_NoScroll) {
  const std::string line = "Hello\nWorld\nThis\nIs\nA\nTest\n";
  add(line);
  ed.cursor_pgup();
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(7, wwiv::stl::ssize(ed));
  EXPECT_EQ(0, ed.cy);
}

TEST_F(FsedModelTest, PageUp_Scroll) {
  add_test_lines(100);
  EXPECT_EQ(100, ed.curli);
  EXPECT_EQ(10, ed.cy);

  // Scroll up
  ed.cursor_pgup();
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(90, ed.curli);
}

TEST_F(FsedModelTest, BackSpace_Color) {
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
