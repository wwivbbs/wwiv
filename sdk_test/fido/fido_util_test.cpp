/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2021, WWIV Software Services              */
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
#include "core/log.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core_test/file_helper.h"
#include "fmt/printf.h"
#include "sdk/fido/fido_address.h"
#include "sdk/fido/fido_util.h"
#include "gtest/gtest.h"
#include <ctime>
#include <memory>
#include <string>

using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk::fido;
using namespace wwiv::sdk::net;

class FidoUtilTest : public testing::Test {
public:
  FidoUtilTest() = default;

protected:
  FileHelper helper_;
  wwiv_to_fido_options opts;

};

TEST_F(FidoUtilTest, PacketName) {
  auto dt = DateTime::now();
  const auto tm = dt.to_tm();
  const auto actual = packet_name(dt);

  const auto expected =
      fmt::sprintf("%2.2d%2.2d%2.2d%2.2d.pkt", tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  EXPECT_EQ(expected, actual);
}

TEST_F(FidoUtilTest, BundleName_Extension) {
  const FidoAddress source("1:105/6");
  const FidoAddress dest("1:105/42");
  const auto actual = bundle_name(source, dest, "su0");

  EXPECT_EQ("0000ffdc.su0", actual);
}

TEST_F(FidoUtilTest, BundleName_NoExtension) {
  const FidoAddress source("1:105/6");
  const FidoAddress dest("1:105/42");
  const auto actual = bundle_name(source, dest, 1, 3);

  EXPECT_EQ("0000ffdc.mo3", actual);
}

TEST_F(FidoUtilTest, DowExtension) {
  EXPECT_EQ("mo0", dow_extension(1, 0));
  EXPECT_EQ("sa9", dow_extension(6, 9));
  EXPECT_EQ("sua", dow_extension(0, 10));
  EXPECT_EQ("suz", dow_extension(0, 35));
}

TEST_F(FidoUtilTest, IsBundleFile) {
  EXPECT_TRUE(is_bundle_file("00000000.su0"));
  EXPECT_FALSE(is_bundle_file("00000000.foo"));
}

TEST_F(FidoUtilTest, ControlFileName) {
  const FidoAddress dest("1:105/42");
  EXPECT_EQ("0069002a.flo", control_file_name(dest, fido_bundle_status_t::normal));
  EXPECT_EQ("0069002a.clo", control_file_name(dest, fido_bundle_status_t::crash));
  EXPECT_EQ("0069002a.dlo", control_file_name(dest, fido_bundle_status_t::direct));
  EXPECT_EQ("0069002a.hlo", control_file_name(dest, fido_bundle_status_t::hold));
}

TEST_F(FidoUtilTest, ToNetNode) {
  EXPECT_EQ("105/42", to_net_node(FidoAddress("1:105/42")));
  EXPECT_EQ("105/42", to_net_node(FidoAddress("105/42")));
}

TEST_F(FidoUtilTest, ToNetNodeZone) {
  EXPECT_EQ("2:105/42", to_zone_net_node(FidoAddress("2:105/42")));
  EXPECT_EQ("1:105/42", to_zone_net_node(FidoAddress("105/42")));
}

TEST_F(FidoUtilTest, FidoToWWIVText_Basic) {
  const std::string fido = "Hello\rWorld\r";
  const std::string wwiv = FidoToWWIVText(fido);
  EXPECT_EQ("Hello\r\nWorld\r\n", wwiv);
}

TEST_F(FidoUtilTest, FidoToWWIVText_BlankLines) {
  const std::string fido = "Hello\r\r\rWorld\r";
  const auto wwiv = FidoToWWIVText(fido);
  EXPECT_EQ("Hello\r\n\r\n\r\nWorld\r\n", wwiv);
}

TEST_F(FidoUtilTest, FidoToWWIVText_SoftCr) {
  const std::string fido = "a\x8d"
                "b\r";
  const auto wwiv = FidoToWWIVText(fido);
  EXPECT_EQ("a\r\nb\r\n", wwiv);
}

TEST_F(FidoUtilTest, FidoToWWIVText_ControlLine) {
  const std::string fido = "\001"
                "PID\rWorld\r";
  const auto wwiv = FidoToWWIVText(fido);
  EXPECT_EQ("\004"
            "0PID\r\nWorld\r\n",
            wwiv);
}

TEST_F(FidoUtilTest, FidoToWWIVText_SeenBy) {
  const std::string fido = "Hello\rSEEN-BY: 1/1\rWorld\r";
  const auto  wwiv = FidoToWWIVText(fido);
  EXPECT_EQ("Hello\r\n\004"
            "0SEEN-BY: 1/1\r\nWorld\r\n",
            wwiv);
}

TEST_F(FidoUtilTest, FidoToWWIVText_ControlLine_DoNotConvert) {
  const std::string fido = "\001"
                "PID\rWorld\r";
  const auto  wwiv = FidoToWWIVText(fido, false);
  EXPECT_EQ("\001"
            "PID\r\nWorld\r\n",
            wwiv);
}

TEST_F(FidoUtilTest, WWIVToFido_Basic) {
  const std::string wwiv = "a\r\nb\r\n";
  const auto  fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\rb\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_BlankLines) {
  const std::string wwiv = "a\r\n\r\n\r\nb\r\n";
  const auto  fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\r\r\rb\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_MalformedControlLine) {
  const std::string wwiv = "a\r\nb\r\n\004"
                "0Foo:\r\n";
  const auto  fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\rb\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_MsgId) {
  const std::string wwiv = "a\r\nb\r\n\004"
                "0MSGID: 1234 5678\r\n";
  const auto  fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\rb\r\001MSGID: 1234 5678\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_SeenBy) {
  const std::string wwiv = "a\r\nb\r\n\004"
                "0SEEN-BY: 1/1\r\n";
  const auto  fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\rb\rSEEN-BY: 1/1\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_Reply) {
  const std::string wwiv = "a\r\nb\r\n\004"
                "0REPLY: 1234 5678\r\n";
  const auto  fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\rb\r\001REPLY: 1234 5678\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_RemovesControlZ) {
  const std::string wwiv = "a\r\n\x1a";
  const auto  fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_RemovesControlZs) {
  const std::string wwiv = "a\r\n\x1a\x1a\x1a\x1a";
  const auto  fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_RemovesControlA) {
  const std::string wwiv = "a\r\nb\001\r\n";
  const auto  fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\rb\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_RemovesControlB) {
  const std::string wwiv = "a\r\n\002b\r\n";
  const auto fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\rb\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_RemovesHeart) {
  const std::string wwiv = "a\r\n\003""1b\r\n";
  const auto fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\rb\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_RemovesHeartAndPipes) {
  const std::string wwiv = "a\r\n\003""1b|#1c|#0\r\n";
  opts.wwiv_heart_color_codes = false;
  opts.wwiv_pipe_color_codes = false;
  opts.allow_any_pipe_codes = false;
  const auto fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\rbc\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_HeartAtEnd) {
  const std::string wwiv = "a\r\n\003";
  opts.wwiv_heart_color_codes = true;
  opts.wwiv_pipe_color_codes = true;
  opts.allow_any_pipe_codes = true;
  const auto fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\r\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_PipeAtEnd) {
  const std::string wwiv = "a\r\n|";
  opts.wwiv_heart_color_codes = true;
  opts.wwiv_pipe_color_codes = true;
  opts.allow_any_pipe_codes = true;
  const auto fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\r|\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_ConvertsHeart) {
  const std::string wwiv = "a\r\n\003""1b\r\n";
  opts.wwiv_heart_color_codes = true;
  const auto fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\r|11b\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_ConvertsHeartOnly) {
  const std::string wwiv = "\003""1";
  opts.wwiv_heart_color_codes = true;
  const auto fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("|11\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_ConvertsUserColorPipe) {
  const std::string wwiv = "a\r\n|#1b\r\n";
  opts.wwiv_pipe_color_codes = true;
  const auto fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\r|11b\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_DoesntWhenNotSelected_ConvertsUserColorPipe) {
  const std::string wwiv = "a\r\n|#1b\r\n";
  opts.wwiv_pipe_color_codes = false;
  opts.wwiv_heart_color_codes = true;
  const auto fido = WWIVToFidoText(wwiv, opts);
  EXPECT_EQ("a\r|#1b\r", fido);
}


TEST_F(FidoUtilTest, MkTime) {
  auto now = time(nullptr);
  auto* tm = localtime(&now);
  const auto rt = mktime(tm);

  EXPECT_EQ(now, rt);
}

TEST_F(FidoUtilTest, Routes) {
  const FidoAddress a("11:1/100");

  EXPECT_TRUE(RoutesThroughAddress(a, "*"));
  EXPECT_TRUE(RoutesThroughAddress(a, "11:*"));
  EXPECT_TRUE(RoutesThroughAddress(a, "11:1/*"));
  EXPECT_TRUE(RoutesThroughAddress(a, "11:1/100"));

  EXPECT_TRUE(RoutesThroughAddress(a, "11:* 1:* 2:* 3:* 4:*"));

  EXPECT_FALSE(RoutesThroughAddress(a, "!*"));
  EXPECT_FALSE(RoutesThroughAddress(a, "12:*"));
  EXPECT_FALSE(RoutesThroughAddress(a, "11:2/*"));
  EXPECT_FALSE(RoutesThroughAddress(a, "11:1/101"));

  EXPECT_FALSE(RoutesThroughAddress(a, "11:* !11:1/100"));
  EXPECT_FALSE(RoutesThroughAddress(a, "11:* 11:1/* !11:1/100"));

  EXPECT_TRUE(RoutesThroughAddress(a, "11:* !11:1/* 11:1/100"));
  EXPECT_TRUE(RoutesThroughAddress(a, "!11:* 11:1/100"));
  EXPECT_TRUE(RoutesThroughAddress(a, "* !11:* 11:1/100"));
  EXPECT_TRUE(RoutesThroughAddress(a, "11:* !11:1/* 11:1/100"));
}

class FidoUtilConfigTest : public testing::Test {
public:
  FidoUtilConfigTest() {
    CHECK(files_.Mkdir("bbs"));
    CHECK(files_.Mkdir("bbs/net"));
    const auto root = files_.Dir("bbs");
    config_.reset(new wwiv::sdk::Config(root));
  }

  FileHelper files_;
  std::unique_ptr<wwiv::sdk::Config> config_;
};

TEST_F(FidoUtilConfigTest, ExistsBundle) {
  Network net;
  net.name = "testnet";
  ASSERT_TRUE(files_.Mkdir("bbs/net"));
  net.dir = files_.DirName("bbs/net");

  EXPECT_FALSE(exists_bundle(*config_, net));
  EXPECT_FALSE(exists_bundle(net.dir));

  files_.CreateTempFile("bbs/net/00124567.su0", "x");
  EXPECT_TRUE(exists_bundle(*config_, net));
  EXPECT_TRUE(exists_bundle(net.dir)) << net.dir;
}

TEST_F(FidoUtilTest, GetAddressFromSingleLine) {
  {
    const auto a = get_address_from_single_line("");
    EXPECT_EQ(-1, a.net());
    EXPECT_EQ(-1, a.node());
  }

  {
    const auto a = get_address_from_single_line("Not an origin line");
    EXPECT_EQ(-1, a.net());
    EXPECT_EQ(-1, a.node());
  }

  {
    const auto a = get_address_from_single_line(" * Origin: Kewl BBS (1:2/3)");
    EXPECT_EQ(1, a.zone());
    EXPECT_EQ(2, a.net());
    EXPECT_EQ(3, a.node());
  }
}

TEST_F(FidoUtilTest, FidoToDaten) {
  const auto t = fido_to_daten("23 Dec 16  20:53:38");
  EXPECT_GT(t, 0u);
}

TEST_F(FidoUtilTest, TzOffsetFromUTC) {
  char s[100];
  auto t = time(nullptr);
  auto* tm = localtime(&t);
  memset(s, 0, sizeof s);
  ASSERT_NE(0UL, strftime(s, sizeof s, "%z", tm));
  
  std::string ss{s};
  if (ss.front() == '+') {
    ss = ss.substr(1);
  }

  EXPECT_EQ(ss, tz_offset_from_utc(DateTime::from_tm(tm)));
}

TEST_F(FidoUtilTest, IsPacketFile) {
  EXPECT_TRUE(is_packet_file("0c386971.pkt"));  
  EXPECT_TRUE(is_packet_file("0000000.pkt"));  
  EXPECT_FALSE(is_packet_file("0c386971.pkts"));  
  EXPECT_FALSE(is_packet_file(""));  
}

TEST_F(FidoUtilTest, FtnBytesWaiting) {
  Network net{};

  net.dir = helper_.TempDir().string();
  net.fido.fido_address = "11:1/211";
  const FidoAddress dest("11:1/100");
  const auto actual = helper_.CreateTempFile("0000006f.su0", std::string(2000, 'X'));
  const auto line = fmt::format("^{}", actual.string());
  auto p = helper_.CreateTempFile("00010064.flo", line);
  const auto bytes_waiting = ftn_bytes_waiting(net, dest);
  EXPECT_EQ(2000, bytes_waiting);
}

TEST_F(FidoUtilTest, FtnBytesWaiting_NonExistant) {
  Network net{};

  net.dir = helper_.TempDir().string();
  net.fido.fido_address = "11:1/211";
  const FidoAddress dest("11:1/100");
  const auto actual = helper_.CreateTempFile("0000006f.su0", std::string(2000, 'X'));
  const auto line = fmt::format("^{}", actual.string());
  auto p = helper_.CreateTempFilePath("00010064.flo");
  const auto bytes_waiting = ftn_bytes_waiting(net, dest);
  EXPECT_EQ(0, bytes_waiting);
}
