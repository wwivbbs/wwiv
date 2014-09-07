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
#include <memory>
#include <string>
#include <sys/stat.h>
#include <vector>

#include "ifcns.h"
#include "init.h"
#include "input.h"
#include "core/strings.h"
#include "core/wwivport.h"
#include "utility.h"
#include "wwivinit.h"

using std::auto_ptr;
using std::string;
using std::vector;
using wwiv::strings::StringPrintf;

void edit_editor(int n) {
  out->Cls(ACS_CKBOARD);
  auto_ptr<CursesWindow> window(out->CreateBoxedWindow("External Editor Configuration", 15, 78));

  const vector<string> bbs_types = { "WWIV    ", "QuickBBS" };
  const int COL1_POSITION = 20;
  editorrec c = editors[n];

  EditItems items{
    new StringEditItem<char*>(COL1_POSITION, 1, 35, c.description, false),
    new ToggleEditItem<uint8_t>(COL1_POSITION, 2, bbs_types, &c.bbs_type),
    new StringEditItem<char*>(2, 4, 75, c.filename, false),
    new StringEditItem<char*>(2, 7, 75, c.filenamecon, false)
  };
  items.set_curses_io(out, window.get());

  int y = 1;
  window->PrintfXY(2, y++, "Description     : %s", c.description);
  window->PrintfXY(2, y++, "BBS Type        : %s", bbs_types.at(c.bbs_type).c_str());
  window->PrintfXY(2, y++, "Filename to run remotely:", c.filename);
  y+=2;
  window->PrintfXY(2, y++, "Filename to run locally:", c.filenamecon);
  y+=2;
  window->PrintfXY(2, y++, "%%1 = filename to edit");
  window->PrintfXY(2, y++, "%%2 = chars per line");
  window->PrintfXY(2, y++, "%%3 = lines per page");
  window->PrintfXY(2, y++, "%%4 = max lines");
  window->PrintfXY(2, y++, "%%5 = instance number");
  items.Run();

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
