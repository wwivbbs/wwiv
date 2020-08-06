/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
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

#include "bbs/bbsovl3.h"
#include "bbs/com.h"
#include "bbs/confutil.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/utility.h"
#include "bbs/input.h"
#include "core/strings.h"
#include "sdk/subxtr.h"
#include "sdk/files/dirs.h"

using namespace wwiv::strings;

void HopSub() {
  bool abort = false;
  int nc = 0;
  while (has_userconf_to_subconf(nc)) {
    nc++;
  }

  if (okansi()) {
    bout.clear_whole_line();
  } else {
    bout.nl();
  }
  bout << "|#9Enter name or partial name of sub to hop to:\r\n:";
  if (okansi()) {
    bout.newline = false;
  }
  auto partial = ToStringUpperCase(input_text(40));
  if (partial.empty()) {
    return;
  }
  if (!okansi()) {
    bout.nl();
  }

  int c = 0;
  int oc = a()->current_user_sub_conf_num();
  int os = a()->current_user_sub().subnum;

  while ((c < nc) && !abort) {
    if (okconf(a()->user())) {
      setuconf(ConferenceType::CONF_SUBS, c, -1);
    }
    uint16_t i = 0;
    while ((i < a()->subs().subs().size())
            && (a()->usub[i].subnum != -1) && !abort) {
      auto subname = ToStringUpperCase(a()->subs().sub(a()->usub[i].subnum).name);
      if (subname.find(partial) != std::string::npos) {
        if (okansi()) {
          bout.clear_whole_line();
        }
        if (!okansi()) {
          bout.nl();
        }
        bout << "|#5Do you mean \"" << a()->subs().sub(a()->usub[i].subnum).name << "\" (Y/N/Q)? ";
        char ch = onek_ncr("QYN\r");
        if (ch == 'Y') {
          abort = true;
          a()->set_current_user_sub_num(i);
          break;
        } else if (ch == 'Q') {
          abort = true;
          if (okconf(a()->user())) {
            setuconf(ConferenceType::CONF_SUBS, oc, os);
          }
          break;
        }
      }
      ++i;
    }
    c++;
    if (!okconf(a()->user())) {
      break;
    }
  }
  if (okconf(a()->user()) && !abort) {
    setuconf(ConferenceType::CONF_SUBS, oc, os);
  }
}


void HopDir() {
  bool abort = false;

  int nc = 0;
  while (has_userconf_to_dirconf(nc)) {
    nc++;
  }

  if (okansi()) {
    bout.clear_whole_line();
  } else {
    bout.nl();
  }
  bout << "|#9Enter name or partial name of dir to hop to:\r\n:";
  if (okansi()) {
    bout.newline = false;
  }
  auto partial = ToStringUpperCase(input_text(20));
  if (partial.empty()) {
    return;
  }
  if (!okansi()) {
    bout.nl();
  }

  int c = 0;
  auto oc = a()->current_user_dir_conf_num();
  auto os = a()->current_user_dir().subnum;

  while (c < nc && !abort) {
    if (okconf(a()->user())) {
      setuconf(ConferenceType::CONF_DIRS, c, -1);
    }
    uint16_t i = 0;
    while ((i < a()->dirs().size()) && (a()->udir[i].subnum != -1) && (!abort)) {
      auto subname = ToStringUpperCase(a()->dirs()[a()->udir[i].subnum].name);
      if (subname.find(partial) != std::string::npos) {
        if (okansi()) {
          bout.clear_whole_line();
        } else {
          bout.nl();
        }
        bout << "|#5Do you mean \""
             << a()->dirs()[a()->udir[i].subnum].name
             << "\" (Y/N/Q)? ";
        char ch = onek_ncr("QYN\r");
        if (ch == 'Y') {
          abort = true;
          a()->set_current_user_dir_num(i);
          break;
        } else if (ch == 'Q') {
          abort = true;
          if (okconf(a()->user())) {
            setuconf(ConferenceType::CONF_DIRS, oc, os);
          }
          break;
        }
      }
      ++i;
    }
    c++;
    if (!okconf(a()->user())) {
      break;
    }
  }
  if (okconf(a()->user()) && !abort) {
    setuconf(ConferenceType::CONF_DIRS, oc, os);
  }
}


