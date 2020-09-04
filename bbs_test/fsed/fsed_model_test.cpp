/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2020, WWIV Software Services               */
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

#include "fsed/model.h"
#include "bbs_test/bbs_helper.h"
#include "core/strings.h"
#include "core/stl.h"
#include <iostream>
#include <memory>
#include <string>

using std::cout;
using std::endl;
using std::string;
using namespace wwiv::stl;

class FakeView : public wwiv::bbs::fsed::editor_viewport_t {
public:
  // Inherited via editor_viewport_t
  int max_view_lines() const override { return 10; }
  int max_view_columns() const override { return 20; }
  int top_line() const override { return top_line_; }
  void set_top_line(int l) override { top_line_ = l; }
  void gotoxy(const wwiv::bbs::fsed::FsedModel& ed) override { 
    x_ = ed.cx;
    y_ = ed.cy;
  }

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

  void Add(string s) {
    for (const auto& c : s) {
      if (c == '\n') {
        ed.enter();
        continue;
      } 
      ed.add(c);
    }
  }

  void AddTestLines(int n) {
    for (int i = 0; i < n; i++) {
      Add(fmt::format("Test-{}\n", i));
    }
  }

  wwiv::bbs::fsed::FsedModel ed{255};
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
  Add(line);
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(ssize(line), ed.cx);
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(1, wwiv::stl::ssize(ed));
}

TEST_F(FsedModelTest, AddTwolines) {
  const std::string line = "Hello\nWorld";
  Add(line);
  EXPECT_EQ(ssize(std::string("World")), ed.cx);
  EXPECT_EQ(1, ed.curli);
  EXPECT_EQ(2, wwiv::stl::ssize(ed));
  EXPECT_EQ(1, ed.cy);
}

TEST_F(FsedModelTest, SixLines) {
  const std::string line = "Hello\nWorld\nThis\nIs\nA\nTest\n";
  Add(line);
  EXPECT_EQ(6, ed.curli);
  EXPECT_EQ(7, wwiv::stl::ssize(ed));
  EXPECT_EQ(6, ed.cy);
}

TEST_F(FsedModelTest, PageUp_NoScroll) {
  const std::string line = "Hello\nWorld\nThis\nIs\nA\nTest\n";
  Add(line);
  ed.cursor_pgup();
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(7, wwiv::stl::ssize(ed));
  EXPECT_EQ(0, ed.cy);
}

TEST_F(FsedModelTest, PageUp_Scroll) {
  AddTestLines(100);
  EXPECT_EQ(100, ed.curli);
  EXPECT_EQ(10, ed.cy);

  // Scroll up
  ed.cursor_pgup();
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(90, ed.curli);
}
