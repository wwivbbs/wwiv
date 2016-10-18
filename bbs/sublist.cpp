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
/*                                                                        */
/**************************************************************************/

#include <algorithm>
#include <string>

#include "bbs/bbsovl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/bbs.h"
#include "bbs/fcns.h"
#include "bbs/mmkey.h"
#include "bbs/vars.h"
#include "bbs/confutil.h"
#include "sdk/subxtr.h"
#include "core/strings.h"

using std::max;
using namespace wwiv::strings;

void old_sublist() {
  char s[255];

  int oc = session()->GetCurrentConferenceMessageArea();
  int os = session()->current_user_sub().subnum;

  bool abort = false;
  int sn = 0;
  int en = subconfnum - 1;
  if (okconf(session()->user())) {
    if (uconfsub[1].confnum != -1) {
      bout.nl();
      bout << "|#2A)ll conferences, Q)uit, <space> for current conference: ";
      char ch = onek("Q A");
      bout.nl();
      switch (ch) {
      case ' ':
        sn = session()->GetCurrentConferenceMessageArea();
        en = session()->GetCurrentConferenceMessageArea();
        break;
      case 'Q':
        return;
      }
    }
  } else {
    oc = -1;
  }

  bout.nl();
  pla("|#9Sub-Conferences Available: ", &abort);
  bout.nl();
  int i = sn;
  while (i <= en && uconfsub[i].confnum != -1 && !abort) {
    if (uconfsub[1].confnum != -1 && okconf(session()->user())) {
      setuconf(ConferenceType::CONF_SUBS, i, -1);
      sprintf(s, "|#1%s %c|#0:|#2 %s", "Conference",
              subconfs[uconfsub[i].confnum].designator,
              stripcolors(reinterpret_cast<char*>(subconfs[uconfsub[i].confnum].name)));
      pla(s, &abort);
    }
    size_t i1 = 0;
    while ((i1 < session()->subs().subs().size()) && (session()->usub[i1].subnum != -1) && (!abort)) {
      sprintf(s, "  |#5%4.4s|#2", session()->usub[i1].keys);
      if (qsc_q[session()->usub[i1].subnum / 32] & (1L << (session()->usub[i1].subnum % 32))) {
        strcat(s, " - ");
      } else {
        strcat(s, "  ");
      }
      if (net_sysnum || session()->max_net_num() > 1) {
        if (!session()->subs().sub(session()->usub[i1].subnum).nets.empty()) {
          const char *ss;
          if (session()->subs().sub(session()->usub[i1].subnum).nets.size() > 1) {
            ss = "Gated";
          } else {
            ss = stripcolors(session()->net_networks[session()->subs().sub(session()->usub[i1].subnum).nets[0].net_num].name);
          }

          char s1[80];
          if (session()->subs().sub(session()->usub[i1].subnum).anony & anony_val_net) {
            sprintf(s1, "|B1|15[%-8.8s]|#9 ", ss);
          } else {
            sprintf(s1, "|B1|15<%-8.8s>|#9 ", ss);
          }
          strcat(s, s1);
        } else {
          strcat(s, charstr(11, ' '));
        }
        strcat(s, "|#9");
      }
      strcat(s, stripcolors(session()->subs().sub(session()->usub[i1].subnum).name.c_str()));
      pla(s, &abort);
      i1++;
    }
    i++;
    bout.nl();
    if (!okconf(session()->user())) {
      break;
    }
  }

  if (i == 0) {
    pla("|#6None.", &abort);
    bout.nl();
  }

  if (okconf(session()->user())) {
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

  int oldConf = session()->GetCurrentConferenceMessageArea();
  int oldSub = session()->current_user_sub().subnum;
  int sn = 0;  // current sub number
  size_t en = std::max<size_t>(0, subconfnum - 1);

  if (okconf(session()->user())) {
    if (uconfsub[1].confnum != -1) {
      bout.nl();
      bout << "|#2A)ll conferences, Q)uit, <space> for current conference: ";
      ch = onek("Q A");
      bout.nl();
      switch (ch) {
      case ' ':
        sn = session()->GetCurrentConferenceMessageArea();
        en = session()->GetCurrentConferenceMessageArea();
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
    while (i <= en && uconfsub[i].confnum != -1 && !abort) {
      int ns = 0;
      if (uconfsub[1].confnum != -1 && okconf(session()->user())) {
        setuconf(ConferenceType::CONF_SUBS, i, -1);
        i1 = 0;
      }
      size_t firstp = 0;
      while (i1 < session()->subs().subs().size() && session()->usub[i1].subnum != -1 && !abort) {
        if (p) {
          p = 0;
          firstp = i1;
          bout.cls();
          if (uconfsub[1].confnum != -1 && okconf(session()->user())) {
            sprintf(s, "Conference %c: %s",
                    subconfs[uconfsub[i].confnum].designator,
                    stripcolors(reinterpret_cast<char*>(subconfs[uconfsub[i].confnum].name)));
          } else {
            sprintf(s, "%s Message Areas", syscfg.systemname);
          }
          bout.litebar(s);
          DisplayHorizontalBar(78, 7);
          bout << "|#2 Sub   Scan   Net/Local   Sub Name                                 Old   New\r\n";
          DisplayHorizontalBar(78, 7);
        }
        ++ns;
        sprintf(s, "    %-3.3s", session()->usub[i1].keys);
        if (qsc_q[session()->usub[i1].subnum / 32] & (1L << (session()->usub[i1].subnum % 32))) {
          strcpy(s2, "|#5Yes");
        } else {
          strcpy(s2, "|#6No ");
        }
        iscan(i1);
        if (net_sysnum || session()->max_net_num() > 1) {
          if (!session()->subs().sub(session()->usub[i1].subnum).nets.empty()) {
	          const char* ss;
            if (session()->subs().sub(session()->usub[i1].subnum).nets.size() > 1) {
              wc = 6;
              ss = "Gated";
            } else {
              strcpy(s3, session()->net_networks[session()->subs().sub(session()->usub[i1].subnum).nets[0].net_num].name);
              ss = stripcolors(s3);
              wc = session()->net_num() % 8;
            }
            if (session()->subs().sub(session()->usub[i1].subnum).anony & anony_val_net) {
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
        while ((msgIndex <= session()->GetNumMessagesInCurrentMessageArea())
               && (get_post(msgIndex)->qscan <= qsc_p[session()->usub[i1].subnum])) {
          ++msgIndex;
        }
        newTally = session()->GetNumMessagesInCurrentMessageArea() - msgIndex + 1;
        if (session()->current_user_sub().subnum == session()->usub[i1].subnum) {
          sprintf(sdf, " |#9%-3.3d |#9\xB3 %3s |#9\xB3 %6s |#9\xB3 |B1|15%-36.36s |#9\xB3 |#9%5d |#9\xB3 |#%c%5u |#9",
                  i1 + 1, s2, s3, session()->subs().sub(session()->usub[i1].subnum).name.c_str(), 
                  session()->GetNumMessagesInCurrentMessageArea(),
                  newTally ? '6' : '3', newTally);
        } else {
          sprintf(sdf, " |#9%-3.3d |#9\xB3 %3s |#9\xB3 %6s |#9\xB3 |#1%-36.36s |#9\xB3 |#9%5d |#9\xB3 |#%c%5u |#9",
                  i1 + 1, s2, s3, session()->subs().sub(session()->usub[i1].subnum).name.c_str(),
                  session()->GetNumMessagesInCurrentMessageArea(),
                  newTally ? '6' : '3', newTally);
        }
        if (okansi()) {
          osan(sdf, &abort, &next);
        } else {
          osan(stripcolors(sdf), &abort, &next);
        }
        size_t lastp = i1++;
        bout.nl();
        if (bout.lines_listed() >= session()->screenlinest - 2) {
          p = 1;
          bout.clear_lines_listed();
          DisplayHorizontalBar(78, 7);
          bout.bprintf("|#1Select |#9[|#2%d-%d, [N]ext Page, [Q]uit|#9]|#0 : ", firstp + 1, lastp + 1);
          const char* ss = mmkey(0, true);
          if (isdigit(ss[0])) {
            for (size_t i2 = 0; i2 < session()->subs().subs().size(); i2++) {
              if (IsEquals(session()->usub[i2].keys, ss)) {
                session()->set_current_user_sub_num(i2);
                oldSub = session()->current_user_sub().subnum;
                done = true;
                abort = true;
              }
            }
          } else {
            switch (ss[0]) {
            case 'Q': {
              if (okconf(session()->user())) {
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
        if (okconf(session()->user())) {
          if (uconfsub[1].confnum != -1) {
            bout.bprintf("|#1Select |#9[|#21-%d, J=Join Conference, ?=List Again, Q=Quit|#9]|#0 : ", ns);
          } else {
            bout.bprintf("|#1Select |#9[|#21-%d, ?=List Again, Q=Quit|#9]|#0 : ", ns);
          }
        } else {
          bout.bprintf("|#1Select |#9[|#21-%d, ?=List Again, Q=Quit|#9]|#0 : ", ns);
        }
        const char* ss = mmkey(0, true);

        if (IsEquals(ss, "?")) {
          p = 1;
          ns = i = i1 = 0;
        }

        if (IsEquals(ss, " ") ||
            IsEquals(ss, "Q") ||
            IsEquals(ss, "\r")) {
          bout.nl(2);
          done = true;
          if (!okconf(session()->user())) {
            abort = true;
          }
        }
        if (IsEquals(ss, "J")) {
          if (okconf(session()->user())) {
            jump_conf(ConferenceType::CONF_SUBS);
          }
          sn = en = oldConf = session()->GetCurrentConferenceMessageArea();
          ns = i = 0;
        }
        if (isdigit(ss[0])) {
          for (size_t i2 = 0; i2 < session()->subs().subs().size(); i2++) {
            if (IsEquals(session()->usub[i2].keys, ss)) {
              session()->set_current_user_sub_num(i2);
              oldSub = session()->current_user_sub().subnum;
              done = true;
              abort = true;
            }
          }
        }
      } else {
        if (okconf(session()->user())) {
          setuconf(ConferenceType::CONF_SUBS, oldConf, oldSub);
        }
        done = true;
      }
    }
    if (i == 0) {
      pla("None.", &abort);
      bout.nl();
    }
  } while (!hangup && !done);

  if (okconf(session()->user())) {
    setuconf(ConferenceType::CONF_SUBS, oldConf, oldSub);
  }
}


