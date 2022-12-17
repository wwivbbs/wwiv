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
#include "bbs/gfileedit.h"

#include "acs.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl1.h"
#include "common/com.h"
#include "common/datetime.h"
#include "common/input.h"
#include "common/pause.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "sdk/config.h"
#include "sdk/gfiles.h"
#include "sdk/status.h"

#include <string>
#include <vector>

using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

static std::string gfiledata(const gfile_dir_t& r, int nSectionNum) {
  return fmt::sprintf("|#2%2d |#1%-40s  |#2%-8s |#9%-3d |#5%-23s",
          nSectionNum, stripcolors(r.name), r.filename, r.maxfiles, r.acs);
}

static void showsec() {
  bout.cls();
  auto abort = false;
  bout.bpla("|#2NN Name                                      FN       ACS", &abort);
  bout.bpla("|#7-- ----------------------------------------  ======== -----------------------", &abort);
  auto current_section = 0;
  for (const auto& r : a()->gfiles().dirs()) {
    bout.bpla(gfiledata(r, current_section++), &abort);
    if (abort) {
      break;
    }
  }
}

void modify_sec(int n) {
  auto& r = a()->gfiles().dir(n);
  auto done = false;
  do {
    bout.cls();
    bout.litebar(StrCat("Editing G-File Area # ", n));
    bout.print("|#9A) Name      : |#2{}\r\n", r.name);
    bout.print("|#9B) Filename  : |#2{}\r\n", r.filename);
    bout.print("|#9C) ACS       : |#2{}\r\n", r.acs);
    bout.print("|#9D) Max Files : |#2{}\r\n", r.maxfiles);
    bout.nl();
    bout.puts("|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1F,|#1[|#7,|#1]|#7) : ");
    const auto ch = onek("QABCD[]", true);
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case '[':
      a()->gfiles().set_dir(n, r);
      if (--n < 0) {
        n = size_int(a()->gfiles()) - 1;
      }
      r = a()->gfiles().dir(n);
      break;
    case ']':
      a()->gfiles().set_dir(n, r);
      if (++n >= size_int(a()->gfiles())) {
        n = 0;
      }
      r = a()->gfiles().dir(n);
      break;
    case 'A': {
      bout.nl();
      bout.puts("|#2New name? ");
      r.name = bin.input_text(r.name, 40);
    } break;
    case 'B': {
      bout.nl();
      if (File::Exists(FilePath(a()->config()->gfilesdir(), r.filename))) {
        bout.puts("\r\nThere is currently a directory for this g-file section.\r\n");
        bout.puts("If you change the filename, the directory will still be there.\r\n\n");
      }
      bout.nl();
      bout.puts("|#2New filename? ");
      auto fname = bin.input_filename(r.filename, 20);
      if (!fname.empty()) {
        r.filename = fname;
        if (!File::Exists(FilePath(a()->config()->gfilesdir(), r.filename))) {
          bout.nl();
          bout.puts("|#5Create directory for this section? ");
          if (bin.yesno()) {
            File dir(FilePath(a()->config()->gfilesdir(), r.filename));
            File::mkdirs(dir);
          } else {
            bout.puts("\r\nYou will have to create the directory manually, then.\r\n\n");
          }
        } else {
          bout.puts("\r\nA directory already exists under this filename.\r\n\n");
        }
        bout.pausescr();
      }
    } break;
    case 'C': {
      bout.nl();
      r.acs = wwiv::bbs::input_acs(bin, bout, "New ACS?", r.acs, 60);
    } break;
    case 'D': {
      bout.nl();
      bout.puts("|#1Max 99 files/section.\r\n|#2New max files? ");
      r.maxfiles = bin.input_number(r.maxfiles, 0, 999, true);
    } break;
    }
  } while (!done && !a()->sess().hangup());
  a()->gfiles().set_dir(n, r);
}


void insert_sec(int n) {
  gfile_dir_t r{};
  r.filename = "NONAME";
  r.name = "** NEW SECTION **";
  r.acs = "user.sl >= 10";
  r.maxfiles = 99;
  a()->gfiles().insert(n, r);
  modify_sec(n);
}


void delete_sec(int n) {
  a()->gfiles().erase(n);
}


void gfileedit() {
  if (!ValidateSysopPassword()) {
    return;
  }
  showsec();
  auto done = false;
  do {
    bout.nl();
    bout.puts("|#2G-files: D:elete, I:nsert, M:odify, Q:uit, ? : ");
    const auto ch = onek("QDIM?");
    switch (ch) {
    case '?':
      showsec();
      break;
    case 'Q':
      done = true;
      break;
    case 'M': {
      bout.nl();
      bout.puts("|#2Section number? ");
      std::set<char> q{'Q', 27, '\r'};
      const auto t = bin.input_number_hotkey(0, q, 0, size_int(a()->gfiles()), false);
      if (!t.key) {      
        modify_sec(t.num);
      }
      } break;
    case 'I': {
      if (size_int(a()->gfiles()) < a()->max_gfilesec) {
        bout.nl();
        bout.puts("|#2Insert before which section? ");
        std::set<char> q{'Q', 27, '\r'};
        const auto t = bin.input_number_hotkey(0, q, 0, size_int(a()->gfiles()), false);
        if (!t.key) {
          insert_sec(t.num);
        }
      }
      } break;
    case 'D': {
      bout.nl();
      bout.puts("|#2Delete which section? ");
      std::set<char> q{'Q', 27, '\r'};
      const auto t = bin.input_number_hotkey(0, q, 0, size_int(a()->gfiles()), false);
      if (!t.key) {
        bout.nl();
        bout.print("|#5Delete {}?", a()->gfiles().dir(t.num).name);
        if (bin.yesno()) {
          delete_sec(t.num);
        }
      }
      } break;
    }
  } while (!done && !a()->sess().hangup());
  a()->gfiles().Save();
}


bool fill_sec(int sn) {
  const auto nf = size_int(a()->gfiles().dir(sn).files);
  const auto path = FilePath(a()->config()->gfilesdir(), a()->gfiles()[sn].filename);
  const auto filespec = FilePath(path, "*.*");
  FindFiles ff(filespec, FindFiles::FindFilesType::files);
  bool ok{true};
  for (const auto& f : ff) {
    if (a()->sess().hangup() || !ok) {
      break;
    }
    auto found = false;
    auto& g = a()->gfiles().dir(sn).files;
    for (auto i1 = 0; i1 < nf && !found; i1++) {
      if (iequals(f.name,  g[i1].filename)) {
        found = true;
      }
    }
    if (found) {
      continue;
    }
    bout.print("|#2{} : ", f.name);
    auto s1s = bin.input_text(60);
    if (s1s.empty()) {
      bout.puts("|#6Aborted.\r\n");
      return false;
    }
    gfile_t g1{};
    g1.filename = f.name;
    g1.description = s1s;
    g1.daten = daten_t_now();
    a()->gfiles().dir(sn).files.emplace_back(g1);
  }

  if (nf >= a()->gfiles().dir(sn).maxfiles) {
    bout.puts("Section full.\r\n");
  }
  a()->gfiles().Save();
  a()->status_manager()->Run([&](Status& status) {
    status.gfile_date(date());
  });
  return true;
}
