/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2017, WWIV Software Services             */
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
#include "wwivconfig/convert.h"

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include "localui/wwiv_curses.h"
#include <fcntl.h>
#include <memory>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <sys/stat.h>

#include "local_io/wconstants.h"
#include "core/strings.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/version.h"
#include "core/wwivport.h"
#include "wwivconfig/archivers.h"
#include "wwivconfig/wwivconfig.h"
#include "wwivconfig/wwivinit.h"
#include "localui/input.h"
#include "localui/curses_io.h"
#include "sdk/filenames.h"
#include "sdk/user.h"
#include "sdk/vardec.h"
#include "sdk/wwivcolors.h"

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

#define CONFIG_USR "config.usr"

struct user_config {
  char name[31];          // verify against a user

  unsigned long unused_status;

  unsigned long lp_options;
  unsigned char lp_colors[32];

  char menu_set[9];   // Selected AMENU set to use
  char hot_keys;       // Use hot keys in AMENU

  char junk[119];   // AMENU took 11 bytes from here
};

static void ShowBanner(UIWindow* window, const std::string& m) {
  // TODO(rushfan): make a subwindow here but until this clear the altcharset background.
  out->window()->Bkgd(' ');
  window->SetColor(SchemeId::INFO);
  window->Puts(StrCat(m, "\n"));
  window->SetColor(SchemeId::NORMAL);
}

bool ensure_offsets_are_updated(UIWindow* window, const wwiv::sdk::Config& config) {
  File file(config.config_filename());
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return false;
  }

  // update user info data
  int16_t userreclen = static_cast<int16_t>(sizeof(userrec));
  int16_t waitingoffset = offsetof(userrec, waiting);
  int16_t inactoffset = offsetof(userrec, inact);
  int16_t sysstatusoffset = offsetof(userrec, sysstatus);
  int16_t fuoffset = offsetof(userrec, forwardusr);
  int16_t fsoffset = offsetof(userrec, forwardsys);
  int16_t fnoffset = offsetof(userrec, net_num);

  if (userreclen != syscfg.userreclen ||
      waitingoffset != syscfg.waitingoffset ||
      inactoffset != syscfg.inactoffset ||
      sysstatusoffset != syscfg.sysstatusoffset ||
      fuoffset != syscfg.fuoffset ||
      fsoffset != syscfg.fsoffset ||
      fnoffset != syscfg.fnoffset) {

    ShowBanner(window, "Updating OFfsets...");
    syscfg.userreclen = userreclen;
    syscfg.waitingoffset = waitingoffset;
    syscfg.inactoffset = inactoffset;
    syscfg.sysstatusoffset = sysstatusoffset;
    syscfg.fuoffset = fuoffset;
    syscfg.fsoffset = fsoffset;
    syscfg.fnoffset = fnoffset;

    // Write it all back.
    file.Seek(0, File::Whence::begin);
    file.Write(&syscfg, sizeof(configrec));
    file.Close();
  }
  return true;
}

bool convert_config_to_52(UIWindow* window, const wwiv::sdk::Config& config) {
  File file(config.config_filename());
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return false;
  }

  ShowBanner(window, "Converting config.dat to 4.3/5.x format...");
  file.Read(&syscfg, sizeof(configrec));

  configrec_header_t h = {};
  h.config_revision_number = 0;
  h.config_size = sizeof(configrec);
  h.written_by_wwiv_num_version = wwiv_num_version;
  to_char_array(h.signature, "WWIV");

  // Save old newuser password.
  string newuserpw = syscfg.header.newuserpw;
  // Update newuser password to new location.
  to_char_array(syscfg.newuserpw, newuserpw);
  // Set new header on config.dat.
  syscfg.header.header = h;

  // Write it all back.
  file.Seek(0, File::Whence::begin);
  file.Write(&syscfg, sizeof(configrec));
  file.Close();
  return true;
}

static bool convert_to_52_1(UIWindow* window, const wwiv::sdk::Config& config) {
  ShowBanner(window, "Updating to latest 5.2 format...");

  string users_lst = StrCat(config.datadir(), USER_LST);
  string backup_file = StrCat(users_lst, ".backup.pre-wwivconfig-upgrade");

  // Make a backup file.
  File::Copy(users_lst, backup_file);

  DataFile<userrec> usersFile(FilePath(config.datadir(), USER_LST),
    File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyReadWrite);
  if (!usersFile) {
    messagebox(window, "Unable to open user.lst.");
    return false;
  }

  vector<userrec> users;
  if (!usersFile.ReadVector(users)) {
    messagebox(window, "Unable to read user.lst.");
    return false;
  }

  // zero out the res_xxx records for good housekeeping.
  for (auto& u : users) {
    memset(u.res_byte, 0, sizeof(u.res_byte));
    memset(u.res_char, 0, sizeof(u.res_char));
    memset(u.res_float, 0, sizeof(u.res_float));
    memset(u.res_gp, 0, sizeof(u.res_gp));
    memset(u.res_long, 0, sizeof(u.res_long));
    memset(u.res_short, 0, sizeof(u.res_short));
    memset(u.menu_set, 0, sizeof(u.menu_set));

    // Set new defaults.
    memset(u.lp_colors, static_cast<uint8_t>(Color::CYAN), sizeof(u.lp_colors));
    u.lp_colors[0] = static_cast<uint8_t>(Color::LIGHTGREEN);
    u.lp_colors[1] = static_cast<uint8_t>(Color::LIGHTGREEN);
    u.lp_colors[2] = static_cast<uint8_t>(Color::CYAN);
    u.lp_colors[3] = static_cast<uint8_t>(Color::CYAN);
    u.lp_colors[4] = static_cast<uint8_t>(Color::LIGHTCYAN);
    u.lp_colors[5] = static_cast<uint8_t>(Color::LIGHTCYAN);
    u.lp_colors[6] = static_cast<uint8_t>(Color::CYAN);
    u.lp_colors[7] = static_cast<uint8_t>(Color::CYAN);
    u.lp_colors[8] = static_cast<uint8_t>(Color::CYAN);
    u.lp_colors[9] = static_cast<uint8_t>(Color::CYAN);
    u.lp_colors[10] = static_cast<uint8_t>(Color::LIGHTCYAN);
    u.lp_options = cfl_fname | cfl_extension | cfl_dloads | cfl_kbytes | cfl_description;
    u.hot_keys = 0;
    to_char_array(u.menu_set, "wwiv");
  }

  // Save where we are.
  usersFile.Seek(0);
  usersFile.WriteVector(users);

  // Update config.dat with new version to consider this "successful"
  // enough of an upgrade at this point.
  {
    File file(config.config_filename());
    if (!file.Open(File::modeBinary | File::modeReadWrite)) {
      return false;
    }
    if (file.Read(&syscfg, sizeof(configrec)) < sizeof(configrec)) {
      return false;
    }
    memset(syscfg.res, 0, sizeof(syscfg.res));
    memset(syscfg.unused1, 0, sizeof(syscfg.unused1));
    memset(syscfg.unused2, 0, sizeof(syscfg.unused2));
    memset(syscfg.unused3, 0, sizeof(syscfg.unused3));
    memset(syscfg.unused4, 0, sizeof(syscfg.unused4));
    memset(syscfg.unused5, 0, sizeof(syscfg.unused5));
    memset(syscfg.unused6, 0, sizeof(syscfg.unused6));
    memset(syscfg.unused7, 0, sizeof(syscfg.unused7));
    memset(syscfg.unused8, 0, sizeof(syscfg.unused8));
    memset(syscfg.unused9, 0, sizeof(syscfg.unused9));
    syscfg.header.header.config_revision_number = 1;

    file.Seek(0, File::Whence::begin);
    file.Write(&syscfg, sizeof(configrec));
    file.Close();
  }

  DataFile<user_config> configUsrFile(FilePath(config.datadir(), "config.usr"),
    File::modeReadOnly | File::modeBinary, File::shareDenyWrite);
  if (!configUsrFile) {
    return false;
  }

  vector<user_config> second_config;
  if (!configUsrFile.ReadVector(second_config)) {
    return false;
  }

  // merge in data from user_config
  for (size_t i = 0; i < users.size(); i++) {
    auto& u = users.at(i);
    if (i >= second_config.size()) {
      continue;
    }
    const auto& c = second_config.at(i);
    u.hot_keys = c.hot_keys;
    u.lp_options = c.lp_options;
    memcpy(u.lp_colors, c.lp_colors, sizeof(u.lp_colors));
  }

  // Save where we are.
  usersFile.Seek(0);
  if (!usersFile.WriteVector(users)) {
    messagebox(window, "Unable to write user.lst.");
    return false;
  }

  // Close the user_config file (config.usr) and delete it.
  configUsrFile.Close();
  configUsrFile.file().Delete();

  // 2nd version of config.usr that wwivconfig was mistakenly creating.
  File userDatFile(FilePath(config.datadir(), "user.dat"));
  userDatFile.Delete();

  messagebox(window, "Converted to config version #1");
  return true;
}

bool ensure_latest_5x_config(UIWindow* window, const wwiv::sdk::Config& config) {
  const auto v = syscfg.header.header.config_revision_number;
  if (v < 1) {
    if (!convert_to_52_1(window, config)) {
      return false;
    }
  }

  // add others

  return true;
}

void convert_config_424_to_430(UIWindow* window, const wwiv::sdk::Config& config) {
  File file(config.config_filename());
  if (!file.Open(File::modeBinary|File::modeReadWrite)) {
    return;
  }
  window->SetColor(SchemeId::INFO);
  window->Puts("Converting config.dat to 4.3/5.x format...\n");
  window->SetColor(SchemeId::NORMAL);
  file.Read(&syscfg, sizeof(configrec));
  auto menus_dir = StrCat("menus", File::pathSeparatorString);
  to_char_array(syscfg.menudir, FilePath(syscfg.gfilesdir, menus_dir));

  arcrec arc[MAX_ARCS];
  for (int i = 0; i < MAX_ARCS; i++) {
    if (syscfg.arcs[i].extension[0] && i < 4) {
      to_char_array(arc[i].name, syscfg.arcs[i].extension);
      to_char_array(arc[i].extension, syscfg.arcs[i].extension);
      to_char_array(arc[i].arca, syscfg.arcs[i].arca);
      to_char_array(arc[i].arce, syscfg.arcs[i].arce);
      to_char_array(arc[i].arcl, syscfg.arcs[i].arcl);
    } else {
      to_char_array(arc[i].name, "New Archiver Name");
      to_char_array(arc[i].extension, "EXT");
      to_char_array(arc[i].arca, "archive add command");
      to_char_array(arc[i].arce, "archive extract command");
      to_char_array(arc[i].arcl, "archive list command");
    }
  }
  file.Seek(0, File::Whence::begin);
  file.Write(&syscfg, sizeof(configrec));
  file.Close();

  File archiver(FilePath(config.datadir(), ARCHIVER_DAT));
  if (!archiver.Open(File::modeBinary|File::modeWriteOnly|File::modeCreateFile)) {
    window->Puts("Couldn't open 'ARCHIVER_DAT' for writing.\n");
    window->Puts("Creating new file....\n");
    create_arcs(window, config.datadir());
    window->Puts("\n");
    if (!archiver.Open(File::modeBinary|File::modeWriteOnly|File::modeCreateFile)) {
      messagebox(window, "Still unable to open archiver.dat. Something is really wrong.");
      return;
    }
  }
  archiver.Write(arc, MAX_ARCS * sizeof(arcrec));
  archiver.Close();
}
