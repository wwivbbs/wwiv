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


int select_qwk_archiver(qwk_state* qwk_info, int ask) {
  std::string allowed = "Q\r";

  bout.nl(2);
  bout.puts("|#5Select an archiver");
  bout.nl();
  if (ask) {
    bout.puts("|#20|#9) Ask each time\r\n");
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
  bout.puts("|#7(|#1Q=Quit|#7,|#1<CR>=1|#7) Enter # :");

  if (ask) {
    allowed.push_back('0');
  }

  const auto archiver = onek(allowed);

  if (archiver == '\r') {
    return 1;
  }

  if (archiver == 'Q') {
    qwk_info->abort = true;
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

unsigned short select_qwk_protocol(qwk_state *qwk_info) {
  const auto protocol = get_protocol(xfertype::xf_down_temp);
  if (protocol == -1) {
    qwk_info->abort = true;
  }
  return static_cast<unsigned short>(protocol);
}


void modify_bulletins(sdk::qwk_config& qwk_cfg) {
  char s[101], t[101];

  while (!a()->sess().hangup()) {
    bout.nl();
    bout.puts("Add - Delete - ? List - Quit");
    bout.mpl(1);

    const int key = onek("Q\rAD?");

    switch (key) {
    case 'Q':
    case '\r':
      return;
    case 'D': {
      bout.nl();
      bout.puts("Which one?");
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
      bout.puts("Enter complete path to Bulletin");
      bin.input(s, 80);

      if (!core::File::Exists(s)) {
        bout.puts("File doesn't exist, continue?");
        if (!bin.yesno()) {
          break;
        }
      }

      bout.puts("Now enter its bulletin name, in the format BLT-????.???");
      bin.input(t, BNAME_SIZE);

      if (strncasecmp(t, "BLT-", 4) != 0) {
        bout.puts("Improper format");
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
        bout.printf("[%d] %s Is copied over from", x++, b.name);
        bout.nl();
        bout.Color(7);
        bout << std::string(78, '\xCD');
        bout.nl();
        bout << b.path;
        bout.nl();
      }
    } break;
    }
  }
}

bool get_qwk_max_msgs(uint16_t *qwk_max_msgs, uint16_t *max_per_sub) {
  bout.nl();
  bout.puts("|#9(0=Unlimited) Most messages you want per sub? ");
  *max_per_sub = bin.input_number(*max_per_sub);
  bout.puts("|#9(0=Unlimited) Most messages per packet?       ");
  *qwk_max_msgs = bin.input_number(*qwk_max_msgs);
  return true;
}

}
