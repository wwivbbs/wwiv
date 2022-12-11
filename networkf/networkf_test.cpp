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
#include "sdk/fido/test/ftn_directories_test_helper.h"

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

  wwiv::core::FakeClock clock(wwiv::core::DateTime::now());

  auto net = helper.CreateTestNetwork(wwiv::sdk::net::network_type_t::ftn);
  wwiv::net::networkf::NetworkF f(helper, opts, net, bbslist, clock);

  EXPECT_EQ(net.dir.string(), f.net().dir.string());
}

