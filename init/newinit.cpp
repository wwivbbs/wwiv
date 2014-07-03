/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#include "newinit.h"

#include <curses.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <string>
#include <sys/stat.h>

#include "archivers.h"
#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "wwivinit.h"
#include "platform/incl1.h"
#include "utility.h"

extern char bbsdir[];


static void create_text(const char *pszFileName) {
  char szFullFileName[MAX_PATH];
  char szMessage[ 255 ];

  sprintf(szFullFileName, "gfiles%c%s", WWIV_FILE_SEPERATOR_CHAR, pszFileName);
  int hFile = open(szFullFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  sprintf(szMessage, "This is %s.\nEdit to suit your needs.\r\n", pszFileName);
  write(hFile, szMessage, strlen(szMessage));
  close(hFile);
}

static char *date() {
  static char ds[9];
  time_t t;
  time(&t);
  struct tm * pTm = localtime(&t);

  sprintf(ds, "%02d/%02d/%02d", pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_year % 100);
  return ds;
}

#define OFFOF(x) (short) (((long)(&(user_record.x))) - ((long)&user_record))

static int qscn_file = -1;
static unsigned long *qsc;

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

static void write_qscn(unsigned int un, unsigned long *qscn, int stayopen) {
  if (open_qscn()) {
    long pos = ((long)syscfg.qscn_len) * ((long)un);
    lseek(qscn_file, pos, SEEK_SET);
    write(qscn_file, qscn, syscfg.qscn_len);
    if (!stayopen) {
      close_qscn();
    }
  }
}

static void init_files() {
  int i;
  valrec v;
  slrec sl;
  subboardrec s1;
  directoryrec d1;

  out->SetColor(Scheme::PROMPT);
  Puts("Creating Data Files.");
  out->SetColor(Scheme::NORMAL);

  memset(&syscfg, 0, sizeof(configrec));

  strcpy(syscfg.systempw, "SYSOP");
  Printf(".");
  sprintf(syscfg.msgsdir, "%smsgs%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR);
  Printf(".");
  sprintf(syscfg.gfilesdir, "%sgfiles%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR);
  Printf(".");
  sprintf(syscfg.datadir, "%sdata%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR);
  Printf(".");
  sprintf(syscfg.dloadsdir, "%sdloads%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR);
  Printf(".");
  sprintf(syscfg.tempdir, "%stemp1%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR);
  Printf(".");
  sprintf(syscfg.menudir, "%sgfiles%cmenus%c", bbsdir, WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
  Printf(".");
  strcpy(syscfg.batchdir, syscfg.tempdir);
  Printf(".");
  strcpy(syscfg.bbs_init_modem, "ATS0=0M0Q0V0E0S2=1S7=20H0{");
  Printf(".");
  strcpy(syscfg.answer, "ATA{");
  strcpy(syscfg.connect_300, "1");
  strcpy(syscfg.connect_1200, "5");
  Printf(".");
  strcpy(syscfg.connect_2400, "10");
  strcpy(syscfg.connect_9600, "13");
  strcpy(syscfg.connect_19200, "50");
  Printf(".");
  strcpy(syscfg.no_carrier, "3");
  strcpy(syscfg.ring, "2");
  strcpy(syscfg.hangupphone, "ATH0{");
  Printf(".");
  strcpy(syscfg.pickupphone, "ATH1{");
  strcpy(syscfg.terminal, "");
  strcpy(syscfg.systemname, "My WWIV BBS");
  Printf(".");
  strcpy(syscfg.systemphone, "   -   -    ");
  strcpy(syscfg.sysopname, "The New Sysop");
  Printf(".");

  syscfg.newusersl = 10;
  syscfg.newuserdsl = 0;
  syscfg.maxwaiting = 50;
  Printf(".");
  for (i = 0; i < 5; i++) {
    syscfg.baudrate[i] = 300;
  }
  Printf(".");
  syscfg.com_ISR[0] = 0;
  syscfg.com_ISR[1] = 4;
  syscfg.com_ISR[2] = 3;
  syscfg.com_ISR[3] = 4;
  syscfg.com_ISR[4] = 3;
  Printf(".");
  syscfg.com_base[0] = 0;
  syscfg.com_base[1] = 0x3f8;
  syscfg.com_base[2] = 0x2f8;
  syscfg.com_base[3] = 0x3e8;
  syscfg.com_base[4] = 0x2e8;
  Printf(".");
  syscfg.comport[1] = 0;
  syscfg.primaryport = 0;
  Printf(".");
  syscfg.newuploads = 0;
  syscfg.maxusers = 500;
  syscfg.newuser_restrict = restrict_validate;
  Printf(".");
  syscfg.req_ratio = 0.0;
  syscfg.newusergold = 100.0;
  v.ar = 0;
  v.dar = 0;
  v.restrict = 0;
  v.sl = 10;
  v.dsl = 0;
  Printf(".");
  for (i = 0; i < 10; i++) {
    syscfg.autoval[i] = v;
  }
  for (i = 0; i < 256; i++) {
    sl.time_per_logon = (i / 10) * 10;
    sl.time_per_day = static_cast<unsigned short>(((float)sl.time_per_logon) * 2.5);
    sl.messages_read = (i / 10) * 100;
    if (i < 10) {
      sl.emails = 0;
    } else if (i <= 19) {
      sl.emails = 5;
    } else {
      sl.emails = 20;
    }

    if (i <= 10) {
      sl.posts = 10;
    } else if (i <= 25) {
      sl.posts = 10;
    } else if (i <= 39) {
      sl.posts = 4;
    } else if (i <= 79) {
      sl.posts = 10;
    } else {
      sl.posts = 25;
    }

    sl.ability = 0;

    if (i >= 150) {
      sl.ability |= ability_cosysop;
    }
    if (i >= 100) {
      sl.ability |= ability_limited_cosysop;
    }
    if (i >= 90) {
      sl.ability |= ability_read_email_anony;
    }
    if (i >= 80) {
      sl.ability |= ability_read_post_anony;
    }
    if (i >= 70) {
      sl.ability |= ability_email_anony;
    }
    if (i >= 60) {
      sl.ability |= ability_post_anony;
    }
    if (i == 255) {
      sl.time_per_logon = 255;
      sl.time_per_day = 255;
      sl.posts = 255;
      sl.emails = 255;
    }
    syscfg.sl[i] = sl;
  }

  userrec user_record;
  syscfg.userreclen = static_cast<short>(sizeof(user_record));
  syscfg.waitingoffset = OFFOF(waiting);
  syscfg.inactoffset = OFFOF(inact);
  syscfg.sysstatusoffset = OFFOF(sysstatus);
  syscfg.fuoffset = OFFOF(forwardusr);
  syscfg.fsoffset = OFFOF(forwardsys);
  syscfg.fnoffset = OFFOF(net_num);

  syscfg.max_subs = 64;
  syscfg.max_dirs = 64;

  Printf(".");
  syscfg.qscn_len = 4 * (1 + syscfg.max_subs + ((syscfg.max_subs + 31) / 32) + ((syscfg.max_dirs + 31) / 32));

  strcpy(syscfg.dial_prefix, "ATDT");
  syscfg.post_call_ratio = 0.0;
  strcpy(syscfg.modem_type, "H2400");

  for (i = 0; i < 4; i++) {
    syscfgovr.com_ISR[i + 1] = syscfg.com_ISR[i + 1];
    syscfgovr.com_base[i + 1] = syscfg.com_base[i + 1];
    syscfgovr.com_ISR[i + 5] = syscfg.com_ISR[i + 1];
    syscfgovr.com_base[i + 5] = syscfg.com_base[i + 1];
  }
  syscfgovr.primaryport = syscfg.primaryport;
  strcpy(syscfgovr.modem_type, syscfg.modem_type);
  strcpy(syscfgovr.tempdir, syscfg.tempdir);
  strcpy(syscfgovr.batchdir, syscfg.batchdir);

  save_config();

  create_arcs();

  Printf(".");

  memset(&status, 0, sizeof(statusrec));

  strcpy(status.date1, date());
  strcpy(status.date2, "00/00/00");
  strcpy(status.date3, "00/00/00");
  strcpy(status.log1, "000000.LOG");
  strcpy(status.log2, "000000.LOG");
  strcpy(status.gfiledate, date());
  Printf(".");
  status.callernum = 65535;
  status.qscanptr = 2;
  status.net_bias = 0.001f;
  status.net_req_free = 3.0;

  qsc = (unsigned long *)malloc(syscfg.qscn_len);
  Printf(".");
  memset(qsc, 0, syscfg.qscn_len);

  save_status();
  userrec u;
  memset(&u, 0, sizeof(u));
  write_user(0, &u);
  write_qscn(0, qsc, 0);
  u.inact = inact_deleted;
  // Note: this is where init makes a user record #1 that is deleted for new installs.
  write_user(1, &u);
  write_qscn(1, qsc, 0);
  Printf(".");
  int hFile = open("data/names.lst", O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  close(hFile);

  Printf(".");
  memset(&s1, 0, sizeof(subboardrec));

  strcpy(s1.name, "General");
  strcpy(s1.filename, "GENERAL");
  s1.readsl = 10;
  s1.postsl = 20;
  s1.maxmsgs = 50;
  s1.storage_type = 2;
  hFile = open("data/subs.dat", O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  write(hFile, (void *)(&s1), sizeof(subboardrec));
  close(hFile);

  memset(&d1, 0, sizeof(directoryrec));

  Printf(".");
  strcpy(d1.name, "Sysop");
  strcpy(d1.filename, "SYSOP");
  sprintf(d1.path, "dloads%csysop%c", WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
  mkdir(d1.path);
  d1.dsl = 100;
  d1.maxfiles = 50;
  d1.type = 65535;
  Printf(".");
  hFile = open("data/dirs.dat", O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  write(hFile, &d1, sizeof(directoryrec));

  memset(&d1, 0, sizeof(directoryrec));

  strcpy(d1.name, "Miscellaneous");
  strcpy(d1.filename, "misc");
  sprintf(d1.path, "dloads%cmisc%c", WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
  mkdir(d1.path);
  d1.dsl = 10;
  d1.age = 0;
  d1.dar = 0;
  d1.maxfiles = 50;
  d1.mask = 0;
  d1.type = 0;
  write(hFile, (void *)(&d1), sizeof(directoryrec));
  close(hFile);
  Printf(".\n");
  ////////////////////////////////////////////////////////////////////////////
  out->SetColor(Scheme::PROMPT);
  Puts("Copying String and Miscellaneous files.");
  out->SetColor(Scheme::NORMAL);

  Printf(".");
  rename("wwivini.500", "wwiv.ini");
  Printf(".");
  char szDestination[MAX_PATH];
  sprintf(szDestination, "data%cmenucmds.dat", WWIV_FILE_SEPERATOR_CHAR);
  rename("menucmds.dat", szDestination);
  Printf(".");
  sprintf(szDestination, "data%cregions.dat", WWIV_FILE_SEPERATOR_CHAR);
  rename("regions.dat", szDestination);
  Printf(".");
  sprintf(szDestination, "data%cwfc.dat", WWIV_FILE_SEPERATOR_CHAR);
  rename("wfc.dat", szDestination);
  Printf(".");
  Printf(".");
  // Create the sample files.
  create_text("welcome.msg");
  create_text("newuser.msg");
  create_text("feedback.msg");
  create_text("system.msg");
  create_text("logon.msg");
  create_text("logoff.msg");
  create_text("logoff.mtr");
  create_text("comment.txt");
  Printf(".\n");

  ////////////////////////////////////////////////////////////////////////////
  out->SetColor(Scheme::PROMPT);
  Puts("Decompressing archives.  Please wait");
  out->SetColor(Scheme::NORMAL);
  if (exist("en-menus.zip")) {
    char szDestination[MAX_PATH];
    Printf(".");
    system("unzip -qq -o EN-menus.zip -dgfiles ");
    Printf(".");
    sprintf(szDestination, "dloads%csysop%cen-menus.zip",
            WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
    rename("en-menus.zip", szDestination);
    Printf(".");
  }
  if (exist("regions.zip")) {
    char szDestination[MAX_PATH];
    Printf(".");
    system("unzip -qq -o regions.zip -ddata");
    Printf(".");
    sprintf(szDestination, "dloads%csysop%cregions.zip",
            WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
    rename("regions.zip", szDestination);
    Printf(".");
  }
  if (exist("zip-city.zip")) {
    char szDestination[MAX_PATH];
    Printf(".");
    system("unzip -qq -o zip-city.zip -ddata");
    Printf(".");
    sprintf(szDestination, "dloads%csysop%czip-city.zip",
            WWIV_FILE_SEPERATOR_CHAR, WWIV_FILE_SEPERATOR_CHAR);
    rename("zip-city.zip", szDestination);
    Printf(".");
  }
  out->SetColor(Scheme::NORMAL);
  Printf(".\n");
}

void new_init() {
  const int ENTRIES = 12;
  const char *dirname[] = {
    "attach",
    "data",
    "data/regions",
    "data/zip-city",
    "gfiles",
    "gfiles/menus",
    "msgs",
    "dloads",
    "dloads/misc",
    "dloads/sysop",
    "temp1",
    "temp2",
    0L,
  };
  out->SetColor(Scheme::PROMPT);
  Puts("\n\nNow performing installation.  Please wait...\n\n");
  Puts("Creating Directories");
  out->SetColor(Scheme::NORMAL);
  for (int i = 0; i < ENTRIES; ++i) {
    out->SetColor(Scheme::NORMAL);
    Printf(".");
    int nRet = chdir(dirname[i]);
    if (nRet) {
      if (mkdir(dirname[i])) {
        out->SetColor(Scheme::ERROR_TEXT);
        Printf("\n\nERROR!!! Couldn't make '%s' Sub-Dir.\nExiting...", dirname[i]);
        exit_init(2);
      }
    } else {
      WWIV_ChangeDirTo(bbsdir);
    }
  }
  Printf(".\n");

  init_files();
}
