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

#include "core/file.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/config.h"
#include "sdk/networks.h"
#include "sdk_test/sdk_helper.h"

using namespace std;
using namespace wwiv::sdk;
using namespace wwiv::strings;

// TODO(rushfan): These tests don't work yet - just testing locally
// for now. Need to create a tree under the tempdir containing a 
// stub BBS.
class ConfigTest : public testing::Test {
public:
  SdkHelper helper;
};

TEST_F(ConfigTest, Helper_CreatedBBSRoot) {
  ASSERT_TRUE(ends_with(helper.root(), "bbs")) << helper.root();
}

TEST_F(ConfigTest, Config_CurrentDirectory) {
  ASSERT_EQ(0, chdir(helper.root().c_str()));

  Config config;
  ASSERT_TRUE(config.IsInitialized());
  EXPECT_STREQ(helper.data_.c_str(), config.datadir().c_str());
}

TEST_F(ConfigTest, Config_DifferentDirectory) {
  Config config(helper.root());
  ASSERT_TRUE(config.IsInitialized());
  EXPECT_STREQ(helper.data_.c_str(), config.datadir().c_str());
}

TEST_F(ConfigTest, SetConfig_Stack) {
  Config config(helper.root());
  ASSERT_TRUE(config.IsInitialized());

  configrec c{};
  strcpy(c.systemname, "mysys");
  config.set_config(&c);
  ASSERT_STREQ(c.systemname, config.config()->systemname);
}

TEST_F(ConfigTest, SetConfig_Heap) {
  Config config(helper.root());
  ASSERT_TRUE(config.IsInitialized());

  configrec* c = new configrec();
  strcpy(c->systemname, "mysys");
  config.set_config(c);
  ASSERT_STREQ(c->systemname, config.config()->systemname);
  EXPECT_NE(nullptr, c);
  delete c;
}
