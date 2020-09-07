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
#include "bbs/gfileedit.h"


#include "arword.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl1.h"
#include "common/com.h"
#include "common/datetime.h"
#include "bbs/gfiles.h"
#include "common/input.h"
#include "common/pause.h"
#include "bbs/xfer.h"
#include "core/datafile.h"
#include "core/file.h"
#include "core/findfiles.h"
#include "core/stl.h"
#include "core/strings.h"
#include "fmt/printf.h"
#include "local_io/keycodes.h"
#include "sdk/config.h"
#include "sdk/filenames.h"
#include "sdk/status.h"
#include "sdk/files/files.h"

#include <string>
#include <vector>

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::stl;
using namespace wwiv::strings;

static string gfiledata(const gfiledirrec& r, int nSectionNum) {
  char x = SPACE;
  if (r.ar != 0) {
    for (int i = 0; i < 16; i++) {
      if ((1 << i) & r.ar) {
        x = static_cast<char>('A' + i);
      }
    }
  }
  return fmt::sprintf("|#2%2d |#3%1c  |#1%-40s  |#2%-8s |#9%-3d %-3d %-3d",
          nSectionNum, x, stripcolors(r.name), r.filename, r.sl, r.age, r.maxfiles);
}

static void showsec() {
  bout.cls();
  bool abort = false;
  bout.bpla("|#2NN AR Name                                      FN       SL  AGE MAX", &abort);
  bout.bpla("|#7-- == ----------------------------------------  ======== --- === ---", &abort);
  int current_section = 0;
  for (const auto& r : a()->gfilesec) {
    bout.bpla(gfiledata(r, current_section++), &abort);
    if (abort) {
      break;
    }
  }
}

static string GetArString(gfiledirrec r) {
  return word_to_arstr(r.ar, "None.");
}

void modify_sec(int n) {
  gfiledirrec& r = a()->gfilesec[n];
  bool done = false;
  char s[81];
  do {
    bout.cls();
    bout.litebar(StrCat("Editing G-File Area # ", n));
    bout << "|#9A) Name       : |#2" << r.name << wwiv::endl;
    bout << "|#9B) Filename   : |#2" << r.filename << wwiv::endl;
    bout << "|#9C) SL         : |#2" << static_cast<int>(r.sl) << wwiv::endl;
    bout << "|#9D) Min. Age   : |#2" << static_cast<int>(r.age) << wwiv::endl;
    bout << "|#9E) Max Files  : |#2" << r.maxfiles << wwiv::endl;
    bout << "|#9F) AR         : |#2" << GetArString(r) << wwiv::endl;
    bout.nl();
    bout << "|#7(|#2Q|#7=|#1Quit|#7) Which (|#1A|#7-|#1F,|#1[|#7,|#1]|#7) : ";
    char ch = onek("QABCDEF[]", true);
    switch (ch) {
    case 'Q':
      done = true;
      break;
    case '[':
      a()->gfilesec[n] = r;
      if (--n < 0) {
        n = a()->gfilesec.size() - 1;
      }
      r = a()->gfilesec[n];
      break;
    case ']':
      a()->gfilesec[n] = r;
      if (++n >= ssize(a()->gfilesec)) {
        n = 0;
      }
      r = a()->gfilesec[n];
      break;
    case 'A': {
      bout.nl();
      bout << "|#2New name? ";
      auto name = bin.input_text(40);
      if (s[0]) {
        to_char_array(r.name, name);
      }
    } break;
    case 'B': {
      bout.nl();
      if (File::Exists(FilePath(a()->config()->gfilesdir(), r.filename))) {
        bout << "\r\nThere is currently a directory for this g-file section.\r\n";
        bout << "If you change the filename, the directory will still be there.\r\n\n";
      }
      bout.nl();
      bout << "|#2New filename? ";
      bin.input(s, 8);
      if ((s[0] != 0) && (strchr(s, '.') == 0)) {
        strcpy(r.filename, s);
        if (!File::Exists(FilePath(a()->config()->gfilesdir(), r.filename))) {
          bout.nl();
          bout << "|#5Create directory for this section? ";
          if (bin.yesno()) {
            File dir(FilePath(a()->config()->gfilesdir(), r.filename));
            File::mkdirs(dir);
          } else {
            bout << "\r\nYou will have to create the directory manually, then.\r\n\n";
          }
        } else {
          bout << "\r\nA directory already exists under this filename.\r\n\n";
        }
        bout.pausescr();
      }
    }
    break;
    case 'C': {
      bout.nl();
      bout << "|#2New SL? ";
      bin.input(s, 3);
      int i = to_number<int>(s);
      if (i >= 0 && i < 256 && s[0]) {
        r.sl = static_cast<unsigned char>(i);
      }
    }
    break;
    case 'D': {
      bout.nl();
      bout << "|#2New Min Age? ";
      bin.input(s, 3);
      int i = to_number<int>(s);
      if ((i >= 0) && (i < 128) && (s[0])) {
        r.age = static_cast<unsigned char>(i);
      }
    }
    break;
    case 'E': {
      bout.nl();
      bout << "|#1Max 99 files/section.\r\n|#2New max files? ";
      bin.input(s, 3);
      int i = to_number<int>(s);
      if ((i >= 0) && (i < 99) && (s[0])) {
        r.maxfiles = static_cast<uint16_t>(i);
      }
    }
    break;
    case 'F':
      bout.nl();
      bout << "|#2New AR (<SPC>=None) ? ";
      char ch2 = onek("ABCDEFGHIJKLMNOP ");
      if (ch2 == SPACE) {
        r.ar = 0;
      } else {
        r.ar = 1 << (ch2 - 'A');
      }
      break;
    }
  } while (!done && !a()->sess().hangup());
  a()->gfilesec[n] = r;
}


void insert_sec(int n) {
  gfiledirrec r;

  strcpy(r.name, "** NEW SECTION **");
  strcpy(r.filename, "NONAME");
  r.sl    = 10;
  r.age   = 0;
  r.maxfiles  = 99;
  r.ar    = 0;

  insert_at(a()->gfilesec, n, r);
  modify_sec(n);
}


void delete_sec(int n) {
  erase_at(a()->gfilesec, n);
}


void gfileedit() {
  int i;
  char s[81];

  if (!ValidateSysopPassword()) {
    return;
  }
  showsec();
  bool done = false;
  do {
    bout.nl();
    bout << "|#2G-files: D:elete, I:nsert, M:odify, Q:uit, ? : ";
    char ch = onek("QDIM?");
    switch (ch) {
    case '?':
      showsec();
      break;
    case 'Q':
      done = true;
      break;
    case 'M':
      bout.nl();
      bout << "|#2Section number? ";
      bin.input(s, 2);
      i = to_number<int>(s);
      if (s[0] != 0 && i >= 0 && i < ssize(a()->gfilesec)) {
        modify_sec(i);
      }
      break;
    case 'I':
      if (ssize(a()->gfilesec) < a()->max_gfilesec) {
        bout.nl();
        bout << "|#2Insert before which section? ";
        bin.input(s, 2);
        i = to_number<int>(s);
        if (s[0] != 0 && i >= 0 && i <= ssize(a()->gfilesec)) {
          insert_sec(i);
        }
      }
      break;
    case 'D':
      bout.nl();
      bout << "|#2Delete which section? ";
      bin.input(s, 2);
      i = to_number<int>(s);
      if (s[0] != 0 && i >= 0 && i < ssize(a()->gfilesec)) {
        bout.nl();
        bout << "|#5Delete " << a()->gfilesec[i].name << "?";
        if (bin.yesno()) {
          delete_sec(i);
        }
      }
      break;
    }
  } while (!done && !a()->sess().hangup());

  DataFile<gfiledirrec> file(FilePath(a()->config()->datadir(), GFILE_DAT),
	File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
  if (file) {
    file.WriteVector(a()->gfilesec);
  }
}


bool fill_sec(int sn) {
  int nf = 0, i, i1, n1;
  char s[81];

  gfilerec *g = read_sec(sn, &n1);
  const auto path = FilePath(a()->config()->gfilesdir(), a()->gfilesec[sn].filename);
  const auto filespec = FilePath(path, "*.*");
  FindFiles ff(filespec, FindFilesType::files);
  bool ok{true};
  int chd = 0;
  for (const auto& f : ff) {
    if (nf >= a()->gfilesec[sn].maxfiles || a()->sess().hangup() || !ok) {
      break;
    }
    to_char_array(s, aligns(f.name));
    i = 1;
    for (i1 = 0; i1 < nf; i1++) {
      if (wwiv::sdk::files::aligned_wildcard_match(f.name, g[i1].filename)) {
        i = 0;
      }
    }
    if (i) {
      bout << "|#2" << s << " : ";
      auto s1s = bin.input_text(60);
      if (!s1s.empty()) {
        chd = 1;
        i = 0;
        while (wwiv::strings::StringCompare(s1s.c_str(), g[i].description) > 0 && i < nf) {
          ++i;
        }
        for (i1 = nf; i1 > i; i1--) {
          g[i1] = g[i1 - 1];
        }
        ++nf;
        gfilerec g1;
        to_char_array(g1.filename, f.name);
        to_char_array(g1.description, s1s);
        g1.daten = daten_t_now();
        g[i] = g1;
      } else {
        ok = false;
      }
    }
  }
  if (!ok) {
    bout << "|#6Aborted.\r\n";
  }
  if (nf >= a()->gfilesec[sn].maxfiles) {
    bout << "Section full.\r\n";
  }
  if (chd) {
    auto file_name = StrCat(a()->gfilesec[sn].filename, ".gfl");
    File gflFile(FilePath(a()->config()->datadir(), file_name));
    gflFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
    gflFile.Write(g, nf * sizeof(gfilerec));
    gflFile.Close();
    auto status = a()->status_manager()->BeginTransaction();
    status->SetGFileDate(date());
    a()->status_manager()->CommitTransaction(std::move(status));
  }
  free(g);
  return !ok;
}
