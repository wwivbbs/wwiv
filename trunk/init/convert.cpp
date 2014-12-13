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
#include "core/file.h"
#include "core/wwivport.h"
#include "init/archivers.h"
#include "init/ifcns.h"
#include "init/init.h"
#include "init/wwivinit.h"
#include "initlib/input.h"
#include "initlib/curses_io.h"
#include "sdk/filenames.h"
#include "sdk/vardec.h"

using std::string;
using std::vector;
using namespace wwiv::strings;

void convcfg(CursesWindow* window, const string& config_filename) {
  File file(config_filename);
  if (!file.Open(File::modeBinary|File::modeReadWrite)) {
    return;
  }
  window->SetColor(SchemeId::INFO);
  window->Printf("Converting config.dat to 4.30/5.00 format...\n");
  window->SetColor(SchemeId::NORMAL);
  file.Read(&syscfg, sizeof(configrec));
  sprintf(syscfg.menudir, "%smenus%c", syscfg.gfilesdir, File::pathSeparatorChar);
  strcpy(syscfg.unused_logoff_c, " ");
  strcpy(syscfg.unused_v_scan_c, " ");

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
