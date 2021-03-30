/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2016-2021, WWIV Software Services             */
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
#include "core/stl.h"
#include "sdk/fido/fido_address.h"

#include "gtest/gtest.h"

using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::sdk::fido;

TEST(FidoAddressTest, Basic) {
  const FidoAddress f("11:10/211.123@wwiv-ftn");
  EXPECT_EQ(11, f.zone());
  EXPECT_EQ(10, f.net());
  EXPECT_EQ(211, f.node());
  EXPECT_EQ(123, f.point());
  EXPECT_EQ("wwiv-ftn", f.domain());

  EXPECT_EQ("11:10/211.123@wwiv-ftn", f.as_string(true));
}

TEST(FidoAddressTest, Basic_TryParse) {
  const auto o = try_parse_fidoaddr("11:10/211.123@wwiv-ftn");
  ASSERT_TRUE(o);
  const auto& f = o.value();
  EXPECT_EQ(11, f.zone());
  EXPECT_EQ(10, f.net());
  EXPECT_EQ(211, f.node());
  EXPECT_EQ(123, f.point());
  EXPECT_EQ("wwiv-ftn", f.domain());

  EXPECT_EQ("11:10/211.123@wwiv-ftn", f.as_string(true));
}

TEST(FidoAddressTest, ZoneNetNode) {
  const FidoAddress f("11:10/211");
  EXPECT_EQ(11, f.zone());
  EXPECT_EQ(10, f.net());
  EXPECT_EQ(211, f.node());
  EXPECT_EQ(0, f.point());
  EXPECT_FALSE(f.has_domain());

  EXPECT_EQ("11:10/211", f.as_string(true));
}

TEST(FidoAddressTest, DefaultZone) {
  const FidoAddress f("10/211");
  EXPECT_EQ(1, f.zone());  // 1 is the default
  EXPECT_EQ(10, f.net());
  EXPECT_EQ(211, f.node());
  EXPECT_EQ(0, f.point());
  EXPECT_FALSE(f.has_domain());

  EXPECT_EQ("1:10/211", f.as_string(true));
}

TEST(FidoAddressTest, WithDomain) {
  const FidoAddress f("1:10/211");
  EXPECT_EQ(f.with_domain("foo").as_string(true, true), "1:10/211@foo");
}

TEST(FidoAddressTest, WithDefaultDomain_None) {
  const FidoAddress f("1:10/211");
  EXPECT_EQ(f.with_default_domain("foo").as_string(true, true), "1:10/211@foo");
}

TEST(FidoAddressTest, WithDefaultDomain_Existing) {
  const FidoAddress f("1:10/211@bar");
  EXPECT_EQ(f.with_default_domain("foo").as_string(true, true), "1:10/211@bar");
}

TEST(FidoAddressTest, WithoutDomain) {
  const FidoAddress f("1:10/211@foo");
  EXPECT_EQ(f.without_domain().as_string(true, true), "1:10/211");
}


TEST(FidoAddressTest, MissingNetOrNode) {
  try {
    FidoAddress f("1");
    FAIL() << "Exception expected";
  } catch (const wwiv::sdk::fido::bad_fidonet_address&) {
  }
}

TEST(FidoAddressTest, MissingNetOrNode_TryParse) {
  auto o = try_parse_fidoaddr("1");
  EXPECT_FALSE(o);
}

TEST(FidoAddressTest, LT) {
  const FidoAddress f1("11:10/211");
  const FidoAddress f2("11:10/212");

  EXPECT_LT(f1, f2);
}

TEST(FidoAddressTest, Set) {
  const FidoAddress f1("11:10/211");
  const FidoAddress f2("11:10/212");
  const FidoAddress f3("11:10/213");

  const std::set<FidoAddress> addrs{f1, f2};
  EXPECT_TRUE(contains(addrs, f1));
  EXPECT_TRUE(contains(addrs, f2));
  EXPECT_FALSE(contains(addrs, f3));
}

TEST(FidoAddressTest, Streams) {
  const std::string kAddr = "11:10/211";
  const FidoAddress f1(kAddr);

  std::ostringstream ss;
  ss << f1;
  EXPECT_EQ(kAddr, ss.str());
}

// 1:0/120<1:103/17
TEST(FidoAddressTest, LTGT) {
  const FidoAddress f1("1:0/120");
  const FidoAddress f2("1:103/17");

  EXPECT_LT(f1, f2);
  EXPECT_FALSE(f2 < f1);
}

TEST(FidoAddressTest, TryParseFlex_Smoke) {
  auto o = try_parse_fidoaddr("1:103/17", fidoaddr_parse_t::lax);
  ASSERT_TRUE(o);
  ASSERT_EQ(FidoAddress("1:103/17"), o.value());
}

TEST(FidoAddressTest, TryParseFlex_LeadingName) {
  const FidoAddress expected("1:103/17");
  auto o = try_parse_fidoaddr("SysopName, 1:103/17", fidoaddr_parse_t::lax);
  ASSERT_TRUE(o);
  ASSERT_EQ(expected, o.value());
}

TEST(FidoAddressTest, TryParseFlex_LeadingNameStrict) {
  const FidoAddress expected("1:103/17");
  const auto o = try_parse_fidoaddr("SysopName, 1:103/17", fidoaddr_parse_t::strict);
  ASSERT_FALSE(o);
}

TEST(FidoAddressTest, TryParseFlex_LeadingNameAndTrailingSpace) {
  const FidoAddress expected("1:103/17");
  auto o = try_parse_fidoaddr("SysopName, 1:103/17    ", fidoaddr_parse_t::lax);
  ASSERT_TRUE(o);
  ASSERT_EQ(expected, o.value());
}

TEST(FidoAddressTest, TryParseFlex_LeadingNameAndTrailingChars) {
  const FidoAddress expected("1:103/17");
  auto o = try_parse_fidoaddr("SysopName, 1:103/17  Cool", fidoaddr_parse_t::lax);
  ASSERT_TRUE(o);
  ASSERT_EQ(expected, o.value());
}
