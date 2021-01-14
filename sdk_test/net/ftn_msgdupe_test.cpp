/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)2018-2021, WWIV Software Services             */
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

#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/fido/fido_address.h"
#include "sdk/net/ftn_msgdupe.h"
#include "sdk_test/sdk_helper.h"
#include <cstdint>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

using namespace std;

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::strings;

class FtnMsgDupeTest : public testing::Test {
public:
  FtnMsgDupeTest() : config_(helper.root()) {}

  [[nodiscard]] bool CreateDupes(const std::vector<msgids>& ids) const {
    DataFile<msgids> file(FilePath(config_.datadir(), MSGDUPE_DAT),
                          File::modeReadWrite | File::modeBinary | File::modeCreateFile |
                              File::modeTruncate);
    if (!file) {
      return false;
    }

    return file.WriteVector(ids);
  }

  [[nodiscard]] bool SetLastMessageId(uint32_t message_id) const {
    uint64_t id = message_id;
    DataFile<uint64_t> file(FilePath(config_.datadir(), MSGID_DAT),
                            File::modeReadWrite | File::modeBinary | File::modeCreateFile,
                            File::shareDenyReadWrite);
    if (!file) {
      return false;
    }
    return file.Write(&id, 1);
  }

  SdkHelper helper;
  Config config_;
};

// This is what the old msgdupe used.
uint64_t as64(uint32_t header, uint32_t msgid) {
  return static_cast<uint64_t>(header) << 32 | msgid;
}

TEST_F(FtnMsgDupeTest, As64) {
  const auto i = as64(2, 1);
  LOG(INFO) << i;
  LOG(INFO) << "msgids sizeof: " << sizeof(msgids);
  LOG(INFO) << "i sizeof: " << sizeof(i);
  msgids ids{};
  
  memcpy(static_cast<void*>(&ids), &i, sizeof(i));
  LOG(INFO) << "header: " << ids.header << "; msgid: " << ids.msgid;
  EXPECT_EQ(2u, ids.header);
  EXPECT_EQ(1u, ids.msgid);
}

TEST_F(FtnMsgDupeTest, MsgId_NoExists) { 
  FtnMessageDupe dupe(config_.datadir(), true);
  const FidoAddress a{"1:2/3"};
  const auto now = time(nullptr);
  const auto line = dupe.CreateMessageID(a);
  auto parts = SplitString(line, " ");
  ASSERT_EQ(2, wwiv::stl::ssize(parts));
  EXPECT_TRUE(starts_with(parts.at(0), "1:2/3"));
  const auto id = std::stol(parts.at(1), nullptr, 16);

  EXPECT_LT(std::abs(id - now), 11) << "id: " << id << "; now: " << now;
  EXPECT_TRUE(File::Exists(FilePath(config_.datadir(), MSGID_DAT)));
}

TEST_F(FtnMsgDupeTest, Exists) {
  const auto now = daten_t_now();
  const auto last_message_id = now + 10000;
  ASSERT_TRUE(SetLastMessageId(last_message_id));
  ASSERT_TRUE(File::Exists(FilePath(config_.datadir(), MSGID_DAT)));
  FtnMessageDupe dupe(config_.datadir(), true);
  const FidoAddress a{"1:2/3"};
  const auto line = dupe.CreateMessageID(a);
  auto parts = SplitString(line, " ");
  ASSERT_EQ(2u, parts.size());
  const auto id = static_cast<unsigned int>(std::stoul(parts.at(1), nullptr, 16));

  EXPECT_EQ(id - last_message_id, 1u) << "id: " << id << "; last_message_id: " << last_message_id
                                      << "; delta: " << (id - last_message_id);
}

TEST_F(FtnMsgDupeTest, Smoke) {
  FtnMessageDupe dupe(config_.datadir(), false);
  dupe.add(1, 2);
  EXPECT_TRUE(dupe.is_dupe(1, 2));
  EXPECT_FALSE(dupe.is_dupe(2, 1));
}

TEST_F(FtnMsgDupeTest, Remove) {
  FtnMessageDupe dupe(config_.datadir(), false);
  dupe.add(1, 2);
  EXPECT_TRUE(dupe.is_dupe(1, 2));
  dupe.remove(1, 2);
  EXPECT_FALSE(dupe.is_dupe(1, 2));
}