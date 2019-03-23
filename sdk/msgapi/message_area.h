/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2019, WWIV Software Services             */
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
#ifndef __INCLUDED_SDK_MESSAGE_AREA_H__
#define __INCLUDED_SDK_MESSAGE_AREA_H__

#include <limits>
#include <memory>
#include <string>
#include <vector>

#include "core/wwivport.h"
#include "sdk/vardec.h"
#include "sdk/msgapi/message.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

enum class message_anonymous_t {
  anonymous_none, anonymous_allowed, anonymous_dear_abby, anonymous_forced, anonymous_real_names_only
};

class MessageAreaHeader {
public:
};

// Since message_api.h includes us, we must forward declare.
class MessageApi;

class MessageAreaLastRead {
public:
  MessageAreaLastRead(MessageApi* api);
  virtual ~MessageAreaLastRead();
  virtual uint32_t last_read(int user_number) = 0;
  virtual bool set_last_read(int user_number, uint32_t last_read, uint32_t highest_read) = 0;
  virtual bool Close() = 0;

protected:
  // Not owned.
  MessageApi* api_;
};

struct MessageAreaOptions {
  bool send_post_to_network{false};
};

class MessageArea {
public:
  MessageArea(MessageApi* api);
  virtual ~MessageArea();
  
  // Message Sub Specific Operations
  virtual bool Close() = 0;
  virtual bool Lock() = 0;
  virtual bool Unlock() = 0;
  virtual void ReadMessageAreaHeader(MessageAreaHeader& header) = 0;
  virtual void WriteMessageAreaHeader(const MessageAreaHeader& header) = 0;
  virtual int FindUserMessages(const std::string& user_name) = 0;
  virtual int number_of_messages() = 0;

  // message specific
  virtual std::unique_ptr<Message> ReadMessage(int message_number) = 0;
  virtual std::unique_ptr<MessageHeader> ReadMessageHeader(int message_number) = 0;
  virtual std::unique_ptr<MessageText> ReadMessageText(int message_number) = 0;
  virtual bool AddMessage(const Message& message, const MessageAreaOptions& options) = 0;
  virtual bool DeleteMessage(int message_number) = 0;
  /** Updates message_number to point to the */
  virtual bool ResyncMessage(int& message_number) = 0;
  virtual bool ResyncMessage(int& message_number, Message& message) = 0;

  /** Creates a new empty message for this area. */
  virtual std::unique_ptr<Message> CreateMessage() = 0;
  virtual bool Exists(daten_t d, const std::string& title, uint16_t from_system, uint16_t from_user) = 0;

  int max_messages() const { 
    if (max_messages_ == 0) {
      return std::numeric_limits<int>::max();
    }
    return max_messages_; 
  }
  void set_max_messages(int m) { max_messages_ = m; }

  uint8_t storage_type() const { return storage_type_; }
  void set_storage_type(uint8_t t) { storage_type_ = t; }

  virtual MessageAreaLastRead& last_read() const noexcept = 0;
  virtual message_anonymous_t anonymous_type() const noexcept = 0;

protected:
  static constexpr uint8_t DEFAULT_WWIV_STORAGE_TYPE = 2;

  // Not owned.
  MessageApi* api_;
  int max_messages_{std::numeric_limits<int>::max()};
  uint8_t storage_type_{DEFAULT_WWIV_STORAGE_TYPE};
};


}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv

#endif  // __INCLUDED_SDK_MESSAGE_AREA_H__
