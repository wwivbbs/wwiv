/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2016 WWIV Software Services                */
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

#include <string>
#include <vector>

#include "core/strings.h"
#include "core/textfile.h"
#include "core_test/file_helper.h"
#include "sdk/subxtr.h"
#include "sdk_test/sdk_helper.h"

using std::string;
using std::vector;
using namespace wwiv::sdk;
using namespace wwiv::strings;

namespace wwiv {
namespace sdk {

bool read_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const std::vector<subboardrec_422_t>& subs, std::vector<xtrasubsrec>& xsubs);
bool write_subs_xtr(const std::string& datadir, const std::vector<net_networks_rec>& net_networks, const std::vector<xtrasubsrec>& xsubs);

std::vector<subboardrec_422_t> read_subs(const std::string &datadir);
bool write_subs(const std::string &datadir, const std::vector<subboardrec_422_t>& subboards);

}
}

class SubXtrTest: public testing::Test {
protected:
  virtual void SetUp() { 
    helper.SetUp(); 
    net_networks.emplace_back(net_networks_rec{net_type_wwivnet, "testnet", "testnet/", 2});
    subs.emplace_back(subboardrec_422_t{"Sub1", "S1", '1', 10, 10, 0, 0, 500, 0, 2, 0});
    subs.emplace_back(subboardrec_422_t{"Sub2", "S2", '2', 10, 10, 0, 0, 500, 0, 2, 0});
  }
  const string dir() { return helper.files_.TempDir(); }
  string CreateTempFile(const string& name, const string& contents) {
    return helper.files().CreateTempFile(name, contents);
  }
  SdkHelper helper;
  vector<net_networks_rec> net_networks;
  vector<subboardrec_422_t> subs;
};

TEST_F(SubXtrTest, Write) {
  vector<xtrasubsrec> xsubs;
  xsubs.emplace_back(xtrasubsrec{0, ""});
  xtrasubsrec s2{0, "this is sub2"};
  s2.nets.emplace_back(xtrasubsnetrec{0, 0, 1, 1, "S2"});
  xsubs.emplace_back(s2);

  write_subs_xtr(helper.data(), net_networks, xsubs);
  TextFile subs_xtr_file(helper.data(), "subs.xtr", "r");
  vector<string> actual = SplitString(subs_xtr_file.ReadFileIntoString(), "\n");
  ASSERT_EQ(4, actual.size());
  vector<string> expected = {
    { "!1", "@this is sub2", "#0", "$testnet S2 0 1 1"},
  };
  EXPECT_TRUE(std::equal(expected.begin(), expected.end(), actual.begin()));
}

static bool equal(const xtrasubsnetrec& x1, const xtrasubsnetrec& x2) {
  if (x1.category != x2.category) {
    return false;
  } else if (x1.flags != x2.flags) {
    return false;
  } else if (x1.host != x2.host) {
    return false;
  } else if (x1.net_num != x2.net_num) {
    return false;
  } else return IsEqualsIgnoreCase(x1.stype, x2.stype);
}

static bool equal(const xtrasubsrec& x1, const xtrasubsrec& x2) {
  if (!IsEquals(x1.desc, x2.desc)) {
    return false;
  } else if (x1.nets.size() != x2.nets.size()) {
    return false;
  } else {
    for (std::size_t i = 0; i < x1.nets.size(); i++) {
      if (!equal(x1.nets[i], x2.nets[i])) {
        return false;
      }
    }
    return true;
  }
}

TEST_F(SubXtrTest, Read) {
  vector<xtrasubsrec> expected;
  expected.emplace_back(xtrasubsrec{0, ""});
  xtrasubsrec s2{0, "this is sub2"};
  s2.nets.emplace_back(xtrasubsnetrec{0, 0, 1, 1, "S2"});
  expected.emplace_back(s2);

  {
    vector<string> contents{
      {"!1", "@this is sub2", "#0", "$testnet S2 0 1 1"},
    };
    TextFile subs_xtr_file(helper.data(), "subs.xtr", "w");
    for (const auto& line : contents) {
      subs_xtr_file.WriteLine(line);
    }
  }
  vector<xtrasubsrec> actual;
  read_subs_xtr(helper.data(), net_networks, subs, actual);
  ASSERT_EQ(subs.size(), actual.size());
  ASSERT_EQ(expected.size(), actual.size());
  for (std::size_t i = 0; i < subs.size(); i++) {
    EXPECT_TRUE(equal(expected.at(i), actual.at(i)));
  }
}
