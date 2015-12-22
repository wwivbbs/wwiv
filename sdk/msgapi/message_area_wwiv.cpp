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
#include "sdk/msgapi/msgapi_wwiv.h"
#include "sdk/msgapi/message_api_wwiv.h"

#include <memory>
#include <string>
#include <utility>

#include "core/datafile.h"
#include "core/file.h"
#include "bbs/subacc.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

using wwiv::core::DataFile;

WWIVMessageArea::WWIVMessageArea(WWIVMessageApi* api, const std::string& sub_filename) 
  : MessageArea(api), sub_filename_(sub_filename) {
  DataFile<postrec> sub(sub_filename_);
  if (!sub) {
    // TODO: throw exception
  }
  postrec header;
  sub.Read(0, &header);
  last_num_messages_ = header.owneruser;
  open_ = true;
}

WWIVMessageArea::~WWIVMessageArea() {
  Close();
}

bool WWIVMessageArea::Close() {
  open_ = false;
  return true;
}

bool WWIVMessageArea::Lock() {
  return false;
}

bool WWIVMessageArea::Unlock() {
  return false;
}

void WWIVMessageArea::ReadMessageAreaHeader(MessageAreaHeader& header) {
}

void WWIVMessageArea::WriteMessageAreaHeader(const MessageAreaHeader & header) {}

int WWIVMessageArea::FindUserMessages(const std::string& user_name) {
  return 0;
}

int WWIVMessageArea::number_of_messages() {
  DataFile<postrec> sub(sub_filename_);
  if (!sub) {
    // TODO: throw exception
    return 0;
  }
  postrec header;
  sub.Read(0, &header);
  return header.owneruser;
}

WWIVMessage* WWIVMessageArea::ReadMessage(int message_number) {
  return nullptr;
}

WWIVMessageHeader* WWIVMessageArea::ReadMessageHeader(int message_number) {
  return nullptr;
}

WWIVMessageText* WWIVMessageArea::ReadMessageText(int message_number) {
  return nullptr;
}

bool WWIVMessageArea::AddMessage(const Message & message) {
  return false;
}

bool WWIVMessageArea::DeleteMessage(int message_number) {
  return false;
}


}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
