/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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

#include "bbs/version.cpp"
#include "bbs/wconstants.h"

#include "core/inifile.h"
#include "core/strings.h"
#include "core/wfile.h"
#include "core/wfndfile.h"
#include "core/wwivport.h"

#include "init/archivers.h"
#include "init/editors.h"
#include "init/ifcns.h"
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

using std::string;
using std::vector;
using wwiv::core::IniFile;
using wwiv::strings::StringPrintf;
using wwiv::strings::StringReplace;

initinfo_rec initinfo;
configrec syscfg;
statusrec status;
subboardrec *subboards;
chainfilerec *chains;
newexternalrec *externs, *over_intern;
editorrec *editors;
arcrec *arcs;
net_networks_rec *net_networks;
languagerec *languages;

char bbsdir[MAX_PATH];
char configdat[20] = "config.dat";

static void convcfg() {
  arcrec arc[MAX_ARCS];

  int hFile = open(configdat, O_RDWR | O_BINARY);
  if (hFile > 0) {
    out->SetColor(SchemeId::INFO);
    out->window()->Printf("Converting config.dat to 4.30/5.00 format...\n");
    out->SetColor(SchemeId::NORMAL);
    read(hFile, &syscfg, sizeof(configrec));
    sprintf(syscfg.menudir, "%smenus%c", syscfg.gfilesdir, WFile::pathSeparatorChar);
    strcpy(syscfg.unused_logoff_c, " ");
    strcpy(syscfg.unused_v_scan_c, " ");

    for (int i = 0; i < MAX_ARCS; i++) {
      if ((syscfg.arcs[i].extension[0]) && (i < 4)) {
        strncpy(arc[i].name, syscfg.arcs[i].extension, 32);
        strncpy(arc[i].extension, syscfg.arcs[i].extension, 4);
        strncpy(arc[i].arca, syscfg.arcs[i].arca, 32);
        strncpy(arc[i].arce, syscfg.arcs[i].arce, 32);
        strncpy(arc[i].arcl, syscfg.arcs[i].arcl, 32);
      } else {
        strncpy(arc[i].name, "New Archiver Name", 32);
        strncpy(arc[i].extension, "EXT", 4);
        strncpy(arc[i].arca, "archive add command", 32);
        strncpy(arc[i].arce, "archive extract command", 32);
        strncpy(arc[i].arcl, "archive list command", 32);
      }
    }
    lseek(hFile, 0L, SEEK_SET);
    write(hFile, &syscfg, sizeof(configrec));
    close(hFile);

    char szFileName[ MAX_PATH ];
    sprintf(szFileName, "%sarchiver.dat", syscfg.datadir);
    hFile = open(szFileName, O_WRONLY | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
    if (hFile < 0) {
      out->window()->Printf("Couldn't open '%s' for writing.\n", szFileName);
      out->window()->Printf("Creating new file....");
      create_arcs();
      out->window()->Printf("\n");
      hFile = open(szFileName, O_WRONLY | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
    }
    write(hFile, arc, MAX_ARCS * sizeof(arcrec));
    close(hFile);
  }
}

static void show_help() {
  out->window()->Printf("   -Pxxx - Password via commandline (where xxx is your password)\n");
  out->window()->Printf("\n\n\n");
}

static void ValidateConfigOverlayExists() {
  IniFile ini("wwiv.ini", "WWIV");
  int num_instances = ini.GetNumericValue("NUM_INSTANCES", 4);

  WFile config_overlay("config.ovr");
  if (!config_overlay.Exists() || config_overlay.GetLength() < sizeof(configoverrec)) {
    // Handle the case where there is no config.ovr.
    write_instance(1, syscfg.batchdir, syscfg.tempdir);
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

        WFile::MakeAbsolutePath(base, &temp_directory);
        WFile::MakeAbsolutePath(base, &batch_directory);

        WFile::EnsureTrailingSlash(&temp_directory);
        WFile::EnsureTrailingSlash(&batch_directory);
        write_instance(i, batch_directory, temp_directory);
      }
    }
  }
}

WInitApp::WInitApp() {
  CursesIO::Init();
}

WInitApp::~WInitApp() {
  delete out;
  out = nullptr;
}

int main(int argc, char* argv[]) {
  std::unique_ptr<WInitApp> app(new WInitApp());
  return app->main(argc, argv);
}

int WInitApp::main(int argc, char *argv[]) {
  char s[81];
  bool newbbs = false;
  bool pwok = false;

  setlocale (LC_ALL,"");

  char *ss = getenv("WWIV_DIR");
  if (ss) {
    chdir(ss);
  }
  getcwd(bbsdir, MAX_PATH);

  trimstrpath(bbsdir);

  out->Cls(ACS_CKBOARD);
  out->SetColor(SchemeId::NORMAL);

  int configfile = open(configdat, O_RDWR | O_BINARY);
  if (configfile > 0) {
    // try to read it initially so we can process args right.
    read(configfile, &syscfg, sizeof(configrec));
    close(configfile);
  }
  for (int i = 1; i < argc; ++i) {
    if (strlen(argv[i]) < 2) {
      continue;
    }

    if (argv[i][0] == '-') {
      char ch = toupper(argv[i][1]);
      switch (ch) {
      case 'P': {
        if (strlen(argv[i]) > 2) {
          if (strcasecmp(argv[i] + 2, syscfg.systempw) == 0) {
            pwok = true;
          }
        }
        break;
      }
      case 'D': {
        setpaths();
        exit_init(0);
      } break;
      case '?':
        show_help();
        exit_init(0);
        break;
      }
    }
  }

  configfile = open(configdat, O_RDWR | O_BINARY);
  if (configfile < 0) {
    out->SetColor(SchemeId::ERROR_TEXT);
    out->window()->Printf("%s NOT FOUND.\n\n", configdat);
    if (dialog_yn(out->window(), "Perform initial installation")) {
      new_init();
      newbbs = true;
      configfile = open(configdat, O_RDWR | O_BINARY);
    } else {
      exit_init(1);
    }
  }

  // Convert 4.2X to 4.3 format if needed.
  if (filelength(configfile) != sizeof(configrec)) {
    close(configfile);
    convcfg();
    configfile = open(configdat, O_RDWR | O_BINARY);
  }

  read(configfile, &syscfg, sizeof(configrec));
  close(configfile);

  ValidateConfigOverlayExists();

  sprintf(s, "%sarchiver.dat", syscfg.datadir);
  int hFile = open(s, O_RDONLY | O_BINARY);
  if (hFile < 0) {
    create_arcs();
  } else {
    close(hFile);
  }

  externs = (newexternalrec *) malloc(15 * sizeof(newexternalrec));
  initinfo.numexterns = 0;
  sprintf(s, "%snextern.dat", syscfg.datadir);
  hFile = open(s, O_RDWR | O_BINARY);
  if (hFile > 0) {
    initinfo.numexterns = (read(hFile, externs, 15 * sizeof(newexternalrec))) / sizeof(newexternalrec);
    close(hFile);
  }
  over_intern = (newexternalrec *) malloc(3 * sizeof(newexternalrec));
  memset(over_intern, 0, 3 * sizeof(newexternalrec));
  sprintf(s, "%snintern.dat", syscfg.datadir);
  hFile = open(s, O_RDWR | O_BINARY);
  if (hFile > 0) {
    read(hFile, over_intern, 3 * sizeof(newexternalrec));
    close(hFile);
  }

  bool bDataDirectoryOk = read_status();
  if (bDataDirectoryOk) {
    if ((status.net_version >= 31) || (status.net_version == 0)) {
      net_networks = (net_networks_rec *) malloc(MAX_NETWORKS * sizeof(net_networks_rec));
      if (!net_networks) {
        out->window()->Printf("needed %d bytes\n", MAX_NETWORKS * sizeof(net_networks_rec));
        exit_init(2);
      }
      memset(net_networks, 0, MAX_NETWORKS * sizeof(net_networks_rec));

      sprintf(s, "%snetworks.dat", syscfg.datadir);
      hFile = open(s, O_RDONLY | O_BINARY);
      if (hFile > 0) {
        initinfo.net_num_max = filelength(hFile) / sizeof(net_networks_rec);
        if (initinfo.net_num_max > MAX_NETWORKS) {
          initinfo.net_num_max = MAX_NETWORKS;
        }
        if (initinfo.net_num_max) {
          read(hFile, net_networks, initinfo.net_num_max * sizeof(net_networks_rec));
        }
        close(hFile);
      }
    }
  }

  languages = (languagerec*) malloc(MAX_LANGUAGES * sizeof(languagerec));
  if (!languages) {
    out->window()->Printf("needed %d bytes\n", MAX_LANGUAGES * sizeof(languagerec));
    exit_init(2);
  }

  sprintf(s, "%slanguage.dat", syscfg.datadir);
  int hLanguageFile = open(s, O_RDONLY | O_BINARY);
  if (hLanguageFile >= 0) {
    initinfo.num_languages = filelength(hLanguageFile) / sizeof(languagerec);
    read(hLanguageFile, languages, initinfo.num_languages * sizeof(languagerec));
    close(hLanguageFile);
  }
  if (!initinfo.num_languages) {
    initinfo.num_languages = 1;
    strcpy(languages->name, "English");
    strncpy(languages->dir, syscfg.gfilesdir, sizeof(languages->dir) - 1);
    strncpy(languages->mdir, syscfg.gfilesdir, sizeof(languages->mdir) - 1);
  }

  if (languages->mdir[0] == 0) {
    strncpy(languages->mdir, syscfg.gfilesdir, sizeof(languages->mdir) - 1);
  }

  if (!pwok) {
    nlx();
    vector<string> lines { "Please enter the System Password. "};
    if (newbbs) {
      lines.insert(lines.begin(), "");
      lines.insert(lines.begin(), "Note: Your system password defaults to 'SYSOP'.");
    }
    input_password(out->window(), "SY:", lines, s, 20);
    if (strcmp(s, (syscfg.systempw)) != 0) {
      out->Cls(ACS_CKBOARD);
      messagebox(out->window(), "I'm sorry, that isn't the correct system password.");
      exit_init(2);
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
      edit_arc(0);
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
    case '$':
      nlx();
      out->window()->Printf("QSCan Lenth: %lu\n", syscfg.qscn_len);
      out->window()->Printf("WWIV %s%s INIT compiled %s\n", wwiv_version, beta_version, const_cast<char*>(wwiv_date));
      out->window()->GetChar();
      break;
    case 'X':
      up_subs_dirs();
      break;
    }
    out->SetIndicatorMode(IndicatorMode::NONE);
  } while (!done);

  // Don't leak the localIO (also fix the color when the app exits)
  delete out;
  out = nullptr;
  return 0;
}
