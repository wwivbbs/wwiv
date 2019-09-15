/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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

#include "localui/wwiv_curses.h"
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <sys/stat.h>

#include "core/datafile.h"
#include "core/file.h"
#include "core/filesystem.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/wwivport.h"
#include "local_io/wconstants.h"
#include "localui/curses_io.h"
#include "localui/input.h"
#include "sdk/filenames.h"
#include "sdk/user.h"
#include "sdk/vardec.h"
#include "sdk/wwivcolors.h"
#include "wwivconfig/archivers.h"
#include "wwivconfig/wwivconfig.h"

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

#define CONFIG_USR "config.usr"

struct user_config {
  char name[31]; // verify against a user

  unsigned long unused_status;

  unsigned long lp_options;
  unsigned char lp_colors[32];

  char menu_set[9]; // Selected AMENU set to use
  char hot_keys;    // Use hot keys in AMENU

  char junk[119]; // AMENU took 11 bytes from here
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
  configrec syscfg53{};
  file.Seek(0, File::Whence::begin);
  file.Read(&syscfg53, sizeof(configrec));

  if (userreclen != config.userrec_length() || waitingoffset != config.waitingoffset() ||
      inactoffset != config.inactoffset() || sysstatusoffset != config.sysstatusoffset() ||
      fuoffset != config.fuoffset() || fsoffset != config.fsoffset() ||
      fnoffset != config.fnoffset()) {

    ShowBanner(window, "Updating Offsets...");
    syscfg53.userreclen = userreclen;
    syscfg53.waitingoffset = waitingoffset;
    syscfg53.inactoffset = inactoffset;
    syscfg53.sysstatusoffset = sysstatusoffset;
    syscfg53.fuoffset = fuoffset;
    syscfg53.fsoffset = fsoffset;
    syscfg53.fnoffset = fnoffset;

    // Write it all back.
    file.Seek(0, File::Whence::begin);
    file.Write(&syscfg53, sizeof(configrec));
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
  configrec syscfg53{};
  file.Read(&syscfg53, sizeof(configrec));

  configrec_header_t h = {};
  h.config_revision_number = 0;
  h.config_size = sizeof(configrec);
  h.written_by_wwiv_num_version = wwiv_num_version;
  to_char_array(h.signature, "WWIV");

  // Save old newuser password.
  string newuserpw = syscfg53.header.newuserpw;
  // Update newuser password to new location.
  to_char_array(syscfg53.newuserpw, newuserpw);
  // Set new header on config.dat.
  syscfg53.header.header = h;

  // Write it all back.
  file.Seek(0, File::Whence::begin);
  file.Write(&syscfg53, sizeof(configrec));
  file.Close();
  return true;
}

static bool convert_to_52_1(UIWindow* window, const wwiv::sdk::Config& config) {
  ShowBanner(window, "Updating to latest 5.2 format...");

  wwiv::fs::path users_lst = PathFilePath(config.datadir(), USER_LST);
  auto backup_file = users_lst;
  backup_file += ".backup.pre-wwivconfig-upgrade";

  // Make a backup file.
  std::error_code ec;
  // Note we ignore the ec since we fail open.
  wwiv::fs::copy_file(users_lst, backup_file, ec);

  DataFile<userrec> usersFile(PathFilePath(config.datadir(), USER_LST),
                              File::modeReadWrite | File::modeBinary | File::modeCreateFile,
                              File::shareDenyReadWrite);
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
    configrec syscfg53{};
    if (file.Read(&syscfg53, sizeof(configrec)) < sizeof(configrec)) {
      return false;
    }
    memset(syscfg53.res, 0, sizeof(syscfg53.res));
    memset(syscfg53.unused1, 0, sizeof(syscfg53.unused1));
    memset(syscfg53.unused2, 0, sizeof(syscfg53.unused2));
    memset(syscfg53.unused3, 0, sizeof(syscfg53.unused3));
    memset(syscfg53.unused4, 0, sizeof(syscfg53.unused4));
    memset(syscfg53.unused5, 0, sizeof(syscfg53.unused5));
    memset(syscfg53.unused6, 0, sizeof(syscfg53.unused6));
    memset(syscfg53.unused7, 0, sizeof(syscfg53.unused7));
    memset(syscfg53.unused8, 0, sizeof(syscfg53.unused8));
    memset(syscfg53.unused9, 0, sizeof(syscfg53.unused9));
    syscfg53.header.header.config_revision_number = 1;

    file.Seek(0, File::Whence::begin);
    file.Write(&syscfg53, sizeof(configrec));
    file.Close();
  }

  const auto config_usr_filename = PathFilePath(config.datadir(), "config.usr");
  DataFile<user_config> configUsrFile(config_usr_filename, File::modeReadOnly | File::modeBinary,
                                      File::shareDenyWrite);
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
  File::Remove(config_usr_filename);

  // 2nd version of config.usr that wwivconfig was mistakenly creating.
  const auto user_dat_fn = PathFilePath(config.datadir(), "user.dat");
  File::Remove(user_dat_fn);

  messagebox(window, "Converted to config version #1");
  return true;
}

bool ensure_latest_5x_config(UIWindow* window, const wwiv::sdk::Config& config,
                             const configrec& cr) {
  const auto v = cr.header.header.config_revision_number;
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
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return;
  }
  window->SetColor(SchemeId::INFO);
  window->Puts("Converting config.dat to 4.3/5.x format...\n");
  window->SetColor(SchemeId::NORMAL);
  configrec syscfg53{};
  file.Read(&syscfg53, sizeof(configrec));
  auto menus_dir = File::EnsureTrailingSlash("menus");
  to_char_array(syscfg53.menudir, FilePath(syscfg53.gfilesdir, menus_dir));

  arcrec arc[MAX_ARCS];
  for (int i = 0; i < MAX_ARCS; i++) {
    if (syscfg53.arcs[i].extension[0] && i < 4) {
      to_char_array(arc[i].name, syscfg53.arcs[i].extension);
      to_char_array(arc[i].extension, syscfg53.arcs[i].extension);
      to_char_array(arc[i].arca, syscfg53.arcs[i].arca);
      to_char_array(arc[i].arce, syscfg53.arcs[i].arce);
      to_char_array(arc[i].arcl, syscfg53.arcs[i].arcl);
    } else {
      to_char_array(arc[i].name, "New Archiver Name");
      to_char_array(arc[i].extension, "EXT");
      to_char_array(arc[i].arca, "archive add command");
      to_char_array(arc[i].arce, "archive extract command");
      to_char_array(arc[i].arcl, "archive list command");
    }
  }
  file.Seek(0, File::Whence::begin);
  file.Write(&syscfg53, sizeof(configrec));
  file.Close();

  File archiver(PathFilePath(config.datadir(), ARCHIVER_DAT));
  if (!archiver.Open(File::modeBinary | File::modeWriteOnly | File::modeCreateFile)) {
    window->Puts("Couldn't open 'ARCHIVER_DAT' for writing.\n");
    window->Puts("Creating new file....\n");
    create_arcs(window, config.datadir());
    window->Puts("\n");
    if (!archiver.Open(File::modeBinary | File::modeWriteOnly | File::modeCreateFile)) {
      messagebox(window, "Still unable to open archiver.dat. Something is really wrong.");
      return;
    }
  }
  archiver.Write(arc, MAX_ARCS * sizeof(arcrec));
  archiver.Close();
}
