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

#include "core/strings.h"
#include "sdk/config.h"
#include "sdk/qscan.h"
#include "sdk_test/sdk_helper.h"
#include <vector>
#include "core/stl.h"

using namespace std;
using namespace wwiv::sdk;
using namespace wwiv::strings;

class QScanTest : public testing::Test {
public:
  QScanTest() : config_(helper.root()) {
    EXPECT_TRUE(config_.IsInitialized());
  }

  void SetUp() override {
  }

  SdkHelper helper;
  Config config_;
};

TEST_F(QScanTest, Smoke) {
  const size_t max_subs = 8u;
  const size_t max_dirs = 10u;
  uint32_t q[100];
  memset(q, 0, sizeof(q));
  *q = 999;
  RawUserQScan qscan(&q[0], calculate_qscan_length(max_subs, max_dirs), max_subs, max_dirs);
  ASSERT_EQ(999u, *qscan.qsc());
  ASSERT_EQ(max_subs, qscan.subs().size());
  ASSERT_EQ(max_dirs, qscan.dirs().size());
}

TEST_F(QScanTest, Dirs_Smoke) {
  const auto max_subs = 8;
  const auto max_dirs = 10;
  const auto qscn_len = calculate_qscan_length(max_subs, max_dirs);
  RawUserQScan qscan(qscn_len, max_subs, max_dirs);

  // Flip off all dirs (forcefully)
  qscan.qsc()[1] = 0;
  auto& x = qscan.dirs();
  ASSERT_FALSE(x.test(1));
  
  // Flip a bit on the dirs.
  x.set(1);
  x.set(3);
  ASSERT_TRUE(x.test(1));
  ASSERT_TRUE(x.test(3));

  ASSERT_EQ(10u, qscan.qsc()[1]);
  ASSERT_EQ(static_cast<unsigned>(max_dirs), x.size());
}

TEST_F(QScanTest, Subs_Smoke) {
  const auto max_subs = 8;
  const auto max_dirs = 10;
  const auto qscn_len = calculate_qscan_length(max_subs, max_dirs);
  RawUserQScan qscan(qscn_len, max_subs, max_dirs);

  // Flip off all subs (forcefully)
  qscan.qsc()[2] = 0;
  auto& x = qscan.subs();
  ASSERT_FALSE(x.test(1));

  // Flip a bit on the dirs.
  x.set(1);
  x.set(3);
  ASSERT_TRUE(x.test(1));
  ASSERT_TRUE(x.test(3));

  ASSERT_EQ(10u, qscan.qsc()[2]);

  ASSERT_EQ(max_subs, wwiv::stl::ssize(x));
}

TEST_F(QScanTest, BitSet) {
  uint32_t data[2] = {};
  qscan_bitset b(data, 64);

  b.set(32);
  ASSERT_EQ(1u, data[1]);
  ASSERT_TRUE(b.test(32));
  b.reset(32);
  ASSERT_FALSE(b.test(32));
  b.flip(32);
  ASSERT_TRUE(b.test(32));
}
