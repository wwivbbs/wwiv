/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2016, WWIV Software Services             */
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
#include "sdk/msgapi/message_wwiv.h"

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "sdk/vardec.h"

using std::string;
using std::vector;

namespace wwiv {
namespace sdk {
namespace msgapi {

WWIVMessageHeader::WWIVMessageHeader(const MessageApi* api)
  : header_(postrec{}) {}

WWIVMessageHeader::WWIVMessageHeader(postrec header, const std::string& from, const std::string& to,
  const std::string& in_reply_to, const MessageApi* api)
  : header_(header), from_(from), to_(to), in_reply_to_(in_reply_to),
    api_(api) {}

WWIVMessageHeader::~WWIVMessageHeader() {}

bool WWIVMessageHeader::is_local() const {
  uint8_t net_num = header_.network.network_msg.net_number;
  if (net_num >= api_->network().size()) {
    // not a valid network number.
    return true;
  }
  if (header_.ownersys == 0) {
    // no network system
    return true;
  }
  int local_net = api_->network().at(net_num).sysnum;
  return local_net == header_.ownersys;
}

void WWIVMessageHeader::set_locked(bool b) {
  if (b) {
    header_.status |= status_no_delete;
  } else {
    header_.status &= ~status_no_delete;
  }
}

void WWIVMessageHeader::set_deleted(bool b) {
  if (b) {
    header_.status |= status_delete;
  } else {
    header_.status &= ~status_delete;
  }
}

void WWIVMessageHeader::set_title(std::string& t) {
  if (t.size() > 72) {
    t.resize(72);
  }
  strcpy(header_.title, t.c_str());
}

WWIVMessageText::WWIVMessageText(const std::string& text)
  : MessageText(), text_(text) {}

WWIVMessageText::~WWIVMessageText() {}

WWIVMessage::WWIVMessage(WWIVMessageHeader* header, WWIVMessageText* text)
  : Message(), header_(header), text_(text) {}

WWIVMessage::~WWIVMessage() {}


}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
