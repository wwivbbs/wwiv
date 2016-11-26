/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*               Copyright (C)2014-2016 WWIV Software Services            */
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
#include "core/md5.h"

#include <map>
#include <iostream>
#include <string>
#include <vector>

using std::map;
using std::string;
using std::vector;

static string md5(const std::string& s) {
  MD5_CTX ctx;
  MD5_Init(&ctx);

  unsigned char hash[16];
  std::cerr << "s=" << s;
  MD5_Update(&ctx, (void*)s.c_str(), s.size());
  MD5_Final(hash, &ctx);

  std::ostringstream ss;
  for (int i = 0; i < 16; i++) {
    ss << std::setw(2) << std::setfill('0') << std::hex << static_cast<int>(hash[i]);
  }
  return ss.str();
}

TEST(Md5Test, Welcome) {
  EXPECT_EQ("f851256dff2a8825ad4af615111b6a4f", md5("WELCOME"));
}
