/**************************************************************************/
/*                                                                        */
/*                            WWIV Version 5                              */
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
#ifndef INCLUDED_SDK_MESSAGE_AREA_WWIV_H
#define INCLUDED_SDK_MESSAGE_AREA_WWIV_H

#include "sdk/msgapi/message.h"
#include "sdk/msgapi/message_api.h"
#include "sdk/msgapi/message_wwiv.h"
#include "sdk/msgapi/type2_text.h"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace wwiv::sdk::msgapi {

class WWIVMessageApi;

class WWIVMessageAreaHeader : public MessageAreaHeader {
public:
  explicit WWIVMessageAreaHeader(subfile_header_t& header) : header_(header) {}
  WWIVMessageAreaHeader(int ver, uint32_t num_messages);

  [[nodiscard]] const subfile_header_t& header() const { return header_; }
  [[nodiscard]] uint16_t active_message_count() const { return header_.active_message_count; }
  void set_active_message_count(uint16_t active_message_count) {
    header_.active_message_count = active_message_count;
  }
  uint16_t increment_active_message_count() { return ++header_.active_message_count; }
  [[nodiscard]] bool initialized() const { return initialized_; }
  void set_initialized(bool initialized) { initialized_ = initialized; }

  [[nodiscard]] subfile_header_t raw_header() const { return header_; }

private:
  subfile_header_t header_{};
  bool initialized_{true};
};

class WWIVMessageAreaLastRead : public MessageAreaLastRead {
public:
  WWIVMessageAreaLastRead(WWIVMessageApi* api, int message_area_number);

  [[nodiscard]] uint32_t last_read(int user_number) override;
  bool set_last_read(int user_number, uint32_t last_read, uint32_t highest_read) override;
  bool Close() override;

private:
  WWIVMessageApi* wapi_;
  int message_area_number_;
};

struct wwiv_parsed_text_fieds {
  std::string from_username;
  std::string date;
  std::string to;
  std::string in_reply_to;
  std::string text;
};

class WWIVMessageArea final : public MessageArea, Type2Text {
public:
  WWIVMessageArea(WWIVMessageApi* api, const subboard_t& sub, 
                  std::filesystem::path sub_filename,
                  std::filesystem::path text_filename, int subnum,
                  std::vector<net::Network> net_networks);
  ~WWIVMessageArea() override;

  // Message Sub Specific Operations
  bool Close() override;
  bool Lock() override;
  bool Unlock() override;
  std::unique_ptr<MessageAreaHeader> ReadMessageAreaHeader() override;
  // Note: This is not implemented on wwiv.
  void WriteMessageAreaHeader(const MessageAreaHeader& header) override;
  int FindUserMessages(const std::string& user_name) override;
  int number_of_messages() override;

  // message specific.
  // I would return a unique_ptr here but that doesn't work with
  // covariant return types for subclasses.
  std::unique_ptr<Message> ReadMessage(int message_number) override;
  std::unique_ptr<MessageHeader> ReadMessageHeader(int message_number) override;
  std::unique_ptr<MessageText> ReadMessageText(int message_number) override;
  bool AddMessage(const Message& message, const MessageAreaOptions& options) override;
  bool DeleteMessage(int message_number) override;
  bool ResyncMessage(int& message_number) override;
  bool ResyncMessage(int& message_number, Message& message) override;

  [[nodiscard]] std::unique_ptr<Message> CreateMessage() override;
  [[nodiscard]] bool Exists(daten_t d, const std::string& title, uint16_t from_system,
                            uint16_t from_user) override;
  [[nodiscard]] MessageAreaLastRead& last_read() const noexcept override;
  [[nodiscard]] message_anonymous_t anonymous_type() const noexcept override;

private:
  int DeleteExcess();
  [[nodiscard]] bool add_post(const postrec& post);
  [[nodiscard]] std::optional<wwiv_parsed_text_fieds> ParseMessageText(const postrec& header, int message_number);
  [[nodiscard]] [[nodiscard]] bool HasSubChanged() const;
  [[nodiscard]] bool ResyncMessageImpl(int& message_number, Message& message);

  static constexpr uint8_t STORAGE_TYPE = 2;

  // not owned.  Extra copy of the message API but the
  // WWIV subclass of it.
  WWIVMessageApi* wwiv_api_;
  // The currently opened sub if available.
  const subboard_t sub_;
  // Full path to the *.sub filename.
  const std::filesystem::path sub_filename_;
  bool open_{false};
  subfile_header_t header_;
  const std::vector<net::Network> net_networks_;
  std::unique_ptr<MessageAreaLastRead> last_read_;
  int nonce_{0};
};

} // namespace

#endif
