/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2020, WWIV Software Services             */
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

#include "core/strings.h"
#include "sdk/vardec.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

using std::string;
using std::vector;
using namespace wwiv::strings;

namespace wwiv::sdk::msgapi {

WWIVMessageHeader::WWIVMessageHeader(const MessageApi* api)
  : header_(postrec{}), api_(api) {}

WWIVMessageHeader::WWIVMessageHeader(postrec header, std::string from, std::string to,
                                     std::string in_reply_to, const MessageApi* api)
    : header_(header), from_(std::move(from)), to_(std::move(to)),
      in_reply_to_(std::move(in_reply_to)), api_(api) {}

bool WWIVMessageHeader::local() const {
  const auto net_num = header_.network.network_msg.net_number;
  if (net_num >= api_->network().size()) {
    // not a valid network number.
    return true;
  }
  if (header_.ownersys == 0) {
    // no network system
    return true;
  }
  const int local_net = api_->network().at(net_num).sysnum;
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
  auto title = t;
  if (title.size() > 72) {
    title.resize(72);
  }
  to_char_array(header_.title, title);
}

WWIVMessageText::WWIVMessageText() : WWIVMessageText("") {}

WWIVMessageText::WWIVMessageText(std::string text) : text_(std::move(text)) {}

void WWIVMessageText::set_text(std::string text) { text_ = std::move(text); }

const std::string& WWIVMessageText::text() const {
  return text_;
}

WWIVMessage::WWIVMessage(std::unique_ptr<WWIVMessageHeader> header,
                         std::unique_ptr<WWIVMessageText> text)
    : header_(std::move(header)), text_(std::move(text)) {}


} // namespace wwiv
