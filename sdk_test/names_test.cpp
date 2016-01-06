/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2015 WWIV Software Services                */
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

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include "core/file.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk_test/sdk_helper.h"

using namespace std;

using namespace wwiv::sdk;
using namespace wwiv::strings;

class NamesTest : public testing::Test {
public:
  NamesTest() : config(helper.root()) {
    EXPECT_TRUE(config.IsInitialized());
    EXPECT_TRUE(CreateNames(config));
    names_.reset(new Names(config));
  }

  virtual void SetUp() {
    config.IsInitialized();
  }

  bool CreateNames(const Config& config) {
    File file(config.datadir(), NAMES_LST);
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
  Config config;
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
  names_.reset(new Names(config));
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
  names_.reset(new Names(config));
  EXPECT_EQ(4, names_->size());
}

TEST_F(NamesTest, SaveOnExit) {
  names_->set_save_on_exit(true);
  ASSERT_TRUE(names_->save_on_exit());
}
