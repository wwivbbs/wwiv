/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*               Copyright (C)2014-2015 WWIV Software Services            */
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
#include "core/transaction.h"

#include <iostream>
#include <string>

using std::string;
using wwiv::core::Transaction;

TEST(TransactionTest, Commit) {
  bool committed = false;
  bool rolledback = false;
  auto c = [&] { committed = true; };
  auto r = [&] { rolledback = true; };
  {
    Transaction t(c, r);
    ASSERT_FALSE(rolledback);
    ASSERT_FALSE(committed);
  }
  ASSERT_TRUE(committed);
  ASSERT_FALSE(rolledback);
}

TEST(TransactionTest, Rollback) {
  bool committed = false;
  bool rolledback = false;
  auto c = [&] { committed = true; };
  auto r = [&] { rolledback = true; };
  {
    Transaction t(c, r);
    ASSERT_FALSE(rolledback);
    ASSERT_FALSE(committed);
    t.Rollback();
  }
  ASSERT_FALSE(committed);
  ASSERT_TRUE(rolledback);
}

TEST(TransactionTest, Rollback_RolebackNull) {
  bool committed = false;
  auto c = [&] { committed = true; };
  {
    Transaction t(c, nullptr);
    ASSERT_FALSE(committed);
    t.Rollback();
  }
  ASSERT_FALSE(committed);
}

TEST(TransactionTest, Commit_RollbackNull) {
  bool committed = false;
  auto c = [&] { committed = true; };
  {
    Transaction t(c, nullptr);
    ASSERT_FALSE(committed);
  }
  ASSERT_TRUE(committed);
}

TEST(TransactionTest, Commit_CommitNull) {
  bool rolledback = false;
  auto r = [&] { rolledback = true; };
  {
    Transaction t(nullptr, r);
    ASSERT_FALSE(rolledback);
  }
  ASSERT_FALSE(rolledback);
}
