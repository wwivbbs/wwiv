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

#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <string>
#include <curses.h>
#include <sys/stat.h>

#include "common.h"
#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "w5assert.h"
#include "wconstants.h"
#include "wwivinit.h"
#include "platform/incl1.h"
#include "platform/wfndfile.h"

#ifdef __unix__
#include <termios.h>
#endif  // __unix__

extern char configdat[];
extern char bbsdir[];

extern char **mdm_desc;
extern int mdm_count, mdm_cur;

extern autosel_data *autosel_info;
extern int num_autosel;

extern resultrec *result_codes;
extern int num_result_codes;

extern net_networks_rec *net_networks;

extern int inst;

static int wfc = 0;
static int useron = 0;

void init() {
  curatr = 0x03;
  hangup = false;  // TODO(rushfan): Remove this from init
  thisuser.sysstatus = 0;
  daylight = 0; // C Runtime Variable -- WHY?
}

/* This converts a character to uppercase */
int upcase(int ch) {
  if ((ch > '`') && (ch < '{')) {
    ch = ch - 32;
  }
  return ch;
}

void fix_user_rec(userrec *u) {
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
  WWIV_ASSERT(false);
  return -1;
}


void read_user(unsigned int un, userrec *u) {
  if (((useron) && ((long) un == initinfo.usernum)) || ((wfc) && (un == 1))) {
    *u = thisuser;
    fix_user_rec(u);
    return;
  }

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

  if (((useron) && ((long) un == initinfo.usernum)) || ((wfc) && (un == 1))) {
    thisuser = *u;
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
  strcpy(SecondUserRec.name, (char *) thisuser.name);
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

int open_qscn() {
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


void close_qscn() {
  if (qscn_file != -1) {
    close(qscn_file);
    qscn_file = -1;
  }
}


void read_qscn(unsigned int un, unsigned long *qscn, int stayopen) {
  if (((useron) && ((long) un == initinfo.usernum)) || ((wfc) && (un == 1))) {
    for (int i = (syscfg.qscn_len / 4) - 1; i >= 0; i--) {
      qscn[i] = qsc[i];
    }
    return;
  }

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

  if (((useron) && ((long) un == initinfo.usernum)) || ((wfc) && (un == 1))) {
    for (int i = (syscfg.qscn_len / 4) - 1; i >= 0; i--) {
      qsc[i] = qscn[i];
    }
  }

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


char *date() {
  static char ds[9];
  time_t t;
  time(&t);
  struct tm * pTm = localtime(&t);

  sprintf(ds, "%02d/%02d/%02d", pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_year % 100);
  return ds;
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
  int configfile, n;

  if (inst == 1) {
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
  }

  configfile = open(configdat, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  write(configfile, (void *)(&syscfg), sizeof(configrec));
  close(configfile);

  configfile = open("config.ovr", O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  if (configfile > 0) {
    n = filelength(configfile) / sizeof(configoverrec);
    while (n < (inst - 1)) {
      lseek(configfile, 0L, SEEK_END);
      write(configfile, &syscfgovr, sizeof(configoverrec));
      n++;
    }
    lseek(configfile, sizeof(configoverrec) * (inst - 1), SEEK_SET);
    write(configfile, &syscfgovr, sizeof(configoverrec));
    close(configfile);
  }

  return (0);
}

void create_text(const char *pszFileName) {
  char szFullFileName[MAX_PATH];
  char szMessage[ 255 ];

  sprintf(szFullFileName, "gfiles%c%s", WWIV_FILE_SEPERATOR_CHAR, pszFileName);
  int hFile = open(szFullFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  sprintf(szMessage, "This is %s.\nEdit to suit your needs.\n\x1a", pszFileName);
  write(hFile, szMessage, strlen(szMessage));
  close(hFile);
}

void cvtx(unsigned short sp, char *rc) {
  if (*rc) {
    resultrec *rr = &(result_codes[num_result_codes++]);
    std::string s = std::to_string(sp);
    strcpy(rr->curspeed, s.c_str());
    strcpy(rr->return_code, rc);
    rr->modem_speed = sp;
    if ((syscfg.sysconfig & sysconfig_high_speed) &&
        (syscfgovr.primaryport < MAX_ALLOWED_PORT)) {
      rr->com_speed = syscfg.baudrate[syscfgovr.primaryport];
    } else {
      rr->com_speed = sp;
    }
  }
}

void convert_result_codes() {
  num_result_codes = 1;
  strcpy(result_codes[0].curspeed, "No Carrier");
  strcpy(result_codes[0].return_code, syscfg.no_carrier);
  result_codes[0].modem_speed = 0;
  result_codes[0].com_speed = 0;
  cvtx(300, syscfg.connect_300);
  cvtx(300, syscfg.connect_300_a);
  cvtx(1200, syscfg.connect_1200);
  cvtx(1200, syscfg.connect_1200_a);
  cvtx(2400, syscfg.connect_2400);
  cvtx(2400, syscfg.connect_2400_a);
  cvtx(9600, syscfg.connect_9600);
  cvtx(9600, syscfg.connect_9600_a);
  cvtx(19200, syscfg.connect_19200);
  cvtx(19200, syscfg.connect_19200_a);

  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%sresults.dat", syscfg.datadir);
  int hFile = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(hFile, result_codes, num_result_codes * sizeof(resultrec));
  close(hFile);
}

/****************************************************************************/

#define OFFOF(x) (short) (((long)(&(thisuser.x))) - ((long)&thisuser))

void init_files() {
  int i;
  char s2[81], *env;
  valrec v;
  slrec sl;
  subboardrec s1;
  directoryrec d1;

  textattr(COLOR_YELLOW);
  Puts("Creating Data Files.");
  textattr(COLOR_CYAN);

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
  syscfg.comport[1] = 1;
  syscfg.primaryport = 1;
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

  syscfg.userreclen = static_cast<short>(sizeof(thisuser));
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

  qsc = (unsigned long *)bbsmalloc(syscfg.qscn_len);
  Printf(".");
  memset(qsc, 0, syscfg.qscn_len);

  save_status();
  memset(&thisuser, 0, sizeof(thisuser));
  write_user(0, &thisuser);
  write_qscn(0, qsc, 0);
  thisuser.inact = inact_deleted;
  write_user(1, &thisuser);
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
  textattr(COLOR_YELLOW);
  Puts("Copying String and Miscellaneous files.");
  textattr(COLOR_CYAN);

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
  sprintf(szDestination, "data%cmodems.mdm", WWIV_FILE_SEPERATOR_CHAR);
  rename("modems.500", szDestination);
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

  WFindFile fnd;
  fnd.open("*.mdm", 0);
  while (fnd.next()) {
    sprintf(s2, "data%c%s", WWIV_FILE_SEPERATOR_CHAR, fnd.GetFileName());
    rename(fnd.GetFileName(), s2);
  }

  if ((env = getenv("TZ")) == NULL) {
    putenv("TZ=EST5EDT");
  }

  Printf(".\n");

  ////////////////////////////////////////////////////////////////////////////
  textattr(COLOR_YELLOW);
  Puts("Decompressing archives.  Please wait");
  textattr(COLOR_CYAN);
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
  // we changed the environment, clear the change
  if (env == NULL) {
    putenv("TZ=");
  }

  textattr(COLOR_CYAN);
  Printf(".\n");
}


void convert_modem_info(const char *fn) {
  char szFileName[MAX_PATH];
  FILE *pFile;
  int i;

  sprintf(szFileName, "%s%s.mdm", syscfg.datadir, fn);
  pFile = fopen(szFileName, "w");
  if (!pFile) {
    Printf("Couldn't open '%s' for writing.\n", szFileName);
    textattr(COLOR_WHITE);
    exit_init(2);
  }

  fprintf(pFile, "NAME: \"Local defaults\"\n");
  fprintf(pFile, "INIT: \"%s\"\n", syscfg.bbs_init_modem);
  fprintf(pFile, "SETU: \"\"\n");
  fprintf(pFile, "ANSR: \"%s\"\n", syscfg.answer);
  fprintf(pFile, "PICK: \"%s\"\n", syscfg.pickupphone);
  fprintf(pFile, "HANG: \"%s\"\n", syscfg.hangupphone);
  fprintf(pFile, "DIAL: \"%s\"\n", syscfg.dial_prefix);
  fprintf(pFile, "SEPR: \"\"\n");
  fprintf(pFile, "DEFL: MS=%u CS=%u EC=N DC=N AS=N FC=%c\n",
          syscfg.baudrate[syscfgovr.primaryport],
          syscfg.baudrate[syscfgovr.primaryport],
          /* syscfg.sysconfig & sysconfig_flow_control?'Y':'N' */ 'N');
  fprintf(pFile, "RESL: \"0\"     \"Normal\"       NORM\n");
  fprintf(pFile, "RESL: \"OK\"    \"Normal\"       NORM\n");
  fprintf(pFile, "RESL: \"%s\"    \"Ring\"         RING\n", syscfg.ring);

  for (i = 0; i < num_result_codes; i++) {
    if (result_codes[i].modem_speed) {
      fprintf(pFile, "RESL: \"%s\"    \"%s\"         CON MS=%u CS=%u\n",
              result_codes[i].return_code, result_codes[i].curspeed,
              result_codes[i].modem_speed, result_codes[i].com_speed);
    } else {
      fprintf(pFile, "RESL: \"%s\"    \"%s\"         DIS\n",
              result_codes[i].return_code, result_codes[i].curspeed);
    }
  }

  fclose(pFile);
}


void init_modem_info() {
  get_descriptions(syscfg.datadir, &mdm_desc, &mdm_count, &autosel_info, &num_autosel);

  // The modem type isn't used much, and should be used less under Win32
  syscfgovr.primaryport = 1;
  if (!set_modem_info("LOCAL", true)) {
    result_codes = (resultrec *) bbsmalloc(40 * sizeof(resultrec));
    convert_result_codes();
    convert_modem_info("LOCAL");
    BbsFreeMemory(result_codes);
    set_modem_info("LOCAL", true);
  }
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
  textattr(COLOR_YELLOW);
  Puts("\n\nNow performing installation.  Please wait...\n\n");
  Puts("Creating Directories");
  textattr(COLOR_CYAN);
  for (int i = 0; i < ENTRIES; ++i) {
    textattr(11);
    Printf(".");
    int nRet = chdir(dirname[i]);
    if (nRet) {
      if (mkdir(dirname[i])) {
        textattr(COLOR_RED);
        Printf("\n\nERROR!!! Couldn't make '%s' Sub-Dir.\nExiting...", dirname[i]);
        textattr(COLOR_WHITE);
        exit_init(2);
      }
    } else {
      WWIV_ChangeDirTo(bbsdir);
    }
  }
  Printf(".\n");

  init_files();

  init_modem_info();
}

int verify_inst_dirs(configoverrec *co, int inst) {
  int i, rc = 0;
  configoverrec tco;
  char szMessage[255];

  int hFile = open("config.ovr", O_RDONLY | O_BINARY);
  if (hFile > 0) {
    int n = filelength(hFile) / sizeof(configoverrec);
    for (i = 0; i < n; i++) {
      if ((i + 1) != inst) {
        lseek(hFile, sizeof(configoverrec) * i, SEEK_SET);
        read(hFile, &tco, sizeof(configoverrec));

        if ((strcmp(co->tempdir, tco.tempdir) == 0) ||
            (strcmp(co->tempdir, tco.batchdir) == 0)) {
          rc++;
          sprintf(szMessage, "The Temporary directory: %s is in use on Instance #%d.",
                  co->tempdir, i + 1);
        }
        if ((strcmp(co->batchdir, tco.batchdir) == 0) ||
            (strcmp(co->batchdir, tco.tempdir) == 0)) {
          rc++;
          sprintf(szMessage, "The Batch directory: %s is in use on Instance #%d.",
                  co->batchdir, i + 1);
        }
      }
    }
    close(hFile);
  }
  if (rc) {
    app->localIO->LocalGotoXY(0, 8);
    textattr(COLOR_RED);
    app->localIO->LocalPuts(szMessage);
    for (i = 0; i < (int) strlen(szMessage); i++) {
      Printf("\b \b");
    }

    textattr(COLOR_YELLOW);
    app->localIO->LocalPuts("<ESC> when done.");
    textattr(COLOR_CYAN);
  }
  return 0;
}

void exit_init(int level) {
  // Don't leak the localIO (also fix the color when the app exits)
  delete app->localIO;

  exit(level);
}
