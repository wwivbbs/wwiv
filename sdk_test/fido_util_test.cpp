/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*            Copyright (C)2016-2019, WWIV Software Services              */
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
#include "core/file.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "core_test/file_helper.h"
#include "sdk/fido/fido_util.h"
#include "sdk/fido/fido_address.h"

#include <cstdint>
#include <ctime>
#include <memory>
#include <string>

using std::endl;
using std::string;
using std::unique_ptr;
using namespace wwiv::core;
using namespace wwiv::strings;
using namespace wwiv::sdk::fido;

class FidoUtilTest: public testing::Test {
public:
  FidoUtilTest() {}
protected:
  FileHelper helper_;
};

TEST_F(FidoUtilTest, PacketName) {
  auto dt = DateTime::now();
  auto now = dt.to_time_t();
  auto tm = dt.to_tm();
  string actual = packet_name(dt);

  string expected = StringPrintf("%2.2d%2.2d%2.2d%2.2d.pkt", tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec);
  EXPECT_EQ(expected, actual);
}

TEST_F(FidoUtilTest, BundleName_Extension) {
  FidoAddress source("1:105/6");
  FidoAddress dest("1:105/42");
  string actual = bundle_name(source, dest, "su0");

  EXPECT_EQ("0000ffdc.su0", actual);
}

TEST_F(FidoUtilTest, BundleName_NoExtension) {
  FidoAddress source("1:105/6");
  FidoAddress dest("1:105/42");
  string actual = bundle_name(source, dest, 1, 3);

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
  FidoAddress dest("1:105/42");
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
  string fido = "Hello\rWorld\r";
  string wwiv = FidoToWWIVText(fido);
  EXPECT_EQ("Hello\r\nWorld\r\n", wwiv);
}

TEST_F(FidoUtilTest, FidoToWWIVText_BlankLines) {
  string fido = "Hello\r\r\rWorld\r";
  string wwiv = FidoToWWIVText(fido);
  EXPECT_EQ("Hello\r\n\r\n\r\nWorld\r\n", wwiv);
}

TEST_F(FidoUtilTest, FidoToWWIVText_SoftCr) {
  string fido = "a\x8d""b\r";
  string wwiv = FidoToWWIVText(fido);
  EXPECT_EQ("a\r\nb\r\n", wwiv);
}

TEST_F(FidoUtilTest, FidoToWWIVText_ControlLine) {
  string fido = "\001""PID\rWorld\r";
  string wwiv = FidoToWWIVText(fido);
  EXPECT_EQ("\004""0PID\r\nWorld\r\n", wwiv);
}

TEST_F(FidoUtilTest, FidoToWWIVText_SeenBy) {
  string fido = "Hello\rSEEN-BY: 1/1\rWorld\r";
  string wwiv = FidoToWWIVText(fido);
  EXPECT_EQ("Hello\r\n\004""0SEEN-BY: 1/1\r\nWorld\r\n", wwiv);
}

TEST_F(FidoUtilTest, FidoToWWIVText_ControlLine_DoNotConvert) {
  string fido = "\001""PID\rWorld\r";
  string wwiv = FidoToWWIVText(fido, false);
  EXPECT_EQ("\001""PID\r\nWorld\r\n", wwiv);
}

TEST_F(FidoUtilTest, WWIVToFido_Basic) {
  string wwiv = "a\r\nb\r\n";
  string fido = WWIVToFidoText(wwiv);
  EXPECT_EQ("a\rb\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_BlankLines) {
  string wwiv = "a\r\n\r\n\r\nb\r\n";
  string fido = WWIVToFidoText(wwiv);
  EXPECT_EQ("a\r\r\rb\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_MalformedControlLine) {
  string wwiv = "a\r\nb\r\n\004""0Foo:\r\n";
  string fido = WWIVToFidoText(wwiv);
  EXPECT_EQ("a\rb\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_MsgId) {
  string wwiv = "a\r\nb\r\n\004""0MSGID: 1234 5678\r\n";
  string fido = WWIVToFidoText(wwiv);
  EXPECT_EQ("a\rb\r\001MSGID: 1234 5678\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_SeenBy) {
  string wwiv = "a\r\nb\r\n\004""0SEEN-BY: 1/1\r\n";
  string fido = WWIVToFidoText(wwiv);
  EXPECT_EQ("a\rb\rSEEN-BY: 1/1\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_Reply) {
  string wwiv = "a\r\nb\r\n\004""0REPLY: 1234 5678\r\n";
  string fido = WWIVToFidoText(wwiv);
  EXPECT_EQ("a\rb\r\001REPLY: 1234 5678\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_RemovesControlZ) {
  string wwiv = "a\r\n\x1a";
  string fido = WWIVToFidoText(wwiv);
  EXPECT_EQ("a\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_RemovesControlZs) {
  string wwiv = "a\r\n\x1a\x1a\x1a\x1a";
  string fido = WWIVToFidoText(wwiv);
  EXPECT_EQ("a\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_RemovesControlA) {
  string wwiv = "a\r\nb\001\r\n";
  string fido = WWIVToFidoText(wwiv);
  EXPECT_EQ("a\rb\r", fido);
}

TEST_F(FidoUtilTest, WWIVToFido_RemovesControlB) {
  string wwiv = "a\r\n\002b\r\n";
  string fido = WWIVToFidoText(wwiv);
  EXPECT_EQ("a\rb\r", fido);
}

TEST_F(FidoUtilTest, MkTime) {
  auto now = time(nullptr);
  auto tm = localtime(&now);
  auto rt = mktime(tm);

  EXPECT_EQ(now, rt);
}

TEST_F(FidoUtilTest, Routes) {
  FidoAddress a("11:1/100");

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

class FidoUtilConfigTest : public testing::Test{
public:
  FidoUtilConfigTest() {
    files_.Mkdir("bbs");
    files_.Mkdir("bbs/net");
    string root = files_.DirName("bbs");
    config_.reset(new wwiv::sdk::Config(root));
  }

  FileHelper files_;
  std::unique_ptr<wwiv::sdk::Config> config_;
};

TEST_F(FidoUtilConfigTest, ExistsBundle) {
  net_networks_rec net;
  to_char_array(net.name, "testnet");
  files_.Mkdir("bbs/net");
  net.dir = files_.DirName("bbs/net");

  EXPECT_FALSE(exists_bundle(*config_, net));
  EXPECT_FALSE(exists_bundle(net.dir));

  string path = files_.CreateTempFile("bbs/net/00124567.su0", "x");
  EXPECT_TRUE(exists_bundle(*config_, net));
  EXPECT_TRUE(exists_bundle(net.dir)) << net.dir;
}

TEST_F(FidoUtilTest, GetAddressFromSingleLine) {
  {
    auto a = get_address_from_single_line("");
    EXPECT_EQ(-1, a.net());
    EXPECT_EQ(-1, a.node());
  }

  {
    auto a = get_address_from_single_line("Not an origin line");
    EXPECT_EQ(-1, a.net());
    EXPECT_EQ(-1, a.node());
  }

  {
    auto a = get_address_from_single_line(" * Origin: Kewl BBS (1:2/3)");
    EXPECT_EQ(1, a.zone());
    EXPECT_EQ(2, a.net());
    EXPECT_EQ(3, a.node());
  }
}

TEST_F(FidoUtilTest, FloFile) {
  auto b = helper_.CreateTempFile("00010064.flo", "^C:\\db\\outbound\\0000006f.su0");
  File f(b);
  net_networks_rec net{};
  net.dir = helper_.TempDir();
  net.fido.fido_address = "11:1/211";
  FloFile flo(net, f.parent(), f.GetName());
  EXPECT_FALSE(flo.poll());
  EXPECT_EQ(1, flo.flo_entries().size());

  {
    auto e = flo.flo_entries().front();
    EXPECT_EQ("C:\\db\\outbound\\0000006f.su0", e.first);
    EXPECT_EQ(flo_directive::delete_file, e.second);
    flo.clear();
    EXPECT_TRUE(flo.empty());
    EXPECT_TRUE(flo.Save());
    EXPECT_FALSE(f.Exists()) << f;
  }

  flo.insert("C:\\db\\outbound\\0000006f.mo0", flo_directive::truncate_file);
  {
    auto e = flo.flo_entries().front();
    EXPECT_EQ("C:\\db\\outbound\\0000006f.mo0", e.first);
    EXPECT_EQ(flo_directive::truncate_file, e.second);
    flo.Save();
    EXPECT_TRUE(f.Exists());
    flo.clear();
    f.Delete();
  }

  {
    flo.set_poll(true);
    flo.Save();
    EXPECT_TRUE(f.Exists());

    TextFile tf(f.full_pathname(), "r");
    const string contents = tf.ReadFileIntoString();
    EXPECT_EQ("", contents);
  }

}

TEST_F(FidoUtilTest, FidoToDaten) {
  auto t = fido_to_daten("23 Dec 16  20:53:38");
  EXPECT_GT(t, 0u);
}

#if (_MSC_VER)
#define timezone _timezone
#endif  // MSV_VER && !timezone

TEST_F(FidoUtilTest, TzOffsetFromUTC) {
  char s[100];
  auto t = time(nullptr);
  auto tm = localtime(&t);
  ASSERT_NE(0UL, strftime(s, sizeof(s), "%z", tm));
  string ss(s);
  EXPECT_EQ(ss, tz_offset_from_utc());
}