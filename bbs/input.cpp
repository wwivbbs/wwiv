/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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
#include "bbs/input.h"

#include <algorithm>
#include <string>

#include "core/strings.h"
#include "core/wwivassert.h"
#include "core/wwivport.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl3.h"
#include "bbs/bputch.h"
#include "bbs/com.h"
#include "bbs/keycodes.h"
#include "bbs/utility.h"
#include "bbs/wconstants.h"
#include "bbs/wsession.h"
#include "bbs/vars.h"

using std::string;
using wwiv::bbs::InputMode;

static const char* FILENAME_DISALLOWED = "/\\<>|*?\";:";
static const char* FULL_PATH_NAME_DISALLOWED = "<>|*?\";";

// TODO: put back in high ascii characters after finding proper hex codes
static const unsigned char *valid_letters =
  (unsigned char *) "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";


/**
 * This will input a line of data, maximum max_length characters long, terminated
 * by a C/R.  if (lc) is non-zero, lowercase is allowed, otherwise all
 * characters are converted to uppercase.
 * @param out_text the text entered by the user (output value)
 * @param max_length Maximum length to allow for the input text
 * @param lc The case to return, this can be InputMode::UPPER, InputMode::MIXED, InputMode::PROPER, or InputMode::FILENAME
 * @param crend output the CR/LF if one is entered.
 * @param auto_mpl Call bout.mpl(max_length) automatically.
 */
static void input1(char *out_text, int max_length, InputMode lc, bool crend, bool auto_mpl) {
  if (auto_mpl) {
    bout.mpl(max_length);
  }

  int curpos = 0, in_ansi = 0;
  static unsigned char chLastChar;
  bool done = false;

  while (!done && !hangup) {
    unsigned char chCurrent = getkey();

    if (curpos) {
      bChatLine = true;
    } else {
      bChatLine = false;
    }

    if (in_ansi) {
      if (in_ansi == 1 &&  chCurrent != '[') {
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
            const char *ss = strchr(reinterpret_cast<const char*>(valid_letters), out_text[curpos - 1]);
            if (ss != nullptr || out_text[curpos - 1] == 39) {
              if (curpos < 2 || out_text[curpos - 2] != 77 || out_text[curpos - 1] != 99) {
                chCurrent = locase(chCurrent);
              }
            }
          }
          break;
        case InputMode::FILENAME:
        case InputMode::FULL_PATH_NAME: {
          string disallowed = (lc == InputMode::FILENAME) ? FILENAME_DISALLOWED : FULL_PATH_NAME_DISALLOWED;
          if (strchr(disallowed.c_str(), chCurrent)) {
            chCurrent = 0;
          } else {
#ifdef _WIN32
            chCurrent = upcase(chCurrent);
#endif  // _WIN32
          }
        } break;
        }
        if (curpos < max_length && chCurrent) {
          out_text[curpos++] = chCurrent;
          bputch(chCurrent);
        }
      } else {
        switch (chCurrent) {
        case SOFTRETURN:
          if (chLastChar != RETURN) {
            //
            // Handle the "one-off" case where UNIX telnet clients only return
            // '\n' (#10) instead of '\r' (#13).
            //
            out_text[curpos] = '\0';
            done = true;
            local_echo = true;
            if (newline || crend) {
              bout.nl();
            }
          }
          break;
        case CN:
        case RETURN:
          out_text[curpos] = '\0';
          done = true;
          local_echo = true;
          if (newline || crend) {
            bout.nl();
          }
          break;
        case CW:                          // Ctrl-W
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
        chLastChar = chCurrent;
      }
    }
    if (in_ansi == 3) {
      in_ansi = 0;
    }
  }
  if (hangup) {
    out_text[0] = '\0';
  }
}

// This will input an upper-case string
void input(char *out_text, int max_length, bool auto_mpl) {
  input1(out_text, max_length, InputMode::UPPER, true, auto_mpl);
}

// This will input an upper-case string
string input(int max_length, bool auto_mpl) {
  std::unique_ptr<char[]> line = std::make_unique<char[]>(max_length + 1);
  input(line.get(), max_length, auto_mpl);
  return string(line.get());
}

// This will input an upper or lowercase string of characters
void inputl(char *out_text, int max_length, bool auto_mpl) {
  input1(out_text, max_length, InputMode::MIXED, true, auto_mpl);
}

// This will input an upper or lowercase string of characters
std::string inputl(int max_length, bool auto_mpl) {
  std::unique_ptr<char[]> line = std::make_unique<char[]>(max_length + 1);
  inputl(line.get(), max_length, auto_mpl);
  return string(line.get());
}

std::string input_password(const string& prompt_text, int max_length) {
  bout << prompt_text;
  local_echo = false;

  std::unique_ptr<char[]> line = std::make_unique<char[]>(max_length + 1);

  input1(line.get(), max_length, InputMode::UPPER, true, false);
  return string(line.get());
}

// TODO(rushfan): Make this a WWIV ini setting.
static const uint8_t input_background_char = 32; // Was '\xB1';

//==================================================================
// Function: Input1
//
// Purpose:  Input a line of text of specified length using a ansi
//           formatted input line
//
// Parameters:  *out_text   = variable to save the input to
//              *orgiText     = line to edit.  appears in edit box
//              max_length      = max characters allowed
//              bInsert     = insert mode false = off, true = on
//              mode      = formatting mode.
//
// Returns: length of string
//==================================================================
void Input1(char *out_text, const string& orig_text, int max_length, bool bInsert, InputMode mode) {
  char szTemp[255];
  const char dash = '-';
  const char slash = '/';

#if defined( __unix__ )
  input1(szTemp, max_length, mode, true, false);
  strcpy(out_text, szTemp);
  return;
#endif // __unix__

  if (!okansi()) {
    input1(szTemp, max_length, mode, true, false);
    strcpy(out_text, szTemp);
    return;
  }
  int nTopDataSaved = session()->topdata;
  if (session()->topdata != LocalIO::topdataNone) {
    session()->topdata = LocalIO::topdataNone;
    session()->UpdateTopScreen();
  }
  if (mode == InputMode::DATE || mode == InputMode::PHONE) {
    bInsert = false;
  }
  int nTopLineSaved = session()->localIO()->GetTopLine();
  session()->localIO()->SetTopLine(0);
  int pos = 0;
  int nLength = 0;
  szTemp[0] = '\0';

  max_length = std::min<int>(max_length, 80);
  bout.Color(4);
  int x = session()->localIO()->WhereX() + 1;
  int y = session()->localIO()->WhereY() + 1;

  bout.GotoXY(x, y);
  for (int i = 0; i < max_length; i++) {
    bout << input_background_char;
  }
  bout.GotoXY(x, y);
  if (!orig_text.empty()) {
    strcpy(szTemp, orig_text.c_str());
    bout << szTemp;
    bout.GotoXY(x, y);
    pos = nLength = strlen(szTemp);
  }
  x = session()->localIO()->WhereX() + 1;

  bool done = false;
  do {
    bout.GotoXY(pos + x, y);

    int c = get_kb_event(NUMBERS);

    switch (c) {
    case CX:                // Control-X
    case ESC:               // ESC
      if (nLength) {
        bout.GotoXY(nLength + x, y);
        while (nLength--) {
          bout.GotoXY(nLength + x, y);
          bputch(input_background_char);
        }
        nLength = pos = szTemp[0] = 0;
      }
      break;
    case COMMAND_LEFT:                    // Left Arrow
    case CS:
      if ((mode != InputMode::DATE) && (mode != InputMode::PHONE)) {
        if (pos) {
          pos--;
        }
      }
      break;
    case COMMAND_RIGHT:                   // Right Arrow
    case CD:
      if ((mode != InputMode::DATE) && (mode != InputMode::PHONE)) {
        if ((pos != nLength) && (pos != max_length)) {
          pos++;
        }
      }
      break;
    case CA:
    case COMMAND_HOME:                    // Home
    case CW:
      pos = 0;
      break;
    case CE:
    case COMMAND_END:                     // End
    case CP:
      pos = nLength;
      break;
    case COMMAND_INSERT:                  // Insert
      if (mode == InputMode::UPPER) {
        bInsert = !bInsert;
      }
      break;
    case COMMAND_DELETE:                  // Delete
    case CG:
      if ((pos == nLength) || (mode == InputMode::DATE) || (mode == InputMode::PHONE)) {
        break;
      }
      for (int i = pos; i < nLength; i++) {
        szTemp[i] = szTemp[i + 1];
      }
      nLength--;
      for (int i = pos; i < nLength; i++) {
        bputch(szTemp[i]);
      }
      bputch(input_background_char);
      break;
    case BACKSPACE:                               // Backspace
      if (pos) {
        if (pos != nLength) {
          if ((mode != InputMode::DATE) && (mode != InputMode::PHONE)) {
            for (int i = pos - 1; i < nLength; i++) {
              szTemp[i] = szTemp[i + 1];
            }
            pos--;
            nLength--;
            bout.GotoXY(pos + x, y);
            for (int i = pos; i < nLength; i++) {
              bputch(szTemp[i]);
            }
            bout << input_background_char;
          }
        } else {
          bout.GotoXY(pos - 1 + x, y);
          bout << input_background_char;
          pos = --nLength;
          if (((mode == InputMode::DATE) && ((pos == 2) || (pos == 5))) ||
              ((mode == InputMode::PHONE) && ((pos == 3) || (pos == 7)))) {
            bout.GotoXY(pos - 1 + x, y);
            bout << input_background_char;
            pos = --nLength;
          }
        }
      }
      break;
    case RETURN:                              // Enter
      done = true;
      break;
    default:                              // All others < 256
      if (c < 255 && c > 31 && ((bInsert && nLength < max_length) || (!bInsert && pos < max_length))) {
        if (mode != InputMode::MIXED && mode != InputMode::FILENAME && mode != InputMode::FULL_PATH_NAME) {
          c = upcase(static_cast<unsigned char>(c));
        }
        if (mode == InputMode::FILENAME || mode == InputMode::FULL_PATH_NAME) {
#ifdef _WIN32
          // Only uppercase filenames on Win32.
          c = wwiv::UpperCase<unsigned char> (static_cast<unsigned char>(c)); 
#endif  // _WIN32
          if (mode == InputMode::FILENAME && strchr("/\\<>|*?\";:", c)) {
            c = 0;
          } else if (mode == InputMode::FILENAME && strchr("<>|*?\";", c)) {
            c = 0;
          }  
        }
        if (mode == InputMode::PROPER && pos) {
          const char *ss = strchr(reinterpret_cast<char*>(const_cast<unsigned char*>(valid_letters)), c);
          // if it's a valid char and the previous char was a space
          if (ss != nullptr && szTemp[pos - 1] != 32) {
            c = locase(static_cast<unsigned char>(c));
          }
        }
        if (mode == InputMode::DATE && (pos == 2 || pos == 5)) {
          bputch(slash);
          for (int i = nLength++; i >= pos; i--) {
            szTemp[i + 1] = szTemp[i];
          }
          szTemp[pos++] = slash;
          bout.GotoXY(pos + x, y);
          bout << &szTemp[pos];
        }
        if (mode == InputMode::PHONE && (pos == 3 || pos == 7)) {
          bputch(dash);
          for (int i = nLength++; i >= pos; i--) {
            szTemp[i + 1] = szTemp[i];
          }
          szTemp[pos++] = dash;
          bout.GotoXY(pos + x, y);
          bout << &szTemp[pos];
        }
        if (((mode == InputMode::DATE && c != slash) ||
             (mode == InputMode::PHONE && c != dash)) ||
            (mode != InputMode::DATE && mode != InputMode::PHONE && c != 0)) {
          if (!bInsert || pos == nLength) {
            bputch(static_cast<unsigned char>(c));
            szTemp[pos++] = static_cast<char>(c);
            if (pos > nLength) {
              nLength++;
            }
          } else {
            bputch((unsigned char) c);
            for (int i = nLength++; i >= pos; i--) {
              szTemp[i + 1] = szTemp[i];
            }
            szTemp[pos++] = (char) c;
            bout.GotoXY(pos + x, y);
            bout << &szTemp[pos];
          }
        }
      }
      break;
    }
    szTemp[nLength] = '\0';
    CheckForHangup();
  } while (!done && !hangup);
  if (nLength) {
    strcpy(out_text, szTemp);
  } else {
    out_text[0] = '\0';
  }

  session()->topdata = nTopDataSaved;
  session()->localIO()->SetTopLine(nTopLineSaved);

  bout.Color(0);
  return;
}

string Input1(const string& orig_text, int max_length, bool bInsert, InputMode mode) {
  std::unique_ptr<char[]> line = std::make_unique<char[]>(max_length + 1);
  Input1(line.get(), orig_text, max_length, bInsert, mode);
  return string(line.get());
}

