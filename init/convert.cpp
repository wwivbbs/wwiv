/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "init/convert.h"

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
#include <sys/stat.h>

#include "bbs/wconstants.h"
#include "core/strings.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/version.h"
#include "core/wwivport.h"
#include "init/archivers.h"
#include "init/init.h"
#include "init/wwivinit.h"
#include "localui/input.h"
#include "localui/curses_io.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using std::string;
using std::vector;
using namespace wwiv::core;
using namespace wwiv::strings;

#define CONFIG_USR "config.usr"

#define cfl_fname                   0x00000001
#define cfl_extension               0x00000002
#define cfl_dloads                  0x00000004
#define cfl_kbytes                  0x00000008
#define cfl_date_uploaded           0x00000010
#define cfl_file_points             0x00000020
#define cfl_days_old                0x00000040
#define cfl_upby                    0x00000080
#define cfl_times_a_day_dloaded     0x00000100
#define cfl_days_between_dloads     0x00000200
#define cfl_description             0x00000400
#define cfl_header                  0x80000000L

// TODO(rushfan): use wwivcolors.h and move it to sdk.
enum COLORS {
  BLACK,
  BLUE,
  GREEN,
  CYAN,
  RED,
  MAGENTA,
  BROWN,
  LIGHTGRAY,
  DARKGRAY,
  LIGHTBLUE,
  LIGHTGREEN,
  LIGHTCYAN,
  LIGHTRED,
  LIGHTMAGENTA,
  YELLOW,
  WHITE
};

struct user_config {
  char name[31];          // verify against a user

  unsigned long unused_status;

  unsigned long lp_options;
  unsigned char lp_colors[32];

  char szMenuSet[9];   // Selected AMENU set to use
  char cHotKeys;       // Use hot keys in AMENU

  char junk[119];   // AMENU took 11 bytes from here
};

static void ShowBanner(CursesWindow* window, const std::string& m) {
  // TODO(rushfan): make a subwindow here but until this clear the altcharset background.
  out->window()->Bkgd(' ');
  window->SetColor(SchemeId::INFO);
  window->Printf("%s\n", m.c_str());
  window->SetColor(SchemeId::NORMAL);
}

bool convert_config_to_52(CursesWindow* window, const string& config_filename) {
  File file(config_filename);
  if (!file.Open(File::modeBinary | File::modeReadWrite)) {
    return false;
  }

  ShowBanner(window, "Converting config.dat to 4.3/5.x format...");
  file.Read(&syscfg, sizeof(configrec));

  configrec_header_t h = {};
  h.config_revision_number = 0;
  h.config_size = sizeof(configrec);
  h.written_by_wwiv_num_version = wwiv_num_version;
  strcpy(h.signature, "WWIV");
  h.signature[4] = 0;

  // Save old newuser password.
  string newuserpw = syscfg.header.newuserpw;
  // Update newuser password to new location.
  to_char_array(syscfg.newuserpw, newuserpw);
  // Set new header on config.dat.
  syscfg.header.header = h;

  // Write it all back.
  file.Seek(0, File::seekBegin);
  file.Write(&syscfg, sizeof(configrec));
  file.Close();
  return true;
}

static bool convert_to_52_1(CursesWindow* window, const std::string& config_filename) {
  ShowBanner(window, "Updating to latest 5.2 format...");

  string users_lst = StrCat(syscfg.datadir, USER_LST);
  string backup_file = StrCat(users_lst, ".backup.pre-init-upgrade");

  // Make a backup file.
  File::Copy(users_lst, backup_file);

  DataFile<userrec> usersFile(syscfg.datadir, USER_LST,
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
    memset(u.szMenuSet, 0, sizeof(u.szMenuSet));

    // Set new defaults.
    memset(u.lp_colors, CYAN, sizeof(u.lp_colors));
    u.lp_colors[0] = LIGHTGREEN;
    u.lp_colors[1] = LIGHTGREEN;
    u.lp_colors[2] = CYAN;
    u.lp_colors[3] = CYAN;
    u.lp_colors[4] = LIGHTCYAN;
    u.lp_colors[5] = LIGHTCYAN;
    u.lp_colors[6] = CYAN;
    u.lp_colors[7] = CYAN;
    u.lp_colors[8] = CYAN;
    u.lp_colors[9] = CYAN;
    u.lp_colors[10] = LIGHTCYAN;
    u.lp_options = cfl_fname | cfl_extension | cfl_dloads | cfl_kbytes | cfl_description;
    u.cHotKeys = 0;
    strcpy(u.szMenuSet, "wwiv");
  }

  // Save where we are.
  usersFile.Seek(0);
  usersFile.WriteVector(users);

  // Update config.dat with new version to consider this "successful"
  // enough of an upgrade at this point.
  {
    File file(config_filename);
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

    file.Seek(0, File::seekBegin);
    file.Write(&syscfg, sizeof(configrec));
    file.Close();
  }

  DataFile<user_config> configUsrFile(syscfg.datadir, "config.usr",
    File::modeReadOnly | File::modeBinary, File::shareDenyWrite);
  if (!configUsrFile) {
    messagebox(window, "Unable to read CONFIG_USR.");
    return false;
  }

  vector<user_config> second_config;
  if (!configUsrFile.ReadVector(second_config)) {
    messagebox(window, "Unable to read CONFIG_USR.");
    return false;
  }

  // merge in data from user_config
  for (size_t i = 0; i < users.size(); i++) {
    auto& u = users.at(i);
    if (i >= second_config.size()) {
      continue;
    }
    const auto& c = second_config.at(i);
    u.cHotKeys = c.cHotKeys;
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

  // 2nd version of config.usr that INIT was mistankenly creating.
  File userDatFile(syscfg.datadir, "user.dat");
  userDatFile.Delete();

  messagebox(window, "Converted to config version #1");
  return true;
}

bool ensure_latest_5x_config(CursesWindow* window, const std::string& config_filename) {
  const auto v = syscfg.header.header.config_revision_number;
  if (v < 1) {
    if (!convert_to_52_1(window, config_filename)) {
      return false;
    }
  }

  // add others

  return true;
}

void convert_config_424_to_430(CursesWindow* window, const string& config_filename) {
  File file(config_filename);
  if (!file.Open(File::modeBinary|File::modeReadWrite)) {
    return;
  }
  window->SetColor(SchemeId::INFO);
  window->Printf("Converting config.dat to 4.3/5.x format...\n");
  window->SetColor(SchemeId::NORMAL);
  file.Read(&syscfg, sizeof(configrec));
  sprintf(syscfg.menudir, "%smenus%c", syscfg.gfilesdir, File::pathSeparatorChar);

  arcrec arc[MAX_ARCS];
  for (int i = 0; i < MAX_ARCS; i++) {
    if (syscfg.arcs[i].extension[0] && i < 4) {
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
  file.Seek(0, File::seekBegin);
  file.Write(&syscfg, sizeof(configrec));
  file.Close();

  File archiver(syscfg.datadir, ARCHIVER_DAT);
  if (!archiver.Open(File::modeBinary|File::modeWriteOnly|File::modeCreateFile)) {
    window->Printf("Couldn't open 'ARCHIVER_DAT' for writing.\n");
    window->Printf("Creating new file....\n");
    create_arcs(window);
    window->Printf("\n");
    if (!archiver.Open(File::modeBinary|File::modeWriteOnly|File::modeCreateFile)) {
      messagebox(window, "Still unable to open archiver.dat. Something is really wrong.");
      return;
    }
  }
  archiver.Write(arc, MAX_ARCS * sizeof(arcrec));
  archiver.Close();
}
