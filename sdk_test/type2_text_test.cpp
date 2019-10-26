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

  [[nodiscard]] std::optional<messagerec> save_message(const std::string& text) {
    return t_->savefile(text);
  }

  [[nodiscard]] std::optional<std::string> readfile(const messagerec& m) const {
    return t_->readfile(m);
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

  auto m = t_->savefile("Hello World");
  ASSERT_TRUE(m.has_value());
  EXPECT_EQ(m->stored_as, 1);
}

TEST_F(Type2TextTest, Save_Then_Load) {
  ASSERT_TRUE(CreateMsgTextFile());

  auto m1 = save_message("Hello World");
  ASSERT_EQ(1, m1->stored_as);
  auto m2 = save_message("Hello World2");
  ASSERT_EQ(2, m2->stored_as);

  auto out = readfile(m1.value());
  ASSERT_EQ("Hello World", *out);

  out = readfile(m2.value());
  ASSERT_EQ("Hello World2", *out);
}

TEST_F(Type2TextTest, TwoBlocks) {
  ASSERT_TRUE(CreateMsgTextFile());

  auto m1 = save_message("Hello World");
  ASSERT_EQ(1, m1->stored_as);

  const std::string two_blocks(513, 'x');
  auto m2 = save_message(two_blocks);
  ASSERT_EQ(2, m2->stored_as);

  auto m4 = save_message("Hello World");
  ASSERT_EQ(4, m4->stored_as);

  auto out = readfile(m2.value());
  ASSERT_EQ(two_blocks, *out);
}

TEST_F(Type2TextTest, Reuse_Block_After_Delete) {
  ASSERT_TRUE(CreateMsgTextFile());

  auto m1 = save_message("Hello World");
  ASSERT_EQ(1, m1->stored_as);
  auto m2 = save_message("Hello World2");
  ASSERT_EQ(2, m2->stored_as);

  ASSERT_TRUE(t_->remove_link(m1.value()));
  auto m3 = save_message("Hello World3");
  ASSERT_EQ(1, m3->stored_as);

}

TEST_F(Type2TextTest, Move_To_Next_Gat_Section) {
  ASSERT_TRUE(CreateMsgTextFile());

  const std::string msg32k(32 * 1024, 'x'); 
  // Fill up first section.  First section will have only 63 free blocks
  for (auto i = 0; i < 31; i++) {
    auto m = save_message(msg32k);
    ASSERT_EQ((i * 64) + 1, m->stored_as);
  }

  // Need 64 blocks, so will start in 2nd section.
  auto m2 = save_message(msg32k);
  ASSERT_EQ(1, m2->stored_as / 2048);
  ASSERT_EQ(1, m2->stored_as % 2048);

  // Need 1 blocks, so will start in 1nd section again.
  auto m3 = save_message("Hello World3");
  ASSERT_EQ(0, m3->stored_as / 2048);
  ASSERT_EQ(1985, m3->stored_as % 2048);
}


