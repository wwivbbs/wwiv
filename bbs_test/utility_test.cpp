/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2020, WWIV Software Services               */
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

#include "bbs/utility.h"
#include "bbs_test/bbs_helper.h"
#include "gtest/gtest.h"
#include <ctime>
#include <string>

static const time_t MAY_18_2014 = 1400461200;

using std::cout;
using std::endl;
using std::string;

/*
class UtilityTest : public ::testing::Test {
protected:
  void SetUp() override {
      helper.SetUp();
    }
    BbsHelper helper{};
};
*/

TEST(UtilityTest, StripFnSmoke) {
  EXPECT_EQ("", stripfn(""));
  EXPECT_EQ("", stripfn("/"));
  EXPECT_EQ("", stripfn("///"));
  EXPECT_EQ("", stripfn("C:\\"));

  EXPECT_EQ("foo.zip", stripfn("foo     .zip"));
  EXPECT_EQ("foo.zip", stripfn("foo.zip"));
  EXPECT_EQ("foo.zip", stripfn("FOO.ZIP"));
  EXPECT_EQ("foo.zip", stripfn("X\\FOO.ZIP"));
  EXPECT_EQ("foo.zip", stripfn("X/FOO.ZIP"));
  EXPECT_EQ("foo.zip", stripfn("/X/FOO.ZIP"));
  EXPECT_EQ("foo.zip", stripfn("//X/FOO.ZIP"));
  EXPECT_EQ("foo.zip", stripfn("C:\\X\\FOO.ZIP"));
  EXPECT_EQ("foo.zip", stripfn("C:\\X\\FOO  .ZIP"));
  EXPECT_EQ("foo", stripfn("C:\\X\\FOO"));
}
