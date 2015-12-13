/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
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
#include "bbs/read_message.h"

#include <iterator>
#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/bputch.h"
#include "bbs/connect1.h"
#include "bbs/message_file.h"
#include "bbs/subacc.h"
#include "bbs/utility.h"
#include "bbs/vars.h"
#include "core/file.h"
#include "core/strings.h"
#include "sdk/net.h"
#include "sdk/filenames.h"

using std::string;
using std::unique_ptr;
using namespace wwiv::strings;


/**
* Sets the global variables pszOutOriginStr and pszOutOriginStr2.
* Note: This is a private function
*/
static void SetMessageOriginInfo(int system_number, int user_number, string* outNetworkName,
  string* outLocation) {
  string netName;

  if (session()->max_net_num() > 1) {
    netName = StrCat(net_networks[session()->net_num()].name, "- ");
  }

  outNetworkName->clear();
  outLocation->clear();

  if (IsEqualsIgnoreCase(session()->GetNetworkName(), "Internet") ||
    system_number == 32767) {
    outNetworkName->assign("Internet Mail and Newsgroups");
    return;
  }

  if (system_number && session()->net_type() == net_type_wwivnet) {
    net_system_list_rec *csne = next_system(system_number);
    if (csne) {
      string netstatus;
      if (user_number == 1) {
        if (csne->other & other_net_coord) {
          netstatus = "{NC}";
        } else if (csne->other & other_group_coord) {
          netstatus = StringPrintf("{GC%d}", csne->group);
        } else if (csne->other & other_area_coord) {
          netstatus = "{AC}";
        }
      }
      const string filename = StringPrintf(
        "%s%s%c%s.%-3u",
        syscfg.datadir,
        REGIONS_DIR,
        File::pathSeparatorChar,
        REGIONS_DIR,
        atoi(csne->phone));

      string description;
      if (File::Exists(filename)) {
        // Try to use the town first.
        const string phone_prefix = StringPrintf("%c%c%c", csne->phone[4], csne->phone[5], csne->phone[6]);
        description = describe_area_code_prefix(atoi(csne->phone), atoi(phone_prefix.c_str()));
      }
      if (description.empty()) {
        // Try area code if we still don't have a description.
        description = describe_area_code(atoi(csne->phone));
      }

      *outNetworkName = StrCat(netName, csne->name, " [", csne->phone, "] ", netstatus.c_str());
      *outLocation = (!description.empty()) ? description : "Unknown Area";
    } else {
      *outNetworkName = StrCat(netName, "Unknown System");
      *outLocation = "Unknown Area";
    }
  }
}

void display_message_text(const std::string& text, bool *next) {
  int nNumCharsPtr = 0;
  int nLineLenPtr = 0;
  int ctrld = 0;
  bool done = false;
  bool printit = false;
  bool ctrla = false;
  bool centre = false;
  bool abort = false;
  bool ansi = false;
  char s[205];

  bout.nl();
  for (char ch : text) {
    if (done) {
      break;
    }
    if (ch == CZ) {
      done = true;
    } else {
      if (ch != SOFTRETURN) {
        if (ch == RETURN || !ch) {
          printit = true;
        } else if (ch == CA) {
          ctrla = true;
        } else if (ch == CB) {
          centre = true;
        } else if (ch == CD) {
          ctrld = 1;
        } else if (ctrld == 1) {
          if (ch >= '0' && ch <= '9') {
            if (ch == '0') {
              ctrld = -1; // don't display
            } else {
              if (session()->user()->GetOptionalVal() == 0) {
                ctrld = 0; // display
              } else {
                if (10 - (session()->user()->GetOptionalVal()) < (ch - '0')) {
                  ctrld = -1; // don't display
                } else {
                  ctrld = 0; // display
                }
              }
            }
          } else {
            ctrld = 0; // ctrl-d and non-numeric
          }
        } else {
          if (ch == ESC) {
            ansi = true;
          }
          if (g_flags & g_flag_ansi_movement) {
            g_flags &= ~g_flag_ansi_movement;
            lines_listed = 0;
            if (session()->localIO()->GetTopLine() && session()->localIO()->GetScreenBottom() == 24) {
              session()->localIO()->set_protect(0);
            }
          }
          s[nNumCharsPtr++] = ch;
          if (ch == CC || ch == BACKSPACE) {
            --nLineLenPtr;
          } else {
            ++nLineLenPtr;
          }
        }

        if (printit || ansi || nLineLenPtr >= 80) {
          if (centre && (ctrld != -1)) {
            int nSpacesToCenter = (session()->user()->GetScreenChars() - session()->localIO()->WhereX() -
              nLineLenPtr) / 2;
            osan(charstr(nSpacesToCenter, ' '), &abort, next);
          }
          if (nNumCharsPtr) {
            if (ctrld != -1) {
              if ((session()->localIO()->WhereX() + nLineLenPtr >= session()->user()->GetScreenChars()) && !centre
                && !ansi) {
                bout.nl();
              }
              s[nNumCharsPtr] = '\0';
              osan(s, &abort, next);
              if (ctrla && s[nNumCharsPtr - 1] != SPACE && !ansi) {
                if (session()->localIO()->WhereX() < session()->user()->GetScreenChars() - 1) {
                  bputch(SPACE);
                  bout.nl();
                } else {
                  bout.nl();
                }
                checka(&abort, next);
              }
            }
            nLineLenPtr = 0;
            nNumCharsPtr = 0;
          }
          centre = false;
        }
        if (ch == RETURN) {
          if (ctrla == false) {
            if (ctrld != -1) {
              bout.nl();
              checka(&abort, next);
            }
          } else {
            ctrla = false;
          }
          if (printit) {
            ctrld = 0;
          }
        }
        printit = false;
      } else {
        ctrld = 0;
      }
    }

  }

  if (!abort && nNumCharsPtr) {
    s[nNumCharsPtr] = '\0';
    bout << s;
    bout.nl();
  }
  bout.Color(0);
  bout.nl();
  if (express && abort && !*next) {
    expressabort = true;
  }
  if (ansi && session()->topdata && session()->IsUserOnline()) {
    session()->UpdateTopScreen();
  }
  if (syscfg.sysconfig & sysconfig_enable_mci) {
    g_flags &= ~g_flag_disable_mci;
  }
}

static void UpdateHeaderInformation(int8_t anon_type, bool readit, const string default_name,
                                    string* name, string* date) {
  switch (anon_type) {
  case anony_sender:
    if (readit) {
      *name = StrCat("<<< ", default_name, " >>>");
    } else {
      *name = ">UNKNOWN<";
    }
    break;
  case anony_sender_da:
  case anony_sender_pp:
    *date = ">UNKNOWN<";
    if (anon_type == anony_sender_da) {
      *name = "Abby";
    } else {
      *name = "Problemed Person";
    }
    if (readit) {
      *name = StrCat("<<< ", default_name, " >>>");
    }
    break;
  default:
    *name = default_name;
    break;
  }
}

void read_message1(messagerec * pMessageRecord, char an, bool readit, bool *next, const char *pszFileName,
  int nFromSystem, int nFromUser) {
  string name, date;

  g_flags &= ~g_flag_ansi_movement;

  string message_text;
  *next = false;
  switch (pMessageRecord->storage_type) {
  case 0:
  case 1:
  case 2:
  {
    if (!readfile(pMessageRecord, pszFileName, &message_text)) {
      return;
    }

    size_t ptr = 0;
    for (ptr = 0; ptr < message_text.size() && message_text[ptr] != RETURN && ptr<=200; ptr++) {
      name.push_back(message_text[ptr]);
    }
    if (message_text[++ptr] == SOFTRETURN) {
      ++ptr;
    }
    int start = ptr;
    for (; ptr < message_text.size() && message_text[ptr] != RETURN && ptr-start <= 60; ptr++) {
      date.push_back(message_text[ptr]);
    }
    message_text = message_text.substr(ptr + 1);
  }
  break;
  default:
    // illegal storage type
    bout.nl();
    bout << "->ILLEGAL STORAGE TYPE<-\r\n\n";
    return;
  }

  irt_name[0] = '\0';
  g_flags |= g_flag_disable_mci;
  string from;
  string loc;

  UpdateHeaderInformation(an, readit, name, &name, &date);
  if (an == 0) {
    if (syscfg.sysconfig & sysconfig_enable_mci) {
      g_flags &= ~g_flag_disable_mci;
    }
    SetMessageOriginInfo(nFromSystem, nFromUser, &from, &loc);
    strcpy(irt_name, name.c_str());
  }

  bout << "|#9Name|#7: |#1" << name << wwiv::endl;
  bout << "|#9Date|#7: |#1" << date << wwiv::endl;
  if (!from.empty()) {
    bout << "|#9From|#7: |#1" << from << wwiv::endl;
  }
  if (!loc.empty()) {
    bout << "|#9Loc|#7:  |#1" << loc << wwiv::endl;
  }
  display_message_text(message_text, next);
}

void read_post(int n, bool *next, int *val) {
  bout.nl();
  bool abort = false;
  *next = false;
  if (session()->user()->IsUseClearScreen()) {
    bout.cls();
  }
  if (forcescansub) {
    bout.cls();
    bout.GotoXY(1, 1);
    bout << "|#4   FORCED SCAN OF SYSOP INFORMATION - YOU MAY NOT ABORT.  PLEASE READ THESE!  |#0\r\n";
  }

  bout.bprintf(" |#9Msg|#7: [|#1%u|#7/|#1%lu|#7]|#%d |#2%s\r\n", n,
    session()->GetNumMessagesInCurrentMessageArea(), session()->GetMessageColor(),
    subboards[session()->GetCurrentReadMessageArea()].name);
  const string subjectLine = "|#9Subj|#7: ";
  osan(subjectLine, &abort, next);
  bout.Color(session()->GetMessageColor());
  postrec p = *get_post(n);
  if (p.status & (status_unvalidated | status_delete)) {
    bout.Color(6);
    osan("<<< NOT VALIDATED YET >>>", &abort, next);
    bout.nl();
    if (!lcs()) {
      return;
    }
    *val |= 1;
    osan(subjectLine, &abort, next);
    bout.Color(session()->GetMessageColor());
  }
  strncpy(irt, p.title, 60);
  irt_name[0] = '\0';
  bout.Color(session()->GetMessageColor());
  osan(irt, &abort, next);
  bout.nl();
  if ((p.status & status_no_delete) && lcs()) {
    osan("|#9Info|#7: ", &abort, next);
    bout.Color(session()->GetMessageColor());
    osan("Permanent Message", &abort, next);
    bout.nl();
  }
  if ((p.status & status_pending_net) && session()->user()->GetSl() > syscfg.newusersl) {
    osan("|#9Val|#7:  ", &abort, next);
    bout.Color(session()->GetMessageColor());
    osan("Not Network Validated", &abort, next);
    bout.nl();
    *val |= 2;
  }
  if (!abort) {
    bool bReadit = (lcs() || (getslrec(session()->GetEffectiveSl()).ability & ability_read_post_anony)) ? true : false;
    int nNetNumSaved = session()->net_num();

    if (p.status & status_post_new_net) {
      set_net_num(p.network.network_msg.net_number);
    }
    read_message1(&(p.msg), static_cast<char>(p.anony & 0x0f), bReadit, next,
      (subboards[session()->GetCurrentReadMessageArea()].filename), p.ownersys, p.owneruser);

    if (nNetNumSaved != session()->net_num()) {
      set_net_num(nNetNumSaved);
    }

    session()->user()->SetNumMessagesRead(session()->user()->GetNumMessagesRead() + 1);
    session()->SetNumMessagesReadThisLogon(session()->GetNumMessagesReadThisLogon() + 1);
  } else if (express && !*next) {
    expressabort = true;
  }
  if (p.qscan > qsc_p[session()->GetCurrentReadMessageArea()]) {
    qsc_p[session()->GetCurrentReadMessageArea()] = p.qscan;
  }

  unsigned long current_qscan_pointer = 0;
  {
    std::unique_ptr<WStatus> wwiv_status(session()->GetStatusManager()->GetStatus());
    // not sure why we check this twice...
    // maybe we need a getCachedQScanPointer?
    current_qscan_pointer = wwiv_status->GetQScanPointer();
  }
  if (p.qscan >= current_qscan_pointer) {
    WStatus* pStatus = session()->GetStatusManager()->BeginTransaction();
    if (p.qscan >= pStatus->GetQScanPointer()) {
      pStatus->SetQScanPointer(p.qscan + 1);
    }
    session()->GetStatusManager()->CommitTransaction(pStatus);
  }
}
