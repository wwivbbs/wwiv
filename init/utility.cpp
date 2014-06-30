/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#include <cstdlib>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <string>
#include <curses.h>
#include <sys/stat.h>

#include "archivers.h"
#include "bbs/common.h"
#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "bbs/wconstants.h"
#include "wwivinit.h"
#include "platform/incl1.h"
#include "platform/wfile.h"
#include "platform/wfndfile.h"

#ifdef __unix__
#include <termios.h>
#endif  // __unix__

extern char bbsdir[];

static void fix_user_rec(userrec *u) {
  u->name[sizeof(u->name) - 1] = 0;
  u->realname[sizeof(u->realname) - 1] = 0;
  u->callsign[sizeof(u->callsign) - 1] = 0;
  u->phone[sizeof(u->phone) - 1] = 0;
  u->pw[sizeof(u->pw) - 1] = 0;
  u->laston[sizeof(u->laston) - 1] = 0;
  u->note[sizeof(u->note) - 1] = 0;
  u->macros[0][sizeof(u->macros[0]) - 1] = 0;
  u->macros[1][sizeof(u->macros[1]) - 1] = 0;
  u->macros[2][sizeof(u->macros[2]) - 1] = 0;
}

int number_userrecs() {
  WFile file(syscfg.datadir, "user.lst");
  if (file.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
                WFile::shareDenyReadWrite, WFile::permRead)) {
    return static_cast<int>(file.GetLength() / sizeof(userrec)) - 1;
  }
  return -1;
}

void read_user(unsigned int un, userrec *u) {
  WFile file(syscfg.datadir, "user.lst");
  if (!file.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareDenyReadWrite,
                 WFile::permReadWrite)) {
    u->inact = inact_deleted;
    fix_user_rec(u);
    return;
  }

  int nu = static_cast<int>((file.GetLength() / syscfg.userreclen) - 1);

  if ((int) un > nu) {
    file.Close();
    u->inact = inact_deleted;
    fix_user_rec(u);
    return;
  }
  long pos = ((long) syscfg.userreclen) * ((long) un);
  file.Seek(pos, WFile::seekBegin);
  file.Read(u, syscfg.userreclen);
  file.Close();
  fix_user_rec(u);
}

void write_user(unsigned int un, userrec *u) {
  if ((un < 1) || (un > syscfg.maxusers)) {
    return;
  }

  WFile file(syscfg.datadir, "user.lst");
  if (file.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareUnknown,
                WFile::permReadWrite)) {
    long pos = un * syscfg.userreclen;
    file.Seek(pos, WFile::seekBegin);
    file.Write(u, syscfg.userreclen);
    file.Close();
  }

  user_config SecondUserRec = { 0 };
  strcpy(SecondUserRec.name, (char *) u->name);
  strcpy(SecondUserRec.szMenuSet, "WWIV");
  SecondUserRec.cHotKeys = 1;
  SecondUserRec.cMenuType = 0;

  WFile userdat(syscfg.datadir, "user.dat");
  if (userdat.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareDenyNone,
                   WFile::permReadWrite)) {
    userdat.Seek(un * sizeof(user_config), WFile::seekBegin);
    userdat.Write(&SecondUserRec, sizeof(user_config));
    userdat.Close();
  }

}

static int qscn_file = -1;

static int open_qscn() {
  char szFileName[MAX_PATH];

  if (qscn_file == -1) {
    sprintf(szFileName, "%suser.qsc", syscfg.datadir);
    qscn_file = open(szFileName, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
    if (qscn_file < 0) {
      qscn_file = -1;
      return 0;
    }
  }
  return 1;
}

static void close_qscn() {
  if (qscn_file != -1) {
    close(qscn_file);
    qscn_file = -1;
  }
}

void read_qscn(unsigned int un, unsigned long *qscn, int stayopen) {
  if (open_qscn()) {
    long pos = ((long)syscfg.qscn_len) * ((long)un);
    if (pos + (long)syscfg.qscn_len <= filelength(qscn_file)) {
      lseek(qscn_file, pos, SEEK_SET);
      read(qscn_file, qscn, syscfg.qscn_len);
      if (!stayopen) {
        close_qscn();
      }
      return;
    }
  }

  if (!stayopen) {
    close_qscn();
  }

  memset(qsc, 0, syscfg.qscn_len);
  *qsc = 999;
  memset(qsc + 1, 0xff, ((syscfg.max_dirs + 31) / 32) * 4);
  memset(qsc + 1 + (syscfg.max_dirs + 31) / 32, 0xff, ((syscfg.max_subs + 31) / 32) * 4);
}

void write_qscn(unsigned int un, unsigned long *qscn, int stayopen) {
  if (open_qscn()) {
    long pos = ((long)syscfg.qscn_len) * ((long)un);
    lseek(qscn_file, pos, SEEK_SET);
    write(qscn_file, qscn, syscfg.qscn_len);
    if (!stayopen) {
      close_qscn();
    }
  }
}

int exist(const char *pszFileName) {
  WFindFile fnd;
  return ((fnd.open(pszFileName, 0) == false) ? 0 : 1);
}

void save_status() {
  char szFileName[MAX_PATH];

  sprintf(szFileName, "%sstatus.dat", syscfg.datadir);
  int statusfile = open(szFileName, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  write(statusfile, (void *)(&status), sizeof(statusrec));
  close(statusfile);
}

/** returns true if status.dat is read correctly */
bool read_status() {
  char szFileName[81];

  sprintf(szFileName, "%sstatus.dat", syscfg.datadir);
  int statusfile = open(szFileName, O_RDWR | O_BINARY);
  if (statusfile >= 0) {
    read(statusfile, (void *)(&status), sizeof(statusrec));
    close(statusfile);
    return true;
  }
  return false;
}

int save_config() {
  int configfile;

  if (syscfgovr.primaryport < 5) {
    syscfg.primaryport = syscfgovr.primaryport;
    syscfg.com_ISR[syscfg.primaryport] = syscfgovr.com_ISR[syscfgovr.primaryport];
    syscfg.com_base[syscfg.primaryport] = syscfgovr.com_base[syscfgovr.primaryport];
    strcpy(syscfg.modem_type, syscfgovr.modem_type);
    strcpy(syscfg.tempdir, syscfgovr.tempdir);
    strcpy(syscfg.batchdir, syscfgovr.batchdir);
    if (syscfgovr.comflags & comflags_buffered_uart) {
      syscfg.sysconfig |= sysconfig_high_speed;
    } else {
      syscfg.sysconfig &= ~sysconfig_high_speed;
    }
  }

  configfile = open("config.dat", O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  write(configfile, (void *)(&syscfg), sizeof(configrec));
  close(configfile);

  configfile = open("config.ovr", O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  if (configfile > 0) {
    lseek(configfile, 0, SEEK_SET);
    write(configfile, &syscfgovr, sizeof(configoverrec));
    close(configfile);
  }

  return (0);
}

void exit_init(int level) {
  out->SetColor(Scheme::NORMAL);
  // Don't leak the localIO (also fix the color when the app exits)
  delete out;
  out = nullptr;

  exit(level);
}
