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
#include <vector>
#include <set>
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

/** 
 * Note: This isn't used on disk 
 */
struct confrec {
  // A to Z?
  uint8_t designator;
  // Name of conference                                          
  std::string conf_name;
  // Minimum SL needed for access
  uint8_t minsl;
  // Maximum SL allowed for access
  uint8_t maxsl;
  // Minimum DSL needed for access
  uint8_t mindsl;
  // Maximum DSL allowed for acces
  uint8_t maxdsl;
  // Minimum age needed for access
  uint8_t minage;
  // Maximum age allowed for acces
  uint8_t maxage;
  // Gender: 0=male, 1=female 2=all
  uint8_t sex;
  // Bit-mapped stuff
  uint16_t status;
  // Minimum bps rate for access
  uint16_t minbps;
  // ARs necessary for access
  uint16_t ar;
  // DARs necessary for access
  uint16_t dar;
  // Num "subs" in this conference
  uint16_t num;
  // max num subs allocated in 'subs'
  uint16_t maxnum;
  // "Sub" numbers in the conference
  std::set<subconf_t> subs;
};

class conf_info_t {
public:
  conf_info_t(std::vector<confrec>& c, std::vector<userconfrec>& u)
    : confs(c), uc(u), num_confs(c.size()) {}
  
  std::vector<confrec>& confs;
  std::vector<userconfrec>& uc;
  int num_confs;
  std::string file_name;
  int num_subs_or_dirs = 0;
};

bool in_conference(subconf_t subnum, confrec* c);

void tmp_disable_conf(bool disable);
void reset_disable_conf();
conf_info_t get_conf_info(ConferenceType conftype);

void jump_conf(ConferenceType conftype);
void update_conf(ConferenceType conftype, subconf_t * sub1, subconf_t * sub2, int action);
char first_available_designator(ConferenceType conftype);
bool save_confs(ConferenceType conftype);
void addsubconf(ConferenceType conftype, confrec * c, subconf_t * which);
void conf_edit(ConferenceType conftype);
void list_confs(ConferenceType conftype, int ssc);
int  select_conf(const char *prompt_text, ConferenceType conftype, int listconfs);
// static
void read_in_conferences(ConferenceType conftype);
void read_all_conferences();
int wordcount(const std::string& instr, const char *delimstr);
std::string extractword(int ww, const std::string& instr, const char *delimstr);

#endif  // __INCLUDED_BBS_CONF_H__