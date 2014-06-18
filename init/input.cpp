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
#include <curses.h>
#include <cstdint>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

#include "ifcns.h"
#include "init.h"
#include "wwivinit.h"
#include "wconstants.h"


// local functions.
void winput_password(WINDOW* dialog, char *pszOutText, int nMaxLength);

using std::string;
using std::vector;

template<> int StringEditItem<char *>::Run() {
  out->GotoXY(x_, y_);
  int return_code = 0;
  int status = uppercase_ ? UPPER_ONLY : ALL;
  editline(data_, maxsize_, status, &return_code, "");
  return return_code;
}

template<> int StringEditItem<unsigned char *>::Run() {
  out->GotoXY(x_, y_);
  int return_code = 0;
  int status = uppercase_ ? UPPER_ONLY : ALL;
  editline(reinterpret_cast<char*>(data_), maxsize_, status, &return_code, "");
  return return_code;
}

template<typename T> 
static int EditNumberItem(T* data, int maxlen) {
  char s[21];
  int return_code = 0;
  sprintf(s, "%-7u", *data);
  editline(s, maxlen, NUM_ONLY, &return_code, "");
  *data = atoi(s);
  return return_code;
}

template<> int NumberEditItem<uint32_t>::Run() {
  out->GotoXY(x_, y_);
  return EditNumberItem<uint32_t>(data_, 5);
}

template<> int NumberEditItem<int8_t>::Run() {
  out->GotoXY(x_, y_);
  return EditNumberItem<int8_t>(data_, 3);
}

template<> int NumberEditItem<uint8_t>::Run() {
  out->GotoXY(x_, y_);
  return EditNumberItem<uint8_t>(data_, 3);
}

int CustomEditItem::Run() {
  out->GotoXY(x_, y_);
  std::string s = to_field_();
  char data[81];
  strcpy(data, s.c_str());

  int return_code = 0;
  editline(&s, maxsize_, ALL, &return_code, "");
  s.assign(data);
  from_field_(data);
  return return_code;
}

void CustomEditItem::Display() const {
  out->GotoXY(x_, y_);
  std::string blanks(maxsize_, ' ');
  Puts(blanks.c_str());

  string s = to_field_();
  if (display_) {
    display_(s);
  } else {
    out->GotoXY(x_, y_);
    Puts(s.c_str());
  }
}

void EditItems::Run() {
  int cp = 0;
  const int size = static_cast<int>(items_.size());
  Display();
  for (;;) {
    int i1 = items_[cp]->Run();
    if (i1 == PREV) {
      if (--cp < 0) {
        cp = size - 1;
      }
    } else if (i1 == NEXT) {
      if (++cp >= size) {
        cp = 0;
      }
    } else if (i1 == DONE) {
      return;
    }
  }
}

void EditItems::Display() const {
  // Show help bar.
  ShowHelp();
  if (additional_helpfn_)
    additional_helpfn();

  textattr(COLOR_CYAN);

  for (BaseEditItem* item : items_) {
    item->Display();
  }
}

void EditItems::ShowHelp() const {
  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_YELLOW)); 
  wattron(out->footer(), A_BOLD); 
  mvwaddstr(out->footer(), 0, 0, "Esc");
  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_CYAN)); 
  wattroff(out->footer(), A_BOLD);
  waddstr(out->footer(), "-Exit ");

  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_YELLOW));
  wattron(out->footer(), A_BOLD); 
  waddstr(out->footer(), "Enter");
  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_CYAN)); 
  wattroff(out->footer(), A_BOLD);
  waddstr(out->footer(), "-Edit ");  

  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_YELLOW)); 
  wattron(out->footer(), A_BOLD); 
  waddstr(out->footer(), "[");
  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_CYAN)); 
  wattroff(out->footer(), A_BOLD);
  waddstr(out->footer(), "-Previous ");
  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_YELLOW)); 
  wattron(out->footer(), A_BOLD); 
  waddstr(out->footer(), "]");
  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_CYAN)); 
  wattroff(out->footer(), A_BOLD);
  waddstr(out->footer(), "-Next ");
  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_YELLOW)); 
  wattron(out->footer(), A_BOLD); 
  waddstr(out->footer(), "{");
  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_CYAN)); 
  wattroff(out->footer(), A_BOLD);
  waddstr(out->footer(), "-Previous 10 ");
  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_YELLOW)); 
  wattron(out->footer(), A_BOLD); 
  waddstr(out->footer(), "}");
  wattrset(out->footer(), COLOR_PAIR((COLOR_BLUE * 16) + COLOR_CYAN)); 
  wattroff(out->footer(), A_BOLD);
  waddstr(out->footer(), "-Next 10 ");

  if (additional_helpfn_) {
    wmove(out->footer(), 1, 0);
    additional_helpfn_();
  }

  wrefresh(out->footer());
}

EditItems::~EditItems() {
  // Since we added raw pointers we must cleanup.  Since AFAIK there is 
  // no easy way to convert from std::initializer_list<T> to
  // std::initializer_list<unique_ptr<T>>
  for (auto item : items_) {
    delete item;
  }

  // Clear the help bar on exit.
  werase(out->footer());
  wrefresh(out->footer());
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
  out->PutsXY(x, y, szBuffer);
}

void Puts(const char *pszText) {
  out->Puts(pszText);
}

void PutsXY(int x, int y, const char *pszText) {
  out->PutsXY(x, y, pszText);
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
  out->Puts(szBuffer);
}

void nlx(int numLines) {
  for (int i = 0; i < numLines; i++) {
    Puts("\r\n");
  }
}

static WINDOW* CreateDialogWindow(int height, int width) {
  const int maxx = getmaxx(stdscr);
  const int maxy = getmaxy(stdscr);
  const int startx = (maxx - width - 4) / 2;
  const int starty = (maxy - height - 2) / 2;
  WINDOW *dialog = newwin(height + 2, width + 4, starty, startx);
  wbkgd(dialog, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_WHITE));
  wattrset(dialog, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_YELLOW));
  wattron(dialog, A_BOLD);
  box(dialog, 0, 0);
  return dialog;
}

static void CloseDialog(WINDOW* dialog) {
  delwin(dialog);
  redrawwin(out->window());
  out->Refresh();
  touchwin(out->window());
}

bool dialog_yn(const std::string prompt) {
  std::string s = prompt + " ? ";
  WINDOW *dialog = CreateDialogWindow(1, s.size());
  mvwaddstr(dialog, 1, 2, s.c_str());
  wrefresh(dialog);
  int ch = wgetch(dialog);
  CloseDialog(dialog);
  return ch == 'Y' || ch == 'y';
}


void input_password(const std::string prompt, char *output, int max_length) {
  vector<std::string> empty;
  input_password(prompt, empty, output, max_length);
}

void input_password(const std::string prompt, const vector<std::string>& text, char *output, int max_length) {
  int maxlen = prompt.size() + max_length;
  for (const auto& s : text) {
    maxlen = std::max<int>(maxlen, s.length());
  }
  WINDOW *dialog = CreateDialogWindow(text.size() + 2, maxlen);
  wattrset(dialog, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_CYAN));
  int curline = 1;
  for (const auto& s : text) {
    mvwaddstr(dialog, curline++, 2, s.c_str());
  }
  wattrset(dialog, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_YELLOW));
  mvwaddstr(dialog, text.size() + 2, 2, prompt.c_str());
  wrefresh(dialog);
  winput_password(dialog, output, max_length);
  CloseDialog(dialog);
}

void messagebox(const std::string text) {
  const vector<string> vector = { text };
  messagebox(vector);
}

void messagebox(const std::vector<std::string>& text) {
  const string prompt = "Press Any Key";
  int maxlen = prompt.length();
  for (const auto& s : text) {
    maxlen = std::max<int>(maxlen, s.length());
  }
  WINDOW *dialog = CreateDialogWindow(text.size() + 2, maxlen);
  wattrset(dialog, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_CYAN));
  int curline = 1;
  for (const auto& s : text) {
    mvwaddstr(dialog, curline++, 2, s.c_str());
  }
  wattrset(dialog, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_YELLOW));
  int x = (maxlen - prompt.length()) / 2;
  mvwaddstr(dialog, text.size() + 2, x + 2, prompt.c_str());
  wrefresh(dialog);
  wgetch(dialog);
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
void winput_password(WINDOW* dialog, char *pszOutText, int nMaxLength) {
  wattrset(dialog, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_YELLOW));

  int curpos = 0;

  for (;;) {
    int ch = wgetch(dialog);
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
          waddstr(dialog, "\b \b");
          if (pszOutText[curpos] == 26) {
            waddstr(dialog, "\b \b");
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
        waddstr(dialog, "\b \b");
        if (pszOutText[curpos] == 26) {
          waddstr(dialog, "\b \b");
        }
      }
      break;
    case 21: // control U
    case 24: // control X
      while (curpos) {
        curpos--;
        waddstr(dialog, "\b \b");
        if (pszOutText[curpos] == 26) {
          waddstr(dialog, "\b \b");
        }
      }
      break;
    default:
      if (ch > 31 && curpos < nMaxLength) {
        ch = upcase(ch);
        pszOutText[curpos++] = ch;
#ifdef _WIN32
        waddch(dialog, ACS_CKBOARD);
#else
        waddch(dialog, ACS_DIAMOND);
#endif  // _WIN32
      }
      break;
    }
  }
}

char onek(const char *pszKeys) {
  char ch = 0;

  while (!strchr(pszKeys, ch = upcase(wgetch(out->window()))))
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

void editline(std::string* s, int len, int status, int *returncode, const char *ss) {
  char pszBuffer[255];
  strcpy(pszBuffer, s->c_str());
  editline(pszBuffer, len, status, returncode, ss);
  s->assign(pszBuffer);
}

/* editline edits a string, doing I/O to the screen only. */
void editline(char *s, int len, int status, int *returncode, const char *ss) {
  int i;
  int oldatr = curatr;
  int cx = out->WhereX();
  int cy = out->WhereY();
  for (i = strlen(s); i < len; i++) {
    s[i] = static_cast<char>(background_character);
  }
  s[len] = '\0';
  textattr((16 * COLOR_BLUE) + COLOR_WHITE);
  out->Puts(s);
  out->GotoXY(76, 0);
  out->Puts("OVR");
  out->GotoXY(cx, cy);
  bool done = false;
  int pos = 0;
  bool bInsert = false;
  do {
    int ch = wgetch(out->window());
    switch (ch) {
    case KEY_F(1): // curses
      done = true;
      *returncode = DONE;
      break;
    case KEY_HOME: // curses
      pos = 0;
      out->GotoXY(cx, cy);
      break;
    case KEY_END: // curses
      pos = editlinestrlen(s);
      out->GotoXY(cx + pos, cy);
      break;
    case KEY_RIGHT: // curses
      if (pos < len) {                       //right
        int nMaxPos = editlinestrlen(s);
        if (pos < nMaxPos) {
          pos++;
          out->GotoXY(cx + pos, cy);
        }
      }
      break;
    case KEY_LEFT: // curses
      if (pos > 0) { //left
        pos--;
        out->GotoXY(cx + pos, cy);
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
          out->GotoXY(77, 0);
          out->Puts("OVR");
          out->GotoXY(cx + pos, cy);
        } else {
          bInsert = true;
          out->GotoXY(77, 0);
          out->Puts("INS");
          out->GotoXY(cx + pos, cy);
        }
      }
      break;
    case KEY_DC: // curses
    case CD: // control-d
      if (status != SET) {
        for (i = pos; i < len; i++) {
          s[i] = s[i + 1];
        }
        s[len - 1] = static_cast<char>(background_character);
        out->GotoXY(cx, cy);
        out->Puts(s);
        out->GotoXY(cx + pos, cy);
      }
      break;
    default:
      if (ch > 31) {
        if (status == UPPER_ONLY) {
          ch = upcase(ch);
        }
        if (status == SET) {
          ch = upcase(ch);
          if (ch != ' ')  {
            int i1 = 1;
            for (i = 0; i < len; i++) {
              if (ch == ss[i] && i1) {
                i1 = 0;
                pos = i;
                out->GotoXY(cx + pos, cy);
                if (s[pos] == ' ') {
                  ch = ss[pos];
                } else {
                  ch = ' ';
                }
              }
              if (i1) {
                ch = ss[pos];
              }
            }
          }
        }
        if ((pos < len) && ((status == ALL) || (status == UPPER_ONLY) || (status == SET) ||
                            ((status == NUM_ONLY) && (((ch >= '0') && (ch <= '9')) || (ch == ' '))))) {
          if (bInsert)  {
            for (i = len - 1; i > pos; i--) {
              s[i] = s[i - 1];
            }
            s[pos++] = ch;
            out->GotoXY(cx, cy);
            out->Puts(s);
            out->GotoXY(cx + pos, cy);
          }  else  {
            s[pos++] = ch;
            out->Putch(ch);
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
      if (pos > 0) {
        for (i = pos - 1; i < len; i++) {
          s[i] = s[i + 1];
        }
        s[len - 1] = static_cast<char>(background_character);
        pos--;
        out->GotoXY(cx, cy);
        out->Puts(s);
        out->GotoXY(cx + pos, cy);
      }
      break;
    case CA: // control-a
      pos = 0;
      out->GotoXY(cx, cy);
      break;
    case CE: // control-e
      pos = editlinestrlen(s);
      out->GotoXY(cx + pos, cy);
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
  out->GotoXY(cx, cy);
  curatr = oldatr;
  out->Puts(szFinishedString);
  out->GotoXY(cx, cy);
}

int toggleitem(int value, const char **strings, int num, int *returncode) {
  if (value < 0 || value >= num) {
    value = 0;
  }

  int oldatr = curatr;
  int cx = out->WhereX();
  int cy = out->WhereY();
  int curatr = 0x1f;
  out->Puts(strings[value]);
  out->GotoXY(cx, cy);
  bool done = false;
  do  {
    int ch = out->GetChar();
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
        out->Puts(strings[value]);
        out->GotoXY(cx, cy);
      }
      break;
    }
  } while (!done);
  out->GotoXY(cx, cy);
  curatr = oldatr;
  out->Puts(strings[value]);
  out->GotoXY(cx, cy);
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
  textattr(COLOR_MAGENTA);
  Puts("[PAUSE]");
  textattr(COLOR_CYAN);
  wgetch(out->window());
  for (int i = 0; i < 7; i++) {
    waddstr(stdscr, "\b \b");
  }
}
