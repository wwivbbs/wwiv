/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
/*               Copyright (C)2015, WWIV Software Services                */
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
#ifndef __INCLUDED_SDK_MSGAPI_H__
#define __INCLUDED_SDK_MSGAPI_H__

#include <memory>
#include <string>

#include "sdk/msgapi/message_api.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

class MessageAreaHeader {

};

class MessageHeader {
public:
  virtual std::string title() = 0;
  virtual std::string to() = 0;
  virtual std::string from() = 0;
  virtual uint32_t daten() = 0;
  virtual uint8_t status() = 0;
  virtual uint8_t anony() = 0;
  virtual std::string oaddress() = 0;

  virtual std::string destination_address() = 0;

  virtual bool is_local() = 0;
  virtual bool is_private() = 0;
  virtual bool is_locked() = 0;
  virtual bool is_deleted() = 0;
};

class MessageText {
public:
  MessageText(const std::string& text): text_(text) {}
  virtual ~MessageText() {}

private:
  std::string text_;
};

class Message {
public:
  Message(MessageHeader* header, MessageText* text);
  ~Message();
};

class MessageApi;

class MessageArea {
public:
  MessageArea(MessageApi* api);
  virtual ~MessageArea();
  
  // Message Area Specific Operations
  virtual bool Close() = 0;
  virtual bool Lock() = 0;
  virtual bool Unlock() = 0;
  virtual void ReadMessageAreaHeader(MessageAreaHeader& header) = 0;
  virtual void WriteMessageAreaHeader(const MessageAreaHeader& header) = 0;
  virtual int FindUserMessages(const std::string& user_name) = 0;
  virtual int number_of_messages() =0;

  // message specific
  virtual Message* ReadMessage(int message_number) = 0;
  virtual MessageHeader* ReadMessageHeader(int message_number) = 0;
  virtual MessageText*  ReadMessageText(int message_number) = 0;
  virtual bool AddMessage(const Message& message) = 0;
  virtual bool DeleteMessage(int message_number) = 0;

protected:
  MessageApi* api_;
};


}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv

#endif  // __INCLUDED_SDK_MSGAPI_H__
