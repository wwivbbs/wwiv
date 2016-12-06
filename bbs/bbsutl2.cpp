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
#include <sstream>
#include <string>
#include <vector>

#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/fcns.h"
#include "bbs/vars.h"
#include "core/inifile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/filenames.h"

using std::string;
using std::vector;
using wwiv::core::IniFile;
using wwiv::core::FilePath;
using namespace wwiv::stl;
using namespace wwiv::strings;

/**
 * Returns the computer type string for computer type number num.
 *
 * @param num The computer type number for which to return the name
 *
 * @return The text describing computer type num
 */
std::string ctypes(int num) {
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

  IniFile iniFile(FilePath(a()->GetHomeDir(), WWIV_INI), {"CTYPES"});
  if (iniFile.IsOpen()) {
    return iniFile.value<string>(StringPrintf("COMP_TYPE[%d]", num + 1));
  }
  if (num < 0 || num > size_int(default_ctypes)) {
    return "";
  }
  return default_ctypes[num];
}


/**
 * Displays s which checking for abort and next
 * @see checka
 * <em>Note: osan means Output String And Next</em>
 *
 * @param text The text to display
 * @param abort The abort flag (Output Parameter)
 * @param next The next flag (Output Parameter)
 */
void osan(const string& text, bool *abort, bool *next) {
  CheckForHangup();
  checka(abort, next);

  for (auto ch : text) {
    bout.bputch(ch, true);     // RF20020927 use buffered bputch
    if (checka(abort, next) || hangup) {
      break;
    }
  }
  bout.flush();
}

/**
 * @todo Document this
 */
string strip_to_node(const string& txt) {
  std::ostringstream os;
  if (txt.find("@") != string::npos) {
    bool ok = true;
    for (auto i = txt.begin(); i != txt.end(); i++) {
      if (ok) {
        os << *i;
      }
      if ((i + 1) != txt.end() && (i + 2) != txt.end() && *(i + 2) == '#') {
        ok = false;
      }
    }
    return os.str();
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
    return os.str();
  }
  return txt;
}
