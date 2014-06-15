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
void backspace();


using std::string;
using std::vector;

template<> int StringEditItem<char *>::Run() {
  app->localIO->LocalGotoXY(x_, y_);
  int return_code = 0;
  int status = uppercase_ ? UPPER_ONLY : ALL;
  editline(data_, maxsize_, status, &return_code, "");
  return return_code;
}

template<> int StringEditItem<unsigned char *>::Run() {
  app->localIO->LocalGotoXY(x_, y_);
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
  app->localIO->LocalGotoXY(x_, y_);
  return EditNumberItem<uint32_t>(data_, 5);
}

template<> int NumberEditItem<int8_t>::Run() {
  app->localIO->LocalGotoXY(x_, y_);
  return EditNumberItem<int8_t>(data_, 3);
}

template<> int NumberEditItem<uint8_t>::Run() {
  app->localIO->LocalGotoXY(x_, y_);
  return EditNumberItem<uint8_t>(data_, 3);
}

int CustomEditItem::Run() {
  app->localIO->LocalGotoXY(x_, y_);
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
  app->localIO->LocalGotoXY(x_, y_);
  std::string blanks(maxsize_, ' ');
  Puts(blanks.c_str());

  string s = to_field_();
  if (display_) {
    display_(s);
  } else {
    app->localIO->LocalGotoXY(x_, y_);
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
  textattr(COLOR_CYAN);

  for (BaseEditItem* item : items_) {
    item->Display();
  }
}

EditItems::~EditItems() {
  // Since we added raw pointers we must cleanup.  Since AFAIK there is 
  // no easy way to convert from std::initializer_list<T> to
  // std::initializer_list<unique_ptr<T>>
  for (auto item : items_) {
    delete item;
  }
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
  app->localIO->LocalXYPuts(x, y, szBuffer);
}

void Puts(const char *pszText) {
  app->localIO->LocalPuts(pszText);
}

void PutsXY(int x, int y, const char *pszText) {
  app->localIO->LocalXYPuts(x, y,pszText);
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
  app->localIO->LocalPuts(szBuffer);
}

void nlx(int numLines) {
  for (int i = 0; i < numLines; i++) {
    Puts("\r\n");
  }
}

bool dialog_yn(const std::string prompt) {
  const int maxx = getmaxx(stdscr);
  const int maxy = getmaxy(stdscr);
  std::string s = prompt + " ? ";
  const int width = s.size() + 4;
  const int startx = (maxx - s.size()) / 2;
  const int starty = (maxy - 3) / 2;
  WINDOW *dialog = newwin(3, width, starty, startx);
  wbkgd(dialog, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_WHITE));
  wattrset(dialog, COLOR_PAIR((16 * COLOR_BLUE) + COLOR_YELLOW));
  wattron(dialog, A_BOLD);
  box(dialog, 0, 0);
  mvwaddstr(dialog, 1, 2, s.c_str());
  wrefresh(dialog);
  int ch = wgetch(dialog);
  delwin(dialog);
  redrawwin(stdscr);
  refresh();
  return ch == 'Y' || ch == 'y';
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
void input_password(char *pszOutText, int nMaxLength) {
  int curpos = 0;
  bool done = false;

  while (!done) {
    int ch = getch();
    switch (ch) {
    case 14:
    case 13: // 13 on Win32
    case 10: // 10 on unix
    case KEY_ENTER:
#ifdef PADENTER
    case PADENTER:
#endif
      pszOutText[curpos] = 0;
      done = true;
      break;
    case 23: // Ctrl-W
      if (curpos) {
        do {
          curpos--;
          backspace();
          if (pszOutText[curpos] == 26) {
            backspace();
          }
        } while (curpos && (pszOutText[curpos - 1] != 32));
      }
      break;
    case 26:  // control Z
      break;
    case 8:
    case 0x7f: // some other backspace
    case KEY_BACKSPACE:
      if (curpos) {
        curpos--;
        backspace();
        if (pszOutText[curpos] == 26) {
          backspace();
        }
      }
      break;
    case 21: // control U
    case 24: // control X
      while (curpos) {
        curpos--;
        backspace();
        if (pszOutText[curpos] == 26) {
          backspace();
        }
      }
      break;
    default:
      if (ch > 31 && curpos < nMaxLength) {
        ch = upcase(ch);
        pszOutText[curpos++] = ch;
        app->localIO->LocalPutch('\xFE');
      }
      break;
    }
  }
}

char onek(const char *pszKeys) {
  char ch = 0;

  while (!strchr(pszKeys, ch = upcase(getch())))
    ;
  app->localIO->LocalPutch(ch);
  nlx();
  return ch;
}

static int background_character = 0xb0;

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
  int cx = app->localIO->WhereX();
  int cy = app->localIO->WhereY();
  for (i = strlen(s); i < len; i++) {
    s[i] = static_cast<char>(background_character);
  }
  s[len] = '\0';
  textattr((16 * COLOR_BLUE) + COLOR_WHITE);
  app->localIO->LocalPuts(s);
  app->localIO->LocalGotoXY(77, 0);
  app->localIO->LocalPuts("OVR");
  app->localIO->LocalGotoXY(cx, cy);
  bool done = false;
  int pos = 0;
  bool bInsert = false;
  do {
    int ch = getch();
    switch (ch) {
    case KEY_F(1): // curses
      done = true;
      *returncode = DONE;
      break;
    case KEY_HOME: // curses
      pos = 0;
      app->localIO->LocalGotoXY(cx, cy);
      break;
    case KEY_END: // curses
      pos = editlinestrlen(s);
      app->localIO->LocalGotoXY(cx + pos, cy);
      break;
    case KEY_RIGHT: // curses
      if (pos < len) {                       //right
        int nMaxPos = editlinestrlen(s);
        if (pos < nMaxPos) {
          pos++;
          app->localIO->LocalGotoXY(cx + pos, cy);
        }
      }
      break;
    case KEY_LEFT: // curses
      if (pos > 0) { //left
        pos--;
        app->localIO->LocalGotoXY(cx + pos, cy);
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
          app->localIO->LocalGotoXY(77, 0);
          app->localIO->LocalPuts("OVR");
          app->localIO->LocalGotoXY(cx + pos, cy);
        } else {
          bInsert = true;
          app->localIO->LocalGotoXY(77, 0);
          app->localIO->LocalPuts("INS");
          app->localIO->LocalGotoXY(cx + pos, cy);
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
        app->localIO->LocalGotoXY(cx, cy);
        app->localIO->LocalPuts(s);
        app->localIO->LocalGotoXY(cx + pos, cy);
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
                app->localIO->LocalGotoXY(cx + pos, cy);
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
            app->localIO->LocalGotoXY(cx, cy);
            app->localIO->LocalPuts(s);
            app->localIO->LocalGotoXY(cx + pos, cy);
          }  else  {
            s[pos++] = ch;
            app->localIO->LocalPutch(ch);
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
        app->localIO->LocalGotoXY(cx, cy);
        app->localIO->LocalPuts(s);
        app->localIO->LocalGotoXY(cx + pos, cy);
      }
      break;
    case CA: // control-a
      pos = 0;
      app->localIO->LocalGotoXY(cx, cy);
      break;
    case CE: // control-e
      pos = editlinestrlen(s);
      app->localIO->LocalGotoXY(cx + pos, cy);
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
  app->localIO->LocalGotoXY(cx, cy);
  curatr = oldatr;
  app->localIO->LocalPuts(szFinishedString);
  app->localIO->LocalGotoXY(cx, cy);
}

int toggleitem(int value, const char **strings, int num, int *returncode) {
  if (value < 0 || value >= num) {
    value = 0;
  }

  int oldatr = curatr;
  int cx = app->localIO->WhereX();
  int cy = app->localIO->WhereY();
  int curatr = 0x1f;
  app->localIO->LocalPuts(strings[value]);
  app->localIO->LocalGotoXY(cx, cy);
  bool done = false;
  do  {
    int ch = app->localIO->getchd();
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
        app->localIO->LocalPuts(strings[value]);
        app->localIO->LocalGotoXY(cx, cy);
      }
      break;
    }
  } while (!done);
  app->localIO->LocalGotoXY(cx, cy);
  curatr = oldatr;
  app->localIO->LocalPuts(strings[value]);
  app->localIO->LocalGotoXY(cx, cy);
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
  getch();
  for (int i = 0; i < 7; i++) {
    backspace();
  }
}

/* This function executes a backspace, space, backspace sequence. */
static void backspace() {
  Printf("\b \b");
}
