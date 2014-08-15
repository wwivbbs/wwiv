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
#include "archivers.h"

#include <curses.h>
#include <cstdint>
#include <cstring>
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
#include "core/wwivport.h"
#include "bbs/wconstants.h" // for MAX_ARCHIVERS
#include "utility.h"
#include "wwivinit.h"


void edit_arc(int nn) {
  arcrec arc[MAX_ARCS];

  int i = nn;
  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%sarchiver.dat", syscfg.datadir);
  int hFile = _open(szFileName, O_RDWR | O_BINARY);
  if (hFile < 0) {
    create_arcs();
    hFile = _open(szFileName, O_RDWR | O_BINARY);
  }

  _read(hFile, &arc, MAX_ARCS * sizeof(arcrec));

  // display arcs and edit menu, get choice

  bool done1 = false;
  do {
    int cp = 4;
    done1 = false;
    out->Cls();
    out->SetColor(Scheme::PROMPT);
    Printf("                 Archiver Configuration\n\n");
    out->SetColor(Scheme::NORMAL);
    if (i == 0) {
      Printf("Archiver #%d  ", i + 1);
      out->SetColor(Scheme::PROMPT);
      Printf("(Default)\n\n");
    } else {
      Printf("Archiver #%d           \n\n", i + 1);
    }
    out->SetColor(Scheme::NORMAL);
    Printf("Archiver Name      : %s\n", arc[i].name);
    Printf("Archiver Extension : %s\n", arc[i].extension);
    Printf("List Archive       : %s\n", arc[i].arcl);
    Printf("Extract Archive    : %s\n", arc[i].arce);
    Printf("Add to Archive     : %s\n", arc[i].arca);
    Printf("Delete from Archive: %s\n", arc[i].arcd);
    Printf("Comment Archive    : %s\n", arc[i].arck);
    Printf("Test Archive       : %s\n", arc[i].arct);
    out->GotoXY(0, 13);
    Printf("                                                             \n");
    out->SetColor(Scheme::NORMAL);
    Printf("[ = Previous Archiver  ] = Next Archiver\n");
    out->SetColor(Scheme::NORMAL);
    Printf("                                                             \n");
    Printf("                                                             \n");
    out->SetColor(Scheme::PROMPT);
    Puts("<ENTER> to edit    <ESC> when done.");
    out->SetColor(Scheme::NORMAL);
    nlx();
    char ch = onek("\033[]\r");
    switch (ch) {
    case '\r': {
      out->GotoXY(0, 13);
      Printf("                                                             \n");
      Printf("%%1 %%2 etc. are parameters passed.  Minimum of two on Add and \n");
      Printf("Extract command lines. For added security, a complete path to\n");
      Printf("the archiver and extension should be used. i.e.:             \n");
      Printf("c:\\bin\\arcs\\zip.exe -a %%1 %%2                              \n");
      Printf("                                                             \n");
      out->SetColor(Scheme::PROMPT);
      Printf("<ESC> when done\n");
      out->SetColor(Scheme::NORMAL);
      bool done = false;
      do {
        int i1 = 0;
        out->GotoXY(21, cp);
        switch (cp) {
        case 4:
          editline(arc[i].name, 31, ALL, &i1, "");
          trimstr(arc[i].name);
          break;
        case 5:
          editline(arc[i].extension, 3, UPPER_ONLY, &i1, "");
          trimstr(arc[i].extension);
          break;
        case 6:
          editline(arc[i].arcl, 49, ALL, &i1, "");
          trimstr(arc[i].arcl);
          break;
        case 7:
          editline(arc[i].arce, 49, ALL, &i1, "");
          trimstr(arc[i].arce);
          break;
        case 8:
          editline(arc[i].arca, 49, ALL, &i1, "");
          trimstr(arc[i].arca);
          break;
        case 9:
          editline(arc[i].arcd, 49, ALL, &i1, "");
          trimstr(arc[i].arcd);
          break;
        case 10:
          editline(arc[i].arck, 49, ALL, &i1, "");
          trimstr(arc[i].arck);
          break;
        case 11:
          editline(arc[i].arct, 49, ALL, &i1, "");
          trimstr(arc[i].arct);
          break;
        }
        cp = GetNextSelectionPosition(4, 11, cp, i1);
        if (i1 == DONE) {
          done = true;
        }
      } while (!done);
    }
    break;
    case '\033': {
      done1 = true;
    }
    break;
    case ']':
      i++;
      if (i > MAX_ARCS - 1) {
        i = 0;
      }
      break;
    case '[':
      i--;
      if (i < 0) {
        i = MAX_ARCS - 1;
      }
      break;
    }
  } while (!done1);

  // copy first four new fomat archivers to oldarcsrec

  for (int j = 0; j < 4; j++) {
    strncpy(syscfg.arcs[j].extension, arc[j].extension, 4);
    strncpy(syscfg.arcs[j].arca     , arc[j].arca     , 32);
    strncpy(syscfg.arcs[j].arce     , arc[j].arce     , 32);
    strncpy(syscfg.arcs[j].arcl     , arc[j].arcl     , 32);
  }

  // seek to beginning of file, write arcrecs, close file

  _lseek(hFile, 0L, SEEK_SET);
  _write(hFile, arc, MAX_ARCS * sizeof(arcrec));
  _close(hFile);
}

void create_arcs() {
  arcrec arc[MAX_ARCS];

  strncpy(arc[0].name, "Zip", 32);
  strncpy(arc[0].extension, "ZIP", 4);
  strncpy(arc[0].arca, "zip %1 %2", 50);
  strncpy(arc[0].arce, "unzip -C %1 %2", 50);
  strncpy(arc[0].arcl, "unzip -l %1", 50);
  strncpy(arc[0].arcd, "zip -d %1 -@ < BBSADS.TXT", 50);
  strncpy(arc[0].arck, "zip -z %1 < COMMENT.TXT", 50);
  strncpy(arc[0].arct, "unzip -t %1", 50);
  strncpy(arc[1].name, "ARJ", 32);
  strncpy(arc[1].extension, "ARJ", 4);
  strncpy(arc[1].arca, "arj.exe a %1 %2", 50);
  strncpy(arc[1].arce, "arj.exe e %1 %2", 50);
  strncpy(arc[1].arcl, "arj.exe i %1", 50);
  strncpy(arc[1].arcd, "arj.exe d %1 !BBSADS.TXT", 50);
  strncpy(arc[1].arck, "arj.exe c %1 -zCOMMENT.TXT", 50);
  strncpy(arc[1].arct, "arj.exe t %1", 50);
  strncpy(arc[2].name, "RAR", 32);
  strncpy(arc[2].extension, "RAR", 4);
  strncpy(arc[2].arca, "rar.exe a -std -s- %1 %2", 50);
  strncpy(arc[2].arce, "rar.exe e -std -o+ %1 %2", 50);
  strncpy(arc[2].arcl, "rar.exe l -std %1", 50);
  strncpy(arc[2].arcd, "rar.exe d -std %1 < BBSADS.TXT ", 50);
  strncpy(arc[2].arck, "rar.exe zCOMMENT.TXT -std %1", 50);
  strncpy(arc[2].arct, "rar.exe t -std %1", 50);
  strncpy(arc[3].name, "LZH", 32);
  strncpy(arc[3].extension, "LZH", 4);
  strncpy(arc[3].arca, "lha.exe a %1 %2", 50);
  strncpy(arc[3].arce, "lha.exe eo  %1 %2", 50);
  strncpy(arc[3].arcl, "lha.exe l %1", 50);
  strncpy(arc[3].arcd, "lha.exe d %1  < BBSADS.TXT", 50);
  strncpy(arc[3].arck, "", 50);
  strncpy(arc[3].arct, "lha.exe t %1", 50);
  strncpy(arc[4].name, "PAK", 32);
  strncpy(arc[4].extension, "PAK", 4);
  strncpy(arc[4].arca, "pkpak.exe -a %1 %2", 50);
  strncpy(arc[4].arce, "pkunpak.exe -e  %1 %2", 50);
  strncpy(arc[4].arcl, "pkunpak.exe -p %1", 50);
  strncpy(arc[4].arcd, "pkpak.exe -d %1  @BBSADS.TXT", 50);
  strncpy(arc[4].arck, "pkpak.exe -c %1 < COMMENT.TXT ", 50);
  strncpy(arc[4].arct, "pkunpak.exe -t %1", 50);


  for (int i = 5; i < MAX_ARCS; i++) {
    strncpy(arc[i].name, "New Archiver Name", 32);
    strncpy(arc[i].extension, "EXT", 4);
    strncpy(arc[i].arca, "archive add command", 50);
    strncpy(arc[i].arce, "archive extract command", 50);
    strncpy(arc[i].arcl, "archive list command", 50);
    strncpy(arc[i].arcd, "archive delete command", 50);
    strncpy(arc[i].arck, "archive comment command", 50);
    strncpy(arc[i].arct, "archive test command", 50);
  }

  char szFileName[MAX_PATH];
  sprintf(szFileName, "%sarchiver.dat", syscfg.datadir);
  int hFile = _open(szFileName, O_WRONLY | O_BINARY | O_CREAT | O_EXCL, S_IREAD | S_IWRITE);
  if (hFile < 0) {
    Printf("Couldn't open '%s' for writing.\n", szFileName);
    exit_init(1);
  }
  _write(hFile, arc, MAX_ARCS * sizeof(arcrec));
  _close(hFile);
}

