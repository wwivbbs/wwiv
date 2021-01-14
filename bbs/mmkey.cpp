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
#include "bbs/mmkey.h"

#include "bbsovl3.h"
#include "bbs/bbs.h"
#include "common/input.h"
#include "common/output.h"
#include "core/log.h"
#include "core/stl.h"
#include "core/strings.h"
#include "local_io/keycodes.h"
#include <set>
#include <string>

using std::string;
using namespace wwiv::strings;
using namespace wwiv::stl;

static bool mmkey_use_bgetch_event = true;

static char mmkey_getch() {
  if (!mmkey_use_bgetch_event) {
    return bin.getkey(false);
  }
  const auto bge = bin.bgetch_event(wwiv::common::Input::numlock_status_t::NUMBERS);
  if (do_sysop_command(bge)) {
    // Handle local sysop commands.
    return 0;
  }
  if (bge == COMMAND_DELETE || bge == COMMAND_LEFT) {
    return BACKSPACE;
  }
  if (bge >= 255) {
    return 0;
  }
  return static_cast<char>(bge & 0xff);
}

static int max_sub_key(std::vector<usersubrec>& container) {
  auto key = 0;
  for (const auto& r : container) {
    const auto current_key = to_number<int>(r.keys);
    if (current_key == 0) {
      return key;
    }
    key = current_key;
  }
  return key;
}

string mmkey(std::set<char>& x, std::set<char>& xx, bool bListOption) {
  unsigned char ch;
  string cmd1;

  do {
    do {
      ch = mmkey_getch();
      if (bListOption && (ch == RETURN || ch == SPACE)) {
        ch = upcase(ch);
        return string(1, ch);
      }
    } while ((ch <= ' ' || ch > 126) && !a()->sess().hangup());
    ch = upcase(ch);
    bout.bputch(ch);
    if (ch == RETURN) {
      cmd1.clear();
    } else {
      cmd1 = ch;
    }
    int p = 0;
    if (contains(xx, ch)) {
      p = 2;
    } else if (contains(x, ch)) {
      p = 1;
    } else {
      bout.nl();
      bout.newline = true;
      return cmd1;
    }
    
    int cp = 1;
    do {
      do {
        ch = mmkey_getch();
      } while (((ch < ' ' && ch != RETURN && ch != BACKSPACE) || ch > 126) && !a()->sess().hangup());
      ch = upcase(ch);
      if (ch == RETURN) {
        bout.nl();
        bout.newline = true;
        return cmd1;
      }
      if (ch == BACKSPACE) {
        bout.bs();
        cmd1.pop_back();
        cp--;
      } else {
        cmd1.push_back(ch);
        bout.bputch(ch);
        cp++;
        if (ch == '/' && cmd1[0] == '/') {
          bout.newline = true;
          return bin.input_upper(50);
        }
        if (cp == p + 1) {
          bout.nl();
          bout.newline = true;
          return cmd1;
        }
      }
    } while (cp > 0);
  } while (!a()->sess().hangup());
  return{};
}

string mmkey(std::set<char>& x) {
  std::set<char> xx;
  return mmkey(x, xx, false);
}


std::string mmkey(MMKeyAreaType dl, bool bListOption) {
  std::set<char> x = {'/'};
  std::set<char> xx{};
  switch (dl) {
  case MMKeyAreaType::subs: {
    const auto max_key = max_sub_key(a()->usub);
    for (char i = 1; i <= max_key / 10; i++) {
      x.insert(i + '0');
    }
    for (char i = 1; i <= max_key / 100; i++) {
      xx.insert(i + '0');
    }
  } break;
  case MMKeyAreaType::dirs: {
    const auto max_key = max_sub_key(a()->udir);
    for (char i = 1; i <= max_key / 10; i++) {
      x.insert(i + '0');
    }
    for (char i = 1; i <= max_key / 100; i++) {
      xx.insert(i + '0');
    }
  } break;
  default: {
    DLOG(FATAL) << "invalid mmkey dl: " << static_cast<int>(dl);
  }
  }

  return mmkey(x, xx, bListOption);
}
