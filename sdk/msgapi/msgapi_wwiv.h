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
#ifndef __INCLUDED_SDK_MSGAPI_WWIV_H__
#define __INCLUDED_SDK_MSGAPI_WWIV_H__

#include "sdk/msgapi/msgapi.h"

#include <memory>
#include <string>

namespace wwiv {
namespace sdk {
namespace msgapi {

class WWIVMessageAreaHeader : public MessageAreaHeader {

};

class WWIVMessageHeader : public MessageHeader {

};

class WWIVMessageText : public MessageText {
public:
  WWIVMessageText();
private:
  std::string text_;
};

class WWIVMessage : public Message {
public:
  WWIVMessage(MessageHeader* header, MessageText* text);
  ~WWIVMessage();

private:
  WWIVMessageHeader header_;
  WWIVMessageText text_;
};

class WWIVMessageApi;

class WWIVMessageArea : public MessageArea {
public:
  WWIVMessageArea(WWIVMessageApi* api);
  virtual ~WWIVMessageArea();
  
  // Message Area Specific Operations
  virtual bool Close() override;
  virtual bool Lock() override;
  virtual bool Unlock() override;
  virtual void ReadMessageAreaHeader(MessageAreaHeader& header) override;
  virtual void WriteMessageAreaHeader(const MessageAreaHeader& header) override;
  virtual int FindUserMessages(const std::string& user_name) override;
  virtual int number_of_messages() override;

  // message specific
  virtual WWIVMessage* ReadMessage(int message_number) override;
  virtual WWIVMessageHeader* ReadMessageHeader(int message_number) override;
  virtual WWIVMessageText*  ReadMessageText(int message_number) override;
  virtual bool AddMessage(const Message& message) override;
  virtual bool DeleteMessage(int message_number) override;
};

class WWIVMessageApi: public MessageApi {
public:
  WWIVMessageApi(const std::string& subs_directory,
    const std::string& messages_directory);
  virtual ~WWIVMessageApi();
  virtual bool Exist(const std::string& name) const override;
  virtual WWIVMessageArea* Create(const std::string& name) override;
  virtual bool Remove(const std::string& name) override;
  virtual WWIVMessageArea* Open(const std::string& name) override;

};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv

#endif  // __INCLUDED_SDK_MSGAPI_WWIV_H__
