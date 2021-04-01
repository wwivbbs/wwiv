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

#include "bbs/acs.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/wqscn.h"
#include "common/com.h"
#include "common/input.h"
#include "core/datafile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/usermanager.h"
#include "sdk/files/dirs.h"
#include "sdk/net/networks.h"
#include <string>

using wwiv::common::InputMode;
using wwiv::core::DataFile;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::sdk::net;
using namespace wwiv::stl;
using namespace wwiv::strings;

//
// Local Function Prototypes
//

void modify_dir(int n);
void swap_dirs(int dir1, int dir2);
void insert_dir(int n);
void delete_dir(int n);

static std::string tail(const std::string& s, int len) {
  return len >= ssize(s) ? s : s.substr(s.size() - len);
}

static std::string dirdata(int n) {
  const auto& r = a()->dirs()[n];
  return fmt::sprintf("|#2%4d |#1%-24.24s |#2%-8s |#9%-3d |#3%-23.23s |#1%s", n, stripcolors(r.name), r.filename,
                      r.maxfiles, tail(r.path, 23), r.conf.to_string());
}

static void showdirs() {
  bout.cls();
  bout << "|#7(|#1File Areas Editor|#7) Enter Substring: ";
  const auto pattern = bin.input_text(20);
  bout.cls();
  bool abort = false;
  bout.bpla("|#2##   Description              Filename Num Path                    Conf", &abort);
  bout.bpla("|#7==== ======================== -------- === ----------------------- -------",
            &abort);
  for (auto i = 0; i < wwiv::stl::ssize(a()->dirs()) && !abort; i++) {
    auto text = StrCat(a()->dirs()[i].name, " ", a()->dirs()[i].filename);
    if (ifind_first(text, pattern)) {
      bout.bpla(dirdata(i), &abort);
    }
  }
}

std::optional<Network> select_network() {
  std::map<int, Network> nets;
  auto num = 0;
  for (const auto& n : a()->nets().networks()) {
    if (n.type == network_type_t::ftn) {
      nets.emplace(++num, n);
    }
  }

  bout << "|#5Networks: " << wwiv::endl;
  for (const auto& n : nets) {
    bout << "|#1" << n.first << "|#9) |#2" << n.second.name << wwiv::endl;
  }
  bout.nl();
  bout << "|#2(Q=Quit) Select Network Number : ";
  const auto r = bin.input_number_hotkey(0, {'Q'}, 1, num, false);
  if (r.key == 'Q') {
    return std::nullopt;
  }
  auto it = nets.find(r.num);
  if (it == std::end(nets)) {
    return std::nullopt;
  }
  return {it->second};
}

enum class list_area_tags_style_t { number, indent };
static void list_area_tags(const std::vector<wwiv::sdk::files::dir_area_t>& area_tags,
                           list_area_tags_style_t style) {
  if (area_tags.empty()) {
    bout << "|#6(None)" << wwiv::endl;
    return;
  }
  Network empty(network_type_t::unknown, "(Unknown)");

  auto nn = 0;
  auto first{true};
  for (const auto& t : area_tags) {
    if (style == list_area_tags_style_t::number) {
      bout << "|#2" << nn++ << "|#9) ";
    } else if (style == list_area_tags_style_t::indent) {
      if (!first) {
        bout << "                  ";
      }
      first = false;
    }
    bout << "|#1" << t.area_tag << "|#9@|#5" << a()->nets().by_uuid(t.net_uuid).value_or(empty).name
         << wwiv::endl;
  }
}

static void edit_ftn_area_tags(std::vector<wwiv::sdk::files::dir_area_t>& area_tags) {
  auto done{false};
  do {
    bout.cls();
    bout.litebar("Editing File Area Tags");
    bout.nl();
    list_area_tags(area_tags, list_area_tags_style_t::number);

    bout.nl();
    bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#2A|#7dd, |#2E|#7dit, or |#2D|#7elete) : ";
    const auto ch = onek("QAED", true);
    switch (ch) {
    case 'A': {
      files::dir_area_t da{};
      bout << "Enter Name? ";
      da.area_tag = bin.input_upper(12);
      const auto r = select_network();
      if (!r) {
        break;
      }
      da.net_uuid = r.value().uuid;
      area_tags.push_back(da);
    } break;
    case 'D': {
      if (area_tags.empty()) {
        break;
      }
      bout << "(Q=Quit, 1=" << area_tags.size() << ") Enter Number? ";
      const auto r = bin.input_number_hotkey(0, {'Q'}, 1, size_int(area_tags), false);
      if (r.key == 'Q') {
        break;
      }
      bout << "Are you sure?";
      if (bin.noyes()) {
        erase_at(area_tags, r.num - 1);
      }
    } break;
    case 'E': {
      bout << "Edit!";
      if (area_tags.empty()) {
        break;
      }
      bout << "(Q=Quit, 1=" << area_tags.size() << ") Enter Number? ";
      const auto r = bin.input_number_hotkey(0, {'Q'}, 1, size_int(area_tags), false);
      if (r.key == 'Q') {
        break;
      }
      files::dir_area_t da{};
      bout << "Enter Name? ";
      da.area_tag = bin.input_upper(12);
      const auto nr = select_network();
      if (!nr) {
        break;
      }
      da.net_uuid = nr.value().uuid;
      bout << "Are you sure?";
      if (bin.noyes()) {
        area_tags[r.num - 1] = da;
      }
    } break;
    case 'Q':
      done = true;
      break;
    }
  } while (!done && !a()->sess().hangup());
}

void modify_dir(int n) {
  auto r = a()->dirs()[n];
  auto done = false;
  do {
    bout.cls();
    bout.litebar(StrCat("Editing File Area #", n));
    bout << "|#9A) Name         : |#2" << r.name << wwiv::endl;
    bout << "|#9B) Filename     : |#2" << r.filename << wwiv::endl;
    bout << "|#9C) Path         : |#2" << r.path << wwiv::endl;
    bout << "|#9D) ACS          : |#2" << r.acs << wwiv::endl;
    bout << "|#9F) Max Files    : |#2" << r.maxfiles << wwiv::endl;
    bout << "|#9H) Require PD   : |#2" << YesNoString((r.mask & mask_PD) ? true : false)
         << wwiv::endl;
    bout << "|#9J) Uploads      : |#2" << ((r.mask & mask_no_uploads) ? "Disallowed" : "Allowed")
         << wwiv::endl;
    bout << "|#9K) Arch. only   : |#2" << YesNoString((r.mask & mask_archive) ? true : false)
         << wwiv::endl;
    bout << "|#9L) Drive Type   : |#2" << ((r.mask & mask_cdrom) ? "|#3CD ROM" : "HARD DRIVE")
         << wwiv::endl;
    if (r.mask & mask_cdrom) {
      bout << "|#9M) Available    : |#2" << YesNoString((r.mask & mask_offline) ? true : false)
           << wwiv::endl;
    }
    bout << "|#9N) //UPLOADALL  : |#2" << YesNoString((r.mask & mask_uploadall) ? true : false)
         << wwiv::endl;
    bout << "|#9O) WWIV Reg     : |#2" << YesNoString((r.mask & mask_wwivreg) ? true : false)
         << wwiv::endl;
    bout << "|#9T) FTN Area Tags: |#2";
    list_area_tags(r.area_tags, list_area_tags_style_t::indent);
    bout << "|#9   Conferences  : |#2" << r.conf.to_string() << wwiv::endl;
    bout.nl(2);
    bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1O|#7,|#1[T|#7,|#1[|#7,|#1]|#7) : ";
    const auto ch = onek("QABCDEFGHJKLMNOT[]", true);
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case '[':
      a()->dirs()[n] = r;
      if (--n < 0) {
        n = a()->dirs().size() - 1;
      }
      r = a()->dirs()[n];
      break;
    case ']':
      a()->dirs()[n] = r;
      if (++n >= a()->dirs().size()) {
        n = 0;
      }
      r = a()->dirs()[n];
      break;
    case 'A': {
      bout.nl();
      bout << "|#2New name? ";
      auto s = bin.input_text(r.name, 40);
      if (!s.empty()) {
        r.name = s;
      }
    } break;
    case 'B': {
      bout.nl();
      bout << "|#2New filename? ";
      auto s = bin.input_filename(r.filename, 8);
      if (!s.empty() && !contains(s, '.')) {
        r.filename = s;
      }
    } break;
    case 'C': {
      bout.nl();
      bout << "|#9Enter new path, optionally with drive specifier.\r\n"
           << "|#9No backslash on end.\r\n\n"
           << "|#9The current path is:\r\n"
           << "|#1" << r.path << wwiv::endl
           << wwiv::endl;
      bout << " \b";
      if (auto s = bin.input_path(r.path, 79); !s.empty()) {
        const std::string dir{s};
        if (!File::Exists(dir)) {
          File::set_current_directory(a()->bbspath());
          if (!File::mkdirs(dir)) {
            bout << "|#6Unable to create or change to directory." << wwiv::endl;
            bout.pausescr();
            s.clear();
          }
        }
        if (!s.empty()) {
          s = File::EnsureTrailingSlash(s);
          r.path = s;
          bout.nl(2);
          bout << "|#3The path for this directory is changed.\r\n";
          bout << "|#9If there are any files in it, you must manually move them to the new "
                  "directory.\r\n";
          bout.pausescr();
        }
      }
    } break;
    case 'D': {
      bout.nl();
      r.acs = wwiv::bbs::input_acs(bin, bout, "New ACS?", r.acs, 77);
    } break;
    case 'F': {
      bout.nl();
      bout << "|#2New max files? ";
      r.maxfiles = bin.input_number(r.maxfiles);
    } break;
    case 'H':
      r.mask ^= mask_PD;
      break;
    case 'J':
      r.mask ^= mask_no_uploads;
      break;
    case 'K':
      r.mask ^= mask_archive;
      break;
    case 'L':
      r.mask ^= mask_cdrom;
      if (r.mask & mask_cdrom) {
        r.mask |= mask_no_uploads;
      } else {
        r.mask &= ~mask_offline;
      }
      break;
    case 'M':
      if (r.mask & mask_cdrom) {
        r.mask ^= mask_offline;
      }
      break;
    case 'N':
      r.mask ^= mask_uploadall;
      break;
    case 'O':
      r.mask &= ~mask_wwivreg;
      bout.nl();
      bout << "|#5Require WWIV 4.xx registration? ";
      if (bin.yesno()) {
        r.mask |= mask_wwivreg;
      }
      break;
    case 'T': {
      edit_ftn_area_tags(r.area_tags);
    } break;
    }
  } while (!done && !a()->sess().hangup());

  a()->dirs()[n] = r;
}

void swap_dirs(int dir1, int dir2) {
  if (dir1 < 0 || dir1 >= a()->dirs().size() || dir2 < 0 || dir2 >= a()->dirs().size()) {
    return;
  }
  const int num_user_records = a()->users()->num_user_records();

  if (auto * pTempQScan = static_cast<uint32_t*>(BbsAllocA(a()->config()->qscn_len())); pTempQScan) {
    for (auto i = 1; i <= num_user_records; i++) {
      read_qscn(i, pTempQScan, true);
      auto* pTempQScan_n = pTempQScan + 1;

      int i1 = 0;
      if (pTempQScan_n[dir1 / 32] & (1L << (dir1 % 32))) {
        i1 = 1;
      }

      int i2 = 0;
      if (pTempQScan_n[dir2 / 32] & (1L << (dir2 % 32))) {
        i2 = 1;
      }
      if (i1 + i2 == 1) {
        pTempQScan_n[dir1 / 32] ^= (1L << (dir1 % 32));
        pTempQScan_n[dir2 / 32] ^= (1L << (dir2 % 32));
      }
      write_qscn(i, pTempQScan, true);
    }
    close_qscn();
    free(pTempQScan);
  }
  const auto drt = a()->dirs()[dir1];
  a()->dirs()[dir1] = a()->dirs()[dir2];
  a()->dirs()[dir2] = drt;
}

void insert_dir(int n) {
  if (n < 0 || n > a()->dirs().size()) {
    return;
  }

  files::directory_t r{};
  r.name = "** NEW DIR **";
  r.filename = "noname";
  r.path = a()->config()->dloadsdir();
  r.acs = "user.sl >= 10";
  r.maxfiles = 500;
  r.mask = 0;

  a()->dirs().insert(n, r);
  const auto num_user_records = a()->users()->num_user_records();

  if (auto * pTempQScan = static_cast<uint32_t*>(BbsAllocA(a()->config()->qscn_len())); pTempQScan) {
    auto* pTempQScan_n = pTempQScan + 1;

    const uint32_t m1 = 1L << (n % 32);
    const uint32_t m2 = 0xffffffff << ((n % 32) + 1);
    const uint32_t m3 = 0xffffffff >> (32 - (n % 32));

    for (int i = 1; i <= num_user_records; i++) {
      read_qscn(i, pTempQScan, true);

      int i1;
      for (i1 = a()->dirs().size() / 32; i1 > n / 32; i1--) {
        pTempQScan_n[i1] = (pTempQScan_n[i1] << 1) | (pTempQScan_n[i1 - 1] >> 31);
      }
      pTempQScan_n[i1] = m1 | (m2 & (pTempQScan_n[i1] << 1)) | (m3 & pTempQScan_n[i1]);

      write_qscn(i, pTempQScan, true);
    }
    close_qscn();
    free(pTempQScan);
  }
}

void delete_dir(int n) {
  if (n < 0 || n >= a()->dirs().size()) {
    return;
  }
  a()->dirs().erase(n);

  const auto num_users = a()->users()->num_user_records();

  const auto pTempQScan = std::make_unique<uint32_t[]>(a()->config()->qscn_len());
  if (pTempQScan) {
    auto* pTempQScan_n = pTempQScan.get() + 1;

    const uint32_t m2 = 0xffffffff << (n % 32);
    const uint32_t m3 = 0xffffffff >> (32 - (n % 32));

    for (auto i = 1; i <= num_users; i++) {
      read_qscn(i, pTempQScan.get(), true);

      pTempQScan_n[n / 32] = (pTempQScan_n[n / 32] & m3) | ((pTempQScan_n[n / 32] >> 1) & m2) |
                             (pTempQScan_n[(n / 32) + 1] << 31);

      for (auto i1 = (n / 32) + 1; i1 <= a()->dirs().size() / 32; i1++) {
        pTempQScan_n[i1] = (pTempQScan_n[i1] >> 1) | (pTempQScan_n[i1 + 1] << 31);
      }

      write_qscn(i, pTempQScan.get(), true);
    }
    close_qscn();
  }
}

void dlboardedit() {

  if (!ValidateSysopPassword()) {
    return;
  }
  showdirs();
  auto done = false;
  do {
    bout.nl();
    bout << "|#9(|#2Q|#9)uit (|#2D|#9)elete, (|#2I|#9)nsert, (|#2M|#9)odify, (|#2S|#9)wapDirs, (|#2C|#9)onference : ";
    char s[81];
    switch (const auto ch = onek("QSDIMC?"); ch) {
    case '?':
      showdirs();
      break;
    case 'C':
      edit_conf_subs(a()->all_confs().dirs_conf());
      break;
    case 'Q':
      done = true;
      break;
    case 'M': {
      bout.nl();
      bout << "|#2(Q=Quit) Dir number? ";
      const auto r = bin.input_number_hotkey(0, {'Q'}, 0, a()->dirs().size());
      if (r.key == 'Q') {
        break;
      }
      modify_dir(r.num);
    } break;
    case 'S':
      if (a()->dirs().size() < a()->config()->max_dirs()) {
        bout.nl();
        bout << "|#2Take dir number? ";
        bin.input(s, 4);
        auto i1 = to_number<int>(s);
        if (!s[0] || i1 < 0 || i1 >= a()->dirs().size()) {
          break;
        }
        bout.nl();
        bout << "|#2And put before dir number? ";
        bin.input(s, 4);
        const auto i2 = to_number<int>(s);
        if (!s[0] || i2 < 0 || i2 % 32 == 0 || i2 > a()->dirs().size() || i1 == i2) {
          break;
        }
        bout.nl();
        if (i2 < i1) {
          i1++;
        }
        write_qscn(a()->sess().user_num(), a()->sess().qsc, true);
        bout << "|#1Moving dir now...Please wait...";
        insert_dir(i2);
        swap_dirs(i1, i2);
        delete_dir(i1);
        showdirs();
      } else {
        bout << "\r\n|#6You must increase the number of dirs in WWIVconfig first.\r\n";
      }
      break;
    case 'I': {
      if (a()->dirs().size() < a()->config()->max_dirs()) {
        bout.nl();
        bout << "|#2Insert before which dir? ";
        bin.input(s, 4);
        const auto i = to_number<int>(s);
        if (s[0] != 0 && i >= 0 && i <= a()->dirs().size()) {
          insert_dir(i);
          modify_dir(i);
        }
      }
    } break;
    case 'D': {
      bout.nl();
      bout << "|#2Delete which dir? ";
      bin.input(s, 4);
      const auto i = to_number<int>(s);
      if (s[0] != 0 && i >= 0 && i < a()->dirs().size()) {
        bout.nl();
        bout << "|#5Delete " << a()->dirs()[i].name << "? ";
        if (bin.yesno()) {
          delete_dir(i);
          bout.nl();
          bout << "|#5Delete data files (.DIR/.EXT) for dir also? ";
          if (bin.yesno()) {
            const auto& fn = a()->dirs()[i].filename;
            File::Remove(FilePath(a()->config()->datadir(), StrCat(fn, ".dir")));
            File::Remove(FilePath(a()->config()->datadir(), StrCat(fn, ".ext")));
          }
        }
      }
    } break;
    }
  } while (!done && !a()->sess().hangup());
  a()->dirs().Save();
  if (!a()->at_wfc()) {
    changedsl();
  }
}
