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

#include "bbs/fsed/model.h"
#include "bbs_test/bbs_helper.h"
#include "core/strings.h"
#include <iostream>
#include <memory>
#include <string>

using std::cout;
using std::endl;
using std::string;

class FsedModelTest : public ::testing::Test {
protected:
  int Puts(string s) {
    for (const auto& c : s) {
      if (c == '\n') {
        ed.enter();
        continue;
      } 
      ed.add(c);
    }
  }
  wwiv::bbs::fsed::FsedModel ed{255};
};

TEST_F(FsedModelTest, AddSingle_Character) {
  ed.add('H');
  EXPECT_EQ(0, ed.cy);
  EXPECT_EQ(1, ed.cx);
  EXPECT_EQ(0, ed.curli);
  EXPECT_EQ(1, wwiv::stl::ssize(ed));
}
