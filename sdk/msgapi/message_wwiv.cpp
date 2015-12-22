/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2015, WWIV Software Services                */
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

WWIVMessageHeader::WWIVMessageHeader(postrec header, const std::string& from, const std::string& to,
  const std::string& date, const std::string& in_reply_to, std::vector<string>& control_lines) 
  : header_(header), from_(from), to_(to), date_(date), in_reply_to_(in_reply_to),
    control_lines_(control_lines) {

}

WWIVMessageHeader::~WWIVMessageHeader() {

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
