/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2019, WWIV Software Services               */
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

#include <memory>
#include <sstream>
#include <string>
#include <vector>
#include "bbs/new_bbslist.h"

#include "bbs_test/bbs_helper.h"
#include "core/strings.h"
#include "core_test/file_helper.h"

using std::cout;
using std::endl;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::bbslist;

static string FindAddressByType(const BbsListEntry& entry, const std::string& type) {
  for (const auto& a : entry.addresses) {
    if (type == a.type) {
      return a.address;
    }
  }
  return{};
}

class NewBbsListTest : public testing::Test {
protected:
    virtual void SetUp() { helper.SetUp(); }
    const string dir() { return helper.files().TempDir(); }
    string CreateTempFile(const string& name, const string& contents) {
      return helper.files().CreateTempFile(name, contents);
    }
    BbsHelper helper;
};

TEST_F(NewBbsListTest, NoFile) {
  vector<BbsListEntry> entries;
  ASSERT_FALSE(LoadFromJSON(dir(), "bbslist.json", entries));
}

TEST_F(NewBbsListTest, SingleItem_NoAddress) {
  const string json = "{ \"bbslist\": [ { \"name\": \"n1\", \"software\": \"s1\" } ] }";
  this->CreateTempFile("bbslist.json", json);
   
  vector<BbsListEntry> entries;
  ASSERT_TRUE(LoadFromJSON(dir(), "bbslist.json", entries));

  ASSERT_EQ(1, entries.size());
  EXPECT_EQ("n1", entries[0].name);
  EXPECT_EQ("s1", entries[0].software);
}

TEST_F(NewBbsListTest, SingleItem_Address) {
  const string json =
    "{ \"bbslist\": "
      "[ { \"name\": \"n1\", \"software\": \"s1\", "
      "\"addresses\": [ "
        "{ \"type\":\"telnet\", \"address\":\"example.com\" } "
        "] } "
      "] "
    "}";
  this->CreateTempFile("bbslist.json", json);

  vector<BbsListEntry> entries;
  EXPECT_TRUE(LoadFromJSON(dir(), "bbslist.json", entries));

  EXPECT_EQ(1, entries.size());
  const auto& e = entries[0];
  EXPECT_EQ("n1", e.name);
  EXPECT_EQ("s1", e.software);
  EXPECT_EQ(1, e.addresses.size());
  EXPECT_EQ("example.com", FindAddressByType(e, "telnet"));
}

TEST_F(NewBbsListTest, MultipleEntries) {
  const string json =
    "{ \"bbslist\": ["
      "{ \"name\": \"n1\", \"software\": \"s\", \"addresses\": [ { \"type\":\"telnet\", \"address\":\"example.com\" } ] }, "
      "{ \"name\": \"n2\", \"software\": \"s\", \"addresses\": [ { \"type\":\"ssh\", \"address\":\"foobar.com\" } ] }, "
      "{ \"name\": \"n3\", \"software\": \"s\", \"addresses\": [ { \"type\":\"modem\", \"address\":\"415-000-0000\" } ] } "
      "] "
    "}";
  this->CreateTempFile("bbslist.json", json);

  vector<BbsListEntry> entries;
  ASSERT_TRUE(LoadFromJSON(dir(), "bbslist.json", entries));
  
  EXPECT_EQ(3, entries.size());
  EXPECT_EQ("n1", entries[0].name);
  EXPECT_EQ("n2", entries[1].name);
  EXPECT_EQ("n3", entries[2].name);
  EXPECT_EQ("s", entries[0].software);
  EXPECT_EQ("s", entries[1].software);
  EXPECT_EQ("s", entries[2].software);
  EXPECT_EQ(1, entries[0].addresses.size());
  EXPECT_EQ("example.com", FindAddressByType(entries[0], "telnet"));
  EXPECT_EQ("foobar.com", FindAddressByType(entries[1], "ssh"));
  EXPECT_EQ("415-000-0000", FindAddressByType(entries[2], "modem"));
}

TEST_F(NewBbsListTest, WriteSingleEntry) {
  vector<BbsListEntry> entries;
  {
    BbsListEntry e{};
    e.name = "n1";
    e.software = "SOFT";
    e.addresses.push_back({"ssh", "example.com"});
    entries.emplace_back(e);
  }

  ASSERT_TRUE(SaveToJSON(dir(), "bbslist.json", entries));
  vector<BbsListEntry> new_entries;
  ASSERT_TRUE(LoadFromJSON(dir(), "bbslist.json", new_entries));
  EXPECT_EQ(1, new_entries.size());
  const auto& e = new_entries.at(0);
  EXPECT_EQ("n1", e.name);
  EXPECT_EQ("SOFT", e.software);
  EXPECT_EQ(1, new_entries.at(0).addresses.size());
  const auto a = new_entries.at(0).addresses.cbegin();
  EXPECT_EQ("ssh", a->type);
  EXPECT_EQ("example.com", a->address);
}
