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
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/sdk_helper.h"
#include <memory>
#include <string>
#include <vector>

using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

class NamesTest : public testing::Test {
public:
  NamesTest() {
    EXPECT_TRUE(CreateNames());
    names_.reset(new Names(helper.config()));
  }

  [[nodiscard]] bool CreateNames() const {
    File file(FilePath(helper.datadir(), NAMES_LST));
    file.Open(File::modeBinary|File::modeWriteOnly|File::modeCreateFile, File::shareDenyNone);
    if (!file.IsOpen()) {
      return false;
    }

    smalrec u3{"A", 3};
    smalrec u2{"B", 2};
    smalrec u1{"C", 1};

    file.Write(&u3, sizeof(smalrec));
    file.Write(&u2, sizeof(smalrec));
    file.Write(&u1, sizeof(smalrec));
    return true;
  }

  SdkHelper helper;
  unique_ptr<Names> names_;
};

TEST_F(NamesTest, UserName) {
  EXPECT_STREQ("A #3", names_->UserName(3).c_str());
  EXPECT_STREQ("A #3 @1234", names_->UserName(3, 1234).c_str());
}

TEST_F(NamesTest, Remove) {
  // Force a save/load and make sure it's there.
  names_->set_save_on_exit(true);
  ASSERT_EQ(3, names_->size());

  EXPECT_TRUE(names_->Remove(2));
  EXPECT_EQ(2, names_->size());
  EXPECT_TRUE(names_->UserName(2).empty());

  // already removed, so nothing do to.
  EXPECT_FALSE(names_->Remove(2));

  // Release the old names 1st.
  names_.reset();
  names_.reset(new Names(helper.config()));
  // Should still have 2.
  EXPECT_EQ(2, names_->size());
}

TEST_F(NamesTest, Insert) {
  // Force a save/load and make sure it's there.
  names_->set_save_on_exit(true);

  ASSERT_EQ(3, names_->size());

  EXPECT_TRUE(names_->Add("Z", 26));
  EXPECT_EQ(4, names_->size());

  EXPECT_EQ("Z #26", names_->UserName(26));

  // Release the old names 1st.
  names_.reset();
  names_.reset(new Names(helper.config()));
  EXPECT_EQ(4, names_->size());
}

TEST_F(NamesTest, SaveOnExit) {
  names_->set_save_on_exit(true);
  ASSERT_TRUE(names_->save_on_exit());
}

TEST(NamesRebuildTest, Rebuild) {
  SdkHelper helper;

  UserManager um(helper.config());
  User u1{};
  User::CreateNewUserRecord(&u1, 50, 20, 0, 0.1234f, 
  { 7, 11, 14, 13, 31, 10, 12, 9, 5, 3 }, { 7, 15, 15, 15, 112, 15, 15, 7, 7, 7 });
  u1.set_name("FOO");
  um.writeuser(&u1, 1);
  User u2{};
  User::CreateNewUserRecord(&u2, 50, 20, 0, 0.1234f, 
  { 7, 11, 14, 13, 31, 10, 12, 9, 5, 3 }, { 7, 15, 15, 15, 112, 15, 15, 7, 7, 7 });
  u2.set_name("BAR");
  um.writeuser(&u2, 2);

  Names names(helper.config());
  names.Rebuild(um);
  ASSERT_TRUE(names.Save());

  auto v = names.names_vector();
  ASSERT_EQ(2u, v.size());
  EXPECT_STREQ("BAR", (char*) v.at(0).name);
  EXPECT_STREQ("FOO", (char*) v.at(1).name);
}