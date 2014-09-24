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
#include "core/wwivport.h"
#include "core/wfile.h"
#include "core/wfndfile.h"
#include "sdk/filenames.h"


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
  WFile file(syscfg.datadir, USER_LST);
  if (file.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
                WFile::shareDenyReadWrite)) {
    return static_cast<int>(file.GetLength() / sizeof(userrec)) - 1;
  }
  return -1;
}

void read_user(unsigned int un, userrec *u) {
  WFile file(syscfg.datadir, USER_LST);
  if (!file.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareDenyReadWrite)) {
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

  WFile file(syscfg.datadir, USER_LST);
  if (file.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile)) {
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
  if (userdat.Open(WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile, WFile::shareDenyNone)) {
    userdat.Seek(un * sizeof(user_config), WFile::seekBegin);
    userdat.Write(&SecondUserRec, sizeof(user_config));
    userdat.Close();
  }

}

void save_status() {
  WFile file(syscfg.datadir, STATUS_DAT);
  if (file.Open(WFile::modeBinary|WFile::modeReadWrite|WFile::modeCreateFile)) {
    file.Write(&status, sizeof(statusrec));
  }
}

/** returns true if status.dat is read correctly */
bool read_status() {
  WFile file(syscfg.datadir, STATUS_DAT);
  if (file.Open(WFile::modeBinary|WFile::modeReadWrite)) {
    file.Read(&status, sizeof(statusrec));
    return true;
  }
  return false;
}

void save_config() {
  WFile file(CONFIG_DAT);
  if (file.Open(WFile::modeBinary|WFile::modeReadWrite|WFile::modeCreateFile)) {
    file.Write(&syscfg, sizeof(configrec));
  }
}

void exit_init(int level) {
  out->window()->SetColor(SchemeId::NORMAL);
  // Don't leak the localIO (also fix the color when the app exits)
  delete out;
  out = nullptr;

  exit(level);
}

void trimstrpath(char *s) {
  StringTrimEnd(s);

  int i = strlen(s);
  if (i && (s[i - 1] != WFile::pathSeparatorChar)) {
    // We don't have pathSeparatorString.
    s[i] = WFile::pathSeparatorChar;
    s[i + 1] = 0;
  }
}