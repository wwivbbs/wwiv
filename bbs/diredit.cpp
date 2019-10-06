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
#include <string>

#include "bbs/bbsutl.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/conf.h"
#include "bbs/confutil.h"
#include "bbs/input.h"
#include "local_io/keycodes.h"
#include "bbs/bbs.h"
#include "bbs/pause.h"
#include "bbs/sysoplog.h"
#include "bbs/wqscn.h"
#include "core/datafile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/filenames.h"
#include "sdk/usermanager.h"
#include "sdk/user.h"


using std::string;
using wwiv::bbs::InputMode;
using wwiv::core::DataFile;
using namespace wwiv::core;
using namespace wwiv::stl;
using namespace wwiv::strings;

//
// Local Function Prototypes
//

void modify_dir(int n);
void swap_dirs(int dir1, int dir2);
void insert_dir(int n);
void delete_dir(int n);


static std::string dirdata(int n) {
  char x = 0;
  directoryrec r = a()->directories[n];
  if (r.dar == 0) {
    x = 32;
  } else {
    for (int i = 0; i < 16; i++) {
      if ((1 << i) & r.dar) {
        x = static_cast<char>('A' + i);
      }
    }
  }
  return fmt::sprintf("|#2%4d |#9%1c   |#1%-39.39s |#2%-8s |#9%-3d %-3d %-3d %-9.9s", n, x,
                      stripcolors(r.name), r.filename, r.dsl, r.age, r.maxfiles, r.path);
}

static void showdirs() {
  bout.cls();
  bout << "|#7(|#1File Areas Editor|#7) Enter Substring: ";
  auto pattern = input_text(20);
  bool abort = false;
  bout.bpla("|#2##   DAR Area Description                        FileName DSL AGE FIL PATH", &abort);
  bout.bpla("|#7==== --- ======================================= -------- === --- === ---------", &abort);
  for (size_t i = 0; i < a()->directories.size() && !abort; i++) {
    auto text = StrCat(a()->directories[i].name, " ", a()->directories[i].filename);
    if (strcasestr(text.c_str(), pattern.c_str())) {
      bout.bpla(dirdata(i), &abort);
    }
  }
}

static string GetAttributeString(const directoryrec& r) {
  if (r.dar != 0) {
    for (int i = 0; i < 16; i++) {
      if ((1 << i) & r.dar) {
        return string(1, static_cast<char>('A' + i));
      }
    }
  }
  return "None.";
}

void modify_dir(int n) {
  directoryrec r = a()->directories[n];
  bool done = false;
  do {
    bout.cls();
    bout.litebar(StrCat("Editing File Area #", n));
    bout << "|#9A) Name       : |#2" << r.name << wwiv::endl;
    bout << "|#9B) Filename   : |#2" << r.filename << wwiv::endl;
    bout << "|#9C) Path       : |#2" << r.path << wwiv::endl;
    bout << "|#9D) DSL        : |#2" << static_cast<int>(r.dsl) << wwiv::endl;
    bout << "|#9E) Min. Age   : |#2" << static_cast<int>(r.age) << wwiv::endl;
    bout << "|#9F) Max Files  : |#2" << r.maxfiles << wwiv::endl;
    bout << "|#9G) DAR        : |#2" << GetAttributeString(r)  << wwiv::endl;
    bout << "|#9H) Require PD : |#2" << YesNoString((r.mask & mask_PD) ? true : false) << wwiv::endl;
    bout << "|#9I) Dir Type   : |#2" <<  r.type << wwiv::endl;
    bout << "|#9J) Uploads    : |#2" << ((r.mask & mask_no_uploads) ? "Disallowed" : "Allowed") << wwiv::endl;
    bout << "|#9K) Arch. only : |#2" << YesNoString((r.mask & mask_archive) ? true : false) << wwiv::endl;
    bout << "|#9L) Drive Type : |#2" << ((r.mask & mask_cdrom) ? "|#3CD ROM" : "HARD DRIVE") << wwiv::endl;
    if (r.mask & mask_cdrom) {
      bout << "|#9M) Available  : |#2" << YesNoString((r.mask & mask_offline) ? true : false) << wwiv::endl;
    }
    bout << "|#9N) //UPLOADALL: |#2" << YesNoString((r.mask & mask_uploadall) ? true : false) << wwiv::endl;
    bout << "|#9O) WWIV Reg   : |#2" << YesNoString((r.mask & mask_wwivreg) ? true : false) << wwiv::endl;
    bout.nl();
    bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1M|#7,|#1[|#7,|#1]|#7) : ";
    char ch = onek("QABCDEFGHIJKLMNO[]", true);
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case '[':
      a()->directories[n] = r;
      if (--n < 0) {
        n = size_int(a()->directories) - 1;
      }
      r = a()->directories[n];
      break;
    case ']':
      a()->directories[n] = r;
      if (++n >= size_int(a()->directories)) {
        n = 0;
      }
      r = a()->directories[n];
      break;
    case 'A':
    {
      bout.nl();
      bout << "|#2New name? ";
      auto s = input_text(r.name, 40);
      if (!s.empty()) {
        to_char_array(r.name, s);
      }
    } break;
    case 'B':
    {
      bout.nl();
      bout << "|#2New filename? ";
      auto s = input_filename(r.filename, 8);
      if (!s.empty() && !contains(s, '.')) {
        to_char_array(r.filename, s);
      }
    } break;
    case 'C':
    {
      bout.nl();
      bout << "|#9Enter new path, optionally with drive specifier.\r\n"
           << "|#9No backslash on end.\r\n\n"
           << "|#9The current path is:\r\n"
           << "|#1" << r.path << wwiv::endl << wwiv::endl;
      bout << " \b";
      auto s = input_path(r.path, 79);
      if (!s.empty()) {
        const string dir{s};
        if (!File::Exists(dir)) {
          a()->CdHome();
          if (!File::mkdirs(dir)) {
            bout << "|#6Unable to create or change to directory." << wwiv::endl;
            pausescr();
            s.clear();
          }
        }
        if (!s.empty()) {
          s = File::EnsureTrailingSlash(s);
          to_char_array(r.path, s);
          bout.nl(2);
          bout << "|#3The path for this directory is changed.\r\n";
          bout << "|#9If there are any files in it, you must manually move them to the new directory.\r\n";
          pausescr();
        }
      }
    } break;
    case 'D': {
      bout.nl();
      bout << "|#2New DSL? ";
      r.dsl = input_number(r.dsl);
    }
    break;
    case 'E': {
      bout.nl();
      bout << "|#2New Min Age? ";
      r.age = input_number(r.age);
    }
    break;
    case 'F':
    {
      bout.nl();
      bout << "|#2New max files? ";
      r.maxfiles = input_number(r.maxfiles);
    } break;
    case 'G':
    {
      bout.nl();
      bout << "|#2New DAR (<SPC>=None) ? ";
      char ch2 = onek("ABCDEFGHIJKLMNOP ");
      if (ch2 == SPACE) {
        r.dar = 0;
      } else {
        r.dar = 1 << (ch2 - 'A');
      }
    } break;
    case 'H':
      r.mask ^= mask_PD;
      break;
    case 'I':
      bout << "|#2New Dir Type? ";
      r.type = input_number(r.type, 0, 9999);
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
      if (yesno()) {
        r.mask |= mask_wwivreg;
      }
      break;
    }
  } while (!done && !a()->hangup_);

  a()->directories[n] = r;
}


void swap_dirs(int dir1, int dir2) {
  subconf_t dir1conv = static_cast<subconf_t>(dir1);
  subconf_t dir2conv = static_cast<subconf_t>(dir2);

  if (dir1 < 0 || dir1 >= size_int(a()->directories) || dir2 < 0 || dir2 >= size_int(a()->directories)) {
    return;
  }

  update_conf(ConferenceType::CONF_DIRS, &dir1conv, &dir2conv, CONF_UPDATE_SWAP);

  dir1 = static_cast<int>(dir1conv);
  dir2 = static_cast<int>(dir2conv);

  int nNumUserRecords = a()->users()->num_user_records();

  uint32_t *pTempQScan = static_cast<uint32_t*>(BbsAllocA(a()->config()->qscn_len()));
  if (pTempQScan) {
    for (int i = 1; i <= nNumUserRecords; i++) {
      read_qscn(i, pTempQScan, true);
      uint32_t* pTempQScan_n = pTempQScan + 1;

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
  directoryrec drt = a()->directories[dir1];
  a()->directories[dir1] = a()->directories[dir2];
  a()->directories[dir2] = drt;
}

void insert_dir(int n) {
  if (n < 0 || n > size_int(a()->directories)) {
    return;
  }
  subconf_t nconv = static_cast<subconf_t>(n);

  update_conf(ConferenceType::CONF_DIRS, &nconv, nullptr, CONF_UPDATE_INSERT);

  n = static_cast<int>(nconv);

  directoryrec r{};
  to_char_array(r.name, "** NEW DIR **");
  to_char_array(r.filename, "noname");
  to_char_array(r.path, a()->config()->dloadsdir());
  r.dsl = 10;
  r.age = 0;
  r.maxfiles = 50;
  r.dar = 0;
  r.type = 0;
  r.mask = 0;

  {
    insert_at(a()->directories, n, r);
  }
  int nNumUserRecords = a()->users()->num_user_records();

  uint32_t* pTempQScan = static_cast<uint32_t*>(BbsAllocA(a()->config()->qscn_len()));
  if (pTempQScan) {
   uint32_t* pTempQScan_n = pTempQScan + 1;

    uint32_t m1 = 1L << (n % 32);
    uint32_t m2 = 0xffffffff << ((n % 32) + 1);
    uint32_t m3 = 0xffffffff >> (32 - (n % 32));

    for (int i = 1; i <= nNumUserRecords; i++) {
      read_qscn(i, pTempQScan, true);

      int i1;
      for (i1 = size_int(a()->directories) / 32; i1 > n / 32; i1--) {
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
  uint32_t *pTempQScan, *pTempQScan_n, m2, m3;
  subconf_t nconv{static_cast<subconf_t>(n)};

  if ((n < 0) || (n >= size_int(a()->directories))) {
    return;
  }

  update_conf(ConferenceType::CONF_DIRS, &nconv, nullptr, CONF_UPDATE_DELETE);

  n = static_cast<int>(nconv);
  erase_at(a()->directories, n);

  auto num_users = a()->users()->num_user_records();

  pTempQScan = static_cast<uint32_t*>(BbsAllocA(a()->config()->qscn_len()));
  if (pTempQScan) {
    pTempQScan_n = pTempQScan + 1;

    m2 = 0xffffffff << (n % 32);
    m3 = 0xffffffff >> (32 - (n % 32));

    for (int i = 1; i <= num_users; i++) {
      read_qscn(i, pTempQScan, true);

      pTempQScan_n[n / 32] = (pTempQScan_n[n / 32] & m3) | ((pTempQScan_n[n / 32] >> 1) & m2) |
                             (pTempQScan_n[(n / 32) + 1] << 31);

      for (size_t i1 = (n / 32) + 1; i1 <= (a()->directories.size() / 32); i1++) {
        pTempQScan_n[i1] = (pTempQScan_n[i1] >> 1) | (pTempQScan_n[i1 + 1] << 31);
      }

      write_qscn(i, pTempQScan, true);
    }
    close_qscn();
    free(pTempQScan);
  }
}

void dlboardedit() {
  int i1, i2, confchg = 0;
  char s[81], ch;

  if (!ValidateSysopPassword()) {
    return;
  }
  showdirs();
  bool done = false;
  do {
    bout.nl();
    bout << "|#7(Q=Quit) (D)elete, (I)nsert, (M)odify, (S)wapDirs : ";
    ch = onek("QSDIM?");
    switch (ch) {
    case '?':
      showdirs();
      break;
    case 'Q':
      done = true;
      break;
    case 'M':
    {
      bout.nl();
      bout << "|#2(Q=Quit) Dir number? ";
      auto r = input_number_hotkey(0, {'Q'}, 0, size_int(a()->directories));
      if (r.key == 'Q') {
        break;
      }
      modify_dir(r.num);
    } break;
    case 'S':
      if (a()->directories.size() < a()->config()->max_dirs()) {
        bout.nl();
        bout << "|#2Take dir number? ";
        input(s, 4);
        i1 = to_number<int>(s);
        if (!s[0] || i1 < 0 || i1 >= size_int(a()->directories)) {
          break;
        }
        bout.nl();
        bout << "|#2And put before dir number? ";
        input(s, 4);
        i2 = to_number<int>(s);
        if ((!s[0]) || (i2 < 0) || (i2 % 32 == 0) || (i2 > size_int(a()->directories)) || (i1 == i2)) {
          break;
        }
        bout.nl();
        if (i2 < i1) {
          i1++;
        }
        write_qscn(a()->usernum, a()->context().qsc, true);
        bout << "|#1Moving dir now...Please wait...";
        insert_dir(i2);
        swap_dirs(i1, i2);
        delete_dir(i1);
        confchg = 1;
        showdirs();
      } else {
        bout << "\r\n|#6You must increase the number of dirs in wwivconfig first.\r\n";
      }
      break;
    case 'I':
      if (a()->directories.size() < a()->config()->max_dirs()) {
        bout.nl();
        bout << "|#2Insert before which dir? ";
        input(s, 4);
        subconf_t i = to_number<uint16_t>(s);
        if ((s[0] != 0) && (i >= 0) && (i <= size_int(a()->directories))) {
          insert_dir(i);
          modify_dir(i);
          confchg = 1;
          if (a()->dirconfs.size() > 1) {
            bout.nl();
            list_confs(ConferenceType::CONF_DIRS, 0);
            i2 = select_conf("Put in which conference? ", ConferenceType::CONF_DIRS, 0);
            if (i2 >= 0) {
              if (!in_conference(i, &a()->dirconfs[i2])) {
                addsubconf(ConferenceType::CONF_DIRS, &a()->dirconfs[i2], &i);
              }
            }
          } else {
            if (!in_conference(i, &a()->dirconfs[0])) {
              addsubconf(ConferenceType::CONF_DIRS, &a()->dirconfs[0], &i);
            }
          }
        }
      }
      break;
    case 'D':
    {
      bout.nl();
      bout << "|#2Delete which dir? ";
      input(s, 4);
      auto i = to_number<int>(s);
      if ((s[0] != 0) && (i >= 0) && (i < size_int(a()->directories))) {
        bout.nl();
        bout << "|#5Delete " << a()->directories[i].name << "? ";
        if (yesno()) {
          strcpy(s, a()->directories[i].filename);
          delete_dir(i);
          confchg = 1;
          bout.nl();
          bout << "|#5Delete data files (.DIR/.EXT) for dir also? ";
          if (yesno()) {
            File::Remove(PathFilePath(a()->config()->datadir(), StrCat(s, ".dir")));
            File::Remove(PathFilePath(a()->config()->datadir(), StrCat(s, ".ext")));
          }
        }
      }
    } break;
    }
  } while (!done && !a()->hangup_);
  DataFile<directoryrec> dirsFile(PathFilePath(a()->config()->datadir(), DIRS_DAT),
      File::modeReadWrite | File::modeCreateFile | File::modeBinary | File::modeTruncate);
  if (!dirsFile) {
    sysoplog(false) << "!!! Unable to open DIRS.DAT for writing, some changes may have been lost";
  } else {
    dirsFile.WriteVector(a()->directories);
  }
  if (confchg) {
    save_confs(ConferenceType::CONF_DIRS);
  }
  if (!a()->at_wfc()) {
    changedsl();
  }
}
