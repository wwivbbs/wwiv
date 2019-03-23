/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2019, WWIV Software Services               */
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
#include "core/version.h"
#include "core_test/file_helper.h"
#include "sdk/config.h"
#include "sdk/files/allow.h"
#include "sdk_test/sdk_helper.h"

using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using namespace wwiv::strings;

class AllowTest : public testing::Test {
public:
public:
  AllowTest() : config_(helper.root()) {
    EXPECT_TRUE(config_.IsInitialized());
    config_.set_paths_for_test(helper.data(), helper.msgs(), helper.gfiles(), helper.menus(),
                               helper.dloads(), helper.scripts());
  }

  virtual void SetUp() { helper.SetUp(); }

  SdkHelper helper;
  Config config_;
};

TEST_F(AllowTest, Empty) {
  Allow a(config_);
  const auto v = a.allow_vector();
  EXPECT_EQ(0, v.size());
}

TEST_F(AllowTest, Add) {
  Allow a(config_);
  a.Add("1.zip");
  a.Add("2.zip");
  a.Add("3.zip");
  const auto v = a.allow_vector();
  ASSERT_EQ(3, v.size());
  allow_entry_t f = to_allow_entry("1.zip");
  EXPECT_EQ(f, v.front());
  allow_entry_t b = to_allow_entry("3.zip");
  EXPECT_EQ(b, v.back());
}

TEST_F(AllowTest, IsAllowed) {
  Allow a(config_);
  a.Load();
  EXPECT_TRUE(a.IsAllowed("1.zip"));
  a.Add("1.zip");
  EXPECT_FALSE(a.IsAllowed("1.zip"));
}

TEST_F(AllowTest, Remove) {
  Allow a(config_);
  a.Add("1.zip");
  a.Add("2.zip");
  a.Add("3.zip");
  EXPECT_FALSE(a.IsAllowed("2.zip"));
  a.Remove("2.zip");
  EXPECT_TRUE(a.IsAllowed("2.zip"));
}

TEST_F(AllowTest, Load) {
  const std::string contents("badworld.zip\0foobar2u.arc\0", 26);
  auto path = helper.files().CreateTempFilePath("bbs/data/allow.dat");
  {
    File f(path);
    ASSERT_TRUE(
        f.Open(File::modeBinary | File::modeCreateFile | File::modeTruncate | File::modeReadWrite));
    f.Write(contents);
  }
  Allow a(config_);
  ASSERT_TRUE(a.Load());
  EXPECT_EQ(2, a.allow_vector().size());
  EXPECT_EQ(to_allow_entry("badworld.zip"), a.allow_vector().front());
  EXPECT_FALSE(a.IsAllowed("foobar2u.arc"));
  EXPECT_TRUE(a.IsAllowed("foobar2u.zip"));
}

TEST_F(AllowTest, Save) {
  const std::string contents("badworld.zip\0foobar2u.arc\0", 26);
  auto path = helper.files().CreateTempFilePath("bbs/data/allow.dat");
  {
    File f(path);
    ASSERT_TRUE(
        f.Open(File::modeBinary | File::modeCreateFile | File::modeTruncate | File::modeReadWrite));
    f.Write(contents);
  }
  // Load default allow.data
  Allow a(config_);
  ASSERT_TRUE(a.Load());
  EXPECT_EQ(2, a.allow_vector().size());
  // Add a new one then save the reload it
  a.Add("1.zip");
  ASSERT_TRUE(a.Save());
  ASSERT_TRUE(a.Load());
  // Now we are using the reloaded version.
  EXPECT_EQ(3, a.allow_vector().size());
  EXPECT_FALSE(a.IsAllowed("1.zip"));
  EXPECT_TRUE(a.IsAllowed("2.zip"));
}
