/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <string>

#include "bbs/wwiv.h"

#include "bbs/pause.h"
#include "bbs/qscan.h"
#include "bbs/wconstants.h"
#include "bbs/keycodes.h"
#include "bbs/wstatus.h"
#include "core/strings.h"
#include "core/wwivassert.h"

// from qwk.c
void qwk_menu();

using std::string;
using wwiv::bbs::TempDisablePause;
using wwiv::bbs::SaveQScanPointers;

void kill_old_email() {
  mailrec m, m1;
  WUser user;
  filestatusrec fsr;

  bout << "|#5List mail starting at most recent? ";
  int forward = (yesno());
  WFile *pFileEmail = OpenEmailFile(false);
  WWIV_ASSERT(pFileEmail);
  if (!pFileEmail->IsOpen()) {
    bout << "\r\nNo mail.\r\n";
    return;
  }
  int max = static_cast< int >(pFileEmail->GetLength() / sizeof(mailrec));
  int cur = 0;
  if (forward) {
    cur = max - 1;
  }

  bool done = false;
  do {
    pFileEmail->Seek(cur * sizeof(mailrec), WFile::seekBegin);
    pFileEmail->Read(&m, sizeof(mailrec));
    while ((m.fromsys != 0 || m.fromuser != GetSession()->usernum || m.touser == 0) && cur < max && cur >= 0) {
      if (forward) {
        --cur;
      } else {
        ++cur;
      }
      if (cur < max && cur >= 0) {
        pFileEmail->Seek(cur * sizeof(mailrec), WFile::seekBegin);
        pFileEmail->Read(&m, sizeof(mailrec));
      }
    }
    if (m.fromsys != 0 || m.fromuser != GetSession()->usernum || m.touser == 0 || cur >= max || cur < 0) {
      done = true;
    } else {
      pFileEmail->Close();
      delete pFileEmail;

      bool done1 = false;
      do {
        bout.nl();
        bout << "|#1  To|#9: |#" << GetSession()->GetMessageColor();

        if (m.tosys == 0) {
          GetApplication()->GetUserManager()->ReadUser(&user, m.touser);
          string tempName = user.GetUserNameAndNumber(m.touser);
          if ((m.anony & (anony_receiver | anony_receiver_pp | anony_receiver_da))
              && ((getslrec(GetSession()->GetEffectiveSl()).ability & ability_read_email_anony) == 0)) {
            tempName = ">UNKNOWN<";
          }
          bout << tempName;
          bout.nl();
        } else {
          bout << "#" << m.tosys << " @" << m.tosys << wwiv::endl;
        }
        bout.bprintf("|#1Subj|#9: |#%d%60.60s\r\n", GetSession()->GetMessageColor(), m.title);
        time_t lCurrentTime;
        time(&lCurrentTime);
        int nDaysAgo = static_cast<int>((lCurrentTime - m.daten) / HOURS_PER_DAY_FLOAT / SECONDS_PER_HOUR_FLOAT);
        bout << "|#1Sent|#9: |#" << GetSession()->GetMessageColor() << nDaysAgo << " days ago" << wwiv::endl;
        if (m.status & status_file) {
          WFile fileAttach(syscfg.datadir, ATTACH_DAT);
          if (fileAttach.Open(WFile::modeBinary | WFile::modeReadOnly)) {
            bool found = false;
            long l1 = fileAttach.Read(&fsr, sizeof(fsr));
            while (l1 > 0 && !found) {
              if (m.daten == static_cast<unsigned long>(fsr.id)) {
                bout << "|#1Filename|#0.... |#2" << fsr.filename << " (" << fsr.numbytes << " bytes)|#0" << wwiv::endl;
                found = true;
              }
              if (!found) {
                l1 = fileAttach.Read(&fsr, sizeof(fsr));
              }
            }
            if (!found) {
              bout << "|#1Filename|#0.... |#2Unknown or missing|#0\r\n";
            }
            fileAttach.Close();
          } else {
            bout << "|#1Filename|#0.... |#2Unknown or missing|#0\r\n";
          }
        }
        bout.nl();
        bout << "|#9(R)ead, (D)elete, (N)ext, (Q)uit : ";
        char ch = onek("QRDN");
        switch (ch) {
        case 'Q':
          done1   = true;
          done    = true;
          break;
        case 'N':
          done1 = true;
          if (forward) {
            --cur;
          } else {
            ++cur;
          }
          if (cur >= max || cur < 0) {
            done = true;
          }
          break;
        case 'D': {
          done1 = true;
          WFile *pFileEmail = OpenEmailFile(true);
          pFileEmail->Seek(cur * sizeof(mailrec), WFile::seekBegin);
          pFileEmail->Read(&m1, sizeof(mailrec));
          if (memcmp(&m, &m1, sizeof(mailrec)) == 0) {
            delmail(pFileEmail, cur);
            bool found = false;
            if (m.status & status_file) {
              WFile fileAttach(syscfg.datadir, ATTACH_DAT);
              if (fileAttach.Open(WFile::modeBinary | WFile::modeReadWrite)) {
                long l1 = fileAttach.Read(&fsr, sizeof(fsr));
                while (l1 > 0 && !found) {
                  if (m.daten == static_cast<unsigned long>(fsr.id)) {
                    found = true;
                    fsr.id = 0;
                    fileAttach.Seek(static_cast<long>(sizeof(filestatusrec)) * -1L, WFile::seekCurrent);
                    fileAttach.Write(&fsr, sizeof(filestatusrec));
                    WFile::Remove(GetApplication()->GetAttachmentDirectory().c_str(), fsr.filename);
                  } else {
                    l1 = fileAttach.Read(&fsr, sizeof(filestatusrec));
                  }
                }
                fileAttach.Close();
              }
            }
            bout.nl();
            if (found) {
              bout << "Mail and file deleted.\r\n\n";
              sysoplogf("Deleted mail and attached file %s.", fsr.filename);
            } else {
              bout << "Mail deleted.\r\n\n";
              sysoplogf("Deleted mail sent to %s", user.GetUserNameAndNumber(m1.touser));
            }
          } else {
            bout << "Mail file changed; try again.\r\n";
          }
          pFileEmail->Close();
        }
        break;
        case 'R': {
          bout.nl(2);
          bout.bprintf("|#1Subj|#9: |#%d%60.60s\r\n", GetSession()->GetMessageColor(), m.title);
          bool next;
          read_message1(&m.msg, static_cast<char>(m.anony & 0x0f), false, &next, "email", 0, 0);
        }
        break;
        }
      } while (!hangup && !done1);
      pFileEmail = OpenEmailFile(false);
      WWIV_ASSERT(pFileEmail);
      if (!pFileEmail->IsOpen()) {
        break;
      }
    }
  } while (!done && !hangup);
  pFileEmail->Close();
  delete pFileEmail;
}


void list_users(int mode) {
  subboardrec s;
  directoryrec d;
  memset(&s, 0, sizeof(subboardrec));
  memset(&d, 0, sizeof(directoryrec));
  WUser user;
  char szFindText[21];

  if (usub[GetSession()->GetCurrentMessageArea()].subnum == -1 && mode == LIST_USERS_MESSAGE_AREA) {
    bout << "\r\n|#6No Message Area Available!\r\n\n";
    return;
  }
  if (udir[GetSession()->GetCurrentFileArea()].subnum == -1 && mode == LIST_USERS_FILE_AREA) {
    bout << "\r\n|#6 No Dirs Available.\r\n\n";
    return;
  }

  int snum = GetSession()->usernum;

  bout.nl();
  bout << "|#5Sort by user number? ";
  bool bSortByUserNumber = yesno();
  bout.nl();
  bout << "|#5Search for a name or city? ";
  if (yesno()) {
    bout.nl();
    bout << "|#5Enter text to find: ";
    input(szFindText, 10, true);
  } else {
    szFindText[0] = '\0';
  }

  if (mode == LIST_USERS_MESSAGE_AREA) {
    s = subboards[usub[GetSession()->GetCurrentMessageArea()].subnum];
  } else {
    d = directories[udir[GetSession()->GetCurrentFileArea()].subnum];
  }

  bool abort  = false;
  bool next   = false;
  int p       = 0;
  int num     = 0;
  bool found  = true;
  int count   = 0;
  int ncnm    = 0;
  int numscn  = 0;
  int color   = 3;
  GetSession()->WriteCurrentUser();
  write_qscn(GetSession()->usernum, qsc, false);
  GetApplication()->GetStatusManager()->RefreshStatusCache();

  WFile userList(syscfg.datadir, USER_LST);
  int nNumUserRecords = GetApplication()->GetUserManager()->GetNumberOfUserRecords();

  for (int i = 0; (i < nNumUserRecords) && !abort && !hangup; i++) {
    GetSession()->usernum = 0;
    if (ncnm > 5) {
      count++;
      bout << "|#" << color << ".";
      if (count == NUM_DOTS) {
        osan("\r", &abort, &next);
        osan("|#2Searching ", &abort, &next);
        color++;
        count = 0;
        if (color == 4) {
          color++;
        }
        if (color == 7) {
          color = 0;
        }
      }
    }
    if (p == 0 && found) {
      bout.cls();
      char szTitleLine[255];
      sprintf(szTitleLine, "[ %s User Listing ]", syscfg.systemname);
      if (okansi()) {
        bout.litebar(szTitleLine);
      } else {
        int i1;
        for (i1 = 0; i1 < 78; i1++) {
          bputch(45);
        }
        bout.nl();
        bout << "|#5" << szTitleLine;
        bout.nl();
        for (i1 = 0; i1 < 78; i1++) {
          bputch(45);
        }
        bout.nl();
      }
      bout.Color(FRAME_COLOR);
      pla("\xD5\xCD\xCD\xCD\xCD\xCD\xCD\xD1\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xD1\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xD1\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xD1\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xB8",
          &abort);
      found = false;
    }

    int nUserNumber = (bSortByUserNumber) ? i + 1 : smallist[i].number;
    GetApplication()->GetUserManager()->ReadUser(&user, nUserNumber);
    read_qscn(nUserNumber, qsc, false);
    changedsl();
    bool in_qscan = (qsc_q[usub[GetSession()->GetCurrentMessageArea()].subnum / 32] & (1L <<
                     (usub[GetSession()->GetCurrentMessageArea()].subnum % 32))) ? true : false;
    bool ok = true;
    if (user.IsUserDeleted()) {
      ok = false;
    }
    if (mode == LIST_USERS_MESSAGE_AREA) {
      if (user.GetSl() < s.readsl) {
        ok = false;
      }
      if (user.GetAge() < (s.age & 0x7f)) {
        ok = false;
      }
      if (s.ar != 0 && !user.HasArFlag(s.ar)) {
        ok = false;
      }
    }
    if (mode == LIST_USERS_FILE_AREA) {
      if (user.GetDsl() < d.dsl) {
        ok = false;
      }
      if (user.GetAge() < d.age) {
        ok = false;
      }
      if (d.dar != 0 && !user.HasDarFlag(d.dar)) {
        ok = false;
      }
    }
    if (szFindText[0] != '\0') {
      char s5[ 41 ];
      strcpy(s5, user.GetCity());
      if (!strstr(user.GetName(), szFindText) &&
          !strstr(strupr(s5), szFindText) &&
          !strstr(user.GetState(), szFindText)) {
        ok = false;
      }
    }
    if (ok) {
      found = true;
      //bout.backline();
      bout.clreol();
      if (user.GetLastBaudRate() > 32767 || user.GetLastBaudRate() < 300) {
        user.SetLastBaudRate(33600);
      }

      char szCity[ 81 ];
      if (user.GetCity()[0] == '\0') {
        strcpy(szCity, "Unknown");
      } else {
        char s5[ 81 ];
        strcpy(s5, user.GetCity());
        s5[19] = '\0';
        sprintf(szCity, "%s, %s", s5, user.GetState());
      }
      string properName = properize(user.GetName());
      char szUserListLine[ 255 ];
      sprintf(szUserListLine,
              "|#%d\xB3|#9%5d |#%d\xB3|#6%c|#1%-20.20s|#%d\xB3|#2 %-24.24s|#%d\xB3 |#1%-9s |#%d\xB3  |#3%-5u  |#%d\xB3",
              FRAME_COLOR, nUserNumber, FRAME_COLOR, in_qscan ? '*' : ' ', properName.c_str(),
              FRAME_COLOR, szCity, FRAME_COLOR, user.GetLastOn(), FRAME_COLOR,
              user.GetLastBaudRate(), FRAME_COLOR);
      pla(szUserListLine, &abort);
      num++;
      if (in_qscan) {
        numscn++;
      }
      ++p;
      if (p == (GetSession()->GetCurrentUser()->GetScreenLines() - 6)) {
        //bout.backline();
        bout.clreol();
        bout.Color(FRAME_COLOR);
        pla("\xD4\xCD\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBE",
            &abort);
        bout << "|#1[Enter] to continue or Q=Quit : ";
        char ch = onek("Q\r ");
        switch (ch) {
        case 'Q':
          abort = true;
          i = GetApplication()->GetStatusManager()->GetUserCount();
          break;
        case SPACE:
        case RETURN:
          p = 0;
          break;
        }
      }
    } else {
      ncnm++;
    }
  }
  //bout.backline();
  bout.clreol();
  bout.Color(FRAME_COLOR);
  pla("\xD4\xCD\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCF\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xCD\xBE",
      &abort);
  if (!abort) {
    bout.nl(2);
    bout << "|#1" << num << " user(s) have access and " << numscn << " user(s) scan this subboard.";
    bout.nl();
    pausescr();
  }
  GetSession()->ReadCurrentUser(snum);
  read_qscn(snum, qsc, false);
  GetSession()->usernum = snum;
  changedsl();
}


void time_bank() {
  char s[81], bc[81];
  int i;
  double nsln;

  bout.nl();
  if (GetSession()->GetCurrentUser()->GetSl() <= syscfg.newusersl) {
    bout << "|#6You must be validated to access the timebank.\r\n";
    return;
  }
  if (GetSession()->GetCurrentUser()->GetTimeBankMinutes() > getslrec(GetSession()->GetEffectiveSl()).time_per_logon) {
    GetSession()->GetCurrentUser()->SetTimeBankMinutes(getslrec(GetSession()->GetEffectiveSl()).time_per_logon);
  }

  if (okansi()) {
    strcpy(bc, "Ú¿ÀÙÄ³ÂÃÅ´Á");
  } else {
    strcpy(bc, "++++-|+++++");
  }

  bool done = false;
  do {
    bout.cls();
    bout << "|#5WWIV TimeBank\r\n";
    bout.nl();
    bout << "|#2D|#9)eposit Time\r\n";
    bout << "|#2W|#9)ithdraw Time\r\n";
    bout << "|#2Q|#9)uit\r\n";
    bout.nl();
    bout << "|#9Balance: |#2" << GetSession()->GetCurrentUser()->GetTimeBankMinutes() << "|#9 minutes\r\n";
    bout << "|#9Time Left: |#2" << static_cast<int>(nsl() / 60) << "|#9 minutes\r\n";
    bout.nl();
    bout << "|#9(|#2Q|#9=|#1Quit|#9) [|#2Time Bank|#9] Enter Command: |#2";
    bout.mpl(1);
    char c = onek("QDW");
    switch (c) {
    case 'D':
      bout.nl();
      bout << "|#1Deposit how many minutes: ";
      input(s, 3, true);
      i = atoi(s);
      if (i > 0) {
        nsln = nsl();
        if ((i + GetSession()->GetCurrentUser()->GetTimeBankMinutes()) > getslrec(
              GetSession()->GetEffectiveSl()).time_per_logon) {
          i = getslrec(GetSession()->GetEffectiveSl()).time_per_logon - GetSession()->GetCurrentUser()->GetTimeBankMinutes();
        }
        if (i > (nsln / SECONDS_PER_MINUTE_FLOAT)) {
          i = static_cast<int>(nsln / SECONDS_PER_MINUTE_FLOAT);
        }
        GetSession()->GetCurrentUser()->SetTimeBankMinutes(GetSession()->GetCurrentUser()->GetTimeBankMinutes() +
            static_cast<unsigned short>(i));
        GetSession()->GetCurrentUser()->SetExtraTime(GetSession()->GetCurrentUser()->GetExtraTime() - static_cast<float>
            (i * SECONDS_PER_MINUTE_FLOAT));
        GetSession()->localIO()->tleft(false);
      }
      break;
    case 'W':
      bout.nl();
      if (GetSession()->GetCurrentUser()->GetTimeBankMinutes() == 0) {
        break;
      }
      bout << "|#1Withdraw How Many Minutes: ";
      input(s, 3, true);
      i = atoi(s);
      if (i > 0) {
        nsln = nsl();
        if (i > GetSession()->GetCurrentUser()->GetTimeBankMinutes()) {
          i = GetSession()->GetCurrentUser()->GetTimeBankMinutes();
        }
        GetSession()->GetCurrentUser()->SetTimeBankMinutes(GetSession()->GetCurrentUser()->GetTimeBankMinutes() -
            static_cast<unsigned short>(i));
        GetSession()->GetCurrentUser()->SetExtraTime(GetSession()->GetCurrentUser()->GetExtraTime() + static_cast<float>
            (i * SECONDS_PER_MINUTE_FLOAT));
        GetSession()->localIO()->tleft(false);
      }
      break;
    case 'Q':
      done = true;
      break;
    }
  } while (!done && !hangup);
}


int getnetnum(const char *pszNetworkName) {
  WWIV_ASSERT(pszNetworkName);
  for (int i = 0; i < GetSession()->GetMaxNetworkNumber(); i++) {
    if (wwiv::strings::IsEqualsIgnoreCase(net_networks[i].name, pszNetworkName)) {
      return i;
    }
  }
  return -1;
}


void uudecode(const char *pszInputFileName, const char *pszOutputFileName) {
  bout << "|#2Now UUDECODING " << pszInputFileName;
  bout.nl();

  char szCmdLine[ MAX_PATH ];
  sprintf(szCmdLine, "UUDECODE %s %s", pszInputFileName, pszOutputFileName);
  ExecuteExternalProgram(szCmdLine, EFLAG_NONE);    // run command
  WFile::Remove(pszInputFileName);        // delete the input file
}

void Packers() {
  do {
    bout.nl();
    bout << "|#2Message Packet Options:\r\n";
    bout.nl();
    if (GetSession()->internal_qwk_enabled()) {
      bout << "|#9[|#2I|#9] Internal WWIV QWK\r\n";
    }
    if (GetSession()->wwivmail_enabled()) {
      bout << "|#9[|#2W|#9] WWIVMail/QWK\r\n";
    }
    bout << "|#9[|#2Z|#9] Zipped ASCII Text\r\n";
    bout << "|#9[|#2C|#9] Configure Sub Scan\r\n";
    bout << "|#9[|#2Q|#9] Quit back to BBS!\r\n";
    bout.nl();
    bout << "|#9Choice : ";
    char ch = onek("WIZCQ\r ");
    switch (ch) {
    case 'W': {
      if (GetSession()->wwivmail_enabled()) {
        // We used to write STATUS_DAT here.  I don't think we need to anymore.
        GetSession()->localIO()->set_protect(0);
        sysoplog("@ Ran WWIVMail/QWK");
        string chain_file = create_chain_file();
        string command_line = wwiv::strings::StringPrintf("wwivqwk %s", chain_file.c_str());
        ExecuteExternalProgram(command_line, EFLAG_FOSSIL);
        return;
      }
    }
    case 'I':
      if (GetSession()->internal_qwk_enabled()) {
        qwk_menu();
      }
      break;
    case 'Z':
      // TODO(rushfan): Merge this with the code in DownloadPosts
      bout << "|#5This could take quite a while.  Are you sure? ";
      if (yesno()) {
        TempDisablePause disable_pause;
        SaveQScanPointers save_qscan;
        bout << "\r\nPlease wait...\r\n";
        GetSession()->localIO()->set_x_only(1, "posts.txt", 0);
        bool ac = false;
        if (uconfsub[1].confnum != -1 && okconf(GetSession()->GetCurrentUser())) {
          ac = true;
        }
        preload_subs();
        nscan();
        GetSession()->localIO()->set_x_only(0, nullptr, 0);
        add_arc("offline", "posts.txt", 0);
        bool sent = download_temp_arc("offline", false);
        if (!sent) {
          // If the file was not downloaded, restore the old qscan pointers.
          save_qscan.restore();
        }
      } else {
        bout << "|#6Aborted.\r\n";
      }
      return;
    case 'C':
      bout.cls();
      config_qscan();
      bout.cls();
      break;
    default:
      return;
    }
  } while (!hangup);
}

