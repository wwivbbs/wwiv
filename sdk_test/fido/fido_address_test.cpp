/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*                Copyright (C)2016, WWIV Software Services               */
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

#include <cstring>
#include <type_traits>

#include "sdk/fido/fido_address.h"

using std::cout;
using std::endl;
using std::is_standard_layout;
using std::string;

using namespace wwiv::sdk;
using namespace wwiv::sdk::fido;

TEST(FidoAddressTest, Basic) {
  FidoAddress f("11:10/211.123@wwiv-ftn");
  EXPECT_EQ(11, f.zone());
  EXPECT_EQ(10, f.net());
  EXPECT_EQ(211, f.node());
  EXPECT_EQ(123, f.point());
  EXPECT_EQ("wwiv-ftn", f.domain());

  EXPECT_EQ("11:10/211.123@wwiv-ftn", f.as_string(true));
}
