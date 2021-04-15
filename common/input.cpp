/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
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
#include <set>
#include <string>

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
    bout.Color(ok ? 4 : 6);
    bout.bputs(std::to_string(result));
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

  bout.Color(1);
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

  bout.Color(1);
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

  bout.Color(1);
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

//==================================================================
// Function: Input1
//
// Purpose:  Input a line of text of specified length using a ansi
//           formatted input line
//
// Parameters:  *out_text   = variable to save the input to
//              *orig_text     = line to edit.  appears in edit box
//              max_length      = max characters allowed
//              insert     = insert mode false = off, true = on
//              mode      = formatting mode.
//
// Returns: length of string
//==================================================================
void Input::Input1(char* out_text, const std::string& orig_text, int max_length, bool insert,
                   InputMode mode, bool auto_mpl) {
  char szTemp[255];
  static const auto dash = '-';
  static const auto slash = '/';

  if (!okansi(user())) {
    input1(szTemp, max_length, mode, true, false);
    strcpy(out_text, szTemp);
    return;
  }
  const auto saved_topdata = localIO()->topdata();
  if (localIO()->topdata() != LocalIO::topdata_t::none) {
    localIO()->topdata(LocalIO::topdata_t::none);
    bus().invoke<UpdateTopScreenEvent>();
  }
  if (mode == InputMode::DATE || mode == InputMode::PHONE) {
    insert = false;
  }
  const auto saved_topline = localIO()->GetTopLine();
  localIO()->SetTopLine(0);
  auto pos = 0;
  auto nLength = 0;
  szTemp[0] = '\0';

  max_length = std::min<int>(max_length, 78);

  bout.SavePosition();
  if (auto_mpl) {
    bout.Color(4);
    for (auto i = 0; i < max_length; i++) {
      bout.bputch(input_background_char);
    }
    bout.RestorePosition();
    bout.SavePosition();
  }
  if (!orig_text.empty()) {
    to_char_array(szTemp, orig_text);
    bout << szTemp;
    bout.RestorePosition();
    bout.SavePosition();
    pos = nLength = strings::ssize(szTemp);
  }

  auto done = false;
  do {
    bout.RestorePosition();
    bout.SavePosition();
    bout.Right(pos);

    auto c = bin.bgetch_event(wwiv::common::Input::numlock_status_t::NUMBERS);
    last_input_char = static_cast<char>(c & 0xff);
    switch (c) {
    case CX:  // Control-X
    case ESC: // ESC
      if (nLength) {
        bout.RestorePosition();
        bout.SavePosition();
        bout.Right(nLength);
        while (nLength--) {
          bout.RestorePosition();
          bout.SavePosition();
          bout.Right(nLength);
          bout.bputch(input_background_char);
        }
        nLength = pos = szTemp[0] = 0;
      }
      break;
    case COMMAND_LEFT: // Left Arrow
    case CS:
      if ((mode != InputMode::DATE) && (mode != InputMode::PHONE)) {
        if (pos) {
          pos--;
        }
      }
      break;
    case COMMAND_RIGHT: // Right Arrow
    case CD:
      if (mode != InputMode::DATE && mode != InputMode::PHONE) {
        if (pos != nLength && pos != max_length) {
          pos++;
        }
      }
      break;
    case CA:
    case COMMAND_HOME: // Home
    case CW:
      pos = 0;
      break;
    case CE:
    case COMMAND_END: // End
    case CP:
      pos = nLength;
      break;
    case COMMAND_INSERT: // Insert
      if (mode == InputMode::UPPER) {
        insert = !insert;
      }
      break;
    case COMMAND_DELETE: // Delete
    case CG:
      if (pos == nLength || mode == InputMode::DATE || mode == InputMode::PHONE) {
        break;
      }
      for (auto i = pos; i < nLength; i++) {
        szTemp[i] = szTemp[i + 1];
      }
      nLength--;
      for (auto i = pos; i < nLength; i++) {
        bout.bputch(szTemp[i]);
      }
      bout.bputch(input_background_char);
      break;
    // Backspace (0x08)
    case BACKSPACE:
      if (pos) {
        if (pos != nLength) {
          if (mode != InputMode::DATE && mode != InputMode::PHONE) {
            for (int i = pos - 1; i < nLength; i++) {
              szTemp[i] = szTemp[i + 1];
            }
            pos--;
            nLength--;
            bout.RestorePosition();
            bout.SavePosition();
            bout.Right(pos);
            for (int i = pos; i < nLength; i++) {
              bout.bputch(szTemp[i]);
            }
            bout << input_background_char;
          }
        } else {
          bout.RestorePosition();
          bout.SavePosition();
          bout.Right(pos - 1);
          bout << input_background_char;
          pos = --nLength;
          // ReSharper disable once CppRedundantParentheses
          if ((mode == InputMode::DATE && (pos == 2 || pos == 5)) ||
              (mode == InputMode::PHONE && (pos == 3 || pos == 7))) {
            bout.RestorePosition();
            bout.SavePosition();
            bout.Right(pos - 1);
            bout << input_background_char;
            pos = --nLength;
          }
        }
      }
      break;
      // Enter
    case RETURN:
      done = true;
      break;
    // All others < 256
    default:
      if (c < 255 && c > 31 &&
          (insert && nLength < max_length || !insert && pos < max_length)) {
        if (mode != InputMode::MIXED && mode != InputMode::FILENAME && mode != InputMode::CMDLINE &&
            mode != InputMode::FULL_PATH_NAME) {
          c = upcase(static_cast<unsigned char>(c));
        }
        if (mode == InputMode::FILENAME || mode == InputMode::FULL_PATH_NAME ||
            mode == InputMode::CMDLINE) {
          if (mode == InputMode::FILENAME || mode == InputMode::FULL_PATH_NAME) {
            // Force lowercase filenames but not command lines.
            c = to_lower_case<unsigned char>(static_cast<unsigned char>(c));
          }
          if (mode == InputMode::FILENAME && strchr("/\\<>|*?\";:", c)) {
            c = 0;
          } else if (mode == InputMode::FILENAME && strchr("<>|*?\";", c)) {
            c = 0;
          }
        }
        if (mode == InputMode::PROPER && pos) {
          const auto cc = static_cast<unsigned char>(c);
          const auto found = valid_letters.find(cc) != std::string::npos;
          // if it's a valid char and the previous char was a space
          if (found && szTemp[pos - 1] != 32) {
            c = locase(cc);
          }
        }
        if (mode == InputMode::DATE && (pos == 2 || pos == 5)) {
          bout.bputch(slash);
          for (auto i = nLength++; i >= pos; i--) {
            szTemp[i + 1] = szTemp[i];
          }
          szTemp[pos++] = slash;
          bout.RestorePosition();
          bout.SavePosition();
          bout.Right(pos);
          bout << &szTemp[pos];
        }
        if (mode == InputMode::PHONE && (pos == 3 || pos == 7)) {
          bout.bputch(dash);
          for (auto i = nLength++; i >= pos; i--) {
            szTemp[i + 1] = szTemp[i];
          }
          szTemp[pos++] = dash;
          bout.RestorePosition();
          bout.SavePosition();
          bout.Right(pos);
          bout << &szTemp[pos];
        }
        // ReSharper disable once CppRedundantParentheses
        if ((mode == InputMode::DATE && c != slash) || (mode == InputMode::PHONE && c != dash) ||
        // ReSharper disable once CppRedundantParentheses
            (mode != InputMode::DATE && mode != InputMode::PHONE && c != 0)) {
          if (!insert || pos == nLength) {
            bout.bputch(static_cast<char>(c));
            szTemp[pos++] = static_cast<char>(c);
            if (pos > nLength) {
              nLength++;
            }
          } else {
            bout.bputch(static_cast<char>(c));
            for (auto i = nLength++; i >= pos; i--) {
              szTemp[i + 1] = szTemp[i];
            }
            szTemp[pos++] = static_cast<char>(c);
            bout.RestorePosition();
            bout.SavePosition();
            bout.Right(pos);
            bout << &szTemp[pos];
          }
        }
      }
      break;
    }
    szTemp[nLength] = '\0';
    bus().invoke<CheckForHangupEvent>();
  } while (!done && !sess().hangup());
  if (nLength) {
    strcpy(out_text, szTemp);
  } else {
    out_text[0] = '\0';
  }

  localIO()->topdata(saved_topdata);
  localIO()->SetTopLine(saved_topline);

  bout.Color(0);
  bout.nl();
}

std::string Input::Input1(const std::string& orig_text, int max_length, bool bInsert, InputMode mode, bool auto_mpl) {
  const auto line = std::make_unique<char[]>(max_length + 1);
  Input1(line.get(), orig_text, max_length, bInsert, mode, auto_mpl);
  return line.get();
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
        }
        if (curpos < max_length && chCurrent) {
          out_text[curpos++] = chCurrent;
          bout.bputch(chCurrent);
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
        bout.bputch(mask_char);
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
    bout << prompt_text;
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
      // digit
      if (ssize(text) < max_length && ch) {
        text.push_back(ch);
        bout.bputch(ch);
        result = to_number<int64_t>(text);
        last_ok = colorize(last_ok, result, minv, maxv);
      }
      last_input_char = ch;
      continue;
    }
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
  if (max_length > 255) {
    return "";
  }
  char s[255];
  input1(s, max_length, InputMode::MIXED, true, false);
  return s;
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
  // Use bputs so the local ansi interpretation will work.
  // This should position it one past the end.
  bout.bputs("\x1b[999;999H");
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