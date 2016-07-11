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
#ifndef __INCLUDED_SDK_EMAIL_WWIV_H__
#define __INCLUDED_SDK_EMAIL_WWIV_H__

#include <cstdint>
#include <string>
#include <vector>

#include "core/file.h"
#include "sdk/msgapi/message.h"
#include "sdk/msgapi/message_api.h"
#include "sdk/msgapi/message_wwiv.h"
#include "sdk/msgapi/type2_text.h"

namespace wwiv {
namespace sdk {
namespace msgapi {

class WWIVMessageApi;

class EmailData {
public:
  EmailData() {}
  ~EmailData() {}

  std::string title;
  std::string text;
  int anony = 0;
  int user_number = 0;
  int system_number = 0;
  int from_user = 0;
  int from_system = 0;
  int from_network_number = 0;
  uint32_t daten = 0;
};

class WWIVEmail: private Type2Text {
public:
  WWIVEmail(
      const std::string& data_filename, const std::string& text_filename,
      int max_net_num);
  virtual ~WWIVEmail();

  bool Close();

  bool AddMessage(const EmailData& data);

private:
  bool add_email(const mailrec& m);
  const std::string data_filename_;
  bool open_ = false;
  int last_num_messages_ = 0;
  const int max_net_num_;

  static constexpr uint8_t STORAGE_TYPE = 2;
};

}  // namespace msgapi
}  // namespace sdk
}  // namespace wwiv


#endif  // __INCLUDED_SDK_EMAIL_WWIV_H__
