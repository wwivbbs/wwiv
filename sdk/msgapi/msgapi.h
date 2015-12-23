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
#include <vector>

#include "sdk/msgapi/message_api.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

class MessageAreaHeader {

};

class MessageHeader {
public:
  virtual std::string title() const = 0;
  virtual std::string to() const = 0;
  virtual std::string from() const = 0;
  virtual uint32_t daten() const = 0;
  virtual uint8_t status() const = 0;
  virtual uint8_t anony() const = 0;
  virtual std::string oaddress() const = 0;
  virtual std::string destination_address() const = 0;

  virtual bool is_local() const = 0;
  virtual bool is_private() const = 0;
  virtual bool is_locked() const = 0;
  virtual bool is_deleted() const = 0;
  virtual const std::vector<std::string>& control_lines() const = 0;
};

class MessageText {
public:
  MessageText() {}
  virtual ~MessageText() {}

  virtual std::string text() const = 0;
};

class Message {
public:
  Message();
  virtual ~Message();

  virtual MessageHeader* header() const = 0;
  virtual MessageText* text() const = 0;
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
  virtual int number_of_messages() = 0;

  // message specific
  virtual Message* ReadMessage(int message_number) = 0;
  virtual MessageHeader* ReadMessageHeader(int message_number) = 0;
  virtual MessageText* ReadMessageText(int message_number) = 0;
  virtual bool AddMessage(const Message& message) = 0;
  virtual bool DeleteMessage(int message_number) = 0;

protected:
  MessageApi* api_;
};


}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv

#endif  // __INCLUDED_SDK_MSGAPI_H__
