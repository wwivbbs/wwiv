/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*              Copyright (C)2017-2021, WWIV Software Services            */
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
#include "core/stl.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/net/packets.h"
#include "gtest/gtest.h"
#include <string>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

static std::string CreateFakePacketText(const std::string& subtype, const std::string& title, const std::string& sender,
                                   const std::string& date, const std::string& text) {

  std::string result;
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
  PacketsTest() = default;

protected:
  FileHelper helper_;
};

TEST_F(PacketsTest, GetNetInfoFileInfo_Smoke) {
  std::string text;
  text.push_back(1);
  text.push_back(0);
  text += "BINKP";
  text.push_back(0);
  text += "Hello World";
  ASSERT_EQ(19, wwiv::stl::ssize(text));

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
  const std::string body = "Hello World";
  const auto now = daten_t_now();
  const auto date = daten_to_wwivnet_time(now);

  const auto packet_text = CreateFakePacketText("MYSUB", "This is a title", "Sysop #1", date, body);

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

  Network net{};
  net.dir = "Z:\\";
  net.name = "My Network";
  net.sysnum = 2;
  net.type = network_type_t::wwivnet;
  packet.UpdateRouting(net);

  auto iter = packet.text().begin();
  get_message_field(packet.text(), iter, {'\0', '\r', '\n'}, 80);
  get_message_field(packet.text(), iter, {'\0', '\r', '\n'}, 80);
  get_message_field(packet.text(), iter, {'\0', '\r', '\n'}, 80);
  get_message_field(packet.text(), iter, {'\0', '\r', '\n'}, 80);
  const auto actual_route_str = get_message_field(packet.text(), iter, {'\0', '\r', '\n'}, 80);

  EXPECT_TRUE(starts_with(actual_route_str, "\004"
                                            "0R"));
}

TEST_F(PacketsTest, GetMessageField) {
  char raw[] = {'a', '\0', 'b', 'c', '\r', '\n', 'd'};
  std::string text(raw, 7);
  auto iter = text.begin();
  const auto a = get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
  EXPECT_STREQ("a", a.c_str());
  const auto bc = get_message_field(text, iter, {'\0', '\r', '\n'}, 80);
  EXPECT_STREQ("bc", bc.c_str());
  const auto remaining = std::string(iter, text.end());
  EXPECT_STREQ("d", remaining.c_str());
}

TEST_F(PacketsTest, FromPacketText_FromPacketText_NewPost) {
  const std::string s("a\000b\000c\r\nd\r\ne", 11);
  const auto pp = ParsedPacketText::FromPacketText(main_type_new_post, s);
  EXPECT_EQ(pp.subtype(), "a");
  EXPECT_EQ(pp.title(), "b");
  EXPECT_EQ(pp.sender(), "c");
  EXPECT_EQ(pp.date(), "d");
  EXPECT_EQ(pp.text(), "e");
}


TEST_F(PacketsTest, FromPacketText_ToPacketText_Email_NotName) {
  const std::string expected("b\000c\r\nd\r\ne", 9);
  ParsedPacketText ppt{main_type_email};
  ppt.set_to("a");
  ppt.set_title("b");
  ppt.set_sender("c");
  ppt.set_date("d");
  ppt.set_text("e");

  const auto actual = ParsedPacketText::ToPacketText(ppt);
  EXPECT_EQ(expected, actual);
}

TEST_F(PacketsTest, FromPacketText_ToPacketText_EmailName) {
  const std::string expected("a\000b\000c\r\nd\r\ne", 11);
  ParsedPacketText ppt{main_type_email_name};
  ppt.set_to("a");
  ppt.set_title("b");
  ppt.set_sender("c");
  ppt.set_date("d");
  ppt.set_text("e");

  const auto actual = ParsedPacketText::ToPacketText(ppt);
  EXPECT_EQ(expected, actual);
}

TEST_F(PacketsTest, FromPacketText_Malformed) {
  const std::string s("a", 1);
  const auto pp = ParsedPacketText::FromPacketText(main_type_new_post, s);
  EXPECT_EQ(pp.subtype(), "a");
  EXPECT_EQ(pp.title(), "");
  EXPECT_EQ(pp.sender(), "");
  EXPECT_EQ(pp.date(), "");
}
