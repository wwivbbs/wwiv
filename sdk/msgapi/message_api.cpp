/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2021, WWIV Software Services             */
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
#include "sdk/msgapi/message_api.h"

#include "core/log.h"
#include <string>
#include <utility>

using namespace wwiv::sdk::net;

namespace wwiv::sdk::msgapi {

MessageAreaLastRead::MessageAreaLastRead(MessageApi* api) : api_(api) {}
MessageAreaLastRead::~MessageAreaLastRead() = default;

MessageArea::MessageArea(MessageApi* api) : api_(api) {}
MessageArea::~MessageArea() = default;

int MessageArea::max_messages() const {
  if (max_messages_ == 0) {
    return std::numeric_limits<int>::max();
  }
  return max_messages_;
}

MessageApi::MessageApi(const MessageApiOptions& options, std::string root_directory,
                       std::string subs_directory, std::string messages_directory,
                       std::vector<Network> net_networks)
    : options_(options), root_directory_(std::move(root_directory)),
      subs_directory_(std::move(subs_directory)),
      messages_directory_(std::move(messages_directory)), net_networks_(std::move(net_networks)) {}

MessageArea* MessageApi::CreateOrOpen(const wwiv::sdk::subboard_t& sub, int subnum) {
  if (!Exist(sub)) {
    LOG(INFO) << "Message area: '" << sub.filename << "' does not exist. Attempting to create it.";
    // Since the area does not exist, let's create it automatically like WWIV always does.
    if (!Create(sub, -1)) {
      LOG(ERROR) << "Failed to create area: " << sub.filename << " let's try to open it anyway.";
    }
  }
  return Open(sub, subnum);
}

} // namespace wwiv::sdk::msgapi
