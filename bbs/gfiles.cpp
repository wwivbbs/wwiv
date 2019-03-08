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
#include "bbs/gfiles.h"
#include <string>

#include "bbs/bbs.h"
#include "bbs/bbsutl.h"
#include "bbs/com.h"
#include "bbs/gfileedit.h"
#include "bbs/mmkey.h"
#include "bbs/instmsg.h"
#include "bbs/pause.h"
#include "bbs/printfile.h"
#include "bbs/sysoplog.h"
#include "bbs/sr.h"
#include "bbs/utility.h"
#include "bbs/xfer.h"
#include "core/strings.h"
#include "core/wwivassert.h"
#include "core/datetime.h"

using std::string;
using namespace wwiv::core;
using namespace wwiv::sdk;
using namespace wwiv::strings;

void gfl_hdr(int which);
void list_sec(int *map, int nmap);
void list_gfiles(gfilerec * g, int nf, int sn);
void gfile_sec(int sn);
void gfiles2();
void gfiles3(int n);

gfilerec *read_sec(int sn, int *nf) {
  gfilerec *pRecord;
  *nf = 0;

  int nSectionSize = sizeof(gfilerec) * a()->gfilesec[sn].maxfiles;
  if ((pRecord = static_cast<gfilerec *>(BbsAllocA(nSectionSize))) == nullptr) {
    return nullptr;
  }

  File file(FilePath(a()->config()->datadir(), StrCat(a()->gfilesec[sn].filename, ".gfl")));
  if (file.Open(File::modeBinary | File::modeReadOnly)) {
    *nf = file.Read(pRecord, nSectionSize) / sizeof(gfilerec);
  }
  return pRecord;
}

void gfl_hdr(int which) {
  string s, s1, s2, s3;
  if (okansi()) {
    s2 = std::string(29, '\xC4');
  } else {
    s2 = std::string(29, '-');
  }
  if (which) {
    s1 = std::string(12, ' ');
    s3 = std::string(11, ' ');
  } else {
    s1 = std::string(12, ' ');
    s3 = std::string(17, ' ');
  }
  bool abort = false;
  if (okansi()) {
    if (which) {
      s = StrCat("|#7\xDA\xC4\xC4\xC4\xC2", s2, "\xC2\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC2", s2, "\xC2\xC4\xC4\xC4\xC4\xBF");
    } else {
      s = StrCat("|#7\xDA\xC4\xC4\xC4\xC2", s2, "\xC4\xC4\xC4\xC4\xC4\xC2\xC4\xC4\xC4\xC2", s2, "\xC4\xC4\xC4\xC4\xBF");
    }
  } else {
    if (which) {
      s = StrCat("+---+", s2, "+----+---+", s2, "+----+");
    } else {
      s = StrCat("+---+", s2, "-----+---+", s2, "----+");
    }
  }
  bout.bpla(s, &abort);
  bout.Color(0);
  if (okansi()) {
    if (which) {
      s = StrCat("|#7\xB3|#2 # |#7\xB3", s1, "|#1 Name ", s3, "|#7\xB3|#9Size|#7\xB3|#2 # |#7\xB3", s1, "|#1 Name ", s3, "|#7\xB3|#9Size|#7\xB3");
    } else {
      s = StrCat("|#7\xB3|#2 # |#7\xB3", s1, "|#1 Name", s3, "|#7\xB3|#2 # |#7\xB3", s1, "|#1Name", s3, "|#7\xB3");
    }
  } else {
    if (which) {
      s = StrCat("| #   |", s1, "Name ", s1, "|Size| # |", s1, " Name", s1, "|Size|");
    } else {
      s = StrCat("| # |", s1, " Name     ", s1, "| # |", s1, " Name    ", s1, "|");
    }
  }
  bout.bpla(s, &abort);
  bout.Color(0);
  if (okansi()) {
    if (which) {
      s = StrCat("|#7\xC3\xC4\xC4\xC4\xC5", s2, "\xC5\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC5", s2, "\xC5\xC4\xC4\xC4\xC4\xB4");
    } else {
      s = StrCat("|#7\xC3\xC4\xC4\xC4\xC5", s2, "\xC4\xC4\xC4\xC4\xC4\xC5\xC4\xC4\xC4\xC5", s2, "\xC4\xC4\xC4\xC4\xB4");
    }
  } else {
    if (which) {
      s = StrCat("+---+", s2, "+----+---+", s2, "+----+");
    }
    else {
      s = StrCat("+---+", s2, "-----+---+", s2, "----+");
    }
  }
  bout.bpla(s, &abort);
  bout.Color(0);
}

void list_sec(int *map, int nmap) {
  char lnum[5], rnum[5];

  int i2 = 0;
  bool abort = false;
  string s, s2, s3, s4, s5, s6, s7;
  if (okansi()) {
    s2 = std::string(29, '\xC4');
    s3 = std::string(12, '\xC4');
    s7 = std::string(12, '\xC4');
  } else {
    s2 = std::string(29, '-');
    s3 = std::string(12, '-');
    s7 = std::string(12, '-');
  }

  bout.cls();
  bout.litebar(StrCat(a()->config()->system_name(), " G-Files Section"));
  gfl_hdr(0);
  string t = times();
  for (int i = 0; i < nmap && !abort && !a()->hangup_; i++) {
    sprintf(lnum, "%d", i + 1);
    s4 = trim_to_size_ignore_colors(a()->gfilesec[map[i]].name, 34);
    if (i + 1 >= nmap) {
      if (okansi()) {
        to_char_array(rnum, std::string(3, '\xFE'));
        s5 = std::string(29, '\xFE');
      } else {
        to_char_array(rnum, std::string(3, 'o'));
        s5 = std::string(29, 'o');
      }
    } else {
      to_char_array(rnum, std::to_string(i + 2));
      s5 = trim_to_size_ignore_colors(a()->gfilesec[map[i + 1]].name, 29);
    }
    if (okansi()) {
      s = StringPrintf("|#7\xB3|#2%3s|#7\xB3|#1%-34s|#7\xB3|#2%3s|#7\xB3|#1%-33s|#7\xB3", 
        lnum, s4.c_str(), rnum, s5.c_str());
    } else {
      s = StringPrintf("|%3s|%-34s|%3s|%-33s|", lnum, s4.c_str(), rnum, s5.c_str());
    }
    bout.bpla(s, &abort);
    bout.Color(0);
    i++;
    if (i2 > 10) {
      i2 = 0;
      string s1;
      if (okansi()) {
        s1 = StringPrintf("|#7\xC3\xC4\xC4\xC4X%s\xC4\xC4\xC4\xC4\xC4X\xC4\xC4\xC4X\xC4\xC4\xC4\xC4\xC4\xC4%s|#1\xFE|#7\xC4|#2%s|#7\xC4|#2\xFE|#7\xC4\xC4\xC4X",
                s2.c_str(), s3.c_str(), t.c_str());
      } else {
        s1 = StringPrintf("+---+%s-----+--------+%s-o-%s-o---+",
                s2.c_str(), s3.c_str(), t.c_str());
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
      bout.nl();
      pausescr();
      gfl_hdr(1);
    }
  }
  if (!abort) {
    if (so()) {
      string s1;
      if (okansi()) {
        s1= StringPrintf("|#7\xC3\xC4\xC4\xC4\xC1%s\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC1%s\xC4\xC4\xC4\xC4\xB4", 
          s2.c_str(), s2.c_str());
      } else {
        s1 = StringPrintf("+---+%s-----+---+%s----+", s2.c_str(), s2.c_str());
      }
      bout.bpla(s1, &abort);
      bout.Color(0);

      string padding61 = std::string(61, ' ');
      if (okansi()) {
        s1 = StringPrintf("|#7\xB3  |#2G|#7)|#1G-File Edit%s|#7\xB3", padding61.c_str());
      } else {
        s1 = StringPrintf("|  G)G-File Edit%s|", padding61.c_str());
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
      if (okansi()) {
        s1 = StringPrintf(
            "|#7\xC0\xC4\xC4\xC4\xC4%s\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4%s|#1\xFE|#7\xC4|#2%s|#7\xC4|#1\xFE|#7\xC4\xC4\xC4\xD9",
            s2.c_str(), s7.c_str(), t.c_str());
      } else {
        s1 = StringPrintf("+----%s----------------%so-%s-o---+", s2.c_str(), s7.c_str(), t.c_str());
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
    } else {
      string s1;
      if (okansi()) {
        s1 = StringPrintf(
                "|#7\xC0\xC4\xC4\xC4\xC1%s\xC4\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4%s|#1\xFE|#7\xC4|#2%s|#7\xC4|#1\xFE|#7\xC4\xC4\xC4\xD9",
                s2.c_str(), s3.c_str(), t.c_str());
      } else {
        s1 = StringPrintf("+---+%s-----+---------------------+%so-%s-o---+", 
          s2.c_str(), s3.c_str(), t.c_str());
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
    }
  }
  bout.Color(0);
  bout.nl();
}

void list_gfiles(gfilerec* g, int nf, int sn) {
  int i;
  string s, s2, s3, s4, s5;
  string lnum, rnum, lsize, rsize;
  const auto t = times();

  bool abort = false;
  bout.cls();
  bout.litebar(a()->gfilesec[sn].name);
  int i2 = 0;
  if (okansi()) {
    s2 = std::string(29, '\xC4');
    s3 = std::string(12, '\xC4');
  } else {
    s2 = std::string(29, '-');
    s3 = std::string(12, '-');
  }
  gfl_hdr(1);
  const auto gfilesdir = a()->config()->gfilesdir();
  for (i = 0; i < nf && !abort && !a()->hangup_; i++) {
    i2++;
    lnum = std::to_string(i + 1);
    s4 = trim_to_size_ignore_colors(g[i].description, 29);
    string path_name = StrCat(
      gfilesdir, a()->gfilesec[sn].filename, File::pathSeparatorChar, g[i].filename);
    if (File::Exists(path_name)) {
      File handle(path_name);
      lsize = StrCat(std::to_string(bytes_to_k(handle.length())), "k");
    } else {
      lsize = "OFL";
    }
    if (i + 1 >= nf) {
      if (okansi()) {
        rnum = std::string(3, '\xFE');
        s5 = std::string(29, '\xFE');
        rsize = std::string(4, '\xFE');
      } else {
        rnum = std::string(3, 'o');
        s5 = std::string(29, 'o');
        rsize = std::string(4, 'o');
      }
    } else {
      rnum = std::to_string(i + 2);
      s5 = trim_to_size_ignore_colors(g[i + 1].description, 29);
      auto path_name = StrCat(
        gfilesdir, a()->gfilesec[sn].filename, File::pathSeparatorChar, g[i + 1].filename);
      if (File::Exists(path_name)) {
        File handle(path_name);
        rsize = StrCat(std::to_string(bytes_to_k(handle.length())), "k");
      } else {
        rsize = "OFL";
      }
    }
    if (okansi()) {
      s = StringPrintf("|#7\xB3|#2%3s|#7\xB3|#1%-29s|#7\xB3|#2%4s|#7\xB3|#2%3s|#7\xB3|#1%-29s|#7\xB3|#2%4s|#7\xB3",
              lnum.c_str(), s4.c_str(), lsize.c_str(), rnum.c_str(), s5.c_str(), rsize.c_str());
    } else {
      s = StringPrintf("|%3s|%-29s|%4s|%3s|%-29s|%4s|", 
        lnum.c_str(), s4.c_str(), lsize.c_str(), rnum.c_str(), s5.c_str(), rsize.c_str());
    }
    bout.bpla(s, &abort);
    bout.Color(0);
    i++;
    if (i2 > 10) {
      i2 = 0;
      string s1;
      if (okansi()) {
        s1 = StringPrintf("|#7\xC3\xC4\xC4\xC4X%sX\xC4\xC4\xC4\xC4X\xC4\xC4\xC4X\xC4%s|#1\xFE|#7\xC4|#2%s|#7\xC4|#1\xFE|#7\xC4\xFE\xC4\xC4\xC4\xC4\xD9",
                s2.c_str(), s3.c_str(), t.c_str());
      } else {
        s1 = StringPrintf("+---+%s+----+---+%s-o-%s-o-+----+", 
          s2.c_str(), s3.c_str(), t.c_str());
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
      bout.nl();
      pausescr();
      gfl_hdr(1);
    }
  }
  if (!abort) {
    if (okansi()) {
      s = StringPrintf("|#7\xC3\xC4\xC4\xC4\xC1%s\xC1\xC4\xC4\xC4\xC4\xC1\xC4\xC4\xC4\xC1%s\xC1\xC4\xC4\xC4\xC4\xB4", 
        s2.c_str(), s2.c_str());
    } else {
      s = StringPrintf("+---+%s+----+---+%s+----+", s2.c_str(), s2.c_str());
    }
    bout.bpla(s, &abort);
    bout.Color(0);
    if (so()) {
      string s1 = okansi()
        ? "|#7\xB3 |#1A|#7)|#2Add a G-File  |#1D|#7)|#2Download a G-file  |#1E|#7)|#2Edit this section  |#1R|#7)|#2Remove a G-File |#7\xB3"
        : "| A)Add a G-File  D)Download a G-file  E)Edit this section  R)Remove a G-File |";
      bout.bpla(s1, &abort);
      bout.Color(0);
    } else {
      string s1;
      if (okansi()) {
        s1 = StrCat("|#7\xB3  |#2D  |#1Download a G-file", std::string(55, ' '), "|#7\xB3");
      } else {
        s1 = StrCat("|  D  Download a G-file", std::string(55, ' '), "|");
      }
      bout.bpla(s1, &abort);
      bout.Color(0);
    }
  }
  string s1;
  if (okansi()) {
    s1 = StringPrintf("|#7\xC0\xC4\xC4\xC4\xC4%s\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4%s|#1\xFE|#7\xC4|#2%s|#7\xC4|#1\xFE|#7\xC4\xC4\xC4\xC4\xD9",
        s2.c_str(), s3.c_str(), t.c_str());
  } else {
    s1 = StringPrintf("+----%s----------------%so-%s-o----+",
        s2.c_str(), s3.c_str(), t.c_str());
  }
  bout.bpla(s1, &abort);
  bout.Color(0);
  bout.nl();
}

void gfile_sec(int sn) {
  int i, i1, i2, nf;
  bool abort;

  gfilerec* g = read_sec(sn, &nf);
  if (g == nullptr) {
    return;
  }
  std::set<char> odc;
  for (i = 1; i <= nf / 10; i++) {
    odc.insert(static_cast<char>(i + '0'));
  }
  list_gfiles(g, nf, sn);
  bool done = false;
  while (!done && !a()->hangup_) {
    a()->tleft(true);
    bout << "|#9Current G|#1-|#9File Section |#1: |#5" << a()->gfilesec[sn].name << "|#0\r\n";
    bout << "|#9Which G|#1-|#9File |#1(|#21|#1-|#2" << nf <<
                       "|#1), |#1(|#2Q|#1=|#9Quit|#1, |#2?|#1=|#9Relist|#1) : |#5";
    string ss = mmkey(odc);
    i = to_number<int>(ss);
    if (ss == "Q") {
      done = true;
    } else if (ss == "E" && so()) {
      done = true;
      gfiles3(sn);
    }
    if (ss == "A" && so()) {
      free(g);
      fill_sec(sn);
      g = read_sec(sn, &nf);
      if (g == nullptr) {
        return;
      }
      odc.clear();
      for (i = 1; i <= nf / 10; i++) {
        odc.insert(static_cast<char>(i + '0'));
      }
    } else if (ss == "R" && so()) {
      bout.nl();
      bout << "|#2G-file number to delete? ";
      string ss1 = mmkey(odc);
      i = to_number<int>(ss1);
      if (i > 0 && i <= nf) {
        bout << "|#9Remove " << g[i - 1].description << "|#1? |#5";
        if (yesno()) {
          bout << "|#5Erase file too? ";
          if (yesno()) {
            string gfilesdir = a()->config()->gfilesdir();
            auto file_name = FilePath(a()->gfilesec[sn].filename, g[i - 1].filename);
            File::Remove(a()->config()->datadir(), file_name);
          }
          for (i1 = i; i1 < nf; i1++) {
            g[i1 - 1] = g[i1];
          }
          --nf;
          const auto file_name = StrCat(a()->gfilesec[sn].filename, ".gfl");
          File file(FilePath(a()->config()->datadir(), file_name));
          file.Open(File::modeReadWrite | File::modeBinary | File::modeCreateFile | File::modeTruncate);
          file.Write(g, nf * sizeof(gfilerec));
          file.Close();
          bout << "\r\nDeleted.\r\n\n";
        }
      }
    } else if (ss == "?") {
      list_gfiles(g, nf, sn);
    } else if (ss == "Q") {
      done = true;
    } else if (i > 0 && i <= nf) {
      auto file_name = FilePath(a()->gfilesec[sn].filename, g[i - 1].filename);
      i1 = printfile(file_name);
      a()->user()->SetNumGFilesRead(a()->user()->GetNumGFilesRead() + 1);
      if (i1 == 0) {
        sysoplog() << "Read G-file '" << g[i - 1].filename << "'";
      }
    } else if (ss == "D") {
      bool done1 = false;
      while (!done1 && !a()->hangup_) {
        bout << "|#9Download which G|#1-|#9file |#1(|#2Q|#1=|#9Quit|#1, |#2?|#1=|#9Relist) : |#5";
        ss = mmkey(odc);
        i2 = to_number<int>(ss);
        abort = false;
        if (ss == "?") {
          list_gfiles(g, nf, sn);
          bout << "|#9Current G|#1-|#9File Section |#1: |#5" << a()->gfilesec[sn].name << wwiv::endl;
        } else if (ss == "Q") {
          list_gfiles(g, nf, sn);
          done1 = true;
        } else if (!abort) {
          if (i2 > 0 && i2 <= nf) {
            auto file_name = FilePath(a()->gfilesec[sn].filename, g[i2 - 1].filename);
            File file(FilePath(a()->config()->datadir(), file_name));
            if (!file.Open(File::modeReadOnly | File::modeBinary)) {
              bout << "|#6File not found : [" << file.full_pathname() << "]";
            } else {
              auto file_size = file.length();
              file.Close();
              bool sent = false;
              abort = false;
              send_file(file_name.c_str(), &sent, &abort, g[i2 - 1].filename, -1, file_size);
              char s1[255];
              if (sent) {
                sprintf(s1, "|#2%s |#9successfully transferred|#1.|#0\r\n", g[i2 - 1].filename);
                done1 = true;
              } else {
                sprintf(s1, "|#6\xFE |#9Error transferring |#2%s|#1.|#0", g[i2 - 1].filename);
                done1 = true;
              }
              bout.nl();
              bout << s1;
              bout.nl();
              sysoplog() << s1;
            }
          } else {
            done1 = true;
          }
        }
      }
    }
  }
  free(g);
}

void gfiles2() {
  write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_ONLINE);
  sysoplog() << "@ Ran Gfile Edit";
  gfileedit();
  gfiles();
}

void gfiles3(int n) {
  write_inst(INST_LOC_GFILEEDIT, 0, INST_FLAGS_ONLINE);
  sysoplog() << "@ Ran Gfile Edit";
  modify_sec(n);
  gfile_sec(n);
}

void gfiles() {
  int* map = static_cast<int *>(BbsAllocA(a()->max_gfilesec * sizeof(int)));

  bool done = false;
  int nmap = 0;
  int current_section = 0;
  std::set<char> odc;
  for (const auto& r : a()->gfilesec) {
    bool ok = true;
    if (a()->user()->GetAge() < r.age) {
      ok = false;
    }
    if (a()->effective_sl() < r.sl) {
      ok = false;
    }
    if (!a()->user()->HasArFlag(r.ar) && r.ar) {
      ok = false;
    }
    if (ok) {
      map[nmap++] = current_section;
      if ((nmap % 10) == 0) {
        odc.insert(static_cast<char>('0' + (nmap / 10)));
      }
    }
    current_section++;
  }
  if (nmap == 0) {
    bout << "\r\nNo G-file sections available.\r\n\n";
    free(map);
    return;
  }
  list_sec(map, nmap);
  while (!done && !a()->hangup_) {
    a()->tleft(true);
    bout << "|#9G|#1-|#9Files Main Menu|#0\r\n";
    bout << "|#9Which Section |#1(|#21|#1-|#2" << nmap <<
                       "|#1), |#1(|#2Q|#1=|#9Quit|#1, |#2?|#1=|#9Relist|#1) : |#5";
    string ss = mmkey(odc);
    if (ss == "Q") {
      done = true;
    } else if (ss == "G" && so()) {
      done = true;
      gfiles2();
    } else if (ss == "A" && cs()) {
      bool bIsSectionFull = false;
      for (int i = 0; i < nmap && !bIsSectionFull; i++) {
        bout.nl();
        bout << "Now loading files for " << a()->gfilesec[map[i]].name << "\r\n\n";
        bIsSectionFull = fill_sec(map[i]);
      }
    } else {
      int i = to_number<int>(ss);
      if (i > 0 && i <= nmap) {
        gfile_sec(map[i-1]);
      }
    }
    if (!done) {
      list_sec(map, nmap);
    }
  }
  free(map);
}
