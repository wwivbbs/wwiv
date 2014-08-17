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
#include <iostream>

#include "gtest/gtest.h"
#include "bbs_test/bbs_helper.h"

#include "bbs/utility.h"
#include "bbs/platform/platformfcns.h"

using std::cout;
using std::endl;
using std::string;

class MakeAbsTest : public ::testing::Test {
protected:
    virtual void SetUp() {
        helper.SetUp();
        root = helper.app_->GetHomeDir();
    }
    BbsHelper helper;
    string root;
};

#ifdef _WIN32

TEST_F(MakeAbsTest, Smoke) {
  const string expected = "c:\\windows\\system32\\cmd.exe foo";
  string cmdline = "cmd foo";
  WWIV_make_abs_cmd(root, &cmdline);
  EXPECT_STRCASEEQ(expected.c_str(), cmdline.c_str());
}

#else 

TEST_F(MakeAbsTest, Smoke) {
  const string expected = "/bin/ls foo";
  string cmdline = "ls foo";
  WWIV_make_abs_cmd(root, &cmdline);
  EXPECT_STRCASEEQ(expected.c_str(), cmdline.c_str());
}


#endif  // _WIN32