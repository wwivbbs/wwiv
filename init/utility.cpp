/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
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
#include <cstdlib>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <string>
#include <curses.h>
#include <sys/stat.h>

#include "init/archivers.h"
#include "bbs/common.h"
#include "init/ifcns.h"
#include "init/init.h"
#include "initlib/input.h"
#include "bbs/wconstants.h"
#include "wwivinit.h"
#include "core/wwivport.h"
#include "core/file.h"
#include "core/wfndfile.h"
#include "sdk/filenames.h"

using namespace wwiv::strings;

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
  File file(syscfg.datadir, USER_LST);
  if (file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile,
                File::shareDenyReadWrite)) {
    return static_cast<int>(file.GetLength() / sizeof(userrec)) - 1;
  }
  return -1;
}

void read_user(unsigned int un, userrec *u) {
  File file(syscfg.datadir, USER_LST);
  if (!file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyReadWrite)) {
    u->inact = inact_deleted;
    fix_user_rec(u);
    return;
  }

  std::size_t nu = (file.GetLength() / syscfg.userreclen) - 1;
  if (un > nu) {
    file.Close();
    u->inact = inact_deleted;
    fix_user_rec(u);
    return;
  }
  long pos = un * syscfg.userreclen;
  file.Seek(pos, File::seekBegin);
  file.Read(u, syscfg.userreclen);
  file.Close();
  fix_user_rec(u);
}

void write_user(unsigned int un, userrec *u) {
  if (un < 1 || un > syscfg.maxusers) {
    return;
  }

  File file(syscfg.datadir, USER_LST);
  if (file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    long pos = un * syscfg.userreclen;
    file.Seek(pos, File::seekBegin);
    file.Write(u, syscfg.userreclen);
    file.Close();
  }

  user_config SecondUserRec = { 0 };
  strcpy(SecondUserRec.name, (char *) u->name);
  strcpy(SecondUserRec.szMenuSet, "WWIV");
  SecondUserRec.cHotKeys = 1;
  SecondUserRec.cMenuType = 0;

  File userdat(syscfg.datadir, "user.dat");
  if (userdat.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyNone)) {
    userdat.Seek(un * sizeof(user_config), File::seekBegin);
    userdat.Write(&SecondUserRec, sizeof(user_config));
    userdat.Close();
  }

}

void save_status() {
  File file(syscfg.datadir, STATUS_DAT);
  if (file.Open(File::modeBinary|File::modeReadWrite|File::modeCreateFile)) {
    file.Write(&status, sizeof(statusrec));
  }
}

/** returns true if status.dat is read correctly */
bool read_status() {
  File file(syscfg.datadir, STATUS_DAT);
  if (file.Open(File::modeBinary|File::modeReadWrite)) {
    file.Read(&status, sizeof(statusrec));
    return true;
  }
  return false;
}

void save_config() {
  File file(CONFIG_DAT);
  if (file.Open(File::modeBinary|File::modeReadWrite|File::modeCreateFile)) {
    file.Write(&syscfg, sizeof(configrec));
  }
}
