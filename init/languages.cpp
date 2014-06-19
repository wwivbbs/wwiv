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
#include "languages.h"

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
#include "platform/incl1.h"
#include "wwivinit.h"


static void edit_lang(int nn) {
  int i1;
  languagerec *n;

  out->Cls();
  bool done = false;
  int cp = 0;
  n = &(languages[nn]);
  Printf("Language name  : %s\n", n->name);
  Printf("Data Directory : %s\n", n->dir);
  Printf("Menu Directory : %s\n", n->mdir);
  textattr(COLOR_YELLOW);
  Puts("\n<ESC> when done.\n\n");
  textattr(COLOR_CYAN);
  do {
    out->GotoXY(17, cp);
    switch (cp) {
    case 0:
      editline(n->name, sizeof(n->name) - 1, ALL, &i1, "");
      trimstr(n->name);
#ifdef WHY
      ss = strchr(n->name, ' ');
      if (ss) {
        *ss = 0;
      }
#endif
      Puts(n->name);
      Puts("                  ");
      break;
    case 1:
      editline(n->dir, 60, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(n->dir);
      Puts(n->dir);
      break;
    case 2:
      editline(n->mdir, 60, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(n->mdir);
      Puts(n->mdir);
      break;
    }
    cp = GetNextSelectionPosition(0, 2, cp, i1);
    if (i1 == DONE) {
      done = true;
    }
  } while (!done);
}

void edit_languages() {
  char s1[81];
  int i, i1, i2;

  bool done = false;
  do {
    out->Cls();
    nlx();
    for (i = 0; i < initinfo.num_languages; i++) {
      if (i && ((i % 23) == 0)) {
        pausescr();
      }
      Printf("%-2d. %-20s    %-50s\n", i + 1, languages[i].name, languages[i].dir);
    }
    nlx();
    textattr(COLOR_YELLOW);
    Puts("Languages: M:odify, D:elete, I:nsert, Q:uit : ");
    textattr(COLOR_CYAN);
    char ch = onek("Q\033MID");
    switch (ch) {
    case 'Q':
    case '\033':
      done = true;
      break;
    case 'M':
      nlx();
      textattr(COLOR_YELLOW);
      sprintf(s1, "Edit which (1-%d) ? ", initinfo.num_languages);
      Puts(s1);
      textattr(COLOR_CYAN);
      i = input_number(2);
      if ((i > 0) && (i <= initinfo.num_languages)) {
        edit_lang(i - 1);
      }
      break;
    case 'D':
      if (initinfo.num_languages > 1) {
        nlx();
        sprintf(s1, "Delete which (1-%d) ? ", initinfo.num_languages);
        textattr(COLOR_YELLOW);
        Puts(s1);
        textattr(COLOR_CYAN);
        i = input_number(2);
        if ((i > 0) && (i <= initinfo.num_languages)) {
          nlx();
          textattr(COLOR_RED);
          Puts("Are you sure? ");
          textattr(COLOR_CYAN);
          ch = onek("YN\r");
          if (ch == 'Y') {
            initinfo.num_languages--;
            for (i1 = i - 1; i1 < initinfo.num_languages; i1++) {
              languages[i1] = languages[i1 + 1];
            }
            if (initinfo.num_languages == 1) {
              languages[0].num = 0;
            }
          }
        }
      } else {
        nlx();
        textattr(COLOR_RED);
        Printf("You must leave at least one language.\n");
        textattr(COLOR_CYAN);
        nlx();
        out->GetChar();
      }
      break;
    case 'I':
      if (initinfo.num_languages >= MAX_LANGUAGES) {
        textattr(COLOR_RED);
        Printf("Too many languages.\n");
        textattr(COLOR_CYAN);
        nlx();
        out->GetChar();
        break;
      }
      nlx();
      textattr(COLOR_YELLOW);
      sprintf(s1, "Insert before which (1-%d) ? ", initinfo.num_languages + 1);
      Puts(s1);
      textattr(COLOR_CYAN);
      i = input_number(2);
      if ((i > 0) && (i <= initinfo.num_languages + 1)) {
        textattr(COLOR_RED);
        Puts("Are you sure? ");
        textattr(COLOR_CYAN);
        ch = onek("YN\r");
        if (ch == 'Y') {
          --i;
          for (i1 = initinfo.num_languages; i1 > i; i1--) {
            languages[i1] = languages[i1 - 1];
          }
          initinfo.num_languages++;
          strcpy(languages[i].name, "English");
          strncpy(languages[i].dir, syscfg.gfilesdir, sizeof(languages[i1].dir) - 1);
          strncpy(languages[i].mdir, syscfg.gfilesdir, sizeof(languages[i1].mdir) - 1);
          i2 = 0;
          for (i1 = 0; i1 < initinfo.num_languages; i1++) {
            if ((i != i1) && (languages[i1].num >= i2)) {
              i2 = languages[i1].num + 1;
            }
          }
          if (i2 >= 250) {
            for (i2 = 0; i2 < 255; i2++) {
              for (i1 = 0; i1 < initinfo.num_languages; i1++) {
                if ((i != i1) && (languages[i1].num == i2)) {
                  break;
                }
              }
              if (i1 >= initinfo.num_languages) {
                break;
              }
            }
          }
          languages[i].num = i2;
          edit_lang(i);
        }
      }
      break;
    }
  } while (!done);

  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%slanguage.dat", syscfg.datadir);
  unlink(szFileName);
  int hFile = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(hFile, (void *)languages, initinfo.num_languages * sizeof(languagerec));
  close(hFile);
}

