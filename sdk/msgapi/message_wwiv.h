/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
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
#ifndef __INCLUDED_SDK_MESSAGE_WWIV_H__
#define __INCLUDED_SDK_MESSAGE_WWIV_H__

#include <cstdint>
#include <string>
#include <vector>

//#include "sdk/msgapi/msgapi.h"
#include "sdk/msgapi/message_area.h"
#include "sdk/msgapi/message_api.h"
#include "sdk/msgapi/message.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

class WWIVMessageHeader: public MessageHeader {
public:
  explicit WWIVMessageHeader(const MessageApi* api);

  // Should only be used when reading a message from a type-2 message file.
  // Needs to be public so make_unique will work on it.
  WWIVMessageHeader(postrec header, const std::string& from, const std::string& to,
    const std::string& in_reply_to, const MessageApi* api);

  virtual ~WWIVMessageHeader();

  std::string title() const override { return header_.title;  }
  void set_title(const std::string&) override;
  std::string to() const override { return to_.empty() ? to_ : "ALL";  }
  void set_to(const std::string& to) override { to_ = to;   }
  std::string from() const override { return from_; }
  void set_from(const std::string& f) override { from_ = f; }
  uint16_t from_usernum() const override { return header_.owneruser; }
  void set_from_usernum(uint16_t n) override { header_.owneruser = n; }
  uint16_t from_system() const override { return header_.ownersys; }
  void set_from_system(uint16_t n) override { header_.ownersys = n;  }
  uint32_t daten() const override { return header_.daten; }
  void set_daten(uint32_t d) override { header_.daten = d; }
  uint8_t status() const override { return header_.status; }   // TODO(rushfan): This should be generic
  void set_status(uint8_t t) override { header_.status = t; }
  uint8_t anony() const override { return header_.anony;  }      // TODO(rushfan): This should be generic
  void set_anony(uint8_t a) override { header_.anony = a; }
  std::string oaddress() const override { return oaddress_;  } // TODO(rushfan): Implement me!
  void set_oaddress(const std::string& a) override { oaddress_ = a;  }
  std::string destination_address() const override { return destination_address_;  }
  void set_destination_address(const std::string& a) override { destination_address_ = a; }
  std::string in_reply_to() const override { return in_reply_to_; }
  void set_in_reply_to(const std::string& t) override { in_reply_to_ = t; }

  bool local() const override;
  bool private_msg() const override { return false;  } // we don't support private subs
  void set_private_msg(bool b) override { private_ = b; }

  bool unvalidated() const override { return (header_.status & status_unvalidated) != 0; }
  void set_unvalidated(bool b) override;
  bool locked() const override { return (header_.status & status_no_delete) != 0; } // 
  void set_locked(bool b) override;
  bool deleted() const override { return (header_.status & status_delete) != 0; }
  void set_deleted(bool b) override;

  bool pending_network() const override { return (header_.status & status_pending_net) != 0; }
  void set_pending_network(bool b) override;
  bool source_verified() const override { return (header_.status & status_post_source_verified) != 0; }
  bool net_network_post() const override { return (header_.status & status_post_new_net) != 0; }


  const postrec& data() const { return header_;  }

  // Read only methods
  // In WWIV last_read == highest_read.
  uint32_t last_read() const override { return header_.qscan; }
  uint8_t storage_type() const override { return header_.msg.storage_type; }

  friend class WWIVMessageArea;

private:
  postrec header_ = {};
  std::string from_;
  std::string to_;
  std::string in_reply_to_;
  std::string oaddress_;
  std::string destination_address_;
  bool private_ = false;
  const MessageApi* api_;
};

class WWIVMessageText: public MessageText {
public:

  WWIVMessageText();
  explicit WWIVMessageText(const std::string& text);
  virtual ~WWIVMessageText();

  const std::string& text() const override;
  void set_text(const std::string&) override;

private:
  std::string text_;
};

class WWIVMessage: public Message {
public:
  WWIVMessage(std::unique_ptr<WWIVMessageHeader> header, 
    std::unique_ptr<WWIVMessageText> text);
  ~WWIVMessage();

  MessageHeader& header() const override { return *header_.get(); }
  MessageText& text() const override { return *text_.get(); }
  std::unique_ptr<MessageHeader> release_header() { return std::move(header_); }
  std::unique_ptr<MessageText> release_text() { return std::move(text_); }

private:
  std::unique_ptr<WWIVMessageHeader> header_;
  std::unique_ptr<WWIVMessageText> text_;
};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_MESSAGE_WWIV_H__
