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

#include "bbs/bbs.h"
#include "bbs/confutil.h"
#include "bbs/utility.h"
#include "common/com.h"
#include "common/input.h"
#include "core/strings.h"
#include "sdk/subxtr.h"
#include "sdk/files/dirs.h"

using namespace wwiv::sdk;
using namespace wwiv::strings;

void HopSub() {
  auto abort = false;
  const auto nc = wwiv::stl::size_int(a()->uconfsub);

  if (okansi()) {
    bout.clear_whole_line();
  } else {
    bout.nl();
  }
  bout << "|#9Enter name or partial name of sub to hop to:\r\n:";
  if (okansi()) {
    bout.newline = false;
  }
  const auto partial = ToStringUpperCase(bin.input_text(40));
  if (partial.empty()) {
    return;
  }
  if (!okansi()) {
    bout.nl();
  }

  auto c = 0;
  const int oc = a()->sess().current_user_sub_conf_num();
  const int os = a()->current_user_sub().subnum;

  while (c < nc && !abort) {
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
        const auto ch = onek_ncr("QYN\r");
        if (ch == 'Y') {
          abort = true;
          a()->set_current_user_sub_num(i);
          break;
        }
        if (ch == 'Q') {
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

  const auto nc = wwiv::stl::size_int(a()->uconfdir);

  if (okansi()) {
    bout.clear_whole_line();
  } else {
    bout.nl();
  }
  bout << "|#9Enter name or partial name of dir to hop to:\r\n:";
  if (okansi()) {
    bout.newline = false;
  }
  const auto partial = ToStringUpperCase(bin.input_text(20));
  if (partial.empty()) {
    return;
  }
  if (!okansi()) {
    bout.nl();
  }

  auto c = 0;
  const auto oc = a()->sess().current_user_dir_conf_num();
  const auto os = a()->current_user_dir().subnum;

  while (c < nc && !abort) {
    if (okconf(a()->user())) {
      setuconf(ConferenceType::CONF_DIRS, c, -1);
    }
    for (const auto& udir : a()->udir) {
      auto& dir = a()->dirs().dir(udir.subnum);
      auto subname = ToStringUpperCase(dir.name);
      if (subname.find(partial) != std::string::npos) {
        if (okansi()) {
          bout.clear_whole_line();
        } else {
          bout.nl();
        }
        bout.format("|#5Do you mean \"{}\" (Y/N/Q)? ", dir.name);
        const auto ch = onek_ncr("QYN\r");
        if (ch == 'Y') {
          abort = true;
          a()->set_current_user_dir_num(udir.subnum);
          break;
        }
        if (ch == 'Q') {
          abort = true;
          if (okconf(a()->user())) {
            setuconf(ConferenceType::CONF_DIRS, oc, os);
          }
          break;
        }
      }
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


