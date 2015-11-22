/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*                Copyright (C)2015 WWIV Software Services                */
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
/**************************************************************************/
#include "gtest/gtest.h"
#include "core/strings.h"
#include "core_test/file_helper.h"
#include "networkb/contact.h"

#include <cstdint>
#include <string>

using std::string;
using namespace wwiv::net;
using namespace wwiv::strings;

class ContactTest : public testing::Test {
protected:
  ContactTest() {
    now = time(nullptr);
    then = now - 100;
  }
  net_contact_rec c1{1};
  net_contact_rec c2{2};
  time_t now;
  time_t then;
};

TEST_F(ContactTest, SimpleCase) {
  Contact c({ c1, c2 });
  net_contact_rec* ncr1 = c.contact_rec_for(1);

  c.add_connect(1, then, 100, 200);

  EXPECT_EQ(then, ncr1->lastcontact);
  EXPECT_EQ(then, ncr1->lastcontactsent);
  EXPECT_EQ(then, ncr1->lasttry);
  EXPECT_EQ(1, ncr1->numcontacts);
  EXPECT_EQ(0, ncr1->numfails);
  EXPECT_EQ(100, ncr1->bytes_sent);
  EXPECT_EQ(200, ncr1->bytes_received);
  EXPECT_EQ(0, ncr1->bytes_waiting);
}

TEST_F(ContactTest, MultipleConnects) {
  Contact c({ c1, c2 });
  net_contact_rec* ncr1 = c.contact_rec_for(1);

  c.add_connect(1, then, 100, 200);
  c.add_connect(1, now, 200, 300);

  EXPECT_EQ(now, ncr1->lastcontact);
  EXPECT_EQ(now, ncr1->lastcontactsent);
  EXPECT_EQ(now, ncr1->lasttry);
  EXPECT_EQ(then, ncr1->firstcontact);
  EXPECT_EQ(2, ncr1->numcontacts);
  EXPECT_EQ(0, ncr1->numfails);
  EXPECT_EQ(300, ncr1->bytes_sent);
  EXPECT_EQ(500, ncr1->bytes_received);
  EXPECT_EQ(0, ncr1->bytes_waiting);
}

TEST_F(ContactTest, WithFailure) {
  Contact c({ c1, c2 });
  net_contact_rec* ncr1 = c.contact_rec_for(1);

  c.add_connect(1, then, 100, 200);
  c.add_failure(1, now);

  EXPECT_EQ(now, ncr1->lastcontact);
  EXPECT_EQ(now, ncr1->lasttry);
  EXPECT_EQ(then, ncr1->lastcontactsent);
  EXPECT_EQ(then, ncr1->firstcontact);
  EXPECT_EQ(2, ncr1->numcontacts);
  EXPECT_EQ(1, ncr1->numfails);
  EXPECT_EQ(100, ncr1->bytes_sent);
  EXPECT_EQ(200, ncr1->bytes_received);
  EXPECT_EQ(0, ncr1->bytes_waiting);
}

TEST_F(ContactTest, EnsureBytesWaitingClears) {
  Contact c({ c1, c2 });
  net_contact_rec* ncr1 = c.contact_rec_for(1);

  ncr1->bytes_waiting = 100;
  c.add_connect(1, then, 100, 200);
  EXPECT_EQ(0, ncr1->bytes_waiting);

  ncr1->bytes_waiting = 100;
  c.add_failure(1, now);
  EXPECT_EQ(100, ncr1->bytes_waiting);

  c.add_connect(1, now+1, 100, 200);
  EXPECT_EQ(0, ncr1->bytes_waiting);
}
