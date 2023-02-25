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
#ifndef INCLUDED_SDK_MESSAGE_H
#define INCLUDED_SDK_MESSAGE_H

#include "sdk/vardec.h"
#include <memory>
#include <string>


namespace wwiv::sdk::msgapi {

class MessageApi;

class MessageHeader {
public:
  explicit MessageHeader(const MessageApi* api);

  // Should only be used when reading a message from a type-2 message file.
  // Needs to be public so make_unique will work on it.
  MessageHeader(const postrec& header, std::string from, std::string to,
    std::string in_reply_to, const MessageApi* api);

  [[nodiscard]] std::string title() const { return header_.title; }
  void set_title(const std::string&);
  [[nodiscard]] std::string to() const { return to_.empty() ? "All" : to_; }
  void set_to(const std::string& to) { to_ = to; }
  [[nodiscard]] std::string from() const { return from_; }
  void set_from(const std::string& f) { from_ = f; }
  [[nodiscard]] uint16_t from_usernum() const { return header_.owneruser; }
  void set_from_usernum(uint16_t n) { header_.owneruser = n; }
  [[nodiscard]] uint16_t from_system() const { return header_.ownersys; }
  void set_from_system(uint16_t n) { header_.ownersys = n; }
  [[nodiscard]] daten_t daten() const { return header_.daten; }
  void set_daten(daten_t d) { header_.daten = d; }
  [[nodiscard]] uint8_t status() const { return header_.status; }   // TODO(rushfan): This should be generic
  void set_status(uint8_t t) { header_.status = t; }
  [[nodiscard]] uint8_t anony() const { return header_.anony; }      // TODO(rushfan): This should be generic
  void set_anony(uint8_t a) { header_.anony = a; }
  [[nodiscard]] std::string oaddress() const { return oaddress_; } // TODO(rushfan): Implement me!
  void set_oaddress(const std::string& a) { oaddress_ = a; }
  [[nodiscard]] std::string destination_address() const { return destination_address_; }
  void set_destination_address(const std::string& a) { destination_address_ = a; }
  [[nodiscard]] std::string in_reply_to() const { return in_reply_to_; }
  void set_in_reply_to(const std::string& t) { in_reply_to_ = t; }

  [[nodiscard]] bool local() const;
  [[nodiscard]] bool private_msg() const { return false; } // we don't support private subs
  void set_private_msg(bool b) { private_ = b; }

  [[nodiscard]] bool unvalidated() const;
  void set_unvalidated(bool b);
  [[nodiscard]] bool locked() const; // 
  void set_locked(bool b);
  [[nodiscard]] bool deleted() const;
  void set_deleted(bool b);

  [[nodiscard]] bool pending_network() const;
  void set_pending_network(bool b);
  [[nodiscard]] bool source_verified() const;
  [[nodiscard]] bool net_network_post() const;

  [[nodiscard]] const postrec& data() const { return header_; }

  // Read only methods
  // In WWIV last_read == highest_read.
  [[nodiscard]] uint32_t last_read() const;
  [[nodiscard]] uint8_t storage_type() const;

  friend class WWIVMessageArea;

private:
  postrec header_{};
  std::string from_;
  std::string to_;
  std::string in_reply_to_;
  std::string oaddress_;
  std::string destination_address_;
  bool private_{ false };
  const MessageApi* api_;
};

class MessageText {
public:
  MessageText() = default;
  explicit MessageText(const std::string& text) : text_(text) {}
  virtual ~MessageText() = default;
  [[nodiscard]] virtual const std::string& string() const { return text_; }
  virtual void set_text(const std::string& t) { text_ = t; };

private:
  std::string text_;
};

class Message {
public:
  Message(MessageHeader&& header, MessageText&& text);
  Message(MessageHeader&& header, const std::string& text);
  Message(MessageApi* api) : header_(api) {}
  virtual ~Message() = default;
  [[nodiscard]] virtual MessageHeader& header() { return header_; }
  [[nodiscard]] virtual const MessageHeader& header() const { return header_; }
  [[nodiscard]] virtual MessageText& text() { return text_; };
  virtual void set_text(const std::string& t) { text_.set_text(t); };
  [[nodiscard]] virtual const MessageText& text() const { return text_; };
private:
  MessageHeader header_;
  MessageText text_;
};

}  // namespace

#endif
