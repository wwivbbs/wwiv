/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2007-2020, WWIV Software Services               */
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

#include "bbs/trashcan.h"
#include "bbs_test/bbs_helper.h"
#include "core/strings.h"
#include "sdk/filenames.h"

using std::cout;
using std::endl;
using std::ostringstream;
using std::string;

using namespace wwiv::strings;

class TrashcanTest : public testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    const string text = "all\n*end\nstart*\n*sub*\n";
    helper.files().CreateTempFile(StrCat("gfiles/", TRASHCAN_TXT), text);
  }

public:
  BbsHelper helper;
};

TEST_F(TrashcanTest, SimpleCase) {
  Trashcan t(*a()->config());
  EXPECT_TRUE(t.IsTrashName("all"));
  EXPECT_FALSE(t.IsTrashName("al"));
  EXPECT_FALSE(t.IsTrashName("a"));

  EXPECT_TRUE(t.IsTrashName("end"));
  EXPECT_TRUE(t.IsTrashName("send"));
  EXPECT_FALSE(t.IsTrashName("ends"));

  EXPECT_TRUE(t.IsTrashName("start"));
  EXPECT_TRUE(t.IsTrashName("starting"));
  EXPECT_FALSE(t.IsTrashName("sstart"));

  EXPECT_TRUE(t.IsTrashName("sub"));
  EXPECT_TRUE(t.IsTrashName("ssub"));
  EXPECT_TRUE(t.IsTrashName("subs"));

  EXPECT_FALSE(t.IsTrashName("dude"));
  EXPECT_FALSE(t.IsTrashName("a"));
}
