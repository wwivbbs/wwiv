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
#include "bbs/valscan.h"

#include <algorithm>
#include "bbs/bbs.h"
#include "common/com.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "common/datetime.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/subacc.h"
#include "common/input.h"
#include "bbs/msgbase1.h"
#include "bbs/read_message.h"
#include "core/strings.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"


using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

void valscan() {
  // Must be local cosysop or better
  if (!lcs()) {
    return;
  }

  int ac = 0;
  auto os = a()->current_user_sub_num();

  if (ok_multiple_conf(a()->user(), a()->uconfsub)) {
    ac = 1;
    tmp_disable_conf(true);
  }
  bool done = false;
  for (auto usubn = 0; usubn < wwiv::stl::size_int(a()->usub) && !a()->sess().hangup() && !done; usubn++) {
    if (!iscan(usubn)) {
      continue;
    }
    if (a()->sess().GetCurrentReadMessageArea() < 0) {
      return;
    }

    uint32_t sq = a()->sess().qsc_p[usubn];

    // Must be sub with validation "on"
    if (a()->current_sub().nets.empty()
        || !(a()->current_sub().anony & anony_val_net)) {
      continue;
    }

    bout.nl();
    bout.Color(2);
    bout.clreol();
    bout << "{{ ValScanning " << a()->current_sub().name << " }}\r\n";
    bout.clear_lines_listed();
    bout.clreol();

    bout.move_up_if_newline(2);

    for (int i = 1; i <= a()->GetNumMessagesInCurrentMessageArea() && !a()->sess().hangup() && !done; i++) {    // was i = 0
      if (get_post(i)->status & status_pending_net) {
        a()->CheckForHangup();
        a()->tleft(true);
        if (i > 0 && i <= a()->GetNumMessagesInCurrentMessageArea()) {
          bool next;
          int val;
          read_post(i, &next, &val);
          bout << "|#4[|#4Subboard: " << a()->current_sub().name << "|#1]\r\n";
          bout <<  "|#1D|#9)elete, |#1R|#9)eread |#1V|#9)alidate, |#1M|#9)ark Validated, |#1Q|#9)uit: |#2";
          char ch = onek("QDVMR");
          switch (ch) {
          case 'Q':
            done = true;
            break;
          case 'R':
            i--;
            continue;
          case 'V': {
            open_sub(true);
            resynch(&i, nullptr);
            postrec *p1 = get_post(i);
            p1->status &= ~status_pending_net;
            write_post(i, p1);
            close_sub();
            send_net_post(p1, a()->current_sub());
            bout.nl();
            bout << "|#7Message sent.\r\n\n";
          }
          break;
          case 'M':
            if (lcs() && i > 0 && i <= a()->GetNumMessagesInCurrentMessageArea() &&
                a()->current_sub().anony & anony_val_net &&
                !a()->current_sub().nets.empty()) {
              wwiv::bbs::OpenSub opened_sub(true);
              resynch(&i, nullptr);
              postrec *p1 = get_post(i);
              p1->status &= ~status_pending_net;
              write_post(i, p1);
              bout.nl();
              bout << "|#9Not set for net pending now.\r\n\n";
            }
            break;
          case 'D':
            if (lcs()) {
              if (i > 0) {
                open_sub(true);
                resynch(&i, nullptr);
                postrec p2 = *get_post(i);
                delete_message(i);
                close_sub();
                if (p2.ownersys == 0) {
                  User tu;
                  a()->users()->readuser(&tu, p2.owneruser);
                  if (!tu.IsUserDeleted()) {
                    if (date_to_daten(tu.GetFirstOn()) < p2.daten) {
                      bout.nl();
                      bout << "|#2Remove how many posts credit? ";
                      char szNumCredits[ 11 ];
                      bin.input(szNumCredits, 3, true);
                      int num_post_credits = 1;
                      if (szNumCredits[0]) {
                        num_post_credits = to_number<int>(szNumCredits);
                      }
                      num_post_credits = std::min<int>(tu.GetNumMessagesPosted(), num_post_credits);
                      if (num_post_credits) {
                        tu.SetNumMessagesPosted(tu.GetNumMessagesPosted() - static_cast<uint16_t>(num_post_credits));
                      }
                      bout.nl();
                      bout << "|#3Post credit removed = " << num_post_credits << wwiv::endl;
                      tu.SetNumDeletedPosts(tu.GetNumDeletedPosts() + 1);
                      a()->users()->writeuser(&tu, p2.owneruser);
                      a()->UpdateTopScreen();
                    }
                  }
                }
                resynch(&i, &p2);
              }
            }
            break;
          }
        }
      }
    }
    a()->sess().qsc_p[usubn] = sq;
  }

  if (ac) {
    tmp_disable_conf(false);
  }

  a()->set_current_user_sub_num(os);
  bout.nl(2);
}
