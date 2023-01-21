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
#include "gtest/gtest.h"
#include "core/datetime.h"
#include "core/fake_clock.h"
#include "core/strings.h"
#include "core/test/file_helper.h"
#include "networkf/networkf.h"
#include "sdk/bbslist.h"
#include "sdk/sdk_helper.h"
#include "sdk/fido/fido_util.h"
#include "sdk/fido/test/ftn_directories_test_helper.h"
#include <gmock/gmock-matchers.h>

#include <chrono>
#include <string>

using namespace std::chrono;
using namespace testing;
using namespace wwiv::core;
using namespace wwiv::sdk::fido;

using namespace wwiv::strings;
using namespace wwiv::sdk::fido::test;
using namespace wwiv::net::networkf;

class NetworkFTest : public testing::Test {
public:
  NetworkFTest() {}

protected:
  wwiv::core::test::FileHelper file_helper_;
  SdkHelper helper;
};

TEST_F(NetworkFTest, Smoke) { 
  constexpr int max_backups = 2;
  constexpr bool skip_delete = false;
  networkf_options_t opts{max_backups, skip_delete, 'f', ""};
  opts.system_name = "test bbsname";

  net_system_list_rec netsys;
  netsys.sysnum = 32675;
  wwiv::sdk::BbsListNet bbslist({netsys});

  FakeClock clock(DateTime::now());

  auto net = helper.CreateTestNetwork(wwiv::sdk::net::network_type_t::ftn);
  NetworkF f(helper, opts, net, bbslist, clock);

  EXPECT_EQ(net.dir.string(), f.net().dir.string());
}


TEST_F(NetworkFTest, DaysOld_New) {
  // daten_to_fido
  FakeClock clock(DateTime::now());
  auto fd = daten_to_fido(clock.Now().to_time_t());

  EXPECT_THAT(ftn_date_days_old(clock, fd), Eq(0));
}

TEST_F(NetworkFTest, DaysOld_30) {
  // daten_to_fido
  FakeClock clock(DateTime::now());
  auto fd = daten_to_fido(clock.Now().to_time_t());
  clock.tick(hours(30 * 24));

  EXPECT_THAT(ftn_date_days_old(clock, fd), Eq(30));
}

TEST_F(NetworkFTest, DaysOld_Newer) {
  // daten_to_fido
  FakeClock packet_clock(DateTime::now());
  const auto now_clock = packet_clock;
  packet_clock.tick(hours(25));
  const auto fd = daten_to_fido(packet_clock.Now().to_time_t());

  EXPECT_THAT(ftn_date_days_old(now_clock, fd), Le(0));
}
