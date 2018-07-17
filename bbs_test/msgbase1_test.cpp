/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*           Copyright (C)2014-2017, WWIV Software Services               */
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

#include "bbs/inmsg.h"
#include "bbs/keycodes.h"
#include "bbs/msgbase1.h"
#include "bbs_test/bbs_helper.h"
#include "core/datetime.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/fido/fido_address.h"

using std::string;
using namespace wwiv::strings;
using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;
using namespace wwiv::stl;

TEST(Msgbase1Test, Smoke) {}

TEST(Msgbase1Test, AddFtnMsgid_Smoke) {
  BbsHelper helper;
  helper.SetUp();

  MessageEditorData data{};
  const std::string msgid = "1:2/3 deadbeef";
  FidoAddress addr("1:2/3");
  data.text = "Hello World\r\n\004"
              "0MSGID 1:2/3 deadbeef\r\nHello World2\r\n";
  add_ftn_msgid(*a()->config(), addr, msgid, &data);
  
  // One one gone
  ASSERT_EQ(std::string::npos, data.text.find("MSGID 1:2/3 deadbeef"));
  // Reply line added right.
  ASSERT_NE(std::string::npos, data.text.find("REPLY: 1:2/3 deadbeef\r\n"));
}
