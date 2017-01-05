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
#include "bbs/sysopf.h"

#include <memory>
#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/execexternal.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/finduser.h"
#include "bbs/vars.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/datetime.h"
#include "bbs/dropfile.h"
#include "bbs/email.h"
#include "bbs/instmsg.h"
#include "bbs/input.h"
#include "bbs/keycodes.h"
#include "bbs/mmkey.h"
#include "bbs/multinst.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/read_message.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/uedit.h"
#include "bbs/wconstants.h"
#include "bbs/shortmsg.h"
#include "bbs/wqscn.h"
#include "sdk/status.h"
#include "core/inifile.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "sdk/filenames.h"
#include "sdk/bbslist.h"
#include "sdk/msgapi/message_utils_wwiv.h"

using std::string;
using std::unique_ptr;
using wwiv::core::IniFile;
using wwiv::core::FilePath;

using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::sdk::msgapi;

void prstatus() {
  a()->status_manager()->RefreshStatusCache();
  bout.cls();
  if (a()->config()->config()->newuserpw[0] != '\0') {
    bout << "|#9New User Pass   : " << a()->config()->config()->newuserpw << wwiv::endl;
  }
  bout << "|#9Board is        : " << (a()->config()->config()->closedsystem ? "Closed" : "Open") << wwiv::endl;

  std::unique_ptr<WStatus> pStatus(a()->status_manager()->GetStatus());
  string t = times();
  bout << "|#9Number Users    : |#2" << pStatus->GetNumUsers() << wwiv::endl;
  bout << "|#9Number Calls    : |#2" << pStatus->GetCallerNumber() << wwiv::endl;
  bout << "|#9Last Date       : |#2" << pStatus->GetLastDate() << wwiv::endl;
  bout << "|#9Time            : |#2" << t.c_str() << wwiv::endl;
  bout << "|#9Active Today    : |#2" << pStatus->GetMinutesActiveToday() << wwiv::endl;
  bout << "|#9Calls Today     : |#2" << pStatus->GetNumCallsToday() << wwiv::endl;
  bout << "|#9Net Posts Today : |#2" << (pStatus->GetNumMessagesPostedToday() - pStatus->GetNumLocalPosts())
        << wwiv::endl;
  bout << "|#9Local Post Today: |#2" << pStatus->GetNumLocalPosts() << wwiv::endl;
  bout << "|#9E Sent Today    : |#2" << pStatus->GetNumEmailSentToday() << wwiv::endl;
  bout << "|#9F Sent Today    : |#2" << pStatus->GetNumFeedbackSentToday() << wwiv::endl;
  bout << "|#9Uploads Today   : |#2" << pStatus->GetNumUploadsToday() << wwiv::endl;

  User sysop{};
  int feedback_waiting = 0;
  if (a()->users()->ReadUserNoCache(&sysop, 1)) {
    feedback_waiting = sysop.GetNumMailWaiting();
  }
  bout << "|#9Feedback Waiting: |#2" << feedback_waiting << wwiv::endl;
  bout << "|#9Sysop           : |#2" << ((sysop2()) ? "Available" : "NOT Available") << wwiv::endl;
  bout << "|#9Q-Scan Pointer  : |#2" << pStatus->GetQScanPointer() << wwiv::endl;

  if (num_instances() > 1) {
    multi_instance();
  }
}

void valuser(int user_number) {
  char s[81], s1[81], s2[81], s3[81], ar1[20], dar1[20];

  User user;
  a()->users()->ReadUser(&user, user_number);
  if (!user.IsUserDeleted()) {
    bout.nl();
    const string unn = a()->names()->UserName(user_number);
    bout << "|#9Name: |#2" << unn << wwiv::endl;
    bout << "|#9RN  : |#2" << user.GetRealName() << wwiv::endl;
    bout << "|#9PH  : |#2" << user.GetVoicePhoneNumber() << wwiv::endl;
    bout << "|#9Age : |#2" << user.GetAge() << " " << user.GetGender() << wwiv::endl;
    bout << "|#9Comp: |#2" << ctypes(user.GetComputerType()) << wwiv::endl;
    if (user.GetNote()[0]) {
      bout << "|#9Note: |#2" << user.GetNote() << wwiv::endl;
    }
    bout << "|#9SL  : |#2" << user.GetSl() << wwiv::endl;
    if (user.GetSl() != 255 && user.GetSl() < a()->GetEffectiveSl()) {
      bout << "|#9New : ";
      input(s, 3, true);
      if (s[0]) {
        int nSl = StringToUnsignedInt(s);
        if (!a()->at_wfc() && nSl >= static_cast<int>(a()->GetEffectiveSl())) {
          nSl = -2;
        }
        if (nSl >= 0 && nSl < 255) {
          user.SetSl(nSl);
        }
        if (nSl == -1) {
          bout.nl();
          bout << "|#9Delete? ";
          if (yesno()) {
            deluser(user_number);
            bout.nl();
            bout << "|#6Deleted.\r\n\n";
          } else {
            bout.nl();
            bout << "|#3NOT deleted.\r\n";
          }
          return;
        }
      }
    }
    bout.nl();
    bout << "|#9DSL : |#2" << user.GetDsl() << wwiv::endl;
    if (user.GetDsl() != 255 && user.GetDsl() < a()->user()->GetDsl()) {
      bout << "|#9New ? ";
      input(s, 3, true);
      if (s[0]) {
        int nDsl = StringToUnsignedInt(s);
        if (!a()->at_wfc() && nDsl >= static_cast<int>(a()->user()->GetDsl())) {
          nDsl = -1;
        }
        if (nDsl >= 0 && nDsl < 255) {
          user.SetDsl(nDsl);
        }
      }
    }
    strcpy(s3, restrict_string);
    int ar2     = 1;
    int dar2    = 1;
    ar1[0]      = RETURN;
    dar1[0]     = RETURN;
    for (int i = 0; i <= 15; i++) {
      if (user.HasArFlag(1 << i)) {
        s[i] = static_cast<char>('A' + i);
      } else {
        s[i] = SPACE;
      }
      if (a()->user()->HasArFlag(1 << i)) {
        ar1[ar2++] = static_cast<char>('A' + i);
      }
      if (user.HasDarFlag(1 << i)) {
        s1[i] = static_cast<char>('A' + i);
      } else {
        s1[i] = SPACE;
      }
      if (a()->user()->HasDarFlag(1 << i)) {
        dar1[dar2++] = static_cast<char>('A' + i);
      }
      if (user.HasRestrictionFlag(1 << i)) {
        s2[i] = s3[i];
      } else {
        s2[i] = SPACE;
      }
    }
    s[16]       = '\0';
    s1[16]      = '\0';
    s2[16]      = '\0';
    ar1[ar2]    = '\0';
    dar1[dar2]  = '\0';
    bout.nl();
    char ch1 = '\0';
    if (ar2 > 1) {
      do {
        bout << "|#9AR  : |#2" << s << wwiv::endl;
        bout << "|#9Togl? ";
        ch1 = onek(ar1);
        if (ch1 != RETURN) {
          ch1 -= 'A';
          if (s[ch1] == SPACE) {
            s[ch1] = ch1 + 'A';
          } else {
            s[ch1] = SPACE;
          }
          user.ToggleArFlag(1 << ch1);
          ch1 = 0;
        }
      } while (!hangup && ch1 != RETURN);
    }
    bout.nl();
    ch1 = 0;
    if (dar2 > 1) {
      do {
        bout << "|#9DAR : |#2" << s1 << wwiv::endl;
        bout << "|#9Togl? ";
        ch1 = onek(dar1);
        if (ch1 != RETURN) {
          ch1 -= 'A';
          if (s1[ch1] == SPACE) {
            s1[ch1] = ch1 + 'A';
          } else {
            s1[ch1] = SPACE;
          }
          user.ToggleDarFlag(1 << ch1);
          ch1 = 0;
        }
      } while (!hangup && ch1 != RETURN);
    }
    bout.nl();
    ch1     = 0;
    s[0]    = RETURN;
    s[1]    = '?';
    strcpy(&(s[2]), restrict_string);
    do {
      bout << "      |#2" << s3 << wwiv::endl;
      bout << "|#9Rstr: |#2" << s2 << wwiv::endl;
      bout << "|#9Togl? ";
      ch1 = onek(s);
      if (ch1 != RETURN && ch1 != SPACE && ch1 != '?') {
        int i = -1;
        for (int i1 = 0; i1 < 16; i1++) {
          if (ch1 == s[i1 + 2]) {
            i = i1;
          }
        }
        if (i > -1) {
          user.ToggleRestrictionFlag(1 << i);
          if (s2[i] == SPACE) {
            s2[i] = s3[i];
          } else {
            s2[i] = SPACE;
          }
        }
        ch1 = 0;
      }
      if (ch1 == '?') {
        ch1 = 0;
        printfile(SRESTRCT_NOEXT);
      }
    } while (!hangup && ch1 == 0);
    a()->users()->WriteUser(&user, user_number);
    bout.nl();
  } else {
    bout << "\r\n|#6No Such User.\r\n\n";
  }
}

#define NET_SEARCH_SUBSTR     0x0001
#define NET_SEARCH_AREACODE   0x0002
#define NET_SEARCH_GROUP      0x0004
#define NET_SEARCH_SC         0x0008
#define NET_SEARCH_AC         0x0010
#define NET_SEARCH_GC         0x0020
#define NET_SEARCH_NC         0x0040
#define NET_SEARCH_PHSUBSTR   0x0080
#define NET_SEARCH_NOCONNECT  0x0100
#define NET_SEARCH_NEXT       0x0200
#define NET_SEARCH_HOPS       0x0400
#define NET_SEARCH_ALL        0xFFFF


void print_net_listing(bool bForcePause) {
  int i, gn = 0;
  char town[5];
  unsigned short slist, cmdbit = 0;
  char substr[81], onx[20], acstr[4], phstr[13];
  char s[255], s1[101], s2[101], bbstype;
  bool bHadPause = false;

  a()->status_manager()->RefreshStatusCache();

  if (!a()->max_net_num()) {
    return;
  }

  write_inst(INST_LOC_NETLIST, 0, INST_FLAGS_NONE);

  if (bForcePause) {
    bHadPause  = a()->user()->HasPause();
    if (bHadPause) {
      a()->user()->ToggleStatusFlag(User::pauseOnPage);
    }
  }
  bool done = false;
  while (!done && !hangup) {
    bout.cls();
    if (a()->max_net_num() > 1) {
      std::set<char> odc;
      onx[0] = 'Q';
      onx[1] = 0;
      int onxi = 1;
      bout.nl();
      for (i = 0; i < a()->max_net_num(); i++) {
        if (i < 9) {
          onx[onxi++] = static_cast<char>(i + '1');
          onx[onxi] = 0;
        } else {
          int odci = (i + 1) / 10;
          odc.insert(static_cast<char>(odci + '0'));
        }
        bout << "|#2" << i + 1 << "|#9)|#1 " << a()->net_networks[i].name << wwiv::endl;
      }
      bout << "|#2Q|#9)|#1 Quit\r\n\n";
      bout << "|#9Which network? |#2";
      if (a()->max_net_num() < 9) {
        char ch = onek(onx);
        if (ch == 'Q') {
          done = true;
        } else {
          i = ch - '1';
        }
      } else {
        string mmk = mmkey(odc);
        if (mmk == "Q") {
          done = true;
        } else {
          i = StringToInt(mmk) - 1;
        }
      }

      if (done) {
        break;
      }

      if ((i < 0) || (i > a()->max_net_num())) {
        continue;
      }
    } else {
      // i is the current network number, if there is only 1, they use it.
      i = 1;
    }

    bool done1 = false;
    bool abort;

    while (!done1) {
      int i2 = i;
      set_net_num(i2);
      abort = false;
      cmdbit = 0;
      slist = 0;
      substr[0] = 0;
      acstr[0] = 0;
      phstr[0] = 0;

      bout.cls();
      bout.nl();
      bout << "|#9Network|#2: |#1" << a()->network_name() << wwiv::endl;
      bout.nl();

      bout << "|#21|#9) = |#1List All\r\n";
      bout << "|#22|#9) = |#1Area Code\r\n";
      bout << "|#23|#9) = |#1Group\r\n";
      bout << "|#24|#9) = |#1Subs Coordinators\r\n";
      bout << "|#25|#9) = |#1Area Coordinators\r\n";
      bout << "|#26|#9) = |#1Group Coordinators\r\n";
      bout << "|#27|#9) = |#1Net Coordinator\r\n";
      bout << "|#28|#9) = |#1BBS Name SubString\r\n";
      bout << "|#29|#9) = |#1Phone SubString\r\n";
      bout << "|#20|#9) = |#1Unconnected Systems\r\n";
      bout << "|#2Q|#9) = |#1Quit NetList\r\n";
      bout.nl();
      bout << "|#9Select: |#2";
      char cmd = onek("Q1234567890");

      switch (cmd) {
      case 'Q':
        if (a()->max_net_num() < 2) {
          done = true;
        }
        done1 = true;
        break;
      case '1':
        cmdbit = NET_SEARCH_ALL;
        break;
      case '2':
        cmdbit = NET_SEARCH_AREACODE;
        bout.nl();
        bout << "|#1Enter Area Code|#2: |#0";
        input(acstr, 3);
        if (strlen(acstr) != 3) {
          abort = true;
        }
        if (!abort) {
          for (i = 0; i < 3; i++) {
            if ((acstr[i] < '0') || (acstr[i] > '9')) {
              abort = true;
            }
          }
        }
        if (abort) {
          bout << "|#6Area code must be a 3-digit number!\r\n";
          pausescr();
          cmdbit = 0;
        }
        break;
      case '3':
        cmdbit = NET_SEARCH_GROUP;
        bout.nl();
        bout << "|#1Enter group number|#2: |#0";
        input(s, 2);
        if ((s[0] == 0) || (atoi(s) < 1)) {
          bout << "|#6Invalid group number!\r\n";
          pausescr();
          cmdbit = 0;
          break;
        }
        gn = atoi(s);
        break;
      case '4':
        cmdbit = NET_SEARCH_SC;
        break;
      case '5':
        cmdbit = NET_SEARCH_AC;
        break;
      case '6':
        cmdbit = NET_SEARCH_GC;
        break;
      case '7':
        cmdbit = NET_SEARCH_NC;
        break;
      case '8':
        cmdbit = NET_SEARCH_SUBSTR;
        bout.nl();
        bout << "|#1Enter SubString|#2: |#0";
        input(substr, 40);
        if (substr[0] == 0) {
          bout << "|#6Enter a substring!\r\n";
          pausescr();
          cmdbit = 0;
        }
        break;
      case '9':
        cmdbit = NET_SEARCH_PHSUBSTR;
        bout.nl();
        bout << "|#1Enter phone substring|#2: |#0";
        input(phstr, 12);
        if (phstr[0] == 0) {
          bout << "|#6Enter a phone substring!\r\n";
          pausescr();
          cmdbit = 0;
        }
        break;
      case '0':
        cmdbit = NET_SEARCH_NOCONNECT;
        break;
      }

      if (!cmdbit) {
        continue;
      }

      if (done1) {
        break;
      }

      bout.nl();
      bout << "|#1Print BBS region info? ";
      bool useregion = yesno();

      BbsListNet bbslist = BbsListNet::ReadBbsDataNet(a()->current_net().dir);
      if (bbslist.empty()) {
        bout << "|#6Error opening BBSDATA.NET in " << a()->current_net().dir << wwiv::endl;
        pausescr();
        continue;
      }
      strcpy(s, "000-000-0000");
      bout.nl(2);

      for (const auto& b : bbslist.node_config()) {
        bool matched = false;
        const auto& csne = b.second;
        if ((csne.forsys == 65535) && (cmdbit != NET_SEARCH_NOCONNECT)) {
          continue;
        }
        strcpy(s1, csne.phone);

        if (csne.other & other_net_coord) {
          bbstype = '&';
        } else if (csne.other & other_group_coord) {
          bbstype = '%';
        } else if (csne.other & other_area_coord) {
          bbstype = '^';
        } else if (csne.other & other_subs_coord) {
          bbstype = '~';
        } else {
          bbstype = ' ';
        }

        strcpy(s2, csne.name);
        for (int i1 = 0; i1 < wwiv::strings::GetStringLength(s2); i1++) {
          s2[i1] = upcase(s2[i1]);
        }

        switch (cmdbit) {
        case NET_SEARCH_ALL:
          matched = true;
          break;
        case NET_SEARCH_AREACODE:
          if (strncmp(acstr, csne.phone, 3) == 0) {
            matched = true;
          }
          break;
        case NET_SEARCH_GROUP:
          if (gn == csne.group) {
            matched = true;
          }
          break;
        case NET_SEARCH_SC:
          if (csne.other & other_subs_coord) {
            matched = true;
          }
          break;
        case NET_SEARCH_AC:
          if (csne.other & other_area_coord) {
            matched = true;
          }
          break;
        case NET_SEARCH_GC:
          if (csne.other & other_group_coord) {
            matched = true;
          }
          break;
        case NET_SEARCH_NC:
          if (csne.other & other_net_coord) {
            matched = true;
          }
          break;
        case NET_SEARCH_SUBSTR:
          if (strstr(s2, substr) != nullptr) {
            matched = true;
          } else {
            char s3[81];
            sprintf(s3, "@%u", csne.sysnum);
            if (strstr(s3, substr) != nullptr) {
              matched = true;
            }
          }
          break;
        case NET_SEARCH_PHSUBSTR:
          if (strstr(s1, phstr) != nullptr) {
            matched = true;
          }
          break;
        case NET_SEARCH_NOCONNECT:
          if (csne.forsys == 65535) {
            matched = true;
          }
          break;
        }

        if (matched) {
          slist++;
          if (!useregion && slist == 1) {
            bout.bpla("|#1 Node  Phone         BBS Name                                 Hop  Next Gr", &abort);
            bout.bpla("|#7-----  ============  ---------------------------------------- === ----- --", &abort);
          } else {
            if (useregion && strncmp(s, csne.phone, 3) != 0) {
              strcpy(s, csne.phone);
              const auto regions_dir = FilePath(a()->config()->datadir(), REGIONS_DIR);
              const auto town_fn = StringPrintf("%s.%-3u", REGIONS_DIR, StringToUnsignedInt(csne.phone));
              string areacode;
              if (File::Exists(regions_dir, town_fn)) {
                sprintf(town, "%c%c%c", csne.phone[4], csne.phone[5], csne.phone[6]);
                areacode = describe_area_code_prefix(atoi(csne.phone), atoi(town));
              } else {
                areacode = describe_area_code(atoi(csne.phone));
              }
              const string line = StringPrintf("\r\n%s%s\r\n", "|#2Region|#0: |#2", areacode.c_str());
              bout.bpla(line, &abort);
              bout.bpla("|#1 Node  Phone         BBS Name                                 Hop  Next Gr", &abort);
              bout.bpla("|#7-----  ============  ---------------------------------------- === ----- --", &abort);
            }
          }
          string line;
          if (cmdbit != NET_SEARCH_NOCONNECT) {
            line = StringPrintf("%5u%c %12s  %-40s %3d %5u %2d",
                    csne.sysnum, bbstype, csne.phone, csne.name,
                    csne.numhops, csne.forsys, csne.group);
          } else {
            line = StringPrintf("%5u%c %12s  %-40s%s%2d",
                    csne.sysnum, bbstype, csne.phone, csne.name, " |17|15--- ----- |#0", csne.group);
          }
          bout.bpla(line, &abort);
        }
      }
      if (!abort && slist) {
        bout.nl();
        bout << "|#1Systems Listed |#7: |#2" << slist;
      }
      bout.nl(2);
      pausescr();
    }
  }
  if (bForcePause && bHadPause) {
    a()->user()->ToggleStatusFlag(User::pauseOnPage);
  }
}

void read_new_stuff() {
  set_language_1(a()->language_number());
}


void mailr() {
  mailrec m, m1;
  filestatusrec fsr;

  unique_ptr<File> pFileEmail(OpenEmailFile(false));
  if (pFileEmail->IsOpen()) {
    int nRecordNumber = pFileEmail->GetLength() / sizeof(mailrec) - 1;
    char c = ' ';
    while (nRecordNumber >= 0 && c != 'Q' && !hangup) {
      pFileEmail->Seek(nRecordNumber * sizeof(mailrec), File::Whence::begin);
      pFileEmail->Read(&m, sizeof(mailrec));
      if (m.touser != 0) {
        pFileEmail->Close();
        do {
          User user;
          a()->users()->ReadUser(&user, m.touser);
          const string unn = a()->names()->UserName(m.touser);
          bout << "|#9  To|#7: |#" << a()->GetMessageColor() << unn << wwiv::endl;
          set_net_num(network_number_from(&m));
          bout << "|#9Subj|#7: |#" << a()->GetMessageColor() << m.title << wwiv::endl;
          if (m.status & status_file) {
            File attachDat(a()->config()->datadir(), ATTACH_DAT);
            if (attachDat.Open(File::modeReadOnly | File::modeBinary)) {
              bool found = false;
              auto lAttachFileSize = attachDat.Read(&fsr, sizeof(fsr));
              while (lAttachFileSize > 0 && !found) {
                if (m.daten == static_cast<uint32_t>(fsr.id)) {
                  bout << "|#9Filename.... |#2" << fsr.filename << " (" << fsr.numbytes << " bytes)|#0\r\n";
                  found = true;
                }
                if (!found) {
                  lAttachFileSize = attachDat.Read(&fsr, sizeof(fsr));
                }
              }
              if (!found) {
                bout << "|#1Filename|#0.... |#2File : Unknown or Missing|#0\r\n";
              }
              attachDat.Close();
            } else {
              bout << "|#1Filename|#0.... |#2|#2File : Unknown or Missing|#0\r\n";
            }
          }
          bool next;
          auto msg = read_type2_message(&(m.msg), (char)(m.anony & 0x0f), true, "email", m.fromsys, m.fromuser);
          display_type2_message(msg, &next);
          bout << "|#2R,D,Q,<space>  : ";
          if (next) {
            c = ' ';
          } else {
            c = onek("QRD ");
          }
          if (c == 'D') {
            pFileEmail = OpenEmailFile(true);
            pFileEmail->Seek(nRecordNumber * sizeof(mailrec), File::Whence::begin);
            pFileEmail->Read(&m1, sizeof(mailrec));
            if (memcmp(&m, &m1, sizeof(mailrec)) == 0) {
              delmail(pFileEmail.get(), nRecordNumber);
              bool found = false;
              if (m.status & status_file) {
                File attachFile(a()->config()->datadir(), ATTACH_DAT);
                if (attachFile.Open(File::modeReadWrite | File::modeBinary)) {
                  auto lAttachFileSize = attachFile.Read(&fsr, sizeof(fsr));
                  while (lAttachFileSize > 0 && !found) {
                    if (m.daten == static_cast<uint32_t>(fsr.id)) {
                      found = true;
                      fsr.id = 0;
                      attachFile.Seek(static_cast<long>(sizeof(filestatusrec)) * -1L, File::Whence::current);
                      attachFile.Write(&fsr, sizeof(filestatusrec));
                      File::Remove(a()->GetAttachmentDirectory().c_str(), fsr.filename);
                    } else {
                      attachFile.Read(&fsr, sizeof(filestatusrec));
                    }
                  }
                  attachFile.Close();
                }
              }
            } else {
              bout << "Mail file changed; try again.\r\n";
            }
            pFileEmail->Close();
            if (!a()->IsUserOnline() && m.touser == 1 && m.tosys == 0) {
              a()->user()->SetNumMailWaiting(a()->user()->GetNumMailWaiting() - 1);
            }
          }
          bout.nl(2);
        } while ((c == 'R') && (!hangup));

        pFileEmail = OpenEmailFile(false);
        if (!pFileEmail->IsOpen()) {
          break;
        }
      }
      nRecordNumber -= 1;
    }
    pFileEmail->Close();
  }
}

void chuser() {
  if (!ValidateSysopPassword() || !so()) {
    return;
  }

  bout << "|#9Enter user to change to: ";
  std::string userName = input(30, true);
  int user_number = finduser1(userName);
  if (user_number > 0) {
    a()->WriteCurrentUser();
    write_qscn(a()->usernum, qsc, false);
    a()->ReadCurrentUser(user_number);
    read_qscn(user_number, qsc, false);
    a()->usernum = static_cast<uint16_t>(user_number);
    a()->SetEffectiveSl(255);
    const string unn = a()->names()->UserName(a()->usernum);
    sysoplog() << StringPrintf("#*#*#* Changed to %s", unn.c_str());
    changedsl();
    a()->UpdateTopScreen();
  } else {
    bout << "|#6Unknown user.\r\n";
  }
}

void zlog() {
  File file(a()->config()->datadir(), ZLOG_DAT);
  if (!file.Open(File::modeReadOnly | File::modeBinary)) {
    return;
  }
  int i = 0;
  bool abort = false;
  zlogrec z;
  file.Read(&z, sizeof(zlogrec));
  bout.nl();
  bout.cls();
  bout.nl(2);
  bout.bpla("|#2  Date     Calls  Active   Posts   Email   Fback    U/L    %Act   T/user", &abort);
  bout.bpla("|#7--------   -----  ------   -----   -----   -----    ---    ----   ------", &abort);
  while (i < 97 && !abort && !hangup && z.date[0] != 0) {
    int nTimePerUser = 0;
    if (z.calls) {
      nTimePerUser = z.active / z.calls;
    }
    char szBuffer[255];
    sprintf(szBuffer, "%s    %4d    %4d     %3d     %3d     %3d    %3d     %3d      %3d|B0",
            z.date, z.calls, z.active, z.posts, z.email, z.fback, z.up, 10 * z.active / 144, nTimePerUser);
    // alternate colors to make it easier to read across the lines
    if (i % 2) {
      bout << "|#1";
      bout.bpla(szBuffer, &abort);
    } else {
      bout << "|#3";
      bout.bpla(szBuffer, &abort);
    }
    ++i;
    if (i < 97) {
      file.Seek(i * sizeof(zlogrec), File::Whence::begin);
      file.Read(&z, sizeof(zlogrec));
    }
  }
  file.Close();
}


void set_user_age() {
  std::unique_ptr<WStatus> pStatus(a()->status_manager()->GetStatus());
  int user_number = 1;
  do {
    User user;
    a()->users()->ReadUser(&user, user_number);
    unsigned int nAge = years_old(user.GetBirthdayMonth(), user.GetBirthdayDay(), user.GetBirthdayYear());
    if (nAge != user.GetAge()) {
      user.SetAge(nAge);
      a()->users()->WriteUser(&user, user_number);
    }
    ++user_number;
  } while (user_number <= pStatus->GetNumUsers());
}


void auto_purge() {
  unsigned int days = 0;
  unsigned int skipsl = 0;

  IniFile ini(FilePath(a()->GetHomeDir(), WWIV_INI), {StrCat("WWIV-", a()->instance_number()), INI_TAG});
  if (ini.IsOpen()) {
    days = ini.value<unsigned int>("AUTO_USER_PURGE");
    skipsl = ini.value<int>("NO_PURGE_SL");
  }
  ini.Close();

  if (days < 60) {
    if (days > 0) {
      sysoplog(false) << "!!! WARNING: Auto-Purge canceled [AUTO_USER_PURGE < 60]";
      sysoplog(false) << "!!! WARNING: Edit WWIV.INI and Fix this";
    }
    return;
  }

  time_t tTime = time(nullptr);
  int user_number = 1;
  sysoplog(false) << "Auto-Purged Inactive Users (over " << days << " days, SL less than " << skipsl << ")";

  do {
    User user;
    a()->users()->ReadUser(&user, user_number);
    if (!user.IsExemptAutoDelete()) {
      unsigned int d = static_cast<unsigned int>((tTime - user.GetLastOnDateNumber()) / SECONDS_PER_DAY);
      // if user is not already deleted && SL<NO_PURGE_SL && last_logon
      // greater than AUTO_USER_PURGE days ago
      if (!user.IsUserDeleted() && user.GetSl() < skipsl && d > days) {
        sysoplog(false) << "*** AUTOPURGE: Deleted User: #" << user_number << " " << user.GetName();
        deluser(user_number);
      }
    }
    ++user_number;
  } while (user_number <= a()->status_manager()->GetUserCount());
}


void beginday(bool displayStatus) {
  if ((a()->GetBeginDayNodeNumber() > 0)
      && (a()->instance_number() != a()->GetBeginDayNodeNumber())) {
    // If BEGINDAYNODENUMBER is > 0 or defined in WWIV.INI only handle beginday events on that node number
    a()->status_manager()->RefreshStatusCache();
    return;
  }
  WStatus *pStatus = a()->status_manager()->BeginTransaction();
  pStatus->ValidateAndFixDates();

  if (date() == pStatus->GetLastDate()) {
    a()->status_manager()->CommitTransaction(pStatus);
    return;
  }
  if (displayStatus) {
    bout << "|#7* |#1Running Daily Maintenance...\r\n";
    bout << "  |#7* |#1Updating system activity...\r\n";
  }

  zlogrec z;
  strcpy(z.date, pStatus->GetLastDate());
  z.active            = pStatus->GetMinutesActiveToday();
  z.calls             = pStatus->GetNumCallsToday();
  z.posts             = pStatus->GetNumLocalPosts();
  z.email             = pStatus->GetNumEmailSentToday();
  z.fback             = pStatus->GetNumFeedbackSentToday();
  z.up                = pStatus->GetNumUploadsToday();
  pStatus->NewDay();

  if (displayStatus) {
    bout << "  |#7* |#1Cleaning up log files...\r\n";
  }
  File::Remove(a()->config()->gfilesdir(), pStatus->GetLogFileName(2));

  if (displayStatus) {
    bout << "  |#7* |#1Updating ZLOG information...\r\n";
  }
  File fileZLog(a()->config()->datadir(), ZLOG_DAT);
  zlogrec z1;
  if (!fileZLog.Open(File::modeReadWrite | File::modeBinary)) {
    fileZLog.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyNone);
    z1.date[0]  = '\0';
    z1.active   = 0;
    z1.calls    = 0;
    z1.posts    = 0;
    z1.email    = 0;
    z1.fback    = 0;
    z1.up       = 0;
    for (int i = 0; i < 97; i++) {
      fileZLog.Write(&z1, sizeof(zlogrec));
    }
  } else {
    for (int i = 96; i >= 1; i--) {
      fileZLog.Seek((i - 1) * sizeof(zlogrec), File::Whence::begin);
      fileZLog.Read(&z1, sizeof(zlogrec));
      fileZLog.Seek(i * sizeof(zlogrec), File::Whence::begin);
      fileZLog.Write(&z1, sizeof(zlogrec));
    }
  }
  fileZLog.Seek(0L, File::Whence::begin);
  fileZLog.Write(&z, sizeof(zlogrec));
  fileZLog.Close();

  if (displayStatus) {
    bout << "  |#7* |#1Updating STATUS.DAT...\r\n";
  }
  int nus = a()->config()->config()->maxusers - pStatus->GetNumUsers();

  a()->status_manager()->CommitTransaction(pStatus);
  if (displayStatus) {
    bout << "  |#7* |#1Checking system directories and user space...\r\n";
  }

  long fk = File::GetFreeSpaceForPath(a()->config()->datadir());

  if (fk < 512) {
    ssm(1, 0) << "Only " << fk << "k free in data directory.";
  }
  if (!a()->config()->config()->closedsystem && nus < 15) {
    ssm(1, 0) << "Only " << nus << " new user slots left.";
  }
  if (!a()->beginday_cmd.empty()) {
    const std::string commandLine = stuff_in(a()->beginday_cmd, create_chain_file(), "", "", "", "");
    ExecuteExternalProgram(commandLine, a()->GetSpawnOptions(SPAWNOPT_BEGINDAY));
  }
  if (displayStatus) {
    bout << "  |#7* |#1Purging inactive users (if enabled)...\r\n";
  }
  auto_purge();
  if (displayStatus) {
    bout << "  |#7* |#1Updating user ages...\r\n";
  }
  set_user_age();
  if (displayStatus) {
    bout << "|#7* |#1Done!\r\n";
  }

  sysoplog(false);
  sysoplog(false) << "* Ran Daily Maintenance...";
  sysoplog(false);
}


