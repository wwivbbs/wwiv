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
#include "wwivconfig/wwivconfig.h"

#include "common/datetime.h"
#include "core/command_line.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/log.h"
#include "core/strings.h"
#include "core/version.h"
#include "fmt/format.h"
#include "localui/curses_io.h"
#include "localui/input.h"
#include "localui/listbox.h"
#include "localui/stdio_win.h"
#include "localui/ui_win.h"
#include "localui/wwiv_curses.h"
#include "sdk/config.h"
#include "sdk/config430.h"
#include "sdk/filenames.h"
#include "sdk/usermanager.h"
#include "sdk/vardec.h"
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
#include "wwivconfig/script_ui.h"
#include "wwivconfig/subsdirs.h"
#include "wwivconfig/sysop_account.h"
#include "wwivconfig/system_info.h"
#include "wwivconfig/user_editor.h"
#include "wwivconfig/wwivd_ui.h"

#include <clocale>
#include <cstdlib>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;
using namespace wwiv::wwivconfig::convert;

static bool CreateConfigOvrAndUpdateSysConfig(Config& config, const std::string& bbsdir) {
  IniFile oini(WWIV_INI, {"WWIV"});
  auto num_instances = oini.value("NUM_INSTANCES", 4);

  std::vector<legacy_configovrrec_424_t> config_ovr_data;
  for (auto i = 1; i <= num_instances; i++) {

    // TEMP_DIRECTORY is defined in wwiv.ini, therefore use it over config.ovr, also
    // default the batch_directory to TEMP_DIRECTORY if BATCH_DIRECTORY does not exist.
    auto temp_directory = config.temp_format();
    auto batch_directory = config.batch_format();

    // Replace %n with instance number value.
    const auto instance_num_string = std::to_string(i);
    StringReplace(&temp_directory, "%n", instance_num_string);
    StringReplace(&batch_directory, "%n", instance_num_string);

    temp_directory = File::EnsureTrailingSlash(File::absolute(bbsdir, temp_directory));
    batch_directory = File::EnsureTrailingSlash(File::absolute(bbsdir, batch_directory));

    legacy_configovrrec_424_t r{};
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

  // Duplicated from xinit.cpp
  // Since we never save config.dat from the BBS, we need to also
  // update them here so they'll get saved back to config.dat for
  // legacy applications that use it.  This still needs to happen
  // in the BBS anyway since we use the values there.
  constexpr auto INI_STR_2WAY_CHAT = "2WAY_CHAT";
  constexpr auto INI_STR_NO_NEWUSER_FEEDBACK = "NO_NEWUSER_FEEDBACK";
  constexpr auto INI_STR_TITLEBAR = "TITLEBAR";
  constexpr auto INI_STR_LOG_DOWNLOADS = "LOG_DOWNLOADS";
  constexpr auto INI_STR_CLOSE_XFER = "CLOSE_XFER";
  constexpr auto INI_STR_ALL_UL_TO_SYSOP = "ALL_UL_TO_SYSOP";
  constexpr auto INI_STR_ALLOW_ALIASES = "ALLOW_ALIASES";
  constexpr auto INI_STR_FREE_PHONE = "FREE_PHONE";
  static std::vector<ini_flags_type> sysconfig_flags = {
    {INI_STR_2WAY_CHAT, sysconfig_2_way},
    {INI_STR_NO_NEWUSER_FEEDBACK, sysconfig_no_newuser_feedback},
    {INI_STR_TITLEBAR, sysconfig_titlebar},
    {INI_STR_LOG_DOWNLOADS, sysconfig_log_dl},
    {INI_STR_CLOSE_XFER, sysconfig_no_xfer},
    {INI_STR_ALL_UL_TO_SYSOP, sysconfig_all_sysop},
    {INI_STR_ALLOW_ALIASES, sysconfig_allow_alias},
    {INI_STR_FREE_PHONE, sysconfig_free_phone}};

  config.set_sysconfig(oini.GetFlags(sysconfig_flags, config.sysconfig_flags()));

  return true;
}

WWIVConfigApplication::WWIVConfigApplication() = default;

WWIVConfigApplication::~WWIVConfigApplication() {
  // Don't leak the localIO (also fix the color when the app exits)
  delete curses_out;
  curses_out = nullptr;
}

int main(int argc, char* argv[]) {
  try {
    LoggerConfig config(LogDirFromConfig);
    Logger::Init(argc, argv, config);
    ScopeExit at_exit(Logger::ExitLogger);

    const WWIVConfigApplication app{};
    return app.Main(argc, argv);
  } catch (const std::exception& e) {
    LOG(INFO) << "Fatal exception launching WWIVconfig: " << e.what();
  } catch (...) {
    LOG(INFO) << "Unknown fatal exception launching WWIVconfig.";
  }
}

static bool IsUserDeleted(const User& user) { return user.data.inact & inact_deleted; }

static bool CreateSysopAccountIfNeeded(const std::string& bbsdir) {
  Config config(bbsdir);
  if (!config.IsInitialized()) {
    // We only create the sysop account if we're in config.json mode.
    return false;
  }
  {
    const UserManager usermanager(config);
    const auto num_users = usermanager.num_user_records();
    for (auto n = 1; n <= num_users; n++) {
      if (auto u = usermanager.readuser(n)) {
        if (!IsUserDeleted(u.value())) {
          return true;
        }
      }
    }
  }

  curses_out->Cls(ACS_CKBOARD);
  if (!dialog_yn(curses_out->window(), "Would you like to create a sysop account now?")) {
    messagebox(curses_out->window(), "You will need to log in locally and manually create one");
    return true;
  }

  create_sysop_account(config);
  return true;
}

static void ShowHelp(CommandLine& cmdline) {
  std::cout << cmdline.GetHelp() << std::endl;
  exit(1);
}

static bool config430_offsets_matches_actual(const Config430& config) {
  File file(config.config_filename());
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return false;
  }
  configrec x{};
  file.Read(&x, sizeof(configrec));

  // update user info data
  auto userreclen = static_cast<int16_t>(sizeof(userrec));
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

static bool config_dat_or_json_exists() {
  return File::Exists(CONFIG_DAT) || File::Exists(CONFIG_JSON);
}

bool legacy_4xx_menu(const Config430& config430, UIWindow* window) {
  auto done = false;
  auto selected = -1;

  std::vector<arcrec> arcs;
  Config config(config430.root_directory(), config430.to_json_config(arcs));
  do {
    curses_out->Cls(ACS_CKBOARD);
    curses_out->footer()->SetDefaultFooter();

    std::vector<ListBoxItem> items = {{"N. Network Configuration", 'N'},
                                      {"U. User Editor", 'U'},
                                      {"W. wwivd Configuration", 'W'},
                                      {"Q. Quit", 'Q'}};

    auto selected_hotkey = -1;
    {
      ListBox list(window, "Main Menu", items);
      list.selection_returns_hotkey(true);
      list.set_additional_hotkeys("$");
      list.set_selected(selected);
      auto result = list.Run();
      selected = list.selected();
      if (result.type == ListBoxResultType::HOTKEY) {
        selected_hotkey = result.hotkey;
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
    }
    curses_out->footer()->SetDefaultFooter();

    // It's easier to use the hotkey for this case statement so it's simple to know
    // which case statement matches which item.
    switch (selected_hotkey) {
    case 'Q':
      done = true;
      break;
    case 'N': {
      std::set<int> need_network3;
      networks(config, need_network3);
    } break;
    case 'U':
      user_editor(config);
      break;
    case 'W':
      wwivd_ui(config);
      break;
    case '$': {
      std::vector<std::string> lines;
      std::ostringstream ss;
      ss << "WWIV " << full_version() << " WWIVconfig compiled " << wwiv_compile_datetime();
      lines.push_back(ss.str());
      lines.push_back(StrCat("QSCan Lenth: ", config.qscn_len()));
      messagebox(window, lines);
    } break;
    default: ;
    }
    curses_out->SetIndicatorMode(IndicatorMode::none);
  } while (!done);

  return true;
}

int WWIVConfigApplication::Main(int argc, char** argv) const {
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
  cmdline.add_argument(BooleanCommandLineArgument("network_editor", 'N',
                                                  "Run the network editor and then exit.", false));
  cmdline.add_argument(
      BooleanCommandLineArgument("4xx", '4', "Only run editors that work on WWIV 4.xx.", false));

  if (!cmdline.Parse() || cmdline.help_requested()) {
    ShowHelp(cmdline);
    return 0;
  }

  auto bbsdir = File::EnsureTrailingSlash(cmdline.bbsdir());
  const auto forced_initialize = cmdline.barg("initialize");
  UIWindow* window;
  if (forced_initialize) {
    window = new StdioWindow(nullptr, new ColorScheme());
  } else {
    CursesIO::Init(fmt::format("WWIV {} Configuration Program.", full_version()));
    window = curses_out->window();
    curses_out->Cls(ACS_CKBOARD);
    window->SetColor(SchemeId::NORMAL);
  }

  if (forced_initialize && config_dat_or_json_exists()) {
    messagebox(window, "Unable to use --initialize when CONFIG.DAT exists.");
    return 1;
  }

  if (!config_dat_or_json_exists() || forced_initialize) {
    // If this is true, we need to initialize
    window->Bkgd(' ');
    if (!new_init(window, bbsdir, true)) {
      return 2;
    }
  }

  auto legacy_4xx_mode = cmdline.barg("4xx");
  if (legacy_4xx_mode) {
    Config430 config43(bbsdir);
    if (!config430_offsets_matches_actual(config43)) {
      return 1;
    }
    legacy_4xx_menu(config43, window);
    return 0;
  }

  std::set<int> need_network3;

  if (cmdline.barg("menu_editor")) {
    curses_out->Cls(ACS_CKBOARD);
    curses_out->footer()->SetDefaultFooter();
    Config config(bbsdir);
    if (!config.IsInitialized() || legacy_4xx_mode) {
      return 1;
    }
    menus(config);
    return 0;
  }
  if (cmdline.barg("user_editor")) {
    curses_out->Cls(ACS_CKBOARD);
    curses_out->footer()->SetDefaultFooter();
    Config config(bbsdir);
    if (!config.IsInitialized() || legacy_4xx_mode) {
      return 1;
    }
    user_editor(config);
    return 0;
  }
  if (cmdline.barg("network_editor")) {
    curses_out->Cls(ACS_CKBOARD);
    curses_out->footer()->SetDefaultFooter();
    Config config(bbsdir);
    if (!config.IsInitialized() || legacy_4xx_mode) {
      return 1;
    }
    networks(config, need_network3);
    return 0;
  }
  
  if (const auto config = load_any_config(bbsdir)) {
    // Create archiver.dat with defaults if it does not exist.
    {
      File archiverfile(FilePath(config->datadir(), ARCHIVER_DAT));
      if (!archiverfile.Open(File::modeBinary | File::modeReadOnly)) {
        create_arcs(window, config->datadir());
      }
    }

    // Create wwivd.json with defaults if it does not exist.
    if (!File::Exists(FilePath(config->datadir(), "wwivd.json"))) {
      auto c = LoadDaemonConfig(*config);
      if (!SaveDaemonConfig(*config, c)) {
        LOG(ERROR) << "Error saving wwivd.json";
      }
    }
  } else {
    messagebox(window, "Unable to load any config: 4.24-5.x");
    return 1;
  }

  if (forced_initialize) {
    return 0;
  }

  // This has to happen after --initialize since it needs a real curses UI.
  CreateSysopAccountIfNeeded(bbsdir);

  // Check for Upgrades needed.
  if (do_wwiv_ugprades(window, bbsdir) == ShouldContinue::EXIT) {
    return 1;
  }

  // We should be able to load a modern config.json here.
  Config config(bbsdir);
  if (!config.IsInitialized()) {
    messagebox(window, "Unable to load 5.6+ JSON config");
    return 1;
  }

  // We have a modern config, let's go.
  auto done = false;
  auto selected = -1;
  do {
    curses_out->Cls(ACS_CKBOARD);
    curses_out->footer()->SetDefaultFooter();

    std::vector<ListBoxItem> items = { {"G. General System Configuration", 'G'},
                                       {"P. System Paths", 'P'},
                                       {"T. External Transfer Protocol Configuration", 'T'},
                                       {"E. External Editor Configuration", 'E'},
                                       {"S. Security Level Configuration", 'S'},
                                       {"V. Auto-Validation Level Configuration", 'V'},
                                       {"A. Archiver Configuration", 'A'},
                                       {"L. Language Configuration", 'L'},
                                       {"M. Menu Editor", 'M'},
                                       {"N. Network Configuration", 'N'},
                                       {"U. User Editor", 'U'},
                                       {"X. Update Sub/Directory Maximums", 'X'},
                                       {"W. wwivd Configuration", 'W'},
                                       {"R. Scripting Configuration", 'R'},
                                       {"Q. Quit", 'Q'} };

    auto selected_hotkey = -1;
    {
      ListBox list(window, "Main Menu", items);
      list.selection_returns_hotkey(true);
      list.set_additional_hotkeys("$");
      list.set_selected(selected);
      auto result = list.Run();
      selected = list.selected();
      if (result.type == ListBoxResultType::HOTKEY) {
        selected_hotkey = result.hotkey;
      } else if (result.type == ListBoxResultType::NO_SELECTION) {
        done = true;
      }
    }
    curses_out->footer()->SetDefaultFooter();

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
      networks(config, need_network3);
      break;
    case 'M':
      menus(config);
      break;
    case 'U':
      user_editor(config);
      break;
    case 'W':
      wwivd_ui(config);
      break;
    case 'R':
      script_ui(config);
      break;
    case 'X':
      up_subs_dirs(config);
      break;
    case '$': {
      std::vector<std::string> lines;
      lines.emplace_back(fmt::format("WWIV {} WWIVconfig compiled {}", full_version(), wwiv_compile_datetime()));
      lines.emplace_back(fmt::format("QScan Length: {}", config.qscn_len()));
      messagebox(window, lines);
    } break;
    default: break;
    }
    curses_out->SetIndicatorMode(IndicatorMode::none);
  } while (!done);

  config.Save();
  // Write out config.ovr for compatibility reasons.
  CreateConfigOvrAndUpdateSysConfig(config, bbsdir);

  // Write out config.dat
  auto c430 = std::make_unique<Config430>(config.root_directory(), config.to_config_t());
  if (!c430 || !c430->Save()) {
    LOG(INFO) << "Failed to write legacy condig.dat";
  }

  for (const auto nn : need_network3) {
    const auto path = FilePath(File::current_directory(), "network3");
    std::ostringstream ss;
    ss << path;
#ifdef _WIN32
    ss << ".exe";
#endif
    ss << " ." << nn << " Y";
    system(ss.str().c_str());
  }
  return 0;
}
