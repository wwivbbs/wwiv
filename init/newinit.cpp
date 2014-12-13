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
#include "init/newinit.h"

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
#include <vector>

#include <curses.h>

#include "core/strings.h"
#include "core/file.h"
#include "core/textfile.h"
#include "core/wwivport.h"
#include "init/archivers.h"
#include "init/ifcns.h"
#include "init/init.h"
#include "initlib/input.h"
#include "init/wwivinit.h"
#include "init/utility.h"

#include "sdk/filenames.h"

using std::string;
using std::vector;
using namespace wwiv::strings;

static void create_text(const char *pszFileName) {
  TextFile file("gfiles", pszFileName, "wt");
  file.WriteLine(StringPrintf("This is %s.", pszFileName));
  file.WriteLine("Edit to suit your needs.");
  file.Close();
}

static string date() {
  time_t t = time(nullptr);
  struct tm* pTm = localtime(&t);
  return StringPrintf("%02d/%02d/%02d", pTm->tm_mon + 1, pTm->tm_mday, pTm->tm_year % 100);
}

#define OFFOF(x) (short) (((long)(&(user_record.x))) - ((long)&user_record))

static uint32_t *qsc;

static void write_qscn(unsigned int un, uint32_t *qscn) {
  File file(syscfg.datadir, USER_QSC);
  if (file.Open(File::modeReadWrite|File::modeBinary|File::modeCreateFile)) {
    file.Seek(syscfg.qscn_len * un, File::seekBegin);
    file.Write(qscn, syscfg.qscn_len);
    file.Close();
  }
}

static void init_files(CursesWindow* window, const string& bbsdir) {
  window->SetColor(SchemeId::PROMPT);
  window->Puts("Creating Data Files.\n");
  window->SetColor(SchemeId::NORMAL);

  memset(&syscfg, 0, sizeof(configrec));

  strcpy(syscfg.systempw, "SYSOP");
  sprintf(syscfg.msgsdir, "%smsgs%c", bbsdir.c_str(), File::pathSeparatorChar);
  sprintf(syscfg.gfilesdir, "%sgfiles%c", bbsdir.c_str(), File::pathSeparatorChar);
  sprintf(syscfg.datadir, "%sdata%c", bbsdir.c_str(), File::pathSeparatorChar);
  sprintf(syscfg.dloadsdir, "%sdloads%c", bbsdir.c_str(), File::pathSeparatorChar);
  sprintf(syscfg.tempdir, "%stemp1%c", bbsdir.c_str(), File::pathSeparatorChar);
  sprintf(syscfg.menudir, "%sgfiles%cmenus%c", bbsdir.c_str(), File::pathSeparatorChar, File::pathSeparatorChar);
  strcpy(syscfg.batchdir, syscfg.tempdir);
  strcpy(syscfg.unused_bbs_init_modem, "ATS0=0M0Q0V0E0S2=1S7=20H0{");
  strcpy(syscfg.unused_answer, "ATA{");
  strcpy(syscfg.unused_connect_300, "1");
  strcpy(syscfg.unused_connect_1200, "5");
  strcpy(syscfg.unused_connect_2400, "10");
  strcpy(syscfg.unused_connect_9600, "13");
  strcpy(syscfg.unused_connect_19200, "50");
  strcpy(syscfg.unused_no_carrier, "3");
  strcpy(syscfg.unused_ring, "2");
  strcpy(syscfg.hangupphone, "ATH0{");
  strcpy(syscfg.pickupphone, "ATH1{");
  strcpy(syscfg.unused_terminal, "");
  strcpy(syscfg.systemname, "My WWIV BBS");
  strcpy(syscfg.systemphone, "   -   -    ");
  strcpy(syscfg.sysopname, "The New Sysop");

  syscfg.newusersl = 10;
  syscfg.newuserdsl = 0;
  syscfg.maxwaiting = 50;
  for (int i = 0; i < 5; i++) {
    syscfg.baudrate[i] = 300;
  }
  syscfg.com_ISR[0] = 0;
  syscfg.com_ISR[1] = 4;
  syscfg.com_ISR[2] = 3;
  syscfg.com_ISR[3] = 4;
  syscfg.com_ISR[4] = 3;
  syscfg.com_base[0] = 0;
  syscfg.com_base[1] = 0x3f8;
  syscfg.com_base[2] = 0x2f8;
  syscfg.com_base[3] = 0x3e8;
  syscfg.com_base[4] = 0x2e8;
  syscfg.comport[1] = 0;
  // Always use 1 for the primary port.
  syscfg.primaryport = 1;
  syscfg.newuploads = 0;
  syscfg.maxusers = 500;
  syscfg.newuser_restrict = restrict_validate;
  syscfg.req_ratio = 0.0;
  syscfg.newusergold = 100.0;

  valrec v;
  v.ar = 0;
  v.dar = 0;
  v.restrict = 0;
  v.sl = 10;
  v.dsl = 0;
  for (int i = 0; i < 10; i++) {
    syscfg.autoval[i] = v;
  }
  for (int i = 0; i < 256; i++) {
    slrec sl;
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
  syscfg.qscn_len = 4 * (1 + syscfg.max_subs + ((syscfg.max_subs + 31) / 32) + ((syscfg.max_dirs + 31) / 32));

  strcpy(syscfg.unused_dial_prefix, "ATDT");
  syscfg.post_call_ratio = 0.0;
  strcpy(syscfg.modem_type, "H2400");
  save_config();

  create_arcs(out->window());
  memset(&status, 0, sizeof(statusrec));
  string now(date());
  strcpy(status.date1, now.c_str());
  strcpy(status.date2, "00/00/00");
  strcpy(status.date3, "00/00/00");
  strcpy(status.log1, "000000.LOG");
  strcpy(status.log2, "000000.LOG");
  strcpy(status.gfiledate, now.c_str());
  status.callernum = 65535;
  status.qscanptr = 2;
  status.net_bias = 0.001f;
  status.net_req_free = 3.0;

  qsc = (uint32_t *)malloc(syscfg.qscn_len);
  memset(qsc, 0, syscfg.qscn_len);

  save_status();
  userrec u;
  memset(&u, 0, sizeof(u));
  write_user(0, &u);
  write_qscn(0, qsc);
  u.inact = inact_deleted;
  // Note: this is where init makes a user record #1 that is deleted for new installs.
  write_user(1, &u);
  write_qscn(1, qsc);
  {
    File namesfile(StrCat("data/", NAMES_LST));
    namesfile.Open(File::modeBinary|File::modeReadWrite|File::modeCreateFile);
  }
  {
    subboardrec s1;
    memset(&s1, 0, sizeof(subboardrec));

    strcpy(s1.name, "General");
    strcpy(s1.filename, "GENERAL");
    s1.readsl = 10;
    s1.postsl = 20;
    s1.maxmsgs = 50;
    s1.storage_type = 2;
    File subsfile(StrCat("data/", SUBS_DAT));
    subsfile.Open(File::modeBinary|File::modeCreateFile|File::modeReadWrite);
    subsfile.Write(&s1, sizeof(subboardrec));
    subsfile.Close();
  }

  {
    directoryrec d1;
    memset(&d1, 0, sizeof(directoryrec));
    strcpy(d1.name, "Sysop");
    strcpy(d1.filename, "SYSOP");
    sprintf(d1.path, "dloads%csysop%c", File::pathSeparatorChar, File::pathSeparatorChar);
    File::mkdir(d1.path);
    d1.dsl = 100;
    d1.maxfiles = 50;
    d1.type = 65535;
    File dirsfile(StrCat("data/", SUBS_DAT));
    dirsfile.Open(File::modeBinary|File::modeCreateFile|File::modeReadWrite);
    dirsfile.Write(&d1, sizeof(directoryrec));

    memset(&d1, 0, sizeof(directoryrec));
    strcpy(d1.name, "Miscellaneous");
    strcpy(d1.filename, "misc");
    sprintf(d1.path, "dloads%cmisc%c", File::pathSeparatorChar, File::pathSeparatorChar);
    File::mkdir(d1.path);
    d1.dsl = 10;
    d1.age = 0;
    d1.dar = 0;
    d1.maxfiles = 50;
    d1.mask = 0;
    d1.type = 0;
    dirsfile.Write(&d1, sizeof(directoryrec));
    dirsfile.Close();
  }

  window->Printf(".\n");
  ////////////////////////////////////////////////////////////////////////////
  window->SetColor(SchemeId::PROMPT);
  window->Puts("Copying String and Miscellaneous files.\n");
  window->SetColor(SchemeId::NORMAL);

  File::Rename("wwivini.500", WWIV_INI);
  File::Rename("menucmds.dat", StringPrintf("data%cmenucmds.dat", File::pathSeparatorChar));
  File::Rename("regions.dat", StringPrintf("data%cregions.dat", File::pathSeparatorChar));
  File::Rename("wfc.dat", StringPrintf("data%cwfc.dat", File::pathSeparatorChar));
  // Create the sample files.
  create_text("welcome.msg");
  create_text("newuser.msg");
  create_text("feedback.msg");
  create_text("system.msg");
  create_text("logon.msg");
  create_text("logoff.msg");
  create_text("logoff.mtr");
  create_text("comment.txt");
  window->Printf(".\n");

  ////////////////////////////////////////////////////////////////////////////
  window->SetColor(SchemeId::PROMPT);
  window->Puts("Decompressing archives.  Please wait");
  window->SetColor(SchemeId::NORMAL);
  if (File::Exists("en-menus.zip")) {
    system("unzip -qq -o EN-menus.zip -dgfiles ");
    File::Rename("en-menus.zip", 
                 StringPrintf("dloads%csysop%cen-menus.zip",
                              File::pathSeparatorChar,
                              File::pathSeparatorChar));
  }
  if (File::Exists("regions.zip")) {
    system("unzip -qq -o regions.zip -ddata");
    const string destination = StringPrintf("dloads%csysop%cregions.zip",
        File::pathSeparatorChar, File::pathSeparatorChar);
    File::Rename("regions.zip", destination);
  }
  if (File::Exists("zip-city.zip")) {
    system("unzip -qq -o zip-city.zip -ddata");
    const string destination = StringPrintf("dloads%csysop%czip-city.zip",
        File::pathSeparatorChar, File::pathSeparatorChar);
    File::Rename("zip-city.zip", destination);
  }
  window->SetColor(SchemeId::NORMAL);
}

bool new_init(CursesWindow* window, const string& bbsdir) {
  static const vector<string> dirnames = {
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
  };
  window->SetColor(SchemeId::PROMPT);
  window->Puts("\n\nNow performing installation.  Please wait...\n\n");
  window->Puts("Creating Directories\n");
  window->SetColor(SchemeId::NORMAL);
  for (const auto& dirname : dirnames) {
    window->SetColor(SchemeId::NORMAL);
    int nRet = chdir(dirname.c_str());
    if (nRet) {
      if (!File::mkdir(dirname)) {
        window->SetColor(SchemeId::ERROR_TEXT);
        window->Printf("\n\nERROR!!! Couldn't make '%s' Sub-Dir.\nExiting...", dirname.c_str());
        return false;
      }
    } else {
      chdir(bbsdir.c_str());
    }
  }

  init_files(window, bbsdir);
  return true;
}
