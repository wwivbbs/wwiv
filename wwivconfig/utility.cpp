/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#include "core/datafile.h"
#include "core/file.h"
#include "core/wwivport.h"
#include "localui/input.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

#include <string>

// Make sure it's after windows.h
#include "localui/wwiv_curses.h"

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

extern char bbsdir[];

static void fix_user_rec(userrec* u) {
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

int number_userrecs(const std::string& datadir) {
  if (const auto file =
          DataFile<userrec>(FilePath(datadir, USER_LST),
                            File::modeReadWrite | File::modeBinary | File::modeCreateFile,
                            File::shareDenyReadWrite)) {
    return static_cast<int>(file.number_of_records()) - 1;
  }
  return -1;
}

void read_user(const Config& config, int un, userrec* u) {
  if (auto file = DataFile<userrec>(FilePath(config.datadir(), USER_LST),
                                    File::modeReadWrite | File::modeBinary | File::modeCreateFile,
                                    File::shareDenyReadWrite)) {
    const auto nu = static_cast<int>(file.number_of_records()) - 1;
    if (un > nu) {
      u->inact = inact_deleted;
      fix_user_rec(u);
      return;
    }
    file.Seek(un);
    file.Read(u);
    fix_user_rec(u);
  } else {
    u->inact = inact_deleted;
    fix_user_rec(u);
  }
}

void write_user(const Config& config, int un, userrec* u) {
  if (un < 1 || un > config.max_users()) {
    return;
  }

  if (auto file =
          DataFile<userrec>(FilePath(config.datadir(), USER_LST),
                            File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    file.Seek(un);
    file.Write(u);
  }
}

void save_status(const std::string& datadir, const statusrec_t& statusrec) {
  if (auto file =
          DataFile<statusrec_t>(FilePath(datadir, STATUS_DAT),
                                File::modeBinary | File::modeReadWrite | File::modeCreateFile)) {
    file.Write(&statusrec);
  }
}

/** returns true if "status.dat" is read correctly */
bool read_status(const std::string& datadir, statusrec_t& statusrec) {
  if (auto file = DataFile<statusrec_t>(FilePath(datadir, STATUS_DAT),
                                        File::modeBinary | File::modeReadWrite)) {
    return file.Read(&statusrec);
  }
  return false;
}
