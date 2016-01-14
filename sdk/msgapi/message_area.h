/**************************************************************************/
/*                                                                        */
/*                          WWIV Version 5.x                              */
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
#ifndef __INCLUDED_SDK_MESSAGE_AREA_H__
#define __INCLUDED_SDK_MESSAGE_AREA_H__

#include <memory>
#include <string>
#include <vector>

#include "sdk/vardec.h"
#include "sdk/msgapi/message.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

class MessageAreaHeader {
public:
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

  /** Creates a new empty message for this area. */
  virtual Message* CreateMessage() = 0;

protected:
  MessageApi* api_;
};


}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv

#endif  // __INCLUDED_SDK_MESSAGE_AREA_H__
