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
#include "input.h"

#include <algorithm>
#include <cctype>
#include <curses.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "initlib/curses_io.h"
#include "bbs/wconstants.h"  // TODO(rushfan): Stop including this

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT

#define PREV                1
#define NEXT                2
#define DONE                4
#define ABORTED             8

#define NUM_ONLY            1
#define UPPER_ONLY          2
#define ALL                 4
#define SET                   8

// local functions.
void winput_password(CursesWindow* dialog, char *pszOutText, int nMaxLength);

using std::string;
using std::vector;

int CustomEditItem::Run(CursesWindow* window) {
  out->window()->GotoXY(x_, y_);
  string s = to_field_();

  int return_code = 0;
  editline(&s, maxsize_, ALL, &return_code, "");
  from_field_(s);
  return return_code;
}

void CustomEditItem::Display(CursesWindow* window) const {
  out->window()->GotoXY(x_, y_);
  string blanks(maxsize_, ' ');
  Puts(blanks.c_str());

  string s = to_field_();
  if (display_) {
    display_(s);
  } else {
    out->window()->PutsXY(x_, y_, s.c_str());
  }
}

void EditItems::Run() {
  edit_mode_ = true;
  int cp = 0;
  const int size = static_cast<int>(items_.size());
  Display();
  for (;;) {
    int i1 = items_[cp]->Run(window_);
    if (i1 == PREV) {
      if (--cp < 0) {
        cp = size - 1;
      }
    } else if (i1 == NEXT) {
      if (++cp >= size) {
        cp = 0;
      }
    } else if (i1 == DONE) {
      out->SetIndicatorMode(IndicatorMode::NONE);
      edit_mode_ = false;
      Display();
      return;
    }
  }
}

void EditItems::Display() const {
  // Show help bar.
  if (edit_mode_) {
    ShowHelpItems(editor_help_items_);
  } else {
    ShowHelpItems(navigation_help_items_);
  }

  out->color_scheme()->SetColor(out->window(), SchemeId::NORMAL);

  for (BaseEditItem* item : items_) {
    item->Display(window_);
  }
}


void EditItems::ShowHelpItems(const std::vector<HelpItem>& help_items) const {
  out->footer()->Move(0, 0);
  out->footer()->ClrtoEol();
  for (const auto& h : help_items) {
    out->color_scheme()->SetColor(out->footer(), SchemeId::FOOTER_KEY);
    out->footer()->AddStr(h.key);
    out->color_scheme()->SetColor(out->footer(), SchemeId::FOOTER_TEXT);
    out->footer()->AddStr("-");
    out->footer()->AddStr(h.description.c_str());
    out->footer()->AddStr(" ");
  }
  out->footer()->Refresh();
}

EditItems::~EditItems() {
  // Since we added raw pointers we must cleanup.  Since AFAIK there is 
  // no easy way to convert from std::initializer_list<T> to
  // std::initializer_list<unique_ptr<T>>
  for (auto item : items_) {
    delete item;
  }

  // Clear the help bar on exit.
  out->footer()->Erase();
  out->footer()->Refresh();
  out->SetIndicatorMode(IndicatorMode::NONE);
}

/**
 * Printf sytle output function.  Most init output code should use this.
 */
void PrintfXY(int x, int y, const char *pszFormat, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormat);
  vsnprintf(szBuffer, 1024, pszFormat, ap);
  va_end(ap);
  out->window()->PutsXY(x, y, szBuffer);
}

void Puts(const char *pszText) {
  out->window()->Puts(pszText);
}

void PutsXY(int x, int y, const char *pszText) {
  out->window()->PutsXY(x, y, pszText);
}

/**
 * Printf sytle output function.  Most init output code should use this.
 */
void Printf(const char *pszFormat, ...) {
  va_list ap;
  char szBuffer[1024];

  va_start(ap, pszFormat);
  vsnprintf(szBuffer, 1024, pszFormat, ap);
  va_end(ap);
  out->window()->Puts(szBuffer);
}

void nlx(int numLines) {
  for (int i = 0; i < numLines; i++) {
    Puts("\r\n");
  }
}

static CursesWindow* CreateDialogWindow(int height, int width) {
  const int maxx = getmaxx(stdscr);
  const int maxy = getmaxy(stdscr);
  const int startx = (maxx - width - 4) / 2;
  const int starty = (maxy - height - 2) / 2;
  CursesWindow *dialog = new CursesWindow(out->window(), height + 2, width + 4, starty, startx);
  dialog->Bkgd(out->color_scheme()->GetAttributesForScheme(SchemeId::DIALOG_BOX));
  out->color_scheme()->SetColor(dialog, SchemeId::DIALOG_BOX);
  dialog->Box(0, 0);
  return dialog;
}

static void CloseDialog(CursesWindow* dialog) {
  delete dialog;
  out->window()->RedrawWin();
  out->window()->Refresh();
}

bool dialog_yn(const string prompt) {
  string s = prompt + " ? ";
  
  CursesWindow* dialog(CreateDialogWindow(1, s.size()));
  dialog->MvAddStr(1, 2, s);
  dialog->Refresh();
  int ch = dialog->GetChar();
  CloseDialog(dialog);
  return ch == 'Y' || ch == 'y';
}


void input_password(const string prompt, char *output, int max_length) {
  vector<string> empty;
  input_password(prompt, empty, output, max_length);
}

void input_password(const string prompt, const vector<string>& text, char *output, int max_length) {
  int maxlen = prompt.size() + max_length;
  for (const auto& s : text) {
    maxlen = std::max<int>(maxlen, s.length());
  }
  CursesWindow *dialog = CreateDialogWindow(text.size() + 2, maxlen);
  out->color_scheme()->SetColor(dialog, SchemeId::DIALOG_TEXT);

  int curline = 1;
  for (const auto& s : text) {
    dialog->MvAddStr(curline++, 2, s);
  }
  out->color_scheme()->SetColor(dialog, SchemeId::DIALOG_PROMPT);
  dialog->MvAddStr(text.size() + 2, 2, prompt);
  dialog->Refresh();
  winput_password(dialog, output, max_length);
  CloseDialog(dialog);
}

void messagebox(const string text) {
  const vector<string> vector = { text };
  messagebox(vector);
}

void messagebox(const vector<string>& text) {
  const string prompt = "Press Any Key";
  int maxlen = prompt.length();
  for (const auto& s : text) {
    maxlen = std::max<int>(maxlen, s.length());
  }
  CursesWindow *dialog = CreateDialogWindow(text.size() + 2, maxlen);
  out->color_scheme()->SetColor(dialog, SchemeId::DIALOG_TEXT);
  int curline = 1;
  for (const auto& s : text) {
    dialog->MvAddStr(curline++, 2, s);
  }
  out->color_scheme()->SetColor(dialog, SchemeId::DIALOG_PROMPT);
  int x = (maxlen - prompt.length()) / 2;
  dialog->MvAddStr(text.size() + 2, x + 2, prompt);
  dialog->Refresh();
  dialog->GetChar();
  CloseDialog(dialog);
}

int input_number(int max_digits) {
  char s[81];
  int return_code = 0;
  memset(&s, 0, 81);
  editline(s, max_digits, NUM_ONLY, &return_code, "");
  if (strlen(s) == 0) {
    return 0;
  }
  return atoi(s);
}

/* This will input a line of data, maximum nMaxLength characters long, terminated
* by a C/R.  if (bAllowLowerCase) is true lowercase is allowed, otherwise all
* characters are converted to uppercase.
*/
void winput_password(CursesWindow* dialog, char *pszOutText, int nMaxLength) {
  out->color_scheme()->SetColor(dialog, SchemeId::DIALOG_PROMPT);

  int curpos = 0;

  for (;;) {
    int ch = dialog->GetChar();
    switch (ch) {
    case 14:
    case 13: // 13 on Win32
    case 10: // 10 on unix
    case KEY_ENTER:
#ifdef PADENTER
    case PADENTER:
#endif
      pszOutText[curpos] = '\0';
      return;
    case 23: // Ctrl-W
      if (curpos) {
        do {
          curpos--;
          dialog->AddStr("\b \b");
          if (pszOutText[curpos] == 26) {
            dialog->AddStr("\b \b");
          }
        } while (curpos && (pszOutText[curpos - 1] != 32));
      }
      break;
    case 26:  // control Z
      break;
    case 27:  { // escape
      pszOutText[0] = '\0';
      return;
    };
    case 8:
    case 0x7f: // some other backspace
    case KEY_BACKSPACE:
      if (curpos) {
        curpos--;
        dialog->AddStr("\b \b");
        if (pszOutText[curpos] == 26) {
          dialog->AddStr("\b \b");
        }
      }
      break;
    case 21: // control U
    case 24: // control X
      while (curpos) {
        curpos--;
       dialog->AddStr("\b \b");
        if (pszOutText[curpos] == 26) {
         dialog->AddStr("\b \b");
        }
      }
      break;
    default:
      if (ch > 31 && curpos < nMaxLength) {
        ch = toupper(ch);
        pszOutText[curpos++] = ch;
#ifdef _WIN32
        dialog->AddCh(ACS_CKBOARD);
#else
        dialog->AddCh(ACS_DIAMOND);
#endif  // _WIN32
      }
      break;
    }
  }
}

char onek(const char *pszKeys) {
  char ch = 0;

  while (!strchr(pszKeys, ch = toupper(wgetch(out->window()->window()))))
    ;
  return ch;
}

static int background_character = 32;;

static int editlinestrlen(char *pszText) {
  int i = strlen(pszText);
  while (i >= 0 && (static_cast<unsigned char>(pszText[i - 1]) == background_character)) {
    --i;
  }
  return i;
}

void editline(string* s, int len, int status, int *returncode, const char *ss) {
  char pszBuffer[255];
  strcpy(pszBuffer, s->c_str());
  editline(pszBuffer, len, status, returncode, ss);
  s->assign(pszBuffer);
}

/* editline edits a string, doing I/O to the screen only. */
void editline(char *s, int len, int status, int *returncode, const char *ss) {
  attr_t old_attr;
  short old_pair;
  out->window()->AttrGet(&old_attr, &old_pair);
  int cx = out->window()->GetcurX();
  int cy = out->window()->GetcurY();
  for (int i = strlen(s); i < len; i++) {
    s[i] = static_cast<char>(background_character);
  }
  s[len] = '\0';
  out->color_scheme()->SetColor(out->window(), SchemeId::EDITLINE);
  out->window()->Puts(s);
  out->SetIndicatorMode(IndicatorMode::OVERWRITE);
  out->window()->GotoXY(cx, cy);
  bool done = false;
  int pos = 0;
  bool bInsert = false;
  do {
    int ch = out->window()->GetChar();
    switch (ch) {
    case KEY_F(1): // curses
      done = true;
      *returncode = DONE;
      break;
    case KEY_HOME: // curses
      pos = 0;
      out->window()->GotoXY(cx, cy);
      break;
    case KEY_END: // curses
      pos = editlinestrlen(s);
      out->window()->GotoXY(cx + pos, cy);
      break;
    case KEY_RIGHT: // curses
      if (pos < len) {                       //right
        int nMaxPos = editlinestrlen(s);
        if (pos < nMaxPos) {
          pos++;
          out->window()->GotoXY(cx + pos, cy);
        }
      }
      break;
    case KEY_LEFT: // curses
      if (pos > 0) { //left
        pos--;
        out->window()->GotoXY(cx + pos, cy);
      }
      break;
    case CO:                                      //return
    case KEY_UP: // curses
      done = true;
      *returncode = PREV;
      break;
    case KEY_DOWN: // curses
      done = true;
      *returncode = NEXT;
      break;
    case KEY_IC: // curses
      if (status != SET) {
        if (bInsert) {
          bInsert = false;
	        out->SetIndicatorMode(IndicatorMode::OVERWRITE);
          out->window()->GotoXY(cx + pos, cy);
        } else {
          bInsert = true;
	        out->SetIndicatorMode(IndicatorMode::INSERT);
          out->window()->GotoXY(cx + pos, cy);
        }
      }
      break;
    case KEY_DC: // curses
    case CD: // control-d
      if (status != SET) {
        for (int i = pos; i < len; i++) {
          s[i] = s[i + 1];
        }
        s[len - 1] = static_cast<char>(background_character);
        out->window()->PutsXY(cx, cy, s);
        out->window()->GotoXY(cx + pos, cy);
      }
      break;
    default:
      if (ch > 31) {
        if (status == UPPER_ONLY) {
          ch = toupper(ch);
        }
        if (status == SET) {
          ch = toupper(ch);
          if (ch != ' ')  {
            bool bLookingForSpace = true;
            for (int i = 0; i < len; i++) {
              if (ch == ss[i] && bLookingForSpace) {
                bLookingForSpace = false;
                pos = i;
                out->window()->GotoXY(cx + pos, cy);
                if (s[pos] == ' ') {
                  ch = ss[pos];
                } else {
                  ch = ' ';
                }
              }
            }
            if (bLookingForSpace) {
              ch = ss[pos];
            }
          }
        }
        if ((pos < len) && ((status == ALL) || (status == UPPER_ONLY) || (status == SET) ||
                            ((status == NUM_ONLY) && (((ch >= '0') && (ch <= '9')) || (ch == ' '))))) {
          if (bInsert)  {
            for (int i = len - 1; i > pos; i--) {
              s[i] = s[i - 1];
            }
            s[pos++] = ch;
            out->window()->PutsXY(cx, cy, s);
            out->window()->GotoXY(cx + pos, cy);
          }  else  {
            s[pos++] = ch;
            out->window()->Putch(ch);
          }
        }
      }
      break;
    case KEY_ENTER:
#ifdef PADENTER
    case PADENTER:
#endif
    case RETURN: // return
    case TAB:
      done = true;
      *returncode = NEXT;
      break;
    case ESC: // esc
      done = true;
      *returncode = DONE;
      break;
    case 0x7f:  // yet some other delete key
    case KEY_BACKSPACE:  // curses
    case BACKSPACE:  //backspace
      if (status != SET) {
        if (pos > 0) {
          for (int i = pos - 1; i < len; i++) {
            s[i] = s[i + 1];
          }
          s[len - 1] = static_cast<char>(background_character);
          pos--;
          out->window()->PutsXY(cx, cy, s);
          out->window()->GotoXY(cx + pos, cy);
        }
      }
      break;
    case CA: // control-a
      pos = 0;
      out->window()->GotoXY(cx, cy);
      break;
    case CE: // control-e
      pos = editlinestrlen(s);
      out->window()->GotoXY(cx + pos, cy);
      break;
    }
  } while (!done);

  int z = strlen(s);
  while (z >= 0 && static_cast<unsigned char>(s[z - 1]) == background_character) {
    --z;
  }
  s[z] = '\0';

  char szFinishedString[ 260 ];
  sprintf(szFinishedString, "%-255s", s);
  szFinishedString[ len ] = '\0';
  out->window()->AttrSet(COLOR_PAIR(old_pair) | old_attr);
  out->window()->PutsXY(cx, cy, szFinishedString);
  out->window()->GotoXY(cx, cy);
}

int toggleitem(int value, const char **strings, int num, int *returncode) {
  if (value < 0 || value >= num) {
    value = 0;
  }

  attr_t old_attr;
  short old_pair;
  out->window()->AttrGet(&old_attr, &old_pair);
  int cx = out->window()->GetcurX();
  int cy = out->window()->GetcurY();
  int curatr = 0x1f;
  out->window()->Puts(strings[value]);
  out->window()->GotoXY(cx, cy);
  bool done = false;
  do  {
    int ch = out->window()->GetChar();
    switch (ch) {
    case KEY_ENTER:
    case RETURN:
    case TAB:
      done = true;
      *returncode = NEXT;
      break;
    case ESC:
      done = true;
      *returncode = DONE;
      break;
    case KEY_F(1): // F1
      done = true;
      *returncode = DONE;
      break;
    case KEY_UP: // UP
    case KEY_BTAB: // SHIFT-TAB
      done = true;
      *returncode = PREV;
      break;
    case KEY_DOWN: // DOWN
      done = true;
      *returncode = NEXT;
      break;
    default:
      if (ch == 32) {
        value = (value + 1) % num;
        out->window()->Puts(strings[value]);
        out->window()->GotoXY(cx, cy);
      }
      break;
    }
  } while (!done);
  out->window()->AttrSet(COLOR_PAIR(old_pair) | old_attr);
  out->window()->PutsXY(cx, cy, strings[value]);
  out->window()->GotoXY(cx, cy);
  return value;
}

int GetNextSelectionPosition(int nMin, int nMax, int nCurrentPos, int nReturnCode) {
  switch (nReturnCode) {
  case PREV:
    --nCurrentPos;
    if (nCurrentPos < nMin) {
      nCurrentPos = nMax;
    }
    break;
  case NEXT:
    ++nCurrentPos;
    if (nCurrentPos > nMax) {
      nCurrentPos = nMin;
    }
    break;
  case DONE:
    nCurrentPos = nMin;
    break;
  }
  return nCurrentPos;
}


/* This will pause output, displaying the [PAUSE] message, and wait for
* a key to be hit.
*/
void pausescr() {
  out->color_scheme()->SetColor(out->window(), SchemeId::INFO);
  out->window()->Puts("[PAUSE]");
  out->color_scheme()->SetColor(out->window(), SchemeId::NORMAL);
  out->window()->GetChar();
  for (int i = 0; i < 7; i++) {
    out->window()->AddStr("\b \b");
  }
}
