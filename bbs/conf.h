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

#include "core/stl.h"
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
  conf_info_t(std::vector<wwiv::sdk::confrec_430_t>& c, std::vector<wwiv::sdk::userconfrec>& u)
      : confs(c), uc(u), num_confs(wwiv::stl::size_int32(c)) {}

  std::vector<wwiv::sdk::confrec_430_t>& confs;
  std::vector<wwiv::sdk::userconfrec>& uc;
  int num_confs;
  std::string file_name;
  int num_subs_or_dirs = 0;
};

bool in_conference(wwiv::sdk::subconf_t subnum, wwiv::sdk::confrec_430_t* c);

void tmp_disable_conf(bool disable);
void reset_disable_conf();
conf_info_t get_conf_info(wwiv::sdk::ConferenceType conftype);

void jump_conf(wwiv::sdk::ConferenceType conftype);
void update_conf(wwiv::sdk::ConferenceType conftype, wwiv::sdk::subconf_t * sub1, wwiv::sdk::subconf_t * sub2, int action);
char first_available_designator(wwiv::sdk::ConferenceType conftype);
bool save_confs(wwiv::sdk::ConferenceType conftype);
void addsubconf(wwiv::sdk::ConferenceType conftype, wwiv::sdk::confrec_430_t * c, wwiv::sdk::subconf_t * which);
void conf_edit(wwiv::sdk::ConferenceType conftype);
void list_confs(wwiv::sdk::ConferenceType conftype, int ssc);
int  select_conf(const char *prompt_text, wwiv::sdk::ConferenceType conftype, int listconfs);

void read_in_conferences(wwiv::sdk::ConferenceType conftype);
void read_all_conferences();
int wordcount(const std::string& instr, const char *delimstr);
std::string extractword(int ww, const std::string& instr, const char *delimstr);

#endif
