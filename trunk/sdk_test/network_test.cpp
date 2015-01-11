/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*           Copyright (C)2014-2015 WWIV Software Services                */
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

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "core/file.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/networks.h"
#include "sdk_test/sdk_helper.h"

using namespace std;

using namespace wwiv::sdk;
using namespace wwiv::strings;

class NetworkTest : public testing::Test {
public:
  NetworkTest() : config(helper.root()) {
    EXPECT_TRUE(config.IsInitialized());
    EXPECT_TRUE(CreateNetworksDat(config, { "one", "two" }));
    networks.reset(new Networks(config));
    EXPECT_TRUE(networks->IsInitialized());
  }

  virtual void SetUp() {
    config.IsInitialized();
  }

  bool CreateNetworksDat(const Config& config, vector<string> names) {
    File file(config.datadir(), NETWORKS_DAT);
    file.Open(File::modeBinary|File::modeWriteOnly|File::modeCreateFile, File::shareDenyNone);
    if (!file.IsOpen()) {
      return false;
    }
  
    uint16_t sysnum = 1;
    for (const auto& name : names) {
      const string dir = StrCat(config.root_directory(), File::pathSeparatorString, name);
      net_networks_rec rec{};
      strcpy(rec.name, name.c_str());
      strcpy(rec.dir, dir.c_str());
      rec.sysnum = sysnum++;
      file.Write(&rec, sizeof(net_networks_rec));
    }

    return true;
  }

  Networks& test_networks() const { return *networks.get(); }

  SdkHelper helper;
  Config config;
  unique_ptr<Networks> networks;
};

TEST_F(NetworkTest, Networks_At) {
  const Networks& networks = test_networks();

  EXPECT_STREQ("two", networks.at(1).name);
  EXPECT_STREQ("two", networks.at("two").name);
}

TEST_F(NetworkTest, Networks_Bracket) {
  const Networks& networks = test_networks();

  EXPECT_STREQ("two", networks[1].name);
  EXPECT_STREQ("two", networks["two"].name);
}

TEST_F(NetworkTest, Networks_Dir) {
  const Networks& networks = test_networks();

  const string expected_two_dir = StrCat(config.root_directory(), File::pathSeparatorString, "two");
  EXPECT_STREQ(expected_two_dir.c_str(), networks.at(1).dir);
  EXPECT_STREQ(expected_two_dir.c_str(), networks.at("two").dir);
  EXPECT_STREQ(expected_two_dir.c_str(), networks[1].dir);
  EXPECT_STREQ(expected_two_dir.c_str(), networks["two"].dir);
}

TEST_F(NetworkTest, Networks_NetworkNumber) {
  const Networks& networks = test_networks();

  EXPECT_EQ(0, networks.network_number("one"));
  EXPECT_EQ(1, networks.network_number("two"));
  EXPECT_EQ(-1, networks.network_number("not-here"));
}

TEST_F(NetworkTest, Networks_Contains) {
  const Networks& networks = test_networks();

  EXPECT_TRUE(networks.contains("one"));
  EXPECT_FALSE(networks.contains("foo"));
}
