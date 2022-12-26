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
#include "bbs/qwk/qwk_ui.h"



#include "qwk.h"
#include "qwk_struct.h"
#include "bbs/bbs.h"
#include "bbs/sr.h"
#include "common/com.h"
#include "common/input.h"
#include "common/output.h"
#include "core/strings.h"

using namespace wwiv::core;
using namespace wwiv::strings;

namespace wwiv::bbs::qwk {


int select_qwk_archiver(bool& abort, bool allow_ask_each_time) {
  std::string allowed = "Q\r";

  bout.nl(2);
  bout.pl("|#5Select an archiver");
  if (allow_ask_each_time) {
    bout.pl("|#20|#9) Ask each time");
  }
  auto num = 0;
  for (const auto& arc : a()->arcs) {
    std::string ext = arc.extension;
    ++num;
    if (!StringTrim(ext).empty() && ext != "EXT") {
      allowed.append(std::to_string(num));
      bout.print("|#2{}|#9) {}\r\n", num, ext);
    }
  }
  bout.nl();
  bout.outstr("|#7(|#1Q=Quit|#7,|#1<CR>=1|#7) Enter # :");

  if (allow_ask_each_time) {
    allowed.push_back('0');
  }

  const auto archiver = onek(allowed);

  if (archiver == '\r') {
    // default/first archiver
    return 1;
  }

  if (archiver == 'Q') {
    abort = true;
    // 0 is none.
    return 0;
  }
  return archiver - '0';
}

std::string qwk_which_zip() {
  if (a()->user()->data.qwk_archive == 0) {
    return "ASK";
  }

  if (a()->user()->data.qwk_archive > 4) {
    a()->user()->data.qwk_archive = 0;
    return "ASK";
  }

  if (a()->arcs[a()->user()->data.qwk_archive - 1].extension[0] == '\0') {
    a()->user()->data.qwk_archive = 0;
    return "ASK";
  }

  return a()->arcs[a()->user()->data.qwk_archive - 1].extension;
}

std::string qwk_which_protocol() {
  if (a()->user()->data.qwk_protocol == 1) {
    a()->user()->data.qwk_protocol = 0;
  }

  if (a()->user()->data.qwk_protocol == 0) {
    return std::string("ASK");
  }
  auto prot(prot_name(a()->user()->data.qwk_protocol));
  if (prot.size() > 22) {
    return prot.substr(0, 21);
  }
  return prot;
}

void modify_bulletins(sdk::qwk_config& qwk_cfg) {
  char s[101], t[101];

  while (!a()->sess().hangup()) {
    bout.nl();
    bout.outstr("Add - Delete - ? List - Quit");
    bout.mpl(1);

    const int key = onek("Q\rAD?");

    switch (key) {
    case 'Q':
    case '\r':
      return;
    case 'D': {
      bout.nl();
      bout.outstr("Which one?");
      bout.mpl(2);

      bin.input(s, 2);
      const auto x = strings::to_number<int>(s);
      // Delete the one at the right position.
      if (x >= 0 && x < stl::size_int(qwk_cfg.bulletins)) {
        stl::erase_at(qwk_cfg.bulletins, x);
      }
    } break;
    case 'A': {
      bout.nl();
      bout.outstr("Enter complete path to Bulletin");
      bin.input(s, 80);

      if (!core::File::Exists(s)) {
        bout.outstr("File doesn't exist, continue?");
        if (!bin.yesno()) {
          break;
        }
      }

      bout.outstr("Now enter its bulletin name, in the format BLT-????.???");
      bin.input(t, BNAME_SIZE);

      if (strncasecmp(t, "BLT-", 4) != 0) {
        bout.outstr("Improper format");
        break;
      }

      sdk::qwk_bulletin b{t, s};
      qwk_cfg.bulletins.emplace_back(b);
    } break;
    case '?': {
      auto x = 1;
      for (const auto& b : qwk_cfg.bulletins) {
        if (bin.checka()) {
          break;
        }
        bout.print("|#7[{}] |#1{} |#5Is copied over from\r\n", x++, b.name);
        bout.print("|#7{}\r\n{}|#1\r\n", std::string(78, '\xCD'), b.path);
      }
    } break;
    }
  }
}

bool get_qwk_max_msgs(uint16_t *qwk_max_msgs, uint16_t *max_per_sub) {
  bout.nl();
  bout.outstr("|#9(0=Unlimited) Most messages you want per sub? ");
  *max_per_sub = bin.input_number(*max_per_sub);
  bout.outstr("|#9(0=Unlimited) Most messages per packet?       ");
  *qwk_max_msgs = bin.input_number(*qwk_max_msgs);
  return true;
}

}
