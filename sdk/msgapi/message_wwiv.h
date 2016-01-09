/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.0x                             */
/*             Copyright (C)2015-2016, WWIV Software Services             */
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

#include <string>
#include <vector>

#include "sdk/msgapi/msgapi.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

class WWIVMessageHeader: public MessageHeader {
public:
  explicit WWIVMessageHeader(const MessageApi* api);
  WWIVMessageHeader(postrec header, const std::string& from, const std::string& to,
    const std::string& in_reply_to, std::vector<std::string>& control_lines,
    const MessageApi* api);
  virtual ~WWIVMessageHeader();

  virtual std::string title() const override { return header_.title;  }
  virtual void set_title(std::string&) override;
  virtual std::string to() const override { return to_.empty() ? to_ : "ALL";  }
  virtual void set_to(std::string& to) override { to_ = to;   }
  virtual std::string from() const override { return from_; }
  virtual void set_from(std::string& f) override { from_ = f; }
  virtual uint16_t from_usernum() const override { return header_.owneruser; }
  virtual void set_from_usernum(uint16_t n) override { header_.owneruser = n; }
  virtual uint16_t from_system() const override { return header_.ownersys; }
  virtual void set_from_system(uint16_t n) override { header_.ownersys = n;  }
  virtual uint32_t daten() const override { return header_.daten; }
  virtual void set_daten(uint32_t d) override { header_.daten = d; }
  virtual uint8_t status() const override { return header_.status; }   // TODO(rushfan): This should be generic
  virtual void set_status(uint8_t t) override { header_.status = t; }
  virtual uint8_t anony() const override { return header_.anony;  }      // TODO(rushfan): This should be generic
  virtual void set_anony(uint8_t a) override { header_.anony = a; }
  virtual std::string oaddress() const override { return oaddress_;  } // TODO(rushfan): Implement me!
  virtual void set_oaddress(std::string& a) override { oaddress_ = a;  }
  virtual std::string destination_address() const override { return destination_address_;  }
  virtual void set_destination_address(std::string& a) override { destination_address_ = a; }
  virtual std::string in_reply_to() const override { return in_reply_to_; }
  virtual void set_in_reply_to(std::string& t) override { in_reply_to_ = t; }

  virtual bool is_local() const override;
  virtual bool is_private() const override { return false;  } // we don't support private subs
  virtual void set_private(bool b) override { private_ = b; }
  virtual bool is_locked() const override { return (header_.status & status_no_delete) != 0; } // 
  virtual void set_locked(bool b) override;
  virtual bool is_deleted() const override { return (header_.status & status_delete) != 0; }
  virtual void set_deleted(bool b) override;
  virtual const std::vector<std::string>& control_lines() const override { return control_lines_;  }
  virtual const void set_control_lines(std::vector<std::string>& control_lines) override;

  const postrec& data() const { return header_;  }
private:
  postrec header_;
  std::string from_;
  std::string to_;
  std::string in_reply_to_;
  std::string oaddress_;
  std::string destination_address_;
  bool private_ = false;
  std::vector<std::string> control_lines_;
  const MessageApi* api_;
};

class WWIVMessageText: public MessageText {
public:

  WWIVMessageText(const std::string& text);
  virtual ~WWIVMessageText();

  virtual std::string text() const override { return text_;  }

private:
  std::string text_;
};

class WWIVMessage: public Message {
public:
  WWIVMessage(WWIVMessageHeader* header, WWIVMessageText* text);
  ~WWIVMessage();

  virtual WWIVMessageHeader* header() const override { return header_.get(); }
  virtual WWIVMessageText* text() const override { return text_.get(); }
  WWIVMessageHeader* release_header() { return header_.release(); }
  WWIVMessageText* release_text() { return text_.release(); }

private:
  std::unique_ptr<WWIVMessageHeader> header_;
  std::unique_ptr<WWIVMessageText> text_;
};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_MESSAGE_WWIV_H__
