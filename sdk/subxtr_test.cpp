/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2022, WWIV Software Services               */
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

#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/subxtr.h"
#include "sdk/vardec.h"
#include "sdk/sdk_helper.h"
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

// Made visible for testing
namespace wwiv::sdk {

bool read_subs_xtr(const std::filesystem::path& datadir, const std::vector<Network>& net_networks,
                   const std::vector<subboardrec_422_t>& subs, std::vector<xtrasubsrec>& xsubs);
bool write_subs_xtr(const std::filesystem::path& datadir, const std::vector<Network>& net_networks,
                    const std::vector<xtrasubsrec>& xsubs, int);

std::vector<subboardrec_422_t> read_subs(const std::filesystem::path& datadir);
bool write_subs(const std::filesystem::path& datadir,
                const std::vector<subboardrec_422_t>& subboards);

}

class SubXtrTest: public testing::Test {
protected:
  void SetUp() override { 
    helper.SetUp(); 
    net_networks_.emplace_back(Network{network_type_t::wwivnet, "testnet", "testnet/", 2});
    subs_.emplace_back(subboardrec_422_t{"Sub1", "S1", '1', 10, 10, 0, 0, 500, 0, 2, 0});
    subs_.emplace_back(subboardrec_422_t{"Sub2", "S2", '2', 10, 10, 0, 0, 500, 0, 2, 0});
  }

  [[nodiscard]] std::string dir() const { return helper.files_.TempDir().string(); }
  void CreateTempFile(const std::string& name, const std::string& contents) {
    helper.files().CreateTempFile(name, contents);
  }
  SdkHelper helper;
  std::vector<Network> net_networks_;
  std::vector<subboardrec_422_t> subs_;
};

TEST_F(SubXtrTest, Write) {
  std::vector<xtrasubsrec> xsubs;
  xsubs.emplace_back(xtrasubsrec{0, "", {}});
  xtrasubsrec s2{0, "this is sub2", {}};
  s2.nets.emplace_back(xtrasubsnetrec{0, 0, 1, 1, "S2"});
  xsubs.emplace_back(s2);

  write_subs_xtr(helper.datadir(), net_networks_, xsubs, 0);
  TextFile subs_xtr_file(FilePath(helper.datadir(), "subs.xtr"), "r");
  auto actual = SplitString(subs_xtr_file.ReadFileIntoString(), "\n");
  ASSERT_EQ(4u, actual.size());
  std::vector<std::string> expected = {
    { "!1", "@this is sub2", "#0", "$testnet S2 0 1 1"},
  };
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), actual.begin()));
}

static bool equal(const xtrasubsnetrec& x1, const xtrasubsnetrec& x2) {
  if (x1.category != x2.category) {
    return false;
  }
  if (x1.flags != x2.flags) {
    return false;
  }
  if (x1.host != x2.host) {
    return false;
  }
  if (x1.net_num != x2.net_num) {
    return false;
  }
  return iequals(x1.stype_str, x2.stype_str);
}

static bool equal(const xtrasubsrec& x1, const xtrasubsrec& x2) {
  if (!IsEquals(x1.desc, x2.desc)) {
    return false;
  }
  if (x1.nets.size() != x2.nets.size()) {
    return false;
  }
  for (auto i = 0; i < wwiv::stl::ssize(x1.nets); i++) {
    if (!equal(x1.nets[i], x2.nets[i])) {
      return false;
    }
  }
  return true;
}

TEST_F(SubXtrTest, Read) {
  std::vector<xtrasubsrec> expected;
  expected.emplace_back(xtrasubsrec{0, "", {}});
  xtrasubsrec s2{0, "this is sub2", {}};
  s2.nets.emplace_back(xtrasubsnetrec{0, 0, 1, 1, "S2"});
  expected.emplace_back(s2);

  {
    std::vector<std::string> contents{
      {"!1", "@this is sub2", "#0", "$testnet S2 0 1 1"},
    };
    TextFile subs_xtr_file(FilePath(helper.datadir(), "subs.xtr"), "w");
    for (const auto& line : contents) {
      subs_xtr_file.WriteLine(line);
    }
  }
  std::vector<xtrasubsrec> actual;
  read_subs_xtr(helper.datadir(), net_networks_, subs_, actual);
  ASSERT_EQ(subs_.size(), actual.size());
  ASSERT_EQ(expected.size(), actual.size());
  for (auto i = 0; i < wwiv::stl::ssize(subs_); i++) {
    EXPECT_TRUE(equal(expected.at(i), actual.at(i)));
  }
}


TEST_F(SubXtrTest, JsonSmoke) {
  const std::string json = R"(
  {
    "version": 1,
    "subs": [
      { "name": "n1", "storage_type": 2 }
    ]
  }
 )";
  this->CreateTempFile("subs.json", json);

  std::vector<subboard_t> subs;
  ASSERT_TRUE(Subs::LoadFromJSON(dir(), "subs.json", subs));

  ASSERT_EQ(1, wwiv::stl::ssize(subs));
  EXPECT_EQ("n1", subs[0].name);
  EXPECT_EQ(2, subs[0].storage_type);

}