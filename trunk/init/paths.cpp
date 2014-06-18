/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                 Copyright (C)2014, WWIV Software Services              */
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
#include "template.h"

#include <curses.h>
#include <cstdint>
#include <string>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "wwivinit.h"

/* change msgsdir, gfilesdir, datadir, dloadsdir, ramdrive, tempdir */
void setpaths() {
  out->Cls();
  textattr(COLOR_CYAN);
  Printf("Messages Directory : %s\n", syscfg.msgsdir);
  Printf("GFiles Directory   : %s\n", syscfg.gfilesdir);
  Printf("Menu Directory     : %s\n", syscfg.menudir);
  Printf("Data Directory     : %s\n", syscfg.datadir);
  Printf("Downloads Directory: %s\n", syscfg.dloadsdir);

  textattr(COLOR_MAGENTA);
  nlx(2);
  Printf("CAUTION: ONLY EXPERIENCED SYSOPS SHOULD MODIFY THESE SETTINGS.\n\n");
  textattr(COLOR_YELLOW);
  Printf(" Changing any of these requires YOU to MANUALLY move files and / or \n");
  Printf(" directory structures.  Consult the documentation prior to changing \n");
  Printf(" any of these settings.\n");
  textattr(COLOR_CYAN);

  int i1;
  int cp = 0;
  bool done = false;
  do {
    done = false;
    switch (cp) {
    case 0:
      out->GotoXY(21, cp);
      editline(syscfg.msgsdir, 50, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(syscfg.msgsdir);
      Puts(syscfg.msgsdir);
      //          verify_dir("Messages", syscfg.msgsdir);
      break;
    case 1:
      out->GotoXY(21, cp);
      editline(syscfg.gfilesdir, 50, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(syscfg.gfilesdir);
      Puts(syscfg.gfilesdir);
      //          verify_dir("Gfiles", syscfg.gfilesdir);
      break;
    case 2:
      out->GotoXY(21, cp);
      editline(syscfg.menudir, 50, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(syscfg.menudir);
      Puts(syscfg.menudir);
      //          verify_dir("Menu", syscfg.menudir);
      break;
    case 3:
      out->GotoXY(21, cp);
      editline(syscfg.datadir, 50, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(syscfg.datadir);
      Puts(syscfg.datadir);
      //          verify_dir("Data", syscfg.datadir);
      break;
    case 4:
      out->GotoXY(21, cp);
      editline(syscfg.dloadsdir, 50, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(syscfg.dloadsdir);
      Puts(syscfg.dloadsdir);
      //          verify_dir("Downloads", syscfg.dloadsdir);
      break;
    }
    cp = GetNextSelectionPosition(0, 4, cp, i1);
    if (i1 == DONE) {
      done = true;
    }
  } while (!done);

  save_config();
}

