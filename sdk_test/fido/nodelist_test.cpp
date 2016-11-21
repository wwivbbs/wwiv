/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2016, WWIV Software Services               */
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

#include <cstring>
#include <type_traits>

#include "core/stl.h"
#include "core/strings.h"
#include "sdk/fido/nodelist.h"

using std::cout;
using std::endl;
using std::is_standard_layout;
using std::string;

using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::sdk::fido;

static const char raw[] = R"(
;
Zone,1,North_America,Slaterville_Springs_NY,Sysop_Name1,1-607-555-1212,9600,CM,XX,H16,V32b,V42b,V34,V32T,X2C,INA:filegate.net,IBN,IBN:24555,IFT,ITN
,20,The_North_American_Backbone,Zone_1,Sysop_Name1_20,-Unpublished-,300,CM,MO,XX,INA:the-estar.com,IBN
;
Region,10,Calif-Nevada,Aptos_CA,Sysop_Name10,-Unpublished-,300,CM,XX,INA:realitycheckbbs.org,IBN,IFC
,1,Region_10_Echomail_Coordinator_Calif_Nevada,Aptos_CA,Sysop_Name10_1,-Unpublished-,300,CM,XX,INA:realitycheckbbs.org,IBN,IFC
;
Host,102,SoCalNet,Aptos_CA,Sysop_Name102,-Unpublished-,300,CM,XX,INA:realitycheckbbs.org,IBN,IFC
,501,iNK_tWO,Covina_CA,Sysop_Name102_501,-Unpublished-,300,CM,XA,INA:bbs.inktwo.com,IBN
Down,943,Mysteria,Sierra_Madre_CA,Sysop_Name102_943,1-626-555-51212,9600,CM,XA,H16,V32b,V42b,V34,VFC,INA:mysteria.com,IBN
;
Host,123,South_Carogawgabama_Net,Spartanburg_SC,Sysop_Name123,-Unpublished-,300,CM,MO,INA:the-estar.com,IBN
,5,Hard_Drive_Cafe,Montgomery_AL,Sysop_Name123_5,-Unpublished-,300,CM,INA:irex.hdcbbs.com,ibn
Pvt,789,The_Eastern_Star_Fidonet_Via_Newsreader,Spartanburg_SC,Sysop_Name123_789,-Unpublished-,300
;
;
Host,261,Maryland_Central_Net,NC,Sysop_Name261,-Unpublished-,300,CM,XX,INA:bbs.weather-station.org,IBN:24555
,1,Weather_Station_Hub,Bel_Air_MD,Sysop_Name261_1,-Unpublished-,300,CM,XX,INA:bbs.weather-station.org,IBN:24555
,1300,Weather_Station_BBS_(Mystic),Bel_Air_MD,Sysop_Name261_1300,-Unpublished-,300,CM,XX,INA:bbs.weather-station.org,IBN:24557
;
)";

TEST(NodelistTest, Basic) {
  NodelistEntry e{};
  bool ok = NodelistEntry::ParseDataLine(",1,system_name,location,sysop_name,phone_number,baud_rate,CM,INA:host:port", e);
  ASSERT_TRUE(ok);
  EXPECT_EQ(e.name_, "system name");
}

TEST(NodelistTest, Zone) {
  NodelistEntry e{};
  bool ok = NodelistEntry::ParseDataLine("Zone,1,system_name,location,sysop_name,phone_number,baud_rate,CM,INA:host:port", e);
  ASSERT_TRUE(ok);
  EXPECT_EQ(e.keyword_, NodelistKeyword::zone);
}

TEST(NodelistTest, Smoke) {
  std::vector<std::string> lines = SplitString(raw, "\n");

  Nodelist nl(lines);
  ASSERT_TRUE(nl);

  auto n1_261_1 = nl.entry(1, 261, 1);
  ASSERT_TRUE(n1_261_1 != nullptr);
  EXPECT_EQ("Weather Station Hub", n1_261_1->name_);
  EXPECT_TRUE(n1_261_1->binkp_);
  EXPECT_EQ("bbs.weather-station.org", n1_261_1->hostname_);
  EXPECT_EQ("bbs.weather-station.org", n1_261_1->binkp_hostname_);
  EXPECT_EQ(24555, n1_261_1->binkp_port_);

  EXPECT_FALSE(n1_261_1->vmodem_);
}

TEST(NodelistTest, ZoneNet) {
  std::vector<std::string> lines = SplitString(raw, "\n");
  Nodelist nl(lines);
  ASSERT_TRUE(nl);

  auto n261 = nl.entries(1, 261);
  EXPECT_EQ(FidoAddress("1:261/1"), n261.front().address_);
}

TEST(NodelistTest, Zones) {
  std::vector<std::string> lines = SplitString(raw, "\n");
  Nodelist nl(lines);
  ASSERT_TRUE(nl);

  auto zones = nl.zones();
  EXPECT_EQ(1, zones.size());
  EXPECT_EQ(1, zones.front()) << std::string(zones.begin(), zones.end());
}

TEST(NodelistTest, Nets) {
  std::vector<std::string> lines = SplitString(raw, "\n");
  Nodelist nl(lines);
  ASSERT_TRUE(nl);

  auto nets = nl.nets(1);
  std::vector<uint16_t>expected{102 ,123, 261};
  EXPECT_EQ(expected, nets);
}

TEST(NodelistTest, Nodes) {
  std::vector<std::string> lines = SplitString(raw, "\n");
  Nodelist nl(lines);
  ASSERT_TRUE(nl);

  auto nets = nl.nodes(1, 261);
  std::vector<uint16_t>expected{1, 1300};
  EXPECT_EQ(expected, nets);
}