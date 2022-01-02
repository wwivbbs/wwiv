/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2022, WWIV Software Services               */
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
#include "common/output.h"
#include "common/pause.h"

#include "gtest/gtest.h"
#include <string>

using namespace wwiv::common;
using wwiv::sdk::User;

class PauseTest : public ::testing::Test {
protected:
  void SetUp() override { helper.SetUp(); }

  BbsHelper helper{};
};

TEST_F(PauseTest, Smoke) {
  helper.user()->set_flag(User::pauseOnPage);
  {
    TempDisablePause disable_pause(bout);
    EXPECT_FALSE(helper.user()->pause());
  }
  EXPECT_TRUE(helper.user()->pause());
}
