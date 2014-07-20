/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2014, WWIV Software Services                  */
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

#include <iostream>
#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/pause.h"
#include "bbs_test/bbs_helper.h"
#include "core/strings.h"
#include "core_test/file_helper.h"

using std::cout;
using std::endl;
using std::string;

using wwiv::bbs::TempDisablePause;

class PauseTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        helper.SetUp();
    }

    BbsHelper helper;
};

TEST_F(PauseTest, Smoke) {
  helper.user()->SetStatusFlag(WUser::pauseOnPage);
  {
    TempDisablePause disable_pause;
    EXPECT_FALSE(helper.user()->HasPause());
  }
  EXPECT_TRUE(helper.user()->HasPause());
}
