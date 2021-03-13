/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_MSGBASE1_H
#define INCLUDED_BBS_MSGBASE1_H

#include "common/message_editor_data.h"
#include "sdk/subxtr.h"
#include <string>
#include <utility>

namespace wwiv {
namespace sdk {
class Config;
}
}

struct messagerec;
struct postrec;

namespace wwiv::sdk::fido {
  class FidoAddress;
}

class PostReplyToData {
public:
  // Original text this next post will reply to
  std::string text;
  // Original name of the sender we are replying to
  std::string name;
  // Original title this next post will reply to
  std::string title;
};

class PostData {
public:
  explicit PostData(PostReplyToData i) : reply_to(std::move(i)) {}
  PostData() : PostData(PostReplyToData()) {}
  PostReplyToData reply_to;
};

void send_net_post(postrec* p, const wwiv::sdk::subboard_t& sub);
void post(const PostData& data);
void add_ftn_msgid(const wwiv::sdk::Config& config, const wwiv::sdk::fido::FidoAddress& addr, const std::string& msgid,
                   wwiv::common::MessageEditorData* data);
std::string grab_user_name(messagerec* m, const std::string& file_name, int network_number);
void qscan(uint16_t start_subnum, bool& next_sub);
void nscan(uint16_t start_subnum = 0);
void ScanMessageTitles();
void remove_post();

#endif
