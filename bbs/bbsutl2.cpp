/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
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
#include <string>
#include <vector>

#include "wwiv.h"
#include "core/inifile.h"
#include "core/strings.h"

using std::string;
using std::vector;
using wwiv::core::IniFile;
using wwiv::core::FilePath;
using wwiv::strings::StringPrintf;

/**
 * Display character x repeated amount times in nColor, and if bAddNL is true
 * display a new line character.
 * @param x The Character to repeat
 * @param amount The number of times to repeat x
 * @param nColor the color in which to display the string
 * @param bAddNL if true, add a new line character at the end.
 */
void repeat_char(char x, int amount, int nColor) {
  bout.Color(nColor);
  bout << string(amount, x);
  bout.nl();
}

/**
 * Returns the computer type string for computer type number num.
 *
 * @param num The computer type number for which to return the name
 *
 * @return The text describing computer type num
 */
const char *ctypes(int num) {
  static char szCtype[81];

  // The default list of computer types
  static const vector<string> default_ctypes{
    "IBM PC (8088)",
    "IBM PS/2",
    "IBM AT (80286)",
    "IBM AT (80386)",
    "IBM AT (80486)",
    "Pentium",
    "Apple 2",
    "Apple Mac",
    "Commodore Amiga",
    "Commodore",
    "Atari",
    "Other",
  };

  IniFile iniFile(FilePath(application()->GetHomeDir(), WWIV_INI), "CTYPES");
  if (iniFile.IsOpen()) {
    const string comptype = StringPrintf("COMP_TYPE[%d]", num + 1);
    const char *ss = iniFile.GetValue(comptype.c_str());
    if (ss && *ss) {
      strcpy(szCtype, ss);
      return szCtype;
    }
    return nullptr;
  }
  if ((num < 0) || (num > static_cast<int>(default_ctypes.size()))) {
    return nullptr;
  }
  return default_ctypes[num].c_str();
}


/**
 * Displays s which checking for abort and next
 * @see checka
 * <em>Note: osan means Output String And Next</em>
 *
 * @param pszText The text to display
 * @param abort The abort flag (Output Parameter)
 * @param next The next flag (Output Parameter)
 */
void osan(const string& text, bool *abort, bool *next) {
  CheckForHangup();
  checka(abort, next);

  for (auto ch : text) {
    bputch(ch, true);     // RF20020927 use buffered bputch
    if (checka(abort, next) || hangup) {
      break;
    }
  }
  FlushOutComChBuffer();
}

/**
 * Displays pszText in color nWWIVColor which checking for abort and next with a nl
 * @see checka
 * <em>Note: osan means Output String And Next</em>
 *
 * @param nWWIVColor The WWIV color code to use.
 * @param pszText The text to display
 * @param abort The abort flag (Output Parameter)
 * @param next The next flag (Output Parameter)
 */
void plan(int nWWIVColor, const string& text, bool *abort, bool *next) {
  bout.Color(nWWIVColor);
  osan(text, abort, next);
  if (!(*abort)) {
    bout.nl();
  }
}

/**
 * @todo Document this
 */
string strip_to_node(const string& txt) {
  std::ostringstream os;
  if (txt.find("@") != string::npos) {
    bool ok = true;
    for (string::const_iterator i = txt.begin(); i != txt.end(); i++) {
      if (ok) {
        os << *i;
      }
      if ((i + 1) != txt.end() && (i + 2) != txt.end() && *(i + 2) == '#') {
        ok = false;
      }
    }
    return string(os.str());
  } else if (txt.find("AT") != string::npos) {
    bool ok = true;
    for (string::const_iterator i = txt.begin() + 2; i != txt.end(); i++) {
      if (ok) {
        os << *i;
      }
      if (*(i + 1) == '`') {
        ok = false;
      }
    }
    return string(os.str());
  }
  return string(txt);
}
