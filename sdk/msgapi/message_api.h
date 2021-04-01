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
#ifndef INCLUDED_SDK_MSGAPI_MESSAGE_API_H
#define INCLUDED_SDK_MSGAPI_MESSAGE_API_H

#include "sdk/msgapi/message_area.h"
#include "sdk/net/net.h"
#include "sdk/subxtr.h"
#include <stdexcept>
#include <string>
#include <vector>

namespace wwiv::sdk::msgapi {

class MessageArea;

enum class OverflowStrategy {
  delete_one, delete_all, delete_none
};

struct MessageApiOptions {
  OverflowStrategy overflow_strategy = wwiv::sdk::msgapi::OverflowStrategy::delete_one;
};

class bad_message_area : public ::std::runtime_error {
public:
  explicit bad_message_area(const ::std::string& sub_filename) : ::std::runtime_error(sub_filename) {}
};

class MessageApi {
public:
  virtual ~MessageApi() = default;
  MessageApi(
    const MessageApiOptions& options,
    std::string root_directory,
    std::string subs_directory,
    std::string messages_directory,
    std::vector<net::Network> net_networks);

  /** Checks to see if the files for a subboard exist. */
  [[nodiscard]] virtual bool Exist(const wwiv::sdk::subboard_t& sub) const = 0;
  /** Opens a message area, throwing bad_message_area if it does not exist. */
  [[nodiscard]] virtual bool Create(const wwiv::sdk::subboard_t& sub, int subnum) = 0;
  /** Opens a message area, throwing bad_message_area if it does not exist. */
  [[nodiscard]] virtual MessageArea* Open(const wwiv::sdk::subboard_t& sub, int subnum) = 0;
  /** Creates or Opens a message area. throwing bad_message_area if it exists but can not be opened.*/
  [[nodiscard]] virtual MessageArea* CreateOrOpen(const wwiv::sdk::subboard_t& sub, int subnum);
  /** Deletes the message area identified by filename: name */
  [[nodiscard]] virtual bool Remove(const std::string& name) = 0;

  [[nodiscard]] const std::vector<net::Network>& network() const { return net_networks_; }
  [[nodiscard]] std::string root_directory() const { return root_directory_; }
  [[nodiscard]] MessageApiOptions options() const { return options_; }

  // TODO(rushfan): Here's where we add hooks to the lastread system
  // so that message api's created inside the bbs will share *qsc with
  // the legacy code until it's all removed.
protected:
  const MessageApiOptions options_;

  std::string root_directory_;
  std::string subs_directory_;
  std::string messages_directory_;
  std::vector<net::Network> net_networks_;
};

}  // namespace

#endif
