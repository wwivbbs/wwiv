/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2016-2019, WWIV Software Services             */
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
#include "core_test/file_helper.h"
#include "sdk/msgapi/message_api_wwiv.h"
#include "sdk/msgapi/type2_text.h"
#include <memory>
#include <optional>
#include <tuple>

using namespace std;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::msgapi;
using namespace wwiv::strings;

class Type2TextTest : public testing::Test {
public:
  void SetUp() override {
    path_ = helper.CreateTempFilePath("foo.dat");
    t_ = std::make_unique<Type2Text>(path_);
  }

  [[nodiscard]] bool CreateMsgTextFile() const {
    File f(path_);
    if (!f.Open(File::modeBinary | File::modeCreateFile | File::modeReadWrite)) {
      return false;
    }
    f.set_length(GAT_SECTION_SIZE + (75L * 1024L));

    return true;
  }

  [[nodiscard]] std::tuple<bool, messagerec> save_message(const std::string& text) {
    messagerec m{};
    const auto ok = t_->savefile(text, &m);
    return { ok, m };
  }

  FileHelper helper;
  unique_ptr<Type2Text> t_;
  std::filesystem::path path_;
};

TEST_F(Type2TextTest, Empty) {
  ASSERT_TRUE(CreateMsgTextFile());

  File f(path_);
  ASSERT_TRUE(f.Open(File::modeBinary | File::modeReadWrite));
  auto gat = t_->load_gat(f, 0);
  for (const auto g : gat) {
    EXPECT_EQ(g, 0);
  }
}

TEST_F(Type2TextTest, NotEmpty) {
  ASSERT_TRUE(CreateMsgTextFile());

  messagerec m{};
  m.storage_type = 2;
  m.stored_as = 0xffffffff;
  ASSERT_TRUE(t_->savefile("Hello World", &m));
  EXPECT_EQ(m.stored_as, 1);
}

TEST_F(Type2TextTest, Save_Then_Load) {
  ASSERT_TRUE(CreateMsgTextFile());

  auto [ok, m] = save_message("Hello World");
  ASSERT_TRUE(ok);
  ASSERT_EQ(1, m.stored_as);
  auto [ok2, m2] = save_message("Hello World2");
  ASSERT_TRUE(ok2);
  ASSERT_EQ(2, m2.stored_as);

  std::string out;
  ASSERT_TRUE(t_->readfile(&m, &out));
  ASSERT_EQ(1, m.stored_as);
  ASSERT_EQ("Hello World", out);

  ASSERT_TRUE(t_->readfile(&m2, &out));
  ASSERT_EQ(2, m2.stored_as);
  ASSERT_EQ("Hello World2", out);
}

TEST_F(Type2TextTest, TwoBlocks) {
  ASSERT_TRUE(CreateMsgTextFile());

  auto [ok, m] = save_message("Hello World");
  ASSERT_TRUE(ok);
  ASSERT_EQ(1, m.stored_as);

  const std::string two_blocks(513, 'x');
  auto [ok2, m2] = save_message(two_blocks);
  ASSERT_TRUE(ok2);
  ASSERT_EQ(2, m2.stored_as);

  auto [ok4, m4] = save_message("Hello World");
  ASSERT_TRUE(ok);
  ASSERT_EQ(4, m4.stored_as);

  std::string out;
  ASSERT_TRUE(t_->readfile(&m2, &out));
  ASSERT_EQ(2, m2.stored_as);
  ASSERT_EQ(two_blocks, out);
}