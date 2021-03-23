/**************************************************************************/
/*                                                                        */
/*                  WWIV Initialization Utility Version 5                 */
/*               Copyright (C)2014-2021, WWIV Software Services           */
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
#include "localui/input.h"

// has to go before cureses

#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "localui/curses_io.h"
#include "localui/curses_win.h"
#include "localui/stdio_win.h"
#include "localui/ui_win.h"
#include "localui/wwiv_curses.h"
#include "local_io/keycodes.h"
#include "sdk/acs/acs.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

namespace wwiv::local::ui {


void Label::Display(CursesWindow* window) const {
  window->GotoXY(x_, y_);
  if (right_justify_) {
    const auto pad = std::max<int>(0, width_ - size_int(text_));
    window->Puts(std::string(pad, ' '));
  }
  window->Puts(text_);
}

MultilineLabel::MultilineLabel(const std::string& text)
    : Label(0, text) {
  lines_ = SplitString(text, "\r\n", false);
  auto w = 0;
  for (const auto& l : lines_) {
    w = std::max<int>(w, size_int(l));
  }
  width_ = w;
}

void MultilineLabel::Display(CursesWindow* window) const {
  for (auto i = 0; i < size_int(lines_); i++) {
    window->GotoXY(x_, y_ + i);
    window->Puts(at(lines_, i));
  }
}

int MultilineLabel::height() const {
  return size_int(lines_);
}

EditlineResult ACSEditItem::Run(CursesWindow* window) {
  window->GotoXY(this->x_, this->y_);
  DefaultDisplay(window);
  window->GotoXY(this->x_, this->y_);
  std::string last_expr;
  const edline_validation_fn fn = [&](const std::string& acs)->void
  {
    if (acs == last_expr) {
      // Don't keep re-evaluating the same thing.
      return;
    }
    last_expr = acs;

    auto [result, ex, info] = sdk::acs::validate_acs(acs, providers_);
    window->SetColor(SchemeId::WINDOW_DATA);
    if (result) {
      curses_out->footer()->ShowContextHelp("The expression is valid.");
    } else {
      curses_out->footer()->ShowContextHelp(StrCat("ACS Error: ", ex));
    }
  };
  return editline(window, &this->data_, this->width_, edit_line_mode_, "", fn);
}

void ACSEditItem::DefaultDisplay(CursesWindow* window) const {
  window->GotoXY(this->x_, this->y_);
  this->DefaultDisplayString(window, data_);
}

EditlineResult FidoAddressStringEditItem::Run(CursesWindow* window) {
  window->GotoXY(this->x_, this->y_);
  DefaultDisplay(window);
  window->GotoXY(this->x_, this->y_);
  std::string last_addr;
  const edline_validation_fn fn = [&](const std::string& raw) -> void {
    const auto addr = StringTrim(raw);
    if (addr == last_addr) {
      // Don't keep re-evaluating the same thing.
      return;
    }
    last_addr = addr;

    if (const auto o = wwiv::sdk::fido::try_parse_fidoaddr(last_addr)) {
      if (!o->domain().empty()) {
        curses_out->footer()->ShowContextHelp(this->help_text());
      } else {
        curses_out->footer()->ShowContextHelp("Domain is missing. Address should be of the form:'Z:N/O@D'");
      }
      return;
    }
    curses_out->footer()->ShowContextHelp("Address is NOT valid. Address should be of the form:'Z:N/O@D'");
  };
  // Run the validation once on entry
  if (!this->data_.empty()) {
    fn(this->data_);
  }
  return editline(window, &this->data_, this->width_, edit_line_mode_, "", fn);
}

void FidoAddressStringEditItem::DefaultDisplay(CursesWindow* window) const {
  window->GotoXY(this->x_, this->y_);
  this->DefaultDisplayString(window, data_);
}


EditlineResult CustomEditItem::Run(CursesWindow* window) {
  window->GotoXY(x_, y_);
  auto s = to_field_();

  // The width_ may be wider than the field width, so we need use field_width_.
  const auto return_code = editline(window, &s, field_width_, EditLineMode::ALL, "");
  from_field_(s);
  return return_code;
}

void CustomEditItem::Display(CursesWindow* window) const {
  window->GotoXY(x_, y_);
  // The width_ may be wider than the field width, so we need use field_width_.
  const std::string blanks(field_width_, ' ');
  window->Puts(blanks);

  const auto s = to_field_();
  if (display_) {
    display_(s);
  } else {
    window->PutsXY(x_, y_, s);
  }
}

EditExternalFileItem::EditExternalFileItem(std::filesystem::path path)
  : BaseEditItem(25), path_(std::move(path)) {
}

static UIWindow* CreateDialogWindow(UIWindow* parent, int height, int width) {
  const auto maxx = parent->GetMaxX();
  const auto maxy = parent->GetMaxY();
  const auto startx = (maxx - width - 4) / 2;
  const auto starty = (maxy - height - 2) / 2;
  UIWindow* dialog;
  if (parent->IsGUI()) {
    dialog = new CursesWindow(dynamic_cast<CursesWindow*>(parent), parent->color_scheme(),
                              height + 2, width + 4, starty, startx);
  } else {
    dialog = new StdioWindow(parent, parent->color_scheme());
  }
  dialog->Bkgd(parent->color_scheme()->GetAttributesForScheme(SchemeId::DIALOG_BOX));
  dialog->SetColor(SchemeId::DIALOG_BOX);
  dialog->Box(0, 0);
  return dialog;
}

bool dialog_yn(CursesWindow* window, const std::vector<std::string>& text) {
  auto maxlen = 4;
  for (const auto& s : text) {
    maxlen = std::max<int>(maxlen, size_int(s));
  }
  std::unique_ptr<UIWindow> dialog(CreateDialogWindow(window, size_int(text), maxlen));
  dialog->SetColor(SchemeId::DIALOG_TEXT);
  auto curline = 1;
  for (const auto& s : text) {
    dialog->PutsXY(2, curline++, s);
  }
  dialog->SetColor(SchemeId::DIALOG_PROMPT);
  dialog->Refresh();
  return toupper(dialog->GetChar()) == 'Y';
}

bool dialog_yn(CursesWindow* window, const std::string& text) {
  const std::vector<std::string> text_vector = {text};
  return dialog_yn(window, text_vector);
}

static void winput_password(CursesWindow* dialog, std::string* output, int max_length) {
  dialog->SetColor(SchemeId::DIALOG_PROMPT);

  auto curpos = 0;
  std::string s;
  s.resize(max_length);
  output->clear();
  for (;;) {
    const auto ch = dialog->GetChar();
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
    case 26: // control Z
      break;
    case 27: { // escape
      output->clear();
      return;
    }
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
        s[curpos++] = to_upper_case<char>(static_cast<char>(ch));
        dialog->Putch(ACS_DIAMOND);
      }
      break;
    }
  }
}

void input_password(CursesWindow* window, const std::string& prompt, const std::vector<std::string>& text,
                    std::string* output, int max_length) {
  auto maxlen = size_int(prompt) + max_length;
  for (const auto& s : text) {
    maxlen = std::max<int>(maxlen, size_int(s));
  }
  CHECK(window->IsGUI()) << "input_password needs a GUI.";
  std::unique_ptr<CursesWindow> dialog(
      dynamic_cast<CursesWindow*>(CreateDialogWindow(window, size_int(text) + 2, maxlen)));
  dialog->SetColor(SchemeId::DIALOG_TEXT);

  auto curline = 1;
  for (const auto& s : text) {
    dialog->PutsXY(2, curline++, s);
  }
  dialog->SetColor(SchemeId::DIALOG_PROMPT);
  dialog->PutsXY(2, size_int(text) + 2, prompt);
  dialog->Refresh();
  winput_password(dialog.get(), output, max_length);
}

int messagebox(UIWindow* window, const std::string& text) {
  const std::vector<std::string> vector = {text};
  return messagebox(window, vector);
}

int messagebox(UIWindow* window, const std::vector<std::string>& text) {
  const std::string prompt = "Press Any Key";
  auto maxlen = size_int(prompt) + 2;
  for (const auto& s : text) {
    maxlen = std::max<int>(maxlen, size_int(s));
  }
  std::unique_ptr<UIWindow> dialog(CreateDialogWindow(window, size_int(text) + 2, maxlen));
  dialog->SetColor(SchemeId::DIALOG_TEXT);
  auto curline = 1;
  for (const auto& s : text) {
    dialog->PutsXY(2, curline++, s);
  }
  dialog->SetColor(SchemeId::DIALOG_PROMPT);
  const auto x = (maxlen - ssize(prompt)) / 2;
  dialog->PutsXY(x + 2, size_int(text) + 2, prompt);
  dialog->Refresh();
  return dialog->GetChar();
}

int input_select_item(UIWindow* window, const std::string& prompt, const std::vector<std::string>& items) {
  auto maxlen = size_int(prompt) + 2;
  for (const auto& s : items) {
    maxlen = std::max<int>(maxlen, size_int(s));
  }
  std::unique_ptr<UIWindow> dialog(CreateDialogWindow(window, size_int(items) + 2, maxlen));
  dialog->SetColor(SchemeId::DIALOG_TEXT);
  auto curline = 1;
  int index = 0;
  std::string allowed("Q");
  for (const auto& s : items) {
    dialog->PutsXY(2, curline++, fmt::format("{}) {}", index, s));
    allowed.push_back(static_cast<char>('0' + index));
    ++index;
  }
  dialog->SetColor(SchemeId::DIALOG_PROMPT);
  const auto x = (maxlen - ssize(prompt)) / 2;
  dialog->PutsXY(x + 2, size_int(items) + 2, prompt);
  dialog->Refresh();
  return onek(window, allowed, false);
}

std::string dialog_input_string(CursesWindow* window, const std::string& prompt,
                                int max_length) {
  std::unique_ptr<UIWindow> dialog(CreateDialogWindow(window, 3, size_int(prompt) + 4 + max_length));
  dialog->PutsXY(2, 2, prompt);
  dialog->Refresh();

  CHECK(window->IsGUI()) << "dialog_input_string needs a GUI.";
  std::string s;
  editline(dynamic_cast<CursesWindow*>(dialog.get()), &s, max_length, EditLineMode::ALL, "");
  return s;
}

static int max_length_for_number(int64_t n) {
  return (n == 0) ? 1 : static_cast<int>(std::floor(std::log10(std::abs(n)))) + 1;
}

int dialog_input_number(CursesWindow* window, const std::string& prompt, int min_value, int max_value) {
  const auto num_digits = max_length_for_number(max_value);
  CHECK(window->IsGUI()) << "dialog_input_number needs a GUI.";
  std::unique_ptr<CursesWindow> dialog(
      dynamic_cast<CursesWindow*>(CreateDialogWindow(window, 3, size_int(prompt) + 4 + num_digits)));
  dialog->PutsXY(2, 2, prompt);
  dialog->Refresh();

  std::string s;
  editline(dialog.get(), &s, num_digits, EditLineMode::NUM_ONLY, "");
  if (s.empty()) {
    return min_value;
  }
  try {
    const auto v = std::stoi(s);
    if (v < min_value) {
      return min_value;
    }
    return v;
  } catch (std::logic_error&) {
    // No conversion possible.
    return min_value;
  }
}

int onek(UIWindow* window, const std::string& allowed, bool allow_keycodes) {
  for (;;) {
    const auto key = window->GetChar();
    if (has_key(key)) {
      if (allow_keycodes) {
        return key;
      }
      continue;
    }
    const auto ch = static_cast<char>(std::toupper(key));
    if (allowed.find(ch) != std::string::npos) {
      return ch;
    }
  }
}


static const int background_character = 32;
;

static int editlinestrlen(char* text) {
  auto i = ssize(text);
  while (i >= 0 && (static_cast<unsigned char>(text[i - 1]) == background_character)) {
    --i;
  }
  return i;
}


EditlineResult editline(CursesWindow* window, std::string* s, int len, EditLineMode status,
                         const char* ss) {
  return editline(window, s, len, status, ss, {});
}

EditlineResult editline(CursesWindow* window, std::string* s, int len, EditLineMode status,
                         const char* ss, edline_validation_fn fn) {
  char buffer[255];
  to_char_array(buffer, *s);
  const auto rc = editline(window, buffer, len, status, ss, fn);
  s->assign(buffer);
  return rc;
}

EditlineResult editline(CursesWindow* window, char* s, int len, EditLineMode status,
                         const char* ss) {
  return editline(window, s, len, status, ss, {});
}

/* Edits a string, doing I/O to the screen only. */
EditlineResult editline(CursesWindow* window, char* s, int len, EditLineMode status,
                         const char* ss, const edline_validation_fn& fn) {
  uint32_t old_attr;
  short old_pair;
  window->AttrGet(&old_attr, &old_pair);
  const auto cx = window->GetcurX();
  const auto cy = window->GetcurY();
  auto rc = EditlineResult::NEXT;
  for (auto i = ssize(s); i < len; i++) {
    s[i] = static_cast<char>(background_character);
  }
  s[len] = '\0';
  window->SetColor(SchemeId::EDITLINE);
  window->Puts(s);
  curses_out->SetIndicatorMode(IndicatorMode::overwrite);
  window->GotoXY(cx, cy);
  auto done = false;
  auto pos = 0;
  auto bInsert = false;
  const auto timeout = std::chrono::seconds((fn) ? 1 : 60*60*24);
  do {
    const auto raw_ch = window->GetChar(timeout);
    if (fn && raw_ch == ERR) {
      // We have a timeout, invoke the validation function if we have one.
      fn(s);
      // reset the color since the validator may have changed it.
      window->SetColor(SchemeId::EDITLINE);
      continue;
    }
    switch (raw_ch) {
    case KEY_F(1): // curses
      done = true;
      rc = EditlineResult::DONE;
      break;
    case KEY_HOME: // curses
      pos = 0;
      window->GotoXY(cx, cy);
      break;
    case KEY_END: // curses
      pos = editlinestrlen(s);
      window->GotoXY(cx + pos, cy);
      break;
    case KEY_RIGHT:    // curses
      if (pos < len) { // right
        const auto mp = editlinestrlen(s);
        if (pos < mp) {
          pos++;
          window->GotoXY(cx + pos, cy);
        }
      }
      break;
    case KEY_LEFT:   // curses
      if (pos > 0) { // left
        pos--;
        window->GotoXY(cx + pos, cy);
      }
      break;
    case io::CO:     // return
    case KEY_UP: // curses
      done = true;
      rc = EditlineResult::PREV;
      break;
    case KEY_DOWN: // curses
      done = true;
      rc = EditlineResult::NEXT;
      break;
#ifdef __PDCURSES__
    // This is a PD-CURSES only key, ncurses doesn't support alt keys
    // directly.
    case ALT_I:
#endif
    case KEY_IC: // curses
      if (status != EditLineMode::SET) {
        if (bInsert) {
          bInsert = false;
          curses_out->SetIndicatorMode(IndicatorMode::overwrite);
          window->GotoXY(cx + pos, cy);
        } else {
          bInsert = true;
          curses_out->SetIndicatorMode(IndicatorMode::insert);
          window->GotoXY(cx + pos, cy);
        }
      }
      break;
    case KEY_DC: // curses
    case io::CD: // control-d
      if (status != EditLineMode::SET) {
        for (int i = pos; i < len; i++) {
          s[i] = s[i + 1];
        }
        s[len - 1] = static_cast<char>(background_character);
        window->PutsXY(cx, cy, s);
        window->GotoXY(cx + pos, cy);
      }
      break;
    case KEY_PPAGE:
    case KEY_NPAGE:
      // Ignore page up and page down when editing a line.
      break;
    default: {
      if (raw_ch > 31) {
        auto ch = static_cast<char>(raw_ch & 0xff);
        if (status == EditLineMode::UPPER_ONLY) {
          ch = to_upper_case_char(ch);
        } else if (status == EditLineMode::LOWER) {
          ch = to_lower_case_char(ch);
        }
        if (status == EditLineMode::SET) {
          ch = to_upper_case<char>(ch);
          if (ch != ' ') {
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
        // ReSharper disable CppRedundantParentheses
        if (pos < len &&
            (status == EditLineMode::ALL || status == EditLineMode::UPPER_ONLY ||
             status == EditLineMode::SET || status == EditLineMode::LOWER ||
             (status == EditLineMode::NUM_ONLY &&
             ((ch >= '0' && ch <= '9') || ch == ' ' || (pos == 0 && ch == '-'))))) {
          if (bInsert) {
            for (auto i = len - 1; i > pos; i--) {
              s[i] = s[i - 1];
            }
            s[pos++] = ch;
            window->PutsXY(cx, cy, s);
            window->GotoXY(cx + pos, cy);
          } else {
            s[pos++] = ch;
            window->Putch(ch);
          }
        }
      }
    } break;
    case KEY_ENTER:
#ifdef PADENTER
    case PADENTER:
#endif
    case io::ENTER: // return
    case io::TAB:
      done = true;
      rc = EditlineResult::NEXT;
      break;
    case io::ESC: // esc
      done = true;
      rc = EditlineResult::DONE;
      break;
    case 0x7f:          // yet some other delete key
    case KEY_BACKSPACE: // curses
    case io::BACKSPACE: // backspace
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
    case io::CA: // control-a
      pos = 0;
      window->GotoXY(cx, cy);
      break;
    case io::CE: // control-e
      pos = editlinestrlen(s);
      window->GotoXY(cx + pos, cy);
      break;
    }
  } while (!done);

  int z = ssize(s);
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

std::vector<std::string>::size_type toggleitem(CursesWindow* window,
                                               std::vector<std::string>::size_type value,
                                               const std::vector<std::string>& strings,
                                               EditlineResult* rc) {
  if (value >= strings.size()) {
    value = 0;
  }

  size_t max_size = 0;
  for (const auto& item : strings) {
    max_size = std::max<std::size_t>(max_size, item.size());
  }

  uint32_t old_attr;
  short old_pair;
  window->AttrGet(&old_attr, &old_pair);
  window->SetColor(SchemeId::EDITLINE);
  const auto cx = window->GetcurX();
  const auto cy = window->GetcurY();
  window->PutsXY(cx, cy, strings.at(value));
  window->GotoXY(cx, cy);
  auto done = false;
  do {
    const auto ch = window->GetChar();
    switch (ch) {
    case KEY_ENTER:
    case io::ENTER:
    case io::TAB:
      done = true;
      *rc = EditlineResult::NEXT;
      break;
    case io::ESC:
    case KEY_F(1): // F1
      done = true;
      *rc = EditlineResult::DONE;
      break;
    case KEY_UP:   // UP
    case KEY_BTAB: // SHIFT-TAB
      done = true;
      *rc = EditlineResult::PREV;
      break;
    case KEY_DOWN: // DOWN
      done = true;
      *rc = EditlineResult::NEXT;
      break;
    default:
      if (ch == 32) {
        value = (value + 1) % strings.size();
        auto s = strings.at(value);
        if (s.size() < max_size) {
          s += std::string(max_size - s.size(), ' ');
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

void trimstrpath(char* s) {
  StringTrimEnd(s);

  if (const auto i = strlen(s); i && (s[i - 1] != File::pathSeparatorChar)) {
    s[i] = File::pathSeparatorChar;
    s[i + 1] = 0;
  }
}

}
