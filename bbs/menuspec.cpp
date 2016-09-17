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

#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsovl3.h"
#include "bbs/chains.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/datetime.h"
#include "bbs/defaults.h"
#include "bbs/input.h"
#include "bbs/msgbase1.h"
#include "bbs/fcns.h"
#include "bbs/instmsg.h"
#include "bbs/menuspec.h"
#include "bbs/menusupp.h"
#include "bbs/mmkey.h"
#include "bbs/multinst.h"
#include "bbs/vars.h"
#include "core/stl.h"
#include "core/strings.h"

using std::string;
using namespace wwiv::menus;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

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
  User ur;
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
    session()->tleft(true);
    File fileDownload(g_szDownloadFileName);
    fileDownload.Open(File::modeBinary | File::modeReadOnly);
    FileAreaSetRecord(fileDownload, nRecordNumber);
    fileDownload.Read(&u, sizeof(uploadsrec));
    fileDownload.Close();
    bout.nl();

    if (bTitle) {
      bout << "Directory  : " << session()->directories[dn].name << wwiv::endl;
    }
    bOkToDL = printfileinfo(&u, dn);


    if (strncmp(u.filename, "WWIV4", 5) == 0 && !session()->HasConfigFlag(OP_FLAGS_NO_EASY_DL)) {
      bOkToDL = 1;
    } else {
      if (!ratio_ok()) {
        return -1;
      }
    }
    if (bOkToDL || bFreeDL) {
      write_inst(INST_LOC_DOWNLOAD, session()->current_user_dir().subnum, INST_FLAGS_NONE);
      sprintf(s1, "%s%s", session()->directories[dn].path, u.filename);
      if (session()->directories[dn].mask & mask_cdrom) {
        sprintf(s2, "%s%s", session()->directories[dn].path, u.filename);
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
          session()->users()->ReadUser(&ur, u.ownerusr);
          if (!ur.IsUserDeleted()) {
            if (date_to_daten(ur.GetFirstOn()) < static_cast<time_t>(u.daten)) {
              const string username_num = session()->names()->UserName(session()->usernum);
              ssm(u.ownerusr, 0) << username_num << " downloaded '" << u.filename << "' on " << date();
            }
          }
        }
      }

      bout.nl(2);
      bout.bprintf("Your ratio is now: %-6.3f\r\n", ratio());

      if (session()->IsUserOnline()) {
        session()->UpdateTopScreen();
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
  return abort ? -1 : 1;
}


int FindDN(const char *pszDownloadFileName) {
  for (size_t i = 0; (i < session()->directories.size()); i++) {
    if (IsEqualsIgnoreCase(session()->directories[i].filename, pszDownloadFileName)) {
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
  for (size_t i = 0; i < session()->chains.size(); i++) {
    if (IsEqualsIgnoreCase(session()->chains[i].description, pszDoor)) {
      return i;
    }
  }

  return -1;
}

bool ValidateDoorAccess(int nDoorNumber) {
  int inst = inst_ok(INST_LOC_CHAINS, nDoorNumber + 1);
  if (inst != 0) {
    char szChainInUse[255];
    sprintf(szChainInUse,  "|#2Chain %s is in use on instance %d.  ", session()->chains[nDoorNumber].description, inst);
    if (!(session()->chains[nDoorNumber].ansir & ansir_multi_user)) {
      bout << szChainInUse << " Try again later.\r\n";
      return false;
    } else {
      bout << szChainInUse << " Care to join in? ";
      if (!(yesno())) {
        return false;
      }
    }
  }
  chainfilerec& c = session()->chains[nDoorNumber];
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
  if (session()->HasConfigFlag(OP_FLAGS_CHAIN_REG) 
      && session()->chains_reg.size() > 0
      && session()->GetEffectiveSl() < 255) {
    chainregrec& r = session()->chains_reg[ nDoorNumber ];
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
  for (size_t i = 0; (i < session()->subs().subs().size())
       && (session()->usub[i].subnum != -1); i++) {
    if (wwiv::strings::IsEquals(session()->usub[i].keys, s)) {
      session()->set_current_user_sub_num(i);
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
    for (size_t i = 0; i < session()->directories.size(); i++) {
      if (wwiv::strings::IsEquals(session()->udir[i].keys, s)) {
        session()->set_current_user_dir_num(i);
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

  get_conf_info(ConferenceType::CONF_SUBS, &nc, &cp, nullptr, nullptr, &uc);

  for (int i = 0; (i < MAX_CONFERENCES) && (uc[i].confnum != -1); i++) {
    if (iConf == cp[uc[i].confnum].designator) {
      setuconf(ConferenceType::CONF_SUBS, i, -1);
      break;
    }
  }
}


void SetDirConf(int iConf) {
  int nc;
  confrec *cp = nullptr;
  userconfrec *uc = nullptr;

  get_conf_info(ConferenceType::CONF_DIRS, &nc, &cp, nullptr, nullptr, &uc);

  for (int i = 0; (i < MAX_CONFERENCES) && (uc[i].confnum != -1); i++) {
    if (iConf == cp[uc[i].confnum].designator) {
      setuconf(ConferenceType::CONF_DIRS, i, -1);
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
