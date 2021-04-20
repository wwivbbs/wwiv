/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#include "wwivconfig/newinit.h"

#include "subsdirs.h"
#include "core/datafile.h"
#include "core/datetime.h"
#include "core/file.h"
#include "core/strings.h"
#include "core/version.h"
#include "core/wwivport.h"
#include "localui/input.h"
#include "localui/ui_win.h"
#include "localui/wwiv_curses.h"
#include "sdk/filenames.h"
#include "sdk/subxtr.h"
#include "sdk/user.h"
#include "sdk/vardec.h"
#include "sdk/wwivcolors.h"
#include "sdk/files/dirs.h"
#include "wwivconfig/archivers.h"
#include "wwivconfig/utility.h"
#include "wwivconfig/wwivd_ui.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::local::ui;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static void write_qscn(const Config& config, unsigned int un, uint32_t* qscn) {
  File file(FilePath(config.datadir(), USER_QSC));
  if (file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile)) {
    file.Seek(config.qscn_len() * un, File::Whence::begin);
    file.Write(qscn, config.qscn_len());
    file.Close();
  }
}

static bool unzip_file(UIWindow* window, const std::string& zipfile, const std::string& dir) {
  if (File::Exists(zipfile)) {
    window->SetColor(SchemeId::NORMAL);
    window->Puts(fmt::format("Decompressing file: {} to dir: {}\n", zipfile, dir));
    const auto unzip_cmd = StrCat("unzip -qq -o ", zipfile, " -d", dir);
    const auto rc = system(unzip_cmd.c_str());
    if (rc != 0) {
      window->SetColor(SchemeId::ERROR_TEXT);
      window->Puts("ERROR Unable to unzip file.\n");
    }

    const auto sysop_dir = FilePath("dloads", "sysop");
    File::Rename(zipfile, FilePath(sysop_dir, zipfile));
    return true;
  }
  return false;
}

static void init_files(UIWindow* window, const std::string& bbsdir, bool unzip_files) {
  window->SetColor(SchemeId::PROMPT);
  window->Puts("Creating Data Files.\n");
  window->SetColor(SchemeId::NORMAL);

  config_t cfg{};

  // Set header
  cfg.header.config_revision_number = 5;
  cfg.header.written_by_wwiv_num_version = wwiv_config_version();
  cfg.header.last_written_date = DateTime::now().to_string();

  cfg.systempw = "SYSOP";
  cfg.systemname = "My WWIV BBS";
  cfg.systemphone = "   -   -    ";
  cfg.sysopname = "The New Sysop";

  cfg.datadir = "data";
  cfg.msgsdir = "msgs";
  cfg.gfilesdir = "gfiles";
  cfg.dloadsdir = "dloads";
  cfg.menudir = "menus";
  cfg.scriptdir = "scripts";
  cfg.logdir = "logs";

  cfg.tempdir_format = "e/%n/temp";
  cfg.batchdir_format = "e/%n/batch";
  cfg.scratchdir_format = "e/%n/scratch";
  cfg.num_instances = 8;

  cfg.newusersl = 10;
  cfg.newuserdsl = 0;
  cfg.validated_sl = cfg.newusersl + 1;
  cfg.maxwaiting = 50;

  cfg.newuploads = 0;
  cfg.maxusers = 500;
  cfg.newuser_restrict = User::restrictValidate;
  cfg.req_ratio = 0.0;
  cfg.newusergold = 100.0;

  validation_config_t v{};
  v.sl = 10;
  v.dsl = 0;
  v.ar = 0;
  v.dar = 0;
  v.restrict = 0;

  for (auto i = 1; i <= 10; i++) {
    v.name = fmt::format("Validation #{}", i);
    cfg.autoval.emplace(i, v);
  }
  for (auto i = 0; i < 256; i++) {
    slrec sl{};
    sl.time_per_logon = static_cast<uint16_t>((i / 10) * 10);
    sl.time_per_day = static_cast<uint16_t>(sl.time_per_logon * 2.5);
    sl.messages_read = static_cast<uint16_t>((i / 10) * 100);
    if (i < 10) {
      sl.emails = 0;
    } else if (i <= 19) {
      sl.emails = 5;
    } else {
      sl.emails = 20;
    }

    if (i <= 25) {
      sl.posts = 10;
    } else if (i <= 79) {
      sl.posts = 20;
    } else {
      sl.posts = 50;
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
      sl.time_per_day = 1440;
      sl.posts = 255;
      sl.emails = 255;
    }
    cfg.sl.emplace(i, sl);
  }

  cfg.userreclen = sizeof(userrec);
  cfg.waitingoffset = offsetof(userrec, waiting);
  cfg.inactoffset = offsetof(userrec, inact);
  cfg.sysstatusoffset = offsetof(userrec, sysstatus);
  cfg.fuoffset = offsetof(userrec, forwardusr);
  cfg.fsoffset = offsetof(userrec, forwardsys);
  cfg.fnoffset = offsetof(userrec, net_num);

  cfg.max_subs = default_num_subs;
  cfg.max_dirs = default_num_dirs;
  cfg.qscn_len =
      4 * (1 + cfg.max_subs + (cfg.max_subs + 31) / 32 + (cfg.max_dirs + 31) / 32);

  cfg.post_call_ratio = 0.0;

  // Create a 5.6+ style config.json
  Config config(bbsdir, cfg);
  // Since this was not loaded from disk as JSON, mark it mutable so it can
  // be saved as JSON.
  config.set_readonly(false);
  config.Save();

  const auto datadir = FilePath(bbsdir, "data");
  create_arcs(window, datadir);
  statusrec_t statusrec{};
  memset(&statusrec, 0, sizeof(statusrec_t));
  auto now = date();
  to_char_array(statusrec.date1, now);
  to_char_array(statusrec.date2, "00/00/00");
  to_char_array(statusrec.date3, "00/00/00");
  to_char_array(statusrec.log1, "000000.log");
  to_char_array(statusrec.log2, "000000.log");
  to_char_array(statusrec.gfiledate, now);
  statusrec.callernum = 65535;
  statusrec.qscanptr = 2;
  statusrec.net_bias = 0.001f;
  statusrec.net_req_free = 3.0;

  auto qsc = std::make_unique<uint32_t[]>(config.qscn_len() / sizeof(uint32_t));

  save_status(datadir.string(), statusrec);
  userrec u{};
  memset(&u, 0, sizeof(u));
  write_user(config, 0, &u);
  write_qscn(config, 0, qsc.get());

  // Note: this is where wwivconfig makes a user record #1 that is deleted for new installs.
  // TODO(rushfan): We should use User::CreateNewUserRecord here.
  u.inact = User::userDeleted;
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

  write_user(config, 1, &u);
  write_qscn(config, 1, qsc.get());
  {
    File namesfile(FilePath("data", NAMES_LST));
    namesfile.Open(File::modeBinary | File::modeReadWrite | File::modeCreateFile);
  }
  {
    subboard_t r{};
    r.name = "General";
    r.filename = "GENERAL";
    r.read_acs = "user.sl >= 10";
    r.post_acs = "user.sl >= 20";
    r.maxmsgs = 50;
    r.storage_type = 2;

    Subs subs("data/", {});
    subs.insert(0, r);
    // TODO(rushfan): Check for error.
    subs.Save();
  }

  {
    files::Dirs dirs("data/", 0);
    {
      files::directory_t d1{};
      d1.name = "Sysop";
      d1.filename = "SYSOP";
      d1.path = File::EnsureTrailingSlash(FilePath("dloads", "sysop").string());
      File::mkdir(d1.path);
      d1.acs = "user.dsl >= 100";
      d1.maxfiles = 50;

      dirs.insert(0, d1);
    }
    {
      files::directory_t d1{};
      d1.name = "Miscellaneous";
      d1.filename = "misc";
      d1.path = File::EnsureTrailingSlash(FilePath("dloads", "misc").string());
      File::mkdir(d1.path);
      d1.acs = "user.dsl >= 10";
      d1.maxfiles = 50;
      d1.mask = 0;
      dirs.insert(1, d1);
    }
    dirs.Save();
  }

  // Create wwivd.json
  auto c = LoadDaemonConfig(config);
  if (!SaveDaemonConfig(config, c)) {
    LOG(ERROR) << "Error saving wwivd.json";
  }
  window->Puts(".\n");

  window->SetColor(SchemeId::PROMPT);
  window->Puts("Copying String and Miscellaneous files.\n");
  window->SetColor(SchemeId::NORMAL);

  window->Puts(".\n");

  window->SetColor(SchemeId::PROMPT);
  window->Puts("Decompressing archives.  Please wait");
  window->SetColor(SchemeId::NORMAL);

  if (unzip_files) {
    unzip_file(window, "inifiles.zip", bbsdir);
    unzip_file(window, "gfiles.zip", "gfiles");
    unzip_file(window, "scripts.zip", "scripts");
    unzip_file(window, "data.zip", "data");
    unzip_file(window, "menus.zip", "menus");
    unzip_file(window, "regions.zip", StrCat("data", File::pathSeparatorChar, "regions"));
    unzip_file(window, "zip-city.zip", StrCat("data", File::pathSeparatorChar, "zip-city"));

#ifdef WIN32
    // Unzip netfoss to "${WWIV_DIR}/netfoss"
    unzip_file(window, "netf124.zip", "netfoss");
#endif
  }

  window->SetColor(SchemeId::NORMAL);
}

bool new_init(UIWindow* window, const std::string& bbsdir, bool unzip_files) {
  const std::vector<std::string> dirnames = {
      "attach", "data",        "data/regions", "data/zip-city", "gfiles",  "menus", "msgs",
      "dloads", "dloads/misc", "dloads/sysop", "temp",          "temp/1",  "temp/2",       "temp/3",
      "temp/4", "batch",       "batch/1",      "batch/2",       "batch/3", "batch/4",
      "logs"};
  window->SetColor(SchemeId::PROMPT);
  window->Puts("\n\nNow performing installation.  Please wait...\n\n");
  window->Puts("Creating Directories\n");
  window->SetColor(SchemeId::NORMAL);
  for (const auto& dirname : dirnames) {
    window->SetColor(SchemeId::NORMAL);
    if (const auto chdir_ok = File::set_current_directory(dirname); !chdir_ok) {
      if (!File::mkdir(dirname)) {
        window->SetColor(SchemeId::ERROR_TEXT);
        window->Puts(StrCat("\n\nERROR!!! Couldn't make '", dirname, "' Sub-Dir.\nExiting..."));
        return false;
      }
    } else {
      File::set_current_directory(bbsdir);
    }
  }

  init_files(window, bbsdir, unzip_files);
  return true;
}
