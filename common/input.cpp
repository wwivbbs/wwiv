/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2022, WWIV Software Services             */
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
#include "common/input.h"

#include "common/common_events.h"
#include "common/output.h"
#include "core/eventbus.h"
#include "core/stl.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include "local_io/local_io.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstring>
#include <optional>
#include <set>
#include <string>
#include <vector>

using namespace wwiv::common;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::stl;
using namespace wwiv::strings;

// static global bin.
Input bin;

namespace wwiv::common {

static const char* FILENAME_DISALLOWED = "/\\<>|*?\";:";
static const char* FULL_PATH_NAME_DISALLOWED = "<>|*?\";";

// TODO: put back in high ascii characters after finding proper hex codes
static const std::string valid_letters("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ");

// This needs to work across input1 and input_password
static unsigned char last_input_char;

// TODO(rushfan): Make this a WWIV ini setting.
static const uint8_t input_background_char = 32; // Was '\xB1';

// WWIV color (as in |#n) for the arrows Input1 shows at the edges of the
// field when text has scrolled off. Unset: bright white on the field's
// background color.
// TODO(rushfan): Make this a theme color.
static std::optional<int> input_scroll_arrow_color;

static bool okansi(const wwiv::sdk::User& user) { return user.ansi(); }

Input::Input() { memset(charbuffer, 0, sizeof(charbuffer)); }
Input::~Input() = default;

// This will input an upper-case string
void Input::input(char* out_text, int max_length, bool auto_mpl) {
  input1(out_text, max_length, InputMode::UPPER, true, auto_mpl);
}

// This will input an upper-case string
std::string Input::input(int max_length, bool auto_mpl) {
  const auto line = std::make_unique<char[]>(max_length + 1);
  input(line.get(), max_length, auto_mpl);
  return line.get();
}


static int max_length_for_number(int64_t n) {
  return n == 0 ? 1 : static_cast<int>(std::floor(std::log10(std::abs(n)))) + 1;
}

static bool colorize(bool last_ok, int64_t result, int64_t minv, int64_t maxv) {
  const auto ok = (result >= minv && result <= maxv);
  if (ok != last_ok) {
    bout.RestorePosition();
    bout.SavePosition();
    bout.mpl(max_length_for_number(maxv));
    bout.ansic(ok ? 4 : 6);
    bout.outstr(std::to_string(result));
  }
  return ok;
}

// TODO(rushfan): HACK - Fix this and put back language support
static std::string YesNoString(bool b) {
  if (b) {
   return bout.lang().value("KEY_YES", "Yes"); 
  }
  return bout.lang().value("KEY_NO", "No"); 
}

static char YesNoKey(bool b) {
  return YesNoString(b).front();
}

static void print_yn(bool yes) {
  if (yes) {
    bout.str("YES");
  } else {
    bout.str("NO");
  }
  bout.nl();
}

/* The keyboard is checked for either a Y, N, or C/R to be hit.  C/R is
 * assumed to be the same as a N.  Yes or No is output, and yn is set to
 * zero if No was returned, and bin.yesno() is non-zero if Y was hit.
 */
bool Input::yesno() {
  char ch = 0;

  bout.ansic(1);
  while (!sess().hangup() && (ch = to_upper_case(bin.getkey())) != YesNoKey(true) &&
         ch != YesNoKey(false) && ch != RETURN)
    ;

  const bool mci_enabled = bout.mci_enabled();
  bout.enable_mci();
  if (ch == YesNoKey(true)) {
    print_yn(true);
  } else {
    print_yn(false);
  }
  bout.set_mci_enabled(mci_enabled);
  return ch == YesNoKey(true);
}

/**
 * This is the same as bin.yesno(), except C/R is assumed to be "Y"
 */
bool Input::noyes() {
  char ch = 0;

  bout.ansic(1);
  while (!sess().hangup() && (ch = to_upper_case(bin.getkey())) != YesNoKey(true) &&
         ch != YesNoKey(false) && ch != RETURN)
    ;

  const bool mci_enabled = bout.mci_enabled();
  bout.enable_mci();
  if (ch == YesNoKey(false)) {
    print_yn(false);
  } else {
    print_yn(true);
  }
  bout.set_mci_enabled(mci_enabled);
  return ch == YesNoKey(true) || ch == RETURN;
}

char Input::ynq() {
  char ch = 0;

  bout.ansic(1);
  const auto key_quit = bout.lang().value("KEY_QUIT", "Q").front();
  while (!sess().hangup() && (ch = to_upper_case(bin.getkey())) != YesNoKey(true) &&
         ch != YesNoKey(false) && ch != key_quit && ch != RETURN) {
    // NOP
  }
  const bool mci_enabled = bout.mci_enabled();
  bout.enable_mci();
  if (ch == YesNoKey(true)) {
    ch = 'Y';
    print_yn(true);
  } else if (ch == key_quit) {
    ch = 'Q';
    bout.str("QUIT");
    bout.nl();
  } else {
    ch = 'N';
    print_yn(false);
  }
  bout.set_mci_enabled(mci_enabled);
  return ch;
}

/**
 * Inputs a line of text using an ANSI input field, editing orig_text.
 *
 * The field is one screen row: max_length, or the room left on the row if
 * that's less. Longer text scrolls horizontally, and an arrow at either edge
 * of the field shows there is more text that way. orig_text may be longer
 * than max_length; it can be edited but not made longer.
 *
 * Callers without ANSI get the plain line input (input1), which ignores
 * orig_text.
 */
std::string Input::Input1(const std::string& orig_text, int max_length, bool insert,
                          InputMode mode, bool auto_mpl) {
  max_length = std::max(0, max_length);
  if (!okansi(user())) {
    // input1 writes up to max_length chars plus a terminating null.
    std::string line(static_cast<size_t>(max_length) + 1, '\0');
    input1(line.data(), max_length, mode, true, false);
    line.resize(std::strlen(line.c_str()));
    return line;
  }
  const auto saved_topdata = localIO()->topdata();
  if (localIO()->topdata() != LocalIO::topdata_t::none) {
    localIO()->topdata(LocalIO::topdata_t::none);
    bus().invoke<UpdateTopScreenEvent>();
  }
  const auto saved_topline = localIO()->GetTopLine();
  localIO()->SetTopLine(0);

  // Stay on this row: wrapping would break the cursor positioning below.
  // Fall back to a minimum width if the prompt left almost no room.
  static constexpr int min_field_width = 10;
  const auto room = static_cast<int>(user().screen_width()) - 1 - bout.wherex();
  const auto width = std::max(1, std::min(max_length, std::max(min_field_width, room)));
  InputLineEditor ed(orig_text, max_length, width, mode, insert);

  if (auto_mpl) {
    bout.ansic(4);
  }
  const auto field_attr = bout.curatr();
  const auto arrow_attr = input_scroll_arrow_color
                              ? user().color(input_scroll_arrow_color.value())
                              : static_cast<uint8_t>((field_attr & 0xF0) | 0x0F);

  // What's on screen in each cell of the field, so only changed cells are
  // redrawn. Without auto_mpl the caller drew the field, so start from empty.
  using Cell = InputLineEditor::Cell;
  using CellKind = InputLineEditor::CellKind;
  const Cell unknown{CellKind::text, '\0'};
  std::vector<Cell> drawn(width, auto_mpl ? unknown : Cell{});

  bout.SavePosition();
  auto redraw = [&] {
    auto first = -1;
    auto last = -1;
    for (auto i = 0; i < width; i++) {
      if (ed.cell(i) != drawn[i]) {
        if (first < 0) {
          first = i;
        }
        last = i;
      }
    }
    if (first >= 0) {
      bout.RestorePosition();
      bout.SavePosition();
      bout.Right(first);
      for (auto i = first; i <= last; i++) {
        const auto c = ed.cell(i);
        const auto arrow = c.kind == CellKind::more_left || c.kind == CellKind::more_right;
        const auto attr = arrow ? arrow_attr : field_attr;
        if (bout.curatr() != attr) {
          bout.setc(attr);
        }
        bout.outchr(c.kind == CellKind::empty ? static_cast<char>(input_background_char) : c.ch);
        drawn[i] = c;
      }
      if (bout.curatr() != field_attr) {
        bout.setc(field_attr);
      }
    }
    bout.RestorePosition();
    bout.SavePosition();
    bout.Right(ed.cursor_cell());
  };

  auto done = false;
  do {
    redraw();
    auto c = bin.bgetch_event(wwiv::common::Input::numlock_status_t::NUMBERS);
    last_input_char = static_cast<char>(c & 0xff);
    switch (c) {
    case CX:  // Control-X
    case ESC: // ESC
      ed.clear();
      break;
    case COMMAND_LEFT: // Left Arrow
    case CS:
      ed.left();
      break;
    case COMMAND_RIGHT: // Right Arrow
    case CD:
      ed.right();
      break;
    case CA:
    case COMMAND_HOME: // Home
    case CW:
      ed.home();
      break;
    case CE:
    case COMMAND_END: // End
    case CP:
      ed.end();
      break;
    case COMMAND_INSERT: // Insert
      ed.toggle_insert();
      break;
    case COMMAND_DELETE: // Delete
    case CG:
      ed.del();
      break;
    case BACKSPACE:
      ed.backspace();
      break;
    case RETURN:
      done = true;
      break;
    default:
      if (c > 31 && c < 255) {
        ed.type(static_cast<unsigned char>(c));
      }
      break;
    }
    bus().invoke<CheckForHangupEvent>();
  } while (!done && !sess().hangup());

  localIO()->topdata(saved_topdata);
  localIO()->SetTopLine(saved_topline);

  bout.ansic(0);
  bout.nl();
  return ed.text();
}

/**
 * This will input a line of data, maximum max_length characters long, terminated
 * by a C/R.  if (lc) is non-zero, lowercase is allowed, otherwise all
 * characters are converted to uppercase.
 * @param out_text the text entered by the user (output value)
 * @param max_length Maximum length to allow for the input text
 * @param lc The case to return, this can be InputMode::UPPER, InputMode::MIXED, InputMode::PROPER,
 * or InputMode::FILENAME
 * @param crend output the CR/LF if one is entered.
 * @param auto_mpl Call bout.mpl(max_length) automatically.
 */
void Input::input1(char* out_text, int max_length, InputMode lc, bool crend, bool auto_mpl) {
  if (auto_mpl) {
    bout.mpl(max_length);
  }

  auto curpos = 0, in_ansi = 0;
  auto done = false;

  while (!done && !sess().hangup()) {
    unsigned char chCurrent = bin.getkey();

    sess().chatline(curpos != 0);

    if (in_ansi) {
      if (in_ansi == 1 && chCurrent != '[') {
        in_ansi = 0;
      } else {
        if (in_ansi == 1) {
          in_ansi = 2;
        } else {
          if ((chCurrent < '0' || chCurrent > '9') && chCurrent != ';') {
            in_ansi = 3;
          } else {
            in_ansi = 2;
          }
        }
      }
    }
    if (!in_ansi) {
      if (chCurrent > 31) {
        switch (lc) {
        case InputMode::UPPER:
          chCurrent = upcase(chCurrent);
          break;
        case InputMode::MIXED:
          break;
        case InputMode::PROPER:
          chCurrent = upcase(chCurrent);
          if (curpos) {
            const auto found = valid_letters.find(out_text[curpos - 1]) != std::string::npos;
            if (found || out_text[curpos - 1] == 39) {
              if (curpos < 2 || out_text[curpos - 2] != 77 || out_text[curpos - 1] != 99) {
                chCurrent = locase(chCurrent);
              }
            }
          }
          break;
        case InputMode::CMDLINE:
        case InputMode::FILENAME:
        case InputMode::FULL_PATH_NAME: {
          std::string disallowed =
              lc == InputMode::FILENAME ? FILENAME_DISALLOWED : FULL_PATH_NAME_DISALLOWED;
          if (disallowed.find(chCurrent) != std::string::npos) {
            chCurrent = 0;
          } else if (lc != InputMode::CMDLINE) {
            // Lowercase filenames on all platforms, not just Win32.
            // Leave command lines alone.
            chCurrent = to_lower_case(chCurrent);
          }
        } break;
        case InputMode::DATE:
        case InputMode::PHONE:
          break;
        }
        if (curpos < max_length && chCurrent) {
          out_text[curpos++] = chCurrent;
          bout.outchr(chCurrent);
        }
      } else {
        switch (chCurrent) {
        case SOFTRETURN:
          if (last_input_char != RETURN) {
            //
            // Handle the "one-off" case where UNIX telnet clients only return
            // '\n' (#10) instead of '\r' (#13).
            //
            out_text[curpos] = '\0';
            done = true;
            if (bout.newline || crend) {
              bout.nl();
            }
          }
          break;
        case CN:
        case RETURN:
          out_text[curpos] = '\0';
          done = true;
          if (bout.newline || crend) {
            bout.nl();
          }
          break;
        case CW: // Ctrl-W
          if (curpos) {
            do {
              curpos--;
              bout.bs();
              if (out_text[curpos] == CZ) {
                bout.bs();
              }
            } while (curpos && out_text[curpos - 1] != SPACE);
          }
          break;
        case CZ:
          break;
        case BACKSPACE:
          if (curpos) {
            curpos--;
            bout.bs();
            if (out_text[curpos] == CZ) {
              bout.bs();
            }
          }
          break;
        case CU:
        case CX:
          while (curpos) {
            curpos--;
            bout.bs();
            if (out_text[curpos] == CZ) {
              bout.bs();
            }
          }
          break;
        case ESC:
          in_ansi = 1;
          break;
        }
        last_input_char = chCurrent;
      }
    }
    if (in_ansi == 3) {
      in_ansi = 0;
    }
  }
  if (sess().hangup()) {
    out_text[0] = '\0';
  }
}

// private
std::string Input::input_password_minimal(int max_length) {
  const char mask_char = okansi(user()) ? '\xFE' : 'X';
  std::string pw;
  bout.mpl(max_length);

  while (!sess().hangup()) {
    unsigned char ch = bin.getkey();

    if (ch > 31) {
      ch = upcase(ch);
      if (ssize(pw) < max_length && ch) {
        pw.push_back(ch);
        bout.outchr(mask_char);
      }
      continue;
    }
    switch (ch) {
    case SOFTRETURN:
      // Handle the case where some telnet clients only return \n vs \r\n
      if (last_input_char != RETURN) {
        bout.nl();
        last_input_char = ch;
        return pw;
      }
      break;
    case CN:
    case RETURN:
      bout.nl();
      last_input_char = ch;
      return pw;
    case CW: // Ctrl-W
      while (!pw.empty() && pw.back() != SPACE) {
        pw.pop_back();
        bout.bs();
      }
      break;
    case BACKSPACE:
      if (!pw.empty()) {
        pw.pop_back();
        bout.bs();
      }
      break;
    case CU:
    case CX:
      while (!pw.empty()) {
        pw.pop_back();
        bout.bs();
      }
      break;
    }
    last_input_char = ch;
  }
  return "";
}

std::string Input::input_password(const std::string& prompt_text, int max_length) {
  if (!prompt_text.empty()) {
    bout.outstr(prompt_text);
  }
  return input_password_minimal(max_length);
}

/**
 * Raw input_number method. Clients should use input_number or  input_number_hotkey instead.
 */
input_result_t<int64_t> Input::input_number_or_key_raw(int64_t cur, int64_t minv, int64_t maxv,
                                                       bool setdefault,
                                                       const std::set<char>& hotkeys) {
  const auto max_length = max_length_for_number(maxv);
  std::string text;
  bout.mpl(max_length);
  auto allowed = hotkeys;
  auto last_ok = false;
  auto keypressed_ever = false;
  allowed.insert(
      {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', '\r', '\n', '\b', '\x15', '\x17', '\x18'});
  bout.SavePosition();
  int64_t result = 0;
  if (setdefault) {
    result = cur;
    text = std::to_string(result);
    last_ok = colorize(last_ok, result, minv, maxv);
  }
  while (!sess().hangup()) {
    auto ch = bin.getkey();
    if (std::isdigit(ch)) {
      if (!keypressed_ever) {
        // We have a digit key, clear the input and start taking the caller's text.
        keypressed_ever = true;
        while (!text.empty()) {
          text.pop_back();
          bout.bs();
        }
      }
      // digit
      if (ssize(text) < max_length && ch) {
        text.push_back(ch);
        bout.outchr(ch);
        result = to_number<int64_t>(text);
        last_ok = colorize(last_ok, result, minv, maxv);
      }
      last_input_char = ch;
      continue;
    }
    keypressed_ever = true;

    if (ch > 31) {
      // Only non-control characters should be upper cased.
      // this way it covers the hotkeys.
      ch = to_upper_case(ch);
    }
    if (!contains(allowed, ch)) {
      continue;
    }
    switch (ch) {
    case '\n':
      // Handle the case where some telnet clients only return \n vs \r\n
      if (last_input_char != '\r') {
        last_input_char = ch;
        if (result >= minv && result <= maxv) {
          bout.nl();
          return {result, '\0'};
        }
      } else {
        last_input_char = ch;
      }
      break;
    case '\r':
      last_input_char = ch;
      if (result >= minv && result <= maxv) {
        bout.nl();
        return {result, '\0'};
      }
      break;
    case '\b':
      last_input_char = ch;
      if (!text.empty()) {
        text.pop_back();
        bout.bs();
      }
      result = to_number<int64_t>(text);
      last_ok = colorize(last_ok, result, minv, maxv);
      break;
    case CW: // Ctrl-W
    case CX: {
      last_input_char = ch;
      while (!text.empty()) {
        text.pop_back();
        bout.bs();
      }
      bout.mpl(max_length);
    } break;
    default:
      // This is a hotkey.
      return {0, ch};
    }
  }
  return {0, 0};
}

std::string Input::input_filename(const std::string& orig_text, int max_length) {
  return Input1(orig_text, max_length, true, InputMode::FILENAME, true);
}

std::string Input::input_filename(int max_length) { return input_filename("", max_length); }

std::string Input::input_path(const std::string& orig_text, int max_length) {
  return Input1(orig_text, max_length, true, InputMode::FULL_PATH_NAME, true);
}

std::string Input::input_path(int max_length) { return input_path("", max_length); }

std::string Input::input_cmdline(const std::string& orig_text, int max_length) {
  return Input1(orig_text, max_length, true, InputMode::CMDLINE, true);
}


std::string Input::input_phonenumber(const std::string& orig_text, int max_length) {
  return Input1(orig_text, max_length, true, InputMode::PHONE, true);
}

std::string Input::input_text(const std::string& orig_text, int max_length) {
  return Input1(orig_text, max_length, true, InputMode::MIXED, true);
}

std::string Input::input_text(const std::string& orig_text, bool mpl, int max_length) {
  if (mpl) {
    return Input1(orig_text, max_length, true, InputMode::MIXED, mpl);
  }
  // input1 writes up to max_length chars plus a terminating null.
  std::string line(static_cast<size_t>(std::max(0, max_length)) + 1, '\0');
  input1(line.data(), max_length, InputMode::MIXED, true, false);
  line.resize(std::strlen(line.c_str()));
  return line;
}

std::string Input::input_text(int max_length) {
  return Input1("", max_length, true, InputMode::MIXED, true);
}

std::string Input::input_upper(const std::string& orig_text, int max_length, bool auto_mpl) {
  return Input1(orig_text, max_length, true, InputMode::UPPER, auto_mpl);
}

std::string Input::input_upper(const std::string& orig_text, int max_length) {
  return Input1(orig_text, max_length, true, InputMode::UPPER, true);
}

std::string Input::input_upper(int max_length) { return input_upper("", max_length); }

std::string Input::input_proper(const std::string& orig_text, int max_length) {
  return Input1(orig_text, max_length, true, InputMode::PROPER, true);
}

std::string Input::input_date_mmddyyyy(const std::string& orig_text) {
  return Input1(orig_text, 10, true, InputMode::DATE, true);
}

void Input::resetnsp() {
  if (nsp() == 1 && !user().pause()) {
    user().toggle_flag(wwiv::sdk::User::pauseOnPage);
  }
  clearnsp();
}

void Input::clearnsp() { nsp_ = 0; }

int Input::nsp() const noexcept { return nsp_; }

void Input::nsp(int n) { nsp_ = n; }

std::optional<ScreenPos> Input::screen_size() {
  bout.SavePosition();
  // Use puts so the local ansi interpretation will work.
  // This should position it one past the end.
  bout.outstr("\x1b[999;999H");
  auto pos = remoteIO()->screen_position();
  bout.RestorePosition();
  if (!pos) {
    return std::nullopt;
  }
  if (pos->x < 20 || pos->y < 10) {
    // If the screensize is less than 20 wide or 10 tall, this seems sus,
    // so fail to detect it by returning a nullopt.
    return std::nullopt;
  }
  return {ScreenPos{pos->x, pos->y}};
}

}