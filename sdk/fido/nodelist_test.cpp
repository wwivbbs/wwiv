/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2016-2022, WWIV Software Services             */
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
#include "gmock/gmock.h"

#include "core/stl.h"
#include "core/strings.h"
#include "sdk/fido/nodelist.h"
#include <type_traits>

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
;
Host,109,National_Capital_Area_Net_109,Washington_DC,Frank_Reid,1-757-481-6611,9600,CM,XA,V32b,V42b,V34,IBN:bbsdoors.com
,41,Internet_Email_Gateway,Wheaton_MD,Andrew_Miller,1-301-949-5764,9600,CM,XR,V32b,V42
,42,Usenet_News_Gateway,Wheaton_MD,Andrew_Miller,1-301-949-5764,9600,CM,XR,V32b,V42
,201,bbs.cyberchatnet.com,Vienna_VA,Phillip_Taylor,-Unpublished-,9600,CM,INA:bbs.cyberchatnet.com,IBN,IFC
,202,bbs.cyberchatnet.com,Vienna_VA,Phillip_Taylor,1-703-310-7439,9600,XR,V32b,V42
Pvt,264,www.TheLitterBox.org,Bluemont_VA,Tony_Campbell,-Unpublished-,300,IP
,432,The_Idea_Link,Wheaton_MD,Andrew_Miller,1-301-949-5764,9600,CM,XR,V32b,V42
Hub,500,Greater_PG_Co_Lcl_Hub,Laurel,Frank_Reid,1-757-481-6611,9600,CM,XA,V32b,V42b,V34
Pvt,560,The_Play_Pen,Upper_Marlboro_MD,Jeffrey_Estevez,-Unpublished-,300,IP
,567,www.bbsdoors.com,Laurel_MD,Frank_Reid,-Unpublished-,300,CM,IBN:bbsdoors.com
,568,Eagle's_Dare,Upper_Marlboro_MD,Frank_Reid,1-757-481-6611,9600,CM,XA,V32b,V42b,V34
,909,Lincoln_Legacy,Leesburg_VA,John_Covici,1-703-777-8383,9600,CM,HST,V42B,V32B
;
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
;
;
Zone,42,Mars,Slaterville_Springs_NY,Sysop_Name1,1-607-555-1212,9600,CM,XX,H16,V32b,V42b,V34,V32T,X2C,INA:filegate.net,IBN,IBN:24555,IFT,ITN
,20,The_Martian_Backbone,Zone_1,Sysop_Name1_20,-Unpublished-,300,CM,MO,XX,INA:the-estar.com,IBN
;
Region,21,Mars_Station,Station_1,Sysop_Name10,-Unpublished-,300,CM,XX,INA:elonsbbs.example.org,IBN,IFC
,1,Region_10_Echomail_Coordinator_Mars_Station,Mars_Station,Sysop_Name10_1,-Unpublished-,300,CM,XX,INA:elonsbbs.example.org,IBN,IFC
;
Host,123,Elons_BBS,Mars_Station,Sysop_Name123,-Unpublished-,300,CM,MO,INA:elonsbbs.example.org,IBN
;
;
)";

TEST(NodelistTest, Basic) {
  const auto e = NodelistEntry::ParseDataLine(
      ",1,system_name,location,sysop_name,phone_number,baud_rate,CM,INA:host:port");
  ASSERT_TRUE(e);
  EXPECT_EQ(e->name(), "system name");
}

TEST(NodelistTest, Zone) {
  const auto e = NodelistEntry::ParseDataLine(
      "Zone,1,system_name,location,sysop_name,phone_number,baud_rate,CM,INA:host:port");
  ASSERT_TRUE(e);
  EXPECT_EQ(e->keyword(), NodelistKeyword::zone);
}

TEST(NodelistTest, Smoke) {
  const auto lines = SplitString(raw, "\n");
  Nodelist nl(lines, "");
  ASSERT_TRUE(nl);

  const auto n1_261_1 = nl.entry(1, 261, 1);
  ASSERT_TRUE(n1_261_1 != nullptr);
  EXPECT_EQ("Weather Station Hub", n1_261_1->name());
  EXPECT_EQ("Sysop Name261 1", n1_261_1->sysop_name());
  EXPECT_EQ("Bel Air MD", n1_261_1->location());
  EXPECT_TRUE(n1_261_1->binkp());
  EXPECT_EQ("bbs.weather-station.org", n1_261_1->hostname());
  EXPECT_EQ("bbs.weather-station.org", n1_261_1->binkp_hostname());
  EXPECT_EQ(24555u, n1_261_1->binkp_port());
  EXPECT_FALSE(n1_261_1->vmodem());
}

TEST(NodelistTest, ZoneNet) {
  const auto lines = SplitString(raw, "\n");
  const Nodelist nl(lines, "");
  ASSERT_TRUE(nl);

  auto n261 = nl.entries(1, 261);
  EXPECT_EQ(FidoAddress("1:261/1"), n261.front().address());
}

TEST(NodelistTest, Zones) {
  const auto lines = SplitString(raw, "\n");
  const Nodelist nl(lines, "");
  ASSERT_TRUE(nl);

  const auto zones = nl.zones();
  EXPECT_EQ(2u, zones.size());
  EXPECT_THAT(zones, testing::ElementsAre(static_cast<uint16_t>(1), static_cast<uint16_t>(42)));
}

TEST(NodelistTest, Has_Zone) {
  const auto lines = SplitString(raw, "\n");
  const Nodelist nl(lines, "");
  ASSERT_TRUE(nl);

  auto zones = nl.zones();
  EXPECT_TRUE(nl.has_zone(1));
  EXPECT_TRUE(nl.has_zone(42));
  EXPECT_FALSE(nl.has_zone(8));
}


TEST(NodelistTest, Nets) {
  const auto lines = SplitString(raw, "\n");
  const Nodelist nl(lines, "");
  ASSERT_TRUE(nl);

  const auto nets = nl.nets(1);
  const std::vector<uint16_t>expected{10, 102 ,109, 123, 261};
  EXPECT_EQ(expected, nets);
}

TEST(NodelistTest, Nodes_With_Hub_109) {
  const auto lines = SplitString(raw, "\n");
  const Nodelist nl(lines, "");
  ASSERT_TRUE(nl);

  const auto nets = nl.nodes(1, 109);
  // 500 is the hub entry in net 109
  EXPECT_THAT(nets, testing::Contains(500));
}

TEST(NodelistTest, Nodes_261) {
  const auto lines = SplitString(raw, "\n");
  const Nodelist nl(lines, "");
  ASSERT_TRUE(nl);

  const auto nets = nl.nodes(1, 261);
  EXPECT_THAT(nets, testing::ElementsAre(1, 1300));
}