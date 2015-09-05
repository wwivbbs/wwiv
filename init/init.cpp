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
#define _DEFINE_GLOBALS_

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <curses.h>
#include <fcntl.h>
#include <memory>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <locale.h>
#include <sys/stat.h>

#include "bbs/wconstants.h"

#include "core/inifile.h"
#include "core/strings.h"
#include "core/file.h"
#include "core/os.h"
#include "core/version.cpp"
#include "core/wfndfile.h"
#include "core/wwivport.h"

#include "init/archivers.h"
#include "init/autoval.h"
#include "init/editors.h"
#include "init/init.h"
#include "init/instance_settings.h"
#include "init/languages.h"
#include "init/levels.h"
#include "init/networks.h"
#include "init/newinit.h"
#include "init/paths.h"
#include "init/protocols.h"
#include "init/regcode.h"
#include "init/subsdirs.h"
#include "init/system_info.h"
#include "init/user_editor.h"
#include "init/wwivinit.h"
#include "init/utility.h"

#include "initlib/input.h"
#include "initlib/curses_io.h"
#include "initlib/listbox.h"

#include "sdk/filenames.h"

using std::string;
using std::vector;
using wwiv::core::IniFile;
using wwiv::strings::StringPrintf;
using wwiv::strings::StringReplace;

initinfo_rec initinfo;
configrec syscfg;
statusrec status;
newexternalrec *externs, *over_intern;
net_networks_rec *net_networks;

// from convert.cpp
void convcfg(CursesWindow* window, const string& config_filename);

static void ValidateConfigOverlayExists(const string& bbsdir) {
  IniFile ini(WWIV_INI, "WWIV");
  int num_instances = ini.GetNumericValue("NUM_INSTANCES", 4);

  File config_overlay(CONFIG_OVR);
  if (!config_overlay.Exists() || config_overlay.GetLength() < sizeof(configoverrec)) {
    // Handle the case where there is no config.ovr.
    write_instance(1, syscfg.tempdir, syscfg.tempdir);
  }

  if (config_overlay.GetLength() < static_cast<long>(num_instances * sizeof(configoverrec))) {
    const string base(bbsdir);
    // Not enough instances are configured.  Recreate all of them based on INI setting.
    for (int i=1; i <= num_instances; i++) {
      string instance_tag = StringPrintf("WWIV-%u", i);
      IniFile ini("wwiv.ini", instance_tag, "WWIV");

      const char* temp_directory_char = ini.GetValue("TEMP_DIRECTORY");
      if (temp_directory_char != nullptr) {
        string temp_directory(temp_directory_char);
        // TEMP_DIRECTORY is defined in wwiv.ini, therefore use it over config.ovr, also 
        // default the batch_directory to TEMP_DIRECTORY if BATCH_DIRECTORY does not exist.
        string batch_directory(ini.GetValue("BATCH_DIRECTORY", temp_directory.c_str()));

        // Replace %n with instance number value.
        const string instance_num_string = std::to_string(i);
        StringReplace(&temp_directory, "%n", instance_num_string);
        StringReplace(&batch_directory, "%n", instance_num_string);

        File::MakeAbsolutePath(base, &temp_directory);
        File::MakeAbsolutePath(base, &batch_directory);

        File::EnsureTrailingSlash(&temp_directory);
        File::EnsureTrailingSlash(&batch_directory);
        write_instance(i, batch_directory, temp_directory);
      }
    }
  }
}

WInitApp::WInitApp() {
  const string title = StringPrintf("WWIV %s%s Initialization/Configuration Program.", wwiv_version, beta_version);
  CursesIO::Init(title);
}

WInitApp::~WInitApp() {
  // Don't leak the localIO (also fix the color when the app exits)
  delete out;
  out = nullptr;
}

int main(int argc, char* argv[]) {
  try {
    std::unique_ptr<WInitApp> app(new WInitApp());
    return app->main(argc, argv);
  } catch (std::exception& e) {
    std::clog << "Fatal exception launching init: " << std::endl 
              << e.what() << std::endl;
  }
}

int WInitApp::main(int argc, char *argv[]) {
  setlocale (LC_ALL,"");

  const string wwiv_dir = wwiv::os::environment_variable("WWIV_DIR");
  if (!wwiv_dir.empty()) {
    File::set_current_directory(wwiv_dir);
  }

  string current_dir = File::current_directory();
  File::EnsureTrailingSlash(&current_dir);
  const string bbsdir(current_dir);

  out->Cls(ACS_CKBOARD);
  out->window()->SetColor(SchemeId::NORMAL);

  bool newbbs = false;
  if (!File::Exists(CONFIG_DAT)) {
    vector<string> lines = { StringPrintf("%s NOT FOUND.", CONFIG_DAT), "", "Perform initial installation?" };
    if (dialog_yn(out->window(), lines)) {
      // TODO(rushfan): make a subwindow here but until this clear the altcharset background.
      out->window()->Bkgd(' ');
      if (!new_init(out->window(), bbsdir)) {
        return 2;
      }
      newbbs = true;
      if (!File::Exists(CONFIG_DAT)) {
        messagebox(out->window(), "Unable to open config.dat");
      }
    } else {
      return 1;
    }
  }

  // Convert 4.2X to 4.3 format if needed.
  File configfile(CONFIG_DAT);
  if (configfile.GetLength() != sizeof(configrec)) {
    // TODO(rushfan): Create a subwindow
    convcfg(out->window(), CONFIG_DAT);
  }

  if (configfile.Open(File::modeBinary|File::modeReadOnly)) {
    configfile.Read(&syscfg, sizeof(configrec));
  }
  configfile.Close();

  ValidateConfigOverlayExists(bbsdir);

  {
    File archiverfile(syscfg.datadir, ARCHIVER_DAT);
    if (!archiverfile.Open(File::modeBinary|File::modeReadOnly)) {
      create_arcs(out->window());
    }
  }
  bool bDataDirectoryOk = read_status();
  if (bDataDirectoryOk) {
    net_networks = (net_networks_rec *) malloc(MAX_NETWORKS * sizeof(net_networks_rec));
    memset(net_networks, 0, MAX_NETWORKS * sizeof(net_networks_rec));

    File networksfile(syscfg.datadir, NETWORKS_DAT);
    if (networksfile.Open(File::modeBinary|File::modeReadOnly)) {
      initinfo.net_num_max = networksfile.GetLength() / sizeof(net_networks_rec);
      if (initinfo.net_num_max > MAX_NETWORKS) {
        initinfo.net_num_max = MAX_NETWORKS;
      }
      if (initinfo.net_num_max) {
        networksfile.Read(net_networks, initinfo.net_num_max * sizeof(net_networks_rec));
      }
      networksfile.Close();
    }
  }

  if (newbbs) {
    out->Cls(ACS_CKBOARD);
    bool pwok = false;
    while (!pwok) {
      vector<string> lines { "Please enter the System Password. "};
      lines.insert(lines.begin(), "");
      lines.insert(lines.begin(), "Note: Your system password defaults to 'SYSOP'.");
      string given_password;
      input_password(out->window(), "SY:", lines, &given_password, 20);
      if (given_password != syscfg.systempw) {
        out->Cls(ACS_CKBOARD);
        messagebox(out->window(), "I'm sorry, that isn't the correct system password.");
      } else {
        pwok = true;
      }
    }
  }

  bool done = false;
  do {
    out->Cls(ACS_CKBOARD);
    out->footer()->SetDefaultFooter();

    vector<ListBoxItem> items = {
        { "G. General System Configuration", 'G' },
        { "P. System Paths", 'P' },
        { "T. External Transfer Protocol Configuration", 'T' },
        { "E. External Editor Configuration", 'E' },
        { "S. Security Level Configuration", 'S' },
        { "V. Auto-Validation Level Configuration", 'V' },
        { "A. Archiver Configuration", 'A' },
        { "I. Instance Configuration", 'I' },
        { "L. Language Configuration", 'L' },
        { "N. Network Configuration", 'N' },
        { "R. Registration Information", 'R' },
        { "U. User Editor", 'U' },
        { "X. Update Sub/Directory Maximums", 'X' },
        { "Q. Quit", 'Q' }
    };

    int selected_hotkey = -1;
    {
      ListBox list(out, out->window(), "Main Menu", static_cast<int>(floor(out->window()->GetMaxX() * 0.8)), 
          static_cast<int>(floor(out->window()->GetMaxY() * 0.8)), items, out->color_scheme());
      list.selection_returns_hotkey(true);
      list.set_additional_hotkeys("$");
      ListBoxResult result = list.Run();
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
      sysinfo1();
      break;
    case 'P':
      setpaths();
      break;
    case 'T':
      extrn_prots();
      break;
    case 'E':
      extrn_editors();
      break;
    case 'S':
      sec_levs();
      break;
    case 'V':
      autoval_levs();
      break;
    case 'A':
      edit_archivers();
      break;
    case 'I':
      instance_editor();
      break;
    case 'L':
      edit_languages();
      break;
    case 'N':
      networks();
      break;
    case 'R':
      edit_registration_code();
      break;
    case 'U':
      user_editor();
      break;
    case '$': {
      vector<string> lines;
      lines.push_back(StringPrintf("QSCan Lenth: %lu", syscfg.qscn_len));
      lines.push_back(StringPrintf("WWIV %s%s INIT compiled %s", wwiv_version, beta_version, const_cast<char*>(wwiv_date)));
      messagebox(out->window(), lines);
    } break;
    case 'X':
      up_subs_dirs();
      break;
    }
    out->SetIndicatorMode(IndicatorMode::NONE);
  } while (!done);

  return 0;
}
