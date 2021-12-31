/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2021, WWIV Software Services               */
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
#include "core/file.h"
#include "core/strings.h"
#include "core/version.h"
#include "sdk/config.h"
#include "sdk/sdk_helper.h"

#include "gtest/gtest.h"
#include <string>

// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/vardec.h"

using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

class ConfigTest : public testing::Test {
public:
  SdkHelper helper;
};

TEST_F(ConfigTest, Helper_CreatedBBSRoot) {
  ASSERT_TRUE(ends_with(helper.root(), "bbs")) << helper.root();
}

TEST_F(ConfigTest, Helper_ConfigWorks) { ASSERT_EQ(helper.config().datadir(), helper.data()); }

TEST_F(ConfigTest, Config_CurrentDirectory) {
  ASSERT_EQ(0, chdir(helper.root().c_str()));

  const Config config(File::current_directory());
  ASSERT_TRUE(config.IsInitialized());
  EXPECT_EQ(helper.data_, config.datadir());
}

TEST_F(ConfigTest, Config_DifferentDirectory) {
  const Config config(helper.root());
  ASSERT_TRUE(config.IsInitialized());
  EXPECT_EQ(helper.data_, config.datadir());
}

TEST_F(ConfigTest, Config_WrongDirectory) {
  const Config config(StrCat(helper.root(), "x"));
  ASSERT_FALSE(config.IsInitialized());
}

TEST_F(ConfigTest, SetConfig) {
  Config config(helper.root());
  ASSERT_TRUE(config.IsInitialized());

  config_t c{};
  c.systemname = "mysys";
  config.set_config(c, true);
  ASSERT_EQ("mysys", config.system_name());
}

TEST_F(ConfigTest, WrittenByNumVersion) {
  const Config config(helper.root());

  ASSERT_EQ(wwiv_config_version(), config.written_by_wwiv_num_version());
}

TEST_F(ConfigTest, Is5XXOrLater) {
  const Config config(helper.root());

  ASSERT_TRUE(config.is_5xx_or_later());
}

TEST_F(ConfigTest, LogDirFromConfig_Found) {
  const Config config(helper.root());
  ASSERT_EQ(config.logdir(), LogDirFromConfig(helper.root()));
}

TEST_F(ConfigTest, LogDirFromConfig_NotFound) {
  Config config(helper.root());
  ASSERT_EQ("", LogDirFromConfig(StrCat(helper.root(), "x")));
}
