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
#define _DEFINE_GLOBALS_

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <memory>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <locale.h>
#include <sys/stat.h>

#include "local_io/wconstants.h"

#include "core/command_line.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/os.h"
#include "core/strings.h"
#include "core/version.cpp"
#include "core/wwivport.h"

#include "wwivconfig/archivers.h"
#include "wwivconfig/autoval.h"
#include "wwivconfig/convert.h"
#include "wwivconfig/editors.h"
#include "wwivconfig/languages.h"
#include "wwivconfig/levels.h"
#include "wwivconfig/menus.h"
#include "wwivconfig/networks.h"
#include "wwivconfig/newinit.h"
#include "wwivconfig/paths.h"
#include "wwivconfig/protocols.h"
#include "wwivconfig/regcode.h"
#include "wwivconfig/subsdirs.h"
#include "wwivconfig/sysop_account.h"
#include "wwivconfig/system_info.h"
#include "wwivconfig/user_editor.h"
#include "wwivconfig/utility.h"
#include "wwivconfig/wwivconfig.h"
#include "wwivconfig/wwivd_ui.h"
#include "sdk/vardec.h"

#include "localui/curses_io.h"
#include "localui/curses_win.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/stdio_win.h"
#include "localui/ui_win.h"
#include "localui/wwiv_curses.h"

#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/usermanager.h"

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

static bool CreateConfigOvr(const string& bbsdir) {
  IniFile oini(WWIV_INI, {"WWIV"});
  int num_instances = oini.value("NUM_INSTANCES", 4);

  std::vector<legacy_configovrrec_424_t> config_ovr_data;
  for (int i = 1; i <= num_instances; i++) {
    string instance_tag = StringPrintf("WWIV-%u", i);
    IniFile ini("wwiv.ini", {instance_tag, "WWIV"});

    string temp_directory = ini.value<string>("TEMP_DIRECTORY");
    if (temp_directory.empty()) {
      LOG(ERROR) << "TEMP_DIRECTORY is not set! Unable to create CONFIG.OVR";
      return false;
    }

    // TEMP_DIRECTORY is defined in wwiv.ini, therefore use it over config.ovr, also
    // default the batch_directory to TEMP_DIRECTORY if BATCH_DIRECTORY does not exist.
    string batch_directory(ini.value<string>("BATCH_DIRECTORY", temp_directory));

    // Replace %n with instance number value.
    const auto instance_num_string = std::to_string(i);
    StringReplace(&temp_directory, "%n", instance_num_string);
    StringReplace(&batch_directory, "%n", instance_num_string);

    File::absolute(bbsdir, &temp_directory);
    File::absolute(bbsdir, &batch_directory);

    File::EnsureTrailingSlash(&temp_directory);
    File::EnsureTrailingSlash(&batch_directory);

    legacy_configovrrec_424_t r = {};
    r.primaryport = 1;
    to_char_array(r.batchdir, batch_directory);
    to_char_array(r.tempdir, temp_directory);

    config_ovr_data.emplace_back(r);
  }

  DataFile<legacy_configovrrec_424_t> file(CONFIG_OVR, File::modeBinary | File::modeReadWrite |
                                                           File::modeCreateFile |
                                                           File::modeTruncate);
  if (!file) {
    LOG(ERROR) << "Unable to open CONFIG.OVR for writing.";
    return false;
  }

  if (!file.WriteVector(config_ovr_data)) {
    LOG(ERROR) << "Unable to write to CONFIG.OVR.";
    return false;
  }

  return true;
}

WInitApp::WInitApp() {}

WInitApp::~WInitApp() {
  // Don't leak the localIO (also fix the color when the app exits)
  delete out;
  out = nullptr;
}

int main(int argc, char* argv[]) {
  try {
    LoggerConfig config{};
    config.log_startup = true;
    config.logdir_fn_ = LogDirFromConfig;
    Logger::Init(argc, argv, config);

    std::unique_ptr<WInitApp> app(new WInitApp());
    return app->main(argc, argv);
  } catch (const std::exception& e) {
    LOG(INFO) << "Fatal exception launching wwivconfig: " << e.what();
  }
}

static bool IsUserDeleted(const User& user) { return user.data.inact & inact_deleted; }

static bool CreateSysopAccountIfNeeded(const std::string& bbsdir) {
  Config config(bbsdir);
  {
    UserManager usermanager(config);
    auto num_users = usermanager.num_user_records();
    for (int n = 1; n <= num_users; n++) {
      User u{};
      usermanager.readuser(&u, n);
      if (!IsUserDeleted(u)) {
        return true;
      }
    }
  }

  out->Cls(ACS_CKBOARD);
  if (!dialog_yn(out->window(), "Would you like to create a sysop account now?")) {
    messagebox(out->window(), "You will need to log in locally and manually create one");
    return true;
  }

  create_sysop_account(config);
  return true;
}

enum class ShouldContinue { CONTINUE, EXIT };

static ShouldContinue
read_configdat_and_upgrade_datafiles_if_needed(UIWindow* window, const wwiv::sdk::Config& config) {
  // Convert 4.2X to 4.3 format if needed.
  configrec cfg;

  File file(config.config_filename());
  if (file.length() != sizeof(configrec)) {
    // TODO(rushfan): make a subwindow here but until this clear the altcharset background.

    if (!dialog_yn(out->window(), "Upgrade config.dat from 4.x format?")) {
      return ShouldContinue::EXIT;
    }
    window->Bkgd(' ');
    convert_config_424_to_430(window, config);
  }

  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    file.Read(&cfg, sizeof(configrec));
  }
  file.Close();

  // Check for 5.2 config
  {
    static const std::string expected_sig = "WWIV";
    if (expected_sig != cfg.header.header.signature) {
      // We don't have a 5.2 header, let's convert.
      if (!dialog_yn(out->window(), "Upgrade config.dat to 5.2 format?")) {
        return ShouldContinue::EXIT;
      }

      convert_config_to_52(window, config);
      {
        if (file.Open(File::modeBinary | File::modeReadOnly)) {
          file.Read(&cfg, sizeof(configrec));
        }
        file.Close();
      }
    }
    ensure_latest_5x_config(window, config, cfg);
  }

  ensure_offsets_are_updated(window, config);
  return ShouldContinue::CONTINUE;
}

static void ShowHelp(CommandLine& cmdline) {
  std::cout << cmdline.GetHelp() << std::endl;
  exit(1);
}

static bool config_offsets_matches_actual(const Config& config) {
  File file(config.config_filename());
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return false;
  }
  configrec x{};
  file.Read(&x, sizeof(configrec));

  // update user info data
  int16_t userreclen = static_cast<int16_t>(sizeof(userrec));
  int16_t waitingoffset = offsetof(userrec, waiting);
  int16_t inactoffset = offsetof(userrec, inact);
  int16_t sysstatusoffset = offsetof(userrec, sysstatus);
  int16_t fuoffset = offsetof(userrec, forwardusr);
  int16_t fsoffset = offsetof(userrec, forwardsys);
  int16_t fnoffset = offsetof(userrec, net_num);

  if (userreclen != x.userreclen || waitingoffset != x.waitingoffset ||
      inactoffset != x.inactoffset || sysstatusoffset != x.sysstatusoffset ||
      fuoffset != x.fuoffset || fsoffset != x.fsoffset || fnoffset != x.fnoffset) {
    return false;
  }
  return true;
}

bool legacy_4xx_menu(const Config& config, UIWindow* window) {
  bool done = false;
  int selected = -1;
  do {
    out->Cls(ACS_CKBOARD);
    out->footer()->SetDefaultFooter();

    vector<ListBoxItem> items = {{"N. Network Configuration", 'N'},
                                 {"U. User Editor", 'U'},
                                 {"W. wwivd Configuration", 'W'},
                                 {"Q. Quit", 'Q'}};

    int selected_hotkey = -1;
    {
      ListBox list(window, "Main Menu", items);
      list.selection_returns_hotkey(true);
      list.set_additional_hotkeys("$");
      list.set_selected(selected);
      ListBoxResult result = list.Run();
      selected = list.selected();
      if (result.type == ListBoxResultType::HOTKEY) {
        selected_hotkey = result.hotkey;
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
    }
    out->footer()->SetDefaultFooter();

    // It's easier to use the hotkey for this case statement so it's simple to know
    // which case statement matches which item.
    switch (selected_hotkey) {
    case 'Q':
      done = true;
      break;
    case 'N':
      networks(config);
      break;
    case 'U':
      user_editor(config);
      break;
    case 'W':
      wwivd_ui(config);
      break;
    case '$': {
      vector<string> lines;
      std::ostringstream ss;
      ss << "WWIV " << wwiv_version << beta_version << " wwivconfig compiled " << wwiv_date;
      lines.push_back(ss.str());
      lines.push_back(StrCat("QSCan Lenth: ", config.qscn_len()));
      messagebox(window, lines);
    } break;
    }
    out->SetIndicatorMode(IndicatorMode::NONE);
  } while (!done);

  return true;
}

int WInitApp::main(int argc, char** argv) {
  setlocale(LC_ALL, "");

  CommandLine cmdline(argc, argv, "net");
  cmdline.AddStandardArgs();
  cmdline.set_no_args_allowed(true);
  cmdline.add_argument(BooleanCommandLineArgument(
      "initialize", "Initialize the datafiles for the 1st time and exit.", false));
  cmdline.add_argument(
      BooleanCommandLineArgument("user_editor", 'U', "Run the user editor and then exit.", false));
  cmdline.add_argument(
      BooleanCommandLineArgument("menu_editor", 'M', "Run the menu editor and then exit.", false));
  cmdline.add_argument(
      BooleanCommandLineArgument("network_editor", 'N', "Run the network editor and then exit.", false));
  cmdline.add_argument(
      BooleanCommandLineArgument("4xx", '4', "Only run editors that work on WWIV 4.xx.", false));
  cmdline.add_argument({"menu_dir", "Override the menu directory when using --menu_editor.", ""});

  if (!cmdline.Parse() || cmdline.help_requested()) {
    ShowHelp(cmdline);
    return 0;
  }
  
  auto bbsdir = File::EnsureTrailingSlash(cmdline.bbsdir());
  const bool forced_initialize = cmdline.barg("initialize");
  UIWindow* window;
  if (forced_initialize) {
    window = new StdioWindow(nullptr, new ColorScheme());
  } else {
    CursesIO::Init(StringPrintf("WWIV %s%s Configuration Program.", wwiv_version, beta_version));
    window = out->window();
    out->Cls(ACS_CKBOARD);
    window->SetColor(SchemeId::NORMAL);
  }

  if (forced_initialize && File::Exists(CONFIG_DAT)) {
    messagebox(window, "Unable to use --initialize when CONFIG.DAT exists.");
    return 1;
  }
  bool need_to_initialize = !File::Exists(CONFIG_DAT) || forced_initialize;

  if (need_to_initialize) {
    window->Bkgd(' ');
    if (!new_init(window, bbsdir, need_to_initialize)) {
      return 2;
    }
  }

  Config config(bbsdir);

  bool legacy_4xx_mode = false;
  if (cmdline.barg("menu_editor")) {
    out->Cls(ACS_CKBOARD);
    out->footer()->SetDefaultFooter();
    auto menu_dir = config.menudir();
    auto menu_dir_arg = cmdline.sarg("menu_dir");
    if (!menu_dir_arg.empty()) {
      menu_dir = menu_dir_arg;
    }
    menus(menu_dir);
    return 0;
  } else if (cmdline.barg("user_editor")) {
    out->Cls(ACS_CKBOARD);
    out->footer()->SetDefaultFooter();
    user_editor(config);
    return 0;
  } else if (cmdline.barg("network_editor")) {
    out->Cls(ACS_CKBOARD);
    out->footer()->SetDefaultFooter();
    if (!config_offsets_matches_actual(config)) {
      return 1;
    }
    networks(config);
    return 0;
  } else if (cmdline.barg("4xx")) {
    if (!config_offsets_matches_actual(config)) {
      return 1;
    }
    legacy_4xx_mode = true;
  }

  if (!legacy_4xx_mode &&
        read_configdat_and_upgrade_datafiles_if_needed(window, config) ==
        ShouldContinue::EXIT) {
    legacy_4xx_mode = true;
  }

  if (legacy_4xx_mode) {
    legacy_4xx_menu(config, window);
    return 0;
  }
  CreateConfigOvr(bbsdir);

  {
    File archiverfile(FilePath(config.datadir(), ARCHIVER_DAT));
    if (!archiverfile.Open(File::modeBinary | File::modeReadOnly)) {
      create_arcs(window, config.datadir());
    }
  }

  if (forced_initialize) {
    return 0;
  }

  // GP - We can move this up to after "read_status" if the
  // wwivconfig --initialize flow should query the user to make an account.
  CreateSysopAccountIfNeeded(bbsdir);

  bool done = false;
  int selected = -1;
  do {
    out->Cls(ACS_CKBOARD);
    out->footer()->SetDefaultFooter();

    vector<ListBoxItem> items = {{"G. General System Configuration", 'G'},
                                 {"P. System Paths", 'P'},
                                 {"T. External Transfer Protocol Configuration", 'T'},
                                 {"E. External Editor Configuration", 'E'},
                                 {"S. Security Level Configuration", 'S'},
                                 {"V. Auto-Validation Level Configuration", 'V'},
                                 {"A. Archiver Configuration", 'A'},
                                 {"L. Language Configuration", 'L'},
                                 {"M. Menu Editor", 'M'},
                                 {"N. Network Configuration", 'N'},
                                 {"R. Registration Information", 'R'},
                                 {"U. User Editor", 'U'},
                                 {"X. Update Sub/Directory Maximums", 'X'},
                                 {"W. wwivd Configuration", 'W'},
                                 {"Q. Quit", 'Q'}};

    int selected_hotkey = -1;
    {
      ListBox list(window, "Main Menu", items);
      list.selection_returns_hotkey(true);
      list.set_additional_hotkeys("$");
      list.set_selected(selected);
      ListBoxResult result = list.Run();
      selected = list.selected();
      if (result.type == ListBoxResultType::HOTKEY) {
        selected_hotkey = result.hotkey;
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
    }
    out->footer()->SetDefaultFooter();

    // It's easier to use the hotkey for this case statement so it's simple to know
    // which case statement matches which item.
    switch (selected_hotkey) {
    case 'Q':
      done = true;
      break;
    case 'G':
      sysinfo1(config);
      break;
    case 'P':
      setpaths(config);
      break;
    case 'T':
      extrn_prots(config.datadir());
      break;
    case 'E':
      extrn_editors(config);
      break;
    case 'S':
      sec_levs(config);
      break;
    case 'V':
      autoval_levs(config);
      break;
    case 'A':
      edit_archivers(config);
      break;
    case 'L':
      edit_languages(config);
      break;
    case 'N':
      networks(config);
      break;
    case 'M':
      menus(config.menudir());
      break;
    case 'R':
      edit_registration_code(config);
      break;
    case 'U':
      user_editor(config);
      break;
    case 'W':
      wwivd_ui(config);
      break;
    case 'X':
      up_subs_dirs(config);
      break;
    case '$': {
      vector<string> lines;
      std::ostringstream ss;
      ss << "WWIV " << wwiv_version << beta_version << " wwivconfig compiled " << wwiv_date;
      lines.push_back(ss.str());
      lines.push_back(StrCat("QSCan Lenth: ", config.qscn_len()));
      messagebox(window, lines);
    } break;
    }
    out->SetIndicatorMode(IndicatorMode::NONE);
  } while (!done);

  config.Save();
  return 0;
}
