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
#ifndef INCLUDED_SDK_MESSAGE_H
#define INCLUDED_SDK_MESSAGE_H

#include <memory>
#include <string>

namespace wwiv::sdk::msgapi {

class MessageHeader {
public:
  virtual ~MessageHeader() = default;

  [[nodiscard]] virtual std::string title() const = 0;
  virtual void set_title(const std::string&) = 0;
  [[nodiscard]] virtual std::string to() const = 0;
  virtual void set_to(const std::string&) = 0;
  [[nodiscard]] virtual std::string from() const = 0;
  virtual void set_from(const std::string&) = 0;
  [[nodiscard]] virtual uint16_t from_usernum() const = 0;
  virtual void set_from_usernum(uint16_t) = 0;
  [[nodiscard]] virtual uint16_t from_system() const = 0;
  virtual void set_from_system(uint16_t) = 0;
  [[nodiscard]] virtual uint32_t daten() const = 0;
  virtual void set_daten(uint32_t) = 0;
  [[nodiscard]] virtual uint8_t status() const = 0;
  virtual void set_status(uint8_t) = 0;
  [[nodiscard]] virtual uint8_t anony() const = 0;
  virtual void set_anony(uint8_t) = 0;
  [[nodiscard]] virtual std::string oaddress() const = 0;
  virtual void set_oaddress(const std::string&) = 0;
  [[nodiscard]] virtual std::string destination_address() const = 0;
  virtual void set_destination_address(const std::string&) = 0;
  [[nodiscard]] virtual std::string in_reply_to() const = 0;
  virtual void set_in_reply_to(const std::string&) = 0;

  [[nodiscard]] virtual bool local() const = 0;
  [[nodiscard]] virtual bool private_msg() const = 0;
  virtual void set_private_msg(bool b) = 0;

  [[nodiscard]] virtual bool unvalidated() const = 0;
  virtual void set_unvalidated(bool b) = 0;

  [[nodiscard]] virtual bool locked() const = 0;
  virtual void set_locked(bool b) = 0;
  [[nodiscard]] virtual bool deleted() const = 0;
  virtual void set_deleted(bool b) = 0;

  [[nodiscard]] virtual bool pending_network() const = 0;
  virtual void set_pending_network(bool b) = 0;
  [[nodiscard]] virtual bool source_verified() const = 0;
  [[nodiscard]] virtual bool net_network_post() const = 0;

  // Read only methods
  [[nodiscard]] virtual uint32_t last_read() const = 0;
  [[nodiscard]] virtual uint8_t storage_type() const = 0;


protected:
  MessageHeader() = default;
};

class MessageText {
public:
  virtual ~MessageText() = default;
  [[nodiscard]] virtual const std::string& text() const = 0;
  virtual void set_text(std::string) = 0;

protected:
  MessageText() = default;
};

class Message {
public:
  virtual ~Message() = default;
  [[nodiscard]] virtual MessageHeader& header() const = 0;
  [[nodiscard]] virtual MessageText& text() const = 0;
  virtual std::unique_ptr<MessageHeader> release_header() = 0;
  virtual std::unique_ptr<MessageText> release_text() = 0;

protected:
  Message() = default;
};

}  // namespace

#endif
