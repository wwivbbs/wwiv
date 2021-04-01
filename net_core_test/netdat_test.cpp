/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2020-2021, WWIV Software Services             */
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
#include "core/fake_clock.h"
#include "core/log.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "net_core/netdat.h"
#include "sdk/net/net.h"

#include "gtest/gtest.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;


class NetDatTest: public testing::Test {
public:
  NetDatTest() {
    CHECK(helper_.Mkdir("gfiles"));
    CHECK(helper_.Mkdir("logs"));
    gfiles_ = helper_.Dir("gfiles");
    logs_ = helper_.Dir("logs");

    // default date
    t_.tm_mon = 9; // 10
    t_.tm_mday = 25;
    t_.tm_year = 120;

    // default network.
    net.sysnum = 1;
    net.type = network_type_t::wwivnet;
    net.name = "testnet";
  }

protected:
  FileHelper helper_;
  std::filesystem::path gfiles_;
  std::filesystem::path logs_;
  tm t_{};
  Network net{};
};

TEST_F(NetDatTest, Smoke) {
  EXPECT_FALSE(File::Exists(FilePath(gfiles_, "netdat0.log")));

  FakeClock clock(DateTime::from_tm(&t_));
  wwiv::net::NetDat n(gfiles_, logs_, net, 'z', clock);

  EXPECT_TRUE(File::Exists(FilePath(gfiles_, "netdat0.log")));
}

TEST_F(NetDatTest, Rollover) {
  EXPECT_FALSE(File::Exists(FilePath(gfiles_, "netdat0.log")));

  FakeClock clock(DateTime::from_tm(&t_));
  {
    wwiv::net::NetDat n(gfiles_, logs_, net, 'z', clock);
  }

  EXPECT_TRUE(File::Exists(FilePath(gfiles_, "netdat0.log")));
  EXPECT_FALSE(File::Exists(FilePath(gfiles_, "netdat1.log")));

  clock.tick(std::chrono::hours(24));
  {
    wwiv::net::NetDat n(gfiles_, logs_, net, 'z', clock);
  }
  EXPECT_TRUE(File::Exists(FilePath(gfiles_, "netdat0.log")));
  EXPECT_TRUE(File::Exists(FilePath(gfiles_, "netdat1.log")));
}
