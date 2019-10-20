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
#include "sdk/net.h"
#include "sdk/networks.h"
#include "sdk_test/sdk_helper.h"

using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

class NetworkTest : public testing::Test {
public:
  NetworkTest() : config_(helper.root()) {
    EXPECT_TRUE(config_.IsInitialized());
    EXPECT_TRUE(CreateNetworksDat({ "one", "two" }));
    networks.reset(new Networks(config_));
    EXPECT_TRUE(networks->IsInitialized());
  }

  virtual void SetUp() {
    config_.IsInitialized();
  }

  bool CreateNetworksDat(std::vector<std::string> names) {
    std::clog << "Writing NETWORK.DAT to: " << config_.datadir() << std::endl;
    File file(PathFilePath(config_.datadir(), NETWORKS_DAT));
    file.Open(File::modeBinary|File::modeWriteOnly|File::modeCreateFile, File::shareDenyNone);
    if (!file.IsOpen()) {
      return false;
    }
  
    uint16_t sysnum = 1;
    for (const auto& name : names) {
      const auto dir = name;
      net_networks_rec_disk rec{};
      to_char_array(rec.name, name);
      to_char_array(rec.dir, dir);
      rec.sysnum = sysnum++;
      file.Write(&rec, sizeof(net_networks_rec_disk));
    }

    return true;
  }

  Networks& test_networks() const { return *networks.get(); }

  SdkHelper helper;
  Config config_;
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

  const std::string expected_two_dir = "two";
  EXPECT_EQ(expected_two_dir, networks.at(1).dir);
  EXPECT_EQ(expected_two_dir, networks.at("two").dir);
  EXPECT_EQ(expected_two_dir, networks[1].dir);
  EXPECT_EQ(expected_two_dir, networks["two"].dir);
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
