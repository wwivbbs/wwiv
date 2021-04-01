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
#include "bbs/subedit.h"


#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/subreq.h"
#include "bbs/wqscn.h"
#include "common/com.h"
#include "common/input.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/status.h"
#include "sdk/subxtr.h"
#include "sdk/usermanager.h"
#include "sdk/net/networks.h"
#include "sdk/net/subscribers.h"
#include <string>
#include <vector>

using wwiv::common::InputMode;
using namespace wwiv::core;
using namespace wwiv::local::io;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

static void save_subs() { a()->subs().Save(); }

static std::string boarddata(size_t n, const subboard_t& r) {
  std::string stype;
  if (!r.nets.empty()) {
    stype = r.nets[0].stype;
  }
  return fmt::sprintf("|#2%4d |#1%-37.37s |#2%-8s |#9%-5d |#5%-12s |#1%s", n, stripcolors(r.name), r.filename,
                      r.maxmsgs, stype, r.conf.to_string());
}

static void showsubs() {
  bout.cls();
  auto abort = false;
  bout << "|#7(|#1Message Areas Editor|#7) Enter Substring: ";
  const auto substring = bin.input(20, true);
  bout.cls();
  bout.bpla("|#2NN   Name                                  FN       MSGS  SUBTYPE      CONF", &abort);
  bout.bpla("|#7==== ------------------------------------- ======== ===== ------------ -------", &abort);
  auto subnum = 0;
  for (const auto& r : a()->subs().subs()) {
    const auto subdata = StrCat(r.name, " ", r.filename);
    if (ifind_first(subdata, substring)) {
      const auto line = boarddata(subnum, r);
      bout.bpla(line, &abort);
      if (abort)
        break;
    }
    ++subnum;
  }
}

static std::string GetKey(const subboard_t& r) { return (r.key == 0) ? "None." : std::string(1, r.key); }

static std::string GetAnon(const subboard_t& r) {
  switch (r.anony & 0x0f) {
  case 0:
    return YesNoString(false);
  case anony_enable_anony:
    return YesNoString(true);
  case anony_enable_dear_abby:
    return "Dear Abby";
  case anony_force_anony:
    return "Forced";
  case anony_real_name:
    return "Real Name";
  default:
    return "Real screwed up";
  }
}

static void DisplayNetInfo(size_t nSubNum) {
  if (a()->subs().sub(nSubNum).nets.empty()) {
    bout << "|#2Not networked.\r\n";
    return;
  }

  bout << "\r\n|#9      Network      Host         Type                  Scrb   Flags\r\n";
  int i = 0;
  const auto& nets = a()->subs().sub(nSubNum).nets;
  for (const auto& sn : nets) {
    const auto auto_info_text =
        (sn.category) ? fmt::format(" Auto-Info({})", sn.category) : " Auto-Info";
    if (ssize(a()->nets()) <= sn.net_num) {
      bout << "      |#6No network exists at number: " << sn.net_num << " to display. \r\n";
      continue;
    }
    const auto& net = a()->nets()[sn.net_num];
    bout.bprintf("   |#9|#1%c|#9) |#2", static_cast<char>('a' + i));
    if (sn.host == 0) {
      std::string host = "<HERE>";
      const auto net_file_name = fmt::format("n{}.net", net.dir, sn.stype);
      std::set<uint16_t> subscribers;
      ReadSubcriberFile(FilePath(net.dir, net_file_name), subscribers);
      auto num = ssize(subscribers);
      bout.bprintf("%-12.12s %-12.12s %-20.20s  %-4d  %s%s\r\n",
                           net.name, host, sn.stype, num,
                           (sn.flags & XTRA_NET_AUTO_ADDDROP) ? " Auto-Req" : "",
                           (sn.flags & XTRA_NET_AUTO_INFO) ? auto_info_text : "");
    } else if (net.type == network_type_t::ftn) {
      const auto host = sn.ftn_uplinks.empty() ? "[-none-]" : sn.ftn_uplinks.begin()->as_string();
      bout.bprintf("%-12.12s %-12.12s %s\r\n", net.name, host, sn.stype);
    } else {
      auto host = fmt::format("{} ", sn.host);
      bout.bprintf("%-12.12s %-12.12s %-20.20s  %s%s\r\n", net.name,
                           host, sn.stype, (sn.flags & XTRA_NET_AUTO_ADDDROP) ? " Auto-Req" : "",
                           (sn.flags & XTRA_NET_AUTO_INFO) ? auto_info_text : "");
    }
    ++i;
  }
}

// returns the sub name using the file filename or empty string.
static std::string subname_using(const std::string& filename) {
  for (const auto& sub : a()->subs().subs()) {
    if (iequals(filename, sub.filename)) {
      return sub.name;
    }
  }
  return "";
}

static void modify_sub(int n) {
  auto r = a()->subs().sub(n);
  auto done = false;
  do {
    bout.cls();
    bout.litebar(StrCat("Editing Message Sub #", n));
    bout << "|#9A) Name       : |#2" << r.name << wwiv::endl;
    bout << "|#9B) Filename   : |#2" << r.filename << wwiv::endl;
    bout << "|#9C) Key        : |#2" << GetKey(r) << wwiv::endl;
    bout << "|#9D) Read ACS   : |#2" << r.read_acs << wwiv::endl;
    bout << "|#9E) Post ACS   : |#2" << r.post_acs << wwiv::endl;
    bout << "|#9F) Anony      : |#2" << GetAnon(r) << wwiv::endl;
    bout << "|#9H) Max Msgs   : |#2" << r.maxmsgs << wwiv::endl;
    bout << "|#9J) Net info   : |#2";
    DisplayNetInfo(n);

    bout << "|#9K) Storage typ: |#2" << static_cast<int>(r.storage_type) << wwiv::endl;
    bout << "|#9L) Val network: |#2" << YesNoString((r.anony & anony_val_net) ? true : false)
         << wwiv::endl;
    bout << "|#9M) Req ANSI   : |#2" << YesNoString((r.anony & anony_ansi_only) ? true : false)
         << wwiv::endl;
    bout << "|#9N) Disable tag: |#2" << YesNoString((r.anony & anony_no_tag) ? true : false)
         << wwiv::endl;
    bout << "|#9O) Description: |#2"
         << ((!a()->subs().sub(n).desc.empty()) ? a()->subs().sub(n).desc : "None.") << wwiv::endl;
    bout << "|#9P) Disable FS:  |#2" << YesNoString((r.anony & anony_no_fullscreen) ? true : false)
         << wwiv::endl;
    bout << "|#9   Conferences: |#2" << r.conf.to_string() << wwiv::endl;
    bout.nl();
    bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1O|#7,|#1[|#7=|#1Prev|#7,|#1]|#7=|#1Next|#7) "
            ": ";
    auto ch = onek("QABCDEFGHIJKLMNOP[]", true);
    bout.nl();
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case '[':
      a()->subs().set_sub(n, r);
      if (--n < 0) {
        n = size_int(a()->subs().subs()) - 1;
      }
      r = a()->subs().sub(n);
      break;
    case ']':
      a()->subs().set_sub(n, r);
      if (++n >= size_int(a()->subs().subs())) {
        n = 0;
      }
      r = a()->subs().sub(n);
      break;
    case 'A': {
      bout << "|#2New name? ";
      auto new_name = bin.input_text(r.name, 40);
      if (!new_name.empty()) {
        r.name = new_name;
      }
    } break;
    case 'B': {
      bout << "|#2New base filename (e.g. 'GENERAL')? ";
      auto new_fn = bin.input_filename(r.filename, 8);
      if (new_fn.empty() || contains(new_fn, '.') || new_fn == r.filename) {
        break;
      }
      auto new_sub_fullpath = FilePath(a()->config()->datadir(), StrCat(new_fn, ".sub"));
      if (File::Exists(new_sub_fullpath)) {
        // Find out which sub was using it.
        bout.nl();
        auto sub_name_using_file = subname_using(new_fn);
        bout << "|#6" << new_fn << " already in use for '" << sub_name_using_file << "'"
             << wwiv::endl
             << wwiv::endl
             << "|#5Use anyway? ";
        if (!bin.yesno()) {
          break;
        }
      }
      auto old_subname(r.filename);
      r.filename = new_fn;

      if (r.storage_type != 2) {
        // Only rename files for type2
        break;
      }

      const auto old_sub_fullpath = FilePath(a()->config()->datadir(), StrCat(old_subname, ".sub"));
      const auto old_msg_fullpath = FilePath(a()->config()->msgsdir(), StrCat(old_subname, ".dat"));
      const auto new_msg_fullpath = FilePath(a()->config()->msgsdir(), StrCat(new_fn, ".dat"));

      if (!File::Exists(new_sub_fullpath) && !File::Exists(new_msg_fullpath) &&
          new_fn != "NONAME" && old_subname != "NONAME") {
        bout.nl();
        bout << "|#7Rename current data files (.SUB/.DAT)? ";
        if (bin.yesno()) {
          File::Rename(old_sub_fullpath, new_sub_fullpath);
          File::Rename(old_msg_fullpath, new_msg_fullpath);
        }
      }
    } break;
    case 'C': {
      bout.nl();
      bout << "|#2New Key (space = none) ? ";
      auto ch2 = onek("@%^&()_=\\|;:'\",` ");
      r.key = (ch2 == SPACE) ? '\0' : ch2;
    } break;
    case 'D': {
      bout.nl();
      r.read_acs = wwiv::bbs::input_acs(bin, bout, "New Read ACS?", r.read_acs, 78);
    } break;
    case 'E': {
      bout.nl();
      r.post_acs = wwiv::bbs::input_acs(bin, bout, "New Post ACS?", r.post_acs, 78);
    } break;
    case 'F': {
      std::string allowed("NYDFR");
      bout.nl();
      bout << "|#2New Anony (Y,N,D,F,R) ? ";
      const auto Y = YesNoString(true)[0];
      const auto N = YesNoString(false)[0];
      allowed.push_back(Y);
      allowed.push_back(N);
      char ch2 = onek(allowed, true);
      if (ch2 == N) {
        ch2 = 0;
      } else if (ch2 == Y) {
        ch2 = 1;
      }
      r.anony &= 0xf0;
      switch (ch2) {
      case 0:
        break;
      case 1:
        r.anony |= anony_enable_anony;
        break;
      case 'D':
        r.anony |= anony_enable_dear_abby;
        break;
      case 'F':
        r.anony |= anony_force_anony;
        break;
      case 'R':
        r.anony |= anony_real_name;
        break;
      }
    } break;
    case 'H': {
      bout.nl();
      bout << "|#2New Max Msgs? ";
      r.maxmsgs = bin.input_number(r.maxmsgs);
    } break;
    case 'J': {
      const auto num_nets = wwiv::stl::ssize(a()->nets());
      if (!num_nets) {
        break;
      }
      a()->subs().set_sub(n, r);
      auto ch2 = 'A';
      if (!r.nets.empty()) {
        bout.nl();
        bout << "|#2A)dd, D)elete, or M)odify net reference (Q=Quit)? ";
        ch2 = onek("QAMD");
      }

      if (ch2 == 'A') {
        if (!sub_xtr_add(n, -1)) {
          continue;
        }
        if (a()->subs().sub(n).name == "** New WWIV Message Sub **") {
          a()->subs().sub(n).name = a()->subs().sub(n).desc;
        }
        const auto stype = a()->subs().sub(n).nets[0].stype;
        if (a()->subs().sub(n).filename == "NONAME" && stype.size() <= 8) {
          a()->subs().sub(n).filename = stype;
        }
      } else if (ch2 == 'D' || ch2 == 'M') {
        bout.nl();
        if (ch2 == 'D') {
          bout << "|#2Delete which (a-";
        } else {
          bout << "|#2Modify which (a-";
        }
        bout << static_cast<char>('a' + a()->subs().sub(n).nets.size() - 1) << "), <space>=Quit? ";
        std::string charstring;
        for (size_t i = 0; i < a()->subs().sub(n).nets.size(); i++) {
          charstring.push_back(static_cast<char>('A' + i));
        }
        bout.Color(0);
        auto ch3 = onek(charstring);
        if (ch3 != ' ') {
          int i = ch3 - 'A';
          if (i >= 0 && i < ssize(a()->subs().sub(n).nets)) {
            if (ch2 == 'D') {
              sub_xtr_del(n, i, 1);
            } else {
              sub_xtr_del(n, i, 0);
              if (!sub_xtr_add(n, i)) {
                continue;
              }
            }
          }
        }
      }
      r = a()->subs().sub(n);
    } break;
    case 'K': {
      bout.nl();
      //bout << "|#2New Storage Type ( 2 ) ? ";
      //auto new_type = bin.input_number<uint8_t>(r.storage_type, 2, 2);
      r.storage_type = 2;
    } break;
    case 'L':
      bout.nl();
      bout << "|#5Require sysop validation for network posts? ";
      r.anony &= ~anony_val_net;
      if (bin.yesno()) {
        r.anony |= anony_val_net;
      }
      break;
    case 'M':
      bout.nl();
      bout << "|#5Require ANSI to read this sub? ";
      r.anony &= ~anony_ansi_only;
      if (bin.yesno()) {
        r.anony |= anony_ansi_only;
      }
      break;
    case 'N':
      bout.nl();
      bout << "|#5Disable tag lines for this sub? ";
      r.anony &= ~anony_no_tag;
      if (bin.yesno()) {
        r.anony |= anony_no_tag;
      }
      break;
    case 'O': {
      bout.nl();
      bout << "|#2Enter new Description : \r\n|#7:";
      a()->subs().sub(n).desc = bin.input_text(a()->subs().sub(n).desc, 60);
    } break;
    case 'P':
      bout.nl();
      bout << "|#5Disable the Full Screen Reader for this sub? ";
      r.anony &= ~anony_no_fullscreen;
      if (bin.yesno()) {
        r.anony |= anony_no_fullscreen;
      }
      break;
    }
  } while (!done && !a()->sess().hangup());
  a()->subs().set_sub(n, r);
}

static void swap_subs(int sub1, int sub2) {
  if (sub1 < 0 || sub1 >= ssize(a()->subs().subs()) || sub2 < 0 ||
      sub2 >= ssize(a()->subs().subs())) {
    return;
  }

  const auto num_user_records = a()->users()->num_user_records();

  auto pTempQScan = std::make_unique<uint32_t[]>(a()->config()->qscn_len());
  for (auto i = 1; i <= num_user_records; i++) {
    int i1, i2;
    read_qscn(i, pTempQScan.get(), true);
    uint32_t* pTempQScan_n = &pTempQScan.get()[1];
    uint32_t* pTempQScan_q = pTempQScan_n + (a()->config()->max_dirs() + 31) / 32;
    uint32_t* pTempQScan_p = pTempQScan_q + (a()->config()->max_subs() + 31) / 32;

    if (pTempQScan_q[sub1 / 32] & (1L << (sub1 % 32))) {
      i1 = 1;
    } else {
      i1 = 0;
    }

    if (pTempQScan_q[sub2 / 32] & (1L << (sub2 % 32))) {
      i2 = 1;
    } else {
      i2 = 0;
    }
    if (i1 + i2 == 1) {
      pTempQScan_q[sub1 / 32] ^= (1L << (sub1 % 32));
      pTempQScan_q[sub2 / 32] ^= (1L << (sub2 % 32));
    }
    const auto tl = pTempQScan_p[sub1];
    pTempQScan_p[sub1] = pTempQScan_p[sub2];
    pTempQScan_p[sub2] = tl;

    write_qscn(i, pTempQScan.get(), true);
  }
  close_qscn();

  const auto sbt = a()->subs().sub(sub1);
  a()->subs().set_sub(sub1, a()->subs().sub(sub2));
  a()->subs().set_sub(sub2, sbt);

  save_subs();
}

static void insert_sub(int n) {
  int i2;
  if (n < 0 || n > ssize(a()->subs().subs())) {
    return;
  }


  subboard_t r = {};
  r.name = "** New WWIV Message Sub **";
  r.filename = "NONAME";
  r.key = 0;
  r.read_acs = "user.sl >= 10";
  r.post_acs = "user.sl >= 20";
  r.anony = 0;
  r.maxmsgs = 5000;
  r.storage_type = 2;

  // Insert new item.
  a()->subs().insert(n, r);

  const auto num_user_records = a()->users()->num_user_records();

  const auto pTempQScan = std::make_unique<uint32_t[]>(a()->config()->qscn_len());
  const auto pTempQScan_n = &pTempQScan.get()[1];
  const auto pTempQScan_q = pTempQScan_n + (a()->config()->max_dirs() + 31) / 32;
  const auto pTempQScan_p = pTempQScan_q + (a()->config()->max_subs() + 31) / 32;

  const uint32_t m1 = 1L << (n % 32);
  const uint32_t m2 = 0xffffffff << ((n % 32) + 1);
  const uint32_t m3 = 0xffffffff >> (32 - (n % 32));

  for (auto i = 1; i <= num_user_records; i++) {
    read_qscn(i, pTempQScan.get(), true);

    if (pTempQScan[0] != 999 && pTempQScan[0] >= static_cast<uint32_t>(n)) {
      pTempQScan[0]++;
    }

    for (auto i1 = size_int(a()->subs().subs()) - 1; i1 > n; i1--) {
      pTempQScan_p[i1] = pTempQScan_p[i1 - 1];
    }
    pTempQScan_p[n] = 0;

    for (i2 = size_int(a()->subs().subs()) / 32; i2 > n / 32; i2--) {
      pTempQScan_q[i2] = (pTempQScan_q[i2] << 1) | (pTempQScan_q[i2 - 1] >> 31);
    }
    pTempQScan_q[i2] = m1 | (m2 & (pTempQScan_q[i2] << 1)) | (m3 & pTempQScan_q[i2]);

    write_qscn(i, pTempQScan.get(), true);
  }
  close_qscn();
  save_subs();

  if (a()->sess().GetCurrentReadMessageArea() >= n) {
    a()->sess().SetCurrentReadMessageArea(a()->sess().GetCurrentReadMessageArea() + 1);
  }
}

static void delete_sub(int n) {
  if (n < 0 || n >= ssize(a()->subs().subs())) {
    return;
  }
  while (ssize(a()->subs().subs()) > n && !a()->subs().sub(n).nets.empty()) {
    sub_xtr_del(n, 0, 1);
  }
  a()->subs().erase(n);
  const auto num_user_records = a()->users()->num_user_records();

  const auto pTempQScan = std::make_unique<uint32_t[]>(a()->config()->qscn_len() + 1);
  auto pTempQScan_n = &pTempQScan.get()[1];
  auto pTempQScan_q = pTempQScan_n + (a()->config()->max_dirs() + 31) / 32;
  unsigned* pTempQScan_p = pTempQScan_q + (a()->config()->max_subs() + 31) / 32;

  auto m2 = 0xffffffff << (n % 32);
  auto m3 = 0xffffffff >> (32 - (n % 32));

  for (int i = 1; i <= num_user_records; i++) {
    read_qscn(i, pTempQScan.get(), true);

    if (pTempQScan[0] != 999) {
      if (pTempQScan[0] == static_cast<uint32_t>(n)) {
        pTempQScan[0] = 999;
      } else if (pTempQScan[0] > static_cast<uint32_t>(n)) {
        pTempQScan[0]--;
      }
    }
    for (auto i1 = n; i1 < ssize(a()->subs().subs()); i1++) {
      pTempQScan_p[i1] = pTempQScan_p[i1 + 1];
    }

    pTempQScan_q[n / 32] = (pTempQScan_q[n / 32] & m3) | ((pTempQScan_q[n / 32] >> 1) & m2) |
                           (pTempQScan_q[(n / 32) + 1] << 31);

    for (auto i2 = (n / 32) + 1; i2 <= ssize(a()->subs().subs()) / 32; i2++) {
      pTempQScan_q[i2] = (pTempQScan_q[i2] >> 1) | (pTempQScan_q[i2 + 1] << 31);
    }

    write_qscn(i, pTempQScan.get(), true);
  }
  close_qscn();
  save_subs();

  if (a()->sess().GetCurrentReadMessageArea() == n) {
    a()->sess().SetCurrentReadMessageArea(-1);
  } else if (a()->sess().GetCurrentReadMessageArea() > n) {
    a()->sess().SetCurrentReadMessageArea(a()->sess().GetCurrentReadMessageArea() - 1);
  }
}

void boardedit() {
  if (!ValidateSysopPassword()) {
    return;
  }
  showsubs();
  auto done = false;
  a()->status_manager()->reload_status();
  do {
    bout.nl();
    bout << "|#9(|#2Q|#9)uit (|#2D|#9)elete, (|#2I|#9)nsert, (|#2M|#9)odify, (|#2S|#9)wapSubs, (|#2C|#9)onferences : ";
    switch (const auto ch = onek("QSDIMC?"); ch) {
    case '?':
      showsubs();
      break;
    case 'C':
      edit_conf_subs(a()->all_confs().subs_conf());
      break;
    case 'Q':
      done = true;
      break;
    case 'M': {
      bout.nl();
      bout << "|#2Sub number? ";
      const int subnum = bin.input_number(-1, 0, ssize(a()->subs().subs()) - 1, false);
      if (subnum >= 0) {
        modify_sub(subnum);
      }
    } break;
    case 'S': {
      if (a()->subs().subs().size() < a()->config()->max_subs()) {
        bout.nl();
        bout << "|#2Take sub number? ";
        auto subnum1 = bin.input_number(-1, 0, ssize(a()->subs().subs()) - 1, false);
        if (subnum1 <= 0) {
          break;
        }
        bout.nl();
        bout << "|#2And move before sub number? ";
        const auto subnum2 = bin.input_number(-1, 1, ssize(a()->subs().subs()) - 1, false);
        if (subnum2 <= 0) {
          break;
        }
        bout.nl();
        if (subnum2 < subnum1) {
          subnum1++;
        }
        write_qscn(a()->sess().user_num(), a()->sess().qsc, true);
        bout << "|#1Moving sub now...Please wait...";
        insert_sub(subnum2);
        swap_subs(subnum1, subnum2);
        delete_sub(subnum1);
        showsubs();
      } else {
        bout << "\r\nYou must increase the number of subs in WWIVconfig first.\r\n";
      }
    } break;
    case 'I': {
      if (a()->subs().subs().size() >= a()->config()->max_subs()) {
        break;
      }
      bout.nl();
      bout << "|#2Insert before which sub ('$' for end) : ";
      auto s = bin.input(4);
      subconf_t subnum;
      if (s[0] == '$') {
        subnum = static_cast<subconf_t>(ssize(a()->subs().subs()));
      } else {
        subnum = to_number<uint16_t>(s);
      }
      if (!s.empty() && subnum <= ssize(a()->subs().subs())) {
        insert_sub(subnum);
        modify_sub(subnum);
        const auto confs_size = a()->all_confs().subs_conf().size();
        if (confs_size > 1) {
          bout.nl();
          if (auto o = select_conf("Put in which conference? ", a()->all_confs().subs_conf(), true)) {
            a()->subs().sub(subnum).conf.insert(o.value());
          }
        } else if (confs_size == 1) {
          a()->subs().sub(subnum).conf.insert(a()->all_confs().subs_conf().front().key.key());
        }
      }
    } break;
    case 'D': {
      bout.nl();
      bout << "|#2Delete which sub? ";
      const int subnum = bin.input_number(-1, 1, size_int(a()->subs().subs()) - 1, false);
      if (subnum >= 0) {
        bout.nl();
        bout << "|#5Delete " << a()->subs().sub(subnum).name << "? ";
        if (bin.yesno()) {
          auto fn = a()->subs().sub(subnum).filename;
          delete_sub(subnum);
          bout.nl();
          bout << "|#5Delete data files (including messages) for sub also? ";
          if (bin.yesno()) {
            File::Remove(FilePath(a()->config()->datadir(), StrCat(fn, ".sub")));
            File::Remove(FilePath(a()->config()->msgsdir(), StrCat(fn, ".dat")));
          }
        }
      }
    } break;
    }
  } while (!done && !a()->sess().hangup());
  save_subs();
  if (!a()->at_wfc()) {
    changedsl();
  }
  a()->subchg = 1;
}
