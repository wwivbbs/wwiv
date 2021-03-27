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
#include "bbs/sysopf.h"

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/bbsutl2.h"
#include "bbs/confutil.h"
#include "bbs/connect1.h"
#include "bbs/dropfile.h"
#include "bbs/email.h"
#include "bbs/execexternal.h"
#include "bbs/finduser.h"
#include "bbs/instmsg.h"
#include "bbs/mmkey.h"
#include "bbs/multinst.h"
#include "bbs/read_message.h"
#include "bbs/shortmsg.h"
#include "bbs/stuffin.h"
#include "bbs/sysoplog.h"
#include "bbs/uedit.h"
#include "bbs/wqscn.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/output.h"
#include "core/file.h"
#include "core/inifile.h"
#include "core/scope_exit.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/textfile.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "local_io/wconstants.h"
#include "sdk/arword.h"
#include "sdk/bbslist.h"
#include "sdk/filenames.h"
#include "sdk/names.h"
#include "sdk/status.h"
#include "sdk/user.h"
#include "sdk/usermanager.h"
#include "sdk/msgapi/message_utils_wwiv.h"
#include "sdk/net/networks.h"

#include <memory>
#include <string>

using wwiv::core::IniFile;
using wwiv::core::FilePath;

using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;
using namespace wwiv::sdk::msgapi;

void prstatus() {
  a()->status_manager()->reload_status();
  bout.cls();
  if (!a()->config()->newuser_password().empty()) {
    bout << "|#9New User Pass   : " << a()->config()->newuser_password() << wwiv::endl;
  }
  bout << "|#9Board is        : " << (a()->config()->closed_system() ? "Closed" : "Open") << wwiv::endl;

  const auto status = a()->status_manager()->get_status();
  const auto t = times();
  bout << "|#9Number Users    : |#2" << status->num_users() << wwiv::endl;
  bout << "|#9Number Calls    : |#2" << status->caller_num() << wwiv::endl;
  bout << "|#9Last Date       : |#2" << status->last_date() << wwiv::endl;
  bout << "|#9Time            : |#2" << t.c_str() << wwiv::endl;
  bout << "|#9Active Today    : |#2" << status->active_today_minutes() << wwiv::endl;
  bout << "|#9Calls Today     : |#2" << status->calls_today() << wwiv::endl;
  bout << "|#9Net Posts Today : |#2" << (status->msgs_today() - status->localposts()) << wwiv::endl;
  bout << "|#9Local Post Today: |#2" << status->localposts() << wwiv::endl;
  bout << "|#9E Sent Today    : |#2" << status->email_today() << wwiv::endl;
  bout << "|#9F Sent Today    : |#2" << status->feedback_today() << wwiv::endl;
  bout << "|#9Uploads Today   : |#2" << status->uploads_today() << wwiv::endl;

  auto feedback_waiting = 0;
  if (const auto sysop = a()->users()->readuser(1)) {
    feedback_waiting = sysop->email_waiting();
  }
  bout << "|#9Feedback Waiting: |#2" << feedback_waiting << wwiv::endl;
  bout << "|#9Sysop           : |#2" << ((sysop2()) ? "Available" : "NOT Available") << wwiv::endl;
  bout << "|#9Q-Scan Pointer  : |#2" << status->qscanptr() << wwiv::endl;

  if (num_instances() > 1) {
    multi_instance();
  }
}

// returns true if user deleted.
static bool valuser_delete(int user_number) {
  bout.nl();
  bout << "|#5Delete? ";
  if (bin.yesno()) {
    a()->users()->delete_user(user_number);
    bout << "\r\n|#6Deleted.\r\n\n";
    return true;
  }
  bout << "\r\n|#3NOT deleted.\r\n";
  return false;
}

static void valuser_manual(User& user) {
  bout << "|#9SL  : |#2" << user.sl() << wwiv::endl;
  if (user.sl() < a()->sess().effective_sl()) {
    bout << "|#9New : ";
    const auto sl = bin.input_number(user.sl(), 0, 255);
    user.sl(sl);
  }
  bout.nl();
  bout << "|#9DSL : |#2" << user.dsl() << wwiv::endl;
  if (user.dsl() < a()->user()->dsl()) {
    bout << "|#9New ? ";
    const auto dsl = bin.input_number(user.dsl(), 0, 255);
    user.dsl(dsl);
  }
  bout.nl();
  if (auto allowed_ar = word_to_arstr(a()->user()->ar_int(), ""); !allowed_ar.empty()) {
    allowed_ar.push_back(RETURN);
    do {
      bout << "|#9AR  : |#2" << word_to_arstr(user.ar_int(), "") << wwiv::endl;
      bout << "|#9Togl? ";
      const auto ch_ar = onek(allowed_ar);
      if (ch_ar == RETURN) {
        break;
      }
      const auto ar = ch_ar - 'A';
      user.toggle_ar(1 << ar);
    } while (!a()->sess().hangup());
  }
  bout.nl();
  if (auto allowed_dar = word_to_arstr(a()->user()->dar_int(), ""); !allowed_dar.empty()) {
    allowed_dar.push_back(RETURN);
    do {
      bout << "|#9DAR : |#2" << word_to_arstr(user.dar_int(), "") << wwiv::endl;
      bout << "|#9Togl? ";
      const auto ch_dar = onek(allowed_dar);
      if (ch_dar == RETURN) {
        break;
      }
      const auto dar = ch_dar - 'A';
      user.toggle_dar(1 << dar);
    } while (!a()->sess().hangup());
  }
  bout.nl();

  const std::string r{restrict_string};
  std::string allowed{restrict_string};
  allowed.push_back(' ');
  allowed.push_back(RETURN);
  do {
    std::string user_rs;
    for (auto i = 0; i <= 15; i++) {
      user_rs.push_back(user.has_restrict(1 << i) ? restrict_string[i] : ' ');
    }
    bout << "      |#2" << restrict_string << wwiv::endl;
    bout << "|#9Rstr: |#2" << user_rs << wwiv::endl;
    bout << "|#9Togl? ";
    const auto ch = onek(allowed);
    if (ch != RETURN && ch != SPACE && ch != '?') {
      if (const auto pos = r.find(ch); pos != std::string::npos) {
        user.toggle_restrict(1 << pos);
      }
    }
    if (ch == RETURN) {
      break;
    }
    if (ch == '?') {
      bout.print_help_file(SRESTRCT_NOEXT);
    }
  } while (!a()->sess().hangup());
}

static void valuser_auto(User& user) {
  for (auto i = 1; i <= 10; i++) {
    const auto& v = a()->config()->auto_val(i);
    bout.format("|#2{:>2}|#9) |#1{:<30.30} |#9(SL: |#5{:<3}|#9 DSL: |#5{:<3}|#9)\r\n", i, v.name, v.sl, v.dsl);
  }
  bout.nl();
  bout << "|#9(|#2Q|#9=|#1Quit|#9) Which Auto Validation: ";
  const auto [num, key] = bin.input_number_hotkey(0, {'Q'}, 1, 10);
  if (key == 'Q') {
    return;
  }
  auto_val(num, &user);
}

// TODO(rushfan): This is copied from wwivconfig, hard to share there but
// If we need this somewhere else, should move to SDK class.
static void write_semaphore_if_user_online(const Config& config, int current_usernum) {
  Instances instances(config);
  if (!instances) {
    std::cout << "Unable to read Instance information.";
    return;
  }

  for (const auto& inst : instances) {
    if (!inst.online()) {
      continue;
    }
    if (inst.user_number() != current_usernum) {
      continue;
    }
    // we have user.
    const auto path = config.scratch_dir(inst.node_number());
    const auto fn = FilePath(path, "readuser.wwiv");
    if (TextFile tf(fn, "wt"); tf) {
      tf.Write("User edited in wwivconfig usereditor");
    }
    return;
  }
}

void valuser(int user_number) {

  bout.nl();
  auto o = a()->users()->readuser(user_number);
  if (!o) {
    bout << "\r\n|#6No Such User.\r\n\n";
    return;
  }
  auto user = o.value();
  while (true) {
    bout.cls();
    bout.litebar("WWIV Quick User Validation");
    bout.nl();
    const auto unn = a()->names()->UserName(user_number);
    bout << "|#9Name: |#2" << unn << wwiv::endl;
    if (a()->config()->newuser_config().use_real_name != newuser_item_type_t::unused) {
      bout << "|#9RN  : |#2" << user.real_name() << wwiv::endl;
    }
    if (a()->config()->newuser_config().use_voice_phone != newuser_item_type_t::unused) {
      bout << "|#9PH  : |#2" << user.voice_phone() << wwiv::endl;
    }
    if (a()->config()->newuser_config().use_birthday != newuser_item_type_t::unused) {
      bout << "|#9Age : |#2" << user.age() << " " << user.gender() << wwiv::endl;
    }
    if (a()->config()->newuser_config().use_computer_type != newuser_item_type_t::unused) {
      bout << "|#9Comp: |#2" << ctypes(user.computer_type()) << wwiv::endl;
    }
    if (user.note().empty()) {
      bout << "|#9Note: |#2" << user.note() << wwiv::endl;
    }
    bout << "|#9SL  : |#2" << user.sl() << wwiv::endl;
    bout << "|#9DSL : |#2" << user.sl() << wwiv::endl;
    bout.nl(2);
    bout << "|#9(|#2Q|#9=|#1Quit|#9) (|#2D|#9)elete, (|#2M|#9)anual, (|#2A|#9)utoval : ";
    switch (onek("ADMQ", true)) { 
    case 'A':
      valuser_auto(user);
      write_semaphore_if_user_online(*a()->config(), user.usernum());
      break;
    case 'D':
      if (valuser_delete(user_number)) {
        return;
      }
      break;
    case 'M':
      valuser_manual(user);
      write_semaphore_if_user_online(*a()->config(), user.usernum());
      break;
    case 'Q':
      bout.nl(2);
      return;
    }
    if (!a()->users()->writeuser(&user, user_number)) {
      bout << "|#6Error writing user #" << user_number << wwiv::endl;
      bout.pausescr();
    }
  }
}

enum class net_search_type_t {
  UNKNOWN, NET_SEARCH_SUBSTR, NET_SEARCH_AREACODE, NET_SEARCH_GROUP, NET_SEARCH_SC,
  NET_SEARCH_AC, NET_SEARCH_GC, NET_SEARCH_NC, NET_SEARCH_PHSUBSTR,
  NET_SEARCH_NOCONNECT, NET_SEARCH_ALL };

bool print_wwivnet_net_listing(const net_networks_rec& net) {
  int gn = 0;
  char s2[101], bbstype;

  while (!a()->sess().hangup()) {
    char s[255];
    auto abort = false;
    auto cmdbit = net_search_type_t::UNKNOWN;
    uint16_t slist = 0;
    char substr[81], acstr[4], phstr[13];
    substr[0] = 0;
    acstr[0] = 0;
    phstr[0] = 0;

    bout.cls();
    bout.nl();
    bout << "|#9Network|#2: |#1" << net.name << wwiv::endl;
    bout.nl();

    bout << "|#21|#9) List All\r\n";
    bout << "|#22|#9) Area Code\r\n";
    bout << "|#23|#9) Group\r\n";
    bout << "|#24|#9) Subs Coordinators\r\n";
    bout << "|#25|#9) Area Coordinators\r\n";
    bout << "|#26|#9) Group Coordinators\r\n";
    bout << "|#27|#9) Net Coordinator\r\n";
    bout << "|#28|#9) BBS Name SubString\r\n";
    bout << "|#29|#9) Phone SubString\r\n";
    bout << "|#20|#9) Unconnected Systems\r\n";
    bout << "|#2Q|#9) Quit NetList\r\n";
    bout.nl();
    bout << "|#9Select: |#2";
    switch (auto cmd = onek("Q1234567890"); cmd) {
    case 'Q':
      return true;
    case '1':
      cmdbit = net_search_type_t::NET_SEARCH_ALL;
      break;
    case '2':
      cmdbit = net_search_type_t::NET_SEARCH_AREACODE;
      bout.nl();
      bout << "|#1Enter Area Code|#2: |#0";
      bin.input(acstr, 3);
      if (strlen(acstr) != 3) {
        abort = true;
      }
      if (!abort) {
        for (int i = 0; i < 3; i++) {
          if (acstr[i] < '0' || acstr[i] > '9') {
            abort = true;
          }
        }
      }
      if (abort) {
        bout << "|#6Area code must be a 3-digit number!\r\n";
        bout.pausescr();
        continue;
      }
      break;
    case '3':
      cmdbit = net_search_type_t::NET_SEARCH_GROUP;
      bout.nl();
      bout << "|#1Enter group number|#2: |#0";
      bin.input(s, 2);
      if (s[0] == 0 || to_number<int>(s) < 1) {
        bout << "|#6Invalid group number!\r\n";
        bout.pausescr();
        continue;
      }
      gn = to_number<int>(s);
      break;
    case '4':
      cmdbit = net_search_type_t::NET_SEARCH_SC;
      break;
    case '5':
      cmdbit = net_search_type_t::NET_SEARCH_AC;
      break;
    case '6':
      cmdbit = net_search_type_t::NET_SEARCH_GC;
      break;
    case '7':
      cmdbit = net_search_type_t::NET_SEARCH_NC;
      break;
    case '8':
      cmdbit = net_search_type_t::NET_SEARCH_SUBSTR;
      bout.nl();
      bout << "|#1Enter SubString|#2: |#0";
      bin.input(substr, 40);
      if (substr[0] == 0) {
        bout << "|#6Enter a substring!\r\n";
        bout.pausescr();
        continue;
      }
      break;
    case '9':
      cmdbit = net_search_type_t::NET_SEARCH_PHSUBSTR;
      bout.nl();
      bout << "|#1Enter phone substring|#2: |#0";
      bin.input(phstr, 12);
      if (phstr[0] == 0) {
        bout << "|#6Enter a phone substring!\r\n";
        bout.pausescr();
        continue;
      }
      break;
    case '0':
      cmdbit = net_search_type_t::NET_SEARCH_NOCONNECT;
      break;
    default: 
      return true;
    }

    if (cmdbit == net_search_type_t::UNKNOWN) {
      continue;
    }

    bout.nl();
    bout << "|#1Print BBS region info? ";
    bool useregion = bin.yesno();

    auto bbslist = BbsListNet::ReadBbsDataNet(net.dir);
    if (bbslist.empty()) {
      bout << "|#6Error opening bbsdata.net in " << net.dir << wwiv::endl;
      bout.pausescr();
      continue;
    }
    to_char_array(s, "000-000-0000");
    bout.nl(2);

    bout.cls();
    bout.litebar(fmt::format("Network list for: {}", net.name));
    for (const auto& b : bbslist.node_config()) {
      char s1[101];
      auto matched = false;
      const auto& csne = b.second;
      if (csne.forsys == WWIVNET_NO_NODE && cmdbit != net_search_type_t::NET_SEARCH_NOCONNECT) {
        continue;
      }
      strcpy(s1, csne.phone);

      if (csne.other & other_net_coord) {
        bbstype = '&';
      } else if (csne.other & other_group_coord) {
        bbstype = '%';
      } else if (csne.other & other_area_coord) {
        bbstype = '^';
      } else if (csne.other & other_subs_coord) {
        bbstype = '~';
      } else {
        bbstype = ' ';
      }

      strcpy(s2, csne.name);
      for (size_t i1 = 0; i1 < size(s2); i1++) {
        s2[i1] = to_upper_case_char(s2[i1]);
      }

      switch (cmdbit) {
      case net_search_type_t::NET_SEARCH_ALL:
        matched = true;
        break;
      case net_search_type_t::NET_SEARCH_AREACODE:
        if (strncmp(acstr, csne.phone, 3) == 0) {
          matched = true;
        }
        break;
      case net_search_type_t::NET_SEARCH_GROUP:
        if (gn == csne.group) {
          matched = true;
        }
        break;
      case net_search_type_t::NET_SEARCH_SC:
        if (csne.other & other_subs_coord) {
          matched = true;
        }
        break;
      case net_search_type_t::NET_SEARCH_AC:
        if (csne.other & other_area_coord) {
          matched = true;
        }
        break;
      case net_search_type_t::NET_SEARCH_GC:
        if (csne.other & other_group_coord) {
          matched = true;
        }
        break;
      case net_search_type_t::NET_SEARCH_NC:
        if (csne.other & other_net_coord) {
          matched = true;
        }
        break;
      case net_search_type_t::NET_SEARCH_SUBSTR:
        if (strstr(s2, substr) != nullptr) {
          matched = true;
        } else {
          char s3[81];
          sprintf(s3, "@%u", csne.sysnum);
          if (strstr(s3, substr) != nullptr) {
            matched = true;
          }
        }
        break;
      case net_search_type_t::NET_SEARCH_PHSUBSTR:
        if (strstr(s1, phstr) != nullptr) {
          matched = true;
        }
        break;
      case net_search_type_t::NET_SEARCH_NOCONNECT:
        if (csne.forsys == WWIVNET_NO_NODE) {
          matched = true;
        }
        break;
      case net_search_type_t::UNKNOWN:
        continue;
      }

      if (matched) {
        slist++;
        if (!useregion && slist == 1) {
          bout.bpla("|#1 Node  Phone         BBS Name                                 Hop  Next Gr",
                    &abort);
          bout.bpla("|#7-----  ============  ---------------------------------------- === ----- --",
                    &abort);
        } else {
          if (useregion && strncmp(s, csne.phone, 3) != 0) {
            strcpy(s, csne.phone);
            const auto regions_dir = FilePath(a()->config()->datadir(), REGIONS_DIR);
            const auto town_fn =
                fmt::sprintf("%s.%-3u", REGIONS_DIR, to_number<unsigned int>(csne.phone));
            std::string areacode;
            if (File::Exists(FilePath(regions_dir, town_fn))) {
              auto town = fmt::sprintf("%c%c%c", csne.phone[4], csne.phone[5], csne.phone[6]);
              areacode =
                  describe_area_code_prefix(to_number<int>(csne.phone), to_number<int>(town));
            } else {
              areacode = describe_area_code(to_number<int>(csne.phone));
            }
            const auto line = fmt::sprintf("\r\n%s%s\r\n", "|#2Region|#0: |#2", areacode);
            bout.bpla(line, &abort);
            bout.bpla(
                "|#1 Node  Phone         BBS Name                                 Hop  Next Gr",
                &abort);
            bout.bpla(
                "|#7-----  ============  ---------------------------------------- === ----- --",
                &abort);
          }
        }
        std::string line;
        if (cmdbit != net_search_type_t::NET_SEARCH_NOCONNECT) {
          line = fmt::sprintf("%5u%c %12s  %-40s %3d %5u %2d", csne.sysnum, bbstype, csne.phone,
                              csne.name, csne.numhops, csne.forsys, csne.group);
        } else {
          line = fmt::sprintf("%5u%c %12s  %-40s%s%2d", csne.sysnum, bbstype, csne.phone, csne.name,
                              " |17|15--- ----- |#0", csne.group);
        }
        bout.bpla(line, &abort);
      }
    }
    if (!abort && slist) {
      bout.nl();
      bout << "|#1Systems Listed |#7: |#2" << slist;
    }
    bout.nl(2);
    bout.pausescr();
  }
  return false;
}

static bool print_ftn_net_listing(net_networks_rec& net) {
  if (!try_load_nodelist(net)) {
    return false;
  }

  bout.cls();
  bout.nl();
  bout << "|#9Network|#2: |#1" << net.name << wwiv::endl;
  bout.nl();

  bout << "|#21|#9) List All\r\n";
  bout << "|#22|#9) Zone\r\n";
  bout << "|#23|#9) BBS Name SubString\r\n";
  bout << "|#2Q|#9) Quit NetList\r\n";
  bout.nl();
  bout << "|#9Select: |#2";
  auto zone = 0;
  std::string name_part;
  switch (const auto cmd = onek("Q123"); cmd) {
  case 'Q': {
    return true;
  }
  case '2': {
    bout.nl();
    bout << "|#9Enter Zone Number|#7: |#0";
    const auto r = bin.input_number_hotkey(zone, {'Q'}, 0, 37627);
    if (r.key == 'Q') {
      return false;
    }
    zone = r.num;
  } break;
  case '3': {
    zone = 0;
    bout.nl();
    bout << "|#9Enter Name Substring|#7: |#0";
    name_part = bin.input_upper(20);
  } break;
  }
  bool abort = false;
  bout.cls();
  bout.litebar(fmt::format("Network list for: {}", net.name));
  bout.nl(2);
  bout.bpla("|#2 Address             BBS Name", &abort);
  bout.bpla("|#9 ------------------  =============================================", &abort);

  auto count = 0;
  if (net.nodelist->initialized()) {
    for (const auto& e : net.nodelist->entries()) {
      if (zone != 0 && e.first.zone() != zone) {
        continue;
      }
      if (!name_part.empty()) {
        const auto bbs_name = ToStringUpperCase(e.second.name_);
        const auto idx = bbs_name.find(name_part);
        if (idx == std::string::npos) {
          continue;
        }
      }
      bout.format(" |#5{:<18.18}  |#1{}\r\n", e.first.as_string(false, false), e.second.name_);
      if (bin.checka()) {
        break;
      }
      ++count;
    }
  }
  if (!abort) {
    bout.nl();
    bout << "|#9Systems Listed |#7: |#2" << count;
  }
  bout.nl(2);
  bout.pausescr();
  return false;
}

void query_print_net_listing(bool force_pause) {
  bool had_pause{false};

  a()->status_manager()->reload_status();

  if (a()->nets().empty()) {
    return;
  }

  write_inst(INST_LOC_NETLIST, 0, INST_FLAGS_NONE);

  if (force_pause) {
    had_pause = a()->user()->pause();
    if (had_pause) {
      a()->user()->toggle_flag(User::pauseOnPage);
    }
  }
  ScopeExit at_exit([=] {
  if (force_pause && had_pause) {
    a()->user()->toggle_flag(User::pauseOnPage);
  }
    
  });

  while (!a()->sess().hangup()) {
    auto current_net = 0;
    bout.cls();
    if (wwiv::stl::ssize(a()->nets()) > 1) {
      std::set<char> odc;
      std::string onx{'Q'};
      bout.nl();
      for (int i = 0; i < ssize(a()->nets()); i++) {
        if (i < 9) {
          onx.push_back(static_cast<char>(i + '1'));
        } else {
          const int odci = (i + 1) / 10;
          odc.insert(static_cast<char>(odci + '0'));
        }
        bout << "|#2" << i + 1 << "|#9)|#1 " << a()->nets()[i].name << wwiv::endl;
      }
      bout << "|#2Q|#9)|#1 Quit\r\n\n";
      bout << "|#9Which network? |#2";
      if (wwiv::stl::ssize(a()->nets()) < 9) {
        if (const char ch = onek(onx); ch == 'Q') {
          return;
        } else {
          current_net = ch - '1';
        }
      } else {
        if (auto mmk = mmkey(odc); mmk == "Q") {
          return;
        } else {
          current_net = to_number<int>(mmk) - 1;
        }
      }

      if (current_net < 0 || current_net > ssize(a()->nets())) {
        continue;
      }
    }
    if (auto & net = a()->nets()[current_net]; net.type == network_type_t::wwivnet) {
      if (print_wwivnet_net_listing(net)) {
        return;
      }
    } else if (net.type == network_type_t::ftn) {
      if (!try_load_nodelist(net)) {
        continue;
      }
      if (print_ftn_net_listing(net)) {
        return;
      }
    }
    
  }
}

void mailr() {
  mailrec m, m1;
  filestatusrec fsr;

  auto pFileEmail(OpenEmailFile(false));
  if (!pFileEmail->IsOpen()) {
    return;
  }
  auto nRecordNumber = (pFileEmail->length() / sizeof(mailrec)) - 1;
  char c = ' ';
  while (nRecordNumber >= 0 && c != 'Q' && !a()->sess().hangup()) {
    pFileEmail->Seek(nRecordNumber * sizeof(mailrec), File::Whence::begin);
    pFileEmail->Read(&m, sizeof(mailrec));
    if (m.touser != 0) {
      pFileEmail->Close();
      do {
        User user;
        a()->users()->readuser(&user, m.touser);
        const auto unn = a()->names()->UserName(m.touser);
        bout << "|#9  To|#7: ";
        bout.Color(a()->GetMessageColor());
        bout << unn << wwiv::endl;
        set_net_num(network_number_from(&m));
        bout << "|#9Subj|#7: ";
        bout.Color(a()->GetMessageColor());
        bout << m.title << wwiv::endl;
        if (m.status & status_file) {
          File attachDat(FilePath(a()->config()->datadir(), ATTACH_DAT));
          if (attachDat.Open(File::modeReadOnly | File::modeBinary)) {
            bool found = false;
            auto lAttachFileSize = attachDat.Read(&fsr, sizeof(fsr));
            while (lAttachFileSize > 0 && !found) {
              if (m.daten == static_cast<uint32_t>(fsr.id)) {
                bout << "|#9Filename.... |#2" << fsr.filename << " (" << fsr.numbytes << " bytes)|#0\r\n";
                found = true;
              }
              if (!found) {
                lAttachFileSize = attachDat.Read(&fsr, sizeof(fsr));
              }
            }
            if (!found) {
              bout << "|#1Filename|#0.... |#2File : Unknown or Missing|#0\r\n";
            }
            attachDat.Close();
          } else {
            bout << "|#1Filename|#0.... |#2|#2File : Unknown or Missing|#0\r\n";
          }
        }
        bool next;
        auto msg = read_type2_message(&m.msg, m.anony & 0x0f, true, "email", m.fromsys, m.fromuser);
        int fake_msgno = -1;
        display_type2_message(fake_msgno, msg, &next);
        bout << "|#2R,D,Q,<space>  : ";
        if (next) {
          c = ' ';
        } else {
          c = onek("QRD ");
        }
        if (c == 'D') {
          pFileEmail = OpenEmailFile(true);
          pFileEmail->Seek(nRecordNumber * sizeof(mailrec), File::Whence::begin);
          pFileEmail->Read(&m1, sizeof(mailrec));
          if (memcmp(&m, &m1, sizeof(mailrec)) == 0) {
            delmail(*pFileEmail.get(), nRecordNumber);
            bool found = false;
            if (m.status & status_file) {
              File attachFile(FilePath(a()->config()->datadir(), ATTACH_DAT));
              if (attachFile.Open(File::modeReadWrite | File::modeBinary)) {
                auto lAttachFileSize = attachFile.Read(&fsr, sizeof(fsr));
                while (lAttachFileSize > 0 && !found) {
                  if (m.daten == static_cast<uint32_t>(fsr.id)) {
                    found = true;
                    fsr.id = 0;
                    attachFile.Seek(static_cast<long>(sizeof(filestatusrec)) * -1L, File::Whence::current);
                    attachFile.Write(&fsr, sizeof(filestatusrec));
                    File::Remove(FilePath(a()->GetAttachmentDirectory(), fsr.filename));
                  } else {
                    attachFile.Read(&fsr, sizeof(filestatusrec));
                  }
                }
                attachFile.Close();
              }
            }
          } else {
            bout << "Mail file changed; try again.\r\n";
          }
          pFileEmail->Close();
          if (!a()->sess().IsUserOnline() && m.touser == 1 && m.tosys == 0) {
            a()->user()->email_waiting(a()->user()->email_waiting() - 1);
          }
        }
        bout.nl(2);
      } while (c == 'R' && !a()->sess().hangup());

      pFileEmail = OpenEmailFile(false);
      if (!pFileEmail->IsOpen()) {
        break;
      }
    }
    nRecordNumber -= 1;
  }
}

void chuser() {
  if (!ValidateSysopPassword() || !so()) {
    return;
  }

  bout << "|#9Enter user to change to: ";
  const auto userName = bin.input(30, true);
  const auto user_number = finduser1(userName);
  if (user_number <= 0) {
    bout << "|#6Unknown user.\r\n";
    return;
  }
  a()->WriteCurrentUser();
  write_qscn(a()->sess().user_num(), a()->sess().qsc, false);
  a()->ReadCurrentUser(user_number);
  read_qscn(user_number, a()->sess().qsc, false);
  a()->sess().user_num(static_cast<uint16_t>(user_number));
  a()->sess().effective_sl(255);
  sysoplog() << StrCat("#*#*#* Changed to ", a()->user()->name_and_number());
  changedsl();
  a()->UpdateTopScreen();
}

void zlog() {
  File file(FilePath(a()->config()->datadir(), ZLOG_DAT));
  if (!file.Open(File::modeReadOnly | File::modeBinary)) {
    return;
  }
  int i = 0;
  bool abort = false;
  zlogrec z;
  file.Read(&z, sizeof(zlogrec));
  bout.nl();
  bout.cls();
  bout.nl(2);
  bout.bpla("|#2  Date     Calls  Active   Posts   Email   Fback    U/L    %Act   T/user", &abort);
  bout.bpla("|#7--------   -----  ------   -----   -----   -----    ---    ----   ------", &abort);
  while (i < 97 && !abort && !a()->sess().hangup() && z.date[0] != 0) {
    int nTimePerUser = 0;
    if (z.calls) {
      nTimePerUser = z.active / z.calls;
    }
    auto buffer = fmt::sprintf(
        "%s    %4d    %4d     %3d     %3d     %3d    %3d     %3d      %3d|16", z.date, z.calls,
        z.active, z.posts, z.email, z.fback, z.up, 10 * z.active / 144, nTimePerUser);
    // alternate colors to make it easier to read across the lines
    if (i % 2) {
      bout << "|#1";
      bout.bpla(buffer, &abort);
    } else {
      bout << "|#3";
      bout.bpla(buffer, &abort);
    }
    ++i;
    if (i < 97) {
      file.Seek(i * sizeof(zlogrec), File::Whence::begin);
      file.Read(&z, sizeof(zlogrec));
    }
  }
  file.Close();
}

void auto_purge() {
  int days = 0;
  int skipsl = 0;
  {
    IniFile ini(FilePath(a()->bbspath(), WWIV_INI),
                {StrCat("WWIV-", a()->sess().instance_number()), INI_TAG});
    if (ini.IsOpen()) {
      days = ini.value<int>("AUTO_USER_PURGE");
      skipsl = ini.value<int>("NO_PURGE_SL");
    }
  }

  if (days < 60) {
    if (days > 0) {
      sysoplog(false) << "!!! WARNING: Auto-Purge canceled [AUTO_USER_PURGE < 60]";
      sysoplog(false) << "!!! WARNING: Edit WWIV.INI and Fix this";
    }
    return;
  }

  const auto current_daten = daten_t_now();
  int user_number = 1;
  sysoplog(false) << "Auto-Purged Inactive Users (over " << days << " days, SL less than " << skipsl << ")";

  do {
    User user;
    a()->users()->readuser(&user, user_number);
    if (!user.exempt_auto_delete()) {
      auto d = static_cast<int>((current_daten - user.last_daten()) / SECONDS_PER_DAY);
      // if user is not already deleted && SL<NO_PURGE_SL && last_logon
      // greater than AUTO_USER_PURGE days ago
      if (!user.deleted() && user.sl() < skipsl && d > days) {
        sysoplog(false) << "*** AUTOPURGE: Deleted User: #" << user_number << " " << user.name();
        a()->users()->delete_user(user_number);
      }
    }
    ++user_number;
  } while (user_number <= a()->status_manager()->user_count());
}

void beginday(bool displayStatus) {
  if (a()->GetBeginDayNodeNumber() > 0
      && (a()->sess().instance_number() != a()->GetBeginDayNodeNumber())) {
    // If BEGINDAYNODENUMBER is > 0 or defined in WWIV.INI only handle beginday events on that node number
    a()->status_manager()->reload_status();
    return;
  }

  const auto st = a()->status_manager()->get_status();
  if (date() == st->last_date()) {
    return;
  }

  a()->status_manager()->Run([&](Status& status) {
    status.ensure_dates_valid();

    if (displayStatus) {
      bout << "|#7* |#1Running Daily Maintenance...\r\n";
      bout << "  |#7* |#1Updating system activity...\r\n";
    }

    zlogrec z{};
    to_char_array(z.date, status.last_date());
    z.active = status.active_today_minutes();
    z.calls = status.calls_today();
    z.posts = status.localposts();
    z.email = status.email_today();
    z.fback = status.feedback_today();
    z.up = status.uploads_today();
    status.NewDay();

    if (displayStatus) {
      bout << "  |#7* |#1Updating ZLOG information...\r\n";
    }
    File fileZLog(FilePath(a()->config()->datadir(), ZLOG_DAT));
    zlogrec z1{};
    if (!fileZLog.Open(File::modeReadWrite | File::modeBinary)) {
      fileZLog.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile, File::shareDenyNone);
      z1.date[0] = '\0';
      z1.active = 0;
      z1.calls = 0;
      z1.posts = 0;
      z1.email = 0;
      z1.fback = 0;
      z1.up = 0;
      for (auto i = 0; i < 97; i++) {
        fileZLog.Write(&z1, sizeof(zlogrec));
      }
    } else {
      for (int i = 96; i >= 1; i--) {
        fileZLog.Seek((i - 1) * sizeof(zlogrec), File::Whence::begin);
        fileZLog.Read(&z1, sizeof(zlogrec));
        fileZLog.Seek(i * sizeof(zlogrec), File::Whence::begin);
        fileZLog.Write(&z1, sizeof(zlogrec));
      }
    }
    fileZLog.Seek(0L, File::Whence::begin);
    fileZLog.Write(&z, sizeof(zlogrec));
    fileZLog.Close();

    if (displayStatus) {
      bout << "  |#7* |#1Updating STATUS.DAT...\r\n";
    }
  });
  const auto nus = a()->config()->max_users() - st->num_users();
  if (displayStatus) {
    bout << "  |#7* |#1Checking system directories and user space...\r\n";
  }

  const auto fk = File::freespace_for_path(a()->config()->datadir());

  if (fk < 512) {
    ssm(1) << "Only " << fk << "k free in data directory.";
  }
  if (!a()->config()->closed_system() && nus < 15) {
    ssm(1) << "Only " << nus << " new user slots left.";
  }
  if (!a()->beginday_cmd.empty()) {
    LOG(INFO) << "Beginday command not empty, executing: " << a()->beginday_cmd;
    const auto commandLine = stuff_in(a()->beginday_cmd, create_chain_file(), "", "", "", "");
    ExecuteExternalProgram(commandLine, a()->spawn_option(SPAWNOPT_BEGINDAY));
  }
  if (displayStatus) {
    bout << "  |#7* |#1Purging inactive users (if enabled)...\r\n";
  }
  auto_purge();
  if (displayStatus) {
    bout << "|#7* |#1Done!\r\n";
  }

  sysoplog(false) << "* Ran Daily Maintenance...";
  LOG(INFO) << "Completed executing beginday";
}


