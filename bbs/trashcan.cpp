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
#include "bbs/trashcan.h"

#include <algorithm>
#include <chrono>
#include <string>

#include "bbs/asv.h"
#include "bbs/bbs.h"
#include "bbs/bbsovl1.h"
#include "bbs/bbsovl2.h"
#include "bbs/bbsovl3.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/com.h"
#include "bbs/confutil.h"
#include "bbs/defaults.h"
#include "bbs/dupphone.h"
#include "bbs/colors.h"
#include "bbs/defaults.h"
#include "bbs/email.h"
#include "bbs/execexternal.h"
#include "bbs/fcns.h"
#include "bbs/finduser.h"
#include "bbs/datetime.h"
#include "bbs/dropfile.h"
#include "bbs/message_file.h"
#include "bbs/mmkey.h"
#include "bbs/inmsg.h"
#include "bbs/inetmsg.h"
#include "bbs/input.h"
#include "bbs/listplus.h"
#include "bbs/printfile.h"
#include "bbs/stuffin.h"
#include "bbs/uedit.h"
#include "bbs/vars.h"
#include "bbs/wconstants.h"
#include "bbs/workspace.h"
#include "sdk/status.h"
#include "core/inifile.h"
#include "core/os.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "sdk/filenames.h"

using std::chrono::milliseconds;
using std::string;
using wwiv::bbs::InputMode;
using namespace wwiv::core;
using namespace wwiv::os;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;


Trashcan::Trashcan(wwiv::sdk::Config& config)
    : file_(config.gfilesdir(), TRASHCAN_TXT) {
}

Trashcan::~Trashcan() {}

static bool Matches(string whole, string pattern) {
  if (!contains(pattern, '*')) {
    return whole == pattern;
  }

  if (pattern.length() == 1) {
    // We have 1 char and it is a '*'.
    return false;
  }
  StringUpperCase(&pattern);
  if ((pattern.length() > 2) && pattern.front() == '*' && pattern.back() == '*') {
    // We have *foo*
    string s = pattern.substr(1);
    s.pop_back();
    return whole.find(s) != string::npos;
  } else if (pattern.front() == '*') {
    return ends_with(whole, pattern.substr(1));
  } else if (pattern.back() == '*') {
    string s = pattern;
    s.pop_back();
    return starts_with(whole, s);
  } else {
    // Don't have a * at either end, so we don't support that pattern.
    return false;
  }
}

bool Trashcan::IsTrashName(const std::string& rawname) {
  if (!file_.Exists()) {
    return false;
  }
  TextFile file(file_.full_pathname(), "rt");
  if (!file.IsOpen()) {
    return false;
  }

  string name(rawname);
  StringUpperCase(&name);

  string line;
  while (file.ReadLine(&line)) {
    StringUpperCase(&line);
    if (Matches(name, line)) {
      return true;
    }
  }

  return false;
}
