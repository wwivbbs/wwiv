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
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#include "bbs/execexternal.h"

#include "bbs/bbs_helper.h"
#include "bbs/exec.h"
#include "gtest/gtest.h"
#include "sdk/vardec.h"

class ExecExternalTest : public ::testing::Test {
protected:
  void SetUp() override {
    helper.SetUp();
    ASSERT_TRUE(helper.files().Mkdir("temp"));
    helper.sess().dirs().temp_directory(helper.files().DirName("temp"));
    a()->init_status_and_instances_for_test();
  }

  BbsHelper helper;
};

// #1737: the rest of the line that picked the program (here the LF of the
// caller's CR LF) must not be left for the program to read.
TEST_F(ExecExternalTest, PurgesPendingCallerInput) {
  helper.io()->push_remote_keys("\n");
  ASSERT_TRUE(helper.io()->remote_io()->incoming());

  wwiv::bbs::CommandLine cl("true");
  EXPECT_EQ(0, ExecuteExternalProgram(cl, EFLAG_NOHUP));

  EXPECT_FALSE(helper.io()->remote_io()->incoming());
}
