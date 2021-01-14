/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_SHORTMSG_H__
#define __INCLUDED_BBS_SHORTMSG_H__

#include <sstream>

#include "sdk/net/net.h"
#include "sdk/user.h"

class ssm {
public:
  explicit ssm(int un) : ssm(un, 0, nullptr) {}
  ssm(int un, int sn, const net_networks_rec* net) : un_(un), sn_(sn), net_(net) {}
  ~ssm();

  template <typename T> ssm& operator<<(T const& value) {
    stream_ << value;
    return *this;
  }

private:
  std::ostringstream stream_;
  const int un_;
  const int sn_{0};
  const net_networks_rec* net_;
};

void rsm(int nUserNum, wwiv::sdk::User* pUser, bool bAskToSaveMsgs);

// True if we've received a short message this session.
bool received_short_message();

// Set the state if a short message has been received this session.
void received_short_message(bool b);

#endif // __INCLUDED_BBS_SHORTMSG_H__
