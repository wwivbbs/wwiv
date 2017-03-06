/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2017, WWIV Software Services           */
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
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

#include "bbs/keycodes.h"
#include "bbs/wconstants.h" 
#include "core/file.h"
#include "core/strings.h"
#include "localui/wwiv_curses.h"
#include "localui/curses_io.h"
#include "localui/ui_win.h"
#include "localui/curses_win.h"
#include "localui/stdio_win.h"

#ifdef INSERT // defined in wconstants.h
#undef INSERT
#endif  // INSERT

#define PREV                1
#define NEXT                2
#define DONE                4
#define ABORTED             8

using std::string;
using std::unique_ptr;
using std::vector;
using namespace wwiv::strings;

int CustomEditItem::Run(CursesWindow* window) {
  window->GotoXY(x_, y_);
  string s = to_field_();

  int return_code = editline(window, &s, maxsize_, EditLineMode::ALL, "");
  from_field_(s);
  return return_code;
}

void CustomEditItem::Display(CursesWindow* window) const {
  window->GotoXY(x_, y_);
  string blanks(maxsize_, ' ');
  window->Puts(blanks.c_str());

  string s = to_field_();
  if (display_) {
    display_(s);
  } else {
    window->PutsXY(x_, y_, s.c_str());
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
      io_->SetIndicatorMode(IndicatorMode::NONE);
      edit_mode_ = false;
      Display();
      return;
    }
  }
}

void EditItems::Display() const {
  // Show help bar.
  if (edit_mode_) {
    io_->footer()->window()->Move(1, 0);
    io_->footer()->window()->ClrtoEol();
    io_->footer()->ShowHelpItems(0, editor_help_items_);
  } else {
    io_->footer()->ShowHelpItems(0, navigation_help_items_);
    io_->footer()->ShowHelpItems(1, navigation_extra_help_items_);
  }

  window_->SetColor(SchemeId::WINDOW_DATA);

  for (BaseEditItem* item : items_) {
    item->Display(window_);
  }
}

EditItems::~EditItems() {
  // Since we added raw pointers we must cleanup.  Since AFAIK there is 
  // no easy way to convert from std::initializer_list<T> to
  // std::initializer_list<unique_ptr<T>>
  for (auto item : items_) {
    delete item;
  }

  // Clear the help bar on exit.
  io_->footer()->window()->Erase();
  io_->footer()->window()->Refresh();
  io_->SetIndicatorMode(IndicatorMode::NONE);
}

static UIWindow* CreateDialogWindow(UIWindow* parent, int height, int width) {
  const int maxx = parent->GetMaxX();
  const int maxy = parent->GetMaxY();
  const int startx = (maxx - width - 4) / 2;
  const int starty = (maxy - height - 2) / 2;
  UIWindow* dialog;
  if (parent->IsGUI()) {
    dialog = new CursesWindow(static_cast<CursesWindow*>(parent), parent->color_scheme(), 
      height + 2, width + 4, starty, startx);
  }
  else {
    dialog = new StdioWindow(parent, parent->color_scheme());
  }
  dialog->Bkgd(parent->color_scheme()->GetAttributesForScheme(SchemeId::DIALOG_BOX));
  dialog->SetColor(SchemeId::DIALOG_BOX);
  dialog->Box(0, 0);
  return dialog;
}

bool dialog_yn(CursesWindow* window, const vector<string>& text) {
  int maxlen = 4;
  for (const auto& s : text) {
    maxlen = std::max<int>(maxlen, s.length());
  }
  unique_ptr<UIWindow> dialog(CreateDialogWindow(window, text.size(), maxlen));
  dialog->SetColor(SchemeId::DIALOG_TEXT);
  int curline = 1;
  for (const auto& s : text) {
    dialog->PutsXY(2, curline++, s);
  }
  dialog->SetColor(SchemeId::DIALOG_PROMPT);
  dialog->Refresh();
  return toupper(dialog->GetChar()) == 'Y';

}

bool dialog_yn(CursesWindow* window, const string& text) {
  const vector<string> text_vector = { text };
  return dialog_yn(window, text_vector);
}

static void winput_password(CursesWindow* dialog, string *output, int max_length) {
  dialog->SetColor(SchemeId::DIALOG_PROMPT);

  int curpos = 0;
  string s;
  s.resize(max_length);
  output->clear();
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
      s.resize(curpos);
      output->assign(s);
      return;
    case 23: // Ctrl-W
      if (curpos) {
        do {
          curpos--;
          dialog->Puts("\b \b");
          if (s[curpos] == 26) {
            dialog->Puts("\b \b");
          }
        } while (curpos && (s[curpos - 1] != 32));
      }
      break;
    case 26:  // control Z
      break;
    case 27:  { // escape
      output->clear();
      return;
    };
    case 8:
    case 0x7f: // some other backspace
    case KEY_BACKSPACE:
      if (curpos) {
        curpos--;
        dialog->Puts("\b \b");
        if (s[curpos] == 26) {
          dialog->Puts("\b \b");
        }
      }
      break;
    case 21: // control U
    case 24: // control X
      while (curpos) {
        curpos--;
        dialog->Puts("\b \b");
        if (s[curpos] == 26) {
         dialog->Puts("\b \b");
        }
      }
      break;
    default:
      if (ch > 31 && curpos < max_length) {
        s[curpos++] = to_upper_case<char>(ch);
        dialog->Putch(ACS_DIAMOND);
      }
      break;
    }
  }
}

void input_password(CursesWindow* window, const string& prompt, const vector<string>& text, string *output, int max_length) {
  int maxlen = prompt.size() + max_length;
  for (const auto& s : text) {
    maxlen = std::max<int>(maxlen, s.length());
  }
  CHECK(window->IsGUI()) << "input_password needs a GUI.";
  unique_ptr<CursesWindow> dialog(
      static_cast<CursesWindow*>(CreateDialogWindow(window, text.size() + 2, maxlen)));
  dialog->SetColor(SchemeId::DIALOG_TEXT);

  int curline = 1;
  for (const auto& s : text) {
    dialog->PutsXY(2, curline++, s);
  }
  dialog->SetColor(SchemeId::DIALOG_PROMPT);
  dialog->PutsXY(2, text.size() + 2, prompt);
  dialog->Refresh();
  winput_password(dialog.get(), output, max_length);
}

int messagebox(UIWindow* window, const string& text) {
  const vector<string> vector = { text };
  return messagebox(window, vector);
}

int messagebox(UIWindow* window, const vector<string>& text) {
  const string prompt = "Press Any Key";
  int maxlen = prompt.length() + 2;
  for (const auto& s : text) {
    maxlen = std::max<int>(maxlen, s.length());
  }
  unique_ptr<UIWindow> dialog(CreateDialogWindow(window, text.size() + 2, maxlen));
  dialog->SetColor(SchemeId::DIALOG_TEXT);
  int curline = 1;
  for (const auto& s : text) {
    dialog->PutsXY(2, curline++, s);
  }
  dialog->SetColor( SchemeId::DIALOG_PROMPT);
  int x = (maxlen - prompt.length()) / 2;
  dialog->PutsXY(x + 2, text.size() + 2, prompt);
  dialog->Refresh();
  return dialog->GetChar();
}

std::string dialog_input_string(CursesWindow* window, const std::string& prompt, size_t max_length) {
  unique_ptr<UIWindow> dialog(CreateDialogWindow(window, 3, prompt.size() + 4 + max_length));
  dialog->PutsXY(2, 2, prompt);
  dialog->Refresh();

  CHECK(window->IsGUI()) << "dialog_input_string needs a GUI.";
  string s;
  editline(static_cast<CursesWindow*>(dialog.get()), &s, max_length, EditLineMode::ALL, "");
  return s;
}

int dialog_input_number(CursesWindow* window, const string& prompt, int min_value, int max_value) {
  int num_digits = max_value > 0 ? static_cast<int>(floor(log10((double)max_value))) + 1 : 1;
  CHECK(window->IsGUI()) << "dialog_input_number needs a GUI.";
  unique_ptr<CursesWindow> dialog(static_cast<CursesWindow*>(
      CreateDialogWindow(window, 3, prompt.size() + 4 + num_digits)));
  dialog->PutsXY(2, 2, prompt);
  dialog->Refresh();

  string s;
  editline(dialog.get(), &s, num_digits, EditLineMode::NUM_ONLY, "");
  if (s.empty()) {
    return min_value;
  }
  try {
    auto v = std::stoi(s);
    if (v < min_value) {
      return min_value;
    }
    return v;
  } catch (std::invalid_argument&) { 
    // No conversion possible.
    return min_value;
  }
}

char onek(CursesWindow* window, const char *pszKeys) {
  char ch = 0;

  while (!strchr(pszKeys, ch = to_upper_case<char>(window->GetChar()))) {
    // NOP
  }
  return ch;
}

static const int background_character = 32;;

static std::size_t editlinestrlen(char *text) {
  std::size_t i = strlen(text);
  while (i >= 0 && (static_cast<unsigned char>(text[i - 1]) == background_character)) {
    --i;
  }
  return i;
}

int editline(CursesWindow* window, string* s, int len, EditLineMode status, const char *ss) {
  char buffer[255];
  wwiv::strings::to_char_array(buffer, *s);
  int rc = editline(window, buffer, len, status, ss);
  s->assign(buffer);
  return rc;
}

/* editline edits a string, doing I/O to the screen only. */
int editline(CursesWindow* window, char *s, int len, EditLineMode status, const char *ss) {
  uint32_t old_attr;
  short old_pair;
  window->AttrGet(&old_attr, &old_pair);
  int cx = window->GetcurX();
  int cy = window->GetcurY();
  int rc = 0;
  for (int i = strlen(s); i < len; i++) {
    s[i] = static_cast<char>(background_character);
  }
  s[len] = '\0';
  window->SetColor(SchemeId::EDITLINE);
  window->Puts(s);
  out->SetIndicatorMode(IndicatorMode::OVERWRITE);
  window->GotoXY(cx, cy);
  bool done = false;
  int pos = 0;
  bool bInsert = false;
  do {
    int ch = window->GetChar();
    switch (ch) {
    case KEY_F(1): // curses
      done = true;
      rc = DONE;
      break;
    case KEY_HOME: // curses
      pos = 0;
      window->GotoXY(cx, cy);
      break;
    case KEY_END: // curses
      pos = editlinestrlen(s);
      window->GotoXY(cx + pos, cy);
      break;
    case KEY_RIGHT: // curses
      if (pos < len) {                       //right
        int nMaxPos = editlinestrlen(s);
        if (pos < nMaxPos) {
          pos++;
          window->GotoXY(cx + pos, cy);
        }
      }
      break;
    case KEY_LEFT: // curses
      if (pos > 0) { //left
        pos--;
        window->GotoXY(cx + pos, cy);
      }
      break;
    case CO:                                      //return
    case KEY_UP: // curses
      done = true;
      rc = PREV;
      break;
    case KEY_DOWN: // curses
      done = true;
      rc = NEXT;
      break;
    case KEY_IC: // curses
      if (status != EditLineMode::SET) {
        if (bInsert) {
          bInsert = false;
	        out->SetIndicatorMode(IndicatorMode::OVERWRITE);
          window->GotoXY(cx + pos, cy);
        } else {
          bInsert = true;
	        out->SetIndicatorMode(IndicatorMode::INSERT);
          window->GotoXY(cx + pos, cy);
        }
      }
      break;
    case KEY_DC: // curses
    case CD: // control-d
      if (status != EditLineMode::SET) {
        for (int i = pos; i < len; i++) {
          s[i] = s[i + 1];
        }
        s[len - 1] = static_cast<char>(background_character);
        window->PutsXY(cx, cy, s);
        window->GotoXY(cx + pos, cy);
      }
      break;
    default:
      if (ch > 31) {
        if (status == EditLineMode::UPPER_ONLY) {
          ch = to_upper_case<char>(ch);
        }
        if (status == EditLineMode::SET) {
          ch = to_upper_case<char>(ch);
          if (ch != ' ')  {
            bool bLookingForSpace = true;
            for (int i = 0; i < len; i++) {
              if (ch == ss[i] && bLookingForSpace) {
                bLookingForSpace = false;
                pos = i;
                window->GotoXY(cx + pos, cy);
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
        if ((pos < len) && 
          (status == EditLineMode::ALL || (status == EditLineMode::UPPER_ONLY) 
            || (status == EditLineMode::SET) 
            || ((status == EditLineMode::NUM_ONLY) 
                && (((ch >= '0') && (ch <= '9')) || (ch == ' ') || (pos == 0) && (ch == '-') ) ))) {
          if (bInsert)  {
            for (int i = len - 1; i > pos; i--) {
              s[i] = s[i - 1];
            }
            s[pos++] = static_cast<char>(ch);
            window->PutsXY(cx, cy, s);
            window->GotoXY(cx + pos, cy);
          }  else  {
            s[pos++] = static_cast<char>(ch);
            window->Putch(ch);
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
      rc = NEXT;
      break;
    case ESC: // esc
      done = true;
      rc = DONE;
      break;
    case 0x7f:  // yet some other delete key
    case KEY_BACKSPACE:  // curses
    case BACKSPACE:  //backspace
      if (status != EditLineMode::SET) {
        if (pos > 0) {
          for (int i = pos - 1; i < len; i++) {
            s[i] = s[i + 1];
          }
          s[len - 1] = static_cast<char>(background_character);
          pos--;
          window->PutsXY(cx, cy, s);
          window->GotoXY(cx + pos, cy);
        }
      }
      break;
    case CA: // control-a
      pos = 0;
      window->GotoXY(cx, cy);
      break;
    case CE: // control-e
      pos = editlinestrlen(s);
      window->GotoXY(cx + pos, cy);
      break;
    }
  } while (!done);

  int z = strlen(s);
  while (z >= 0 && static_cast<unsigned char>(s[z - 1]) == background_character) {
    --z;
  }
  s[z] = '\0';

  char szFinishedString[260];
  sprintf(szFinishedString, "%-255s", s);
  szFinishedString[len] = '\0';
  window->AttrSet(COLOR_PAIR(old_pair) | old_attr);
  window->PutsXY(cx, cy, szFinishedString);
  window->GotoXY(cx, cy);

  return rc;
}

std::vector<std::string>::size_type toggleitem(CursesWindow* window, std::vector<std::string>::size_type value, const std::vector<std::string>& strings, int *rc) {
  if (value < 0 || value >= strings.size()) {
    value = 0;
  }

  size_t max_size = 0;
  for (const auto& item : strings) {
    max_size = std::max<std::size_t>(max_size, item.size());
  }

  uint32_t old_attr;
  short old_pair;
  window->AttrGet(&old_attr, &old_pair);
  int cx = window->GetcurX();
  int cy = window->GetcurY();
  window->PutsXY(cx, cy, strings.at(value));
  window->GotoXY(cx, cy);
  bool done = false;
  do  {
    int ch = window->GetChar();
    switch (ch) {
    case KEY_ENTER:
    case RETURN:
    case TAB:
      done = true;
      *rc = NEXT;
      break;
    case ESC:
      done = true;
      *rc = DONE;
      break;
    case KEY_F(1): // F1
      done = true;
      *rc = DONE;
      break;
    case KEY_UP: // UP
    case KEY_BTAB: // SHIFT-TAB
      done = true;
      *rc = PREV;
      break;
    case KEY_DOWN: // DOWN
      done = true;
      *rc = NEXT;
      break;
    default:
      if (ch == 32) {
        value = (value + 1) % strings.size();
        string s = strings.at(value);
        if (s.size() < max_size) {
          s += string(max_size - s.size(), ' ');
        }
        window->PutsXY(cx, cy, s);
        window->GotoXY(cx, cy);
      }
      break;
    }
  } while (!done);
  window->AttrSet(COLOR_PAIR(old_pair) | old_attr);
  window->PutsXY(cx, cy, strings.at(value));
  window->GotoXY(cx, cy);
  return value;
}

void trimstrpath(char *s) {
  StringTrimEnd(s);

  int i = strlen(s);
  if (i && (s[i - 1] != File::pathSeparatorChar)) {
    // We don't have pathSeparatorString.
    s[i] = File::pathSeparatorChar;
    s[i + 1] = 0;
  }
}
