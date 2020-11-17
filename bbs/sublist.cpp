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
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/mmkey.h"
#include "bbs/subacc.h"
#include "bbs/utility.h"
#include "common/com.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/subxtr.h"
#include <algorithm>
#include <string>

using std::max;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

void old_sublist() {
  int oc = a()->sess().current_user_sub_conf_num();
  int os = a()->current_user_sub().subnum;

  bool abort = false;
  auto sn = std::begin(a()->uconfsub);
  auto en = std::end(a()->uconfsub);
  if (okconf(a()->user())) {
    if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
      bout.nl();
      bout << "|#2A)ll conferences, Q)uit, <space> for current conference: ";
      const auto ch = onek("Q A");
      bout.nl();
      switch (ch) {
      case ' ':
        std::advance(sn, a()->sess().current_user_sub_conf_num());
        en = sn;
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
  auto iter = sn;
  auto count = 0;
  while (iter != en && !abort) {
    if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
      const auto pos = std::distance(std::begin(a()->uconfsub), iter);
      setuconf(ConferenceType::CONF_SUBS, pos, -1);
      const auto s =
          fmt::format("|#1Conference {}|#0:|#2 {}", iter->key, stripcolors(iter->conf_name));
      bout.bpla(s, &abort);
    }
    size_t i1 = 0;
    while (i1 < a()->usub.size() && !abort) {
      std::ostringstream os1;
      os1 << fmt::sprintf("  |#5%4.4s|#2", a()->usub[i1].keys);
      if (a()->sess().qsc_q[a()->usub[i1].subnum / 32] & (1L << (a()->usub[i1].subnum % 32))) {
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
      ++count;
    }
    ++iter;
    bout.nl();
    if (!okconf(a()->user())) {
      break;
    }
  }

  if (!count) {
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
  const auto q = a()->sess().qsc_p[subnum];
  if (num == 0) {
    return 0;
  }
  const auto midpoint = num / 2;
  auto msgIndex = num;
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
           get_post(msgIndex)->qscan <= a()->sess().qsc_p[subnum]) {
      ++msgIndex;
    }
  }
  return a()->GetNumMessagesInCurrentMessageArea() - msgIndex + 1;
}

void SubList() {
  int p, wc; // color code
  char ch;
  bool next;

  int oc = a()->sess().current_user_sub_conf_num();
  int old_sub = a()->current_user_sub().subnum;
  auto sn = std::begin(a()->uconfsub);  // current sub number
  auto en = std::end(a()->uconfsub);

  if (okconf(a()->user())) {
    if (a()->uconfsub.size() > 1) {
      bout.nl();
      bout << "|#2A)ll conferences, Q)uit, <space> for current conference: ";
      ch = onek("Q A");
      bout.nl();
      switch (ch) {
      case ' ':
        sn = std::begin(a()->uconfsub);
        std::advance(sn, a()->sess().current_user_sub_conf_num());
        en = sn + 1;
        break;
      case 'Q':
        return;
      }
    }
  } else {
    oc = -1;
  }

  auto abort = false;
  auto done = false;
  do {
    p = 1;
    auto i1 = 0;
    for(auto iter = sn;  iter != en && !abort; ++iter) {
      auto ns = 0;
      if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
        auto pos = std::distance(std::begin(a()->uconfsub), iter);
        setuconf(ConferenceType::CONF_SUBS, pos, -1);
        i1 = 0;
      }
      size_t firstp = 0;
      while (i1 < size_int(a()->usub) && !abort) {
        if (p) {
          p = 0;
          firstp = i1;
          bout.cls();
          std::string s;
          if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
            s = fmt::format("Conference {}: {}", iter->key.key(), stripcolors(iter->conf_name));
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
            (a()->sess().qsc_q[a()->usub[i1].subnum / 32] & (1L << (a()->usub[i1].subnum % 32)))
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
              const auto nn = a()->nets()[a()->subs().sub(a()->usub[i1].subnum).nets[0].net_num].name;
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
        const auto subname_color = (a()->current_user_sub().subnum == a()->usub[i1].subnum) ? "|#4" : "|#1";
        const auto sdf = fmt::sprintf(" |#9%-3.3d |#9\xB3 %3s |#9\xB3 %6s |#9\xB3 %s%-36.36s |#9\xB3 "
                           "|#9%5d |#9\xB3 |#%c%5u |#9",
                           i1 + 1, yns, net_info, subname_color, a()->subs().sub(a()->usub[i1].subnum).name,
                           a()->GetNumMessagesInCurrentMessageArea(), num_new_posts ? '6' : '3',
                           num_new_posts);
        bout.bputs(sdf, &abort, &next);
        bout.nl();
        auto lastp = i1++;
        if (bout.lines_listed() >= a()->sess().num_screen_lines() - 2) {
          p = 1;
          bout.clear_lines_listed();
          DisplayHorizontalBar(78, 7);
          bout.bprintf("|#1Select |#9[|#2%d-%d, [N]ext Page, [Q]uit|#9]|#0 : ", firstp + 1, lastp + 1);
          const auto ss = mmkey(MMKeyAreaType::subs, true);
          if (isdigit(ss[0])) {
            for (auto i2 = 0; i2 < ssize(a()->usub); i2++) {
              if (ss == a()->usub[i2].keys) {
                a()->set_current_user_sub_num(i2);
                old_sub = a()->current_user_sub().subnum;
                done = true;
                abort = true;
              }
            }
          } else {
            switch (ss[0]) {
            case 'Q': {
              if (okconf(a()->user())) {
                setuconf(ConferenceType::CONF_SUBS, oc, old_sub);
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
      //if (ns) {
      //  ++iter;
      //}

      if (!abort) {
        p = 1;
        DisplayHorizontalBar(78, 7);
        if (okconf(a()->user())) {
          if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
            bout.bprintf("|#1Select |#9[|#21-%d, J=Join Conference, ?=List Again, Q=Quit|#9]|#0 : ", ns);
          } else {
            bout.bprintf("|#1Select |#9[|#21-%d, ?=List Again, Q=Quit|#9]|#0 : ", ns);
          }
        } else {
          bout.bprintf("|#1Select |#9[|#21-%d, ?=List Again, Q=Quit|#9]|#0 : ", ns);
        }
        const auto ss = mmkey(MMKeyAreaType::subs, true);

        if (ss == "?") {
          p = 1;
          ns = i1 = 0;
          iter = std::begin(a()->uconfsub);
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
          oc = a()->sess().current_user_sub_conf_num();
          sn = std::begin(a()->uconfsub);
          std::advance(sn, a()->sess().current_user_sub_conf_num());
          en = sn + 1;
          iter = sn;
          ns = 0;
        }
        if (isdigit(ss.front())) {
          for (uint16_t i2 = 0; i2 < a()->usub.size(); i2++) {
            if (ss == a()->usub[i2].keys) {
              a()->set_current_user_sub_num(i2);
              old_sub = a()->current_user_sub().subnum;
              done = true;
              abort = true;
            }
          }
        }
      } else {
        if (okconf(a()->user())) {
          setuconf(ConferenceType::CONF_SUBS, oc, old_sub);
        }
        done = true;
      }
    }
    //if (i == 0) {
    //  bout.bpla("None.", &abort);
    //  bout.nl();
    //}
  } while (!a()->sess().hangup() && !done);

  if (okconf(a()->user())) {
    setuconf(ConferenceType::CONF_SUBS, oc, old_sub);
  }
}


