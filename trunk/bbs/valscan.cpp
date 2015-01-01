/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2015, WWIV Software Services             */
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

#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/subxtr.h"
#include "bbs/wwiv.h"

void valscan() {
  // Must be local cosysop or better
  if (!lcs()) {
    return;
  }

  int ac = 0;
  int os = session()->GetCurrentMessageArea();

  if (uconfsub[1].confnum != -1 && okconf(session()->user())) {
    ac = 1;
    tmp_disable_conf(true);
  }
  bool done = false;
  for (int sn = 0; sn < session()->num_subs && !hangup && !done; sn++) {
    if (!iscan(sn)) {
      continue;
    }
    if (session()->GetCurrentReadMessageArea() < 0) {
      return;
    }

    uint32_t sq = qsc_p[sn];

    // Must be sub with validation "on"
    if (!(xsubs[session()->GetCurrentReadMessageArea()].num_nets)
        || !(subboards[session()->GetCurrentReadMessageArea()].anony & anony_val_net)) {
      continue;
    }

    bout.nl();
    bout.Color(2);
    bout.clreol();
    bout << "{{ ValScanning " << subboards[session()->GetCurrentReadMessageArea()].name << " }}\r\n";
    lines_listed = 0;
    bout.clreol();
    if (okansi() && !newline) {
      bout << "\r\x1b[2A";
    }

    for (int i = 1; i <= session()->GetNumMessagesInCurrentMessageArea() && !hangup && !done; i++) {    // was i = 0
      if (get_post(i)->status & status_pending_net) {
        CheckForHangup();
        session()->localIO()->tleft(true);
        if (i > 0 && i <= session()->GetNumMessagesInCurrentMessageArea()) {
          bool next;
          int val;
          read_message(i, &next, &val);
          bout << "|#4[|#4Subboard: " << subboards[session()->GetCurrentReadMessageArea()].name << "|#1]\r\n";
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
            send_net_post(p1, subboards[session()->GetCurrentReadMessageArea()].filename,
                          session()->GetCurrentReadMessageArea());
            bout.nl();
            bout << "|#7Message sent.\r\n\n";
          }
          break;
          case 'M':
            if (lcs() && i > 0 && i <= session()->GetNumMessagesInCurrentMessageArea() &&
                subboards[session()->GetCurrentReadMessageArea()].anony & anony_val_net &&
                xsubs[session()->GetCurrentReadMessageArea()].num_nets) {
              open_sub(true);
              resynch(&i, nullptr);
              postrec *p1 = get_post(i);
              p1->status &= ~status_pending_net;
              write_post(i, p1);
              close_sub();
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
                  WUser tu;
                  application()->users()->ReadUser(&tu, p2.owneruser);
                  if (!tu.IsUserDeleted()) {
                    if (date_to_daten(tu.GetFirstOn()) < static_cast<time_t>(p2.daten)) {
                      bout.nl();
                      bout << "|#2Remove how many posts credit? ";
                      char szNumCredits[ 11 ];
                      input(szNumCredits, 3, true);
                      int nNumPostCredits = 1;
                      if (szNumCredits[0]) {
                        nNumPostCredits = atoi(szNumCredits);
                      }
                      nNumPostCredits = std::min<int>(tu.GetNumMessagesPosted(), nNumPostCredits);
                      if (nNumPostCredits) {
                        tu.SetNumMessagesPosted(tu.GetNumMessagesPosted() - static_cast<unsigned short>(nNumPostCredits));
                      }
                      bout.nl();
                      bout << "|#3Post credit removed = " << nNumPostCredits << wwiv::endl;
                      tu.SetNumDeletedPosts(tu.GetNumDeletedPosts() + 1);
                      application()->users()->WriteUser(&tu, p2.owneruser);
                      application()->UpdateTopScreen();
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
    qsc_p[sn] = sq;
  }

  if (ac) {
    tmp_disable_conf(false);
  }

  session()->SetCurrentMessageArea(os);
  bout.nl(2);
}
