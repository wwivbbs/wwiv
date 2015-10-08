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

#include "bbs/confutil.h"
#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/wwiv.h"
#include "bbs/instmsg.h"
#include "bbs/menuspec.h"
#include "bbs/menusupp.h"
#include "bbs/multinst.h"
#include "core/strings.h"

using namespace wwiv::menus;

/* ---------------------------------------------------------------------- */
/* menuspec.cpp - Menu Specific support functions                           */
/*                                                                        */
/* Functions that dont have any direct WWIV function go in here           */
/* ie, functions to help emulate other BBS's.                             */
/* ---------------------------------------------------------------------- */


/**
 *  Download a file
 *
 *  pszDirFileName      = fname of your directory record
 *  pszDownloadFileName = Filename to download
 *  bFreeDL             = true if this is a free download
 *  bTitle              = true if title is to be shown with file info
 */
int MenuDownload(const char *pszDirFileName, const char *pszDownloadFileName, bool bFreeDL, bool bTitle) {
  int bOkToDL;
  uploadsrec u;
  WUser ur;
  char s1[81], s2[81];
  bool abort = false;

  int dn = FindDN(pszDirFileName);

  if (dn == -1) {
    MenuSysopLog("DLDNF");                  /* DL - DIR NOT FOUND */
    return 0;
  }
  dliscan1(dn);
  int nRecordNumber = recno(pszDownloadFileName);
  if (nRecordNumber <= 0) {
    checka(&abort);
    if (abort) {
      return -1;
    } else {
      MenuSysopLog("DLFNF");                /* DL - FILE NOT FOUND */
      return 0;
    }
  }
  bool ok = true;
  while ((nRecordNumber > 0) && ok && !hangup) {
    session()->localIO()->tleft(true);
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, nRecordNumber);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    bout.nl();

    if (bTitle) {
      bout << "Directory  : " << directories[dn].name << wwiv::endl;
    }
    bOkToDL = printfileinfo(&u, dn);


    if (strncmp(u.filename, "WWIV4", 5) == 0 && !application()->HasConfigFlag(OP_FLAGS_NO_EASY_DL)) {
      bOkToDL = 1;
    } else {
      if (!ratio_ok()) {
        return -1;
      }
    }
    if (bOkToDL || bFreeDL) {
      write_inst(INST_LOC_DOWNLOAD, udir[session()->GetCurrentFileArea()].subnum, INST_FLAGS_NONE);
      sprintf(s1, "%s%s", directories[dn].path, u.filename);
      if (directories[dn].mask & mask_cdrom) {
        sprintf(s2, "%s%s", directories[dn].path, u.filename);
        sprintf(s1, "%s%s", syscfgovr.tempdir, u.filename);
        if (!File::Exists(s1)) {
          copyfile(s2, s1, false);
        }
      }
      bool sent = false;
      if (bOkToDL == -1) {
        send_file(s1, &sent, &abort, u.filename, dn, -2L);
      } else {
        send_file(s1, &sent, &abort, u.filename, dn, u.numbytes);
      }

      if (sent) {
        if (!bFreeDL) {
          session()->user()->SetFilesDownloaded(session()->user()->GetFilesDownloaded() + 1);
          session()->user()->SetDownloadK(session()->user()->GetDownloadK() + static_cast<int>
              (bytes_to_k(u.numbytes)));
        }
        ++u.numdloads;
        fileDownload.Open(File::modeBinary | File::modeReadWrite);
        FileAreaSetRecord(fileDownload, nRecordNumber);
        fileDownload.Write(&u, sizeof(uploadsrec));
        fileDownload.Close();

        sysoplogf("Downloaded \"%s\"", u.filename);

        if (syscfg.sysconfig & sysconfig_log_dl) {
          application()->users()->ReadUser(&ur, u.ownerusr);
          if (!ur.IsUserDeleted()) {
            if (date_to_daten(ur.GetFirstOn()) < static_cast<time_t>(u.daten)) {
              ssm(u.ownerusr, 0, "%s downloaded '%s' on %s",
                  session()->user()->GetUserNameAndNumber(session()->usernum),
                  u.filename, date());
            }
          }
        }
      }

      bout.nl(2);
      bout.bprintf("Your ratio is now: %-6.3f\r\n", ratio());

      if (session()->IsUserOnline()) {
        application()->UpdateTopScreen();
      }
    } else {
      bout << "\r\n\nNot enough time left to D/L.\r\n";
    }
    if (abort) {
      ok = false;
    } else {
      nRecordNumber = nrecno(pszDownloadFileName, nRecordNumber);
    }
  }
  return (abort) ? -1 : 1;
}


int FindDN(const char *pszDownloadFileName) {
  for (int i = 0; (i < session()->num_dirs); i++) {
    if (wwiv::strings::IsEqualsIgnoreCase(directories[i].filename, pszDownloadFileName)) {
      return i;
    }
  }
  return -1;
}


/**
 * Run a Door (chain)
 *
 * pszDoor = Door description to run
 * bFree  = If true, security on door will not back checked
 */
bool MenuRunDoorName(const char *pszDoor, bool bFree) {
  int nDoorNumber = FindDoorNo(pszDoor);
  return (nDoorNumber >= 0) ? MenuRunDoorNumber(nDoorNumber, bFree) : false;
}


bool MenuRunDoorNumber(int nDoorNumber, bool bFree) {
  if (!bFree && !ValidateDoorAccess(nDoorNumber)) {
    return false;
  }

  run_chain(nDoorNumber);
  return true;
}



int FindDoorNo(const char *pszDoor) {
  for (int i = 0; i < session()->GetNumberOfChains(); i++) {
    if (wwiv::strings::IsEqualsIgnoreCase(chains[i].description, pszDoor)) {
      return i;
    }
  }

  return -1;
}


bool ValidateDoorAccess(int nDoorNumber) {
  int inst = inst_ok(INST_LOC_CHAINS, nDoorNumber + 1);
  if (inst != 0) {
    char szChainInUse[255];
    sprintf(szChainInUse,  "|#2Chain %s is in use on instance %d.  ", chains[nDoorNumber].description, inst);
    if (!(chains[nDoorNumber].ansir & ansir_multi_user)) {
      bout << szChainInUse << " Try again later.\r\n";
      return false;
    } else {
      bout << szChainInUse << " Care to join in? ";
      if (!(yesno())) {
        return false;
      }
    }
  }
  chainfilerec c = chains[nDoorNumber];
  if ((c.ansir & ansir_ansi) && !okansi()) {
    return false;
  }
  if ((c.ansir & ansir_local_only) && session()->using_modem) {
    return false;
  }
  if (c.sl > session()->GetEffectiveSl()) {
    return false;
  }
  if (c.ar && !session()->user()->HasArFlag(c.ar)) {
    return false;
  }
  if (application()->HasConfigFlag(OP_FLAGS_CHAIN_REG) && chains_reg && session()->GetEffectiveSl() < 255) {
    chainregrec r = chains_reg[ nDoorNumber ];
    if (r.maxage) {
      if (r.minage > session()->user()->GetAge() || r.maxage < session()->user()->GetAge()) {
        return false;
      }
    }
  }
  // passed all the checks, return true
  return true;
}


/* ----------------------- */
/* End of run door section */
/* ----------------------- */
void ChangeSubNumber() {
  bout << "|#7Select Sub number : |#0";

  char* s = mmkey(0);
  for (int i = 0; (i < session()->num_subs) && (usub[i].subnum != -1); i++) {
    if (wwiv::strings::IsEquals(usub[i].keys, s)) {
      session()->SetCurrentMessageArea(i);
    }
  }
}


void ChangeDirNumber() {
  bool done = false;
  while (!done && !hangup) {
    bout << "|#7Select Dir number : |#0";

    char* s = mmkey(1);

    if (s[0] == '?') {
      DirList();
      bout.nl();
      continue;
    }
    for (int i = 0; i < session()->num_dirs; i++) {
      if (wwiv::strings::IsEquals(udir[i].keys, s)) {
        session()->SetCurrentFileArea(i);
        done = true;
      }
    }
  }
}


// have a little conference ability...
void SetMsgConf(int iConf) {
  int nc;
  confrec *cp = nullptr;
  userconfrec *uc = nullptr;

  get_conf_info(CONF_SUBS, &nc, &cp, nullptr, nullptr, &uc);

  for (int i = 0; (i < MAX_CONFERENCES) && (uc[i].confnum != -1); i++) {
    if (iConf == cp[uc[i].confnum].designator) {
      setuconf(CONF_SUBS, i, -1);
      break;
    }
  }
}


void SetDirConf(int iConf) {
  int nc;
  confrec *cp = nullptr;
  userconfrec *uc = nullptr;

  get_conf_info(CONF_DIRS, &nc, &cp, nullptr, nullptr, &uc);

  for (int i = 0; (i < MAX_CONFERENCES) && (uc[i].confnum != -1); i++) {
    if (iConf == cp[uc[i].confnum].designator) {
      setuconf(CONF_DIRS, i, -1);
      break;
    }
  }
}


void EnableConf() {
  tmp_disable_conf(false);
}


void DisableConf() {
  tmp_disable_conf(true);
}


void SetNewScanMsg() {
  sysoplog("Select Subs");
  config_qscan();
}


void ReadMessages() {
  bout << "\r\n|#8Which messages?\r\n|#7(N)ew (A)ll (Q)uit : ";
  char chWhichMessages = onek("QNA");
  if (chWhichMessages == 'Q') {
    return;
  }

  while (!hangup) {
    bout << "|#8Which Subs?\r\n";
    bout << "|#7(S)elected  (A)ll Subs  (L)ist  (Q)uit or Sub# : |#0";

    std::string subsText;
    input(&subsText, 3);

    if (subsText[0] == 'Q') {
      return;
    }


    if (subsText[0] == '?' || subsText[0] == 'L') {
      SubList();
    }

    if (subsText[0] == 'S') {
      if (chWhichMessages == 'N') {
        // Read new messages in the qscan subs
        ReadSelectedMessages(RM_QSCAN_MSGS, RM_QSCAN_SUBS);
        return;
      }
      if (chWhichMessages == 'A') {
        // Read all messages in the qscan subs
        ReadSelectedMessages(RM_ALL_MSGS, RM_QSCAN_SUBS);
      }
      break;
    }
    if (subsText[0] == 'A') {
      if (chWhichMessages == 'N') {
        // Read new messages in all of the subs
        ReadSelectedMessages(RM_QSCAN_MSGS, RM_ALL_SUBS);
      }
      if (chWhichMessages == 'A') {
        // Read new messages in all of the subs
        ReadSelectedMessages(RM_ALL_MSGS, RM_ALL_SUBS);
      }
      break;
    }
    if (isdigit(subsText[0])) {
      for (int iTemp = 0; (iTemp < session()->num_subs) && (usub[iTemp].subnum != -1); iTemp++) {
        if (subsText == usub[iTemp].keys) {
          session()->SetCurrentMessageArea(iTemp);

          if (chWhichMessages == 'N') {
            // read new messages in the current sub
            ReadSelectedMessages(RM_QSCAN_MSGS, session()->GetCurrentMessageArea());
          }
          if (chWhichMessages == 'A') {
            // Read all messages in the current sub
            ReadSelectedMessages(RM_ALL_MSGS, session()->GetCurrentMessageArea());
          }
        }
      }
      break;
    }
  }
}


void ReadSelectedMessages(int chWhichMessages, int iWhere) {
  int iSaveSub = session()->GetCurrentMessageArea();

  int nextsub = 1;
  switch (iWhere) {
  case RM_ALL_SUBS:
  case RM_QSCAN_SUBS: {
    bout.cls();

    bout << "|#3-=< Q-Scan All >=-\r\n";
    for (int i = 0; (usub[i].subnum != -1) && (i < session()->num_subs) && nextsub && !hangup; i++) {
      session()->SetCurrentMessageArea(i);
      iscan(session()->GetCurrentMessageArea());
      if (iWhere == RM_QSCAN_SUBS) {
        if (qsc_q[usub[i].subnum / 32] & (1L << (usub[i].subnum % 32))) {
          if (chWhichMessages == RM_QSCAN_MSGS) {
            qscan(i, &nextsub);
          } else if (chWhichMessages == RM_ALL_MSGS) {
            scan(1, SCAN_OPTION_READ_MESSAGE, &nextsub, false);
          }
        }
      } else {
        if (chWhichMessages == RM_QSCAN_MSGS) {
          qscan(i, &nextsub);
        } else if (chWhichMessages == RM_ALL_MSGS) {
          scan(1, SCAN_OPTION_READ_MESSAGE, &nextsub, false);
        }
      }
      if (checka()) {
        nextsub = 0;
      }
    }
    bout.nl();
    bout << "|#3-=< Global Q-Scan Done >=-\r\n\n";
  }
  break;

  default:                                /* sub # */
    session()->SetCurrentMessageArea(iWhere);

    if (chWhichMessages == RM_QSCAN_MSGS) {
      qscan(iWhere, &nextsub);
    } else if (chWhichMessages == RM_ALL_MSGS) {
      scan(1, SCAN_OPTION_READ_MESSAGE, &nextsub, false);
    }
    break;
  }

  session()->SetCurrentMessageArea(iSaveSub);
  iscan(session()->GetCurrentMessageArea());
}
