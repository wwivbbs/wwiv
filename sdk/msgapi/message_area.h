/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*             Copyright (C)2015-2022, WWIV Software Services             */
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
#ifndef INCLUDED_SDK_MESSAGE_AREA_H
#define INCLUDED_SDK_MESSAGE_AREA_H

#include "core/wwivport.h"
#include "sdk/msgapi/message.h"
#include <limits>
#include <memory>
#include <optional>
#include <string>

namespace wwiv::sdk::msgapi {

class Message;
class MessageHeader;
class MessageText;

enum class message_anonymous_t {
  anonymous_none,
  anonymous_allowed,
  anonymous_dear_abby,
  anonymous_forced,
  anonymous_real_names_only
};

class MessageAreaHeader {};

// Since message_api.h includes us, we must forward declare.
class MessageApi;

class MessageAreaLastRead {
public:
  MessageAreaLastRead(MessageApi* api, int message_area_number) :
    api_(api), message_area_number_(message_area_number) {}
  virtual ~MessageAreaLastRead() = default;

  [[nodiscard]] uint32_t last_read(int user_number);
  bool set_last_read(int user_number, uint32_t last_read, uint32_t highest_read);
  bool Close();

private:
  MessageApi* api_;
  int message_area_number_;
};

struct MessageAreaOptions {
  /** Should the post also be sent to the network automatically */
  bool send_post_to_network{false};

  /**
   * Should a RE and BY line be added automatically.  Note tht this
   * will only happen if there is no FTN network for the sub, since
   * the FidoAddr kludge is added in that case instead.
   */
  bool add_re_and_by_line{true};
};

class MessageArea {
public:
  explicit MessageArea(MessageApi* api);
  virtual ~MessageArea();
  
  // Message Sub Specific Operations
  virtual bool Close() = 0;
  virtual bool Lock() = 0;
  virtual bool Unlock() = 0;
  [[nodiscard]] virtual std::unique_ptr<MessageAreaHeader> ReadMessageAreaHeader() = 0;
  virtual void WriteMessageAreaHeader(const MessageAreaHeader& header) = 0;
  [[nodiscard]] virtual int FindUserMessages(const std::string& user_name) = 0;
  [[nodiscard]] virtual int number_of_messages() = 0;

  // message specific
  [[nodiscard]] virtual std::optional<Message> ReadMessage(int message_number) = 0;
  [[nodiscard]] virtual std::optional<MessageHeader> ReadMessageHeader(int message_number) = 0;
  [[nodiscard]] virtual std::optional<MessageText> ReadMessageText(int message_number) = 0;
  [[nodiscard]] virtual bool AddMessage(Message& message, const MessageAreaOptions& options) = 0;
  [[nodiscard]] virtual bool DeleteMessage(int message_number) = 0;
  /** Updates message_number to point to the */
  virtual bool ResyncMessage(int& message_number) = 0;
  virtual bool ResyncMessage(int& message_number, Message& message) = 0;

  /** Creates a new empty message for this area. */
  [[nodiscard]] virtual Message CreateMessage() = 0;
  [[nodiscard]] virtual bool Exists(daten_t d, const std::string& title, uint16_t from_system, uint16_t from_user) = 0;

  [[nodiscard]] int max_messages() const;
  void set_max_messages(int m) { max_messages_ = m; }

  [[nodiscard]] uint8_t storage_type() const { return storage_type_; }
  void set_storage_type(uint8_t t) { storage_type_ = t; }

  [[nodiscard]] virtual const MessageAreaLastRead& last_read() const noexcept = 0;
  [[nodiscard]] virtual message_anonymous_t anonymous_type() const noexcept = 0;

protected:
  static constexpr uint8_t DEFAULT_WWIV_STORAGE_TYPE = 2;

  // Not owned.
  MessageApi* api_;
  int max_messages_{std::numeric_limits<int>::max()};
  uint8_t storage_type_{DEFAULT_WWIV_STORAGE_TYPE};
};


}  // namespace

#endif
