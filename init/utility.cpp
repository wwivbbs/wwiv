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

#include "bbs/common.h"
#include "bbs/wconstants.h"
#include "core/wwivport.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/wfndfile.h"
#include "init/archivers.h"
#include "initlib/input.h"
#include "init/init.h"
#include "init/wwivinit.h"
#include "sdk/filenames.h"

using namespace wwiv::core;
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
  DataFile<userrec> file(syscfg.datadir, USER_LST,
      File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyReadWrite);
  if (file) {
    return file.number_of_records() - 1;
  }
  return -1;
}

void read_user(unsigned int un, userrec *u) {
  DataFile<userrec> file(syscfg.datadir, USER_LST,
      File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyReadWrite);
  if (!file) {
    u->inact = inact_deleted;
    fix_user_rec(u);
    return;
  }

  std::size_t nu = file.number_of_records() - 1;
  if (un > nu) {
    u->inact = inact_deleted;
    fix_user_rec(u);
    return;
  }
  file.Seek(un);
  file.Read(u);
  fix_user_rec(u);
}

void write_user(unsigned int un, userrec *u) {
  if (un < 1 || un > syscfg.maxusers) {
    return;
  }

  DataFile<userrec> file(syscfg.datadir, USER_LST,
      File::modeReadWrite | File::modeBinary | File::modeCreateFile);
  if (file) {
    file.Seek(un);
    file.Write(u);
  }

  user_config SecondUserRec = { 0 };
  strcpy(SecondUserRec.name, (char *) u->name);
  strcpy(SecondUserRec.szMenuSet, "WWIV");
  SecondUserRec.cHotKeys = 1;
  SecondUserRec.cMenuType = 0;

  DataFile<user_config> userdat(syscfg.datadir, "user.dat",
      File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyNone);
  if (file) {
    userdat.Seek(un);
    userdat.Write(&SecondUserRec);
  }
}

void save_status() {
  DataFile<statusrec> file(syscfg.datadir, STATUS_DAT, File::modeBinary|File::modeReadWrite|File::modeCreateFile);
  if (file) {
    file.Write(&status);
  }
}

/** returns true if status.dat is read correctly */
bool read_status() {
  DataFile<statusrec> file(syscfg.datadir, STATUS_DAT, File::modeBinary|File::modeReadWrite);
  if (file) {
    return file.Read(&status);
  }
  return false;
}

void save_config() {
  DataFile<configrec> file(CONFIG_DAT, File::modeBinary|File::modeReadWrite|File::modeCreateFile);
  if (file) {
    file.Write(&syscfg);
  }
}
