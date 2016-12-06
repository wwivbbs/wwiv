/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2016, WWIV Software Services             */
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

#include <string>
#include <vector>

#include "bbs/datetime.h"
#include "bbs/input.h"
#include "bbs/bbs.h"
#include "bbs/bbsutl1.h"
#include "bbs/com.h"
#include "bbs/fcns.h"
#include "bbs/gfiles.h"
#include "bbs/pause.h"
#include "bbs/vars.h"
#include "bbs/keycodes.h"
#include "sdk/status.h"
#include "core/file.h"
#include "core/datafile.h"
#include "core/stl.h"
#include "core/strings.h"
#include "core/wfndfile.h"
#include "sdk/filenames.h"

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
  return StringPrintf("|#2%2d |#3%1c  |#1%-40s  |#2%-8s |#9%-3d %-3d %-3d",
          nSectionNum, x, stripcolors(r.name), r.filename, r.sl, r.age, r.maxfiles);
}

static void showsec() {
  bout.cls();
  bool abort = false;
  pla("|#2NN AR Name                                      FN       SL  AGE MAX", &abort);
  pla("|#7-- == ----------------------------------------  ======== --- === ---", &abort);
  int current_section = 0;
  for (const auto& r : a()->gfilesec) {
    pla(gfiledata(r, current_section++), &abort);
    if (abort) {
      break;
    }
  }
}

static string GetArString(gfiledirrec r) {
  if (r.ar != 0) {
    for (int i = 0; i < 16; i++) {
      if ((1 << i) & r.ar) {
        return string(1, static_cast<char>('A' + i));
      }
    }
  }
  return "None.";
}

void modify_sec(int n) {
  gfiledirrec& r = a()->gfilesec[n];
  bool done = false;
  char s[81];
  do {
    bout.cls();
    bout.litebar("Editing G-File Area # %d", n);
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
      if (++n >= size_int(a()->gfilesec)) {
        n = 0;
      }
      r = a()->gfilesec[n];
      break;
    case 'A':
      bout.nl();
      bout << "|#2New name? ";
      inputl(s, 40);
      if (s[0]) {
        strcpy(r.name, s);
      }
      break;
    case 'B': {
      bout.nl();
      if (File::Exists(a()->config()->gfilesdir(), r.filename)) {
        bout << "\r\nThere is currently a directory for this g-file section.\r\n";
        bout << "If you change the filename, the directory will still be there.\r\n\n";
      }
      bout.nl();
      bout << "|#2New filename? ";
      input(s, 8);
      if ((s[0] != 0) && (strchr(s, '.') == 0)) {
        strcpy(r.filename, s);
        if (!File::Exists(a()->config()->gfilesdir(), r.filename)) {
          bout.nl();
          bout << "|#5Create directory for this section? ";
          if (yesno()) {
            File dir(a()->config()->gfilesdir(), r.filename);
            File::mkdirs(dir);
          } else {
            bout << "\r\nYou will have to create the directory manually, then.\r\n\n";
          }
        } else {
          bout << "\r\nA directory already exists under this filename.\r\n\n";
        }
        pausescr();
      }
    }
    break;
    case 'C': {
      bout.nl();
      bout << "|#2New SL? ";
      input(s, 3);
      int i = atoi(s);
      if (i >= 0 && i < 256 && s[0]) {
        r.sl = static_cast<unsigned char>(i);
      }
    }
    break;
    case 'D': {
      bout.nl();
      bout << "|#2New Min Age? ";
      input(s, 3);
      int i = atoi(s);
      if ((i >= 0) && (i < 128) && (s[0])) {
        r.age = static_cast<unsigned char>(i);
      }
    }
    break;
    case 'E': {
      bout.nl();
      bout << "|#1Max 99 files/section.\r\n|#2New max files? ";
      input(s, 3);
      int i = atoi(s);
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
  } while (!done && !hangup);
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
      input(s, 2);
      i = atoi(s);
      if (s[0] != 0 && i >= 0 && i < size_int(a()->gfilesec)) {
        modify_sec(i);
      }
      break;
    case 'I':
      if (size_int(a()->gfilesec) < a()->max_gfilesec) {
        bout.nl();
        bout << "|#2Insert before which section? ";
        input(s, 2);
        i = atoi(s);
        if (s[0] != 0 && i >= 0 && i <= size_int(a()->gfilesec)) {
          insert_sec(i);
        }
      }
      break;
    case 'D':
      bout.nl();
      bout << "|#2Delete which section? ";
      input(s, 2);
      i = atoi(s);
      if (s[0] != 0 && i >= 0 && i < size_int(a()->gfilesec)) {
        bout.nl();
        bout << "|#5Delete " << a()->gfilesec[i].name << "?";
        if (yesno()) {
          delete_sec(i);
        }
      }
      break;
    }
  } while (!done && !hangup);

  DataFile<gfiledirrec> file(a()->config()->datadir(), GFILE_DAT,
	File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
  if (file) {
    file.WriteVector(a()->gfilesec);
  }
}


bool fill_sec(int sn) {
  int nf = 0, i, i1, n1;
  char s[81], s1[81];
  WFindFile fnd;
  bool bFound = false;

  gfilerec *g = read_sec(sn, &n1);
  string gfilesdir = a()->config()->gfilesdir();
  sprintf(s1, "%s%s%c*.*", gfilesdir.c_str(), a()->gfilesec[sn].filename, File::pathSeparatorChar);
  bFound = fnd.open(s1, 0);
  bool ok = true;
  int chd = 0;
  while ((bFound) && (!hangup) && (nf < a()->gfilesec[sn].maxfiles) && (ok)) {
    if (fnd.GetFileName()[0] == '.') {
      bFound = fnd.next();
      continue;
    }
    strcpy(s, fnd.GetFileName());
    align(s);
    i = 1;
    for (i1 = 0; i1 < nf; i1++) {
      if (compare(fnd.GetFileName(), g[i1].filename)) {
        i = 0;
      }
    }
    if (i) {
      bout << "|#2" << s << " : ";
      inputl(s1, 60);
      if (s1[0]) {
        chd = 1;
        i = 0;
        while (wwiv::strings::StringCompare(s1, g[i].description) > 0 && i < nf) {
          ++i;
        }
        for (i1 = nf; i1 > i; i1--) {
          g[i1] = g[i1 - 1];
        }
        ++nf;
        gfilerec g1;
        strcpy(g1.filename, fnd.GetFileName());
        strcpy(g1.description, s1);
        g1.daten = static_cast<long>(time(nullptr));
        g[i] = g1;
      } else {
        ok = false;
      }
    }
    bFound = fnd.next();
  }
  if (!ok) {
    bout << "|#6Aborted.\r\n";
  }
  if (nf >= a()->gfilesec[sn].maxfiles) {
    bout << "Section full.\r\n";
  }
  if (chd) {
    string file_name = StrCat(syscfg.datadir, a()->gfilesec[sn].filename, ".gfl");
    File gflFile(file_name);
    gflFile.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
    gflFile.Write(g, nf * sizeof(gfilerec));
    gflFile.Close();
    WStatus *pStatus = a()->status_manager()->BeginTransaction();
    pStatus->SetGFileDate(date());
    a()->status_manager()->CommitTransaction(pStatus);
  }
  free(g);
  return !ok;
}
