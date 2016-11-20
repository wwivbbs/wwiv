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
Zone,1,North_America,Slaterville_Springs_NY,Janis_Kracht,1-607-200-
;
Host,123,South_Carogawgabama_Net,Spartanburg_SC,Ross_Cassell,-Unpublished-,300,CM,MO,INA:the-estar.com,IBN
,5,Hard_Drive_Cafe,Montgomery_AL,Jason_Cullins,-Unpublished-,300,CM,INA:irex.hdcbbs.com,ibn
,10,Archaic_Binary_BBS,Orlando_FL,Wayne_Smith,-Unpublished-,300,CM,INA:bbs.archaicbinary.net,IBN
Pvt,20,tnel_software_bbs,Landrum_SC,Robert_Lent,-Unpublished-,300,MO
,24,Quantum_BBS,Port_Richey_Fl,Matthew_Ionson,-Unpublished-,300,CM,INA:qlbbs.mooo.com,IBN,IFT,ITN,IFC
Pvt,25,Vintage_Computer_BBS,Columbia_SC,Kurt_Hamm,-Unpublished-,300,MO
,52,Killed_in_Action_BBS,Valdosta_Ga,Chip_Hearn,-Unpublished-,300,CM,XR,INA:kia.zapto.org,ITN,IBN
,57,Eye_Of_The_Hurricane_BBS,St_Petersburg_Fl,Kris_Jones,-Unpublished-,300,CM,INA:binkp.eothnet.com,IBN,IFT,ITN
,90,Aftershock,Holiday_FL,Peter_Schroeder,-Unpublished-,300,CM,INA:aftershock.servebeer.com,IBN,ITN
,100,armed.gamilitia.us,Manchester_GA,Hugo_Stroud,-Unpublished-,300,INA:armed.gamilitia.us,IBN,IFT,ITN
,111,Sinner's_Haven,Dothan_Al,Karl_Kunc,-Unpublished-,300,CM,XA,INA:fido.sinnershaven.com,IBN
,130,After_Hours_BBS,Lady_Lake_FL,Greg_Youngblood,-Unpublished-,300,INA:afterhours-bbs.com,IBN
,140,Docsplace_BBS,Clearwater_FL,Ed_Koon,1-727-726-3214,33600,V34,INA:bbs.docsplace.org,IBN
,160,KidsToGoEnglish,ST_Petersburg_Fl,Thomas_Huff,-Unpublished-,300,CM,INA:english.office-on-the.net,IBN
,170,Techinvasion_BBS,Miami_Fl,John_Burns,-Unpublished-,300,CM,INA:bbs.techinvasion.net,IBN,IFT,ITN,IEM
,205,The_Final_Frontier,Birmingham_AL,John_Tipton,-Unpublished-,300,INA:tffbbs.com,IBN,ITN
,228,Catalogue_Services,Orlando_FL,Chad_Michael_Eyer,-Unpublished-,300,INA:athena.eyer.us,IBN
,256,NARF_Live!_BBS,Huntsville_AL,Dallas_Vinson,-Unpublished-,300,CM,INA:bbs.furmen.org,IBN,ITN,IFC
,300,Monterey_BBS,Savannah_GA,Ryan_Fantus,-Unpublished-,300,CM,INA:montereybbs.ath.cx,IBN,ITN
,400,Orbit_BBS,Opp_AL,Allen_Scofield,-Unpublished-,300,CM,INA:orbitbbs.com,IBN,ITN
Pvt,450,Castle_Camelot,Tuscumbia_Al,Donald_Tidmore,-Unpublished-,300,MO
,500,The_Eastern_Star_Mailhub,Spartanburg_SC,Ross_Cassell,-Unpublished-,300,CM,MO,INA:the-estar.com,IBN
,525,Alcholiday,Columbia_SC,Andrew_Haworth,-Unpublished-,300,CM,INA:alco.bbs.io,IBN,ITN
,600,Masturbation_Station,Miami_Beach_FL,Jorge_Rodriguez,-Unpublished-,300,CM,INA:bbs.masturbationstationbbs.com,IBN,IFT,ITN
,518,FLaSHBaCK-BBS,Daytona_Beach_Fl,Philip_Lozier,-Unpublished-,300,CM,INA:flashback-bbs.zapto.org,IBN
Pvt,789,The_Eastern_Star_Fidonet_Via_Newsreader,Spartanburg_SC,Ross_Cassell,-Unpublished-,300
,1406,The_Shack_And_More_BBS,Jefferson_SC,Nicholas_Kill,-Unpublished-,300,CM,INA:96.45.102.248,IBN,IFT,ITN
,1970,Mid_South_BBS,Amory_MS,Kent_Stanford,-Unpublished-,300,CM,INA:bbs.midsouth.com,IBN,ITN:2323
,6502,Lightning_Strikes_Here_II,Orlando_Fl,Austin_Phelps,-Unpublished-,300,CM,INA:71.41.126.198,IBN
;
;
Host,261,Maryland_Central_Net,NC,Mark_Hofmann,-Unpublished-,300,CM,XX,INA:bbs.weather-station.org,IBN:24555
,1,Weather_Station_Hub,Bel_Air_MD,Mark_Hofmann,-Unpublished-,300,CM,XX,INA:bbs.weather-station.org,IBN:24555
,20,Omicron_Theta,Memphis_TN,Robert_Wolfe,-Unpublished-,9600,CM,INA:robertwolfe.org,IBN,IFT
,38,<<PRISM_BBS,Slaterville_Springs_NY,Janis_Kracht,1-607-200-4076,9600,CM,XX,H16,V32b,V42b,V34,V32t,X2C,INA:filegate.net,IBN
,100,<<PRISM_BBS,Slaterville_Springs_NY,Janis_Kracht,-Unpublished-,300,CM,INA:filegate.net,IBN:24555,IFT,ITN
,220,The_Realms_of_Blue,Westminster_MD,Scott_Brown,-Unpublished-,300,CM,INA:blues.zapto.org,IBN
,1300,Weather_Station_BBS_(Mystic),Bel_Air_MD,Mark_Hofmann,-Unpublished-,300,CM,XX,INA:bbs.weather-station.org,IBN:24557
,1304,Weather_Station_BBS,Bel_Air_MD,Mark_Hofmann,-Unpublished-,300,CM,XX,INA:bbs.weather-station.org,IBN
,1305,Technoquarry,Towson_MD,Patrick_McGuire,-Unpublished-,300,CM,XX,INA:technoquarry.com,IBN
Pvt,1466,Owls_Anchor,Columbia_MD,Dale_Shipp,-Unpublished-,9600,XX,MO
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
  ASSERT_TRUE(nl.entries().size() >= 8);

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
  EXPECT_EQ(2, nets.size());
  std::vector<int16_t>expected{123, 261};
  EXPECT_EQ(expected, nets);
}

TEST(NodelistTest, Nodes) {
  std::vector<std::string> lines = SplitString(raw, "\n");
  Nodelist nl(lines);
  ASSERT_TRUE(nl);

  auto nets = nl.nodes(1, 261);
  EXPECT_EQ(8, nets.size());
  std::vector<int16_t>expected{1, 20, 38, 100, 220, 1300, 1304, 1305};
  EXPECT_EQ(expected, nets);
}