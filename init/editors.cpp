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
#include "platform/incl1.h"
#include "wwivinit.h"


void edit_editor(int n) {
  int i1;
  editorrec c;

  out->Cls();
  c = editors[n];
  bool done = false;
  int cp = 0;
  Printf("Description     : %s\n", c.description);
  Printf("Filename to run remotely\n%s\n", c.filename);
  Printf("Filename to run locally\n%s\n", c.filenamecon);
  textattr(COLOR_YELLOW);
  Puts("\n<ESC> when done.\n\n");
  textattr(11);
  Printf("%%1 = filename to edit\n");
  Printf("%%2 = chars per line\n");
  Printf("%%3 = lines per page\n");
  Printf("%%4 = max lines\n");
  Printf("%%5 = instance number\n");
  textattr(COLOR_CYAN);

  do {
    switch (cp) {
    case 0:
      out->GotoXY(18, 0);
      break;
    case 1:
      out->GotoXY(0, 2);
      break;
    case 2:
      out->GotoXY(0, 4);
      break;
    }
    switch (cp) {
    case 0:
      editline(c.description, 35, ALL, &i1, "");
      trimstr(c.description);
      break;
    case 1:
      editline(c.filename, 75, ALL, &i1, "");
      trimstr(c.filename);
      break;
    case 2:
      editline(c.filenamecon, 75, ALL, &i1, "");
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
      Printf("%d. %s\n", i + 1, editors[i].description);
    }
    nlx();
    textattr(COLOR_YELLOW);
    Puts("Editors: M:odify, D:elete, I:nsert, Q:uit : ");
    textattr(COLOR_CYAN);
    char ch = onek("Q\033MID");
    switch (ch) {
    case 'Q':
    case '\033':
      done = true;
      break;
    case 'M':
      if (initinfo.numeditors) {
        nlx();
        textattr(COLOR_RED);
        Printf("Edit which (1-%d) ? ", initinfo.numeditors);
        textattr(COLOR_CYAN);
        int i = input_number(2);
        if ((i > 0) && (i <= initinfo.numeditors)) {
          edit_editor(i - 1);
        }
      }
      break;
    case 'D':
      if (initinfo.numeditors) {
        nlx();
        textattr(COLOR_RED);
        Printf("Delete which (1-%d) ? ", initinfo.numeditors);
        textattr(COLOR_CYAN);
        int i = input_number(2);
        if ((i > 0) && (i <= initinfo.numeditors)) {
          for (i1 = i - 1; i1 < initinfo.numeditors; i1++) {
            editors[i1] = editors[i1 + 1];
          }
          --initinfo.numeditors;
        }
      }
      break;
    case 'I':
      if (initinfo.numeditors >= 10) {
        textattr(COLOR_RED);
        Printf("Too many editors.\n");
        textattr(COLOR_CYAN);
        nlx();
        break;
      }
      nlx();
      textattr(COLOR_YELLOW);
      Printf("Insert before which (1-%d) ? ", initinfo.numeditors + 1);
      textattr(COLOR_CYAN);
      int i = input_number(2);
      if ((i > 0) && (i <= initinfo.numeditors + 1)) {
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
  write(hFile, (void *)editors, initinfo.numeditors * sizeof(editorrec));
  close(hFile);
}
