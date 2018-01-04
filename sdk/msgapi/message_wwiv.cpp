/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2017, WWIV Software Services             */
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
  : header_(postrec{}), api_(api) {}

WWIVMessageHeader::WWIVMessageHeader(postrec header, const std::string& from, const std::string& to,
  const std::string& in_reply_to, const MessageApi* api)
  : header_(header), from_(from), to_(to), in_reply_to_(in_reply_to),
    api_(api) {}

bool WWIVMessageHeader::local() const {
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

static void ToggleBit(uint8_t& word, uint8_t bit, bool b) {
  if (b) {
    word |= bit;
  }
  else {
    word &= ~bit;
  }
}

void WWIVMessageHeader::set_locked(bool b) {
  ToggleBit(header_.status, status_no_delete, b);
}

void WWIVMessageHeader::set_unvalidated(bool b) {
  ToggleBit(header_.status, status_unvalidated, b);
}

void WWIVMessageHeader::set_deleted(bool b) {
  ToggleBit(header_.status, status_delete, b);
}

void WWIVMessageHeader::set_pending_network(bool b) {
  ToggleBit(header_.status, status_pending_net, b);
}

void WWIVMessageHeader::set_title(const std::string& t) {
  string title = t;
  if (title.size() > 72) {
    title.resize(72);
  }
  strcpy(header_.title, title.c_str());
}

WWIVMessageText::WWIVMessageText()
  : WWIVMessageText("") {}

WWIVMessageText::WWIVMessageText(const std::string& text)
  : MessageText(), text_(text) {}

void WWIVMessageText::set_text(const std::string& text) {
  text_ = text;
}

const std::string& WWIVMessageText::text() const {
  return text_;
}

WWIVMessage::WWIVMessage(std::unique_ptr<WWIVMessageHeader> header,
  std::unique_ptr<WWIVMessageText> text)
  : Message(), header_(std::move(header)), text_(std::move(text)) {}


}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
