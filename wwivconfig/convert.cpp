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
#include "wwivconfig/convert.h"
#include "localui/wwiv_curses.h"

#include "core/datafile.h"
#include "core/file.h"
#include "core/jsonfile.h"
#include "core/strings.h"
#include "core/version.h"
#include "local_io/wconstants.h"
#include "localui/curses_io.h"
#include "localui/input.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/chains_cereal.h"
#include "sdk/filenames.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/subs_cereal.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/vardec.h"
#include "sdk/wwivcolors.h"
#include "sdk/acs/expr.h"
#include "sdk/files/dirs.h"
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/files/dirs_cereal.h"
#include "wwivconfig/archivers.h"
#include "wwivconfig/convert_jsonfile.h"
#include <cstring>
#include <filesystem>

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::files;
using namespace wwiv::strings;

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
  curses_out->window()->Bkgd(' ');
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
  auto userreclen = static_cast<int16_t>(sizeof(userrec));
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

config_upgrade_state_t convert_config_to_52(UIWindow* window, const std::string& config_filename) {
  File file(config_filename);
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return config_upgrade_state_t::already_latest;
  }

  ShowBanner(window, "Converting config.dat to 4.3/5.x format...");
  configrec syscfg53{};
  file.Read(&syscfg53, sizeof(configrec));

  configrec_header_t h = {};
  h.config_revision_number = 0;
  h.config_size = sizeof(configrec);
  h.written_by_wwiv_num_version = wwiv_config_version();
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
  return config_upgrade_state_t::upgraded;
}

static bool update_config_revision_number(const std::string& config_filename,
                                          const uint32_t config_revision_number) {
  File file(config_filename);
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return false;
  }
  configrec syscfg53{};
  if (file.Read(&syscfg53, sizeof(configrec)) < static_cast<int>(sizeof(configrec))) {
    return false;
  }
  // Good housekeeping clear out unused fields.
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

  // Update config_revision_number to the specified version
  syscfg53.header.header.config_revision_number = config_revision_number;

  file.Seek(0, File::Whence::begin);
  file.Write(&syscfg53, sizeof(configrec));
  file.Close();
  return true;
}

static bool convert_to_v1(UIWindow* window, const std::string& datadir, const std::string& config_filename) {
  ShowBanner(window, "Updating to 5.2.1+ format...");

  auto users_lst = FilePath(datadir, USER_LST);
  auto backup_file = users_lst;
  backup_file += ".backup.pre-wwivconfig-upgrade";

  // Make a backup file.
  std::error_code ec;
  // Note we ignore the ec since we fail open.
  copy_file(users_lst, backup_file, ec);

  DataFile<userrec> usersFile(FilePath(datadir, USER_LST),
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
  update_config_revision_number(config_filename, 1);

  const auto config_usr_filename = FilePath(datadir, "config.usr");
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
  for (auto i = 0; i < wwiv::stl::ssize(users); i++) {
    auto& u = users.at(i);
    if (i >= wwiv::stl::ssize(second_config)) {
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
  const auto user_dat_fn = FilePath(datadir, "user.dat");
  File::Remove(user_dat_fn);

  messagebox(window, "Converted to config version v1");
  return true;
}

template <>
subboard_t ConvertJsonFile<subboard_52_t, subboard_t>::ConvertType(const subboard_52_t& os) {
  subboard_t s{};
  s.nets = os.nets;
  {
    acs::AcsExpr ae;
    s.read_acs = ae.ar_int(os.ar).min_sl(os.readsl).min_age(os.age).get();
  }
  {
    acs::AcsExpr ae;
    s.post_acs = ae.min_sl(os.postsl).get();
  }
  s.anony = os.anony;
  s.desc = os.desc;
  s.filename = os.filename;
  s.key = os.key;
  s.maxmsgs = os.maxmsgs;
  s.name = os.name;
  s.storage_type = os.storage_type;
  return s;
}

template <>
directory_t ConvertJsonFile<directory_55_t, directory_t>::ConvertType(const directory_55_t& od) {
  directory_t d{};
  d.area_tags = od.area_tags;
  acs::AcsExpr ae;
  d.acs = ae.min_dsl(od.dsl).min_age(od.age).dar_int(od.dar).get();
  d.filename = od.filename;
  d.mask = od.mask;
  d.maxfiles = od.maxfiles;
  d.name = od.name;
  d.path = od.path;
  return d;
}

template <>
chain_t ConvertJsonFile<chain_55_t, chain_t>::ConvertType(const chain_55_t& oc) {
  chain_t c{};
  acs::AcsExpr ae;
  c.acs = ae.min_sl(oc.sl).ar_int(oc.ar).min_age(oc.minage).max_age(oc.maxage).get();
  c.ansi = oc.ansi;
  c.description = oc.description;
  c.dir = oc.dir;
  c.exec_mode = oc.exec_mode;
  c.filename = oc.filename;
  c.local_only = oc.local_only;
  c.multi_user = oc.multi_user;
  c.regby = oc.regby;
  c.usage = oc.usage;
  return c;
}

static bool convert_to_v2(UIWindow* window, const std::string& datadir,
                          const std::string& config_filename) {
  ShowBanner(window, "Updating to 5.2.2+ format...");

  ConvertJsonFile<subboard_52_t, subboard_t> cs(datadir, SUBS_JSON, "subs", 0, 1);
  cs.Convert();

  ConvertJsonFile<directory_55_t, directory_t> cd(datadir, DIRS_JSON, "dirs", 0, 1);
  cd.Convert();

  ConvertJsonFile<chain_55_t, chain_t> cc(datadir, CHAINS_JSON, "chains", 0, 1);
  cc.Convert();

  // Mark config.dat as upgraded.
  return update_config_revision_number(config_filename, 2);
}

config_upgrade_state_t ensure_latest_5x_config(UIWindow* window, const std::string& datadir,
                             const std::string& config_filename,
                             const uint32_t config_revision_number) {
  if (config_revision_number >= 2) {
    return config_upgrade_state_t::already_latest;
  }
  // add others
  if (config_revision_number < 1) {
    convert_to_v1(window, datadir, config_filename);
  }
  if (config_revision_number < 2) {
    convert_to_v2(window, datadir, config_filename);
  }
  return config_upgrade_state_t::upgraded;
}

void convert_config_424_to_430(UIWindow* window, const std::string& datadir, const std::string& config_filename) {
  File file(config_filename);
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return;
  }
  window->SetColor(SchemeId::INFO);
  window->Puts("Converting config.dat to 4.3/5.x format...\n");
  window->SetColor(SchemeId::NORMAL);
  configrec syscfg53{};
  file.Read(&syscfg53, sizeof(configrec));
  auto menus_dir = File::EnsureTrailingSlash("menus");
  to_char_array(syscfg53.menudir, FilePath(syscfg53.gfilesdir, menus_dir).string());

  arcrec arc[MAX_ARCS]{};
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

  File archiver(FilePath(datadir, ARCHIVER_DAT));
  if (!archiver.Open(File::modeBinary | File::modeWriteOnly | File::modeCreateFile)) {
    window->Puts("Couldn't open 'ARCHIVER_DAT' for writing.\n");
    window->Puts("Creating new file....\n");
    create_arcs(window, datadir);
    window->Puts("\n");
    if (!archiver.Open(File::modeBinary | File::modeWriteOnly | File::modeCreateFile)) {
      messagebox(window, "Still unable to open archiver.dat. Something is really wrong.");
      return;
    }
  }
  archiver.Write(arc, MAX_ARCS * sizeof(arcrec));
  archiver.Close();
}
