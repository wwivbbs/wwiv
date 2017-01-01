/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "bbs/confutil.h"
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

typedef uint16_t subconf_t;

static constexpr int CONF_UPDATE_INSERT = 1;
static constexpr int CONF_UPDATE_DELETE = 2;
static constexpr int CONF_UPDATE_SWAP = 3;

struct confrec {
  unsigned char designator,                 // A to Z?
    name[61],                                // Name of conference
    minsl,                                   // Minimum SL needed for access
    maxsl,                                   // Maximum SL allowed for access
    mindsl,                                  // Minimum DSL needed for access
    maxdsl,                                  // Maximum DSL allowed for acces
    minage,                                  // Minimum age needed for access
    maxage,                                  // Maximum age allowed for acces
    sex;                                     // Gender: 0=male, 1=female 2=all
  subconf_t status,                      // Bit-mapped stuff
    minbps,                                  // Minimum bps rate for access
    ar,                                      // ARs necessary for access
    dar,                                     // DARs necessary for access
    num,                                     // Num "subs" in this conference
    maxnum,                                  // max num subs allocated in 'subs'
    *subs;                                    // "Sub" numbers in the conference
};

void tmp_disable_conf(bool disable);
void reset_disable_conf();
int  get_conf_info(ConferenceType conftype, int *num, confrec ** cpp, char *file_name, int *num_s, userconfrec ** uc);
void jump_conf(ConferenceType conftype);
void update_conf(ConferenceType conftype, subconf_t * sub1, subconf_t * sub2, int action);
char first_available_designator(ConferenceType conftype);
int  in_conference(int subnum, confrec * c);
void save_confs(ConferenceType conftype, int whichnum, confrec * c);
void showsubconfs(ConferenceType conftype, confrec * c);
void addsubconf(ConferenceType conftype, confrec * c, subconf_t * which);
void delsubconf(ConferenceType conftype, confrec * c, subconf_t * which);
void conf_edit(ConferenceType conftype);
void list_confs(ConferenceType conftype, int ssc);
int  select_conf(const char *prompt_text, ConferenceType conftype, int listconfs);
void read_in_conferences(ConferenceType conftype);
void read_all_conferences();
int get_num_conferences(const char *file_name);
int wordcount(const std::string& instr, const char *delimstr);
const char *extractword(int ww, const std::string& instr, const char *delimstr);
void sort_conf_str(char *conference_string);

#endif  // __INCLUDED_BBS_CONF_H__