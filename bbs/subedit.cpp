/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2019, WWIV Software Services             */
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

#include <string>
#include <vector>

#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/input.h"
#include "sdk/subxtr.h"
#include "local_io/keycodes.h"
#include "sdk/status.h"
#include "bbs/bbs.h"
#include "bbs/com.h"
#include "bbs/subreq.h"

#include "bbs/wqscn.h"
#include "core/file.h"
#include "core/stl.h"
#include "core/strings.h"
#include "sdk/filenames.h"
#include "sdk/subscribers.h"

using std::string;
using wwiv::bbs::InputMode;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

static void save_subs() {
  a()->subs().Save();
}

static string GetAr(const subboard_t& r, const string& default_value) {
  if (r.ar != 0) {
    for (int i = 0; i < 16; i++) {
      if ((1 << i) & r.ar) {
        return string(1, static_cast<char>('A' + i));
      }
    }
  }
  return default_value;
}

static string boarddata(size_t n, const subboard_t& r) {
  string stype;
  if (!r.nets.empty()) {
    stype = r.nets[0].stype;
  }
  string ar = GetAr(r, " ");
  return StringPrintf("|#2%4d |#9%1s  |#1%-37.37s |#2%-8s |#9%-3d %-3d %-2d %-5d %7s",
          n, ar.c_str(), stripcolors(r.name.c_str()), r.filename.c_str(), r.readsl, r.postsl, r.age,
          r.maxmsgs, stype.c_str());
}

static void showsubs() {
  bout.cls();
  bool abort = false;
  bout << "|#7(|#1Message Areas Editor|#7) Enter Substring: ";
  string substring = input(20, true);
  bout.bpla("|#2NN   AR Name                                  FN       RSL PSL AG MSGS  SUBTYPE", &abort);
  bout.bpla("|#7==== == ------------------------------------- ======== --- === -- ===== -------", &abort);
  int subnum = 0;
  for (const auto& r : a()->subs().subs()) {
    const string subdata = StrCat(r.name, " ", r.filename);
    if (strcasestr(subdata.c_str(), substring.c_str())) {
      const string line = boarddata(subnum, r);
      bout.bpla(line, &abort);
      if (abort) break;
    }
    ++subnum;
  }
}

static string GetKey(const subboard_t& r) {
  return (r.key == 0) ? "None." : string(1, r.key);
}

static string GetAnon(const subboard_t& r) {
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

  bout << "\r\n|#9      Network      Type                 Host    Scrb   Flags\r\n";
  int i = 0;
  const auto& nets = a()->subs().sub(nSubNum).nets;
  for (auto it = nets.begin(); it != nets.end(); i++, it++) {
    char szBuffer[255], szBuffer2[255];
    if ((*it).host == 0) {
      strcpy(szBuffer, "<HERE>");
    } else {
      sprintf(szBuffer, "%u ", (*it).host);
    }
    if ((*it).category) {
      sprintf(szBuffer2, " Auto-Info(%d)", (*it).category);
    } else {
      strcpy(szBuffer2, " Auto-Info");
    }
    if ((*it).host == 0) {
      const auto dir = a()->net_networks[(*it).net_num].dir;
      const string net_file_name = StrCat(dir, "n", (*it).stype, ".net");
      std::set<uint16_t> subscribers;
      ReadSubcriberFile(dir, StrCat("n", (*it).stype, ".net"), subscribers);
      int num = size_int(subscribers);
      bout.bprintf("   |#9%c) |#2%-12.12s %-20.20s %-6.6s  %-4d  %s%s\r\n",
                    i + 'a',
                    a()->net_networks[(*it).net_num].name,
                    (*it).stype.c_str(),
                    szBuffer,
                    num,
                    ((*it).flags & XTRA_NET_AUTO_ADDDROP) ? " Auto-Req" : "",
                    ((*it).flags & XTRA_NET_AUTO_INFO) ? szBuffer2 : "");
    } else {
      bout.bprintf("   |#9%c) |#2%-12.12s %-20.20s %-6.6s  %s%s\r\n",
                    i + 'a',
                    a()->net_networks[(*it).net_num].name,
                    (*it).stype.c_str(),
                    szBuffer,
                    ((*it).flags & XTRA_NET_AUTO_ADDDROP) ? " Auto-Req" : "",
                    ((*it).flags & XTRA_NET_AUTO_INFO) ? szBuffer2 : "");
    }
  }
}

// returns the sub name using the file filename or empty string.
static string subname_using(const string& filename) {
  for (const auto& sub : a()->subs().subs()) {
    if (iequals(filename.c_str(), sub.filename.c_str())) {
      return sub.name;
    }
  }
  return "";
}
static void modify_sub(int n) {
  auto r = a()->subs().sub(n);
  bool done = false;
  do {
    bout.cls();
    bout.litebar(StrCat("Editing Message Sub #", n));
    bout << "|#9A) Name       : |#2" << r.name << wwiv::endl;
    bout << "|#9B) Filename   : |#2" << r.filename << wwiv::endl;
    bout << "|#9C) Key        : |#2" << GetKey(r) << wwiv::endl;
    bout << "|#9D) Read SL    : |#2" << static_cast<int>(r.readsl) << wwiv::endl;
    bout << "|#9E) Post SL    : |#2" << static_cast<int>(r.postsl) << wwiv::endl;
    bout << "|#9F) Anony      : |#2" << GetAnon(r) << wwiv::endl;
    bout << "|#9G) Min. Age   : |#2" << static_cast<int>(r.age) << wwiv::endl;
    bout << "|#9H) Max Msgs   : |#2" << r.maxmsgs << wwiv::endl;
    bout << "|#9I) AR         : |#2" << GetAr(r, "None.") << wwiv::endl;
    bout << "|#9J) Net info   : |#2";
    DisplayNetInfo(n);

    bout << "|#9K) Storage typ: |#2" << r.storage_type << wwiv::endl;
    bout << "|#9L) Val network: |#2" << YesNoString((r.anony & anony_val_net) ? true : false) << wwiv::endl;
    bout << "|#9M) Req ANSI   : |#2" << YesNoString((r.anony & anony_ansi_only) ? true : false) << wwiv::endl;
    bout << "|#9N) Disable tag: |#2" << YesNoString((r.anony & anony_no_tag) ? true : false) << wwiv::endl;
    bout << "|#9O) Description: |#2" << ((!a()->subs().sub(n).desc.empty()) ? a()->subs().sub(n).desc : "None.") << wwiv::endl;
    bout << "|#9P) Disable FS:  |#2" << YesNoString((r.anony & anony_no_fullscreen) ? true : false) << wwiv::endl;
    bout.nl();
    bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1O|#7,|#1[|#7=|#1Prev|#7,|#1]|#7=|#1Next|#7) : ";
    char ch = onek("QABCDEFGHIJKLMNOP[]", true);
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
      auto new_name = input_text(r.name, 40);
      if (!new_name.empty()) {
        r.name = new_name;
      }
    }
    break;
    case 'B': {
      bout << "|#2New base filename (e.g. 'GENERAL')? ";
      auto new_fn = input_filename(r.filename, 8);
      if (new_fn.empty() || contains(new_fn, '.')) {
        break;
      }
      auto new_sub_fullpath = PathFilePath(a()->config()->datadir(), StrCat(new_fn, ".sub"));
      if (File::Exists(new_sub_fullpath)) {
        // Find out which sub was using it.
        bout.nl();
        string sub_name_using_file = subname_using(new_fn);
        bout << "|#6" << new_fn << " already in use for '"
             << sub_name_using_file << "'" << wwiv::endl << wwiv::endl
             << "|#5Use anyway? ";
        if (!yesno()) {
          break;
        }
      }
      string old_subname(r.filename);
      r.filename = new_fn;

      if (r.storage_type != 2) {
        // Only rename files for type2
        break;
      }

      const auto old_sub_fullpath =
          PathFilePath(a()->config()->datadir(), StrCat(old_subname, ".sub"));
      const auto old_msg_fullpath =
          PathFilePath(a()->config()->msgsdir(), StrCat(old_subname, ".dat"));
      const auto new_msg_fullpath = PathFilePath(a()->config()->msgsdir(), StrCat(new_fn, ".dat"));

      if (!File::Exists(new_sub_fullpath) && !File::Exists(new_msg_fullpath)
        && new_fn != "NONAME" && old_subname != "NONAME") {
        bout.nl();
        bout << "|#7Rename current data files (.SUB/.DAT)? ";
        if (yesno()) {
          File::Rename(old_sub_fullpath, new_sub_fullpath);
          File::Rename(old_msg_fullpath, new_msg_fullpath);
        }
      }
    }
    break;
    case 'C': {
      bout.nl();
      bout << "|#2New Key (space = none) ? ";
      char ch2 = onek("@%^&()_=\\|;:'\",` ");
      r.key = (ch2 == SPACE) ? 0 : ch2;
    }
    break;
    case 'D': {
      bout.nl();
      bout << "|#2New Read SL? ";
      r.readsl = input_number(r.readsl); 
    }
    break;
    case 'E': {
      bout.nl();
      bout << "|#2New Post SL? ";
      r.postsl = input_number(r.postsl);
    }
    break;
    case 'F': {
      string allowed("NYDFR");
      bout.nl();
      bout << "|#2New Anony (Y,N,D,F,R) ? ";
      const char Y = *YesNoString(true);
      const char N = *YesNoString(false);
      allowed.push_back(Y);
      allowed.push_back(N);
      char ch2 = onek(allowed.c_str(), true);
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
    }
    break;
    case 'G': {
      bout.nl();
      bout << "|#2New Min Age? ";
      r.age = input_number(r.age);
    }
    break;
    case 'H': {
      bout.nl();
      bout << "|#2New Max Msgs? ";
      r.maxmsgs = input_number(r.maxmsgs);
    }
    break;
    case 'I': {
      bout.nl();
      bout << "|#2Enter New AR (<SPC>=None) : ";
      char ch2 = onek("ABCDEFGHIJKLMNOP ", true);
      if (ch2 == SPACE) {
        r.ar = 0;
      } else {
        r.ar = 1 << (ch2 - 'A');
      }
    }
    break;
    case 'J': {
      a()->subs().set_sub(n, r);
      char ch2 = 'A';
      if (!r.nets.empty()) {
        bout.nl();
        bout << "|#2A)dd, D)elete, or M)odify net reference (Q=Quit)? ";
        ch2 = onek("QAMD");
      }

      if (ch2 == 'A') {
        sub_xtr_add(n, -1);
        if (a()->subs().sub(n).name == "** New WWIV Message Sub **") {
          a()->subs().sub(n).name = a()->subs().sub(n).desc;
        }
        if (a()->subs().sub(n).filename == "NONAME") {
          a()->subs().sub(n).filename = a()->subs().sub(n).nets[0].stype;
        }
      } else if (ch2 == 'D' || ch2 == 'M') {
        bout.nl();
        if (ch2 == 'D') {
          bout << "|#2Delete which (a-";
        } else {
          bout << "|#2Modify which (a-";
        }
        bout << static_cast<char>('a' + a()->subs().sub(n).nets.size() - 1) 
             << "), <space>=Quit? ";
        string charstring;
        for (size_t i = 0; i < a()->subs().sub(n).nets.size(); i++) {
          charstring.push_back(static_cast<char>('A' + i));
        }
        bout.Color(0);
        char ch3 = onek(charstring.c_str());
        if (ch3 != ' ') {
          int i = ch3 - 'A';
          if (i >= 0 && i < size_int(a()->subs().sub(n).nets)) {
            if (ch2 == 'D') {
              sub_xtr_del(n, i, 1);
            } else {
              sub_xtr_del(n, i, 0);
              sub_xtr_add(n, i);
            }
          }
        }
      }
      r = a()->subs().sub(n);
    }
    break;
    case 'K': {
      bout.nl();
      bout << "|#2New Storage Type ( 2 ) ? ";
      auto new_type = input_number<uint8_t>(r.storage_type, 2, 2);
      if (new_type == 2) {
        r.storage_type = new_type;
      }
    }
    break;
    case 'L':
      bout.nl();
      bout << "|#5Require sysop validation for network posts? ";
      r.anony &= ~anony_val_net;
      if (yesno()) {
        r.anony |= anony_val_net;
      }
      break;
    case 'M':
      bout.nl();
      bout << "|#5Require ANSI to read this sub? ";
      r.anony &= ~anony_ansi_only;
      if (yesno()) {
        r.anony |= anony_ansi_only;
      }
      break;
    case 'N':
      bout.nl();
      bout << "|#5Disable tag lines for this sub? ";
      r.anony &= ~anony_no_tag;
      if (yesno()) {
        r.anony |= anony_no_tag;
      }
      break;
    case 'O': {
      bout.nl();
      bout << "|#2Enter new Description : \r\n|#7:";
      a()->subs().sub(n).desc = input_text(a()->subs().sub(n).desc, 60);
    }
    break;
    case 'P':
      bout.nl();
      bout << "|#5Disable the Full Screen Reader for this sub? ";
      r.anony &= ~anony_no_fullscreen;
      if (yesno()) {
        r.anony |= anony_no_fullscreen;
      }
      break;
    }
  } while (!done && !a()->hangup_);
  a()->subs().set_sub(n, r);
}

static void swap_subs(int sub1, int sub2) {
  subconf_t sub1conv = (subconf_t) sub1;
  subconf_t sub2conv = (subconf_t) sub2;

  if (sub1 < 0 || sub1 >= size_int(a()->subs().subs()) || sub2 < 0 || sub2 >= size_int(a()->subs().subs())) {
    return;
  }

  update_conf(ConferenceType::CONF_SUBS, &sub1conv, &sub2conv, CONF_UPDATE_SWAP);
  sub1 = static_cast<int>(sub1conv);
  sub2 = static_cast<int>(sub2conv);
  int nNumUserRecords = a()->users()->num_user_records();

  std::unique_ptr<uint32_t[]> pTempQScan = std::make_unique<uint32_t[]>(a()->config()->qscn_len());
  for (int i = 1; i <= nNumUserRecords; i++) {
    int i1, i2;
    read_qscn(i, pTempQScan.get(), true);
    uint32_t *pTempQScan_n = &pTempQScan.get()[1];
    uint32_t *pTempQScan_q = pTempQScan_n + (a()->config()->max_dirs() + 31) / 32;
    uint32_t *pTempQScan_p = pTempQScan_q + (a()->config()->max_subs() + 31) / 32;

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
    uint32_t tl = pTempQScan_p[sub1];
    pTempQScan_p[sub1] = pTempQScan_p[sub2];
    pTempQScan_p[sub2] = tl;

    write_qscn(i, pTempQScan.get(), true);
  }
  close_qscn();

  subboard_t sbt = a()->subs().sub(sub1);
  a()->subs().set_sub(sub1, a()->subs().sub(sub2));
  a()->subs().set_sub(sub2, sbt);

  save_subs();
}

static void insert_sub(int n) {
  int i, i1, i2;
  uint32_t m1, m2, m3;
  subconf_t nconv = (subconf_t) n;

  if (n < 0 || n > size_int(a()->subs().subs())) {
    return;
  }

  // Don't insert the sub into all conferences. Not sure why wwiv used
  // to do this here.
  // update_conf(ConferenceType::CONF_SUBS, &nconv, nullptr, CONF_UPDATE_INSERT);

  n = static_cast<int>(nconv);

  subboard_t r = {};
  r.name = "** New WWIV Message Sub **";
  r.filename = "NONAME";
  r.key = 0;
  r.readsl = 10;
  r.postsl = 20;
  r.anony = 0;
  r.age = 0;
  r.maxmsgs = 50;
  r.ar = 0;
  r.storage_type = 2;

  // Insert new item.
  a()->subs().insert(n, r);

  auto nNumUserRecords = a()->users()->num_user_records();

  auto pTempQScan = std::make_unique<uint32_t[]>(a()->config()->qscn_len());
  uint32_t* pTempQScan_n = &pTempQScan.get()[1];
  uint32_t* pTempQScan_q = pTempQScan_n + (a()->config()->max_dirs() + 31) / 32;
  uint32_t* pTempQScan_p = pTempQScan_q + (a()->config()->max_subs() + 31) / 32;

  m1 = 1L << (n % 32);
  m2 = 0xffffffff << ((n % 32) + 1);
  m3 = 0xffffffff >> (32 - (n % 32));

  for (i = 1; i <= nNumUserRecords; i++) {
    read_qscn(i, pTempQScan.get(), true);

    if ((pTempQScan[0] != 999) && (pTempQScan[0] >= static_cast<uint32_t>(n))) {
      (pTempQScan[0])++;
    }

    for (i1 = size_int(a()->subs().subs()) - 1; i1 > n; i1--) {
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

  if (a()->GetCurrentReadMessageArea() >= n) {
    a()->SetCurrentReadMessageArea(a()->GetCurrentReadMessageArea() + 1);
  }
}

static void delete_sub(int n) {
  int i, i1, i2, nNumUserRecords;
  subconf_t nconv = static_cast<subconf_t>(n);

  if (n < 0 || n >= size_int(a()->subs().subs())) {
    return;
  }

  update_conf(ConferenceType::CONF_SUBS, &nconv, nullptr, CONF_UPDATE_DELETE);

  n = static_cast<int>(nconv);

  while (size_int(a()->subs().subs()) > n && !a()->subs().sub(n).nets.empty()) {
    sub_xtr_del(n, 0, 1);
  }
  a()->subs().erase(n);
  nNumUserRecords = a()->users()->num_user_records();

  uint32_t *pTempQScan_n, *pTempQScan_q, *pTempQScan_p, m2, m3;
  auto pTempQScan = std::make_unique<uint32_t[]>(a()->config()->qscn_len() + 1);
  pTempQScan_n = &pTempQScan.get()[1];
  pTempQScan_q = pTempQScan_n + (a()->config()->max_dirs() + 31) / 32;
  pTempQScan_p = pTempQScan_q + (a()->config()->max_subs() + 31) / 32;

  m2 = 0xffffffff << (n % 32);
  m3 = 0xffffffff >> (32 - (n % 32));

  for (i = 1; i <= nNumUserRecords; i++) {
    read_qscn(i, pTempQScan.get(), true);

    if (pTempQScan[0] != 999) {
      if (pTempQScan[0] == static_cast<uint32_t>(n)) {
        pTempQScan[0] = 999;
      } else if (pTempQScan[0] > static_cast<uint32_t>(n)) {
        pTempQScan[0]--;
      }
    }
    for (i1 = n; i1 < size_int(a()->subs().subs()); i1++) {
      pTempQScan_p[i1] = pTempQScan_p[i1 + 1];
    }

    pTempQScan_q[n / 32] = (pTempQScan_q[n / 32] & m3) | ((pTempQScan_q[n / 32] >> 1) & m2) |
                            (pTempQScan_q[(n / 32) + 1] << 31);

    for (i2 = (n / 32) + 1; i2 <= (size_int(a()->subs().subs()) / 32); i2++) {
      pTempQScan_q[i2] = (pTempQScan_q[i2] >> 1) | (pTempQScan_q[i2 + 1] << 31);
    }

    write_qscn(i, pTempQScan.get(), true);
  }
  close_qscn();
  save_subs();

  if (a()->GetCurrentReadMessageArea() == n) {
    a()->SetCurrentReadMessageArea(-1);
  } else if (a()->GetCurrentReadMessageArea() > n) {
    a()->SetCurrentReadMessageArea(a()->GetCurrentReadMessageArea() - 1);
  }
}

void boardedit() {
  bool confchg = false;
  subconf_t iconv;

  if (!ValidateSysopPassword()) {
    return;
  }
  showsubs();
  bool done = false;
  a()->status_manager()->RefreshStatusCache();
  do {
    bout.nl();
    bout << "|#7(Q=Quit) (D)elete, (I)nsert, (M)odify, (S)wapSubs : ";
    char ch = onek("QSDIM?");
    switch (ch) {
    case '?':
      showsubs();
      break;
    case 'Q':
      done = true;
      break;
    case 'M':
    {
      bout.nl();
      bout << "|#2Sub number? ";
      int subnum = input_number(-1, 0, size_int(a()->subs().subs()) - 1, false);
      if (subnum >= 0) {
        modify_sub(subnum);
      }
    } break;
    case 'S':
    {
      if (a()->subs().subs().size() < a()->config()->max_subs()) {
        bout.nl();
        bout << "|#2Take sub number? ";
        int subnum1 = input_number(-1, 0, size_int(a()->subs().subs()) - 1, false);
        if (subnum1 <= 0) {
          break;
        }
        bout.nl();
        bout << "|#2And move before sub number? ";
        int subnum2 = input_number(-1, 1, size_int(a()->subs().subs()) - 1, false);
        if (subnum2 <= 0) {
          break;
        }
        bout.nl();
        if (subnum2 < subnum1) {
          subnum1++;
        }
        write_qscn(a()->usernum, a()->context().qsc, true);
        bout << "|#1Moving sub now...Please wait...";
        insert_sub(subnum2);
        swap_subs(subnum1, subnum2);
        delete_sub(subnum1);
        confchg = true;
        showsubs();
      } else {
        bout << "\r\nYou must increase the number of subs in wwivconfig first.\r\n";
      }
    } break;
    case 'I':
    {
      if (a()->subs().subs().size() >= a()->config()->max_subs()) {
        break;
      }
      bout.nl();
      bout << "|#2Insert before which sub ('$' for end) : ";
      auto s = input(4);
      subconf_t subnum = 0;
      if (s[0] == '$') {
        subnum = static_cast<subconf_t>(size_int(a()->subs().subs()));
      } else {
        subnum = to_number<uint16_t>(s);
      }
      if (!s.empty() && subnum >= 0 && subnum <= size_int(a()->subs().subs())) {
        insert_sub(subnum);
        modify_sub(subnum);
        confchg = true;
        if (a()->subconfs.size() > 1) {
          bout.nl();
          list_confs(ConferenceType::CONF_SUBS, 0);
          int i2 = select_conf("Put in which conference? ", ConferenceType::CONF_SUBS, 0);
          if (i2 >= 0) {
            if (!in_conference(subnum, &(a()->subconfs[i2]))) {
              iconv = (subconf_t)subnum;
              addsubconf(ConferenceType::CONF_SUBS, &a()->subconfs[i2], &iconv);
              subnum = static_cast<int>(iconv);
            }
          }
        } else {
          if (!in_conference(subnum, &a()->subconfs[0])) {
            iconv = static_cast<subconf_t>(subnum);
            addsubconf(ConferenceType::CONF_SUBS, &a()->subconfs[0], &iconv);
            subnum = static_cast<int>(iconv);
          }
        }
      }
    } break;
    case 'D':
    {
      bout.nl();
      bout << "|#2Delete which sub? ";
      int subnum = input_number(-1, 1, size_int(a()->subs().subs()) - 1, false);
      if (subnum >= 0) {
        bout.nl();
        bout << "|#5Delete " << a()->subs().sub(subnum).name << "? ";
        if (yesno()) {
          auto fn = a()->subs().sub(subnum).filename;
          delete_sub(subnum);
          confchg = true;
          bout.nl();
          bout << "|#5Delete data files (including messages) for sub also? ";
          if (yesno()) {
            File::Remove(PathFilePath(a()->config()->datadir(), StrCat(fn, ".sub")));
            File::Remove(PathFilePath(a()->config()->msgsdir(), StrCat(fn, ".dat")));
          }
        }
      }
    } break;
    }
  } while (!done && !a()->hangup_);
  save_subs();
  if (!a()->at_wfc()) {
    changedsl();
  }
  a()->subchg = 1;
  if (confchg) {
    save_confs(ConferenceType::CONF_SUBS);
  }
}
