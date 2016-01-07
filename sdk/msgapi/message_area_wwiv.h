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
#ifndef __INCLUDED_SDK_MESSAGE_AREA_WWIV_H__
#define __INCLUDED_SDK_MESSAGE_AREA_WWIV_H__

#include "core/file.h"
#include "sdk/msgapi/msgapi.h"
#include "sdk/msgapi/message_wwiv.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

class WWIVMessageApi;

class WWIVMessageAreaHeader: public MessageAreaHeader {

};

class WWIVMessageArea: public MessageArea {
public:
  WWIVMessageArea(WWIVMessageApi* api, const std::string& sub_filename, const std::string& text_filename);
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

private:
  File* OpenMessageFile(const std::string msgs_filename);
  void set_gat_section(File& file, size_t section);
  void save_gat(File& f);
  bool readfile(const messagerec* pMessageRecord, std::string msgs_filename, std::string* out);
  void savefile(const std::string& text, messagerec* pMessageRecord, const std::string& fileName);
  void remove_link(messagerec& msg, const std::string& fileName);

  const std::string sub_filename_;
  const std::string text_filename_;
  bool open_ = false;
  int last_num_messages_ = 0;

  // gat section used by wwiv message text files.
  int32_t gat_section = 0;
  std::unique_ptr<uint16_t[]> gat;
};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_MESSAGE_AREA_WWIV_H__
