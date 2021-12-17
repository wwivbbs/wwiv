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
#include "gtest/gtest.h"

#include "core/file.h"
#include "core/strings.h"
#include "core/file_helper.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/net/net.h"
#include "sdk/net/networks.h"
#include "sdk/sdk_helper.h"
#include <cstdint>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

class NetworkTest : public testing::Test {
public:
  NetworkTest() : config_(helper.root()) {
    EXPECT_TRUE(config_.IsInitialized());
    EXPECT_TRUE(CreateNetworksDat({ "one", "two" }));
    networks_.reset(new Networks(config_));
    EXPECT_TRUE(networks_->IsInitialized());
  }

  void SetUp() override {
  }

  [[nodiscard]] bool CreateNetworksDat(std::vector<std::string> names) const {
    std::clog << "Writing NETWORK.DAT to: " << config_.datadir() << std::endl;
    File file(FilePath(config_.datadir(), NETWORKS_DAT));
    file.Open(File::modeBinary|File::modeWriteOnly|File::modeCreateFile, File::shareDenyNone);
    if (!file.IsOpen()) {
      return false;
    }
  
    uint16_t sysnum = 1;
    for (const auto& name : names) {
      const auto& dir = name;
      net_networks_rec_disk rec{};
      to_char_array(rec.name, name);
      to_char_array(rec.dir, dir);
      rec.sysnum = sysnum++;
      file.Write(&rec, sizeof(net_networks_rec_disk));
    }

    return true;
  }

  [[nodiscard]] Networks& test_networks() const { return *networks_; }

  SdkHelper helper;
  Config config_;
  unique_ptr<Networks> networks_;
};

TEST_F(NetworkTest, Networks_At) {
  const auto& n = test_networks();

  EXPECT_EQ("two", n.at(1).name);
  EXPECT_EQ("two", n.at("two").name);
}

TEST_F(NetworkTest, Networks_Bracket) {
  const auto& n = test_networks();

  EXPECT_EQ("two", n[1].name);
  EXPECT_EQ("two", n["two"].name);
}

TEST_F(NetworkTest, Networks_Dir) {
  const auto& n = test_networks();

  const std::string expected_two_dir = "two";
  EXPECT_EQ(expected_two_dir, n.at(1).dir);
  EXPECT_EQ(expected_two_dir, n.at("two").dir);
  EXPECT_EQ(expected_two_dir, n[1].dir);
  EXPECT_EQ(expected_two_dir, n["two"].dir);
}

TEST_F(NetworkTest, Networks_NetworkNumber) {
  const auto& n = test_networks();

  EXPECT_EQ(0, n.network_number("one"));
  EXPECT_EQ(1, n.network_number("two"));
  EXPECT_EQ(-1, n.network_number("not-here"));
}

TEST_F(NetworkTest, Networks_Contains) {
  const auto& n = test_networks();

  EXPECT_TRUE(n.contains("one"));
  EXPECT_FALSE(n.contains("foo"));
}
