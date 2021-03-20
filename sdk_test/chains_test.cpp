/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2019-2021, WWIV Software Services           */
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

#include "sdk/chains.h"

using namespace wwiv::sdk;

TEST(ChainsTest, ExecMode_Operator) {
  auto t = chain_exec_mode_t::dos; 
  ASSERT_EQ(chain_exec_mode_t::dos, t++);
  ASSERT_EQ(chain_exec_mode_t::fossil, t++);
  ASSERT_EQ(chain_exec_mode_t::stdio, t++);
  ASSERT_EQ(chain_exec_mode_t::netfoss, t++);
  ASSERT_EQ(chain_exec_mode_t::none, t++);
  ASSERT_EQ(chain_exec_mode_t::dos, t);
}

TEST(ChainsTest, ExecDir_Operator) {
  auto t = chain_exec_dir_t::bbs;
  ASSERT_EQ(chain_exec_dir_t::bbs, t++);
  ASSERT_EQ(chain_exec_dir_t::temp, t++);
  ASSERT_EQ(chain_exec_dir_t::bbs, t);
}
