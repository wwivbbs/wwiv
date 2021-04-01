/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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

#include "bbs/conf.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/inifile.h"
#include "core/jsonfile.h"
#include "core/strings.h"
#include "core/version.h"
#include "localui/curses_io.h"
#include "localui/input.h"
#include "localui/wwiv_curses.h"
#include "local_io/wconstants.h"
#include "sdk/chains.h"
#include "sdk/config430.h"
#include "sdk/filenames.h"
#include "sdk/gfiles.h"
#include "sdk/qwk_config.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/vardec.h"
#include "sdk/wwivcolors.h"
#include "sdk/acs/expr.h"
#include "sdk/fido/fido_callout.h"
#include "sdk/files/dirs.h"
#include "sdk/menus/menu.h"
#include "sdk/net/networks.h"
#include "wwivconfig/archivers.h"
#include "wwivconfig/convert_jsonfile.h"

#include <cstring>
#include <filesystem>

using namespace wwiv::core;
using namespace wwiv::local::ui;
using namespace wwiv::sdk::files;
using namespace wwiv::sdk::menus;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::strings;

namespace wwiv::wwivconfig::convert {

struct user_config {
  char name[31]; // verify against a user

  unsigned long unused_status;

  unsigned long lp_options;
  unsigned char lp_colors[32];

  char menu_set[9]; // Selected AMENU set to use
  char hot_keys;    // Use hot keys in AMENU

  char junk[119]; // AMENU took 11 bytes from here
};

void write_and_log(UIWindow* window, const std::string& m) {
  LOG(INFO) << m;
  window->Puts(StrCat(m, "\n"));
}

int final_wwiv_config_dat_version() {
  // TODO(rushfan): Update when we add a new version.
  return 6;
}


static void ShowBanner(UIWindow* window, const std::string& m) {
  // TODO(rushfan): make a sub-window here but until this clear the altcharset background.
  curses_out->window()->Bkgd(' ');
  window->SetColor(SchemeId::INFO);
  write_and_log(window, m);
  window->SetColor(SchemeId::NORMAL);
}

bool ensure_offsets_are_updated(UIWindow* window, const Config430& config) {
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

  if (userreclen != config.config()->userreclen || waitingoffset != config.config()->waitingoffset ||
      inactoffset != config.config()->inactoffset || sysstatusoffset != config.config()->sysstatusoffset ||
      fuoffset != config.config()->fuoffset || fsoffset != config.config()->fsoffset ||
      fnoffset != config.config()->fnoffset) {

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

config_upgrade_state_t convert_config_to_52(UIWindow* window, const std::filesystem::path& config_filename) {
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

  // Save old new  user password.
  std::string newuserpw = syscfg53.header.newuserpw;
  // Update new user password to new location.
  to_char_array(syscfg53.newuserpw, newuserpw);
  // Set new header on config.dat.
  syscfg53.header.header = h;

  // Write it all back.
  file.Seek(0, File::Whence::begin);
  file.Write(&syscfg53, sizeof(configrec));
  file.Close();
  return config_upgrade_state_t::upgraded;
}


static bool convert_to_v1(UIWindow* window, Config& config) {
  ShowBanner(window, "Updating to 5.2.1+ format...");

  const auto users_lst = FilePath(config.datadir(), USER_LST);
  auto backup_file = users_lst;
  backup_file += ".backup.pre-wwivconfig-upgrade";

  // Make a backup file.
  std::error_code ec;
  // Note we ignore the ec since we fail open.
  copy_file(users_lst, backup_file, ec);

  DataFile<userrec> usersFile(FilePath(config.datadir(), USER_LST),
                              File::modeReadWrite | File::modeBinary | File::modeCreateFile,
                              File::shareDenyReadWrite);
  if (!usersFile) {
    LOG(ERROR) << "Unable to open user.lst.";
    messagebox(window, "Unable to open user.lst.");
    return false;
  }

  std::vector<userrec> users;
  if (!usersFile.ReadVector(users)) {
    LOG(ERROR) << "Unable to read user.lst.";
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
  config.config_revision_number(1);

  const auto config_usr_filename = FilePath(config.datadir(), "config.usr");
  DataFile<user_config> configUsrFile(config_usr_filename, File::modeReadOnly | File::modeBinary,
                                      File::shareDenyWrite);
  if (!configUsrFile) {
    LOG(ERROR) << "No config.usr file to upgrade.";
    return false;
  }

  std::vector<user_config> second_config;
  if (!configUsrFile.ReadVector(second_config)) {
    LOG(ERROR) << "Unable to read config.usr file to upgrade.";
    return false;
  }

  // merge in data from user_config
  for (auto i = 0; i < stl::ssize(users); i++) {
    auto& u = stl::at(users, i);
    if (i >= stl::ssize(second_config)) {
      continue;
    }
    const auto& c = stl::at(second_config, i);
    u.hot_keys = c.hot_keys;
    u.lp_options = c.lp_options;
    memcpy(u.lp_colors, c.lp_colors, sizeof(u.lp_colors));
  }

  // Save where we are.
  usersFile.Seek(0);
  if (!usersFile.WriteVector(users)) {
    LOG(ERROR) << "Unable to write user.lst.";
    messagebox(window, "Unable to write user.lst.");
    return false;
  }

  // Close the user_config file (config.usr) and delete it.
  configUsrFile.Close();
  File::Remove(config_usr_filename);

  // 2nd version of config.usr that wwivconfig was mistakenly creating.
  File::Remove(FilePath(config.datadir(), "user.dat"));

  LOG(INFO) << "Converted to config version v1";
  messagebox(window, "Converted to config version v1");
  return true;
}

template <>
// ReSharper disable once CppMemberFunctionMayBeStatic
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
// ReSharper disable once CppMemberFunctionMayBeStatic
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
// ReSharper disable once CppMemberFunctionMayBeStatic
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

static bool convert_to_v2(UIWindow* window, Config& config) {
  LOG(INFO) << "Updating to 5.2+ v2 format...";
  ShowBanner(window, "Updating to 5.2+ v2 format...");

  const auto datadir = config.datadir();
  LOG(INFO) << "Upgrading subs.json";
  ConvertJsonFile<subboard_52_t, subboard_t> cs(datadir, SUBS_JSON, "subs", 0, 1);
  cs.Convert();

  LOG(INFO) << "Upgrading dirs.json";
  ConvertJsonFile<directory_55_t, directory_t> cd(datadir, DIRS_JSON, "dirs", 0, 1);
  cd.Convert();

  LOG(INFO) << "Upgrading chains.json";
  ConvertJsonFile<chain_55_t, chain_t> cc(datadir, CHAINS_JSON, "chains", 0, 1);
  cc.Convert();

  // Mark config.dat as upgraded.
  config.config_revision_number(2);
  return true;
}

static bool convert_menu(const std::string& menu_dir, const std::string& menu_set,
                         const std::string& menu_name, int max_backups) {

  const auto dir = FilePath(menu_dir, menu_set);
  const auto fname = StrCat(menu_name, ".mnu.json");
  const auto path = FilePath(dir, fname);
  if (File::Exists(path)) {
    LOG(INFO) << "Menu already exists in 5.6+ format: " << path.string();
    return true;
  }

  const Menu430 m4(menu_dir, menu_set, menu_name);
  if (m4.initialized()) {
    if (auto om5 = Create56MenuFrom43(m4, max_backups)) {
      auto& m5 = om5.value();
      if (m5.Save()) {
        LOG(INFO) << "Converted menu: " << menu_set << File::pathSeparatorChar << menu_name
                  << std::endl;
        return true;
      }
    }
  }
  return false;
}

static bool convert_to_v3(UIWindow* window, Config& config) {
  ShowBanner(window, "Updating to 5.2+ v3 format...");

  auto dirs = FindFiles(FilePath(config.menudir(), "*"), FindFiles::FindFilesType::directories);

  for (const auto& d : dirs) {
    const auto& menu_set = d.name;
    const auto path = FilePath(config.menudir(), menu_set);
    auto menus = FindFiles(FilePath(path, "*"), FindFiles::FindFilesType::files, FindFiles::WinNameType::long_name);
    for (const auto& m : menus) {
      auto lm = ToStringLowerCase(m.name);
      if (ends_with(lm, ".mnu")) {
        const auto name = m.name.substr(0, m.name.size() - 4 ); 
        if (!convert_menu(config.menudir(), menu_set, name, config.max_backups())) {
          LOG(ERROR) << "Unable to convert 4.3 menu:" << menu_set << File::pathSeparatorChar
                     << name << std::endl;
        }
      }
    }
  }

  // Mark config.dat as upgraded.
  config.config_revision_number(3);
  return true;
}

static bool convert_to_v4(UIWindow* window, Config& config) {
  ShowBanner(window, "Updating to 5.2+ v4 format...");

  Networks networks(config);
  if (!networks.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks (needed to load subs)";
    return false;
  }

  const auto datadir = config.datadir();
  Subs subs(datadir, networks.networks());
  Dirs dirs(datadir, 0);
  if (!subs.Load()) {
    std::cout << "Unable to load subs.json. Aborting." << std::endl;
    return false;
  }
  if (!dirs.Load()) {
    std::cout << "Unable to load subs.json. Aborting." << std::endl;
    return false;
  }
  Conferences confs(datadir, subs, dirs, 0);

  const auto f = UpgradeConferences(config, subs, dirs);
  if (!confs.LoadFromFile(f)) {
    std::cout << "Failed to upgrade conferences";
  }

  // Save conferences to new version.
  if (confs.Save()) {
    if (!subs.Save()) {
      LOG(ERROR) << "Saved new conference, but failed to save subs";
    }
    if (!dirs.Save()) {
      LOG(ERROR) << "Saved new conference, but failed to save dirs";
    }
  }

  // Mark config.dat as upgraded.
  config.config_revision_number(4);
  return true;
}


bool convert_to_v5(UIWindow* window, Config& config) {
  ShowBanner(window, "Updating to 5.2+ v5 format...");
  Networks networks(config);
  if (!networks.IsInitialized()) {
    LOG(ERROR) << "Unable to load networks (needed to load subs)";
    return false;
  }

  // Update GFiles to JSON
  GFiles gfiles(config.datadir(), config.max_backups());
  if (gfiles.Load()) {
    gfiles.Save();
  }

  // Update networks to get rid of base packet config.
  for (const auto& net : networks.networks()) {
    if (net.type == network_type_t::ftn) {
      fido::FidoCallout callout(config, net);
      for (auto& [a, node_config] : callout.node_configs_map()) {
        auto merged_config = callout.merged_packet_config_for(a, net.fido.packet_config);
        node_config.packet_config = merged_config;
      }
      callout.Save();
    }
  }
  networks.Save();

  // Update QWK to new version.
  auto qwk_config = read_qwk_cfg(config);
  write_qwk_cfg(config, qwk_config);

  // Mark config.dat as upgraded.
  config.config_revision_number(5);
  return true;
}

static bool UseMinimalNewUserInfo(IniFile& ini) {
  if (ini.IsOpen()) {
    return ini.value<bool>("NEWUSER_MIN");
  }
  return false;
}

// This is the first one of 5.7
bool convert_to_v6(UIWindow* window, Config& config) {
  ShowBanner(window, "Updating to 5.7+ v6 format...");
  IniFile ini(FilePath(config.root_directory(), WWIV_INI), {INI_TAG});
  auto minimal = true;
  int num_instances = 8;
  std::string temp_directory = "e/%n/temp";
  std::string batch_directory = "e/%n/batch";
  std::string scratch_directory = "e/%n/scratch";
  if (ini.IsOpen()) {
    minimal = UseMinimalNewUserInfo(ini);
    temp_directory = ini.value<std::string>("TEMP_DIRECTORY", "e/%n/temp");
    batch_directory = ini.value<std::string>("BATCH_DIRECTORY", "e/%n/temp");
    num_instances = ini.value<int>("NUM_INSTANCES", 8);
  }
  auto& nc = config.newuser_config();
  if (minimal) {
    nc.use_real_name = newuser_item_type_t::unused;
    nc.use_voice_phone = newuser_item_type_t::unused;
    nc.use_data_phone = newuser_item_type_t::unused;
    nc.use_address_street = newuser_item_type_t::unused;
    nc.use_callsign = newuser_item_type_t::unused;
    nc.use_computer_type = newuser_item_type_t::unused;
    nc.use_email_address = newuser_item_type_t::optional;
  }

  config.batch_format(batch_directory);
  config.scratch_format(scratch_directory);
  config.temp_format(temp_directory);
  config.num_instances(num_instances);
  // Mark config.dat as upgraded.
  config.config_revision_number(6);
  return true;
}

static config_upgrade_state_t ensure_latest_5x_config(UIWindow* window, Config& config) {
  const auto config_revision_number = config.config_revision_number();
  VLOG(1) << "ensure_latest_5x_config: desired version=" << config_revision_number;
  if (config_revision_number >= final_wwiv_config_dat_version()) {
    VLOG(1) << "ensure_latest_5x_config: ALREADY LATEST";
    return config_upgrade_state_t::already_latest;
  }
  // v1 is the GA 5.5 config version.
  if (config_revision_number < 1) {
    LOG(INFO) << "ensure_latest_5x_config: converting to v1";
    convert_to_v1(window, config);
  }
  // v2-v5 added during 5.6.  Likely v5 will be the GA 5.6 version.
  if (config_revision_number < 2) {
    // Versioned subs, chains, and dirs
    LOG(INFO) << "ensure_latest_5x_config: converting to v2";
    convert_to_v2(window, config);
  }
  if (config_revision_number < 3) {
    // menus in JSON format.
    LOG(INFO) << "ensure_latest_5x_config: converting to v3";
    convert_to_v3(window, config);
  }
  if (config_revision_number < 4) {
    // Conferences in JSON
    LOG(INFO) << "ensure_latest_5x_config: converting to v4";
    convert_to_v4(window, config);
  }
  if (config_revision_number < 5) {
    // gfiles in JSON format.
    LOG(INFO) << "ensure_latest_5x_config: converting to v5";
    convert_to_v5(window, config);
  }
  if (config_revision_number < 6) {
    // Newuser info by-field.
    LOG(INFO) << "ensure_latest_5x_config: converting to v6";
    convert_to_v6(window, config);
  }
  VLOG(1) << "ensure_latest_5x_config: UPGRADED";
  return config_upgrade_state_t::upgraded;
}

bool convert_config_424_to_430(UIWindow* window, const std::filesystem::path& config_filename) {
  File file(config_filename);
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return false;
  }
  window->SetColor(SchemeId::INFO);
  write_and_log(window, "Converting config.dat to 4.3/5.x format...\n");
  window->SetColor(SchemeId::NORMAL);
  configrec syscfg53{};
  file.Read(&syscfg53, sizeof(configrec));
  auto menus_dir = File::EnsureTrailingSlash("menus");
  to_char_array(syscfg53.menudir, FilePath(syscfg53.gfilesdir, menus_dir).string());

  arcrec arc[MAX_ARCS]{};
  for (auto i = 0; i < MAX_ARCS; i++) {
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

  File archiver(FilePath(syscfg53.datadir, ARCHIVER_DAT));
  if (!archiver.Open(File::modeBinary | File::modeWriteOnly | File::modeCreateFile)) {
    write_and_log(window, "Couldn't open 'ARCHIVER_DAT' for writing.\n");
    write_and_log(window, "Creating new file....\n");
    create_arcs(window, syscfg53.datadir);
    window->Puts( "\n");
    if (!archiver.Open(File::modeBinary | File::modeWriteOnly | File::modeCreateFile)) {
      messagebox(window, "Still unable to open archiver.dat. Something is really wrong.");
      return false;
    }
  }
  return archiver.Write(arc, MAX_ARCS * sizeof(arcrec));
}



ShouldContinue do_wwiv_ugprades(UIWindow* window, const std::string& bbsdir) {
  // Start by checking if we're already at the latest.
  if (File::Exists(FilePath(bbsdir, CONFIG_JSON))) {
    // Good, we have a config.json already, now make sure it's at the latest 5.x config version.
    Config config56(bbsdir);
    if (!config56.IsInitialized()) {
      messagebox(window, "Unable to load config.json");
      return ShouldContinue::EXIT;
    }
    if (auto state = ensure_latest_5x_config(window, config56);
        state == config_upgrade_state_t::upgraded) {
      if (!config56.Save()) {
        messagebox(window, "Unable to save upgrades config.json");
        return ShouldContinue::EXIT;
      }
    }
    return ShouldContinue::CONTINUE;
  }

  if (const auto path = FilePath(bbsdir, CONFIG_DAT); File::Exists(path)) {
    // Now start with 4.24 and work forwards, doing each upgrade one at a time.
    // We have a config.dat at least.  
    const File file(FilePath(bbsdir, CONFIG_DAT));
    backup_file(path, 10);
    if (file.length() != sizeof(configrec)) {
      // Convert 4.2X to 4.3 format if needed.
      // TODO(rushfan): make a sub-window here but until this clear the altcharset background.
      if (!dialog_yn(curses_out->window(), "Upgrade config.dat from 4.x format?")) {
        return ShouldContinue::EXIT;
      }
      window->Bkgd(' ');
      if (!convert_config_424_to_430(window, path)) {
        LOG(ERROR) << "Failed to upgrade to 4.3";
        return ShouldContinue::EXIT;
      }
      // TODO(rushfan): We need to create the 4.3 menus here or later.
    }
  }

  Config430 c430(bbsdir);
  if (!c430.IsInitialized()) {
    LOG(ERROR) << "Expected a valid 4.30 system at this point. Can't load config.dat";
    return ShouldContinue::EXIT;
  }

  // Check for 5.2+ config
  if (!c430.versioned_config_dat()) {
    // We don't have a 5.2 header, let's convert.
    if (!dialog_yn(curses_out->window(), "Upgrade config.dat to 5.2+ format?")) {
      return ShouldContinue::EXIT;
    }

    backup_file(c430.config_filename(), 10);
    convert_config_to_52(window, c430.config_filename());
  }
  // Reload changed config
  c430.Load();
  ensure_offsets_are_updated(window, c430);

  // Were done changing config430 at this point.
  // Now create the 5.x JSON. We know we have config.dat and no config.json
  backup_file(c430.config_filename(), 10);
  Config config56(bbsdir, c430.to_json_config());
  // By default a config created any other way is read only.
  config56.set_readonly(false);
  if (!config56.Save()) {
    messagebox(window, "Unable to save config.json");
    return ShouldContinue::EXIT;
  }

  if (auto state = ensure_latest_5x_config(window, config56);
      state == config_upgrade_state_t::upgraded) {
    if (!config56.Save()) {
      messagebox(window, "Unable to save upgrades config.json");
      return ShouldContinue::EXIT;
    }
  }
  return ShouldContinue::CONTINUE;
}

} // namespace wwivconfig::convert
