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
#include "core/strings.h"
#include "core/wwivport.h"
#include "utility.h"
#include "wwivinit.h"


static void edit_lang(CursesWindow* window, int nn) {
  int i1;
  languagerec *n;

  out->Cls();
  bool done = false;
  int cp = 0;
  n = &(languages[nn]);
  window->SetColor(SchemeId::NORMAL);

  window->Printf("Language name  : %s\n", n->name);
  window->Printf("Data Directory : %s\n", n->dir);
  window->Printf("Menu Directory : %s\n", n->mdir);
  window->SetColor(SchemeId::PROMPT);
  window->Puts("\n<ESC> when done.\n\n");
  window->SetColor(SchemeId::NORMAL);
  do {
    window->GotoXY(17, cp);
    switch (cp) {
    case 0:
      editline(window, n->name, sizeof(n->name) - 1, ALL, &i1, "");
      StringTrimEnd(n->name);
#ifdef WHY
      ss = strchr(n->name, ' ');
      if (ss) {
        *ss = 0;
      }
#endif
      window->Puts(n->name);
       window->Puts("                  ");
      break;
    case 1:
      editline(window, n->dir, 60, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(n->dir);
      window->Puts(n->dir);
      break;
    case 2:
      editline(window, n->mdir, 60, EDITLINE_FILENAME_CASE, &i1, "");
      trimstrpath(n->mdir);
      window->Puts(n->mdir);
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

  CursesWindow* window(out->window());

  bool done = false;
  do {
    out->Cls();
    nlx();
    for (i = 0; i < initinfo.num_languages; i++) {
      if (i && ((i % 23) == 0)) {
        pausescr(window);
      }
      window->Printf("%-2d. %-20s    %-50s\n", i + 1, languages[i].name, languages[i].dir);
    }
    nlx();
    window->SetColor(SchemeId::PROMPT);
    window->Puts("Languages: M:odify, D:elete, I:nsert, Q:uit : ");
    window->SetColor(SchemeId::NORMAL);
    char ch = onek(window, "Q\033MID");
    switch (ch) {
    case 'Q':
    case '\033':
      done = true;
      break;
    case 'M':
      nlx();
      window->SetColor(SchemeId::PROMPT);
      sprintf(s1, "Edit which (1-%d) ? ", initinfo.num_languages);
      window->Puts(s1);
      window->SetColor(SchemeId::NORMAL);
      i = input_number(window, 2);
      if ((i > 0) && (i <= initinfo.num_languages)) {
        edit_lang(window, i - 1);
      }
      break;
    case 'D':
      if (initinfo.num_languages > 1) {
        nlx();
        sprintf(s1, "Delete which (1-%d) ? ", initinfo.num_languages);
        window->SetColor(SchemeId::PROMPT);
        window->Puts(s1);
        window->SetColor(SchemeId::NORMAL);
        i = input_number(window, 2);
        if ((i > 0) && (i <= initinfo.num_languages)) {
          nlx();
          window->SetColor(SchemeId::ERROR_TEXT);
          window->Puts("Are you sure? ");
          window->SetColor(SchemeId::NORMAL);
          ch = onek(window, "YN\r");
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
        window->SetColor(SchemeId::ERROR_TEXT);
        window->Printf("You must leave at least one language.\n");
        window->SetColor(SchemeId::NORMAL);
        nlx();
        window->GetChar();
      }
      break;
    case 'I':
      if (initinfo.num_languages >= MAX_LANGUAGES) {
        window->SetColor(SchemeId::ERROR_TEXT);
        window->Printf("Too many languages.\n");
        window->SetColor(SchemeId::NORMAL);
        nlx();
        window->GetChar();
        break;
      }
      nlx();
      window->SetColor(SchemeId::PROMPT);
      sprintf(s1, "Insert before which (1-%d) ? ", initinfo.num_languages + 1);
      window->Puts(s1);
      window->SetColor(SchemeId::NORMAL);
      i = input_number(window, 2);
      if ((i > 0) && (i <= initinfo.num_languages + 1)) {
        window->SetColor(SchemeId::ERROR_TEXT);
        window->Puts("Are you sure? ");
        window->SetColor(SchemeId::NORMAL);
        ch = onek(window, "YN\r");
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
          edit_lang(window, i);
        }
      }
      break;
    }
  } while (!done);

  char szFileName[ MAX_PATH ];
  sprintf(szFileName, "%slanguage.dat", syscfg.datadir);
  unlink(szFileName);
  int hFile = open(szFileName, O_RDWR | O_BINARY | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
  write(hFile, languages, initinfo.num_languages * sizeof(languagerec));
  close(hFile);
}

