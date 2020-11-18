/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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
#ifndef INCLUDED_BBS_CONF_H
#define INCLUDED_BBS_CONF_H

#include "core/transaction.h"
#include "sdk/conf/conf.h"
#include <string>

namespace wwiv::bbs {
class TempDisableConferences: public core::Transaction {
public:
  TempDisableConferences();
};

}  // namespace

class conf_info_t {
public:
  conf_info_t(wwiv::sdk::Conference& c, std::vector<wwiv::sdk::conference_t>& u)
      : confs(c), uc(u), num_confs(c.size()) {}

  wwiv::sdk::Conference& confs;
  std::vector<wwiv::sdk::conference_t>& uc;
  int num_confs;
  std::string file_name;
  int num_subs_or_dirs = 0;
};

void tmp_disable_conf(bool disable);
void reset_disable_conf();
conf_info_t get_conf_info(wwiv::sdk::ConferenceType conftype);

void jump_conf(wwiv::sdk::ConferenceType conftype);
void conf_edit(wwiv::sdk::Conference& conf);
void list_confs(wwiv::sdk::ConferenceType conftype, bool list_subs);
void list_confs(wwiv::sdk::Conference& conf, bool list_subs = true);
std::optional<char> select_conf(const std::string& prompt_text, wwiv::sdk::Conference& conf,
                                bool listconfs);

/**
 * Select the conferences to which dirs and subs belong.
 */
void edit_conf_subs(wwiv::sdk::Conference& conf);

#endif
