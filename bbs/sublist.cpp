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
/*                                                                        */
/**************************************************************************/
#include "bbs/sublist.h"

#include <algorithm>
#include <string>

#include "bbs/bbsovl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/mmkey.h"
#include "bbs/subacc.h"

#include "bbs/confutil.h"
#include "sdk/subxtr.h"
#include "core/stl.h"
#include "core/strings.h"

using std::max;
using namespace wwiv::stl;
using namespace wwiv::strings;

void old_sublist() {
  int oc = a()->GetCurrentConferenceMessageArea();
  int os = a()->current_user_sub().subnum;

  bool abort = false;
  int sn = 0;
  int en = size_int(a()->subconfs) - 1;
  if (okconf(a()->user())) {
    if (a()->uconfsub[1].confnum != -1) {
      bout.nl();
      bout << "|#2A)ll conferences, Q)uit, <space> for current conference: ";
      char ch = onek("Q A");
      bout.nl();
      switch (ch) {
      case ' ':
        sn = a()->GetCurrentConferenceMessageArea();
        en = a()->GetCurrentConferenceMessageArea();
        break;
      case 'Q':
        return;
      }
    }
  } else {
    oc = -1;
  }

  bout.nl();
  bout.bpla("|#9Sub-Conferences Available: ", &abort);
  bout.nl();
  int i = sn;
  while (i <= en && a()->uconfsub[i].confnum != -1 && !abort) {
    if (a()->uconfsub[1].confnum != -1 && okconf(a()->user())) {
      setuconf(ConferenceType::CONF_SUBS, i, -1);
      auto cn = stripcolors(a()->subconfs[a()->uconfsub[i].confnum].conf_name);
      auto s = StringPrintf("|#1%s %c|#0:|#2 %s", "Conference",
        a()->subconfs[a()->uconfsub[i].confnum].designator,
        cn.c_str());
      bout.bpla(s, &abort);
    }
    size_t i1 = 0;
    while ((i1 < a()->subs().subs().size()) && (a()->usub[i1].subnum != -1) && (!abort)) {
      auto s = StringPrintf("  |#5%4.4s|#2", a()->usub[i1].keys);
      if (a()->context().qsc_q[a()->usub[i1].subnum / 32] & (1L << (a()->usub[i1].subnum % 32))) {
        s += " - ";
      } else {
        s += "  ";
      }
      if (a()->current_net().sysnum || wwiv::stl::size_int(a()->net_networks.size()) > 1) {
        if (!a()->subs().sub(a()->usub[i1].subnum).nets.empty()) {
          const char *ss;
          if (a()->subs().sub(a()->usub[i1].subnum).nets.size() > 1) {
            ss = "Gated";
          } else {
            ss = stripcolors(a()->net_networks[a()->subs().sub(a()->usub[i1].subnum).nets[0].net_num].name);
          }

          char s1[80];
          if (a()->subs().sub(a()->usub[i1].subnum).anony & anony_val_net) {
            sprintf(s1, "|17|15[%-8.8s]|#9 ", ss);
          } else {
            sprintf(s1, "|17|15<%-8.8s>|#9 ", ss);
          }
          s += s1;
        } else {
          s += std::string(11, ' ');
        }
        s += "|#9";
      }
      s += stripcolors(a()->subs().sub(a()->usub[i1].subnum).name);
      bout.bpla(s, &abort);
      i1++;
    }
    i++;
    bout.nl();
    if (!okconf(a()->user())) {
      break;
    }
  }

  if (i == 0) {
    bout.bpla("|#6None.", &abort);
    bout.nl();
  }

  if (okconf(a()->user())) {
    setuconf(ConferenceType::CONF_SUBS, oc, os);
  }
}


void SubList() {
  int p,
      wc,  // color code
      msgIndex,  // message Index
      newTally; // new message tally
  char ch, s[81], s2[10], s3[81], sdf[130];
  bool next;

  int oldConf = a()->GetCurrentConferenceMessageArea();
  int oldSub = a()->current_user_sub().subnum;
  int sn = 0;  // current sub number
  size_t en = std::max<size_t>(0, a()->subconfs.size() - 1);

  if (okconf(a()->user())) {
    if (a()->uconfsub[1].confnum != -1) {
      bout.nl();
      bout << "|#2A)ll conferences, Q)uit, <space> for current conference: ";
      ch = onek("Q A");
      bout.nl();
      switch (ch) {
      case ' ':
        sn = a()->GetCurrentConferenceMessageArea();
        en = a()->GetCurrentConferenceMessageArea();
        break;
      case 'Q':
        return;
      }
    }
  } else {
    oldConf = -1;
  }

  bool abort = false;
  bool done = false;
  do {
    p = 1;
    size_t i = sn;
    size_t i1 = 0;
    while (i <= en && a()->uconfsub[i].confnum != -1 && !abort) {
      int ns = 0;
      if (a()->uconfsub[1].confnum != -1 && okconf(a()->user())) {
        setuconf(ConferenceType::CONF_SUBS, i, -1);
        i1 = 0;
      }
      size_t firstp = 0;
      while (i1 < a()->subs().subs().size() && a()->usub[i1].subnum != -1 && !abort) {
        if (p) {
          p = 0;
          firstp = i1;
          bout.cls();
          if (a()->uconfsub[1].confnum != -1 && okconf(a()->user())) {
            sprintf(s, "Conference %c: %s",
              a()->subconfs[a()->uconfsub[i].confnum].designator,
              stripcolors(a()->subconfs[a()->uconfsub[i].confnum].conf_name.c_str()));
          } else {
            sprintf(s, "%s Message Areas", a()->config()->system_name().c_str());
          }
          bout.litebar(s);
          DisplayHorizontalBar(78, 7);
          bout << "|#2 Sub   Scan   Net/Local   Sub Name                                 Old   New\r\n";
          DisplayHorizontalBar(78, 7);
        }
        ++ns;
        sprintf(s, "    %-3.3s", a()->usub[i1].keys);
        if (a()->context().qsc_q[a()->usub[i1].subnum / 32] & (1L << (a()->usub[i1].subnum % 32))) {
          strcpy(s2, "|#5Yes");
        } else {
          strcpy(s2, "|#6No ");
        }
        iscan(i1);
        if (a()->current_net().sysnum || wwiv::stl::size_int(a()->net_networks.size()) > 1) {
          if (!a()->subs().sub(a()->usub[i1].subnum).nets.empty()) {
	          const char* ss;
            if (a()->subs().sub(a()->usub[i1].subnum).nets.size() > 1) {
              wc = 6;
              ss = "Gated";
            } else {
              strcpy(s3, a()->net_networks[a()->subs().sub(a()->usub[i1].subnum).nets[0].net_num].name);
              ss = stripcolors(s3);
              wc = a()->net_num() % 8;
            }
            if (a()->subs().sub(a()->usub[i1].subnum).anony & anony_val_net) {
              sprintf(s3, "|#7[|#%i%-8.8s|#7]", wc, ss);
            } else {
              sprintf(s3, "|#7<|#%i%-8.8s|#7>", wc, ss);
            }
          } else {
            strcpy(s3, " |#7>|#1LOCAL|#7<  ");
          }
        } else {
          strcpy(s3, "|#7>|#1LOCAL|#7<  ");
        }
        msgIndex = 1;
        while ((msgIndex <= a()->GetNumMessagesInCurrentMessageArea()) &&
               (get_post(msgIndex)->qscan <= a()->context().qsc_p[a()->usub[i1].subnum])) {
          ++msgIndex;
        }
        newTally = a()->GetNumMessagesInCurrentMessageArea() - msgIndex + 1;
        if (a()->current_user_sub().subnum == a()->usub[i1].subnum) {
          sprintf(sdf, " |#9%-3.3d |#9\xB3 %3s |#9\xB3 %6s |#9\xB3 |17|15%-36.36s |#9\xB3 |#9%5d |#9\xB3 |#%c%5u |#9",
                  i1 + 1, s2, s3, a()->subs().sub(a()->usub[i1].subnum).name.c_str(), 
                  a()->GetNumMessagesInCurrentMessageArea(),
                  newTally ? '6' : '3', newTally);
        } else {
          sprintf(sdf, " |#9%-3.3d |#9\xB3 %3s |#9\xB3 %6s |#9\xB3 |#1%-36.36s |#9\xB3 |#9%5d |#9\xB3 |#%c%5u |#9",
                  i1 + 1, s2, s3, a()->subs().sub(a()->usub[i1].subnum).name.c_str(),
                  a()->GetNumMessagesInCurrentMessageArea(),
                  newTally ? '6' : '3', newTally);
        }
        bout.bputs(sdf, &abort, &next);
        bout.nl();
        size_t lastp = i1++;
        if (bout.lines_listed() >= a()->screenlinest - 2) {
          p = 1;
          bout.clear_lines_listed();
          DisplayHorizontalBar(78, 7);
          bout.bprintf("|#1Select |#9[|#2%d-%d, [N]ext Page, [Q]uit|#9]|#0 : ", firstp + 1, lastp + 1);
          const std::string ss = mmkey(MMKeyAreaType::subs, true);
          if (isdigit(ss[0])) {
            for (uint16_t i2 = 0; i2 < a()->subs().subs().size(); i2++) {
              if (ss == a()->usub[i2].keys) {
                a()->set_current_user_sub_num(i2);
                oldSub = a()->current_user_sub().subnum;
                done = true;
                abort = true;
              }
            }
          } else {
            switch (ss[0]) {
            case 'Q': {
              if (okconf(a()->user())) {
                setuconf(ConferenceType::CONF_SUBS, oldConf, oldSub);
              }
              done = true;
              abort = true;
            }
            break;
            default:
              bout.backline();
              break;
            }
          }
        }
      }
      if (ns) {
        i++;
      }

      if (!abort) {
        p = 1;
        DisplayHorizontalBar(78, 7);
        if (okconf(a()->user())) {
          if (a()->uconfsub[1].confnum != -1) {
            bout.bprintf("|#1Select |#9[|#21-%d, J=Join Conference, ?=List Again, Q=Quit|#9]|#0 : ", ns);
          } else {
            bout.bprintf("|#1Select |#9[|#21-%d, ?=List Again, Q=Quit|#9]|#0 : ", ns);
          }
        } else {
          bout.bprintf("|#1Select |#9[|#21-%d, ?=List Again, Q=Quit|#9]|#0 : ", ns);
        }
        const std::string ss = mmkey(MMKeyAreaType::subs, true);

        if (ss == "?") {
          p = 1;
          ns = i = i1 = 0;
        }

        if (ss == " " || ss == "Q" || ss == "\r") {
          bout.nl(2);
          done = true;
          if (!okconf(a()->user())) {
            abort = true;
          }
        }
        if (ss == "J") {
          if (okconf(a()->user())) {
            jump_conf(ConferenceType::CONF_SUBS);
          }
          sn = en = oldConf = a()->GetCurrentConferenceMessageArea();
          ns = i = 0;
        }
        if (isdigit(ss.front())) {
          for (uint16_t i2 = 0; i2 < a()->subs().subs().size(); i2++) {
            if (ss == a()->usub[i2].keys) {
              a()->set_current_user_sub_num(i2);
              oldSub = a()->current_user_sub().subnum;
              done = true;
              abort = true;
            }
          }
        }
      } else {
        if (okconf(a()->user())) {
          setuconf(ConferenceType::CONF_SUBS, oldConf, oldSub);
        }
        done = true;
      }
    }
    if (i == 0) {
      bout.bpla("None.", &abort);
      bout.nl();
    }
  } while (!a()->hangup_ && !done);

  if (okconf(a()->user())) {
    setuconf(ConferenceType::CONF_SUBS, oldConf, oldSub);
  }
}


