/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*              Copyright (C)2017-2020, WWIV Software Services            */
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
#include "core/datetime.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/net/packets.h"
#include "gtest/gtest.h"

#include <cstdint>
#include <string>

using std::endl;
using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

static string CreateFakePacketText(const string& subtype, const string& title, const string& sender,
                                   const string& date, const string& text) {

  string result;
  result.append(subtype);
  result.push_back(0);
  result.append(title);
  result.push_back(0);
  result.append(sender).append("\r\n");
  result.append(date).append("\r\n");
  result.append(text);
  result.push_back(26);

  return result;
}

class PacketsTest : public testing::Test {
public:
  PacketsTest() {}

protected:
  FileHelper helper_;
};

TEST_F(PacketsTest, GetNetInfoFileInfo_Smoke) {
  string text;
  text.push_back(1);
  text.push_back(0);
  text += "BINKP";
  text.push_back(0);
  text += "Hello World";
  ASSERT_EQ(19, text.size());

  net_header_rec nh{};
  nh.daten = daten_t_now();
  nh.method = 0;
  nh.main_type = main_type_file;
  nh.minor_type = net_info_file;
  Packet p(nh, {}, text);

  auto info = GetNetInfoFileInfo(p);
  ASSERT_TRUE(info.valid);
  EXPECT_EQ("binkp.net", info.filename);
  EXPECT_TRUE(info.overwrite);
  EXPECT_EQ("Hello World", info.data);
}

TEST_F(PacketsTest, UpdateRouting_Smoke) {
  string body = "Hello World";
  auto now = daten_t_now();
  auto date = daten_to_wwivnet_time(now);

  auto packet_text = CreateFakePacketText("MYSUB", "This is a title", "Sysop #1", date, body);
  auto orig = packet_text;

  net_header_rec nh{};
  nh.daten = now;
  nh.fromsys = 1;
  nh.tosys = 2;
  nh.fromuser = 1;
  nh.touser = 1;
  nh.length = packet_text.size();
  nh.main_type = main_type_new_post;
  nh.method = 0;
  nh.list_len = 0;
  Packet packet(nh, {}, packet_text);

  net_networks_rec net{};
  net.dir = "Z:\\";
  to_char_array(net.name, "My Network");
  net.sysnum = 2;
  net.type = network_type_t::wwivnet;
  packet.UpdateRouting(net);

  auto iter = packet.text().begin();
  get_message_field(packet.text(), iter, {'\0', '\r', '\n'}, 80);
  get_message_field(packet.text(), iter, {'\0', '\r', '\n'}, 80);
  get_message_field(packet.text(), iter, {'\0', '\r', '\n'}, 80);
  get_message_field(packet.text(), iter, {'\0', '\r', '\n'}, 80);
  auto actual_route_str = get_message_field(packet.text(), iter, {'\0', '\r', '\n'}, 80);

  EXPECT_TRUE(starts_with(actual_route_str, "\004"
                                            "0R"));
}

TEST_F(PacketsTest, GetMessageField) {
  char raw[] = {'a', '\0', 'b', 'c', '\r', '\n', 'd'};
  string text(raw, 7);
  auto iter = text.begin();
  string a = get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
  EXPECT_STREQ("a", a.c_str());
  string bc = get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
  EXPECT_STREQ("bc", bc.c_str());
  string remaining = string(iter, text.end());
  EXPECT_STREQ("d", remaining.c_str());
}

TEST_F(PacketsTest, FromPacketText_FromPacketText_NewPost) {
  const string s("a\000b\000c\r\nd\r\ne", 11);
  auto pp = ParsedPacketText::FromPacketText(main_type_new_post, s);
  EXPECT_EQ(pp.subtype(), "a");
  EXPECT_EQ(pp.title(), "b");
  EXPECT_EQ(pp.sender(), "c");
  EXPECT_EQ(pp.date(), "d");
  EXPECT_EQ(pp.text(), "e");
}


TEST_F(PacketsTest, FromPacketText_ToPacketText_Email_NotName) {
  const string expected("b\000c\r\nd\r\ne", 9);
  ParsedPacketText ppt{main_type_email};
  ppt.set_to("a");
  ppt.set_title("b");
  ppt.set_sender("c");
  ppt.set_date("d");
  ppt.set_text("e");

  auto actual = ParsedPacketText::ToPacketText(ppt);
  EXPECT_EQ(expected, actual);
}

TEST_F(PacketsTest, FromPacketText_ToPacketText_EmailName) {
  const string expected("a\000b\000c\r\nd\r\ne", 11);
  ParsedPacketText ppt{main_type_email_name};
  ppt.set_to("a");
  ppt.set_title("b");
  ppt.set_sender("c");
  ppt.set_date("d");
  ppt.set_text("e");

  auto actual = ParsedPacketText::ToPacketText(ppt);
  EXPECT_EQ(expected, actual);
}

TEST_F(PacketsTest, FromPacketText_Malformed) {
  const string s("a", 1);
  auto pp = ParsedPacketText::FromPacketText(main_type_new_post, s);
  EXPECT_EQ(pp.subtype(), "a");
  EXPECT_EQ(pp.title(), "");
  EXPECT_EQ(pp.sender(), "");
  EXPECT_EQ(pp.date(), "");
}
