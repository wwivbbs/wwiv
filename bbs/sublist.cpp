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

#include "bbs/wwiv.h"
#include "bbs/confutil.h"
#include "bbs/subxtr.h"
#include "core/strings.h"

using namespace wwiv::strings;

void old_sublist() {
  char s[ 255 ];

  int oc = session()->GetCurrentConferenceMessageArea();
  int os = usub[session()->GetCurrentMessageArea()].subnum;

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
      setuconf(CONF_SUBS, i, -1);
      sprintf(s, "|#1%s %c|#0:|#2 %s", "Conference",
              subconfs[uconfsub[i].confnum].designator,
              stripcolors(reinterpret_cast<char*>(subconfs[uconfsub[i].confnum].name)));
      pla(s, &abort);
    }
    int i1 = 0;
    while ((i1 < session()->num_subs) && (usub[i1].subnum != -1) && (!abort)) {
      sprintf(s, "  |#5%4.4s|#2", usub[i1].keys);
      if (qsc_q[usub[i1].subnum / 32] & (1L << (usub[i1].subnum % 32))) {
        strcat(s, " - ");
      } else {
        strcat(s, "  ");
      }
      if (net_sysnum || session()->GetMaxNetworkNumber() > 1) {
        if (xsubs[usub[i1].subnum].num_nets) {
          const char *ss;
          if (xsubs[usub[i1].subnum].num_nets > 1) {
            ss = "Gated";
          } else {
            ss = stripcolors(net_networks[xsubs[usub[i1].subnum].nets[0].net_num].name);
          }

          char s1[80];
          if (subboards[usub[i1].subnum].anony & anony_val_net) {
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
      strcat(s, stripcolors(subboards[usub[i1].subnum].name));
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
    setuconf(CONF_SUBS, oc, os);
  }
}


void SubList() {
  int i, i1, i2,                        //loop variables
      ns,                               //number of subs
      p,
      firstp,                           //first sub on page
      lastp,                            //last sub on page
      wc,                               //color code
      oldConf,                          //old conference
      oldSub,                           //old sub
      sn,                               //current sub number
      en,
      msgIndex,                         //message Index
      newTally;                         //new message tally
  char ch, s[81], s2[10], s3[81], sdf[130];
  bool next;

  oldConf = session()->GetCurrentConferenceMessageArea();
  oldSub = usub[session()->GetCurrentMessageArea()].subnum;

  sn = lastp = firstp = 0;
  ns = session()->GetCurrentConferenceMessageArea();
  en = subconfnum - 1;

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
    i = sn;
    i1 = 0;
    ns = session()->GetCurrentConferenceMessageArea();
    while (i <= en && uconfsub[i].confnum != -1 && !abort) {
      if (uconfsub[1].confnum != -1 && okconf(session()->user())) {
        setuconf(CONF_SUBS, i, -1);
        i1 = 0;
      }
      while (i1 < session()->num_subs && usub[i1].subnum != -1 && !abort) {
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
        sprintf(s, "    %-3.3s", usub[i1].keys);
        if (qsc_q[usub[i1].subnum / 32] & (1L << (usub[i1].subnum % 32))) {
          strcpy(s2, "|#5Yes");
        } else {
          strcpy(s2, "|#6No ");
        }
        iscan(i1);
        if (net_sysnum || session()->GetMaxNetworkNumber() > 1) {
          if (xsubs[usub[i1].subnum].num_nets) {
	    const char* ss;
            if (xsubs[usub[i1].subnum].num_nets > 1) {
              wc = 6;
              ss = "Gated";
            } else {
              strcpy(s3, net_networks[xsubs[usub[i1].subnum].nets[0].net_num].name);
              ss = stripcolors(s3);
              wc = session()->GetNetworkNumber() % 8;
            }
            if (subboards[usub[i1].subnum].anony & anony_val_net) {
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
               && (get_post(msgIndex)->qscan <= qsc_p[usub[i1].subnum])) {
          ++msgIndex;
        }
        newTally = session()->GetNumMessagesInCurrentMessageArea() - msgIndex + 1;
        if (usub[session()->GetCurrentMessageArea()].subnum == usub[i1].subnum) {
          sprintf(sdf, " |#9%-3.3d |#9\xB3 %3s |#9\xB3 %6s |#9\xB3 |B1|15%-36.36s |#9\xB3 |#9%5d |#9\xB3 |#%c%5d |#9",
                  i1 + 1, s2, s3, subboards[usub[i1].subnum].name, session()->GetNumMessagesInCurrentMessageArea(),
                  newTally ? '6' : '3', newTally);
        } else {
          sprintf(sdf, " |#9%-3.3d |#9\xB3 %3s |#9\xB3 %6s |#9\xB3 |#1%-36.36s |#9\xB3 |#9%5d |#9\xB3 |#%c%5d |#9",
                  i1 + 1, s2, s3, subboards[usub[i1].subnum].name, session()->GetNumMessagesInCurrentMessageArea(),
                  newTally ? '6' : '3', newTally);
        }
        if (okansi()) {
          osan(sdf, &abort, &next);
        } else {
          osan(stripcolors(sdf), &abort, &next);
        }
        lastp = i1++;
        bout.nl();
        if (lines_listed >= session()->screenlinest - 2) {
          p = 1;
          lines_listed = 0;
          DisplayHorizontalBar(78, 7);
          bout.bprintf("|#1Select |#9[|#2%d-%d, [Enter]=Next Page, Q=Quit|#9]|#0 : ", firstp + 1, lastp + 1);
          const char* ss = mmkey(0, true);
          if (isdigit(ss[0])) {
            for (i2 = 0; i2 < session()->num_subs; i2++) {
              if (wwiv::strings::IsEquals(usub[i2].keys, ss)) {
                session()->SetCurrentMessageArea(i2);
                oldSub = usub[session()->GetCurrentMessageArea()].subnum;
                done = true;
                abort = true;
              }
            }
          } else {
            switch (ss[0]) {
            case 'Q': {
              if (okconf(session()->user())) {
                setuconf(CONF_SUBS, oldConf, oldSub);
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

        if (wwiv::strings::IsEquals(ss, "?")) {
          p = 1;
          ns = i = i1 = 0;
        }

        if (wwiv::strings::IsEquals(ss, " ") ||
            wwiv::strings::IsEquals(ss, "Q") ||
            wwiv::strings::IsEquals(ss, "\r")) {
          bout.nl(2);
          done = true;
          if (!okconf(session()->user())) {
            abort = true;
          }
        }
        if (wwiv::strings::IsEquals(ss, "J")) {
          if (okconf(session()->user())) {
            jump_conf(CONF_SUBS);
          }
          sn = en = oldConf = session()->GetCurrentConferenceMessageArea();
          ns = i = 0;
        }
        if (isdigit(ss[0])) {
          for (i2 = 0; i2 < session()->num_subs; i2++) {
            if (wwiv::strings::IsEquals(usub[i2].keys, ss)) {
              session()->SetCurrentMessageArea(i2);
              oldSub = usub[session()->GetCurrentMessageArea()].subnum;
              done = true;
              abort = true;
            }
          }
        }
      } else {
        if (okconf(session()->user())) {
          setuconf(CONF_SUBS, oldConf, oldSub);
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
    setuconf(CONF_SUBS, oldConf, oldSub);
  }
}


