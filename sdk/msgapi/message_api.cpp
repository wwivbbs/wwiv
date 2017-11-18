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
#include "sdk/msgapi/message_api.h"

#include <memory>
#include <string>
#include <utility>

#include "core/log.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

  MessageAreaLastRead::MessageAreaLastRead(MessageApi* api) : api_(api) {}
MessageAreaLastRead::~MessageAreaLastRead() {}


MessageArea::MessageArea(MessageApi* api): api_(api) {}
MessageArea::~MessageArea() {}

MessageApi::MessageApi(
  const wwiv::sdk::msgapi::MessageApiOptions& options,
  const std::string& root_directory,
  const std::string& subs_directory,
  const std::string& messages_directory,
  const std::vector<net_networks_rec>& net_networks)
  : options_(options), root_directory_(root_directory), 
    subs_directory_(subs_directory), messages_directory_(messages_directory),
    net_networks_(net_networks) {}

MessageApi::~MessageApi() {}

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

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv
