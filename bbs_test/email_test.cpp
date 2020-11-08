/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*              Copyright (C)2014-2020, WWIV Software Services            */
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

#include "bbs/email.h"


TEST(EmailTest, Smoke) {
  EXPECT_EQ("1", fixup_user_entered_email("1"));
  EXPECT_EQ("1@1", fixup_user_entered_email("1@1"));
  EXPECT_EQ("1@1.WWIVNET", fixup_user_entered_email("1@1.WWIVNET"));

  EXPECT_EQ("rushfan@wwiv.us @32767", fixup_user_entered_email("rushfan@wwiv.us @32767"));
  EXPECT_EQ("rushfan@wwiv.us @32767", fixup_user_entered_email("rushfan@wwiv.us"));
  EXPECT_EQ("rushfan (1:1/100) @32765", fixup_user_entered_email("rushfan (1:1/100)"));
  EXPECT_EQ("rushfan (1:1/100) @32765", fixup_user_entered_email("rushfan (1:1/100) @32765"));
}
