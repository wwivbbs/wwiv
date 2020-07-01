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
/*                                                                        */
/**************************************************************************/
#include "bbs/sublist.h"

#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/mmkey.h"
#include "bbs/subacc.h"
#include "bbs/utility.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/subxtr.h"
#include <algorithm>
#include <string>

using std::max;
using namespace wwiv::stl;
using namespace wwiv::strings;

void old_sublist() {
  int oc = a()->GetCurrentConferenceMessageArea();
  int os = a()->current_user_sub().subnum;

  bool abort = false;
  int sn = 0;
  int en = ssize(a()->subconfs) - 1;
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
      auto s = fmt::sprintf("|#1%s %c|#0:|#2 %s", "Conference",
        a()->subconfs[a()->uconfsub[i].confnum].designator,
        cn.c_str());
      bout.bpla(s, &abort);
    }
    size_t i1 = 0;
    while ((i1 < a()->subs().subs().size()) && (a()->usub[i1].subnum != -1) && (!abort)) {
      std::ostringstream os1;
      os1 << fmt::sprintf("  |#5%4.4s|#2", a()->usub[i1].keys);
      if (a()->context().qsc_q[a()->usub[i1].subnum / 32] & (1L << (a()->usub[i1].subnum % 32))) {
        os1 << " - ";
      } else {
        os1 << "  ";
      }
      if (a()->current_net().sysnum || wwiv::stl::ssize(a()->nets()) > 1) {
        if (!a()->subs().sub(a()->usub[i1].subnum).nets.empty()) {
          std::string ss;
          if (a()->subs().sub(a()->usub[i1].subnum).nets.size() > 1) {
            ss = "Gated";
          } else {
            ss = stripcolors(a()->nets()[a()->subs().sub(a()->usub[i1].subnum).nets[0].net_num].name);
          }

          if (a()->subs().sub(a()->usub[i1].subnum).anony & anony_val_net) {
            os1 << fmt::sprintf("|17|15[%-8.8s]|#9 ", ss);
          } else {
            os1 << fmt::sprintf("|17|15<%-8.8s>|#9 ", ss);
          }
        } else {
          os1 << std::string(11, ' ');
        }
        os1 << "|#9";
      }
      os1 << stripcolors(a()->subs().sub(a()->usub[i1].subnum).name);
      bout.bpla(os1.str(), &abort);
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

/**
 * Gets the count of new posts in subnum.
 *
 * Starts at the end and will search backwards from the midpoint before starting
 * from the front.  This assumes most people have read most of the messages, and
 * will prioritize performance for those people. Afterwards will start at the
 * start and search forward.
 *
 * If we need to optimize more ever, we can use a binary search if needed or just
 * load all of the postrecs and search in memory.
 */
int get_new_posts_count(int subnum) {
  const auto num = a()->GetNumMessagesInCurrentMessageArea();
  const auto q = a()->context().qsc_p[subnum];
  if (num == 0) {
    return 0;
  }
  const int midpoint = num / 2;
  int msgIndex = num;
  int64_t last_qscan = 0;
  while(msgIndex > midpoint) {
    const auto cur = get_post(msgIndex)->qscan;
    if (cur <= q) {
      last_qscan = cur;
      ++msgIndex;
      break;
    }
    --msgIndex;
  }
  if (last_qscan < static_cast<int64_t>(q)) {
    msgIndex = 1;
    while (msgIndex <= a()->GetNumMessagesInCurrentMessageArea() &&
           get_post(msgIndex)->qscan <= a()->context().qsc_p[subnum]) {
      ++msgIndex;
    }
  }
  return a()->GetNumMessagesInCurrentMessageArea() - msgIndex + 1;
}

void SubList() {
  int p,
      wc // color code
      // message Index
      ; // new message tally
  char ch;
  bool next;

  int oldConf = a()->GetCurrentConferenceMessageArea();
  int oldSub = a()->current_user_sub().subnum;
  int sn = 0;  // current sub number
  auto en = std::max<size_t>(0, a()->subconfs.size() - 1);

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
          std::string s;
          if (a()->uconfsub[1].confnum != -1 && okconf(a()->user())) {
            s = fmt::sprintf("Conference %c: %s",
                             a()->subconfs[a()->uconfsub[i].confnum].designator,
                             stripcolors(a()->subconfs[a()->uconfsub[i].confnum].conf_name));
          } else {
            s = fmt::format("{} Message Areas", a()->config()->system_name());
          }
          bout.litebar(s);
          DisplayHorizontalBar(78, 7);
          bout << "|#2 Sub   Scan   Net/Local   Sub Name                                 Old   New\r\n";
          DisplayHorizontalBar(78, 7);
        }
        ++ns;
        std::string yns =
            (a()->context().qsc_q[a()->usub[i1].subnum / 32] & (1L << (a()->usub[i1].subnum % 32)))
                ? "|#5Yes"
                : "|#6No ";
        iscan(i1);
        std::string net_info;
        if (a()->current_net().sysnum || wwiv::stl::ssize(a()->nets()) > 1) {
          if (!a()->subs().sub(a()->usub[i1].subnum).nets.empty()) {
	          std::string net_name;
            if (a()->subs().sub(a()->usub[i1].subnum).nets.size() > 1) {
              wc = 6;
              net_name = "Gated";
            } else {
              const std::string nn = a()->nets()[a()->subs().sub(a()->usub[i1].subnum).nets[0].net_num].name;
              net_name = stripcolors(nn);
              wc = a()->net_num() % 8;
            }
            if (a()->subs().sub(a()->usub[i1].subnum).anony & anony_val_net) {
              net_info = fmt::sprintf("|#7[|#%i%-8.8s|#7]", wc, net_name);
            } else {
              net_info = fmt::sprintf("|#7<|#%i%-8.8s|#7>", wc, net_name);
            }
          } else {
            net_info = " |#7>|#1LOCAL|#7<  ";
          }
        } else {
          net_info = "|#7>|#1LOCAL|#7<  ";
        }
        const auto num_new_posts = get_new_posts_count(a()->usub[i1].subnum);
        std::string sdf;
        if (a()->current_user_sub().subnum == a()->usub[i1].subnum) {
          sdf = fmt::sprintf(" |#9%-3.3d |#9\xB3 %3s |#9\xB3 %6s |#9\xB3 |17|15%-36.36s |#9\xB3 "
                             "|#9%5d |#9\xB3 |#%c%5u |#9",
                             i1 + 1, yns, net_info, a()->subs().sub(a()->usub[i1].subnum).name,
                             a()->GetNumMessagesInCurrentMessageArea(), num_new_posts ? '6' : '3',
                             num_new_posts);
        } else {
          sdf = fmt::sprintf(" |#9%-3.3d |#9\xB3 %3s |#9\xB3 %6s |#9\xB3 |#1%-36.36s |#9\xB3 "
                             "|#9%5d |#9\xB3 |#%c%5u |#9",
                             i1 + 1, yns, net_info, a()->subs().sub(a()->usub[i1].subnum).name,
                             a()->GetNumMessagesInCurrentMessageArea(), num_new_posts ? '6' : '3',
                             num_new_posts);
        }
        bout.bputs(sdf, &abort, &next);
        bout.nl();
        auto lastp = i1++;
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


