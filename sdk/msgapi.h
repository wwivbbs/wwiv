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

namespace wwiv {
namespace sdk {
namespace msgapi {

class MessageAreaHeader {

};

class MessageHeader {

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

private:
  MessageHeader header_;
  MessageText text_;
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

class MessageApi {
public:
  MessageApi(const std::string& subs_directory,
    const std::string& messages_directory);
  virtual ~MessageApi();
  virtual bool Exist(const std::string& name) const = 0;
  virtual MessageArea* Create(const std::string& name) = 0;
  virtual bool Remove(const std::string& name) = 0;
  virtual MessageArea* Open(const std::string& name) = 0;

protected:
  std::string subs_directory_;
  std::string messages_directory_;
};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv

#endif  // __INCLUDED_SDK_MSGAPI_H__
