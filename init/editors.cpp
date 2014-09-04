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
#include "editors.h"

#include <curses.h>
#include <cstdint>
#include <fcntl.h>
#ifdef _WIN32
#include <direct.h>
#include <io.h>
#endif
#include <string>
#include <sys/stat.h>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "utility.h"
#include "wwivinit.h"

using std::string;
using wwiv::strings::StringPrintf;

void edit_editor(int n) {
  int i1;
  editorrec c;

  out->Cls();
  c = editors[n];
  bool done = false;
  int cp = 0;
  out->window()->Printf("Description     : %s\n", c.description);
  out->window()->Printf("Filename to run remotely\n%s\n", c.filename);
  out->window()->Printf("Filename to run locally\n%s\n", c.filenamecon);
  out->SetColor(SchemeId::PROMPT);
  out->window()->Puts("\n<ESC> when done.\n\n");
  out->SetColor(SchemeId::NORMAL);
  out->window()->Printf("%%1 = filename to edit\n");
  out->window()->Printf("%%2 = chars per line\n");
  out->window()->Printf("%%3 = lines per page\n");
  out->window()->Printf("%%4 = max lines\n");
  out->window()->Printf("%%5 = instance number\n");
  out->SetColor(SchemeId::NORMAL);

  do {
    switch (cp) {
    case 0:
      out->window()->GotoXY(18, 0);
      break;
    case 1:
      out->window()->GotoXY(0, 2);
      break;
    case 2:
      out->window()->GotoXY(0, 4);
      break;
    }
    switch (cp) {
    case 0:
      editline(out->window(), c.description, 35, ALL, &i1, "");
      trimstr(c.description);
      break;
    case 1:
      editline(out->window(), c.filename, 75, ALL, &i1, "");
      trimstr(c.filename);
      break;
    case 2:
      editline(out->window(), c.filenamecon, 75, ALL, &i1, "");
      trimstr(c.filenamecon);
      break;
    }
    cp = GetNextSelectionPosition(0, 2, cp, i1);
    if (i1 == DONE) {
      done = true;
    }
  } while (!done);
  editors[n] = c;
}

void extrn_editors() {
  int i1;

  bool done = false;
  do {
    out->Cls();
    nlx();
    for (int i = 0; i < initinfo.numeditors; i++) {
      out->window()->Printf("%d. %s\n", i + 1, editors[i].description);
    }
    nlx();
    out->SetColor(SchemeId::PROMPT);
    out->window()->Puts("Editors: M:odify, D:elete, I:nsert, Q:uit : ");
    out->SetColor(SchemeId::NORMAL);
    char ch = onek(out->window(), "Q\033MID");
    switch (ch) {
    case 'Q':
    case '\033':
      done = true;
      break;
    case 'M':
      if (initinfo.numeditors) {
        string prompt = StringPrintf("Edit which (1-%d) : ", initinfo.numeditors);
        int i = dialog_input_number(out->window(), prompt, 1, initinfo.numeditors);
        if (i > 0 && i <= initinfo.numeditors) {
          edit_editor(i - 1);
        }
      }
      break;
    case 'D':
      if (initinfo.numeditors) {
        string prompt = StringPrintf("Delete which (1-%d) : ", initinfo.numeditors);
        int i = dialog_input_number(out->window(), prompt, 1, initinfo.numeditors);
        if (i > 0 && i <= initinfo.numeditors) {
          for (i1 = i - 1; i1 < initinfo.numeditors; i1++) {
            editors[i1] = editors[i1 + 1];
          }
          --initinfo.numeditors;
        }
      }
      break;
    case 'I':
      if (initinfo.numeditors >= 10) {
        messagebox(out->window(), "Too many editors.");
        break;
      }
      string prompt = StringPrintf("Insert before which (1-%d) : ", initinfo.numeditors + 1);
      int i = dialog_input_number(out->window(), prompt, 1, initinfo.numeditors);
      if (i > 0 && i <= initinfo.numeditors + 1) {
        for (i1 = initinfo.numeditors; i1 > i - 1; i1--) {
          editors[i1] = editors[i1 - 1];
        }
        ++initinfo.numeditors;
        editors[i - 1].description[0] = 0;
        editors[i - 1].filenamecon[0] = 0;
        editors[i - 1].filename[0] = 0;
        edit_editor(i - 1);
      }
      break;
    }
  } while (!done);

  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%seditors.dat", syscfg.datadir);
  int hFile = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(hFile, editors, initinfo.numeditors * sizeof(editorrec));
  close(hFile);
}
