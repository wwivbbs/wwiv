/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#ifndef __INCLUDED_BBS_CONF_H__
#define __INCLUDED_BBS_CONF_H__

#include <memory>
#include <string>
#include "core/transaction.h"
#include "sdk/vardec.h"

namespace wwiv {
namespace bbs {

class TempDisableConferences: public wwiv::core::Transaction {
public:
  TempDisableConferences();
};

}  // namespace bbs
}  // namespace wwiv

void tmp_disable_conf(bool disable);
void reset_disable_conf();
int  get_conf_info(int conftype, int *num, confrec ** cpp, char *file_name, int *num_s, userconfrec ** uc);
void jump_conf(int conftype);
void update_conf(int conftype, subconf_t * sub1, subconf_t * sub2, int action);
char first_available_designator(int conftype);
int  in_conference(int subnum, confrec * c);
void save_confs(int conftype, int whichnum, confrec * c);
void showsubconfs(int conftype, confrec * c);
void addsubconf(int conftype, confrec * c, subconf_t * which);
void delsubconf(int conftype, confrec * c, subconf_t * which);
void conf_edit(int conftype);
void list_confs(int conftype, int ssc);
int  select_conf(const char *prompt_text, int conftype, int listconfs);
void read_in_conferences(int conftype);
void read_all_conferences();
int get_num_conferences(const char *file_name);
int wordcount(const std::string& instr, const char *delimstr);
const char *extractword(int ww, const std::string& instr, const char *delimstr);
void sort_conf_str(char *conference_string);

#endif  // __INCLUDED_BBS_CONF_H__