/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)2014, WWIV Software Services                  */
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

#include "bbs/new_bbslist.h"

#include "bbs_test/bbs_helper.h"
#include "core/strings.h"
#include "core_test/file_helper.h"

using std::cout;
using std::endl;
using std::ostringstream;
using std::string;
using std::unique_ptr;
using std::vector;

using namespace wwiv::bbslist;

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
  vector<unique_ptr<BbsListEntry>> entries;
  ASSERT_FALSE(LoadFromJSON(dir(), "bbslist.json", &entries));
}

TEST_F(NewBbsListTest, SingleItem_NoAddress) {
  const string json = "{ \"bbslist\": [ { \"name\": \"n1\", \"software\": \"s1\" } ] }";
  this->CreateTempFile("bbslist.json", json);

  vector<unique_ptr<BbsListEntry>> entries;
  ASSERT_TRUE(LoadFromJSON(dir(), "bbslist.json", &entries));

  EXPECT_EQ(1, entries.size());
  EXPECT_STREQ("n1", entries[0]->name.c_str());
  EXPECT_STREQ("s1", entries[0]->software.c_str());
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

  vector<unique_ptr<BbsListEntry>> entries;
  ASSERT_TRUE(LoadFromJSON(dir(), "bbslist.json", &entries));

  EXPECT_EQ(1, entries.size());
  BbsListEntry* e = entries[0].get();
  EXPECT_STREQ("n1", e->name.c_str());
  EXPECT_STREQ("s1", e->software.c_str());
  EXPECT_EQ(1, e->addresses.size());
  EXPECT_STREQ("example.com", e->addresses.find(ConnectionType::TELNET)->second.c_str());
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

  vector<unique_ptr<BbsListEntry>> entries;
  ASSERT_TRUE(LoadFromJSON(dir(), "bbslist.json", &entries));

  EXPECT_EQ(3, entries.size());
  EXPECT_STREQ("n1", entries[0]->name.c_str());
  EXPECT_STREQ("n2", entries[1]->name.c_str());
  EXPECT_STREQ("n3", entries[2]->name.c_str());
  EXPECT_STREQ("s", entries[0]->software.c_str());
  EXPECT_STREQ("s", entries[1]->software.c_str());
  EXPECT_STREQ("s", entries[2]->software.c_str());
  EXPECT_EQ(1, entries[0]->addresses.size());
  EXPECT_STREQ("example.com", entries[0]->addresses.find(ConnectionType::TELNET)->second.c_str());
  EXPECT_STREQ("foobar.com", entries[1]->addresses.find(ConnectionType::SSH)->second.c_str());
  EXPECT_STREQ("415-000-0000", entries[2]->addresses.find(ConnectionType::MODEM)->second.c_str());
}

TEST_F(NewBbsListTest, WriteSingleEntry) {
  vector<unique_ptr<BbsListEntry>> entries;
  {
    BbsListEntry* e = new BbsListEntry();
    e->name = "n1";
    e->software = "SOFT";
    e->addresses[ConnectionType::SSH] = "example.com";
    entries.emplace_back(e);
  }

  ASSERT_TRUE(SaveToJSON(dir(), "bbslist.json", entries));
  vector<unique_ptr<BbsListEntry>> new_entries;
  ASSERT_TRUE(LoadFromJSON(dir(), "bbslist.json", &new_entries));
  EXPECT_EQ(1, new_entries.size());
  const auto e = new_entries.at(0).get();
  EXPECT_STREQ("n1", e->name.c_str());
  EXPECT_STREQ("SOFT", e->software.c_str());
  EXPECT_EQ(1, new_entries.at(0)->addresses.size());
  const auto a = new_entries.at(0)->addresses.cbegin();
  EXPECT_EQ(ConnectionType::SSH, a->first);
  EXPECT_STREQ("example.com", a->second.c_str());
}
