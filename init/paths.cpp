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
#include "paths.h"

#include <curses.h>
#include <cstdint>
#include <string>
#ifdef _WIN32
#include <direct.h>
#endif

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "core/wwivport.h"
#include "core/wfndfile.h"
#include "utility.h"
#include "wwivinit.h"

extern char bbsdir[];

static int verify_dir(char *typeDir, char *dirName) {
  int rc = 0;
  char s[81], ch;

  WFindFile fnd;
  fnd.open(dirName, 0);

  if (fnd.next() && fnd.IsDirectory()) {
    out->window()->GotoXY(0, 8);
    sprintf(s, "The %s directory: %s is invalid!", typeDir, dirName);
    out->SetColor(SchemeId::ERROR_TEXT);
    out->window()->Puts(s);
    for (unsigned int i = 0; i < strlen(s); i++) {
      out->window()->Printf("\b \b");
    }
    if ((strcmp(typeDir, "Temporary") == 0) || (strcmp(typeDir, "Batch") == 0)) {
      sprintf(s, "Create %s? ", dirName);
      out->SetColor(SchemeId::PROMPT);
      out->window()->Puts(s);
      ch = out->window()->GetChar();
      if (toupper(ch) == 'Y') {
        mkdir(dirName);
      }
      for (unsigned int j = 0; j < strlen(s); j++) {
        out->window()->Printf("\b \b");
      }
    }
    out->SetColor(SchemeId::PROMPT);
    out->window()->Puts("<ESC> when done.");
    rc = 1;
  }
  chdir(bbsdir);
  return rc;
}

/* change msgsdir, gfilesdir, datadir, dloadsdir, ramdrive, tempdir */
void setpaths() {
  out->Cls();
  out->SetColor(SchemeId::NORMAL);
  out->window()->Printf("Messages Directory : %s\n", syscfg.msgsdir);
  out->window()->Printf("GFiles Directory   : %s\n", syscfg.gfilesdir);
  out->window()->Printf("Menu Directory     : %s\n", syscfg.menudir);
  out->window()->Printf("Data Directory     : %s\n", syscfg.datadir);
  out->window()->Printf("Downloads Directory: %s\n", syscfg.dloadsdir);

  nlx(2);
  out->SetColor(SchemeId::WARNING);
  out->window()->Printf("CAUTION: ONLY EXPERIENCED SYSOPS SHOULD MODIFY THESE SETTINGS.\n\n");
  out->SetColor(SchemeId::PROMPT);
  out->window()->Printf(" Changing any of these requires YOU to MANUALLY move files and / or \n");
  out->window()->Printf(" directory structures.  Consult the documentation prior to changing \n");
  out->window()->Printf(" any of these settings.\n");
  out->SetColor(SchemeId::NORMAL);

  int i1;
  int cp = 0;
  bool done = false;
  do {
    done = false;
    switch (cp) {
    case 0:
      out->window()->GotoXY(21, cp);
      editline(out->window(), syscfg.msgsdir, 50, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(syscfg.msgsdir);
      out->window()->Puts(syscfg.msgsdir);
      //          verify_dir("Messages", syscfg.msgsdir);
      break;
    case 1:
      out->window()->GotoXY(21, cp);
      editline(out->window(), syscfg.gfilesdir, 50, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(syscfg.gfilesdir);
      out->window()->Puts(syscfg.gfilesdir);
      //          verify_dir("Gfiles", syscfg.gfilesdir);
      break;
    case 2:
      out->window()->GotoXY(21, cp);
      editline(out->window(), syscfg.menudir, 50, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(syscfg.menudir);
      out->window()->Puts(syscfg.menudir);
      //          verify_dir("Menu", syscfg.menudir);
      break;
    case 3:
      out->window()->GotoXY(21, cp);
      editline(out->window(), syscfg.datadir, 50, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(syscfg.datadir);
      out->window()->Puts(syscfg.datadir);
      //          verify_dir("Data", syscfg.datadir);
      break;
    case 4:
      out->window()->GotoXY(21, cp);
      editline(out->window(), syscfg.dloadsdir, 50, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(syscfg.dloadsdir);
      out->window()->Puts(syscfg.dloadsdir);
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

