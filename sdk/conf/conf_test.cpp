/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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
/**************************************************************************/
#include "core/test/wwivtest.h"
#include "gtest/gtest.h"
#include "core/fake_clock.h"
#include "core/strings.h"
#include "core/test/file_helper.h"
#include "networkf/networkf.h"
#include "sdk/bbslist.h"
#include "sdk/sdk_helper.h"
#include "sdk/files/dirs.h"
#include "sdk/subxtr.h"
#include "sdk/conf/conf.h"
#include "gmock/gmock-matchers.h"

using namespace testing;
using namespace wwiv::core;
using namespace wwiv::core::test;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using namespace wwiv::strings;

class ConfTest : public testing::Test {
public:
  ConfTest() {}

protected:
  wwiv::core::test::FileHelper file_helper_;
  SdkHelper helper;
};


TEST_F(ConfTest, Smoke) {
  const auto path = FilePath(FileHelper::TestData(), "fido/0d73f767.pkt");
  auto f = File(path);
  ASSERT_TRUE(f.Exists()) << f;

  auto testdata = FilePath(FileHelper::TestData(), "conf");
  helper.config().datadir(testdata.string());
  helper.config().raw_config().datadir = testdata.string();
  helper.config().update_paths();
  std::vector<net::Network> net_networks;
  Subs subs(testdata.string(), net_networks);
  files::Dirs dirs(testdata.string(), 0);
  dirs.LoadLegacy();

  auto result = UpgradeConferences(helper.config(), subs, dirs);
  //conference_file_t UpgradeConferences(const Config & config, Subs & subs, files::Dirs & dirs);

}